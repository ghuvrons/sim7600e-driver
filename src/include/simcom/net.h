/*
 * net.h
 *
 *  Created on: Apr 1, 2022
 *      Author: janoko
 */

#ifndef SIM5320E_INC_SIMNET_H_
#define SIM5320E_INC_SIMNET_H_

#if SIM_EN_FEATURE_NET

#define SIM_NET_STATUS_OPEN             0x01
#define SIM_NET_STATUS_OPENING          0x02
#define SIM_NET_STATUS_AVAILABLE        0x04
#define SIM_NET_STATUS_SOCK_OPENING     0x08
#define SIM_NET_STATUS_APN_WAS_SET      0x10
#define SIM_NET_STATUS_GPRS_REGISTERED  0x20

#define SIM_NET_EVENT_ON_OPENED 0x01
#define SIM_NET_EVENT_ON_CLOSED 0x02


uint8_t SIM_NetCheckAsyncResponse(SIM_HandlerTypeDef*);
void    SIM_NetHandleEvents(SIM_HandlerTypeDef*);
void    SIM_NetOpen(SIM_HandlerTypeDef*);

#endif /* SIM_EN_FEATURE_NET */
#endif /* SIM5320E_INC_SIMNET_H_ */
