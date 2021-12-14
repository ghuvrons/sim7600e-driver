/*
 * utils.h
 *
 *  Created on: Dec 3, 2021
 *      Author: janoko
 */

#ifndef SIM5320E_INC_SIMCOM_UTILS_H_
#define SIM5320E_INC_SIMCOM_UTILS_H_

#include "../simcom.h"

void SIM_SendCMD(SIM_HandlerTypeDef *hsim, const char *data, uint16_t size);
void SIM_SendData(SIM_HandlerTypeDef *hsim, const uint8_t *data, uint16_t size);
uint8_t SIM_WaitResponse(SIM_HandlerTypeDef *hsim,
                            const char *respCode, uint16_t rcsize,
                            uint32_t timeout);
uint8_t SIM_GetResponse(SIM_HandlerTypeDef *hsim,
                        const char *respCode, uint16_t rcsize,
                        uint8_t *respData, uint16_t rdsize,
                        uint8_t getRespType,
                        uint32_t timeout);

// MACROS

#define SIM_IsResponse(hsim, resp, bufLen, min_len) \
 ((bufLen) >= (min_len) && strncmp((const char *)(hsim)->buffer, (resp), (int)(min_len)) == 0)

#define SIM_IsResponseOK(hsim) (SIM_GetResponse((hsim), NULL, 0, NULL, 0, SIM_GETRESP_WAIT_OK, 0) == SIM_RESP_OK)

#endif /* SIM5320E_SRC_INCLUDE_SIMCOM_UTILS_H_ */
