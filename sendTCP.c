#include "sendTCP.h"

static int sock = 0;
static struct sockaddr_in serv_addr;
static const uint16_t _TID = 0x00;
static const uint16_t _PID = 0x00;
static const uint8_t _UID = 0x01;
static const uint8_t _baudCode = 0x01;

// Biến toàn cục cho thread nhận dữ liệu
static pthread_mutex_t recvMutex = PTHREAD_MUTEX_INITIALIZER;
static uint8_t lastRecvBuffer[256]; // Buffer lưu gói tin cuối cùng nhận được
static int lastRecvLength = 0;
static pthread_t recvThreadId;

// Hàm chạy trong thread để nhận dữ liệu liên tục từ socket
void* receiveThread(void* arg) {
    uint8_t buffer[256];
    
    while (1) { // Chạy vô hạn
        int bytesReceived = recv(sock, buffer, sizeof(buffer), 0);
        if (bytesReceived > 0) {
            pthread_mutex_lock(&recvMutex);
            lastRecvLength = bytesReceived;
            for (int i = 0; i < bytesReceived; i++) {
                lastRecvBuffer[i] = buffer[i];
            }
            // In ra dữ liệu nhận được (cho debug)
            printf("Received packet: ");
            for (int i = 0; i < bytesReceived; i++) {
                printf("%02x ", buffer[i]);
            }
            printf("\n");
            pthread_mutex_unlock(&recvMutex);
        }
        else if (bytesReceived == 0) {
            // Kết nối bị đóng
            printf("Connection closed by server\n");
            break;
        }
        else {
            // Lỗi socket
            perror("recv failed");
            break;
        }
    }
    // Khi thoát vòng lặp (lỗi hoặc kết nối đóng), thread tự động dừng
    return NULL;
}

// Hàm kiểm tra phản hồi từ buffer gần nhất (dành cho FUNC_OPEN_CAN)
int getResponse(uint8_t expectedFunc, int timeoutMs) {
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    ts.tv_sec += timeoutMs / 1000;
    ts.tv_nsec += (timeoutMs % 1000) * 1000000;

    while (1) {
        pthread_mutex_lock(&recvMutex);
        if (lastRecvLength >= 9) { // Giả sử gói tin tối thiểu có 9 byte
            if (lastRecvBuffer[6] == expectedFunc) {
                pthread_mutex_unlock(&recvMutex);
                return 0; // Thành công
            }
            else if (lastRecvBuffer[6] == (expectedFunc | 0x80)) {
                pthread_mutex_unlock(&recvMutex);
                return -4; // Lỗi từ server
            }
        }
        pthread_mutex_unlock(&recvMutex);

        // Chờ một chút nếu chưa có dữ liệu
        usleep(1000); // Ngủ 1ms để không chiếm CPU quá nhiều

        // Kiểm tra timeout
        struct timespec now;
        clock_gettime(CLOCK_REALTIME, &now);
        if (now.tv_sec > ts.tv_sec || (now.tv_sec == ts.tv_sec && now.tv_nsec > ts.tv_nsec)) {
            return -2; // Timeout
        }
    }
    return -1; // Không bao giờ đến đây do vòng lặp vô hạn
}

// Hàm gửi lệnh mở/đóng CAN (chờ phản hồi)
bool SendTCP_OpenCloseCAN(bool isOpen) {
    uint8_t pkt[10] = {0};
    uint8_t len = 5;
    pkt[0] = (_TID >> 8) & 0xFF;
    pkt[1] = _TID & 0xFF;
    pkt[2] = (_PID >> 8) & 0xFF;
    pkt[3] = _PID & 0xFF;
    pkt[4] = len;
    pkt[5] = _UID;
    pkt[6] = FUNC_OPEN_CAN;
    pkt[7] = DEFAULT_CAN_CHANNEL;
    pkt[8] = isOpen;
    pkt[9] = _baudCode;

    if (send(sock, pkt, sizeof(pkt), 0) < 0) {
        perror("send failed");
        return false;
    }

    switch (getResponse(FUNC_OPEN_CAN, 500)) {
        case 0:
            return true;
        default:
            return false;
    }
}

// Hàm gửi frame (không chờ phản hồi)
void SendTCP_SendFrame(uint32_t laneID, COMMAND_CODE cmd, LED_COLOR ledColor, uint32_t timeCounter) {
    uint8_t pkt[21] = {0};
    pkt[0] = (_TID >> 8) & 0xFF;
    pkt[1] = _TID & 0xFF;
    pkt[2] = (_PID >> 8) & 0xFF;
    pkt[3] = _PID & 0xFF;
    pkt[4] = 0x10; // Độ dài dữ liệu
    pkt[5] = _UID;
    pkt[6] = FUNC_SEND_FRAME;
    pkt[7] = DEFAULT_CAN_CHANNEL;
    // ID
    pkt[8] = (laneID >> 24) & 0xFF;
    pkt[9] = (laneID >> 16) & 0xFF;
    pkt[10] = (laneID >> 8) & 0xFF;
    pkt[11] = laneID & 0xFF;
    // DLC
    pkt[12] = 8;
    // Dữ liệu
    pkt[13] = (cmd >> 8) & 0xFF;
    pkt[14] = cmd & 0xFF;
    pkt[15] = (ledColor >> 8) & 0xFF;
    pkt[16] = ledColor & 0xFF;
    pkt[17] = (timeCounter >> 24) & 0xFF;
    pkt[18] = (timeCounter >> 16) & 0xFF;
    pkt[19] = (timeCounter >> 8) & 0xFF;
    pkt[20] = timeCounter & 0xFF;

    if (send(sock, pkt, sizeof(pkt), 0) < 0) {
        perror("send failed");
    }
}

// Hàm tính thời gian đếm ngược
static uint32_t calculateTimeCounter(uint32_t vehicleCount) {
    const uint32_t SECONDS_PER_VEHICLE = 2; // 2 giây cho mỗi phương tiện
    const uint32_t MIN_TIME = 10;          // Thời gian tối thiểu 10 giây
    const uint32_t MAX_TIME = 60;          // Thời gian tối đa 60 giây

    uint32_t timeCounter = vehicleCount * SECONDS_PER_VEHICLE;
    if (timeCounter < MIN_TIME) {
        timeCounter = MIN_TIME;
    } else if (timeCounter > MAX_TIME) {
        timeCounter = MAX_TIME;
    }
    return timeCounter;
}

// Hàm mới để xử lý và gửi frame với giới hạn 100ms
void SendTCP_Traffic(uint32_t laneID, uint32_t vehicleCount) {
    static struct timespec lastSendTime = {0, 0};
    struct timespec currentTime;

    // Lấy thời gian hiện tại
    clock_gettime(CLOCK_REALTIME, &currentTime);

    // Tính khoảng thời gian kể từ lần gửi cuối (đơn vị: mili giây)
    long timeDiffMs = (currentTime.tv_sec - lastSendTime.tv_sec) * 1000 +
                      (currentTime.tv_nsec - lastSendTime.tv_nsec) / 1000000;

    // Chỉ gửi nếu đã qua 100ms kể từ lần gửi trước
    if (lastSendTime.tv_sec == 0 || timeDiffMs >= 100) {
        uint32_t timeCounter = calculateTimeCounter(vehicleCount);
        SendTCP_SendFrame(laneID, CMD_RESPONSE, LEDColor_RED, timeCounter);
        lastSendTime = currentTime; // Cập nhật thời gian gửi cuối
        printf("Sent traffic frame: laneID=%u, vehicleCount=%u, timeCounter=%u\n", 
               laneID, vehicleCount, timeCounter);
    }
}

// Hàm khởi tạo socket
void initialize_socketcc(void) {
    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        printf("Socket creation error\n");
        exit(1); // Thoát nếu không tạo được socket
    }

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(5000);

    if (inet_pton(AF_INET, "192.168.0.10", &serv_addr.sin_addr) <= 0) {
        printf("Invalid address\n");
        close(sock);
        exit(1);
    }

    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        printf("Connection Failed\n");
        close(sock);
        exit(1);
    }
    printf("Socket initialized and connected\n");

    // Tạo thread nhận dữ liệu
    if (pthread_create(&recvThreadId, NULL, receiveThread, NULL) != 0) {
        perror("pthread_create failed");
        close(sock);
        exit(1);
    }
}

// // Hàm chính
// int main() {

//     // Khởi tạo socket
//     initialize_socketcc();

//     bool result = SendTCP_OpenCloseCAN(true);
//     printf("Open CAN Result: %d\n", result);
//     sleep(2);

//     // Gửi lệnh ví dụ và chạy liên tục
//     while (1) {

//         SendTCP_Traffic(0x01, 5);  // 5 phương tiện
//         usleep(50000); // 50ms, mô phỏng AI gọi nhanh
//     }

//     // Không bao giờ đến đây do vòng lặp vô hạn
//     close(sock);
//     return 0;
// }
