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
#include "Include/simcom/socket.h"
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
  while(SIM_IS_STATUS(hsim, SIM_STATUS_CMD_RUNNING)){
    SIM_Delay(1);
  }
  SIM_SET_STATUS(hsim, SIM_STATUS_CMD_RUNNING);
}

__weak void SIM_UnlockCMD(SIM_HandlerTypeDef *hsim)
{
  SIM_UNSET_STATUS(hsim, SIM_STATUS_CMD_RUNNING);
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
  SIM_LockCMD(hsim);
  hsim->bufferLen = STRM_Readline(hsim->dmaStreamer, hsim->buffer, SIM_BUFFER_SIZE, timeout);
  if (hsim->bufferLen) {
    SIM_HandleAsyncResponse(hsim);
  }
  SIM_UnlockCMD(hsim);
}


/*
 * Handle async response
 */
void SIM_HandleAsyncResponse(SIM_HandlerTypeDef *hsim)
{
  // check async response
  if (SIM_IsResponse(hsim, "START", 5)) {
    SIM_reset(hsim);
  }

  else if (!SIM_IS_STATUS(hsim, SIM_STATUS_START) && SIM_IsResponse(hsim, "PB ", 3)) {
    SIM_SET_STATUS(hsim, SIM_STATUS_START);
  }

#ifdef SIM_EN_FEATURE_SOCKET
  else if (SIM_NetCheckAsyncResponse(hsim)) return;
#endif

}


void SIM_CheckAT(SIM_HandlerTypeDef *hsim)
{
  SIM_LockCMD(hsim);

  // send command;
  SIM_SendCMD(hsim, "AT", 2);

  // wait response
  if (SIM_IsResponseOK(hsim)){
    SIM_SET_STATUS(hsim, SIM_STATUS_CONNECT);
  } else {
    SIM_UNSET_STATUS(hsim, SIM_STATUS_CONNECT);
  }
  SIM_UnlockCMD(hsim);
}


SIM_Datetime SIM_GetTime(SIM_HandlerTypeDef *hsim)
{
  SIM_Datetime result;
  uint8_t resp[22];

  SIM_LockCMD(hsim);

  SIM_SendCMD(hsim, "AT+CCLK?", 8);
  if (SIM_GetResponse(hsim, "+CCLK", 5, resp, 22, SIM_GETRESP_WAIT_OK, 2000) == SIM_RESP_OK) {
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

  hsim->status = 0;
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
