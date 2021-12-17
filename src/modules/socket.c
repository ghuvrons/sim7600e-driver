/*
 * simnet.c
 *
 *  Created on: Sep 23, 2021
 *      Author: janoko
 */



#include "../include/simcom.h"
#include "../include/simcom/conf.h"
#include "../include/simcom/utils.h"
#include "../include/simcom/socket.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#if SIM_EN_FEATURE_SOCKET
// event handlers
static void onNetOpen(SIM_HandlerTypeDef*);
static void onSockReceive(SIM_HandlerTypeDef*);
static void onSockClose(SIM_HandlerTypeDef*);


uint8_t SIM_NetCheckAsyncResponse(SIM_HandlerTypeDef *hsim)
{
  uint8_t isGet = 0;

  if ((isGet = SIM_IsResponse(hsim, "+RECEIVE", 8))) {
    onSockReceive(hsim);
  }

  else if ((isGet = (hsim->respBufferLen == 11 && SIM_IsResponse(hsim, "+NETOPEN", 8)))) {
    onNetOpen(hsim);
  }

  else if ((isGet = SIM_IsResponse(hsim, "+IPCLOSE", 8))) {
    onSockClose(hsim);
  }

  else if ((isGet = SIM_IsResponse(hsim, "+CIPEVENT", 9))) {
    if (strncmp((const char *)&(hsim->respBuffer[11]), "NETWORK CLOSED", 14)) {
      SIM_UNSET_STATE(hsim, SIM_STATE_NET_OPEN);
    }
  }

  return isGet;
}


void SIM_NetOpen(SIM_HandlerTypeDef *hsim)
{
  uint8_t resp;

  if (SIM_IS_STATE(hsim, SIM_STATE_NET_OPENING)) return;

  SIM_LockCMD(hsim);

  // check net state
  SIM_SendCMD(hsim, "AT+NETOPEN?");
  if (SIM_GetResponse(hsim, "+NETOPEN", 8, &resp, 1, SIM_GETRESP_WAIT_OK, 1000) == SIM_OK) {
    if (resp == '1') { // net already open;
      SIM_SET_STATE(hsim, SIM_STATE_NET_OPEN);

      // TCP/IP Config
      SIM_SendCMD(hsim, "AT+CIPCCFG=10,0,1,1,1,1,10000");
      if (SIM_IsResponseOK(hsim)) {
      }
    } else {
      SIM_SendCMD(hsim, "AT+NETOPEN");
      SIM_Delay(1000);
      if (SIM_IsResponseOK(hsim)){
        SIM_SET_STATE(hsim, SIM_STATE_NET_OPENING);
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
  uint8_t resp[4];

  if (!SIM_IS_STATE(hsim, SIM_STATE_NET_OPEN))
  {
    return -1;
  }
  
  for (int16_t i = 0; i < SIM_NUM_OF_SOCKET; i++) {
    if (hsim->net.sockets[i] == NULL) {
      linkNum = i;
      break;
    }
  }
  if (linkNum == -1) return linkNum;

  SIM_LockCMD(hsim);
  SIM_SendCMD(hsim, "AT+CIPOPEN=%d,\"TCP\",\"%s\",%d", host, port);

  memset(resp, 0, 4);
  if (SIM_GetResponse(hsim, "+CIPOPEN", 8, resp, 3, SIM_GETRESP_WAIT_OK, 15000) == SIM_OK) {
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
  if (SIM_GetResponse(hsim, "+CIPSEND", 8, &resp, 1, SIM_GETRESP_WAIT_OK, 5000) == SIM_OK) {
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


void SIM_SOCK_SetAddr(SIM_Socket_t *sock, const char *host, uint16_t port)
{
  char *sockIP = sock->host;
  while (*host != '\0') {
    *sockIP = *host;
    host++;
    sockIP++;
  }

  sock->port = port;
}

void SIM_SOCK_SetBuffer(SIM_Socket_t *sock, uint8_t *buffer, uint16_t size)
{
  sock->buffer = buffer;
  sock->listener.buffer = buffer;
  sock->listener.bufferSize = size;
}

int8_t SIM_SOCK_Open(SIM_Socket_t *sock, SIM_HandlerTypeDef *hsim)
{
  int8_t linkNum = -1;

  if (sock->timeout == 0) sock->timeout = SIM_SOCK_DEFAULT_TO;

  linkNum = SIM_SockOpenTCPIP(hsim, sock->host, sock->port);
  if (linkNum != -1){
    SIM_SOCK_SET_STATE(sock, SIM_SOCK_STATE_OPEN);
    sock->hsim = hsim;
    sock->linkNum = linkNum;
    hsim->net.sockets[linkNum] = (void*)sock;

    SIM_SockAddListener(hsim, linkNum, &(sock->listener));
  }

  return linkNum;
}


void SIM_SOCK_Close(SIM_Socket_t *sock)
{

}


uint16_t SIM_SOCK_SendData(SIM_Socket_t *sock, const uint8_t *data, uint16_t length)
{
  if (!SIM_SOCK_IS_STATE(sock, SIM_SOCK_STATE_OPEN)) return 0;
  SIM_SockSendData(sock->hsim, sock->linkNum, data, length);
  return 0;
}


void SIM_SOCK_OnReceiveData(SIM_Socket_t *sock, void (*onReceived)(uint16_t))
{
  sock->listener.onReceived = onReceived;
}



static void onNetOpen(SIM_HandlerTypeDef *hsim)
{
  if (!SIM_IS_STATE(hsim, SIM_STATE_NET_OPENING)) return;

  // skip string "+NETOPEN: " and read next data
  if (hsim->respBuffer[10] == '0') {
    SIM_UNSET_STATE(hsim, SIM_STATE_NET_OPENING);
    SIM_SET_STATE(hsim, SIM_STATE_NET_OPEN);

    // TCP/IP Config
    SIM_SendCMD(hsim, "AT+CIPCCFG=10,0,1,1,1,1,10000");
    if (SIM_IsResponseOK(hsim)) {
    }
  }

  else {
    SIM_UNSET_STATE(hsim, SIM_STATE_NET_OPENING);
  }
}


static void onSockReceive(SIM_HandlerTypeDef *hsim)
{
  const uint8_t *nextBuf = NULL;
  char linkNum_str[2];
  char dataLen_str[5];
  uint8_t linkNum;
  uint16_t dataLen;
  SIM_SockListener *sockListener;

  memset(linkNum_str, 0, 2);
  memset(dataLen_str, 0, 5);

  // skip string "+RECEIVE" and read next data
  nextBuf = SIM_ParseStr(&hsim->respBuffer[9], ',', 0, (uint8_t*) linkNum_str);
  SIM_ParseStr(nextBuf, ',', 0, (uint8_t*) dataLen_str);
  linkNum = (uint8_t) atoi(linkNum_str);
  dataLen = (uint16_t) atoi(dataLen_str);

  if (linkNum < SIM_NUM_OF_SOCKET && hsim->net.sockets[linkNum] != NULL) {
    sockListener = hsim->net.sockets[linkNum];
    if (dataLen > sockListener->bufferSize) dataLen = sockListener->bufferSize;

    dataLen = SIM_GetData(hsim, sockListener->buffer, dataLen, 1000);
    if (sockListener->onReceived != NULL)
      sockListener->onReceived(dataLen);
  }
}


static void onSockClose(SIM_HandlerTypeDef *hsim)
{
  uint8_t linkNum;
  char linkNum_str[2];
  SIM_SockListener *sockListener;

  memset(linkNum_str, 0, 2);
  // skip string "+IPCLOSE" and read next data
  SIM_ParseStr(&hsim->respBuffer[10], ',', 0, (uint8_t*) linkNum_str);
  linkNum = (uint8_t) atoi(linkNum_str);

  if (linkNum < SIM_NUM_OF_SOCKET && hsim->net.sockets[linkNum] != NULL) {
    sockListener = hsim->net.sockets[linkNum];
    if (sockListener->onClosed != NULL)
      sockListener->onClosed();
    hsim->net.sockets[linkNum] = NULL;
  }
}

#endif /* SIM_EN_FEATURE_SOCKET */
