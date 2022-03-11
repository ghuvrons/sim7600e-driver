/*
 * simnet.h
 *
 *  Created on: Sep 20, 2021
 *      Author: janoko
 */

#ifndef SIM5320E_INC_SIMNET_H_
#define SIM5320E_INC_SIMNET_H_

#include "conf.h"
#if SIM_EN_FEATURE_SOCKET

#include "../simcom.h"

#define SIM_SOCK_DEFAULT_TO 2000

#define SIM_SOCK_UDP    0
#define SIM_SOCK_TCPIP  1

#define SIM_SOCK_STATE_CLOSED   0x01
#define SIM_SOCK_STATE_OPENING  0x02
#define SIM_SOCK_STATE_OPEN     0x04

#define SIM_SOCK_SUCCESS 0
#define SIM_SOCK_ERROR   1

#define SIM_SOCK_EVENT_ON_OPENED        0x01
#define SIM_SOCK_EVENT_ON_OPENING_ERROR 0x02
#define SIM_SOCK_EVENT_ON_RECEIVED      0x04
#define SIM_SOCK_EVENT_ON_CLOSED        0x08

#define SIM_SOCK_IS_STATE(sock, stat)    ((sock)->state == stat)
#define SIM_SOCK_SET_STATE(sock, stat)   ((sock)->state = stat)

typedef struct {
  SIM_HandlerTypeDef  *hsim;
  uint8_t             state;
  uint8_t             events;               // Events flag
  int8_t              linkNum;
  uint8_t             type;                 // SIM_SOCK_UDP or SIM_SOCK_TCPIP

  // configuration
  struct {
    uint32_t timeout;
    uint8_t  autoReconnect;
    uint16_t reconnectingDelay;
  } config;

  // tick register for delay and timeout
  struct {
    uint32_t reconnDelay;
    uint32_t connecting;
  } tick;

  // server
  char     host[64];
  uint16_t port;

  // listener
  struct {
    void (*onConnecting)(void);
    void (*onConnected)(void);
    void (*onConnectError)(void);
    void (*onClosed)(void);
    void (*onReceived)(STRM_Buffer_t*);
  } listeners;

  // buffer
  STRM_Buffer_t buffer;
} SIM_Socket_t;

uint8_t SIM_NetCheckAsyncResponse(SIM_HandlerTypeDef*);
void    SIM_NetHandleEvents(SIM_HandlerTypeDef*);

// simcom feature net and socket
void          SIM_NetOpen(SIM_HandlerTypeDef*);
SIM_Status_t  SIM_SockOpenTCPIP(SIM_HandlerTypeDef*, int8_t *linkNum, const char *host, uint16_t port);
void          SIM_SockClose(SIM_HandlerTypeDef*, uint8_t linkNum);
void          SIM_SockRemoveListener(SIM_HandlerTypeDef*, uint8_t linkNum);
uint16_t      SIM_SockSendData(SIM_HandlerTypeDef*, int8_t linkNum, const uint8_t *data, uint16_t length);

// socket method
SIM_Status_t  SIM_SOCK_Init(SIM_Socket_t*, const char *host, uint16_t port);
void          SIM_SOCK_SetBuffer(SIM_Socket_t*, uint8_t *buffer, uint16_t size);
SIM_Status_t  SIM_SOCK_Open(SIM_Socket_t*, SIM_HandlerTypeDef*);
void          SIM_SOCK_Close(SIM_Socket_t*);
uint16_t      SIM_SOCK_SendData(SIM_Socket_t*, const uint8_t *data, uint16_t length);

#endif /* SIM5300E_INC_SIMNET_H_ */
#endif /* SIM_EN_FEATURE_SOCKET */
