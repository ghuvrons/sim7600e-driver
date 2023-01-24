/*
 * utils.h
 *
 *  Created on: Dec 3, 2021
 *      Author: janoko
 */

#include "include/simcom.h"
#include "include/simcom/conf.h"
#include "include/simcom/utils.h"
#include "include/simcom/debug.h"
#include <stdarg.h>
#include <stdio.h>
#include <string.h>


 __attribute__((weak)) void SIM_Printf(const char *format, ...) {}
 __attribute__((weak)) void SIM_Println(const char *format, ...) {}

uint8_t SIM_SendCMD(SIM_HandlerTypeDef *hsim, const char *format, ...)
{
  uint16_t writeStatus;
  va_list arglist;

  va_start( arglist, format );
  hsim->cmdBufferLen = vsprintf(hsim->cmdBuffer, format, arglist);
  va_end( arglist );
  writeStatus = hsim->serial.writeline(hsim->serial.device,
                                       (uint8_t*)hsim->cmdBuffer,
                                       hsim->cmdBufferLen, 5000);
  if (writeStatus < 0) return 0;
  return 1;
}


uint8_t SIM_SendData(SIM_HandlerTypeDef *hsim, const uint8_t *data, uint16_t size)
{
  uint16_t writeStatus;

  do {
    writeStatus = hsim->serial.write(hsim->serial.device, data, size, 5000);
    if (writeStatus <= 0) return 0;
    data += writeStatus;
    size -= writeStatus;
  } while (size);

  return 1;
}



uint8_t SIM_WaitResponse(SIM_HandlerTypeDef *hsim,
                         const char *respCode, uint16_t rcsize,
                         uint32_t timeout)
{
  int readStatus;
  uint32_t tickstart = hsim->getTick();
  if (rcsize > SIM_RESP_BUFFER_SIZE) rcsize = SIM_RESP_BUFFER_SIZE;
  if (timeout == 0) timeout = hsim->timeout;

  while (1) {
    if ((hsim->getTick() - tickstart) >= timeout) break;
    readStatus = hsim->serial.read(hsim->serial.device, hsim->respBuffer, rcsize, timeout);
    if (SIM_IsResponse(hsim, respCode, rcsize)) {
      return 1;
    }

    hsim->serial.unread(hsim->serial.device, readStatus);

    readStatus = hsim->serial.readline(hsim->serial.device, hsim->respBuffer, SIM_RESP_BUFFER_SIZE, timeout);
    if (readStatus > 0) {
      hsim->respBufferLen = readStatus;
      SIM_CheckAsyncResponse(hsim);
    }
  }
  return 0;
}


SIM_Status_t SIM_GetResponse( SIM_HandlerTypeDef *hsim,
                              const char *respCode, uint16_t rcsize,
                              uint8_t *respData, uint16_t rdsize,
                              uint8_t getRespType,
                              uint32_t timeout)
{
  uint16_t i;
  uint8_t resp = SIM_TIMEOUT;
  uint8_t flagToReadResp = 0;
  uint32_t tickstart = hsim->getTick();
  int readStatus;

  if (timeout == 0) timeout = hsim->timeout;

  // wait until available
  while(1) {
    if((hsim->getTick() - tickstart) >= timeout) break;

    readStatus = hsim->serial.readline(hsim->serial.device, hsim->respBuffer, SIM_RESP_BUFFER_SIZE, timeout);
    if (readStatus > 0) {
      hsim->respBufferLen = readStatus;
      hsim->respBuffer[hsim->respBufferLen] = 0;
      if (rcsize && strncmp((char *)hsim->respBuffer, respCode, (int) rcsize) == 0) {
        if (flagToReadResp) continue;

        // read response data
        for (i = 2; i < hsim->respBufferLen && rdsize; i++) {
          // split string
          if (!flagToReadResp && hsim->respBuffer[i-2] == ':' && hsim->respBuffer[i-1] == ' ') {
            flagToReadResp = 1;
          }

          if (flagToReadResp) {
            *respData = hsim->respBuffer[i];
            respData++;
            rdsize--;
          }
        }
        if (rdsize) *respData = 0;
        if (getRespType == SIM_GETRESP_ONLY_DATA) {
          resp = SIM_OK;
          break;
        }
        if (resp != SIM_TIMEOUT) break;
      }
      else if (getRespType != SIM_GETRESP_ONLY_DATA && SIM_IsResponse(hsim, "OK", 2)) {
        resp = SIM_OK;
      }
      else if (SIM_IsResponse(hsim, "ERROR", 5)) {
        resp = SIM_ERROR;
      }
      else if (SIM_IsResponse(hsim, "+CME ERROR", 10)) {
        resp = SIM_ERROR;
        hsim->respBuffer[hsim->respBufferLen] = 0;
        SIM_Debug("[Error] %s", (char*) (hsim->respBuffer+10));
      }

      // check is got async response
      else {
        SIM_CheckAsyncResponse(hsim);
      }

      // break if will not get data
      if (resp != SIM_TIMEOUT) {
        break;
      }
    }
  }

  return resp;
}


uint16_t SIM_GetData(SIM_HandlerTypeDef *hsim, uint8_t *respData, uint16_t rdsize, uint32_t timeout)
{
  return hsim->serial.read(hsim->serial.device, respData, rdsize, timeout);
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

    else if (idx == 0 && output != 0) {
      *output = *separator;
      output++;
    }
    separator++;
  }

  if (output != 0) {
    *output = 0;
  }

  return separator;
}
