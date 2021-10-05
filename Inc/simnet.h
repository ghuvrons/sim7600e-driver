/*
 * simnet.h
 *
 *  Created on: Sep 20, 2021
 *      Author: janoko
 */

#ifndef SIM5300E_INC_SIMNET_H_
#define SIM5300E_INC_SIMNET_H_

#include "stm32f4xx_hal.h"
#include "simcom.h"

#define SIM_SOCK_DEFAULT_TO 2000

#define SIM_SOCK_UDP    0
#define SIM_SOCK_TCPIP  1

#define SIM_SOCK_STATUS_OPEN  0x01

#define SIM_SOCK_IS_STATUS(sock, stat) SIM_IS_STATUS(sock, stat)
#define SIM_SOCK_SET_STATUS(sock, stat) SIM_SET_STATUS(sock, stat)
#define SIM_SOCK_UNSET_STATUS(sock, stat) SIM_UNSET_STATUS(sock, stat)

#define SIM_SOCK_SUCCESS 0
#define SIM_SOCK_ERROR   1


typedef struct {
  SIM_HandlerTypedef *hsim;
  uint8_t status;
  uint8_t linkNum;
  uint8_t type; // SIM_SOCK_UDP or SIM_SOCK_TCPIP
  uint32_t timeout;
  char host[64];
  uint16_t port;
  uint8_t *buffer;
  SIM_SockListener listener;
} SIM_Socket;

void SIM_SOCK_SetAddr(SIM_Socket*, const char *host, uint16_t port);
void SIM_SOCK_SetBuffer(SIM_Socket*, uint8_t *buffer, uint16_t size);
int8_t SIM_SOCK_Open(SIM_Socket*, SIM_HandlerTypedef*);
void SIM_SOCK_Close(SIM_Socket*);
uint16_t SIM_SOCK_SendData(SIM_Socket*, const uint8_t *data, uint16_t length);
void SIM_SOCK_OnReceiveData(SIM_Socket*, void (*onReceive)(uint16_t));

#endif /* SIM5300E_INC_SIMNET_H_ */
