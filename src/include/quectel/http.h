/*
 * http.h
 *
 *  Created on: Aug 4, 2022
 *      Author: janoko
 */

#ifndef QTEL_QUECTEL_EC25_HTTP_H
#define QTEL_QUECTEL_EC25_HTTP_H
#if QTEL_EN_FEATURE_HTTP

#include "../quectel.h"
#include "../quectel/net.h"


#define QTEL_HTTP_STATUS_REQUESTING 0x01

#define QTEL_HTTP_EVENT_GET_RESP    0x01
#define QTEL_HTTP_EVENT_RESP_ERROR  0x02

#define QTEL_HTTP_CFG_KEYS_NUM 11


typedef enum {
  QTEL_HTTP_GET,
  QTEL_HTTP_POST
} QTEL_HTTP_Method_t;

typedef enum {
  QTEL_HTTP_CFG_CtxId,
  QTEL_HTTP_CFG_ReqHeader,
  QTEL_HTTP_CFG_RespHeader,
  QTEL_HTTP_CFG_SSLCtxId,
  QTEL_HTTP_CFG_ContentType,
  QTEL_HTTP_CFG_RespAuto,
  QTEL_HTTP_CFG_ClosedInd,
  QTEL_HTTP_CFG_WindowSize,
  QTEL_HTTP_CFG_CloseWaitTime,
  QTEL_HTTP_CFG_Auth,
  QTEL_HTTP_CFG_CustomHeader,
} QTEL_HTTP_ConfigKey_t;

typedef void (*QTEL_HTTP_ContentReader_Func)(QTEL_HandlerTypeDef*,
                                             const uint8_t* data,
                                             uint16_t datalen,
                                             uint32_t maxLen);

uint8_t QTEL_HTTP_CheckAsyncResponse(QTEL_HandlerTypeDef*);
void    QTEL_HTTP_HandleEvents(QTEL_HandlerTypeDef*);

QTEL_Status_t QTEL_HTTP_Config(QTEL_HandlerTypeDef*, QTEL_HTTP_ConfigKey_t, void *value);
QTEL_Status_t QTEL_HTTP_Request(QTEL_HandlerTypeDef*,
                                QTEL_HTTP_Method_t, const char* url,
                                QTEL_HTTP_ContentReader_Func,
                                uint8_t *contentBuf, uint16_t contentBufLen,
                                uint32_t timeout);
QTEL_Status_t QTEL_HTTP_Stop(QTEL_HandlerTypeDef*);

#endif /* QTEL_EN_FEATURE_HTTP */
#endif /* QTEL_QUECTEL_EC25_HTTP_H */
