/*
 * utils.h
 *
 *  Created on: Dec 3, 2021
 *      Author: janoko
 */

#ifndef SIM7600E_INC_SIMCOM_UTILS_H_
#define SIM7600E_INC_SIMCOM_UTILS_H_

#include "../simcom.h"
#include <string.h>


#define SIM_IsTimeout(hsim, lastTick, timeout) (((hsim)->getTick() - (lastTick)) > (timeout))

#define SIM_IsResponse(hsim, resp, min_len) \
  ((hsim)->respBufferLen >= (min_len) \
    && strncmp((const char *)(hsim)->respBuffer, (resp), (int)(min_len)) == 0)

#define SIM_IsResponseOK(hsim) \
  (SIM_GetResponse((hsim), NULL, 0, NULL, 0, SIM_GETRESP_WAIT_OK, 0) == SIM_OK)


#define SIM_BITS_IS_ALL(bits, bit) (((bits) & (bit)) == (bit))
#define SIM_BITS_IS_ANY(bits, bit) ((bits) & (bit))
#define SIM_BITS_IS(bits, bit)     SIM_BITS_IS_ALL(bits, bit)
#define SIM_BITS_SET(bits, bit)    {(bits) |= (bit);}
#define SIM_BITS_UNSET(bits, bit)  {(bits) &= ~(bit);}

#define SIM_IS_STATUS(hsim, stat)     SIM_BITS_IS_ALL((hsim)->status, stat)
#define SIM_SET_STATUS(hsim, stat)    SIM_BITS_SET((hsim)->status, stat)
#define SIM_UNSET_STATUS(hsim, stat)  SIM_BITS_UNSET((hsim)->status, stat)

#if SIM_EN_FEATURE_NET
#define SIM_NET_IS_STATUS(hsim, stat)     SIM_BITS_IS_ALL((hsim)->net.status, stat)
#define SIM_NET_SET_STATUS(hsim, stat)    SIM_BITS_SET((hsim)->net.status, stat)
#define SIM_NET_UNSET_STATUS(hsim, stat)  SIM_BITS_UNSET((hsim)->net.status, stat)
#endif

#if SIM_EN_FEATURE_HTTP
#define SIM_HTTP_IS_STATUS(hsim, stat)     SIM_BITS_IS_ALL((hsim)->http.status, stat)
#define SIM_HTTP_SET_STATUS(hsim, stat)    SIM_BITS_SET((hsim)->http.status, stat)
#define SIM_HTTP_UNSET_STATUS(hsim, stat)  SIM_BITS_UNSET((hsim)->http.status, stat)
#endif

#if SIM_EN_FEATURE_MQTT
#define SIM_MQTT_IS_STATUS(hsim, stat)     SIM_BITS_IS_ALL((hsim)->mqtt.status, stat)
#define SIM_MQTT_SET_STATUS(hsim, stat)    SIM_BITS_SET((hsim)->mqtt.status, stat)
#define SIM_MQTT_UNSET_STATUS(hsim, stat)  SIM_BITS_UNSET((hsim)->mqtt.status, stat)
#endif

#if SIM_EN_FEATURE_GPS
#define SIM_GPS_IS_STATUS(hsim, stat)     SIM_BITS_IS_ALL((hsim)->gps.status, stat)
#define SIM_GPS_SET_STATUS(hsim, stat)    SIM_BITS_SET((hsim)->gps.status, stat)
#define SIM_GPS_UNSET_STATUS(hsim, stat)  SIM_BITS_UNSET((hsim)->gps.status, stat)
#endif

uint8_t       SIM_SendCMD(SIM_HandlerTypeDef*, const char *format, ...);
uint8_t       SIM_SendData(SIM_HandlerTypeDef*, const uint8_t *data, uint16_t size);
uint8_t       SIM_WaitResponse(SIM_HandlerTypeDef*, const char *respCode, uint16_t rcsize, uint32_t timeout);
SIM_Status_t  SIM_GetResponse(SIM_HandlerTypeDef*, const char *respCode, uint16_t rcsize,
                              uint8_t *respData, uint16_t rdsize,
                              uint8_t getRespType,
                              uint32_t timeout);
uint16_t      SIM_GetData(SIM_HandlerTypeDef*, uint8_t *respData, uint16_t rdsize, uint32_t timeout);
const uint8_t *SIM_ParseStr(const uint8_t *separator, uint8_t delimiter, int idx, uint8_t *output);

#endif /* SIM7600E_SRC_INCLUDE_SIMCOM_UTILS_H_ */
