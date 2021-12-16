/*
 * utils.h
 *
 *  Created on: Dec 3, 2021
 *      Author: janoko
 */

#include "include/simcom.h"
#include "include/simcom/conf.h"
#include "include/simcom/utils.h"
#include <string.h>
#include <dma_streamer.h>


void SIM_SendCMD(SIM_HandlerTypeDef *hsim, const char *data, uint16_t size)
{
  STRM_Write(hsim->dmaStreamer, (uint8_t*)data, size, STRM_BREAK_CRLF);
}


void SIM_SendData(SIM_HandlerTypeDef *hsim, const uint8_t *data, uint16_t size)
{
  STRM_Write(hsim->dmaStreamer, (uint8_t*)data, size, STRM_BREAK_NONE);
}



uint8_t SIM_WaitResponse( SIM_HandlerTypeDef *hsim,
                          const char *respCode, uint16_t rcsize,
                          uint32_t timeout)
{

  if (timeout == 0) timeout = hsim->timeout;

  hsim->bufferLen = STRM_Read(hsim->dmaStreamer, hsim->buffer, rcsize, timeout);
  if (SIM_IsResponse(hsim, respCode, rcsize)) {
    return 1;
  }
  return 0;
}


uint8_t SIM_GetResponse(SIM_HandlerTypeDef *hsim,
                        const char *respCode, uint16_t rcsize,
                        uint8_t *respData, uint16_t rdsize,
                        uint8_t getRespType,
                        uint32_t timeout)
{
  uint16_t i;
  uint8_t resp = SIM_RESP_TIMEOUT;
  uint8_t flagToReadResp = 0;
  uint32_t tickstart = SIM_GetTick();

  if (timeout == 0) timeout = hsim->timeout;

  // wait until available
  while(1) {
    if((SIM_GetTick() - tickstart) >= timeout) break;

    hsim->bufferLen = STRM_Readline(hsim->dmaStreamer, hsim->buffer, SIM_BUFFER_SIZE, timeout);
    if (hsim->bufferLen) {
      if (rcsize && strncmp((char *)hsim->buffer, respCode, (int) rcsize) == 0) {
        if (flagToReadResp) continue;

        // read response data
        for (i = 2; i < hsim->bufferLen && rdsize; i++) {
          // split string
          if (!flagToReadResp && hsim->buffer[i-2] == ':' && hsim->buffer[i-1] == ' ') {
            flagToReadResp = 1;
          }

          if (flagToReadResp) {
            *respData = hsim->buffer[i];
            respData++;
            rdsize--;
          }
        }
        if (getRespType == SIM_GETRESP_ONLY_DATA) break;
        if (resp) break;
      }
      else if (SIM_IsResponse(hsim, "OK", 2)) {
        resp = SIM_RESP_OK;
      }
      else if (SIM_IsResponse(hsim, "ERROR", 5)) {
        resp = SIM_RESP_ERROR;
      }

      // check is got async response
      else {
        SIM_HandleAsyncResponse(hsim);
      }

      // break if will not get data
      if (resp && !rcsize) break;
      else if (resp && flagToReadResp) break;
    }
  }

  return resp;
}

uint16_t SIM_GetData(SIM_HandlerTypeDef *hsim, uint8_t *respData, uint16_t rdsize, uint32_t timeout)
{
  uint16_t len = 0;
  uint32_t tickstart = SIM_GetTick();

  if (timeout == 0) timeout = hsim->timeout;
  while (len < rdsize) {
    if ((SIM_GetTick() - tickstart) >= timeout) break;

    len += STRM_ReadBuffer(hsim->dmaStreamer, respData, rdsize, STRM_BREAK_NONE);
    if (len == 0) SIM_Delay(1);
  }
  return len;
}


const uint8_t * SIM_ParseStr(const uint8_t *separator, uint8_t delimiter, int idx, uint8_t *output)
{
  uint8_t isInStr = 0;

  while (1)
  {
    if (*separator == 0 || *separator == '\r') break;

    if (!isInStr && *separator == delimiter) {
      idx--;
      if (idx < 0) {
        separator++;
        break;
      }
    }

    else if (*separator == '\"') {
      if (isInStr)  isInStr = 0;
      else          isInStr = 1;
    }

    else if (idx == 0) {
      *output = *separator;
      output++;
    }
    separator++;
  }

  return separator;
}
