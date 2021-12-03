/*
 * conf.h
 *
 *  Created on: Dec 3, 2021
 *      Author: janoko
 */

#ifndef SIM5320E_INC_SIMCOM_CONF_H_
#define SIM5320E_INC_SIMCOM_CONF_H_

#ifndef SIM_GetTick
#define SIM_GetTick() HAL_GetTick()
#endif
#ifndef SIM_Delay
#define SIM_Delay(ms) HAL_Delay(ms)
#endif

#ifndef STRM_GetTick
#define STRM_GetTick() SIM_GetTick()
#endif
#ifndef STRM_Delay
#define STRM_Delay(ms) SIM_Delay(ms)
#endif

#include <dma_streamer/conf.h>

#endif /* SIM5320E_SRC_INCLUDE_SIMCOM_CONF_H_ */
