#ifndef SENDTCP_H
#define SENDTCP_H

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <unistd.h>

#define MAX_LANES 4

#define FUNC_OPEN_CAN 0x00
#define FUNC_SEND_FRAME 0x01
#define FUNC_RECV_FRAME 0x02

#define DEFAULT_CAN_CHANNEL 0x00 // channel 1

typedef enum
{
    CMD_CYCLE = 0x0000,
    CMD_REQUEST = 0x0001,
    CMD_ACK = 0x0002,
    CMD_RESPONSE = 0x0100,
} COMMAND_CODE;

typedef enum
{
    LEDColor_UNKNOWN = 0x00,
    LEDColor_RED,
    LEDColor_YELLOW,
    LEDColor_GREEN
} LED_COLOR;

typedef struct {
    uint8_t laneID;         
    uint32_t vehicleCount;    
    uint32_t timeCounter;
    uint32_t timeRemain;      
    LED_COLOR currentColor;    
    LED_COLOR nextColor;       
    bool isWaitingAck;
    bool acked;
    struct timespec lastSendTime; 
} LANE_STATUS;

#ifdef __cplusplus
extern "C" {
#endif

bool SendTCP_OpenCloseCAN(bool isOpen);
void initialize_socketcc(void);
void SendTCP_Traffic(uint32_t laneID, uint32_t vehicleCount);

#ifdef __cplusplus
}
#endif

#endif
