/*
 * utils.h
 *
 *  Created on: Dec 3, 2021
 *      Author: janoko
 */

#include "Include/simcom.h"
#include "Include/simcom/conf.h"
#include "Include/simcom/utils.h"
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



uint8_t SIM_WaitResponse(SIM_HandlerTypeDef *hsim,
                            const char *respCode, uint16_t rcsize,
                            uint32_t timeout)
{
  uint16_t len = 0;

  if(timeout == 0) timeout = hsim->timeout;

  STRM_Read(hsim->dmaStreamer, hsim->buffer, rcsize, timeout);
  if(len){
    if(SIM_IsResponse(hsim, respCode, len, rcsize)){
      return 1;
    }
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
  uint16_t bufLen = 0;
  uint8_t flagToReadResp = 0;
  uint32_t tickstart = SIM_GetTick();

  if(timeout == 0) timeout = hsim->timeout;

  // wait until available
  while(1){
    if((SIM_GetTick() - tickstart) >= timeout) break;
    bufLen = SIM_checkResponse(hsim, timeout);
    if(bufLen){
      if(rcsize && strncmp((char *)hsim->buffer, respCode, (int) rcsize) == 0){
        if(flagToReadResp) continue;
        // read response data
        for(i = 2; i < bufLen && rdsize; i++){
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
        if(getRespType == SIM_GETRESP_ONLY_DATA) break;
        if(resp) break;
      }
      else if(SIM_IsResponse(hsim, "OK", bufLen, 2)){
        resp = SIM_RESP_OK;
      }
      else if(SIM_IsResponse(hsim, "ERROR", bufLen, 5)){
        resp = SIM_RESP_ERROR;
      }

      // break if will not get data
      if(resp && !rcsize) break;
      else if (resp && flagToReadResp) break;
    }
  }

  return resp;
}
