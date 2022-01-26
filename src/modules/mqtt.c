/*
 * mqtt.c
 *
 *  Created on: Jan 26, 2022
 *      Author: janoko
 */

#include "../include/simcom.h"
#include "../include/simcom/utils.h"
#include "../include/simcom/mqtt.h"
#include <string.h>

#if SIM_EN_FEATURE_MQTT

static void mqttStart(SIM_MQTT_Client_t*);
static void mqttStop(SIM_MQTT_Client_t*);
static uint8_t mqttRelease(SIM_MQTT_Client_t*);
// TODO : static void mqttSetSSL(SIM_MQTT_Client_t*, uint8_t ssl_ctx);
static uint8_t mqttSetWillMessage(SIM_MQTT_Client_t*, SIM_MQTT_Message_t*);
// TODO : static void mqttCheck(SIM_MQTT_Client_t*);
static uint8_t mqttAcquire(SIM_MQTT_Client_t*, const char *clientID);

uint8_t SIM_MqttCheckAsyncResponse(SIM_HandlerTypeDef *hsim)
{
  return 0;
}

void SIM_MqttHandleEvents(SIM_HandlerTypeDef *hsim)
{

}

void SIM_MQTT_Init(SIM_MQTT_Client_t *mqtt_c, SIM_MQTT_Message_t *willMessage)
{
  mqttStart(mqtt_c);
  mqttAcquire(mqtt_c, mqtt_c->clientId);

  if (willMessage != NULL) mqttSetWillMessage(mqtt_c, willMessage);
}

void SIM_MQTT_DeInit(SIM_MQTT_Client_t *mqtt_c)
{
  if (mqttRelease(mqtt_c)) {
    mqttStop(mqtt_c);
  }
}


/* AT COMMAND MQTT */

static void mqttStart(SIM_MQTT_Client_t *mqtt_c)
{
  if (SIM_MQTT_IS_STATE(mqtt_c, SIM_MQTT_STATE_STARTING)) return;

  SIM_LockCMD(mqtt_c->hsim);
  SIM_SendCMD(mqtt_c->hsim, "AT+CMQTTSTART");
  SIM_MQTT_SET_STATE(mqtt_c, SIM_MQTT_STATE_STARTING);
  if (!SIM_IsResponseOK(mqtt_c->hsim)) {
    SIM_MQTT_SET_STATE(mqtt_c, SIM_MQTT_STATE_STOP);
  }
  SIM_UnlockCMD(mqtt_c->hsim);
}

static void mqttStop(SIM_MQTT_Client_t *mqtt_c)
{
  uint8_t prevState = mqtt_c->state;

  SIM_LockCMD(mqtt_c->hsim);
  SIM_SendCMD(mqtt_c->hsim, "AT+CMQTTSTOP");
  SIM_MQTT_SET_STATE(mqtt_c, SIM_MQTT_STATE_STOPING);
  if (!SIM_IsResponseOK(mqtt_c->hsim)) {
    SIM_MQTT_SET_STATE(mqtt_c, prevState);
  }
  SIM_UnlockCMD(mqtt_c->hsim);
}

static uint8_t mqttRelease(SIM_MQTT_Client_t *mqtt_c)
{
  uint8_t resp = 0;

  SIM_LockCMD(mqtt_c->hsim);
  SIM_SendCMD(mqtt_c->hsim, "AT+CMQTTREL=%d", mqtt_c->idx);
  if (!SIM_IsResponseOK(mqtt_c->hsim)) {
    goto endCMD;
  }

  resp = 1;

  endCMD:
  SIM_UnlockCMD(mqtt_c->hsim);
  return resp;
}

static uint8_t mqttSetWillMessage(SIM_MQTT_Client_t *mqtt_c, SIM_MQTT_Message_t *willMessage)
{
  uint8_t resp = 0;
  uint16_t topicLen = strlen(willMessage->topic);

  SIM_LockCMD(mqtt_c->hsim);

  // set topic
  SIM_SendCMD(mqtt_c->hsim, "AT+CMQTTWILLTOPIC=%d,%d", mqtt_c->idx, topicLen);
  if (!SIM_WaitResponse(mqtt_c->hsim, "\r\n>", 3, 3000))
    goto endCMD;

  SIM_SendData(mqtt_c->hsim, (const uint8_t*) willMessage->topic, topicLen);
  if (!SIM_IsResponseOK(mqtt_c->hsim))
    goto endCMD;

  // set message
  SIM_SendCMD(mqtt_c->hsim, "AT+CMQTTWILLMSG=%d,%d,%d",
              mqtt_c->idx, willMessage->payloadLength, willMessage->QoS);
  if (!SIM_WaitResponse(mqtt_c->hsim, "\r\n>", 3, 3000))
    goto endCMD;

  SIM_SendData(mqtt_c->hsim, willMessage->payload, willMessage->payloadLength);
  if (!SIM_IsResponseOK(mqtt_c->hsim))
    goto endCMD;
  resp = 1;

  endCMD:
  SIM_UnlockCMD(mqtt_c->hsim);
  return resp;
}

static uint8_t mqttAcquire(SIM_MQTT_Client_t *mqtt_c, const char *clientID)
{
  // TODO : what if ssl mqtt
  uint8_t resp = 0;

  SIM_LockCMD(mqtt_c->hsim);
  SIM_SendCMD(mqtt_c->hsim, "AT+CMQTTACCQ=%d,%s", mqtt_c->idx, clientID);
  if (!SIM_IsResponseOK(mqtt_c->hsim))
    goto endCMD;

  resp = 1;

  endCMD:
  SIM_UnlockCMD(mqtt_c->hsim);
  return resp;
}

void SIM_MQTT_Connect(SIM_MQTT_Client_t *mqtt_c, uint8_t isCleanSession)
{
  uint8_t prevState = mqtt_c->state;

  SIM_LockCMD(mqtt_c->hsim);
  SIM_SendCMD(mqtt_c->hsim, "AT+CMQTTCONNECT=%d,%s:%d,%d,%d,%s,%s",
              mqtt_c->idx, mqtt_c->host, mqtt_c->port, mqtt_c->config.keepalive, isCleanSession,
              mqtt_c->username, mqtt_c->password);

  SIM_MQTT_SET_STATE(mqtt_c, SIM_MQTT_STATE_CONNECTING);
  if (!SIM_IsResponseOK(mqtt_c->hsim)){
    SIM_MQTT_SET_STATE(mqtt_c, prevState);
  }
  SIM_UnlockCMD(mqtt_c->hsim);
}

void SIM_MQTT_Disconnect(SIM_MQTT_Client_t *mqtt_c, uint8_t timeout)
{
  uint8_t prevState = mqtt_c->state;

  SIM_LockCMD(mqtt_c->hsim);
  SIM_SendCMD(mqtt_c->hsim, "AT+CMQTTDISC=%d,%d",
              mqtt_c->idx, timeout);

  SIM_MQTT_SET_STATE(mqtt_c, SIM_MQTT_STATE_DISCONNECTING);
  if (!SIM_IsResponseOK(mqtt_c->hsim)){
    SIM_MQTT_SET_STATE(mqtt_c, prevState);
  }
  SIM_UnlockCMD(mqtt_c->hsim);
}

uint8_t SIM_MQTT_Publish(SIM_MQTT_Client_t *mqtt_c,
                         SIM_MQTT_Message_t *mqttMesage,
                         uint8_t isRetained,
                         uint8_t isDup,
                         uint8_t timeout)
{
  uint8_t resp = 0;
  uint16_t topicLen = strlen(mqttMesage->topic);

  SIM_LockCMD(mqtt_c->hsim);

  // set topic
  SIM_SendCMD(mqtt_c->hsim, "AT+CMQTTTOPIC=%d,%d", mqtt_c->idx, topicLen);
  if (!SIM_WaitResponse(mqtt_c->hsim, "\r\n>", 3, 3000))
    goto endCMD;

  SIM_SendData(mqtt_c->hsim, (const uint8_t*) mqttMesage->topic, topicLen);
  if (!SIM_IsResponseOK(mqtt_c->hsim))
    goto endCMD;

  // set message
  SIM_SendCMD(mqtt_c->hsim, "AT+CMQTTPAYLOAD=%d,%d", mqtt_c->idx, mqttMesage->payloadLength);
  if (!SIM_WaitResponse(mqtt_c->hsim, "\r\n>", 3, 3000))
    goto endCMD;

  SIM_SendData(mqtt_c->hsim, mqttMesage->payload, mqttMesage->payloadLength);
  if (!SIM_IsResponseOK(mqtt_c->hsim))
    goto endCMD;

  // send
  SIM_LockCMD(mqtt_c->hsim);
  SIM_SendCMD(mqtt_c->hsim, "AT+CMQTTPUB=%d,%d,%d,%d,%d",
              mqtt_c->idx, mqttMesage->QoS, isRetained, isDup, timeout);

  if (!SIM_IsResponseOK(mqtt_c->hsim)){
    goto endCMD;
  }

  resp = 1;
  endCMD:
  SIM_UnlockCMD(mqtt_c->hsim);
  return resp;
}

uint8_t SIM_MQTT_Subscribe(SIM_MQTT_Client_t *mqtt_c,
                           uint8_t QoS,
                           const char *topic,
                           uint8_t isDup)
{
  uint8_t resp = 0;
  uint16_t topicLen = strlen(topic);

  // set topic
  SIM_SendCMD(mqtt_c->hsim, "AT+CMQTTSUBTOPIC=%d,%d,%d", mqtt_c->idx, topicLen, QoS);
  if (!SIM_WaitResponse(mqtt_c->hsim, "\r\n>", 3, 3000))
    goto endCMD;

  SIM_SendData(mqtt_c->hsim, (const uint8_t*) topic, topicLen);
  if (!SIM_IsResponseOK(mqtt_c->hsim))
    goto endCMD;

  // send
  SIM_LockCMD(mqtt_c->hsim);
  SIM_SendCMD(mqtt_c->hsim, "AT+CMQTTSUB=%d,%d",
              mqtt_c->idx, isDup);

  if (!SIM_IsResponseOK(mqtt_c->hsim)){
    goto endCMD;
  }

  resp = 1;
  endCMD:
  SIM_UnlockCMD(mqtt_c->hsim);
  return resp;
}


uint8_t SIM_MQTT_Unsubscribe(SIM_MQTT_Client_t *mqtt_c,
                                    const char *topic,
                                    uint8_t isDup)
{
  uint8_t resp = 0;
  uint16_t topicLen = strlen(topic);

  // set topic
  SIM_SendCMD(mqtt_c->hsim, "AT+CMQTTUNSUBTOPIC=%d,%d", mqtt_c->idx, topicLen);
  if (!SIM_WaitResponse(mqtt_c->hsim, "\r\n>", 3, 3000))
    goto endCMD;

  SIM_SendData(mqtt_c->hsim, (const uint8_t*) topic, topicLen);
  if (!SIM_IsResponseOK(mqtt_c->hsim))
    goto endCMD;

  // send
  SIM_LockCMD(mqtt_c->hsim);
  SIM_SendCMD(mqtt_c->hsim, "AT+CMQTTUNSUB=%d,%d",
              mqtt_c->idx, isDup);

  if (!SIM_IsResponseOK(mqtt_c->hsim)){
    goto endCMD;
  }

  resp = 1;
  endCMD:
  SIM_UnlockCMD(mqtt_c->hsim);
  return resp;
}

#endif /* SIM_EN_FEATURE_MQTT */
