/*
 * config.h
 *
 *  Created on: Aug 15, 2022
 *      Author: janoko
 */

#ifndef QTEL_QUECTEL_EC25_SIMPLE_MODEM_CONF_H_
#define QTEL_QUECTEL_EC25_SIMPLE_MODEM_CONF_H_

#define QTEL_EN_FEATURE_NTP    1
#define QTEL_EN_FEATURE_SOCKET 1
#define QTEL_EN_FEATURE_HTTP   1
#define QTEL_EN_FEATURE_GPS    1

#if USE_FREERTOS
#define QTEL_GetTick() osKernelGetTickCount()
#define QTEL_Delay(ms) osDelay(ms)
#endif

#include <quectel/conf.h>
#endif /* QTEL_QUECTEL_EC25_SIMPLE_MODEM_CONF_H_ */
