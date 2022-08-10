/*
 * modem.h
 *
 *  Created on: Sep 14, 2021
 *      Author: janoko
 */

#ifndef QTEL_QUECTEL_EC25_INC_QUECTEL_H_
#define QTEL_QUECTEL_EC25_INC_QUECTEL_H_

#include "quectel/conf.h"
#include "quectel/types.h"
#include <buffer.h>

#if QTEL_EN_FEATURE_GPS
#include <lwgps/lwgps.h>
#endif

/**
 * SIM STATUS
 * bit  0   is device connected
 *      1   is uart reading
 *      2   is uart writting
 *      3   is cmd running
 *      4   is net opened
 */

#define QTEL_STATUS_ACTIVE          0x01
#define QTEL_STATUS_SIMCARD_READY   0x02
#define QTEL_STATUS_REGISTERED      0x04
#define QTEL_STATUS_ROAMING         0x08
#define QTEL_STATUS_UART_READING    0x10
#define QTEL_STATUS_UART_WRITING    0x20
#define QTEL_STATUS_CMD_RUNNING     0x40

#define QTEL_GETRESP_WAIT_OK   0
#define QTEL_GETRESP_ONLY_DATA 1

#define QTEL_EVENT_ON_STARTING   0x01
#define QTEL_EVENT_ON_STARTED    0x02
#define QTEL_EVENT_ON_REGISTERED 0x04

// MACROS
#define QTEL_LOCK(hqtel) {if ((hqtel)->lock != NULL) (hqtel)->lock();}
#define QTEL_UNLOCK(hqtel) {if ((hqtel)->unlock != NULL) (hqtel)->unlock();}

typedef struct {
  uint8_t             status;
  uint8_t             events;
  uint8_t             errors;
  uint8_t             signal;
  uint32_t            timeout;
  
  // serial method
  struct {
    void      *device;
    uint8_t   (*isAvailable)(void *serialDev);  // return boolean
    uint16_t  (*read)(void *serialDev, uint8_t *dstBuf, uint16_t bufSz, uint32_t timeout);
    uint16_t  (*readline)(void *serialDev, uint8_t *dstBuf, uint16_t bufSz, uint32_t timeout);
    uint16_t  (*forwardToBuffer)(void *serialDev, Buffer_t *buf, uint16_t len, uint32_t timeout);
    void      (*unread)(void *serialDev, uint16_t len);
    uint16_t  (*write)(void *serialDev, const uint8_t *data, uint16_t len);
  } serial;

  struct {
    uint8_t status;
  } file;

  #if QTEL_EN_FEATURE_NET
  struct {
    uint8_t status;
    uint8_t events;
    uint8_t contextId;

    struct {
      const char *APN;
      const char *user;
      const char *pass;
    } APN;

    void (*onOpening)(void);
    void (*onOpened)(void);
    void (*onOpenError)(void);
    void (*onClosed)(void);

    #if QTEL_EN_FEATURE_SOCKET
    void *sockets[QTEL_NUM_OF_SOCKET];
    #endif

  } net;

  #if QTEL_EN_FEATURE_NTP
  struct {
    uint8_t     status;
    uint8_t     events;
    uint8_t     counterIdTmpFile;
    const char  *lastUrl;

    struct {
      uint16_t error;
      uint16_t code;
      uint32_t contentLen;
      void *cb;
    } response;
  } HTTP;
  #endif /* QTEL_EN_FEATURE_NTP */

  #if QTEL_EN_FEATURE_NTP
  struct {
    const char  *server;
    uint32_t    syncTick;
    void (*onSynced)(QTEL_Datetime);

    struct {
      uint32_t retryInterval;
      uint32_t resyncInterval;
    } config;
  } NTP;
  #endif /* QTEL_EN_FEATURE_NTP */
  #endif /* QTEL_EN_FEATURE_NET */

  #if QTEL_EN_FEATURE_NET && QTEL_EN_FEATURE_MQTT
  struct {
    uint8_t status;
    uint8_t events;
  } mqtt;
  #endif

  #if QTEL_EN_FEATURE_GPS
  struct {
    uint8_t   status;
    uint8_t   events;
    Buffer_t  buffer;
    void      *xtraFileTmpPtr;
    uint8_t   readBuffer[QTEL_GPS_TMP_BUF_SIZE];
    lwgps_t   lwgps;
    uint32_t  nmeaTick;
  } gps;
  #endif

  // Buffers
  uint8_t   respTmp[QTEL_TMP_RESP_BUFFER_SIZE];
  uint8_t   respBuffer[QTEL_RESP_BUFFER_SIZE];
  uint16_t  respBufferLen;

  uint8_t   cmdTmp[QTEL_TMP_RESP_BUFFER_SIZE];
  uint8_t   cmdBuffer[QTEL_CMD_BUFFER_SIZE];
  uint16_t  cmdBufferLen;

  uint32_t  initAt;

  // for RTOS
  void (*lock)(void);
  void (*unlock)(void);
} QTEL_HandlerTypeDef;


QTEL_Status_t QTEL_Init(QTEL_HandlerTypeDef*);
void          QTEL_CheckAnyResponse(QTEL_HandlerTypeDef*);
void          QTEL_CheckAsyncResponse(QTEL_HandlerTypeDef*);
void          QTEL_HandleEvents(QTEL_HandlerTypeDef*);
QTEL_Status_t QTEL_Echo(QTEL_HandlerTypeDef*, uint8_t onoff);
QTEL_Status_t QTEL_CheckAT(QTEL_HandlerTypeDef*);
int16_t       QTEL_GetSignal(QTEL_HandlerTypeDef*);
QTEL_Status_t QTEL_CheckSIMCard(QTEL_HandlerTypeDef*);
QTEL_Status_t QTEL_ReqisterNetwork(QTEL_HandlerTypeDef*);
QTEL_Status_t QTEL_AutoUpdateTZ(QTEL_HandlerTypeDef*, uint8_t enable);
QTEL_Datetime QTEL_GetTime(QTEL_HandlerTypeDef*);
void          QTEL_HashTime(QTEL_HandlerTypeDef*, char *hashed);
QTEL_Status_t QTEL_SendSms(QTEL_HandlerTypeDef*);
QTEL_Status_t QTEL_SendUSSD(QTEL_HandlerTypeDef*, const char *ussd);

#endif /* QTEL_QUECTEL_EC25_INC_QUECTEL_H_ */
