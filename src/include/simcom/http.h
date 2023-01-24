/*
 * gps.h
 *
 *  Created on: May 18, 2022
 *      Author: janoko
 */

#ifndef SIM7600E_INC_HTTP_H
#define SIM7600E_INC_HTTP_H_

#include "../simcom.h"
#include "conf.h"

#if SIM_EN_FEATURE_HTTP

#define SIM_HTTP_STATUS_STARTED       0x01
#define SIM_HTTP_STATUS_CONNECTED     0x02
#define SIM_HTTP_STATUS_REQUESTING    0x04
#define SIM_HTTP_STATUS_READ_CONTENT  0x08
#define SIM_HTTP_STATUS_GOT_CONTENT   0x10

#define SIM_HTTP_EVENT_NEW_REQ      0x01
#define SIM_HTTP_EVENT_NEW_RESP     0x02
#define SIM_HTTP_EVENT_NEXT_CONTENT 0x04

#define SIM_HTTP_NO_ERROR                 0x00
#define SIM_HTTP_ERR_UNKNOWN              0x01
#define SIM_HTTP_ERR_SERVICE_CANNOT_INIT  0x02


typedef struct {
  const char* url;
  uint8_t method;
} SIM_HTTP_Request_t;

typedef struct {
  // set by user
  uint8_t *head;      // optional for buffer head
  uint16_t headSize;  // optional for buffer head size
  uint8_t *data;      // optional for buffer data
  uint16_t dataSize;  // optional for buffer data size
  void (*onGetData)(uint8_t *data, uint16_t len);

  // set by simcom
  uint8_t status;
  uint8_t err;
  uint16_t code;
  uint16_t contentLen;
  uint16_t contentHandledLen;
  uint16_t contentHandleLen;
} SIM_HTTP_Response_t;

uint8_t SIM_HTTP_CheckAsyncResponse(SIM_HandlerTypeDef*);
void    SIM_HTTP_HandleEvents(SIM_HandlerTypeDef*);

SIM_Status_t SIM_HTTP_Get(SIM_HandlerTypeDef*, const char *url, SIM_HTTP_Response_t*, uint32_t timeout);

#endif /* SIM_EN_FEATURE_GPS */
#endif /* SIM7600E_INC_HTTP_H */
