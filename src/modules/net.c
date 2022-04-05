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
  if (resp_stat == 1) {
    SIM_NET_SET_STATUS(hsim, SIM_NET_STATUS_GPRS_REGISTERED);
    SIM_Debug("GPRS Registered");
    isOK = 1;
  } else {
    SIM_NET_UNSET_STATUS(hsim, SIM_NET_STATUS_GPRS_REGISTERED);
  }

  endcmd:
  SIM_UnlockCMD(hsim);
  return isOK;
}


#endif /* SIM_EN_FEATURE_NET */
