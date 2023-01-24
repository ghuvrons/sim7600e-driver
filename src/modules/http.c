/*
 * net.c
 *
 *  Created on: Apr 1, 2022
 *      Author: janoko
 */


#include "../include/simcom.h"
#include "../include/simcom/net.h"
#include "../include/simcom/http.h"
#include "../include/simcom/utils.h"
#include "../include/simcom/debug.h"
#include <stdlib.h>

#if SIM_EN_FEATURE_HTTP

static SIM_Status_t httpRequest(SIM_HandlerTypeDef*);
static SIM_Status_t httpHandleResponse(SIM_HandlerTypeDef*);
static SIM_Status_t readHead(SIM_HandlerTypeDef*);
static SIM_Status_t readContent(SIM_HandlerTypeDef*);
static SIM_Status_t readNextContent(SIM_HandlerTypeDef*);
static SIM_Status_t closeHttpService(SIM_HandlerTypeDef*);

uint8_t SIM_HTTP_CheckAsyncResponse(SIM_HandlerTypeDef *hsim)
{
  uint8_t isGet = 0;

  if ((isGet = (hsim->respBufferLen >= 14
                && SIM_IsResponse(hsim, "+HTTPACTION", 11))))
  {
    SIM_HTTP_Request_t  *request  = (SIM_HTTP_Request_t*)   hsim->http.request;
    SIM_HTTP_Response_t *response = (SIM_HTTP_Response_t*)  hsim->http.response;
    char                *resp = (char*) &SIM_RespTmp[32];
    const uint8_t       *next;

    if (request == 0 || hsim->http.response == 0) return 0;

    next = SIM_ParseStr(&hsim->respBuffer[13], ',', 0, (uint8_t*) resp);
    if (request->method != (uint8_t) atoi((char*) resp))
      return 0;

    next = SIM_ParseStr(next, ',', 0, (uint8_t*) resp);
    response->code = (uint16_t) atoi((char*) resp);

    next = SIM_ParseStr(next, ',', 0, (uint8_t*) resp);
    response->contentLen = (uint16_t) atoi((char*) resp);

    SIM_BITS_SET(hsim->http.events, SIM_HTTP_EVENT_NEW_RESP);
  }

  else if ((isGet = (hsim->respBufferLen >= 12 && SIM_IsResponse(hsim, "+HTTPHEAD", 9)))) {
    readHead(hsim);
  }

  else if ((isGet = (hsim->respBufferLen >= 12 && SIM_IsResponse(hsim, "+HTTPREAD", 9)))) {
    readContent(hsim);
    SIM_HTTP_Response_t *response = (SIM_HTTP_Response_t*)  hsim->http.response;
    if (response != 0)
      SIM_BITS_SET(response->status, SIM_HTTP_STATUS_GOT_CONTENT);
  }

  else if ((isGet = (hsim->respBufferLen >= 17 && SIM_IsResponse(hsim, "+HTTP_PEER_CLOSED", 17)))) {

  }

  else if ((isGet = (hsim->respBufferLen >= 17 && SIM_IsResponse(hsim, "+HTTP_NONET_EVENT", 17)))) {

  }

  return isGet;
}


void SIM_HTTP_HandleEvents(SIM_HandlerTypeDef *hsim)
{
  if (SIM_NET_IS_STATUS(hsim, SIM_NET_STATUS_OPEN)) {
    if (SIM_BITS_IS(hsim->http.events, SIM_HTTP_EVENT_NEW_REQ)) {
      SIM_BITS_UNSET(hsim->http.events, SIM_HTTP_EVENT_NEW_REQ);
      httpRequest(hsim);
    }
  }

  if (SIM_BITS_IS(hsim->http.events, SIM_HTTP_EVENT_NEW_RESP)) {
    SIM_BITS_UNSET(hsim->http.events, SIM_HTTP_EVENT_NEW_RESP);
    httpHandleResponse(hsim);
  }

  if (SIM_BITS_IS(hsim->http.events, SIM_HTTP_EVENT_NEXT_CONTENT)) {
    SIM_BITS_UNSET(hsim->http.events, SIM_HTTP_EVENT_NEXT_CONTENT);
    readNextContent(hsim);
  }
}


SIM_Status_t SIM_HTTP_Get(SIM_HandlerTypeDef *hsim,
                          const char *url,
                          SIM_HTTP_Response_t *response,
                          uint32_t timeout)
{
  SIM_Status_t        status    = SIM_TIMEOUT;
  uint32_t            firstTick = hsim->getTick();
  SIM_HTTP_Request_t  request;

  request.url     = url;
  request.method  = 0;

  hsim->http.request  = &request;
  hsim->http.response = response;

  response->status            = 0;
  response->err               = 0;
  response->code              = 0;
  response->contentLen        = 0;
  response->contentHandledLen = 0;
  response->contentHandleLen  = 0;

  // wait requesting was done
  while (SIM_HTTP_IS_STATUS(hsim, SIM_HTTP_STATUS_REQUESTING)) {
    if (hsim->getTick() - firstTick > timeout) {
      goto endCmd;
    }
    hsim->delay(1);
  }

  SIM_HTTP_SET_STATUS(hsim, SIM_HTTP_STATUS_REQUESTING);
  SIM_BITS_SET(hsim->http.events, SIM_HTTP_EVENT_NEW_REQ);
  SIM_BITS_SET(response->status, SIM_HTTP_STATUS_REQUESTING);

  while (1) {
    if ((hsim->getTick() - firstTick) > timeout) {
      goto endCmd;
    }
    if (!SIM_BITS_IS(response->status, SIM_HTTP_STATUS_REQUESTING)) {
      break;
    }
    if (SIM_BITS_IS(response->status, SIM_HTTP_STATUS_GOT_CONTENT)) {
      SIM_BITS_UNSET(response->status, SIM_HTTP_STATUS_GOT_CONTENT);

      firstTick = hsim->getTick();
      if (response->onGetData != 0 && response->contentHandleLen > 0)
        response->onGetData(response->data, response->contentHandleLen);

      response->contentHandledLen += response->contentHandleLen;
      if (response->contentHandledLen < response->contentLen) {
        SIM_BITS_SET(hsim->http.events, SIM_HTTP_EVENT_NEXT_CONTENT);
      }
      continue;
    }
    hsim->delay(1);
  }

  status = SIM_OK;
  if (response->err != SIM_HTTP_NO_ERROR) {
    status = SIM_ERROR;
  }

endCmd:
  SIM_BITS_UNSET(response->status, SIM_HTTP_STATUS_REQUESTING);
  return status;
}


static SIM_Status_t httpRequest(SIM_HandlerTypeDef *hsim)
{
  SIM_Status_t        status    = SIM_TIMEOUT;
  SIM_HTTP_Request_t  *request  = (SIM_HTTP_Request_t*)   hsim->http.request;
  SIM_HTTP_Response_t *response = (SIM_HTTP_Response_t*)  hsim->http.response;

  if (request == 0 || response == 0) return SIM_ERROR;

  /**
   * 1. AT+HTTPINIT
   * 2. AT+HTTPPARA="URL","https://..."
   * 3. AT+HTTPACTION=0
   * 4. AT+HTTPTERM
   */

  hsim->mutexLock(hsim);

  SIM_SendCMD(hsim, "AT+HTTPINIT");
  if (!SIM_IsResponseOK(hsim)) {
    response->err = SIM_HTTP_ERR_SERVICE_CANNOT_INIT;
    goto errorHandler;
  }

  SIM_SendCMD(hsim, "AT+HTTPPARA=\"URL\",\"%s\"", request->url);
  if (!SIM_IsResponseOK(hsim)) {
    goto errorHandler;
  }

  SIM_SendCMD(hsim, "AT+HTTPACTION=%d", request->method);
  if (!SIM_IsResponseOK(hsim)) {
    goto errorHandler;
  }

  hsim->mutexUnlock(hsim);
  status = SIM_OK;
  return status;

errorHandler:
  if (response->err != SIM_HTTP_ERR_SERVICE_CANNOT_INIT) {
    SIM_SendCMD(hsim, "AT+HTTPTERM");
    if (!SIM_IsResponseOK(hsim)) {}
  }

  hsim->mutexUnlock(hsim);

  SIM_BITS_UNSET(response->status, SIM_HTTP_STATUS_REQUESTING);
  SIM_HTTP_UNSET_STATUS(hsim, SIM_HTTP_STATUS_REQUESTING);
  return status;
}


static SIM_Status_t httpHandleResponse(SIM_HandlerTypeDef *hsim)
{
  SIM_Status_t        status    = SIM_TIMEOUT;
  SIM_HTTP_Request_t  *request  = (SIM_HTTP_Request_t*)   hsim->http.request;
  SIM_HTTP_Response_t *response = (SIM_HTTP_Response_t*)  hsim->http.response;
  uint8_t             *resp     = &SIM_RespTmp[0];
  uint8_t             *resp2    = &SIM_RespTmp[32];
  // uint16_t            contentLen;

  if (request == 0 || response == 0) return SIM_ERROR;

  hsim->mutexLock(hsim);

  if (response->code > 600) goto endCmd;

  if (response->head != 0) {
    SIM_SendCMD(hsim, "AT+HTTPHEAD");
    if (!SIM_IsResponseOK(hsim)) {
      goto endCmd;
    }
  }

  if (response->data != 0) {
    SIM_SendCMD(hsim, "AT+HTTPREAD=%d,%d", 0, response->dataSize);
    if (!SIM_IsResponseOK(hsim)) {
      goto endCmd;
    }
  }

  status = SIM_OK;

endCmd:
  hsim->mutexUnlock(hsim);

  if (status != SIM_OK)
    closeHttpService(hsim);

  return status;
}

static SIM_Status_t readHead(SIM_HandlerTypeDef *hsim)
{
  SIM_Status_t        status    = SIM_TIMEOUT;
  SIM_HTTP_Response_t *response = (SIM_HTTP_Response_t*)  hsim->http.response;
  const uint8_t       *resp     = &hsim->respBuffer[11];
  uint8_t             *resp2    = &SIM_RespTmp[0];
  uint16_t            headLen;
  uint16_t            readLen;

  if (strncmp((const char*)resp, "DATA", 4) == 0)
    SIM_ParseStr(resp, ',', 1, (uint8_t*) resp2);

  headLen = (uint16_t) atoi((char*)resp2);

  if (headLen > response->headSize)  readLen = response->headSize;
  else                               readLen = headLen;
  hsim->serial.read(hsim->serial.device, response->head, readLen, 5000);
  headLen -= readLen;

  // just for read all
  while (headLen) {
    if (headLen > 64)  readLen = 64;
    else               readLen = headLen;
    hsim->serial.read(hsim->serial.device, resp2, readLen, 5000);
    headLen -= readLen;
  }
  return status;
}


static SIM_Status_t readContent(SIM_HandlerTypeDef *hsim)
{
  SIM_Status_t        status    = SIM_OK;
  SIM_HTTP_Response_t *response = (SIM_HTTP_Response_t*)  hsim->http.response;
  const uint8_t       *resp     = &hsim->respBuffer[11];
  uint8_t             *resp2    = &SIM_RespTmp[0];
  uint16_t            contentLen;
  uint16_t            readLen;

  if (strncmp((const char*)resp, "DATA", 4) == 0)
    SIM_ParseStr(resp, ',', 1, (uint8_t*) resp2);

  else return;

  contentLen = (uint16_t) atoi((char*)resp2);
  response->contentHandleLen = contentLen;

  if (contentLen > response->dataSize)  readLen = response->dataSize;
  else                                  readLen = contentLen;

  hsim->serial.read(hsim->serial.device, response->data, readLen, 5000);
  contentLen -= readLen;

  // just for read all
  while (contentLen) {
    if (contentLen > 64)  readLen = 64;
    else                  readLen = contentLen;

    hsim->serial.read(hsim->serial.device, resp2, readLen, 5000);
    contentLen -= readLen;
  }

  return status;
}


static SIM_Status_t readNextContent(SIM_HandlerTypeDef *hsim)
{
  SIM_Status_t        status    = SIM_OK;
  SIM_HTTP_Response_t *response = (SIM_HTTP_Response_t*) hsim->http.response;

  if (response == 0) return SIM_ERROR;

  hsim->mutexLock(hsim);
  if (response->data != 0) {
    SIM_SendCMD(hsim, "AT+HTTPREAD=%d,%d", response->contentHandledLen, response->dataSize);
    if (!SIM_IsResponseOK(hsim)) {}
  }
  hsim->mutexUnlock(hsim);

  return status;
}


static SIM_Status_t closeHttpService(SIM_HandlerTypeDef *hsim)
{
  SIM_Status_t        status    = SIM_OK;
  SIM_HTTP_Response_t *response = (SIM_HTTP_Response_t*)  hsim->http.response;

  hsim->mutexLock(hsim);
  SIM_SendCMD(hsim, "AT+HTTPTERM");
  if (!SIM_IsResponseOK(hsim)) {
  }

  SIM_HTTP_UNSET_STATUS(hsim, SIM_HTTP_STATUS_REQUESTING);
  SIM_BITS_UNSET(response->status, SIM_HTTP_STATUS_REQUESTING);
  hsim->http.request  = 0;
  hsim->http.response = 0;
  hsim->mutexUnlock(hsim);

  return status;
}


#endif /* SIM_EN_FEATURE_HTTP */
