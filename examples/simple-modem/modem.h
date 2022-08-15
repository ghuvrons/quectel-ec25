/*
 * modem.h
 *
 *  Created on: Dec 15, 2021
 *      Author: janoko
 */

#ifndef QTEL_QUECTEL_EC25_SIMPLE_MODEM_H_
#define QTEL_QUECTEL_EC25_SIMPLE_MODEM_H_

#include <_defs/general.h>
#include <_defs/types.h>
#include <quectel.h>

#define MDM_TX_BUFFER_SZ 128
#define MDM_RX_BUFFER_SZ 1408
#define MDM_GPS_BUFFER_SZ 1024

typedef struct {
  uint32_t tick;
  uint32_t timeout;
} MDM_DelayConfig;

typedef struct {
  QTEL_HandlerTypeDef driver;
  struct {
    MDM_DelayConfig signalUpdate;
  } delayConfigs;
} MDM_HandlerTypedef;

extern MDM_HandlerTypedef Mod_Modem;

bool        MDM_Init(void);
void        MDM_Run(void *args);
void        MDM_Reset(void);
bool        MDM_WaitReady(uint32_t timeout);

#if QTEL_EN_FEATURE_GPS
bool MDM_IsGPSWeakSignal(void);
#endif

#endif /* QTEL_QUECTEL_EC25_SIMPLE_MODEM_H */
