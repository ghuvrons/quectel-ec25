/*
 * net.h
 *
 *  Created on: Aug 2, 2022
 *      Author: janoko
 */

#ifndef QTEL_QUECTEL_EC25_SIMNET_H_
#define QTEL_QUECTEL_EC25_SIMNET_H_

#if QTEL_EN_FEATURE_NET

#define QTEL_NET_STATUS_OPEN             0x01
#define QTEL_NET_STATUS_OPENING          0x02
#define QTEL_NET_STATUS_AVAILABLE        0x04
#define QTEL_NET_STATUS_APN_WAS_SET      0x08
#define QTEL_NET_STATUS_GPRS_REGISTERED  0x10
#define QTEL_NET_STATUS_GPRS_ROAMING     0x20
#define QTEL_NET_STATUS_NTP_WAS_SYNCING  0x40
#define QTEL_NET_STATUS_NTP_WAS_SYNCED   0x80

#define QTEL_NET_EVENT_ON_OPENED           0x01
#define QTEL_NET_EVENT_ON_CLOSED           0x02
#define QTEL_NET_EVENT_ON_GPRS_REGISTERED  0x04
#define QTEL_NET_EVENT_ON_NTP_WAS_SYNCED   0x08


uint8_t QTEL_NET_CheckAsyncResponse(QTEL_HandlerTypeDef*);
void    QTEL_NET_HandleEvents(QTEL_HandlerTypeDef*);

QTEL_Status_t QTEL_NET_WaitOnline(QTEL_HandlerTypeDef*, uint32_t timeout);
void          QTEL_SetAPN(QTEL_HandlerTypeDef*, const char *APN, const char *user, const char *pass);

#if QTEL_EN_FEATURE_NTP
void QTEL_SetNTP(QTEL_HandlerTypeDef*, const char *server);
#endif /* QTEL_EN_FEATURE_NTP */

#endif /* QTEL_EN_FEATURE_NET */
#endif /* QTEL_QUECTEL_EC25_SIMNET_H_ */
