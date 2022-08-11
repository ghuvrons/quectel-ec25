/*
 * utils.h
 *
 *  Created on: Dec 3, 2021
 *      Author: janoko
 */

#include "include/quectel.h"
#include "include/quectel/conf.h"
#include "include/quectel/utils.h"
#include "include/quectel/debug.h"
#include <stdarg.h>
#include <stdio.h>
#include <string.h>


__attribute__((weak)) void QTEL_Printf(const char *format, ...) {}
__attribute__((weak)) void QTEL_Println(const char *format, ...) {}


uint8_t QTEL_SendCMD(QTEL_HandlerTypeDef *hqtel, const char *format, ...)
{
  va_list arglist;
  va_start( arglist, format );
  hqtel->cmdBufferLen = vsprintf((char*)hqtel->cmdBuffer, format, arglist);
  va_end( arglist );
  if (hqtel->serial.device == NULL || hqtel->serial.write == NULL) return 0;
  
  if (hqtel->serial.write(hqtel->serial.device, hqtel->cmdBuffer, hqtel->cmdBufferLen) == 0) return 0;
  if (hqtel->serial.write(hqtel->serial.device, (uint8_t*) "\r\n", 2) == 0) return 0;
  return 1;
}


uint8_t QTEL_SendData(QTEL_HandlerTypeDef *hqtel, const uint8_t *data, uint16_t size)
{
  if (hqtel->serial.device == NULL || hqtel->serial.write == NULL) return 0;
  if (hqtel->serial.write(hqtel->serial.device, data, size) == 0) return 0;
  return 1;
}



uint8_t QTEL_WaitResponse(QTEL_HandlerTypeDef *hqtel,
                          const char *respCode, uint16_t rcsize,
                          uint32_t timeout)
{
  uint32_t tickstart = QTEL_GetTick();

  if (hqtel->serial.device == NULL
      || hqtel->serial.read == NULL
      || hqtel->serial.unread == NULL
      || hqtel->serial.readline == NULL) return 0;
  if (rcsize > QTEL_RESP_BUFFER_SIZE) rcsize = QTEL_RESP_BUFFER_SIZE;
  if (timeout == 0) timeout = hqtel->timeout;

  while (1) {
    if((QTEL_GetTick() - tickstart) >= timeout) break;
    hqtel->respBufferLen = hqtel->serial.read(hqtel->serial.device, hqtel->respBuffer, rcsize, timeout);
    if (QTEL_IsResponse(hqtel, respCode, rcsize)) {
      return 1;
    }
    hqtel->serial.unread(hqtel->serial.device, hqtel->respBufferLen);

    hqtel->respBufferLen = hqtel->serial.readline(hqtel->serial.device, hqtel->respBuffer, QTEL_RESP_BUFFER_SIZE, timeout);
    if (hqtel->respBufferLen) {
      QTEL_CheckAsyncResponse(hqtel);
    }
  }
  return 0;
}


QTEL_Status_t QTEL_GetResponse(QTEL_HandlerTypeDef *hqtel,
                               const char *respCode, uint16_t rcsize,
                               uint8_t *respData, uint16_t rdsize,
                               uint8_t getRespType,
                               uint32_t timeout)
{
  uint16_t i;
  uint8_t resp = QTEL_TIMEOUT;
  uint8_t flagToReadResp = 0;
  uint32_t tickstart = QTEL_GetTick();

  if (hqtel->serial.device == NULL || hqtel->serial.readline == NULL) return 0;
  if (timeout == 0) timeout = hqtel->timeout;

  // wait until available
  while(1) {
    if((QTEL_GetTick() - tickstart) >= timeout) break;

    hqtel->respBufferLen = hqtel->serial.readline(hqtel->serial.device, hqtel->respBuffer, QTEL_RESP_BUFFER_SIZE, timeout);
    if (hqtel->respBufferLen) {
      hqtel->respBuffer[hqtel->respBufferLen] = 0;
      if (rcsize && strncmp((char *)hqtel->respBuffer, respCode, (int) rcsize) == 0) {
        if (flagToReadResp) continue;

        // read response data
        for (i = 2; i < hqtel->respBufferLen && rdsize; i++) {
          // split string
          if (!flagToReadResp && hqtel->respBuffer[i-2] == ':' && hqtel->respBuffer[i-1] == ' ') {
            flagToReadResp = 1;
          }

          if (flagToReadResp) {
            *respData = hqtel->respBuffer[i];
            respData++;
            rdsize--;
          }
        }
        if (rdsize) *respData = 0;
        if (getRespType == QTEL_GETRESP_ONLY_DATA) {
          resp = QTEL_OK;
          break;
        }
        if (resp != QTEL_TIMEOUT) break;
      }
      else if (getRespType != QTEL_GETRESP_ONLY_DATA && QTEL_IsResponse(hqtel, "OK", 2)) {
        resp = QTEL_OK;
      }
      else if (QTEL_IsResponse(hqtel, "ERROR", 5)) {
        resp = QTEL_ERROR;
      }
      else if (QTEL_IsResponse(hqtel, "+CME ERROR", 10)) {
        resp = QTEL_ERROR;
        hqtel->respBuffer[hqtel->respBufferLen] = 0;
        QTEL_Debug("[Error] %s", (char*) (hqtel->respBuffer+10));
      }

      // check is got async response
      else {
        QTEL_CheckAsyncResponse(hqtel);
      }

      // break if will not get data
      if (resp != QTEL_TIMEOUT) {
        break;
      }
    }
  }

  return resp;
}


QTEL_Status_t QTEL_GetMultipleResponse(QTEL_HandlerTypeDef *hqtel,
                                       const char *respCode, uint16_t rcsize,
                                       uint8_t *respData, uint16_t rsize, uint16_t rdsize,
                                       uint8_t getRespType,
                                       uint32_t timeout)
{
  uint16_t i;
  uint8_t resp = QTEL_TIMEOUT;
  uint8_t flagToReadResp = 0;
  uint32_t tickstart = QTEL_GetTick();
  uint8_t *respDataPtr = respData;
  uint16_t rdsizeCur = rdsize;

  if (hqtel->serial.device == NULL || hqtel->serial.readline == NULL) return 0;

  // initiate
  if (timeout == 0) timeout = hqtel->timeout;
  memset(respData, 0, rsize*rdsize);

  // wait until available
  while(1) {
    if((QTEL_GetTick() - tickstart) >= timeout) break;

    hqtel->respBufferLen = hqtel->serial.readline(hqtel->serial.device, hqtel->respBuffer, QTEL_RESP_BUFFER_SIZE, timeout);
    if (hqtel->respBufferLen) {
      hqtel->respBuffer[hqtel->respBufferLen] = 0;
      if (rcsize && rsize
          && rcsize <= hqtel->respBufferLen
          && strncmp((char *)hqtel->respBuffer, respCode, (int) rcsize) == 0)
      {
        // read response data
        for (i = 2; i < hqtel->respBufferLen && rdsizeCur; i++) {
          // split string
          if (!flagToReadResp && hqtel->respBuffer[i-2] == ':' && hqtel->respBuffer[i-1] == ' ') {
            flagToReadResp = 1;
          }

          if (flagToReadResp) {
            *respDataPtr = hqtel->respBuffer[i];
            respDataPtr++;
            rdsizeCur--;
          }
        }
        if (rdsizeCur) *respDataPtr = 0;
        respData += rdsize;
        respDataPtr = respData;
        rdsizeCur = rdsize;
        rsize--;

        if (rsize == 0 && getRespType == QTEL_GETRESP_ONLY_DATA) {
          resp = QTEL_OK;
          break;
        }
      }
      else if (getRespType != QTEL_GETRESP_ONLY_DATA && QTEL_IsResponse(hqtel, "OK", 2)) {
        resp = QTEL_OK;
      }
      else if (QTEL_IsResponse(hqtel, "ERROR", 5)) {
        resp = QTEL_ERROR;
      }
      else if (QTEL_IsResponse(hqtel, "+CME ERROR", 10)) {
        resp = QTEL_ERROR;
        hqtel->respBuffer[hqtel->respBufferLen] = 0;
        QTEL_Debug("[Error] %s", (char*) (hqtel->respBuffer+10));
      }

      // check is got async response
      else {
        QTEL_CheckAsyncResponse(hqtel);
      }

      // break if will not get data
      if (resp != QTEL_TIMEOUT) {
        break;
      }
    }
  }

  return resp;
}


uint16_t QTEL_GetData(QTEL_HandlerTypeDef *hqtel, uint8_t *respData, uint16_t rdsize, uint32_t timeout)
{
  if (hqtel->serial.device == NULL || hqtel->serial.read == NULL) return 0;
  return hqtel->serial.read(hqtel->serial.device, respData, rdsize, timeout);
}


const uint8_t * QTEL_ParseStr(const uint8_t *separator, uint8_t delimiter, int idx, uint8_t *output)
{
  uint8_t isInStr = 0;

  while (1)
  {
    if (*separator == 0 || *separator == '\r') break;

    if (!isInStr && *separator == delimiter) {
      idx--;
      if (idx < 0) {
        separator++;
        break;
      }
    }

    else if (*separator == '\"') {
      if (isInStr)  isInStr = 0;
      else          isInStr = 1;
    }

    else if (idx == 0 && output != 0) {
      *output = *separator;
      output++;
    }
    separator++;
  }

  return separator;
}
