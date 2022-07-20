/*
 * simnet.c
 *
 *  Created on: Sep 23, 2021
 *      Author: janoko
 */



#include "../include/simcom.h"
#include "../include/simcom/net.h"
#include "../include/simcom/socket.h"
#include "../include/simcom/utils.h"
#include "../include/simcom/debug.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#if SIM_EN_FEATURE_SOCKET

// event handlers
static void resetOpenedSocket(SIM_HandlerTypeDef*);
static void receiveData(SIM_HandlerTypeDef*);
static SIM_Status_t sockOpen(SIM_Socket_t*);

#define Get_Available_LinkNum(hsim, linkNum) {\
  for (int16_t i = 0; i < SIM_NUM_OF_SOCKET; i++) {\
    if ((hsim)->net.sockets[i] == NULL) {\
      *(linkNum) = i;\
      break;\
    }\
  }\
}


uint8_t SIM_SockCheckAsyncResponse(SIM_HandlerTypeDef *hsim)
{
  int8_t linkNum;
  SIM_Socket_t *socket;
  uint8_t isGet = 0;

  if ((isGet = SIM_IsResponse(hsim, "+RECEIVE", 8))) {
    receiveData(hsim);
  }

  else if ((isGet = (hsim->respBufferLen >= 13 && SIM_IsResponse(hsim, "+CIPOPEN", 8))))
  {
    linkNum   = (int8_t) atoi((char*)&(hsim->respBuffer[10]));
    int err   =          atoi((char*)&(hsim->respBuffer[12]));

    socket = (SIM_Socket_t*) hsim->net.sockets[linkNum];
    if (socket != NULL) {
      if (err == 0) {
        SIM_BITS_SET(socket->events, SIM_SOCK_EVENT_ON_OPENED);
        SIM_SOCK_SET_STATE(socket, SIM_SOCK_STATE_OPEN);
      } else {
        SIM_BITS_SET(socket->events, SIM_SOCK_EVENT_ON_OPENING_ERROR);
        SIM_SOCK_SET_STATE(socket, SIM_SOCK_STATE_CLOSED);
      }
    }
  }

  else if ((isGet = (hsim->respBufferLen >= 13 && SIM_IsResponse(hsim, "+IPCLOSE", 8))))
  {
    linkNum     = (int8_t) atoi((char*)&(hsim->respBuffer[10]));
    // int reason  =          atoi((char*)&(hsim->respBuffer[12]));

    socket = (SIM_Socket_t*) hsim->net.sockets[linkNum];
    if (socket != NULL) {
      SIM_BITS_SET(socket->events, SIM_SOCK_EVENT_ON_CLOSED);
      SIM_SOCK_SET_STATE(socket, SIM_SOCK_STATE_CLOSED);
    }
  }

  else if ((isGet = (hsim->respBufferLen >= 14 && hsim->respBufferLen <= 16
                    && SIM_IsResponse(hsim, "+CIPCLOSE", 9))))
  {
    linkNum = (int8_t) atoi((char*)&(hsim->respBuffer[11]));
    int err =          atoi((char*)&(hsim->respBuffer[12]));

    socket = (SIM_Socket_t*) hsim->net.sockets[linkNum];
    if (err == 0 && socket != NULL) {
      SIM_BITS_SET(socket->events, SIM_SOCK_EVENT_ON_CLOSED);
      SIM_SOCK_SET_STATE(socket, SIM_SOCK_STATE_CLOSED);
    }
  }

  return isGet;
}


void SIM_SockHandleEvents(SIM_HandlerTypeDef *hsim)
{
  int16_t i;
  SIM_Socket_t *socket;

  // Socket Event Handler
  for (i = 0; SIM_NET_IS_STATUS(hsim, SIM_NET_STATUS_OPEN) && i < SIM_NUM_OF_SOCKET; i++)
  {
    if ((socket = hsim->net.sockets[i]) != NULL) {
      if (SIM_BITS_IS(socket->events, SIM_SOCK_EVENT_ON_OPENED)) {
        SIM_BITS_UNSET(socket->events, SIM_SOCK_EVENT_ON_OPENED);
        if (socket->listeners.onConnected != NULL)
          socket->listeners.onConnected();
      }

      if (SIM_BITS_IS(socket->events, SIM_SOCK_EVENT_ON_OPENING_ERROR)) {
        SIM_BITS_UNSET(socket->events, SIM_SOCK_EVENT_ON_OPENING_ERROR);
        if (socket->config.autoReconnect) {
          socket->tick.reconnDelay = SIM_GetTick();
          if (socket->listeners.onConnectError != NULL)
            socket->listeners.onConnectError();
        }
      }

      if (SIM_BITS_IS(socket->events, SIM_SOCK_EVENT_ON_CLOSED)) {
        SIM_BITS_UNSET(socket->events, SIM_SOCK_EVENT_ON_CLOSED);
        if (!socket->config.autoReconnect)
          hsim->net.sockets[i] = NULL;
        else socket->tick.reconnDelay = SIM_GetTick();
        if (socket->listeners.onClosed != NULL)
          socket->listeners.onClosed();
      }

      if (SIM_BITS_IS(socket->events, SIM_SOCK_EVENT_ON_RECEIVED)) {
        SIM_BITS_UNSET(socket->events, SIM_SOCK_EVENT_ON_RECEIVED);
        if (socket->listeners.onReceived != NULL)
          socket->listeners.onReceived(&(socket->buffer));
      }

      // auto reconnect
      if (SIM_SOCK_IS_STATE(socket, SIM_SOCK_STATE_CLOSED)) {
        if (SIM_IsTimeout(socket->tick.reconnDelay, socket->config.reconnectingDelay)) {
          sockOpen(socket);
        }
      }
    }
  }
}


void SIM_SockOnStarted(SIM_HandlerTypeDef *hsim)
{
  SIM_Socket_t *socket;

  for (uint8_t i = 0; i < SIM_NUM_OF_SOCKET; i++) {
    if ((socket = hsim->net.sockets[i]) != NULL) {
      if (SIM_SOCK_IS_STATE(socket, SIM_SOCK_STATE_OPENING)) {
        if (socket->config.autoReconnect) {
          socket->tick.reconnDelay = SIM_GetTick();
          if (socket->listeners.onConnectError != NULL)
            socket->listeners.onConnectError();
        }
      }

      else if (SIM_SOCK_IS_STATE(socket, SIM_SOCK_STATE_OPEN)) {
        if (!socket->config.autoReconnect)
          hsim->net.sockets[i] = NULL;
        if (socket->listeners.onClosed != NULL)
          socket->listeners.onClosed();
      }
      SIM_SOCK_SET_STATE(socket, 0);
    }
  }
}


void SIM_SockOnNetOpened(SIM_HandlerTypeDef *hsim)
{
  resetOpenedSocket(hsim);
}


/*
 * return linknum if connected
 * return -1 if not connected
 */
SIM_Status_t SIM_SockOpenTCPIP(SIM_HandlerTypeDef *hsim, int8_t *linkNum, const char *host, uint16_t port)
{
  if (!SIM_NET_IS_STATUS(hsim, SIM_NET_STATUS_OPEN) || !SIM_NET_IS_STATUS(hsim, SIM_NET_STATUS_AVAILABLE))
  {
    return SIM_ERROR;
  }
  
  if (*linkNum == -1 || hsim->net.sockets[*linkNum] == NULL) {
    Get_Available_LinkNum(hsim, linkNum);
    if (*linkNum == -1) return SIM_ERROR;
  }

  SIM_LockCMD(hsim);
  SIM_SendCMD(hsim, "AT+CIPOPEN=%d,\"TCP\",\"%s\",%d", *linkNum, host, port);
  SIM_SOCK_SET_STATE((SIM_Socket_t*)hsim->net.sockets[*linkNum], SIM_SOCK_STATE_OPENING);

  if (SIM_IsResponseOK(hsim)) {
    SIM_UnlockCMD(hsim);
    return SIM_OK;
  }
  SIM_SOCK_SET_STATE((SIM_Socket_t*)hsim->net.sockets[*linkNum], SIM_SOCK_STATE_CLOSED);

  SIM_UnlockCMD(hsim);
  return SIM_ERROR;
}


SIM_Status_t SIM_SockClose(SIM_HandlerTypeDef *hsim, uint8_t linkNum)
{
  uint8_t *resp = &SIM_RespTmp[0];
  SIM_Socket_t *socket;

  SIM_LockCMD(hsim);

  memset(resp, 0, 20);
  SIM_SendCMD(hsim, "AT+CIPCLOSE?");
  if (SIM_GetResponse(hsim, "+CIPCLOSE", 9, resp, 19, SIM_GETRESP_WAIT_OK, 1000) == SIM_OK) {
    if (linkNum < 10 && *(resp+(linkNum*2)) == '1') {
      SIM_SendCMD(hsim, "AT+CIPCLOSE=%d", linkNum);
      if (SIM_IsResponseOK(hsim)) {
        SIM_UnlockCMD(hsim);
        return SIM_OK;
      }
    } else {
      socket = (SIM_Socket_t*) hsim->net.sockets[linkNum];
      if (socket != NULL) {
        SIM_BITS_SET(socket->events, SIM_SOCK_EVENT_ON_CLOSED);
        SIM_SOCK_SET_STATE(socket, SIM_SOCK_STATE_CLOSED);
      }
    }
  }

  SIM_UnlockCMD(hsim);
  return SIM_ERROR;
}


uint16_t SIM_SockSendData(SIM_HandlerTypeDef *hsim, int8_t linkNum, const uint8_t *data, uint16_t length)
{
  uint16_t sendLen = 0;
  uint8_t resp = 0;
  uint8_t *cmdTmp = &SIM_CmdTmp[0];

  SIM_LockCMD(hsim);

  sprintf((char*) cmdTmp, "AT+CIPSEND=%d,%d\r", linkNum, length);
  SIM_SendData(hsim, cmdTmp, strlen((char*)cmdTmp));
  if (!SIM_WaitResponse(hsim, ">", 1, 3000))
    goto endcmd;
  if (!SIM_SendData(hsim, data, length))
    goto endcmd;
  if (!SIM_IsResponseOK(hsim))
    goto endcmd;
  if (SIM_GetResponse(hsim, "+CIPSEND", 8, &resp, 1, SIM_GETRESP_ONLY_DATA, 5000) == SIM_OK) {
    sendLen = length;
  }
  else {
    sendLen = 0;
  }

  endcmd:
  SIM_UnlockCMD(hsim);
  return sendLen;
}


SIM_Status_t SIM_SOCK_Init(SIM_Socket_t *sock, const char *host, uint16_t port)
{
  char *sockIP = sock->host;
  while (*host != '\0') {
    *sockIP = *host;
    host++;
    sockIP++;
  }

  sock->port = port;

  if (sock->config.timeout == 0)
    sock->config.timeout = SIM_SOCK_DEFAULT_TO;
  if (sock->config.reconnectingDelay == 0)
    sock->config.reconnectingDelay = 5000;

  if (sock->buffer.buffer == NULL || sock->buffer.size == 0)
    return SIM_ERROR;

  SIM_SOCK_SET_STATE(sock, SIM_SOCK_STATE_CLOSED);
  return SIM_OK;
}


void SIM_SOCK_SetBuffer(SIM_Socket_t *sock, uint8_t *buffer, uint16_t size)
{
  sock->buffer.buffer = buffer;
  sock->buffer.size = size;
}


SIM_Status_t SIM_SOCK_Open(SIM_Socket_t *sock, SIM_HandlerTypeDef *hsim)
{
  SIM_Status_t status;
  sock->linkNum = -1;

  if (sock->config.autoReconnect) {
    Get_Available_LinkNum(hsim, &(sock->linkNum));
    if (sock->linkNum < 0) return SIM_ERROR;
    hsim->net.sockets[sock->linkNum] = (void*)sock;
    sock->hsim = hsim;
  }

  status = sockOpen(sock);
  if (status != SIM_OK && !sock->config.autoReconnect) {
    hsim->net.sockets[sock->linkNum] = NULL;
    sock->linkNum = -1;
  }

  return status;
}


static SIM_Status_t sockOpen(SIM_Socket_t *sock)
{
  if (SIM_SockOpenTCPIP(sock->hsim, &sock->linkNum, sock->host, sock->port) == SIM_OK) {
    if (sock->listeners.onConnecting != NULL) sock->listeners.onConnecting();
    return SIM_OK;
  }
  SIM_SOCK_SET_STATE(sock, SIM_SOCK_STATE_CLOSED);

  return SIM_ERROR;
}


void SIM_SOCK_Close(SIM_Socket_t *sock)
{
  SIM_SockClose(sock->hsim, sock->linkNum);
}


uint16_t SIM_SOCK_SendData(SIM_Socket_t *sock, const uint8_t *data, uint16_t length)
{
  if (!SIM_SOCK_IS_STATE(sock, SIM_SOCK_STATE_OPEN)) return 0;
  return SIM_SockSendData(sock->hsim, sock->linkNum, data, length);
}


static void resetOpenedSocket(SIM_HandlerTypeDef *hsim)
{
  uint8_t *resp = &SIM_RespTmp[0];
  uint8_t *closing_resp = &SIM_RespTmp[32];

  SIM_LockCMD(hsim);
  // TCP/IP Config
  SIM_SendCMD(hsim, "AT+CIPCCFG=10,0,0,1,1,0,10000");
  if (SIM_IsResponseOK(hsim)){}

  memset(resp, 0, 20);
  SIM_SendCMD(hsim, "AT+CIPCLOSE?");
  if (SIM_GetResponse(hsim, "+CIPCLOSE", 9, resp, 19, SIM_GETRESP_WAIT_OK, 1000) == SIM_OK) {
    for (uint8_t i = 0; i < 10; i++) {
      if (*(resp + (i*2)) == '1') {
        memset(closing_resp, 0, 3);
        SIM_SendCMD(hsim, "AT+CIPCLOSE=%d", i);
        if (SIM_GetResponse(hsim, "+CIPCLOSE", 9, closing_resp, 3, SIM_GETRESP_WAIT_OK, 1000) == SIM_OK) {

        }
      }
    }
    SIM_NET_SET_STATUS(hsim, SIM_NET_STATUS_AVAILABLE);
  }

  SIM_UnlockCMD(hsim);
}


static void receiveData(SIM_HandlerTypeDef *hsim)
{
  const uint8_t *nextBuf = NULL;
  uint8_t *linkNum_str = &SIM_RespTmp[0];
  uint8_t *dataLen_str = &SIM_RespTmp[8];
  uint8_t linkNum;
  uint16_t dataLen;
  uint16_t writeLen;
  SIM_Socket_t *socket;

  memset(linkNum_str, 0, 2);
  memset(dataLen_str, 0, 6);

  // skip string "+RECEIVE" and read next data
  nextBuf = SIM_ParseStr(&hsim->respBuffer[9], ',', 0, linkNum_str);
  SIM_ParseStr(nextBuf, ',', 0, dataLen_str);
  linkNum = (uint8_t) atoi((char*) linkNum_str);
  dataLen = (uint16_t) atoi((char*) dataLen_str);

  if (linkNum < SIM_NUM_OF_SOCKET && hsim->net.sockets[linkNum] != NULL) {
    socket = (SIM_Socket_t*) hsim->net.sockets[linkNum];
    while (dataLen) {
      if (dataLen > socket->buffer.size)  writeLen = socket->buffer.size;
      else                                writeLen = dataLen;

      if (STRM_ReadToBuffer(hsim->dmaStreamer, &socket->buffer, writeLen, 5000) != HAL_OK)
        break;

      dataLen -= writeLen;

      if (socket->listeners.onReceived != NULL)
        socket->listeners.onReceived(&(socket->buffer));
    }

    SIM_BITS_SET(socket->events, SIM_SOCK_EVENT_ON_RECEIVED);
  }
}


#endif /* SIM_EN_FEATURE_SOCKET */
