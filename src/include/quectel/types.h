/*
 * simnet.h
 *
 *  Created on: Aug 2, 2022
 *      Author: janoko
 */


#ifndef QTEL_QUECTEL_EC25_TYPES_H_
#define QTEL_QUECTEL_EC25_TYPES_H_

#include <stdint.h>


typedef uint8_t (*asyncResponseHandler) (uint16_t bufLen);
typedef enum {
  QTEL_OK,
  QTEL_ERROR    = -1,
  QTEL_TIMEOUT  = -2,
  QTEL_BUSY     = -3
} QTEL_Status_t;

typedef struct {
  uint8_t year;
  uint8_t month;
  uint8_t day;
  uint8_t hour;
  uint8_t minute;
  uint8_t second;
  int8_t  timezone;
} QTEL_Datetime;

#endif /* QTEL_QUECTEL_EC25_TYPES_H_ */