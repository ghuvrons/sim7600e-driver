/*
 * modem.h
 *
 *  Created on: Sep 14, 2021
 *      Author: janoko
 */

#ifndef SIM5300E_INC_SIMCOM_H_
#define SIM5300E_INC_SIMCOM_H_

#include "stm32f4xx_hal.h"
#include "dma_streamer.h"

#define SIM_BUFFER_SIZE 256
#define SIM_MAX_SOCKET  4

/**
 * SIM STATUS
 * bit  0   is device connected
 *      1   is uart reading
 *      2   is uart writting
 *      3   is cmd running
 *      4   is net opened
 */

#define SIM_STAT_CONNECT      0x01
#define SIM_STAT_UART_READING 0x02
#define SIM_STAT_UART_WRITING 0x04
#define SIM_STAT_CMD_RUNNING  0x08
#define SIM_STAT_NET_OPEN     0x10

#define SIM_RESP_TIMEOUT  0
#define SIM_RESP_ERROR    1
#define SIM_RESP_OK       2

#define SIM_IS_STATUS(hsim, stat) (((hsim)->status & (stat)) != 0)
#define SIM_SET_STATUS(hsim, stat) {(hsim)->status |= (stat);}
#define SIM_UNSET_STATUS(hsim, stat) {(hsim)->status &= ~(stat);}

typedef struct {
  void (*onReceive)(uint16_t);
  uint16_t (*onClose)(void);
  uint16_t bufferSize;
  uint8_t *buffer;
} SIM_SockListener;

typedef struct {
  STRM_handlerTypedef *dmaStreamer;
  uint8_t buffer[SIM_BUFFER_SIZE];
  uint8_t state;
  uint8_t status;
  uint32_t timeout;

//  SIM_NET_HandlerTypedef net;
  struct {
    uint8_t state;
    uint32_t (*onOpened)(void);
    SIM_SockListener* sockets[SIM_MAX_SOCKET];
  } net;
} SIM_HandlerTypedef;

typedef struct {
  uint8_t year;
  uint8_t month;
  uint8_t day;
  uint8_t hour;
  uint8_t minute;
  uint8_t second;
} SIM_Datetime;

uint32_t SIM_GetTick(void);
void SIM_Delay(uint32_t ms);
void SIM_LockCMD(SIM_HandlerTypedef*);
void SIM_UnlockCMD(SIM_HandlerTypedef*);
void SIM_Init(SIM_HandlerTypedef*, STRM_handlerTypedef*);
void SIM_checkAsyncResponse(SIM_HandlerTypedef*, uint32_t timeout);
uint16_t SIM_checkResponse(SIM_HandlerTypedef*, uint32_t timeout);
void SIM_CheckAT(SIM_HandlerTypedef*);
SIM_Datetime SIM_GetTime(SIM_HandlerTypedef*);
void SIM_HashTime(SIM_HandlerTypedef*, char *hashed);
void SIM_SendSms(SIM_HandlerTypedef*);
void SIM_NetOpen(SIM_HandlerTypedef*);
int8_t SIM_SockOpenTCPIP(SIM_HandlerTypedef*, const char *host, uint16_t port);
void SIM_SockAddListener(SIM_HandlerTypedef*, uint8_t linkNum, SIM_SockListener*);
void SIM_SockRemoveListener(SIM_HandlerTypedef*, uint8_t linkNum);
void SIM_SockSendData(SIM_HandlerTypedef*, int8_t linkNum, const uint8_t *data, uint16_t length);

#endif /* SIM5300E_INC_SIMCOM_H_ */
