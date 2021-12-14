/*
 * modem.h
 *
 *  Created on: Sep 14, 2021
 *      Author: janoko
 */

#ifndef SIM5300E_INC_SIMCOM_H_
#define SIM5300E_INC_SIMCOM_H_

#include "stm32f4xx_hal.h"
#include "simcom/conf.h"
#include <dma_streamer.h>

#define SIM_BUFFER_SIZE 256

/**
 * SIM STATUS
 * bit  0   is device connected
 *      1   is uart reading
 *      2   is uart writting
 *      3   is cmd running
 *      4   is net opened
 */

#define SIM_STATUS_START        0x01
#define SIM_STATUS_CONNECT      0x02
#define SIM_STATUS_UART_READING 0x04
#define SIM_STATUS_UART_WRITING 0x08
#define SIM_STATUS_CMD_RUNNING  0x10
#define SIM_STATUS_NET_OPEN     0x20
#define SIM_STATUS_NET_OPENING  0x40

#define SIM_RESP_TIMEOUT  0
#define SIM_RESP_ERROR    1
#define SIM_RESP_OK       2

#define SIM_GETRESP_WAIT_OK   0
#define SIM_GETRESP_ONLY_DATA 1

typedef uint8_t (*asyncResponseHandler) (uint16_t bufLen);

typedef struct {
  void      (*onReceived)(uint16_t);
  uint16_t  (*onClosed)(void);
  uint16_t  bufferSize;
  uint8_t   *buffer;
} SIM_SockListener;

typedef struct {
  uint8_t             status;
  STRM_handlerTypeDef *dmaStreamer;
  uint8_t             buffer[SIM_BUFFER_SIZE];
  uint16_t            bufferLen;
  uint32_t            timeout;

#ifdef SIM_EN_FEATURE_SOCKET
  struct {
    uint32_t          (*onOpened)(void);
    SIM_SockListener  *sockets[SIM_NUM_OF_SOCKET];
  } net;
#endif

} SIM_HandlerTypeDef;

typedef struct {
  uint8_t year;
  uint8_t month;
  uint8_t day;
  uint8_t hour;
  uint8_t minute;
  uint8_t second;
} SIM_Datetime;


void SIM_LockCMD(SIM_HandlerTypeDef*);
void SIM_UnlockCMD(SIM_HandlerTypeDef*);

void          SIM_Init(SIM_HandlerTypeDef*, STRM_handlerTypeDef*);
void          SIM_CheckAsyncResponse(SIM_HandlerTypeDef*, uint32_t timeout);
void          SIM_HandleAsyncResponse(SIM_HandlerTypeDef*);
void          SIM_CheckAT(SIM_HandlerTypeDef*);
SIM_Datetime  SIM_GetTime(SIM_HandlerTypeDef*);
void          SIM_HashTime(SIM_HandlerTypeDef*, char *hashed);
void          SIM_SendSms(SIM_HandlerTypeDef*);

// MACROS
#define SIM_IS_STATUS(hsim, stat)     (((hsim)->status & (stat)) != (stat))
#define SIM_SET_STATUS(hsim, stat)    {(hsim)->status |= (stat);}
#define SIM_UNSET_STATUS(hsim, stat)  {(hsim)->status &= ~(stat);}

#endif /* SIM5300E_INC_SIMCOM_H_ */
