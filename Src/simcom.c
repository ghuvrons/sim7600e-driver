/*
 * modem.c
 *
 *  Created on: Sep 14, 2021
 *      Author: janoko
 */

#include "stm32f4xx_hal.h"
#include "simcom.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "dma_streamer.h"


// debugger
#include "debugger.h"
uint8_t tmpBuf[10];
uint8_t tmpBufIdx;

#define GETRESP_WAIT_OK 0
#define GETRESP_ONLY_DATA 1


// local macro

#define IS_RESP(hsim, resp, bufLen, min_len) \
 ((bufLen) >= (min_len) && strncmp((const char *)(hsim)->buffer, (resp), (min_len)) == 0)


// static function initiation

static void sendRequest(SIM_HandlerTypedef *hsim, const char *data, uint16_t size);
static void sendData(SIM_HandlerTypedef *hsim, const uint8_t *data, uint16_t size);
static uint16_t getData(SIM_HandlerTypedef *hsim,
                        uint8_t *respData, uint16_t rdsize,
                        uint32_t timeout);
static uint8_t waitResponse(SIM_HandlerTypedef *hsim,
                            const char *respCode, uint16_t rcsize,
                            uint32_t timeout);
static uint8_t getResponse(SIM_HandlerTypedef *hsim,
                        const char *respCode, uint16_t rcsize,
                        uint8_t *respData, uint16_t rdsize,
                        uint8_t getRespType,
                        uint32_t timeout);
static uint8_t isOK(SIM_HandlerTypedef *hsim);
static const uint8_t * parseStr(const uint8_t *separator, uint8_t delimiter, int idx, uint8_t *output);
static void str2Time(SIM_Datetime*, const char*);
// listener
static void onNetOpen(SIM_HandlerTypedef*);
static void onSockReceive(SIM_HandlerTypedef*);
static void onSockClose(SIM_HandlerTypedef*);

// function definition

__weak uint32_t SIM_GetTick(void)
{
  return HAL_GetTick();
}

__weak void SIM_Delay(uint32_t ms)
{
  HAL_Delay(ms);
}

__weak void SIM_LockCMD(SIM_HandlerTypedef *hsim)
{
  while(SIM_IS_STATUS(hsim, SIM_STAT_CMD_RUNNING)){
    SIM_Delay(1);
  }
  SIM_SET_STATUS(hsim, SIM_STAT_CMD_RUNNING);
}

__weak void SIM_UnlockCMD(SIM_HandlerTypedef *hsim)
{
  SIM_UNSET_STATUS(hsim, SIM_STAT_CMD_RUNNING);
}


void SIM_Init(SIM_HandlerTypedef *hsim, STRM_handlerTypedef *dmaStreamer)
{
  hsim->dmaStreamer = dmaStreamer;
  hsim->timeout = 2000;
  return;
}

void SIM_checkAsyncResponse(SIM_HandlerTypedef *hsim, uint32_t timeout)
{
  SIM_LockCMD(hsim);
  SIM_checkResponse(hsim, timeout);
  SIM_UnlockCMD(hsim);
}


uint16_t SIM_checkResponse(SIM_HandlerTypedef *hsim, uint32_t timeout)
{
  uint16_t bufLen = 0;
  uint32_t tickstart = SIM_GetTick();

  if(timeout == 0) timeout = hsim->timeout;

  while(1){
    if((SIM_GetTick() - tickstart) >= timeout) break;
    bufLen = STRM_Readline(hsim->dmaStreamer, hsim->buffer, SIM_BUFFER_SIZE, timeout);
    if(bufLen){
      // check async response
      if(IS_RESP(hsim, "START", bufLen, 3)){
        SIM_RESET(hsim);
      }
      else if(!SIM_IS_STATUS(hsim, SIM_STAT_START) && IS_RESP(hsim, "PB ", bufLen, 3)){
        SIM_SET_STATUS(hsim, SIM_STAT_START);
      }
      else if(IS_RESP(hsim, "+RECEIVE", bufLen, 8)){
        onSockReceive(hsim);
      }
      else if(bufLen == 11 && IS_RESP(hsim, "+NETOPEN", bufLen, 8)){
        onNetOpen(hsim);
      }
      else if(IS_RESP(hsim, "+IPCLOSE", bufLen, 8)){
        onSockClose(hsim);
      }
      else if(IS_RESP(hsim, "+CIPEVENT", bufLen, 9)){
        if(strncmp((const char *)&(hsim->buffer[11]), "NETWORK CLOSED", 14)){
          SIM_UNSET_STATUS(hsim, SIM_STAT_NET_OPEN);
        }
      }
      else break;
    }
  }
  return bufLen;
}


void SIM_CheckAT(SIM_HandlerTypedef *hsim)
{
  SIM_LockCMD(hsim);

  // send command;
  sendRequest(hsim, "AT", 2);

  // wait response
  if (isOK(hsim)){
    SIM_SET_STATUS(hsim, SIM_STAT_CONNECT);
  } else {
    SIM_UNSET_STATUS(hsim, SIM_STAT_CONNECT);
  }
  SIM_UnlockCMD(hsim);
}


SIM_Datetime SIM_GetTime(SIM_HandlerTypedef *hsim)
{
  SIM_Datetime result;
  uint8_t resp[22];

  SIM_LockCMD(hsim);

  sendRequest(hsim, "AT+CCLK?", 8);
  if(getResponse(hsim, "+CCLK", 5, resp, 22, GETRESP_WAIT_OK, 2000) == SIM_RESP_OK){
    str2Time(&result, (char*)&resp[1]);
  }
  SIM_UnlockCMD(hsim);

  return result;
}


void SIM_HashTime(SIM_HandlerTypedef *hsim, char *hashed)
{
  SIM_Datetime dt;
  uint8_t *dtBytes = (uint8_t *) &dt;
  dt = SIM_GetTime(hsim);
  for(uint8_t i = 0; i < 6; i++){
    *hashed = (*dtBytes) + 0x41 + i;
    if(*hashed > 0x7A){
      *hashed = 0x7A - i;
    }
    if(*hashed < 0x30){
      *hashed = 0x30 + i;
    }
    dtBytes++;
    hashed++;
  }
}


void SIM_NetOpen(SIM_HandlerTypedef *hsim)
{
  uint8_t resp;
  uint8_t isNetOpened = 0;

  if(SIM_IS_STATUS(hsim, SIM_STAT_NET_OPENING)) return;

  SIM_LockCMD(hsim);

  // check net state
  sendRequest(hsim, "AT+NETOPEN?", 11);
  if(getResponse(hsim, "+NETOPEN", 8, &resp, 1, GETRESP_WAIT_OK, 1000) == SIM_RESP_OK){
    if(resp == '1'){ // net already open;
      SIM_SET_STATUS(hsim, SIM_STAT_NET_OPEN);

      // TCP/IP Config
      sendRequest(hsim, "AT+CIPCCFG=10,0,1,1,1,1,10000", 29);
      if (isOK(hsim)){
      }
    } else {
      sendRequest(hsim, "AT+NETOPEN", 10);
      SIM_Delay(1000);
      if (isOK(hsim)){
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
int8_t SIM_SockOpenTCPIP(SIM_HandlerTypedef *hsim, const char *host, uint16_t port)
{
  int8_t linkNum = -1;
  char cmd[128];
  uint8_t resp[4];

  if(!SIM_IS_STATUS(hsim, SIM_STAT_NET_OPEN))
  {
    return -1;
  }
  
  for(int16_t i = 0; i < SIM_MAX_SOCKET; i++){
    if(hsim->net.sockets[i] == NULL){
      linkNum = i;
      break;
    }
  }
  if(linkNum == -1) return linkNum;

  SIM_LockCMD(hsim);

  sprintf(cmd, "AT+CIPOPEN=%d,\"TCP\",\"%s\",%d", linkNum, host, port);
  sendRequest(hsim, cmd, strlen(cmd));

  memset(resp, 0, 4);
  if(getResponse(hsim, "+CIPOPEN", 8, resp, 3, GETRESP_WAIT_OK, 15000) == SIM_RESP_OK){
    DBG_Log(resp, 4);
    if(resp[3] != '0') linkNum = -1;
  }

  SIM_UnlockCMD(hsim);
  return linkNum;
}


void SIM_SockSendData(SIM_HandlerTypedef *hsim, int8_t linkNum, const uint8_t *data, uint16_t length)
{
  char cmd[20];
  uint8_t resp;

  SIM_LockCMD(hsim);

  sprintf(cmd, "AT+CIPSEND=%d,%d\r", linkNum, length);
  sendData(hsim, (uint8_t*)cmd, strlen(cmd));
  waitResponse(hsim, "\r\n>", 3, 3000);

  sendData(hsim, data, length);
  SIM_Delay(5000);
  if(getResponse(hsim, "+CIPSEND", 8, &resp, 1, GETRESP_WAIT_OK, 5000) == SIM_RESP_OK){
  }

  SIM_UnlockCMD(hsim);
}


void SIM_SockAddListener(SIM_HandlerTypedef *hsim, uint8_t linkNum, SIM_SockListener *listener)
{
  hsim->net.sockets[linkNum] = listener;
}


void SIM_SockRemoveListener(SIM_HandlerTypedef *hsim, uint8_t linkNum)
{
  hsim->net.sockets[linkNum] = NULL;
}



static void onNetOpen(SIM_HandlerTypedef *hsim)
{
  if(!SIM_IS_STATUS(hsim, SIM_STAT_NET_OPENING)) return;

  // skip string "+NETOPEN: " and read next data
  if(hsim->buffer[10] == '0'){
    SIM_UNSET_STATUS(hsim, SIM_STAT_NET_OPENING);
    SIM_SET_STATUS(hsim, SIM_STAT_NET_OPEN);

    // TCP/IP Config
    sendRequest(hsim, "AT+CIPCCFG=10,0,1,1,1,1,10000", 29);
    if (isOK(hsim)){
    }
  }
  else {
    SIM_UNSET_STATUS(hsim, SIM_STAT_NET_OPENING);
  }
}


static void onSockReceive(SIM_HandlerTypedef *hsim)
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
  nextBuf = parseStr(&hsim->buffer[9], ',', 0, (uint8_t*) linkNum_str);
  parseStr(nextBuf, ',', 0, (uint8_t*) dataLen_str);
  linkNum = (uint8_t) atoi(linkNum_str);
  dataLen = (uint16_t) atoi(dataLen_str);

  if(linkNum < SIM_MAX_SOCKET && hsim->net.sockets[linkNum] != NULL)
  {
    sockListener = hsim->net.sockets[linkNum];
    if(dataLen > sockListener->bufferSize) dataLen = sockListener->bufferSize;
    dataLen = getData(hsim, sockListener->buffer, dataLen, 1000);
    if(sockListener->onReceive != NULL)
      sockListener->onReceive(dataLen);
  }
}


static void onSockClose(SIM_HandlerTypedef *hsim)
{
  uint8_t linkNum;
  char linkNum_str[2];
  SIM_SockListener *sockListener;

  memset(linkNum_str, 0, 2);
  // skip string "+IPCLOSE" and read next data
  parseStr(&hsim->buffer[10], ',', 0, (uint8_t*) linkNum_str);
  linkNum = (uint8_t) atoi(linkNum_str);

  if(linkNum < SIM_MAX_SOCKET && hsim->net.sockets[linkNum] != NULL)
  {
    sockListener = hsim->net.sockets[linkNum];
    if(sockListener->onClose != NULL)
      sockListener->onClose();
    hsim->net.sockets[linkNum] = NULL;
  }
}


static void sendRequest(SIM_HandlerTypedef *hsim, const char *data, uint16_t size)
{
  STRM_Write(hsim->dmaStreamer, (uint8_t*)data, size, STRM_BREAK_CRLF);
}


static void sendData(SIM_HandlerTypedef *hsim, const uint8_t *data, uint16_t size)
{
  STRM_Write(hsim->dmaStreamer, (uint8_t*)data, size, STRM_BREAK_NONE);
}


static uint16_t getData(SIM_HandlerTypedef *hsim,
                        uint8_t *respData, uint16_t rdsize,
                        uint32_t timeout)
{
  uint16_t len = 0;
  uint32_t tickstart = SIM_GetTick();

  if(timeout == 0) timeout = hsim->timeout;
  while(len < rdsize){
    if((STRM_GetTick() - tickstart) >= timeout) break;
    len += STRM_ReadBuffer(hsim->dmaStreamer, respData, rdsize, STRM_BREAK_NONE);
    if(len == 0) STRM_Delay(1);
  }
  return len;
}


static uint8_t waitResponse(SIM_HandlerTypedef *hsim,
                            const char *respCode, uint16_t rcsize,
                            uint32_t timeout)
{
  uint16_t len = 0;

  if(timeout == 0) timeout = hsim->timeout;

  STRM_Read(hsim->dmaStreamer, hsim->buffer, rcsize, timeout);
  if(len){
    if(IS_RESP(hsim, respCode, len, rcsize)){
      return 1;
    }
  }
  return 0;
}


static uint8_t getResponse(SIM_HandlerTypedef *hsim,
                        const char *respCode, uint16_t rcsize,
                        uint8_t *respData, uint16_t rdsize,
                        uint8_t getRespType,
                        uint32_t timeout)
{
  uint8_t resp = SIM_RESP_TIMEOUT;
  uint16_t bufLen = 0;
  uint8_t flagToReadResp = 0;
  uint32_t tickstart = SIM_GetTick();

  if(timeout == 0) timeout = hsim->timeout;

  // wait until available
  while(1){
    if((SIM_GetTick() - tickstart) >= timeout) break;
    bufLen = SIM_checkResponse(hsim, timeout);
    if(bufLen){
      if(rcsize && strncmp((char *)hsim->buffer, respCode, rcsize) == 0){
        if(flagToReadResp) continue;
        // read response data
        for(int i = 2; i < bufLen && rdsize; i++){
          // split string
          if(!flagToReadResp && hsim->buffer[i-2] == ':' && hsim->buffer[i-1] == ' '){
            flagToReadResp = 1;
          }
          if(flagToReadResp){
            *respData = hsim->buffer[i];
            respData++;
            rdsize--;
          }
        }
        if(getRespType == GETRESP_ONLY_DATA) break;
        if(resp) break;
      }
      else if(IS_RESP(hsim, "OK", bufLen, 2)){
        resp = SIM_RESP_OK;
      }
      else if(IS_RESP(hsim, "ERROR", bufLen, 5)){
        resp = SIM_RESP_ERROR;
      }

      // break if will not get data
      if(resp && !rcsize) break;
      else if (resp && flagToReadResp) break;
    }
  }

  return resp;
}



static uint8_t isOK(SIM_HandlerTypedef *hsim)
{
  return (getResponse(hsim, NULL, 0, NULL, 0, GETRESP_WAIT_OK, 0) == SIM_RESP_OK);
}


static const uint8_t * parseStr(const uint8_t *separator, uint8_t delimiter, int idx, uint8_t *output)
{
  uint8_t isInStr = 0;

  while (1)
  {
    if(*separator == 0 || *separator == '\r') break;
    if(!isInStr && *separator == delimiter){
      idx--;
      if(idx < 0){
        separator++;
        break;
      }
    }
    else if(*separator == '\"'){
      if(isInStr) isInStr = 0;
      else        isInStr = 1;
    }
    else if (idx == 0){
      *output = *separator;
      output++;
    }
    separator++;
  }
  
  return separator;
}


static void str2Time(SIM_Datetime *dt, const char *str)
{
  uint8_t *dtbytes = (uint8_t*) dt;
  for(uint8_t i = 0; i < 6; i++){
    *dtbytes = (uint8_t) atoi(str);
    dtbytes++;
    str += 3;
  }
}
