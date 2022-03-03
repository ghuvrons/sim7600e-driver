/*
 * conf.h
 *
 *  Created on: Dec 3, 2021
 *      Author: janoko
 */

#ifndef SIM5320E_INC_SIMCOM_CONF_H_
#define SIM5320E_INC_SIMCOM_CONF_H_

#ifndef SIM_DEBUG
#define SIM_DEBUG 1
#endif

#ifndef SIM_GetTick
#define SIM_GetTick() HAL_GetTick()
#endif
#ifndef SIM_Delay
#define SIM_Delay(ms) HAL_Delay(ms)
#endif


#ifndef SIM_EN_FEATURE_SOCKET
#define SIM_EN_FEATURE_SOCKET 0
#endif

#ifndef SIM_EN_FEATURE_MQTT
#define SIM_EN_FEATURE_MQTT 0
#endif

#ifndef SIM_NUM_OF_SOCKET
#define SIM_NUM_OF_SOCKET  4
#endif

#ifndef SIM_DEBUG
#define SIM_DEBUG 1
#endif

#ifndef SIM_CMD_BUFFER_SIZE
#define SIM_CMD_BUFFER_SIZE  256
#endif

#ifndef SIM_RESP_BUFFER_SIZE
#define SIM_RESP_BUFFER_SIZE  256
#endif

#endif /* SIM5320E_SRC_INCLUDE_SIMCOM_CONF_H_ */
