/*
 * net.c
 *
 *  Created on: Apr 1, 2022
 *      Author: janoko
 */


#include "../include/simcom.h"
#include "../include/simcom/net.h"
#include "../include/simcom/utils.h"
#include "../include/simcom/debug.h"
#include <stdlib.h>

#if SIM_EN_FEATURE_NET

static void GprsSetAPN(SIM_HandlerTypeDef *hsim,
                       const char *APN, const char *user, const char *pass);
static uint8_t GprsCheck(SIM_HandlerTypeDef *hsim);

uint8_t SIM_NetCheckAsyncResponse(SIM_HandlerTypeDef *hsim)
{
  uint8_t isGet = 0;

  if ((isGet = (hsim->respBufferLen >= 11 && SIM_IsResponse(hsim, "+NETOPEN", 8)))) {
    SIM_NET_UNSET_STATUS(hsim, SIM_NET_STATUS_OPENING);
    if (hsim->respBuffer[10] == '0') {
      SIM_NET_SET_STATUS(hsim, SIM_NET_STATUS_OPEN);
      SIM_BITS_SET(hsim->net.events, SIM_NET_EVENT_ON_OPENED);
    }
  }

  else if ((isGet = SIM_IsResponse(hsim, "+CIPEVENT", 9))) {
    if (strncmp((const char *)&(hsim->respBuffer[11]), "NETWORK CLOSED", 14)) {
      SIM_NET_UNSET_STATUS(hsim, SIM_NET_STATUS_OPEN|SIM_NET_STATUS_OPENING);
      SIM_BITS_SET(hsim->net.events, SIM_NET_EVENT_ON_CLOSED);
    }
  }

  return isGet;
}


void SIM_NetHandleEvents(SIM_HandlerTypeDef *hsim)
{
  if (!SIM_NET_IS_STATUS(hsim, SIM_NET_STATUS_APN_WAS_SET)
      && SIM_IS_STATUS(hsim, SIM_STATUS_REGISTERED)
  ){
    GprsSetAPN(hsim, "indosatgprs", "indosat", "indosat");
  }

  if (!SIM_NET_IS_STATUS(hsim, SIM_NET_STATUS_GPRS_REGISTERED)
      && SIM_IS_STATUS(hsim, SIM_STATUS_REGISTERED)
  ){
    GprsCheck(hsim);
  }

  if (!SIM_NET_IS_STATUS(hsim, SIM_NET_STATUS_OPEN)
      && !SIM_NET_IS_STATUS(hsim, SIM_NET_STATUS_OPENING)
      && SIM_IS_STATUS(hsim, SIM_STATUS_REGISTERED)
  ){
    SIM_NetOpen(hsim);
  }

#if SIM_EN_FEATURE_NTP
  if (!SIM_NET_IS_STATUS(hsim, SIM_NET_STATUS_NTP_WAS_SET)
      && SIM_NET_IS_STATUS(hsim, SIM_NET_STATUS_GPRS_REGISTERED)
      && SIM_NET_IS_STATUS(hsim, SIM_NET_STATUS_APN_WAS_SET)
  ){
    SIM_SetupNTP(hsim, "time.google.com", 28);
  }

  if (!SIM_NET_IS_STATUS(hsim, SIM_NET_STATUS_NTP_WAS_SYNCED)
      && SIM_NET_IS_STATUS(hsim, SIM_NET_STATUS_NTP_WAS_SET)
  ){
    if (SIM_IsTimeout(hsim->net.ntpSyncTick, SIM_NTP_SYNC_DELAY_TIMEOUT)) {
      SIM_SyncNTP(hsim);
    }
  }
#endif /* SIM_EN_FEATURE_NTP */

  if (SIM_BITS_IS(hsim->net.events, SIM_NET_EVENT_ON_GPRS_REGISTERED)) {
    SIM_BITS_UNSET(hsim->net.events, SIM_NET_EVENT_ON_GPRS_REGISTERED);
  }
}


void SIM_NetOpen(SIM_HandlerTypeDef *hsim)
{
  uint8_t resp;

  if (SIM_NET_IS_STATUS(hsim, SIM_NET_STATUS_OPENING)) return;

  SIM_LockCMD(hsim);

  // check net state
  SIM_SendCMD(hsim, "AT+NETOPEN?");
  if (SIM_GetResponse(hsim, "+NETOPEN", 8, &resp, 1, SIM_GETRESP_WAIT_OK, 1000) == SIM_OK) {
    if (resp == '1') { // net already open;
      SIM_NET_SET_STATUS(hsim, SIM_NET_STATUS_OPEN);
      SIM_BITS_SET(hsim->net.events, SIM_NET_EVENT_ON_OPENED);
      goto endCMD;
    }
  }
  SIM_SendCMD(hsim, "AT+NETOPEN");
  SIM_NET_SET_STATUS(hsim, SIM_NET_STATUS_OPENING);
  if (SIM_IsResponseOK(hsim)) {
    goto endCMD;
  }
  SIM_NET_UNSET_STATUS(hsim, SIM_NET_STATUS_OPENING);
  endCMD:
  SIM_UnlockCMD(hsim);
}


#if SIM_EN_FEATURE_NTP
void SIM_SetupNTP(SIM_HandlerTypeDef *hsim, const char *host, int8_t region)
{
  SIM_LockCMD(hsim);

  SIM_SendCMD(hsim, "AT+CNTP=\"%s\",%d", host, (int) region);
  if (!SIM_IsResponseOK(hsim)) {
    goto endcmd;
  }

  SIM_NET_SET_STATUS(hsim, SIM_NET_STATUS_NTP_WAS_SET);

  endcmd:
  SIM_UnlockCMD(hsim);
  SIM_SyncNTP(hsim);
}


uint8_t SIM_SyncNTP(SIM_HandlerTypeDef *hsim)
{
  uint8_t resp[5];
  uint8_t status;
  uint8_t isOk = 0;

  hsim->net.ntpSyncTick = SIM_GetTick();

  if (!SIM_NET_IS_STATUS(hsim, SIM_NET_STATUS_NTP_WAS_SET)) return isOk;

  memset(resp, 0, 5);
  SIM_LockCMD(hsim);

  SIM_SendCMD(hsim, "AT+CNTP");
  if (!SIM_IsResponseOK(hsim)) {
    goto endcmd;
  }

  if (SIM_GetResponse(hsim, "+CNTP", 5, &resp[0], 5, SIM_GETRESP_ONLY_DATA, 5000) != SIM_OK) {
    goto endcmd;
  }

  status = (uint8_t) atoi((char*)&resp[0]);
  if (status != 0) {
    SIM_Debug("[ntp] error - %d", status);
    goto endcmd;
  }

  SIM_NET_SET_STATUS(hsim, SIM_NET_STATUS_NTP_WAS_SYNCED);
  isOk = 1;
  endcmd:
  SIM_UnlockCMD(hsim);
  return isOk;
}
#endif /* SIM_EN_FEATURE_NTP */


static void GprsSetAPN(SIM_HandlerTypeDef *hsim,
                       const char *APN, const char *user, const char *pass)
{
  SIM_LockCMD(hsim);

  // check net state
  SIM_SendCMD(hsim, "AT+CGDCONT=1,\"IP\",\"%s\"", APN);
  if (!SIM_IsResponseOK(hsim)) {
    goto endcmd;
  }

  SIM_SendCMD(hsim, "AT+CGAUTH=1,1,\"%s\",\"%s\"", user, pass);
  if (!SIM_IsResponseOK(hsim)) {
    goto endcmd;
  }

  SIM_NET_SET_STATUS(hsim, SIM_NET_STATUS_APN_WAS_SET);
  endcmd:
  SIM_UnlockCMD(hsim);
}


static uint8_t GprsCheck(SIM_HandlerTypeDef *hsim)
{
  uint8_t resp[16];
  // uint8_t resp_n = 0;
  uint8_t resp_stat = 0;
  uint8_t isOK = 0;

  // send command then get response;
  SIM_LockCMD(hsim);

  memset(resp, 0, 16);
  SIM_SendCMD(hsim, "AT+CGREG?");
  if (SIM_GetResponse(hsim, "+CGREG", 5, &resp[0], 3, SIM_GETRESP_WAIT_OK, 2000) == SIM_OK) {
    // resp_n = (uint8_t) atoi((char*)&resp[0]);
    resp_stat = (uint8_t) atoi((char*)&resp[2]);
  }
  else goto endcmd;

  // check response
  if (resp_stat == 1 || resp_stat == 5) {
    SIM_NET_SET_STATUS(hsim, SIM_NET_STATUS_GPRS_REGISTERED);
    SIM_BITS_SET(hsim->net.events, SIM_NET_EVENT_ON_GPRS_REGISTERED);
    SIM_Debug("GPRS Registered%s.", (resp_stat == 5)? " (Roaming)":"");
    isOK = 1;
  } else {
    SIM_NET_UNSET_STATUS(hsim, SIM_NET_STATUS_GPRS_REGISTERED);
  }

  endcmd:
  SIM_UnlockCMD(hsim);
  return isOK;
}


#endif /* SIM_EN_FEATURE_NET */
