/*
 * conf.h
 *
 *  Created on: Dec 3, 2021
 *      Author: janoko
 */

#ifndef SIM7600E_INC_SIMCOM_CONF_H_
#define SIM7600E_INC_SIMCOM_CONF_H_

#ifndef SIM_DEBUG
#define SIM_DEBUG 1
#endif


#ifndef SIM_EN_FEATURE_SOCKET
#define SIM_EN_FEATURE_SOCKET 0
#endif

#ifndef SIM_EN_FEATURE_NTP
#define SIM_EN_FEATURE_NTP 1
#endif

#define SIM_EN_FEATURE_NET SIM_EN_FEATURE_NTP|SIM_EN_FEATURE_SOCKET

#ifndef SIM_EN_FEATURE_GPS
#define SIM_EN_FEATURE_GPS 1
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

#if SIM_EN_FEATURE_NTP
#ifndef SIM_NTP_SYNC_DELAY_TIMEOUT
#define SIM_NTP_SYNC_DELAY_TIMEOUT 10000
#endif
#endif

#ifndef LWGPS_IGNORE_USER_OPTS
#define LWGPS_IGNORE_USER_OPTS
#endif

#if SIM_EN_FEATURE_GPS
#ifndef SIM_GPS_TMP_BUF_SIZE
#define SIM_GPS_TMP_BUF_SIZE  64
#endif
#endif /* SIM_EN_FEATURE_GPS */

#endif /* SIM7600E_SRC_INCLUDE_SIMCOM_CONF_H_ */
