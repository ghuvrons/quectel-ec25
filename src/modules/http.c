/*
 * http.c
 *
 *  Created on: Aug 4, 2022
 *      Author: janoko
 */

#if QTEL_EN_FEATURE_HTTP

#include "../include/quectel.h"
#include "../include/quectel/http.h"
#include "../include/quectel/file.h"
#include "../include/quectel/utils.h"
#include "../include/quectel/debug.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define TMP_FILE_FORMAT "tmp%u.http"


static char* keyStr[QTEL_HTTP_CFG_KEYS_NUM] = {
  "contextid",
  "requestheader",
  "responseheader",
  "sslctxid",
  "contenttype",
  "rspout/auto",
  "closed/ind",
  "windowsize",
  "closewaittime",
  "auth",
  "custom_header",
};

static QTEL_Status_t setDefaultConfig(QTEL_HandlerTypeDef*);
static uint16_t parseHeader(uint8_t *srcbuf, uint16_t bufLen, uint8_t *isHeaderClosed);


uint8_t QTEL_HTTP_CheckAsyncResponse(QTEL_HandlerTypeDef *hqtel)
{
  uint8_t         isGet   = 0;

  return isGet;
}


void QTEL_HTTP_HandleEvents(QTEL_HandlerTypeDef *hqtel)
{
}


QTEL_Status_t QTEL_HTTP_Config(QTEL_HandlerTypeDef *hqtel, QTEL_HTTP_ConfigKey_t key, void *value)
{
  QTEL_Status_t status = QTEL_ERROR;

  QTEL_LOCK(hqtel);

  switch (key) {
  case QTEL_HTTP_CFG_Auth:
  case QTEL_HTTP_CFG_CustomHeader:
    QTEL_SendCMD(hqtel, "AT+QHTTPCFG=\"%s\",\"%s\"", keyStr[key], value);
    break;
  default:
    QTEL_SendCMD(hqtel, "AT+QHTTPCFG=\"%s\",%d", keyStr[key], *(int*)value);
    break;
  }

  if (!QTEL_IsResponseOK(hqtel)) goto endcmd;
  status = QTEL_OK;

  endcmd:
  QTEL_UNLOCK(hqtel);
  return status;
}


QTEL_Status_t QTEL_HTTP_Request(QTEL_HandlerTypeDef *hqtel,
                                QTEL_HTTP_Method_t method, const char *url,
                                QTEL_HTTP_ContentReader_Func cb,
                                uint8_t *contentBuf, uint16_t contentBufLen,
                                uint32_t timeout)
{
  QTEL_Status_t status  = QTEL_ERROR;
  uint16_t      urlLen  = strlen(url);
  uint32_t      tick    = QTEL_GetTick();
  QTEL_File_t   f_content;
  uint32_t      readlen;
  uint16_t      tmpReadlen;
  uint8_t       isHeaderClosed;
  char          *tmpFilename  = (char*) &hqtel->cmdTmp[0];
  uint8_t       *resp         = &hqtel->respTmp[0];
  char          *strTmp       = (char*) &hqtel->respTmp[24];
  const uint8_t *nextBuf;

  while (QTEL_HTTP_IS_STATUS(hqtel, QTEL_HTTP_STATUS_REQUESTING)) {
    if (QTEL_IsTimeout(tick, timeout)) return QTEL_TIMEOUT;
    QTEL_Delay(1);
  }
  QTEL_HTTP_SET_STATUS(hqtel, QTEL_HTTP_STATUS_REQUESTING);

  hqtel->HTTP.lastUrl = url;
  hqtel->HTTP.response.error = 0;
  hqtel->HTTP.response.code = 0;
  hqtel->HTTP.response.contentLen = 0;
  QTEL_HTTP_Stop(hqtel);
  setDefaultConfig(hqtel);
  QTEL_HTTP_Config(hqtel, QTEL_HTTP_CFG_CtxId, &hqtel->net.contextId);

  QTEL_LOCK(hqtel);

  // input url
  QTEL_SendCMD(hqtel, "AT+QHTTPURL=%u,1000", urlLen);
  status = QTEL_GetResponse(hqtel, "CONNECT", 7, NULL, 0, QTEL_GETRESP_ONLY_DATA, 0);
  if (status != QTEL_OK)
    goto handleError;

  QTEL_SendData(hqtel, (uint8_t*)url, urlLen);
  if (!QTEL_IsResponseOK(hqtel)) goto handleError;

  // request
  QTEL_SendCMD(hqtel, "AT+QHTTPGET=%d", (timeout/1000));
  if (!QTEL_IsResponseOK(hqtel)) goto handleError;

  // wait response
  memset(resp, 0, 24);
  status = QTEL_GetResponse(hqtel, "+QHTTPGET", 9, resp, 24, QTEL_GETRESP_ONLY_DATA, timeout);
  if (status != QTEL_OK) goto handleError;

  memset(strTmp, 0, 4);
  nextBuf = QTEL_ParseStr(resp, ',', 0, (uint8_t*) strTmp);
  hqtel->HTTP.response.error = (uint16_t) atoi(strTmp);
  if (hqtel->HTTP.response.error != 0) {
    status = QTEL_ERROR;
    goto handleError;
  }

  memset(strTmp, 0, 4);
  nextBuf = QTEL_ParseStr(nextBuf, ',', 0, (uint8_t*) strTmp);
  hqtel->HTTP.response.code = (uint16_t) atoi(strTmp);

  memset(strTmp, 0, 8);
  QTEL_ParseStr(nextBuf, ',', 0, (uint8_t*) strTmp);
  hqtel->HTTP.response.contentLen = (uint32_t) atoi(strTmp);
  QTEL_BITS_SET(hqtel->HTTP.events, QTEL_HTTP_EVENT_GET_RESP);

  // save file in temporary
  hqtel->HTTP.counterIdTmpFile++;
  sprintf(tmpFilename, TMP_FILE_FORMAT, hqtel->HTTP.counterIdTmpFile);

  QTEL_SendCMD(hqtel, "AT+QHTTPREADFILE=\"RAM:%s\",60", tmpFilename);
  if (!QTEL_IsResponseOK(hqtel)) goto handleError;

  memset(resp, 0, 8);
  status = QTEL_GetResponse(hqtel, "+QHTTPREADFILE", 14, resp, 8, QTEL_GETRESP_ONLY_DATA, 60000);
  if (status != QTEL_OK) goto handleError;

  memset(strTmp, 0, 8);
  nextBuf = QTEL_ParseStr(resp, ',', 0, (uint8_t*) strTmp);
  hqtel->HTTP.response.error = (uint16_t) atoi(strTmp);
  if (hqtel->HTTP.response.error != 0) {
    status = QTEL_ERROR;
    goto handleError;
  }

  QTEL_UNLOCK(hqtel);
  QTEL_HTTP_Stop(hqtel);
  QTEL_HTTP_UNSET_STATUS(hqtel, QTEL_HTTP_STATUS_REQUESTING);

  // readfile
  if (contentBuf != NULL) {
    status = QTEL_File_Open(hqtel, &f_content, QTEL_File_Storage_RAM, tmpFilename);
    if (status != QTEL_OK) goto handleError;

    // read header
    readlen = 0;
    do {
      QTEL_Delay(10);
      tmpReadlen = QTEL_File_Read(&f_content, contentBuf, contentBufLen);
      if (tmpReadlen == 0) break;
      readlen += (uint32_t) parseHeader(contentBuf, tmpReadlen, &isHeaderClosed);
      QTEL_File_Seek(&f_content, readlen);
      if (isHeaderClosed) {
        break;
      }
    } while(1);

    // read content
    readlen = 0;
    do {
      tmpReadlen = QTEL_File_Read(&f_content, contentBuf, contentBufLen);
      if (tmpReadlen == 0) break;
      readlen += (uint32_t)tmpReadlen;
      if (cb != NULL) cb(hqtel, contentBuf, tmpReadlen, hqtel->HTTP.response.contentLen);
    } while(readlen < hqtel->HTTP.response.contentLen);
    QTEL_File_Close(&f_content);
  }
  QTEL_File_Delete(hqtel, QTEL_File_Storage_RAM, tmpFilename);

  return QTEL_OK;

  handleError:
  QTEL_UNLOCK(hqtel);
  QTEL_HTTP_Stop(hqtel);
  QTEL_HTTP_UNSET_STATUS(hqtel, QTEL_HTTP_STATUS_REQUESTING);
  return status;
}


QTEL_Status_t QTEL_HTTP_Stop(QTEL_HandlerTypeDef *hqtel)
{
  QTEL_Status_t status = QTEL_ERROR;

  QTEL_LOCK(hqtel);

  QTEL_SendCMD(hqtel, "AT+QHTTPSTOP");
  if (!QTEL_IsResponseOK(hqtel)) goto endcmd;
  status = QTEL_OK;

  endcmd:
  QTEL_UNLOCK(hqtel);
  return status;
}


static QTEL_Status_t setDefaultConfig(QTEL_HandlerTypeDef *hqtel)
{
  int isReqHeader = 0;
  int isRespHeader = 1;
  int isRespOutAuto = 0;
  int isClosedInd = 0;

  QTEL_HTTP_Config(hqtel, QTEL_HTTP_CFG_ReqHeader, &isReqHeader);
  QTEL_HTTP_Config(hqtel, QTEL_HTTP_CFG_RespHeader, &isRespHeader);
  QTEL_HTTP_Config(hqtel, QTEL_HTTP_CFG_RespAuto, &isRespOutAuto);
  QTEL_HTTP_Config(hqtel, QTEL_HTTP_CFG_ClosedInd, &isClosedInd);

  return QTEL_OK;
}


static uint16_t parseHeader(uint8_t *srcbuf, uint16_t bufLen, uint8_t *isHeaderClosed)
{
  uint16_t i = 0;
  uint16_t readlen = 0;
  uint8_t oneHeaderLen = 0;
  uint8_t prevByte = 0;
  uint8_t curByte = 0;

  *isHeaderClosed = 0;

  for (i = 0; i < bufLen; i++) {
    if (prevByte == '\r' && curByte == '\n') {
      if(oneHeaderLen == 2) {
        *isHeaderClosed = 1;
        oneHeaderLen = 0;
        break;
      }
      oneHeaderLen = 0;
    }

    prevByte = curByte;
    curByte = *srcbuf;
    srcbuf++;
    readlen++;
    oneHeaderLen++;
  }
  return readlen-oneHeaderLen;
}
#endif /* QTEL_EN_FEATURE_HTTP */
