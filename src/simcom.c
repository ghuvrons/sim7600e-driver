/*
 * modem.c
 *
 *  Created on: Sep 14, 2021
 *      Author: janoko
 */

#include "stm32f4xx_hal.h"
#include "include/simcom.h"
#include "include/simcom/conf.h"
#include "include/simcom/utils.h"
#include "include/simcom/socket.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <dma_streamer.h>


// static function initiation
static void SIM_reset(SIM_HandlerTypeDef*);
static void str2Time(SIM_Datetime*, const char*);


// function definition

__weak void SIM_LockCMD(SIM_HandlerTypeDef *hsim)
{
  while(SIM_IS_STATE(hsim, SIM_STATE_CMD_RUNNING)){
    SIM_Delay(1);
  }
  SIM_SET_STATE(hsim, SIM_STATE_CMD_RUNNING);
}

__weak void SIM_UnlockCMD(SIM_HandlerTypeDef *hsim)
{
  SIM_UNSET_STATE(hsim, SIM_STATE_CMD_RUNNING);
}


void SIM_Init(SIM_HandlerTypeDef *hsim, STRM_handlerTypeDef *dmaStreamer)
{
  hsim->dmaStreamer = dmaStreamer;
  hsim->timeout = 2000;
  return;
}


/*
 * Read response per lines at a certain time interval
 */
void SIM_CheckAsyncResponse(SIM_HandlerTypeDef *hsim, uint32_t timeout)
{
  uint32_t tickstart = SIM_GetTick();

  while (STRM_IsReadable(hsim->dmaStreamer)) {
    if((SIM_GetTick() - tickstart) >= timeout) break;

    hsim->respBufferLen = STRM_Readline(hsim->dmaStreamer, hsim->respBuffer, SIM_RESP_BUFFER_SIZE, timeout);
    if (hsim->respBufferLen) {
      SIM_HandleAsyncResponse(hsim);
    }
  }

  if (SIM_IS_STATE(hsim, SIM_STATE_START) && !SIM_IS_STATE(hsim, SIM_STATE_ACTIVE)){
    SIM_CheckAT(hsim);
  }
  if (SIM_IS_STATE(hsim, SIM_STATE_ACTIVE) && !SIM_IS_STATE(hsim, SIM_STATE_REGISTERED)){
    SIM_ReqisterNetwork(hsim);
  }
}


/*
 * Handle async response
 */
void SIM_HandleAsyncResponse(SIM_HandlerTypeDef *hsim)
{
  // check async response
  if (SIM_IsResponse(hsim, "RDY", 3)) {
    SIM_reset(hsim);
  }

  else if (!SIM_IS_STATE(hsim, SIM_STATE_START) && SIM_IsResponse(hsim, "PB ", 3)) {
    SIM_SET_STATE(hsim, SIM_STATE_START);
  }

#ifdef SIM_EN_FEATURE_SOCKET
  else if (SIM_NetCheckAsyncResponse(hsim)) return;
#endif

}


void SIM_CheckAT(SIM_HandlerTypeDef *hsim)
{
  // send command;
  SIM_LockCMD(hsim);
  SIM_SendCMD(hsim, "AT");

  // wait response
  if (SIM_IsResponseOK(hsim)){
    SIM_SET_STATE(hsim, SIM_STATE_ACTIVE);
  } else {
    SIM_UNSET_STATE(hsim, SIM_STATE_ACTIVE);
  }
  SIM_UnlockCMD(hsim);
}


uint8_t SIM_CheckSignal(SIM_HandlerTypeDef *hsim)
{
  uint8_t signal = 0;
  uint8_t resp[16];
  char signalStr[3];

  if (!SIM_IS_STATE(hsim, SIM_STATE_ACTIVE)) return signal;

  // send command then get response;
  SIM_LockCMD(hsim);
  SIM_SendCMD(hsim, "AT+CSQ");

  // do with response
  if (SIM_GetResponse(hsim, "+CSQ", 4, &resp[0], 16, SIM_GETRESP_WAIT_OK, 2000) == SIM_OK) {
    SIM_ParseStr(&resp[0], ',', 1, (uint8_t*) &signalStr[0]);
    signal = (uint8_t) atoi((char*)&resp[0]);
    printf("signal : %d\r\n", (int) signal);
  }
  SIM_UnlockCMD(hsim);

  if (signal == 99) {
    signal = 0;
    SIM_ReqisterNetwork(hsim);
  }
  hsim->signal = signal;

  return signal;
}


void SIM_ReqisterNetwork(SIM_HandlerTypeDef *hsim)
{
  uint8_t resp[16];
  // uint8_t resp_n = 0;
  uint8_t resp_stat = 0;

  // send command then get response;
  SIM_LockCMD(hsim);
  SIM_SendCMD(hsim, "AT+CREG?");
  if (SIM_GetResponse(hsim, "+CREG", 5, &resp[0], 16, SIM_GETRESP_WAIT_OK, 2000) == SIM_OK) {
    // resp_n = (uint8_t) atoi((char*)&resp[0]);
    resp_stat = (uint8_t) atoi((char*)&resp[2]);
  }
  else goto endcmd;

  // check response
  if (resp_stat == 1) {
    SIM_SET_STATE(hsim, SIM_STATE_REGISTERED);
  }
  else {
    SIM_UNSET_STATE(hsim, SIM_STATE_REGISTERED);

    if (resp_stat == 0) {
      // write creg
      SIM_SendCMD(hsim, "AT+CREG=1");
      if (!SIM_IsResponseOK(hsim)) goto endcmd;

      // execute creg
      SIM_SendCMD(hsim, "AT+CREG");
      if (!SIM_IsResponseOK(hsim)) goto endcmd;
    }
  }

  endcmd:
  SIM_UnlockCMD(hsim);
}


SIM_Datetime SIM_GetTime(SIM_HandlerTypeDef *hsim)
{
  SIM_Datetime result;
  uint8_t resp[22];

  // send command then get response;
  SIM_LockCMD(hsim);
  SIM_SendCMD(hsim, "AT+CCLK?");
  if (SIM_GetResponse(hsim, "+CCLK", 5, resp, 22, SIM_GETRESP_WAIT_OK, 2000) == SIM_OK) {
    str2Time(&result, (char*)&resp[1]);
  }
  SIM_UnlockCMD(hsim);

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
  if (!SIM_IS_STATE(hsim, SIM_STATE_REGISTERED)) return;

  SIM_LockCMD(hsim);
  SIM_SendCMD(hsim, "AT+CSCS=\"GSM\"");
  if (!SIM_IsResponseOK(hsim)){
    goto endcmd;
  }

  SIM_SendCMD(hsim, "AT+CUSD=1,%s,15", ussd);

  endcmd:
  SIM_UnlockCMD(hsim);
}


static void SIM_reset(SIM_HandlerTypeDef *hsim)
{
#ifdef SIM_EN_FEATURE_SOCKET
  for (uint8_t i = 0; i < SIM_NUM_OF_SOCKET; i++) {
    if (hsim->net.sockets[i] != NULL) {
      if (hsim->net.sockets[i]->onClosed != NULL) 
        hsim->net.sockets[i]->onClosed();
      hsim->net.sockets[i] = NULL;
    }
  }
#endif

  hsim->state = 0;
}

static void str2Time(SIM_Datetime *dt, const char *str)
{
  uint8_t *dtbytes = (uint8_t*) dt;
  for (uint8_t i = 0; i < 6; i++) {
    *dtbytes = (uint8_t) atoi(str);
    dtbytes++;
    str += 3;
  }
}
