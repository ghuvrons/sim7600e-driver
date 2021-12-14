/*
 * simnet.h
 *
 *  Created on: Sep 20, 2021
 *      Author: janoko
 */

#ifdef SIM_EN_FEATURE_SOCKET
#ifndef SIM5320E_INC_SIMNET_H_
#define SIM5320E_INC_SIMNET_H_

#include "../simcom.h"

#define SIM_SOCK_DEFAULT_TO 2000

#define SIM_SOCK_UDP    0
#define SIM_SOCK_TCPIP  1

#define SIM_SOCK_STATUS_OPEN  0x01

#define SIM_SOCK_IS_STATUS(sock, stat)    SIM_IS_STATUS(sock, stat)
#define SIM_SOCK_SET_STATUS(sock, stat)   SIM_SET_STATUS(sock, stat)
#define SIM_SOCK_UNSET_STATUS(sock, stat) SIM_UNSET_STATUS(sock, stat)

#define SIM_SOCK_SUCCESS 0
#define SIM_SOCK_ERROR   1


typedef struct {
  SIM_HandlerTypeDef  *hsim;
  uint8_t             status;
  uint8_t             linkNum;
  uint8_t             type;                 // SIM_SOCK_UDP or SIM_SOCK_TCPIP
  uint32_t            timeout;
  char                host[64];
  uint16_t            port;
  uint8_t             *buffer;
  SIM_SockListener    listener;
} SIM_Socket_t;

// simcom feature net and socket
void    SIM_NetOpen(SIM_HandlerTypeDef*);
int8_t  SIM_SockOpenTCPIP(SIM_HandlerTypeDef*, const char *host, uint16_t port);
void    SIM_SockClose(SIM_HandlerTypeDef*, uint8_t linkNum);
void    SIM_SockAddListener(SIM_HandlerTypeDef*, uint8_t linkNum, SIM_SockListener*);
void    SIM_SockRemoveListener(SIM_HandlerTypeDef*, uint8_t linkNum);
void    SIM_SockSendData(SIM_HandlerTypeDef*, int8_t linkNum, const uint8_t *data, uint16_t length);
uint8_t SIM_NetCheckAsyncResponse(SIM_HandlerTypeDef*);

// socket method
void      SIM_SOCK_SetAddr(SIM_Socket_t*, const char *host, uint16_t port);
void      SIM_SOCK_SetBuffer(SIM_Socket_t*, uint8_t *buffer, uint16_t size);
int8_t    SIM_SOCK_Open(SIM_Socket_t*, SIM_HandlerTypeDef*);
void      SIM_SOCK_Close(SIM_Socket_t*);
uint16_t  SIM_SOCK_SendData(SIM_Socket_t*, const uint8_t *data, uint16_t length);
void      SIM_SOCK_OnReceiveData(SIM_Socket_t*, void (*onReceive)(uint16_t));

#endif /* SIM5300E_INC_SIMNET_H_ */
#endif /* SIM_EN_FEATURE_SOCKET */
