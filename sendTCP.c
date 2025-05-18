#include "sendTCP.h"

static int sock = 0;
static struct sockaddr_in serv_addr;

static const uint16_t _TID = 0x00;
static const uint16_t _PID = 0x00;
static const uint8_t _UID = 0x01;
static const uint8_t _baudCode = 0x01;
static const uint8_t _myCANID = 0xAF;
static const uint8_t yellowTime = 5;

static LANE_STATUS laneTable[4] = {
    {1, 0, 0, 0, LEDColor_RED, LEDColor_UNKNOWN, false, false, {0, 0}},
    {2, 0, 0, 0, LEDColor_RED, LEDColor_UNKNOWN, false, false, {0, 0}},
    {3, 0, 0, 0, LEDColor_RED, LEDColor_UNKNOWN, false, false, {0, 0}},
    {4, 0, 0, 0, LEDColor_RED, LEDColor_UNKNOWN, false, false, {0, 0}}};

static bool isUpdatingFromAI = true;
static int ackCount = 0;
static bool resendFlag = false;

static pthread_mutex_t recvMutex = PTHREAD_MUTEX_INITIALIZER;
static uint8_t lastRecvBuffer[256]; 
static int lastRecvLength = 0;
static pthread_t recvThreadId;

static int getResponse(uint8_t expectedFunc, int timeoutMs)
{
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    ts.tv_sec += timeoutMs / 1000;
    ts.tv_nsec += (timeoutMs % 1000) * 1000000;

    while (1)
    {
        pthread_mutex_lock(&recvMutex);
        if (lastRecvLength >= 9)
        { 
            if (lastRecvBuffer[6] == expectedFunc)
            {
                pthread_mutex_unlock(&recvMutex);
                return 0; 
            }
            else if (lastRecvBuffer[6] == (expectedFunc | 0x80))
            {
                pthread_mutex_unlock(&recvMutex);
                return -4; 
            }
        }
        pthread_mutex_unlock(&recvMutex);

        usleep(1000); 

        struct timespec now;
        clock_gettime(CLOCK_REALTIME, &now);
        if (now.tv_sec > ts.tv_sec || (now.tv_sec == ts.tv_sec && now.tv_nsec > ts.tv_nsec))
        {
            return -2; 
        }
    }
    return -1; 
}

static void SendTCP_SendFrame(uint8_t laneID, COMMAND_CODE cmd, LED_COLOR ledColor, uint32_t timeCounter)
{
    uint8_t pkt[21] = {0};
    pkt[0] = (_TID >> 8) & 0xFF;
    pkt[1] = _TID & 0xFF;
    pkt[2] = (_PID >> 8) & 0xFF;
    pkt[3] = _PID & 0xFF;
    pkt[4] = 0x10; 
    pkt[5] = _UID;
    pkt[6] = FUNC_SEND_FRAME;
    pkt[7] = DEFAULT_CAN_CHANNEL;
    // ID
    pkt[8] = 0;
    pkt[9] = 0;
    pkt[10] = laneID & 0xFF;
    pkt[11] = _myCANID & 0xFF;

    // DLC
    pkt[12] = 8;
    // Data
    pkt[13] = (cmd >> 8) & 0xFF;
    pkt[14] = cmd & 0xFF;
    pkt[15] = (ledColor >> 8) & 0xFF;
    pkt[16] = ledColor & 0xFF;
    pkt[17] = (timeCounter >> 24) & 0xFF;
    pkt[18] = (timeCounter >> 16) & 0xFF;
    pkt[19] = (timeCounter >> 8) & 0xFF;
    pkt[20] = timeCounter & 0xFF;

    if (send(sock, pkt, sizeof(pkt), 0) < 0)
    {
        perror("send failed");
    }
}

static uint32_t calculateTimeCounter(uint32_t vehicleCount)
{
    const uint32_t SECONDS_PER_VEHICLE = 5; // 2 giây cho mỗi phương tiện
    const uint32_t MIN_TIME = 15;
    const uint32_t MAX_TIME = 90;

    uint32_t timeCounter = vehicleCount * SECONDS_PER_VEHICLE;
    if (timeCounter < MIN_TIME)
        timeCounter = MIN_TIME;
    else if (timeCounter > MAX_TIME)
        timeCounter = MAX_TIME;
    return timeCounter;
}

static LED_COLOR getNextColor(LED_COLOR currentColor)
{
    switch (currentColor)
    {
    case LEDColor_RED:
        return LEDColor_GREEN;
    case LEDColor_GREEN:
    case LEDColor_YELLOW:
        return LEDColor_RED;
    default:
        return LEDColor_UNKNOWN;
    }
}

// Hàm xác định màu đèn tiếp theo và thời gian dựa trên số xe dừng đèn đỏ
static void calculateNextState()
{
    // Tính tổng số xe dừng đèn đỏ (Lane 1 và 3 hoặc Lane 2 và 4)
    uint32_t redVehicleCount = 0;

    if (laneTable[0].currentColor == LEDColor_RED)
        redVehicleCount = laneTable[0].vehicleCount + laneTable[2].vehicleCount;
    else
        redVehicleCount = laneTable[1].vehicleCount + laneTable[3].vehicleCount;

    uint32_t nextTime = calculateTimeCounter(redVehicleCount);

    // Xác định màu đèn tiếp theo (đồng bộ Lane 1-3 và Lane 2-4)
    for (int i = 0; i < MAX_LANES; i++)
    {
        LANE_STATUS *lane = &laneTable[i];

        lane->nextColor = getNextColor(lane->currentColor);
        if (lane->nextColor == LEDColor_RED)
            lane->timeCounter = nextTime + yellowTime;
        if (lane->nextColor == LEDColor_GREEN)
            lane->timeCounter = nextTime;
    }
}

void HandleTCP_Request(uint32_t laneID)
{
    if (laneID < 1 || laneID > MAX_LANES)
        return;

    isUpdatingFromAI = false;
    calculateNextState();

    int laneIndex = laneID - 1;
    LANE_STATUS *lane = &laneTable[laneIndex];

    struct timespec currentTime;
    clock_gettime(CLOCK_REALTIME, &currentTime);

    for (int i = 0; i < MAX_LANES; i++)
    {
        LANE_STATUS *lane = &laneTable[i];
        lane->isWaitingAck = true;
        lane->acked = false;
        SendTCP_SendFrame(lane->laneID, CMD_RESPONSE, lane->nextColor, lane->timeCounter);
        lane->lastSendTime = currentTime;
        usleep(50);
    }
    ackCount = 0;
    resendFlag = true;
    printf("Received REQUEST for Lane %u, sending nextColor=%u, timeCounter=%u\n",
           laneID, lane->nextColor, lane->timeCounter);
}

void HandleTCP_Ack(uint32_t laneID)
{
    if (laneID < 1 || laneID > MAX_LANES)
        return;

    int laneIndex = laneID - 1;
    LANE_STATUS *lane = &laneTable[laneIndex];

    if (lane->isWaitingAck)
    {
        lane->isWaitingAck = false; // Dừng gửi lặp lại
        if (!lane->acked)
        {
            lane->acked = true;
            ackCount++;
        }

        if (ackCount == MAX_LANES)
            resendFlag = false;
        

        printf("Received ACK for Lane %u, updated currentColor=%u\n", laneID, lane->currentColor);
    }
}

// Hàm kiểm tra và gửi lại frame
void CheckAndResend()
{
    static const uint32_t timeDefault = 45;
    struct timespec currentTime;

    if (!resendFlag)
        return;

    clock_gettime(CLOCK_REALTIME, &currentTime);

    // Kiểm tra thời gian còn lại của Lane 1 (đồng bộ cho cả 4)
    
    if (laneTable[0].timeRemain <= 2  && ackCount < MAX_LANES)
    {
        // Gửi thời gian mặc định 45s cho cả 4 lane
        for (int i = 0; i < MAX_LANES; i++)
        {
            LANE_STATUS *lane = &laneTable[i];
            lane->isWaitingAck = true;

            if (lane->nextColor == LEDColor_RED)
                lane->timeCounter = timeDefault + yellowTime;
            if (lane->nextColor == LEDColor_GREEN)
                lane->timeCounter = timeDefault;
            printf("TimeRemain <= 2s, sent default 45s to Lane %u\n", i + 1);
        }
    }

    if (laneTable[0].timeRemain <= 1)
    {    
        isUpdatingFromAI = true;
        resendFlag = false;
    }
    // Gửi lại cho các lane chưa nhận ACK
    for (int i = 0; i < MAX_LANES; i++)
    {
        LANE_STATUS *lane = &laneTable[i];
        if (lane->isWaitingAck)
        {
            long timeDiffMs = (currentTime.tv_sec - lane->lastSendTime.tv_sec) * 1000 +
                              (currentTime.tv_nsec - lane->lastSendTime.tv_nsec) / 1000000;
            if (lane->lastSendTime.tv_sec == 0 || timeDiffMs >= 100)
            {
                SendTCP_SendFrame(lane->laneID, CMD_RESPONSE, lane->nextColor, lane->timeCounter);
                lane->lastSendTime = currentTime;
                // printf("Resent to Lane %u: nextColor=%u, timeCounter=%u\n", i + 1, lane->nextColor, laneTable[0].timeCounter);
            }
        }
        usleep(50);
    }
}

static void CAN_RecvFrameHandler(uint8_t *CANID, uint8_t DLC, uint8_t *CANdata)
{
    uint8_t myCANID = CANID[2];
    uint8_t laneID = CANID[3];

    if (laneID < 1 || laneID > 4 || myCANID != _myCANID)
        return;

    uint16_t cmd = (CANdata[0] << 8) | CANdata[1];
    uint16_t ledColor = (CANdata[2] << 8) | CANdata[3];
    uint32_t timeRemain = (CANdata[4] << 24) | (CANdata[5] << 16) |
                          (CANdata[6] << 8) | CANdata[7];

    LANE_STATUS *lane = &laneTable[laneID - 1];
    if (cmd == CMD_CYCLE)
    {
        lane->currentColor = (LED_COLOR)ledColor;
        lane->timeRemain = timeRemain;
        // printf("Updated Lane %u: Color=%u, Time=%u\n", laneID, ledColor, timeRemain);
    }
    else if (cmd == CMD_REQUEST)
    {
        HandleTCP_Request(laneID);
    }
    else if (cmd == CMD_ACK)
    {
        HandleTCP_Ack(laneID);
    }
}

void *receiveThread(void *arg)
{
    uint8_t buffer[256];

    while (1)
    { // Chạy vô hạn
        int bytesReceived = recv(sock, buffer, sizeof(buffer), 0);
        if (bytesReceived > 0)
        {
            pthread_mutex_lock(&recvMutex);

            lastRecvLength = bytesReceived;
            if (lastRecvLength < 9)
            { // Bản tin tối thiểu: 9 byte header + 8 byte CAN Data
                pthread_mutex_unlock(&recvMutex);
                return NULL;
            }
            for (int i = 0; i < bytesReceived; i++)
            {
                lastRecvBuffer[i] = buffer[i];
            }

            uint8_t function = lastRecvBuffer[6];

            if (function == FUNC_RECV_FRAME)
                CAN_RecvFrameHandler(&lastRecvBuffer[8], lastRecvBuffer[12], &lastRecvBuffer[13]);

            // In ra dữ liệu nhận được (cho debug)
            // printf("Received packet: ");
            // for (int i = 0; i < bytesReceived; i++) {
            //     printf("%02x ", buffer[i]);
            // }
            // printf("\n");
            pthread_mutex_unlock(&recvMutex);
        }
        else if (bytesReceived == 0)
        {
            printf("Connection closed by server\n");
            break;
        }
        else
        {
            perror("recv failed");
            break;
        }
    }
    return NULL;
}

void *resendThread(void *arg)
{
    while (1)
    {
        CheckAndResend();
        usleep(50000); 
    }
    return NULL;
}

void initResendThread()
{
    pthread_t thread;
    pthread_create(&thread, NULL, resendThread, NULL);
    pthread_detach(thread); 
}

// Hàm khởi tạo socket
void initialize_socketcc(void)
{
    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0)
    {
        printf("Socket creation error\n");
        exit(1); // Thoát nếu không tạo được socket
    }

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(5000);

    if (inet_pton(AF_INET, "192.168.0.10", &serv_addr.sin_addr) <= 0)
    {
        printf("Invalid address\n");
        close(sock);
        exit(1);
    }

    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
    {
        printf("Connection Failed\n");
        close(sock);
        exit(1);
    }
    printf("Socket initialized and connected\n");

    // Tạo thread nhận dữ liệu
    if (pthread_create(&recvThreadId, NULL, receiveThread, NULL) != 0)
    {
        perror("pthread_create failed");
        close(sock);
        exit(1);
    }
    initResendThread();

    sleep(1);
    SendTCP_OpenCloseCAN(true);
}

void SendTCP_Traffic(uint32_t laneID, uint32_t vehicleCount)
{
    if (laneID < 0 || laneID >= MAX_LANES)
        return;
    if (!isUpdatingFromAI)
        return;

    int laneIndex = laneID;
    LANE_STATUS *lane = &laneTable[laneIndex];

    lane->vehicleCount = vehicleCount;

    // printf("Updated Lane %u: vehicleCount=%u, timeCounter=%u\n", laneID, vehicleCount, lane->timeCounter);
}

// Hàm gửi lệnh mở/đóng CAN (chờ phản hồi)
bool SendTCP_OpenCloseCAN(bool isOpen)
{
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

    if (send(sock, pkt, sizeof(pkt), 0) < 0)
    {
        perror("send failed");
        return false;
    }

    switch (getResponse(FUNC_OPEN_CAN, 500))
    {
    case 0:
        return true;
    default:
        return false;
    }
}

// Hàm chính
// int main()
// {

//     // Khởi tạo socket
//     initialize_socketcc();

//     sleep(2);
//     bool result = SendTCP_OpenCloseCAN(true);
//     printf("Open CAN Result: %d\n", result);
//     sleep(2);

//     // Gửi lệnh ví dụ và chạy liên tục
//     while (1)
//     {

//         SendTCP_Traffic(0x01, 5); // 5 phương tiện
//         usleep(50000);                 // 50ms, mô phỏng AI gọi nhanh
//         SendTCP_Traffic(0x02, 5); // 5 phương tiện
//         usleep(50000);                 // 50ms, mô phỏng AI gọi nhanh
//         SendTCP_Traffic(0x03, 5); // 5 phương tiện
//         usleep(50000);                 // 50ms, mô phỏng AI gọi nhanh
//         SendTCP_Traffic(0x04, 5); // 5 phương tiện
//         usleep(50000);                 // 50ms, mô phỏng AI gọi nhanh
//     }

//     // Không bao giờ đến đây do vòng lặp vô hạn
//     close(sock);
//     return 0;
// }
