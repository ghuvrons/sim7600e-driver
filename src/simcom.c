/*
 * modem.c
 *
 *  Created on: Sep 14, 2021
 *      Author: janoko
 */


#include "include/simcom.h"
#include "include/simcom/conf.h"
#include "include/simcom/utils.h"
#include "include/simcom/debug.h"
#include "include/simcom/net.h"
#include "include/simcom/socket.h"
#include "include/simcom/gps.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>


uint8_t SIM_CmdTmp[64];
uint8_t SIM_RespTmp[64];

// static function initiation
static void mutexLock(SIM_HandlerTypeDef*);
static void mutexUnlock(SIM_HandlerTypeDef*);
static void SIM_reset(SIM_HandlerTypeDef*);
static void str2Time(SIM_Datetime*, const char*);


// function definition

SIM_Status_t SIM_Init(SIM_HandlerTypeDef *hsim)
{
  SIM_Debug("Init");
  hsim->status = 0;
  hsim->events = 0;
  hsim->errors = 0;
  hsim->signal = 0;
  if (hsim->timeout == 0)
    hsim->timeout = 5000;

  if (hsim->delay == 0) return SIM_ERROR;
  if (hsim->getTick == 0) return SIM_ERROR;
  if (hsim->serial.device == 0) return SIM_ERROR;
  if (hsim->serial.read == 0) return SIM_ERROR;
  if (hsim->serial.readline == 0) return SIM_ERROR;
  if (hsim->serial.readinto == 0) return SIM_ERROR;
  if (hsim->serial.unread == 0) return SIM_ERROR;
  if (hsim->serial.write == 0) return SIM_ERROR;
  if (hsim->serial.writeline == 0) return SIM_ERROR;

  if (hsim->mutexLock == 0) 
    hsim->mutexLock = mutexLock;

  if (hsim->mutexUnlock == 0)
    hsim->mutexUnlock = mutexUnlock;

  hsim->initAt = hsim->getTick();
}


/*
 * Read response per lines at a certain time interval
 */
void SIM_CheckAnyResponse(SIM_HandlerTypeDef *hsim)
{
  int readStatus;

  // Read incoming Response
  hsim->mutexLock(hsim);
  while (hsim->serial.isReadable(hsim->serial.device)) {
    readStatus = hsim->serial.readline(hsim->serial.device, hsim->respBuffer, SIM_RESP_BUFFER_SIZE, 5000);
    if (readStatus > 0) {
      hsim->respBufferLen = readStatus;
      SIM_CheckAsyncResponse(hsim);
    }
  }
  hsim->mutexUnlock(hsim);

  // Event Handler
  SIM_HandleEvents(hsim);
}


void SIM_CheckAsyncResponse(SIM_HandlerTypeDef *hsim)
{
  hsim->respBuffer[hsim->respBufferLen] = 0;

  if (SIM_IsResponse(hsim, "\r\n", 2)) {
    return;
  }

  if (SIM_IsResponse(hsim, "RDY", 3)) {
    SIM_BITS_SET(hsim->events, SIM_EVENT_ON_STARTING);
  }

  else if (!SIM_IS_STATUS(hsim, SIM_STATUS_START) && SIM_IsResponse(hsim, "PB ", 3)) {
    SIM_SET_STATUS(hsim, SIM_STATUS_START);
    SIM_BITS_SET(hsim->events, SIM_EVENT_ON_STARTED);
  }

  #if SIM_EN_FEATURE_NET
  else if (SIM_NetCheckAsyncResponse(hsim)) return;
  #endif

  #if SIM_EN_FEATURE_SOCKET
  else if (SIM_SockCheckAsyncResponse(hsim)) return;
  #endif

  #if SIM_EN_FEATURE_GPS
  else if (SIM_GPS_CheckAsyncResponse(hsim)) return;
  #endif
}


/*
 * Handle async response
 */
void SIM_HandleEvents(SIM_HandlerTypeDef *hsim)
{
  // check async response
  if (hsim->status == 0 && (hsim->getTick() - hsim->initAt > hsim->timeout)) {
    if (!SIM_CheckAT(hsim)) {
      hsim->initAt = hsim->getTick();
    } else {
      SIM_Echo(hsim, 0);
    }
  }

  if (SIM_BITS_IS(hsim->events, SIM_EVENT_ON_STARTING)) {
    SIM_BITS_UNSET(hsim->events, SIM_EVENT_ON_STARTING);
    SIM_Debug("Starting...");
    SIM_reset(hsim);
  }
  if (SIM_BITS_IS(hsim->events, SIM_EVENT_ON_STARTED)) {
    SIM_BITS_UNSET(hsim->events, SIM_EVENT_ON_STARTED);
    SIM_Debug("Started.");
    SIM_SockOnStarted(hsim);
  }
  if (SIM_BITS_IS(hsim->events, SIM_EVENT_ON_REGISTERED)) {
    SIM_BITS_UNSET(hsim->events, SIM_EVENT_ON_REGISTERED);
    SIM_Debug("Network Registered%s.", (SIM_IS_STATUS(hsim, SIM_STATUS_ROAMING))? " (Roaming)": "");
  }
  
  if (SIM_IS_STATUS(hsim, SIM_STATUS_START) && !SIM_IS_STATUS(hsim, SIM_STATUS_ACTIVE)) {
    SIM_Debug("Activating...");
    SIM_Echo(hsim, 0);
    if (SIM_CheckAT(hsim)) {
      SIM_Debug("Activated.");
    }
  }
  if (SIM_IS_STATUS(hsim, SIM_STATUS_ACTIVE) && !SIM_IS_STATUS(hsim, SIM_STATUS_SIM_INSERTED)) {
    if(!SIM_CheckSIMCard(hsim)) {
      hsim->delay(3000);
    }
  }
  if (SIM_IS_STATUS(hsim, SIM_STATUS_SIM_INSERTED) && !SIM_IS_STATUS(hsim, SIM_STATUS_REGISTERED)) {
    if(!SIM_ReqisterNetwork(hsim)) {
      hsim->delay(3000);
    }
  }

#ifdef SIM_EN_FEATURE_NET
  SIM_NetHandleEvents(hsim);
#endif

#ifdef SIM_EN_FEATURE_SOCKET
  SIM_SockHandleEvents(hsim);
#endif

#if SIM_EN_FEATURE_GPS
  SIM_GPS_HandleEvents(hsim);
#endif

}


void SIM_Echo(SIM_HandlerTypeDef *hsim, uint8_t onoff)
{
  hsim->mutexLock(hsim);
  if (onoff)
    SIM_SendCMD(hsim, "ATE1");
  else
    SIM_SendCMD(hsim, "ATE0");
  // wait response
  if (SIM_IsResponseOK(hsim)) {}
  hsim->mutexUnlock(hsim);
}


uint8_t SIM_CheckAT(SIM_HandlerTypeDef *hsim)
{
  uint8_t isOK = 0;
  // send command;
  hsim->mutexLock(hsim);
  SIM_SendCMD(hsim, "AT");

  // wait response
  if (SIM_IsResponseOK(hsim)){
    isOK = 1;
    SIM_SET_STATUS(hsim, SIM_STATUS_START);
    SIM_SET_STATUS(hsim, SIM_STATUS_ACTIVE);
  } else {
    SIM_UNSET_STATUS(hsim, SIM_STATUS_ACTIVE);
  }
  hsim->mutexUnlock(hsim);

  return isOK;
}


uint8_t SIM_CheckSignal(SIM_HandlerTypeDef *hsim)
{
  uint8_t signal = 0;
  uint8_t *resp = &SIM_RespTmp[0];
  char *signalStr = (char*) &SIM_RespTmp[32];

  if (!SIM_IS_STATUS(hsim, SIM_STATUS_ACTIVE)) return signal;

  // send command then get response;
  hsim->mutexLock(hsim);
  SIM_SendCMD(hsim, "AT+CSQ");

  memset(resp, 0, 16);

  // do with response
  if (SIM_GetResponse(hsim, "+CSQ", 4, resp, 16, SIM_GETRESP_WAIT_OK, 2000) == SIM_OK) {
    SIM_ParseStr(resp, ',', 1, (uint8_t*) signalStr);
    signal = (uint8_t) atoi((char*)resp);
    hsim->signal = signal;
  }
  hsim->mutexUnlock(hsim);

  if (signal == 99) {
    signal = 0;
    hsim->signal = signal;
    SIM_ReqisterNetwork(hsim);
  }

  return hsim->signal;
}


uint8_t SIM_CheckSIMCard(SIM_HandlerTypeDef *hsim)
{
  uint8_t *resp = &SIM_RespTmp[0];
  uint8_t isOK = 0;

  hsim->mutexLock(hsim);

  memset(resp, 0, 11);
  SIM_SendCMD(hsim, "AT+CPIN?");
  if (SIM_GetResponse(hsim, "+CPIN", 5, resp, 10, SIM_GETRESP_WAIT_OK, 2000) == SIM_OK) {
    // resp_n = (uint8_t) atoi((char*)&resp[0]);
    if (strcmp((char*) resp, "READY")) {
      SIM_Debug("SIM Ready.");
      isOK = 1;
      SIM_SET_STATUS(hsim, SIM_STATUS_SIM_INSERTED);
    }
  } else {
    SIM_Debug("SIM card error.");
  }
  hsim->mutexUnlock(hsim);
  return isOK;
}


uint8_t SIM_ReqisterNetwork(SIM_HandlerTypeDef *hsim)
{
  uint8_t *resp = &SIM_RespTmp[0];
  // uint8_t resp_n = 0;
  uint8_t resp_stat = 0;
  uint8_t resp_mode = 0;
  uint8_t isOK = 0;

  // send command then get response;
  hsim->mutexLock(hsim);

  memset(resp, 0, 4);
  SIM_SendCMD(hsim, "AT+CREG?");
  if (SIM_GetResponse(hsim, "+CREG", 5, resp, 3, SIM_GETRESP_WAIT_OK, 2000) == SIM_OK) {
    // resp_n = (uint8_t) atoi((char*)&resp[0]);
    resp_stat = (uint8_t) atoi((char*) resp+2);
  }
  else goto endcmd;

  // check response
  if (resp_stat == 1 || resp_stat == 5) {
    SIM_SET_STATUS(hsim, SIM_STATUS_REGISTERED);
    SIM_BITS_SET(hsim->events, SIM_EVENT_ON_REGISTERED);
    isOK = 1;
    if (resp_stat == 5) {
      SIM_SET_STATUS(hsim, SIM_STATUS_ROAMING);
    }
  }
  else {
    SIM_UNSET_STATUS(hsim, SIM_STATUS_REGISTERED);

    if (resp_stat == 0) {
      SIM_Debug("Registering network....");

      // Select operator automatically
      memset(resp, 0, 16);
      SIM_SendCMD(hsim, "AT+COPS?");
      if (SIM_GetResponse(hsim, "+COPS", 5, resp, 1, SIM_GETRESP_WAIT_OK, 2000) == SIM_OK) {
        resp_mode = (uint8_t) atoi((char*) resp);
      }
      else goto endcmd;

      SIM_SendCMD(hsim, "AT+COPS=?");
      if (!SIM_IsResponseOK(hsim)) goto endcmd;

      if (resp_mode != 0) {
        SIM_SendCMD(hsim, "AT+COPS=0");
        if (!SIM_IsResponseOK(hsim)) goto endcmd;

        SIM_SendCMD(hsim, "AT+COPS");
        if (!SIM_IsResponseOK(hsim)) goto endcmd;
      }
    }
    else if (resp_stat == 2) {
      SIM_Debug("Searching network....");
      hsim->delay(2000);
    }
  }

  endcmd:
  hsim->mutexUnlock(hsim);
  return isOK;
}


SIM_Datetime SIM_GetTime(SIM_HandlerTypeDef *hsim)
{
  SIM_Datetime result = {0};
  uint8_t *resp = &SIM_RespTmp[0];

  // send command then get response;
  hsim->mutexLock(hsim);

  memset(resp, 0, 22);
  SIM_SendCMD(hsim, "AT+CCLK?");
  if (SIM_GetResponse(hsim, "+CCLK", 5, resp, 22, SIM_GETRESP_WAIT_OK, 2000) == SIM_OK) {
    str2Time(&result, (char*)&resp[0]);
  }
  hsim->mutexUnlock(hsim);

  return result;
}


void SIM_HashTime(SIM_HandlerTypeDef *hsim, char *hashed)
{
  SIM_Datetime dt;
  uint8_t *dtBytes = (uint8_t *) &dt;
  dt = SIM_GetTime(hsim);
  for (uint8_t i = 0; i < 6; i++) {
    *hashed = (*dtBytes) + 0x41 + i;
    if (*hashed > 0x7A) {
      *hashed = 0x7A - i;
    }
    if (*hashed < 0x30) {
      *hashed = 0x30 + i;
    }
    dtBytes++;
    hashed++;
  }
}


void SIM_SendUSSD(SIM_HandlerTypeDef *hsim, const char *ussd)
{
  if (!SIM_IS_STATUS(hsim, SIM_STATUS_REGISTERED)) return;

  hsim->mutexLock(hsim);
  SIM_SendCMD(hsim, "AT+CSCS=\"GSM\"");
  if (!SIM_IsResponseOK(hsim)){
    goto endcmd;
  }

  SIM_SendCMD(hsim, "AT+CUSD=1,%s,15", ussd);

  endcmd:
  hsim->mutexUnlock(hsim);
}


static void mutexLock(SIM_HandlerTypeDef *hsim)
{
  while (SIM_IS_STATUS(hsim, SIM_STATUS_CMD_RUNNING)) {
    hsim->delay(1);
  }
  SIM_SET_STATUS(hsim, SIM_STATUS_CMD_RUNNING);
}


static void mutexUnlock(SIM_HandlerTypeDef *hsim)
{
  SIM_UNSET_STATUS(hsim, SIM_STATUS_CMD_RUNNING);
}


static void SIM_reset(SIM_HandlerTypeDef *hsim)
{
  hsim->signal = 0;
  hsim->status = 0;
  hsim->errors = 0;
}

static void str2Time(SIM_Datetime *dt, const char *str)
{
  uint8_t *dtbytes;
  int8_t mult = 1;

  str++;
  dt->year = (uint8_t) atoi(str);
  dtbytes = ((uint8_t*) dt) + 1;
  while (*str && *str != '\"') {
    if (*str < '0' || *str > '9') {
      if (*str == '-') {
        mult = -1;
      } else {
        mult = 1;
      }
      str++;
      *dtbytes = ((int8_t) atoi(str)) * mult;
      dtbytes++;
    }
    str++;
  }
}
