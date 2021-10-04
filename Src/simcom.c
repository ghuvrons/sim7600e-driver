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
uint8_t dbg_flag;
uint8_t tmpBuf[10];
uint8_t tmpBufIdx;

#define GETRESP_WAIT_OK 0
#define GETRESP_ONLY_DATA 1


// local macro

#define IS_RESP(hsim, resp, bufLen, min_len) \
 ((bufLen) >= (min_len) && strncmp((const char *)(hsim)->buffer, (resp), (min_len)) == 0)


// static function initiation

static void sendRequest(SIM_HandlerTypedef *hsim, const char *data, uint16_t size);
static void sendData(SIM_HandlerTypedef *hsim, const char *data, uint16_t size);
static uint8_t getResponse(SIM_HandlerTypedef *hsim,
                        const char *respCode, uint16_t rcsize,
                        uint8_t *respData, uint16_t rdsize,
                        uint8_t getRespType,
                        uint32_t timeout);
static uint8_t isOK(SIM_HandlerTypedef *hsim);
static const uint8_t * parseStr(const uint8_t *separator, uint8_t delimiter, int idx, uint8_t *output);
static void str2Time(SIM_Datetime*, const char*);

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
  const uint8_t *nextBuf = NULL;
  char linkNum_str[3];
  char dataLen_str[5];
  uint8_t linkNum;
  uint16_t dataLen;

  if(timeout == 0) timeout = hsim->timeout;

  while(bufLen == 0){
    bufLen = STRM_Readline(hsim->dmaStreamer, hsim->buffer, SIM_BUFFER_SIZE);
    if(bufLen){
      // check async response
      if(dbg_flag){
        printf("%d> ", dbg_flag);
        DBG_PrintS((char *)hsim->buffer, bufLen);
      }

      if(IS_RESP(hsim, "+RECEIVE", bufLen, 8)){
        printf("Receive some thing!\r\n");
        DBG_PrintS((char *)hsim->buffer, bufLen);
        memset(linkNum_str, 0, 3);
        memset(dataLen_str, 0, 5);
        nextBuf = parseStr(&hsim->buffer[9], ',', 0, (uint8_t*) linkNum_str);
        parseStr(nextBuf, ',', 0, (uint8_t*) dataLen_str);
        linkNum = (uint8_t) atoi(linkNum_str);
        dataLen = (uint16_t) atoi(dataLen_str);
        printf("recived %d %d\r\n", linkNum, dataLen);
        DBG_Log("RECEIVE", 7);
        DBG_Log(linkNum, 3);
        // while(1){
        //   STRM_ReadBuffer(hsim->dmaStreamer, hsim->net.sockets[x], size-len, STRM_BREAK_CRLF);
        // }
      }
      else if(IS_RESP(hsim, "+IPCLOSE", bufLen, 8)){

      }
      else if(IS_RESP(hsim, "+CIPEVENT", bufLen, 9)){
        if(strncmp((const char *)&(hsim->buffer[11]), "NETWORK CLOSED", 14)){
          SIM_UNSET_STATUS(hsim, SIM_STAT_NET_OPEN);
        }
      }
    }
    if((SIM_GetTick() - tickstart) >= timeout) break;
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

  sendRequest(hsim, "AT+CCLK?", 11);
  if(getResponse(hsim, "+CCLK", 5, resp, 22, GETRESP_WAIT_OK, 2000) == SIM_RESP_OK){
    str2Time(&result, (char*)&resp[1]);
  }
  SIM_UnlockCMD(hsim);

  return result;
}


void SIM_NetOpen(SIM_HandlerTypedef *hsim)
{
  uint8_t resp;
  uint8_t isNetOpened = 0;

  SIM_LockCMD(hsim);

  // check net state
  sendRequest(hsim, "AT+NETOPEN?", 11);
  if(getResponse(hsim, "+NETOPEN", 8, &resp, 1, GETRESP_WAIT_OK, 1000) == SIM_RESP_OK){
    if(resp == '1'){ // net already open;
      isNetOpened = 1;
    } else {
      sendRequest(hsim, "AT+NETOPEN", 10);
      if(getResponse(hsim, "+NETOPEN", 8, &resp, 1, GETRESP_WAIT_OK, 15000) == SIM_RESP_OK){
        if(resp == '0'){
          isNetOpened = 1;
        }
      }
    }
  }

  if(isNetOpened && !SIM_IS_STATUS(hsim, SIM_STAT_NET_OPEN)){
    SIM_SET_STATUS(hsim, SIM_STAT_NET_OPEN);

    // TCP/IP Config
    sendRequest(hsim, "AT+CIPCCFG=10,0,1,1,1,1,10000", 29);
    if (isOK(hsim)){
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
  uint8_t resp[6];
  uint8_t respErr;

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

  if(getResponse(hsim, "+CIPOPEN", 8, resp, 6, GETRESP_WAIT_OK, 15000) == SIM_RESP_OK){
    parseStr(resp, ',', 1, &respErr);
    if(respErr != '0') linkNum = -1;
    else DBG_Log((uint8_t*)"Socket opened", 13);
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
  sendData(hsim, cmd, strlen(cmd));
  sendRequest(hsim, (char*)data, length);
  if(getResponse(hsim, "+CIPSEND", 8, &resp, 1, GETRESP_WAIT_OK, 5000) == SIM_RESP_OK){
  }

  SIM_UnlockCMD(hsim);
}


static void sendRequest(SIM_HandlerTypedef *hsim, const char *data, uint16_t size)
{
  STRM_Write(hsim->dmaStreamer, (uint8_t *)data, size, STRM_BREAK_CRLF);
}

static void sendData(SIM_HandlerTypedef *hsim, const char *data, uint16_t size)
{
  STRM_Write(hsim->dmaStreamer, (uint8_t *)data, size, STRM_BREAK_NONE);
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
  if(SIM_IS_STATUS(hsim, SIM_STAT_UART_READING)) return 0;
  SIM_SET_STATUS(hsim, SIM_STAT_UART_READING);
  // wait until available
  while(1){
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
    // break if timeout
    if((SIM_GetTick() - tickstart) >= timeout) break;
  }

  SIM_UNSET_STATUS(hsim, SIM_STAT_UART_READING);
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
  printf("str2Time\r\n");
  for(uint8_t i = 0; i < 6; i++){
    DBG_PrintS(str, 2);
    *dtbytes = (uint8_t) atoi(str);
    dtbytes++;
    DBG_PrintB(dtbytes, 1);
    str += 3;
  }
}
