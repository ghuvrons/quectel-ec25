/*
 * utils.h
 *
 *  Created on: Aug 2, 2022
 *      Author: janoko
 */

#ifndef QTEL_QUECTEL_EC25_SIMCOM_UTILS_H_
#define QTEL_QUECTEL_EC25_SIMCOM_UTILS_H_

#include "../quectel.h"
#include <string.h>


// MACROS

#ifndef QTEL_GetTick
#define QTEL_GetTick() 0
#endif
#ifndef QTEL_Delay
#define QTEL_Delay(ms) {}
#endif

#define QTEL_IsTimeout(lastTick, timeout) ((QTEL_GetTick() - (lastTick)) > (timeout))

#define QTEL_IsResponse(hqtel, resp, min_len) \
  ((hqtel)->respBufferLen >= (min_len) \
    && strncmp((const char *)(hqtel)->respBuffer, (resp), (int)(min_len)) == 0)

#define QTEL_IsResponseOK(hqtel) \
  (QTEL_GetResponse((hqtel), NULL, 0, NULL, 0, QTEL_GETRESP_WAIT_OK, 0) == QTEL_OK)


#define QTEL_BITS_IS_ALL(bits, bit) (((bits) & (bit)) == (bit))
#define QTEL_BITS_IS_ANY(bits, bit) ((bits) & (bit))
#define QTEL_BITS_IS(bits, bit)     QTEL_BITS_IS_ALL(bits, bit)
#define QTEL_BITS_SET(bits, bit)    {(bits) |= (bit);}
#define QTEL_BITS_UNSET(bits, bit)  {(bits) &= ~(bit);}

#define QTEL_IS_STATUS(hqtel, stat)     QTEL_BITS_IS_ALL((hqtel)->status, stat)
#define QTEL_SET_STATUS(hqtel, stat)    QTEL_BITS_SET((hqtel)->status, stat)
#define QTEL_UNSET_STATUS(hqtel, stat)  QTEL_BITS_UNSET((hqtel)->status, stat)

#if QTEL_EN_FEATURE_NET
#define QTEL_NET_IS_STATUS(hqtel, stat)     QTEL_BITS_IS_ALL((hqtel)->net.status, stat)
#define QTEL_NET_SET_STATUS(hqtel, stat)    QTEL_BITS_SET((hqtel)->net.status, stat)
#define QTEL_NET_UNSET_STATUS(hqtel, stat)  QTEL_BITS_UNSET((hqtel)->net.status, stat)
#endif

#if QTEL_EN_FEATURE_HTTP
#define QTEL_HTTP_IS_STATUS(hqtel, stat)     QTEL_BITS_IS_ALL((hqtel)->HTTP.status, stat)
#define QTEL_HTTP_SET_STATUS(hqtel, stat)    QTEL_BITS_SET((hqtel)->HTTP.status, stat)
#define QTEL_HTTP_UNSET_STATUS(hqtel, stat)  QTEL_BITS_UNSET((hqtel)->HTTP.status, stat)
#endif

#if QTEL_EN_FEATURE_MQTT
#define QTEL_MQTT_IS_STATUS(hqtel, stat)     QTEL_BITS_IS_ALL((hqtel)->mqtt.status, stat)
#define QTEL_MQTT_SET_STATUS(hqtel, stat)    QTEL_BITS_SET((hqtel)->mqtt.status, stat)
#define QTEL_MQTT_UNSET_STATUS(hqtel, stat)  QTEL_BITS_UNSET((hqtel)->mqtt.status, stat)
#endif

#if QTEL_EN_FEATURE_GPS
#define QTEL_GPS_IS_STATUS(hqtel, stat)     QTEL_BITS_IS_ALL((hqtel)->gps.status, stat)
#define QTEL_GPS_SET_STATUS(hqtel, stat)    QTEL_BITS_SET((hqtel)->gps.status, stat)
#define QTEL_GPS_UNSET_STATUS(hqtel, stat)  QTEL_BITS_UNSET((hqtel)->gps.status, stat)
#endif

uint8_t       QTEL_SendCMD(QTEL_HandlerTypeDef*, const char *format, ...);
uint8_t       QTEL_SendData(QTEL_HandlerTypeDef*, const uint8_t *data, uint16_t size);
uint8_t       QTEL_WaitResponse(QTEL_HandlerTypeDef*, const char *respCode, uint16_t rcsize, uint32_t timeout);
QTEL_Status_t QTEL_GetResponse(QTEL_HandlerTypeDef*, const char *respCode, uint16_t rcsize,
                               uint8_t *respData, uint16_t rdsize,
                               uint8_t getRespType,
                               uint32_t timeout);
QTEL_Status_t QTEL_GetMultipleResponse(QTEL_HandlerTypeDef*,
                                       const char *respCode, uint16_t rcsize,
                                       uint8_t *respData, uint16_t rsize, uint16_t rdsize,
                                       uint8_t getRespType,
                                       uint32_t timeout);
uint16_t      QTEL_GetData(QTEL_HandlerTypeDef*, uint8_t *respData, uint16_t rdsize, uint32_t timeout);
const uint8_t *QTEL_ParseStr(const uint8_t *separator, uint8_t delimiter, int idx, uint8_t *output);

#endif /* QTEL_QUECTEL_EC25_SIMCOM_UTILS_H_ */
