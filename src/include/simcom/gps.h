/*
 * gps.h
 *
 *  Created on: May 18, 2022
 *      Author: janoko
 */

#ifndef SIM7600E_INC_GPS_H
#define SIM7600E_INC_GPS_H_

#include "stm32f4xx_hal.h"
#include "../simcom.h"
#include "conf.h"

#if SIM_EN_FEATURE_GPS
#include "lwgps/lwgps.h"

#define SIM_GPS_STATUS_ACTIVE 0x01

#define SIM_GPS_STATE_NMEA_AVAILABLE 0x01

#define SIM_GPS_RPT_GPGGA 0x0001
#define SIM_GPS_RPT_GPRMC 0x0002
#define SIM_GPS_RPT_GPGSV 0x0004
#define SIM_GPS_RPT_GPGSA 0x0008
#define SIM_GPS_RPT_GPVTG 0x0010
#define SIM_GPS_RPT_PQXFI 0x0020
#define SIM_GPS_RPT_GLGSV 0x0040
#define SIM_GPS_RPT_GNGSA 0x0080
#define SIM_GPS_RPT_GNGNS 0x0100


typedef enum {
  SIM_GPS_MODE_STANDALONE = 1,
  SIM_GPS_MODE_UE_BASED,
  SIM_GPS_MODE_UE_ASISTED,
} SIM_GPS_Mode_t;

typedef enum {
  SIM_GPS_MEARATE_1HZ,
  SIM_GPS_NMEARATE_10HZ,
} SIM_GPS_NMEARate_t;

typedef enum {
  SIM_GPS_METHOD_CONTROL_PLANE,
  SIM_GPS_METHOD_USER_PLANE,
} SIM_GPS_MOAGPS_Method_t;

typedef enum {
  SIM_GPS_ANT_PASSIVE,
  SIM_GPS_ANT_ACTIVE,
} SIM_GPS_ANT_Mode_t;


uint8_t SIM_GPS_CheckAsyncResponse(SIM_HandlerTypeDef*);
void    SIM_GPS_HandleEvents(SIM_HandlerTypeDef*);

void SIM_GPS_Init(SIM_HandlerTypeDef*, uint8_t *buffer, uint16_t bufferSize);
SIM_Status_t SIM_GPS_DefaultSetup(SIM_HandlerTypeDef*);
SIM_Status_t SIM_GPS_Activate(SIM_HandlerTypeDef*, SIM_GPS_Mode_t);
SIM_Status_t SIM_GPS_Deactivate(SIM_HandlerTypeDef*);
SIM_Status_t SIM_GPS_SetAccuracy(SIM_HandlerTypeDef*, uint16_t meter);
SIM_Status_t SIM_GPS_SetOutputRateNMEA(SIM_HandlerTypeDef*, SIM_GPS_NMEARate_t);
SIM_Status_t SIM_GPS_AutoDownloadXTRA(SIM_HandlerTypeDef*, uint8_t isEnable);
SIM_Status_t SIM_GPS_SetReportNMEA(SIM_HandlerTypeDef*,  uint8_t interval, uint16_t reportEn);
SIM_Status_t SIM_GPS_SetMOAGPSMethod(SIM_HandlerTypeDef*, SIM_GPS_MOAGPS_Method_t);
SIM_Status_t SIM_GPS_SetAGPSServer(SIM_HandlerTypeDef*, const char* url, uint8_t isSecure);
SIM_Status_t SIM_GPS_SetAntenna(SIM_HandlerTypeDef*, SIM_GPS_ANT_Mode_t);
SIM_Status_t SIM_GPS_SetAutoSwitchMode(SIM_HandlerTypeDef*, uint8_t isAuto);

#endif /* SIM_EN_FEATURE_GPS */
#endif /* SIM7600E_INC_GPS_H_ */
