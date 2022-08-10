/*
 * gps.h
 *
 *  Created on: Aug 2, 2022
 *      Author: janoko
 */

#ifndef QTEL_QUECTEL_EC25_GPS_H
#define QTEL_QUECTEL_EC25_GPS_H_

#include "../quectel.h"
#include "conf.h"

#if QTEL_EN_FEATURE_GPS
#include "lwgps/lwgps.h"

#define QTEL_GPS_STATUS_ACTIVE 0x01

#define QTEL_GPS_STATE_NMEA_AVAILABLE 0x01

#define QTEL_GPS_CFG_KEYS_NUM 16

#define QTEL_GPS_RPT_GPGGA 0x0001
#define QTEL_GPS_RPT_GPRMC 0x0002
#define QTEL_GPS_RPT_GPGSV 0x0004
#define QTEL_GPS_RPT_GPGSA 0x0008
#define QTEL_GPS_RPT_GPVTG 0x0010
#define QTEL_GPS_RPT_PQXFI 0x0020
#define QTEL_GPS_RPT_GLGSV 0x0040
#define QTEL_GPS_RPT_GNGSA 0x0080
#define QTEL_GPS_RPT_GNGNS 0x0100


typedef enum {
  QTEL_GPS_MODE_STANDALONE = 1,
  QTEL_GPS_MODE_MS_BASED,
  QTEL_GPS_MODE_MS_ASISTED,
  QTEL_GPS_MODE_SPEED_OPTIMAL,
} QTEL_GPS_Mode_t;

typedef enum {
  QTEL_GPS_CFG_OutPort,
  QTEL_GPS_CFG_NMEA_Src,
  QTEL_GPS_CFG_GpsNMEA_Type,
  QTEL_GPS_CFG_GlonassNMEA_Type,
  QTEL_GPS_CFG_GalileoNMEA_Type,
  QTEL_GPS_CFG_BeidouNMEA_Type,
  QTEL_GPS_CFG_GnssConfig,
  QTEL_GPS_CFG_ODP_Control,
  QTEL_GPS_CFG_DPO_Enable,
  QTEL_GPS_CFG_GsvextNMEA_Type,
  QTEL_GPS_CFG_Plane,
  QTEL_GPS_CFG_AutoGps,
  QTEL_GPS_CFG_SUPL_ver,
  QTEL_GPS_CFG_AGPS_Posmode,
  QTEL_GPS_CFG_AGNSS_Protocol,
  QTEL_GPS_CFG_FixFreq,
} QTEL_GPS_ConfigKey_t;


uint8_t QTEL_GPS_CheckAsyncResponse(QTEL_HandlerTypeDef*);
void    QTEL_GPS_HandleEvents(QTEL_HandlerTypeDef*);

QTEL_Status_t QTEL_GPS_Config(QTEL_HandlerTypeDef*, QTEL_GPS_ConfigKey_t, void *value);
void          QTEL_GPS_Init(QTEL_HandlerTypeDef*, uint8_t *buffer, uint16_t bufferSize);
QTEL_Status_t QTEL_GPS_Activate(QTEL_HandlerTypeDef*, QTEL_GPS_Mode_t);
QTEL_Status_t QTEL_GPS_Deactivate(QTEL_HandlerTypeDef*);
QTEL_Status_t QTEL_GPS_SetupOneXTra(QTEL_HandlerTypeDef*);
QTEL_Status_t QTEL_GPS_DeleteOneXTra(QTEL_HandlerTypeDef*);
QTEL_Status_t QTEL_GPS_getLocation(QTEL_HandlerTypeDef*);
QTEL_Status_t QTEL_GPS_AcquireNMEA(QTEL_HandlerTypeDef*, const char* nmea_type);

#endif /* QTEL_EN_FEATURE_GPS */
#endif /* QTEL_QUECTEL_EC25_GPS_H_ */
