/*
 * debug.h
 *
 *  Created on: Feb 2, 2022
 *      Author: janoko
 */


#ifndef SIM5320E_INC_SIMCOM_DEBUG_H_
#define SIM5320E_INC_SIMCOM_DEBUG_H_

#include "conf.h"
#if SIM_DEBUG

#include <stdio.h>


#define SIM_Debug(...) {SIM_Printf("SIM: ");SIM_Println(__VA_ARGS__);}

void SIM_Printf(const char *format, ...);
void SIM_Println(const char *format, ...);

#else

#define SIM_Debug(...) {}
#endif /* SIM_DEBUG */
#endif /* SIM5320E_INC_SIMCOM_DEBUG_H_ */
