/*
 * utils.h
 *
 *  Created on: Dec 3, 2021
 *      Author: janoko
 */

#ifndef SIM5320E_INC_SIMCOM_UTILS_H_
#define SIM5320E_INC_SIMCOM_UTILS_H_

#include "../simcom.h"


// MACROS

#ifndef SIM_GetTick
#define SIM_GetTick() HAL_GetTick()
#endif
#ifndef SIM_Delay
#define SIM_Delay(ms) HAL_Delay(ms)
#endif

#define SIM_IsTimeout(lastTick, timeout) ((SIM_GetTick() - (lastTick)) > (timeout))

#define SIM_IsResponse(hsim, resp, min_len) \
  ((hsim)->respBufferLen >= (min_len) \
      && strncmp((const char *)(hsim)->respBuffer, (resp), (int)(min_len)) == 0)

#define SIM_IsResponseOK(hsim) \
  (SIM_GetResponse((hsim), NULL, 0, NULL, 0, SIM_GETRESP_WAIT_OK, 0) == SIM_OK)
#endif /* SIM5320E_SRC_INCLUDE_SIMCOM_UTILS_H_ */


void          SIM_SendCMD(SIM_HandlerTypeDef*, const char *format, ...);
void          SIM_SendData(SIM_HandlerTypeDef*, const uint8_t *data, uint16_t size);
uint8_t       SIM_WaitResponse(SIM_HandlerTypeDef*, const char *respCode, uint16_t rcsize, uint32_t timeout);
SIM_Status_t  SIM_GetResponse(SIM_HandlerTypeDef*, const char *respCode, uint16_t rcsize,
                              uint8_t *respData, uint16_t rdsize,
                              uint8_t getRespType,
                              uint32_t timeout);
uint16_t      SIM_GetData(SIM_HandlerTypeDef*, uint8_t *respData, uint16_t rdsize, uint32_t timeout);
const uint8_t *SIM_ParseStr(const uint8_t *separator, uint8_t delimiter, int idx, uint8_t *output);
