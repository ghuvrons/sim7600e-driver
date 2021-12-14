/*
 * modem.c
 *
 *  Created on: Sep 14, 2021
 *      Author: janoko
 */

#include "stm32f4xx_hal.h"
#include "Include/simcom.h"
#include "Include/simcom/conf.h"
#include "Include/simcom/utils.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <dma_streamer.h>


// static function initiation

static uint16_t getData(SIM_HandlerTypedef *hsim,
                        uint8_t *respData, uint16_t rdsize,
                        uint32_t timeout);
static const uint8_t * parseStr(const uint8_t *separator, uint8_t delimiter, int idx, uint8_t *output);
static void str2Time(SIM_Datetime*, const char*);
// listener
static void onNetOpen(SIM_HandlerTypedef*);
static void onSockReceive(SIM_HandlerTypedef*);
static void onSockClose(SIM_HandlerTypedef*);

// function definition

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


/*
 * Read response per lines at a certain time interval
 */
void SIM_checkAsyncResponse(SIM_HandlerTypedef *hsim, uint32_t timeout)
{
  SIM_LockCMD(hsim);
  SIM_checkResponse(hsim, timeout);
  SIM_UnlockCMD(hsim);
}


/*
 * Read response per lines
 */
uint16_t SIM_checkResponse(SIM_HandlerTypedef *hsim, uint32_t timeout)
{
  uint16_t bufLen = 0;
  uint32_t tickstart = SIM_GetTick();

  if(timeout == 0) timeout = hsim->timeout;

  while (1) {
    if((SIM_GetTick() - tickstart) >= timeout) break;
    bufLen = STRM_Readline(hsim->dmaStreamer, hsim->buffer, SIM_BUFFER_SIZE, timeout);
    if(bufLen){
      // check async response
      if (SIM_IsResponse(hsim, "START", bufLen, 3)) {
        SIM_RESET(hsim);
      }

      else if (!SIM_IS_STATUS(hsim, SIM_STAT_START) && SIM_IsResponse(hsim, "PB ", bufLen, 3)) {
        SIM_SET_STATUS(hsim, SIM_STAT_START);
      }

      else if (SIM_IsResponse(hsim, "+RECEIVE", bufLen, 8)) {
        onSockReceive(hsim);
      }

      else if (bufLen == 11 && SIM_IsResponse(hsim, "+NETOPEN", bufLen, 8)) {
        onNetOpen(hsim);
      }

      else if (SIM_IsResponse(hsim, "+IPCLOSE", bufLen, 8)) {
        onSockClose(hsim);
      }

      else if (SIM_IsResponse(hsim, "+CIPEVENT", bufLen, 9)) {
        if (strncmp((const char *)&(hsim->buffer[11]), "NETWORK CLOSED", 14)) {
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
  SIM_SendCMD(hsim, "AT", 2);

  // wait response
  if (SIM_IsResponseOK(hsim)){
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

  SIM_SendCMD(hsim, "AT+CCLK?", 8);
  if(SIM_GetResponse(hsim, "+CCLK", 5, resp, 22, SIM_GETRESP_WAIT_OK, 2000) == SIM_RESP_OK){
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


static void onNetOpen(SIM_HandlerTypedef *hsim)
{
  if(!SIM_IS_STATUS(hsim, SIM_STAT_NET_OPENING)) return;

  // skip string "+NETOPEN: " and read next data
  if(hsim->buffer[10] == '0'){
    SIM_UNSET_STATUS(hsim, SIM_STAT_NET_OPENING);
    SIM_SET_STATUS(hsim, SIM_STAT_NET_OPEN);

    // TCP/IP Config
    SIM_SendCMD(hsim, "AT+CIPCCFG=10,0,1,1,1,1,10000", 29);
    if (SIM_IsResponseOK(hsim)){
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


static uint16_t getData(SIM_HandlerTypedef *hsim,
                        uint8_t *respData, uint16_t rdsize,
                        uint32_t timeout)
{
  uint16_t len = 0;
  uint32_t tickstart = SIM_GetTick();

  if(timeout == 0) timeout = hsim->timeout;
  while(len < rdsize){
    if((SIM_GetTick() - tickstart) >= timeout) break;
    len += STRM_ReadBuffer(hsim->dmaStreamer, respData, rdsize, STRM_BREAK_NONE);
    if(len == 0) SIM_Delay(1);
  }
  return len;
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
