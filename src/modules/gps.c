/*
 * net.c
 *
 *  Created on: Apr 1, 2022
 *      Author: janoko
 */


#include "../include/simcom.h"
#include "../include/simcom/net.h"
#include "../include/simcom/gps.h"
#include "../include/simcom/utils.h"
#include "../include/simcom/debug.h"
#include <stdlib.h>

#if SIM_EN_FEATURE_GPS

static void gpsProcessBuffer(SIM_HandlerTypeDef*);


uint8_t SIM_GPS_CheckAsyncResponse(SIM_HandlerTypeDef *hsim)
{
  uint8_t isGet = 0;

  if ((isGet = (hsim->respBufferLen >= 6 && SIM_IsResponse(hsim, "$", 1)))) {
    SIM_BITS_SET(hsim->gps.events, SIM_GPS_STATE_NMEA_AVAILABLE);
    Buffer_Write(&hsim->gps.buffer, hsim->respBuffer, hsim->respBufferLen);
  }

  return isGet;
}


void SIM_GPS_HandleEvents(SIM_HandlerTypeDef *hsim)
{
  if (SIM_IS_STATUS(hsim, SIM_STATUS_ACTIVE) 
      && !SIM_GPS_IS_STATUS(hsim, SIM_GPS_STATUS_ACTIVE)
  ) {
    if (SIM_GPS_Deactivate(hsim) != SIM_OK) {
      return;
    }
    if (SIM_GPS_DefaultSetup(hsim) == SIM_OK) {
      if (SIM_GPS_Activate(hsim, SIM_GPS_MODE_UE_BASED) == SIM_OK) {
        SIM_GPS_SET_STATUS(hsim, SIM_GPS_STATUS_ACTIVE);
      }
    }
  }

  if (SIM_BITS_IS(hsim->gps.events, SIM_GPS_STATE_NMEA_AVAILABLE)) {
    SIM_BITS_UNSET(hsim->gps.events, SIM_GPS_STATE_NMEA_AVAILABLE);
    gpsProcessBuffer(hsim);
  }
}


void SIM_GPS_Init(SIM_HandlerTypeDef *hsim, uint8_t *buffer, uint16_t bufferSize)
{
  memset(&hsim->gps.buffer, 0, sizeof(Buffer_t));
  hsim->gps.buffer.buffer = buffer;
  hsim->gps.buffer.size = bufferSize;
  lwgps_init(&hsim->gps.lwgps);
}


SIM_Status_t SIM_GPS_DefaultSetup(SIM_HandlerTypeDef *hsim)
{
  SIM_Status_t status = SIM_ERROR;

  if (SIM_GPS_SetAccuracy(hsim, 50) != SIM_OK)
    goto endcmd;

  if (SIM_GPS_SetOutputRateNMEA(hsim, SIM_GPS_MEARATE_1HZ) != SIM_OK)
    goto endcmd;

  if (SIM_GPS_AutoDownloadXTRA(hsim, 1) != SIM_OK)
    goto endcmd;

  if (SIM_GPS_SetReportNMEA(
        hsim, 
        5, 
        SIM_GPS_RPT_GPGGA|SIM_GPS_RPT_GPRMC|SIM_GPS_RPT_GPGSV|SIM_GPS_RPT_GPGSA|SIM_GPS_RPT_GPVTG
    ) != SIM_OK)
    goto endcmd;

  if (SIM_GPS_SetMOAGPSMethod(hsim, SIM_GPS_METHOD_USER_PLANE) != SIM_OK)
    goto endcmd;

  if (SIM_GPS_SetAGPSServer(hsim, "supl.google.com:7276", 0) != SIM_OK)
    goto endcmd;

  if (SIM_GPS_SetAntenna(hsim, SIM_GPS_ANT_ACTIVE) != SIM_OK)
    goto endcmd;

  status = SIM_OK;
  endcmd:
  return status;
}


SIM_Status_t SIM_GPS_Activate(SIM_HandlerTypeDef *hsim, SIM_GPS_Mode_t mode)
{
  SIM_Status_t status = SIM_ERROR;

  hsim->mutexLock(hsim);
  SIM_SendCMD(hsim, "AT+CGPS=1,%d", mode);
  if (SIM_IsResponseOK(hsim))
    status = SIM_OK;

  hsim->mutexUnlock(hsim);
  return status;

  return SIM_OK;
}


SIM_Status_t SIM_GPS_Deactivate(SIM_HandlerTypeDef *hsim)
{
  SIM_Status_t status = SIM_ERROR;
  uint8_t resp;

  hsim->mutexLock(hsim);

  SIM_SendCMD(hsim, "AT+CGPS?");
  if (SIM_GetResponse(hsim, "+CGPS", 5, &resp, 1, SIM_GETRESP_WAIT_OK, 1000) == SIM_OK) {
    if (resp == '1') {
      SIM_SendCMD(hsim, "AT+CGPS=0");
      if (!SIM_IsResponseOK(hsim)) {
        goto endcmd;
      }
    }
  }

  status = SIM_OK;
  SIM_GPS_UNSET_STATUS(hsim, SIM_GPS_STATUS_ACTIVE);
  endcmd:
  hsim->mutexUnlock(hsim);
  return status;
}


SIM_Status_t SIM_GPS_SetAccuracy(SIM_HandlerTypeDef *hsim, uint16_t meter)
{
  SIM_Status_t status = SIM_ERROR;

  hsim->mutexLock(hsim);
  SIM_SendCMD(hsim, "AT+CGPSHOR=%d", (int) meter);
  if (SIM_IsResponseOK(hsim))
    status = SIM_OK;

  hsim->mutexUnlock(hsim);
  return status;
}

SIM_Status_t SIM_GPS_SetOutputRateNMEA(SIM_HandlerTypeDef *hsim, SIM_GPS_NMEARate_t rate)
{
  SIM_Status_t status = SIM_ERROR;

  hsim->mutexLock(hsim);
  SIM_SendCMD(hsim, "AT+CGPSNMEARATE=%d", rate);
  if (SIM_IsResponseOK(hsim))
    status = SIM_OK;

  hsim->mutexUnlock(hsim);
  return status;
}


SIM_Status_t SIM_GPS_AutoDownloadXTRA(SIM_HandlerTypeDef *hsim, uint8_t isEnable)
{
  SIM_Status_t status = SIM_ERROR;

  hsim->mutexLock(hsim);
  SIM_SendCMD(hsim, "AT+CGPSXDAUTO=%d", (isEnable)?1:0);
  if (SIM_IsResponseOK(hsim))
    status = SIM_OK;

  hsim->mutexUnlock(hsim);
  return status;
}


SIM_Status_t SIM_GPS_SetReportNMEA(SIM_HandlerTypeDef *hsim, uint8_t interval, uint16_t reportEn)
{
  SIM_Status_t status = SIM_ERROR;

  hsim->mutexLock(hsim);
  SIM_SendCMD(hsim, "AT+CGPSINFOCFG=%d,%d", interval, reportEn);
  if (SIM_IsResponseOK(hsim))
    status = SIM_OK;

  hsim->mutexUnlock(hsim);
  return status;
}


SIM_Status_t SIM_GPS_SetMOAGPSMethod(SIM_HandlerTypeDef *hsim, SIM_GPS_MOAGPS_Method_t method)
{
  SIM_Status_t status = SIM_ERROR;

  hsim->mutexLock(hsim);
  SIM_SendCMD(hsim, "AT+CGPSMD=%d", method);
  if (SIM_IsResponseOK(hsim))
    status = SIM_OK;

  hsim->mutexUnlock(hsim);
  return status;
}


SIM_Status_t SIM_GPS_SetAGPSServer(SIM_HandlerTypeDef *hsim, const char* url, uint8_t isSecure)
{
  SIM_Status_t status = SIM_ERROR;

  hsim->mutexLock(hsim);

  SIM_SendCMD(hsim, "AT+CGPSURL=\"%s\"", url);
  if (!SIM_IsResponseOK(hsim)) {
    goto endcmd;
  }

  SIM_SendCMD(hsim, "AT+CGPSSSL=%d", (isSecure)?1:0);
  if (!SIM_IsResponseOK(hsim)) {
    goto endcmd;
  }

  status = SIM_OK;
  endcmd:
  hsim->mutexUnlock(hsim);
  return status;
}


SIM_Status_t SIM_GPS_SetAntenna(SIM_HandlerTypeDef *hsim, SIM_GPS_ANT_Mode_t mode)
{
  SIM_Status_t status = SIM_ERROR;

  hsim->mutexLock(hsim);
  switch (mode)
  {
  case SIM_GPS_ANT_PASSIVE:
    SIM_SendCMD(hsim, "AT+CVAUXS=0");
    if (SIM_IsResponseOK(hsim))
      status = SIM_OK;
    break;

  case SIM_GPS_ANT_ACTIVE:
    SIM_SendCMD(hsim, "AT+CVAUXV=%d", 3050);
    if (!SIM_IsResponseOK(hsim)) {
      break;
    }
    SIM_SendCMD(hsim, "AT+CVAUXS=1");
    if (SIM_IsResponseOK(hsim))
      status = SIM_OK;
    break;

  default:
    status = SIM_ERROR;
    break;
  }

  hsim->mutexUnlock(hsim);
  return status;
}


SIM_Status_t SIM_GPS_SetAutoSwitchMode(SIM_HandlerTypeDef *hsim, uint8_t isAuto)
{
  SIM_Status_t status = SIM_ERROR;

  hsim->mutexLock(hsim);
  SIM_SendCMD(hsim, "AT+CGPSMSB=%d", (isAuto)?1:0);
  if (SIM_IsResponseOK(hsim))
    status = SIM_OK;

  hsim->mutexUnlock(hsim);
  return status;
}


static void gpsProcessBuffer(SIM_HandlerTypeDef *hsim)
{
  uint16_t readLen = 0;

  while (Buffer_IsAvailable(&hsim->gps.buffer)) {
    readLen = Buffer_Read(&hsim->gps.buffer, &hsim->gps.readBuffer[0], SIM_GPS_TMP_BUF_SIZE);
    lwgps_process(&hsim->gps.lwgps, &hsim->gps.readBuffer[0], readLen);
  }
}

#endif /* SIM_EN_FEATURE_GPS */
