/*
 * simnet.c
 *
 *  Created on: Sep 23, 2021
 *      Author: janoko
 */


#include "../Include/simcom.h"
#include "../Include/simcom/conf.h"
#include "../Include/simcom/utils.h"
#include "../Include/simcom/socket.h"
#include <stdio.h>
#include <string.h>


void SIM_NetOpen(SIM_HandlerTypeDef *hsim)
{
  uint8_t resp;

  if (SIM_IS_STATUS(hsim, SIM_STAT_NET_OPENING)) return;

  SIM_LockCMD(hsim);

  // check net state
  SIM_SendCMD(hsim, "AT+NETOPEN?", 11);
  if (SIM_GetResponse(hsim, "+NETOPEN", 8, &resp, 1, SIM_GETRESP_WAIT_OK, 1000) == SIM_RESP_OK) {
    if (resp == '1') { // net already open;
      SIM_SET_STATUS(hsim, SIM_STAT_NET_OPEN);

      // TCP/IP Config
      SIM_SendCMD(hsim, "AT+CIPCCFG=10,0,1,1,1,1,10000", 29);
      if (SIM_IsResponseOK(hsim)) {
      }
    } else {
      SIM_SendCMD(hsim, "AT+NETOPEN", 10);
      SIM_Delay(1000);
      if (SIM_IsResponseOK(hsim)){
        SIM_SET_STATUS(hsim, SIM_STAT_NET_OPENING);
      }
    }
  }

  SIM_UnlockCMD(hsim);
}


/*
 * return linknum if connected
 * return -1 if not connected
 */
int8_t SIM_SockOpenTCPIP(SIM_HandlerTypeDef *hsim, const char *host, uint16_t port)
{
  int8_t linkNum = -1;
  char cmd[128];
  uint8_t resp[4];

  if (!SIM_IS_STATUS(hsim, SIM_STAT_NET_OPEN))
  {
    return -1;
  }
  
  for (int16_t i = 0; i < SIM_MAX_SOCKET; i++) {
    if (hsim->net.sockets[i] == NULL) {
      linkNum = i;
      break;
    }
  }
  if (linkNum == -1) return linkNum;

  SIM_LockCMD(hsim);

  sprintf(cmd, "AT+CIPOPEN=%d,\"TCP\",\"%s\",%d", linkNum, host, port);
  SIM_SendCMD(hsim, cmd, strlen(cmd));

  memset(resp, 0, 4);
  if (SIM_GetResponse(hsim, "+CIPOPEN", 8, resp, 3, SIM_GETRESP_WAIT_OK, 15000) == SIM_RESP_OK) {
    if (resp[3] != '0') linkNum = -1;
  }

  SIM_UnlockCMD(hsim);
  return linkNum;
}


void SIM_SockClose(SIM_HandlerTypeDef *hsim, uint8_t linkNum)
{
  SIM_LockCMD(hsim);
  SIM_UnlockCMD(hsim);
}


void SIM_SockSendData(SIM_HandlerTypeDef *hsim, int8_t linkNum, const uint8_t *data, uint16_t length)
{
  char cmd[20];
  uint8_t resp;

  SIM_LockCMD(hsim);

  sprintf(cmd, "AT+CIPSEND=%d,%d\r", linkNum, length);
  SIM_SendData(hsim, (uint8_t*)cmd, strlen(cmd));
  SIM_WaitResponse(hsim, "\r\n>", 3, 3000);

  SIM_SendData(hsim, data, length);
  SIM_Delay(5000);
  if (SIM_GetResponse(hsim, "+CIPSEND", 8, &resp, 1, SIM_GETRESP_WAIT_OK, 5000) == SIM_RESP_OK) {
  }

  SIM_UnlockCMD(hsim);
}


void SIM_SockAddListener(SIM_HandlerTypeDef *hsim, uint8_t linkNum, SIM_SockListener *listener)
{
  hsim->net.sockets[linkNum] = listener;
}


void SIM_SockRemoveListener(SIM_HandlerTypeDef *hsim, uint8_t linkNum)
{
  hsim->net.sockets[linkNum] = NULL;
}


void SIM_SOCK_SetAddr(SIM_Socket *sock, const char *host, uint16_t port)
{
  char *sockIP = sock->host;
  while (*host != '\0') {
    *sockIP = *host;
    host++;
    sockIP++;
  }

  sock->port = port;
}

void SIM_SOCK_SetBuffer(SIM_Socket *sock, uint8_t *buffer, uint16_t size)
{
  sock->buffer = buffer;
  sock->listener.buffer = buffer;
  sock->listener.bufferSize = size;
}

int8_t SIM_SOCK_Open(SIM_Socket *sock, SIM_HandlerTypeDef *hsim)
{
  int8_t linkNum = -1;

  if (sock->timeout == 0) sock->timeout = SIM_SOCK_DEFAULT_TO;

  linkNum = SIM_SockOpenTCPIP(hsim, sock->host, sock->port);
  if (linkNum != -1){
    SIM_SOCK_SET_STATUS(sock, SIM_SOCK_STATUS_OPEN);
    sock->hsim = hsim;
    sock->linkNum = linkNum;
    hsim->net.sockets[linkNum] = (void*)sock;

    SIM_SockAddListener(hsim, linkNum, &(sock->listener));
  }

  return linkNum;
}


void SIM_SOCK_Close(SIM_Socket *sock)
{

}


uint16_t SIM_SOCK_SendData(SIM_Socket *sock, const uint8_t *data, uint16_t length)
{
  if (!SIM_SOCK_IS_STATUS(sock, SIM_SOCK_STATUS_OPEN)) return 0;
  SIM_SockSendData(sock->hsim, sock->linkNum, data, length);
  return 0;
}


void SIM_SOCK_OnReceiveData(SIM_Socket *sock, void (*onReceive)(uint16_t))
{
  sock->listener.onReceive = onReceive;
}
