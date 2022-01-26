/*
 * mqtt.h
 *
 *  Created on: Jan 26, 2022
 *      Author: janoko
 */

#ifndef SIM5320E_INC_SIMCOM_MQTT_H_
#define SIM5320E_INC_SIMCOM_MQTT_H_

#include "conf.h"
#if SIM_EN_FEATURE_MQTT

#include "../simcom.h"

#define SIM_MQTT_STATE_STOP           0
#define SIM_MQTT_STATE_STARTING       1
#define SIM_MQTT_STATE_STARTED        2
#define SIM_MQTT_STATE_CONNECTING     3
#define SIM_MQTT_STATE_CONNECTED      4
#define SIM_MQTT_STATE_DISCONNECTING  5
#define SIM_MQTT_STATE_STOPING        6

#define SIM_MQTT_IS_STATE(mqtt_c, stat)    ((mqtt_c)->state == stat)
#define SIM_MQTT_SET_STATE(mqtt_c, stat)   ((mqtt_c)->state = stat)

typedef struct {
  uint8_t idx;
  char clientId[64];
  uint8_t state;
  uint8_t ssl_ctx;
  SIM_HandlerTypeDef* hsim;

  char *host;
  uint16_t port;
  char *username;
  char *password;

  struct {
    uint16_t keepalive;
  } config;
} SIM_MQTT_Client_t;

typedef struct {
  uint8_t QoS;
  const char *topic;
  const uint8_t *payload;
  uint16_t payloadLength;
} SIM_MQTT_Message_t;

uint8_t SIM_MqttCheckAsyncResponse(SIM_HandlerTypeDef*);
void    SIM_MqttHandleEvents(SIM_HandlerTypeDef*);

void SIM_MQTT_Init(SIM_MQTT_Client_t*, SIM_MQTT_Message_t *willMessage);
void SIM_MQTT_DeInit(SIM_MQTT_Client_t*);
void SIM_MQTT_ReInit(SIM_MQTT_Client_t*);
void SIM_MQTT_Connect(SIM_MQTT_Client_t*, uint8_t isCleanSession);
void SIM_MQTT_Disconnect(SIM_MQTT_Client_t*, uint8_t timeout);
uint8_t SIM_MQTT_Publish(SIM_MQTT_Client_t*,
                         SIM_MQTT_Message_t*,
                         uint8_t isRetained,
                         uint8_t isDup,
                         uint8_t timeout);
uint8_t SIM_MQTT_Subscribe(SIM_MQTT_Client_t*,
                           uint8_t QoS,
                           const char *topic,
                           uint8_t isDup);
uint8_t SIM_MQTT_Unsubscribe(SIM_MQTT_Client_t*,
                             const char *topic,
                             uint8_t isDup);

#endif /* SIM_EN_FEATURE_MQTT */
#endif /* SIM5320E_INC_SIMCOM_MQTT_H_ */
