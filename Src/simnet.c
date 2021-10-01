/*
 * simnet.c
 *
 *  Created on: Sep 23, 2021
 *      Author: janoko
 */


#include "stm32f4xx_hal.h"
#include "simcom.h"
#include "simnet.h"
#include <string.h>



void SIM_SOCK_SetAddr(SIM_Socket *sock, const char *host, uint16_t port)
{
  char *sockIP = sock->host;
  while(*host != '\0'){
    *sockIP = *host;
    host++;
    sockIP++;
  }

  sock->port = port;
}


int8_t SIM_SOCK_Open(SIM_Socket *sock, SIM_HandlerTypedef *hsim)
{
  int8_t linkNum = -1;

  if(sock->timeout == 0) sock->timeout = SIM_SOCK_DEFAULT_TO;
  linkNum = SIM_SockOpenTCPIP(hsim, sock->host, sock->port);
  if(linkNum != -1){
    SIM_SOCK_SET_STATUS(sock, SIM_SOCK_STATUS_OPEN);
    sock->hsim = hsim;
    sock->linkNum = linkNum;
    hsim->net.sockets[linkNum] = (void*)sock;
  }

  return linkNum;
}


uint16_t SIM_SOCK_SendData(SIM_Socket *sock, const uint8_t *data, uint16_t length)
{
  if(!SIM_SOCK_IS_STATUS(sock, SIM_SOCK_STATUS_OPEN)) return 0;
  SIM_SockSendData(sock->hsim, sock->linkNum, data, length);
  return 0;
}
