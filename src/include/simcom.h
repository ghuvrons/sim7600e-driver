/*
 * modem.h
 *
 *  Created on: Sep 14, 2021
 *      Author: janoko
 */

#ifndef SIM7600E_INC_SIMCOM_H_
#define SIM7600E_INC_SIMCOM_H_

#include "stm32f4xx_hal.h"
#include "simcom/conf.h"
#include <dma_streamer.h>

#if SIM_EN_FEATURE_GPS
#include "lwgps/lwgps.h"
#endif

/**
 * SIM STATUS
 * bit  0   is device connected
 *      1   is uart reading
 *      2   is uart writting
 *      3   is cmd running
 *      4   is net opened
 */

#define SIM_STATUS_START            0x01
#define SIM_STATUS_ACTIVE           0x02
#define SIM_STATUS_SIM_INSERTED     0x04
#define SIM_STATUS_REGISTERED       0x08
#define SIM_STATUS_ROAMING          0x10
#define SIM_STATUS_UART_READING     0x20
#define SIM_STATUS_UART_WRITING     0x40
#define SIM_STATUS_CMD_RUNNING      0x80

#define SIM_GETRESP_WAIT_OK   0
#define SIM_GETRESP_ONLY_DATA 1

#define SIM_EVENT_ON_STARTING   0x01
#define SIM_EVENT_ON_STARTED    0x02
#define SIM_EVENT_ON_REGISTERED 0x04

// MACROS


typedef uint8_t (*asyncResponseHandler) (uint16_t bufLen);
typedef enum {
  SIM_OK,
  SIM_ERROR,
  SIM_TIMEOUT
} SIM_Status_t;

typedef struct {
  uint8_t year;
  uint8_t month;
  uint8_t day;
  uint8_t hour;
  uint8_t minute;
  uint8_t second;
  int8_t  timezone;
} SIM_Datetime;

typedef struct {
  uint8_t             status;
  uint8_t             events;
  uint8_t             errors;
  uint8_t             signal;
  uint32_t            timeout;
  STRM_handlerTypeDef *dmaStreamer;

  #if SIM_EN_FEATURE_NET
  struct {
    uint8_t status;
    uint8_t events;

    struct {
      const char *APN;
      const char *user;
      const char *pass;
    } APN;

    void (*onOpening)(void);
    void (*onOpened)(void);
    void (*onOpenError)(void);
    void (*onClosed)(void);

    #if SIM_EN_FEATURE_SOCKET
    void *sockets[SIM_NUM_OF_SOCKET];
    #endif

  } net;

  #if SIM_EN_FEATURE_NTP
  struct {
    const char  *server;
    int8_t      region;
    uint32_t    syncTick;
    void (*onSynced)(SIM_Datetime);
    
    struct {
      uint32_t retryInterval;
      uint32_t resyncInterval;
    } config;
  } NTP;
  #endif /* SIM_EN_FEATURE_NTP */
  #endif /* SIM_EN_FEATURE_NET */

  #if SIM_EN_FEATURE_NET && SIM_EN_FEATURE_MQTT
  struct {
    uint8_t status;
    uint8_t events;
  } mqtt;
  #endif

  #if SIM_EN_FEATURE_GPS
  struct {
    uint8_t   status;
    uint8_t   events;
    Buffer_t  buffer;
    uint8_t   readBuffer[SIM_GPS_TMP_BUF_SIZE];
    lwgps_t   lwgps;
  } gps;
  #endif

  // Buffers
  uint8_t  respBuffer[SIM_RESP_BUFFER_SIZE];
  uint16_t respBufferLen;

  char     cmdBuffer[SIM_CMD_BUFFER_SIZE];
  uint16_t cmdBufferLen;

  uint32_t  initAt;
} SIM_HandlerTypeDef;


extern uint8_t SIM_CmdTmp[64];
extern uint8_t SIM_RespTmp[64];

void SIM_LockCMD(SIM_HandlerTypeDef*);
void SIM_UnlockCMD(SIM_HandlerTypeDef*);

void          SIM_Init(SIM_HandlerTypeDef*, STRM_handlerTypeDef*);
void          SIM_CheckAnyResponse(SIM_HandlerTypeDef*);
void          SIM_CheckAsyncResponse(SIM_HandlerTypeDef*);
void          SIM_HandleEvents(SIM_HandlerTypeDef*);
void          SIM_Echo(SIM_HandlerTypeDef*, uint8_t onoff);
uint8_t       SIM_CheckAT(SIM_HandlerTypeDef*);
uint8_t       SIM_CheckSignal(SIM_HandlerTypeDef*);
uint8_t       SIM_CheckSIMCard(SIM_HandlerTypeDef*);
uint8_t       SIM_ReqisterNetwork(SIM_HandlerTypeDef*);
SIM_Datetime  SIM_GetTime(SIM_HandlerTypeDef*);
void          SIM_HashTime(SIM_HandlerTypeDef*, char *hashed);
void          SIM_SendSms(SIM_HandlerTypeDef*);
void          SIM_SendUSSD(SIM_HandlerTypeDef*, const char *ussd);

#endif /* SIM7600E_INC_SIMCOM_H_ */
