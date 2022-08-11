/*
 * modem.c
 *
 *  Created on: Aug 2, 2022
 *      Author: janoko
 */


#include "include/quectel.h"
#include "include/quectel/conf.h"
#include "include/quectel/utils.h"
#include "include/quectel/debug.h"
#include "include/quectel/net.h"
#include "include/quectel/socket.h"
#include "include/quectel/http.h"
#include "include/quectel/gps.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>


// static function initiation
static void QTEL_reset(QTEL_HandlerTypeDef*);
static void str2Time(QTEL_Datetime*, const char*);


// function definition


QTEL_Status_t QTEL_Init(QTEL_HandlerTypeDef *hqtel)
{
  QTEL_Debug("Init");
  if (hqtel->serial.device == NULL
      || hqtel->serial.read == NULL
      || hqtel->serial.readline == NULL
      || hqtel->serial.forwardToBuffer == NULL
      || hqtel->serial.unread == NULL
      || hqtel->serial.write == NULL)
    goto endinit;

  hqtel->status = 0;
  hqtel->events = 0;
  hqtel->errors = 0;
  hqtel->signal = 0;
  if (hqtel->timeout == 0)
    hqtel->timeout = 5000;
  hqtel->initAt = QTEL_GetTick();

  QTEL_Debug("Init Ok");
  return QTEL_OK;

  endinit:
  QTEL_Debug("Init Error");
  return QTEL_ERROR;
}


/*
 * Read response per lines at a certain time interval
 */
void QTEL_CheckAnyResponse(QTEL_HandlerTypeDef *hqtel)
{
  if (hqtel->serial.device == NULL || hqtel->serial.readline == NULL)
    return;

  // Read incoming Response
  QTEL_LOCK(hqtel);
  while (hqtel->serial.isAvailable == NULL || hqtel->serial.isAvailable(hqtel->serial.device)) {
    hqtel->respBufferLen = hqtel->serial.readline(hqtel->serial.device, hqtel->respBuffer, QTEL_RESP_BUFFER_SIZE, 5000);
    if (hqtel->respBufferLen) {
      QTEL_CheckAsyncResponse(hqtel);
    }
  }
  QTEL_UNLOCK(hqtel);

  // Event Handler
  QTEL_HandleEvents(hqtel);
}


void QTEL_CheckAsyncResponse(QTEL_HandlerTypeDef *hqtel)
{
  hqtel->respBuffer[hqtel->respBufferLen] = 0;

  if (QTEL_IsResponse(hqtel, "\r\n", 2)) {
    return;
  }

  if (QTEL_IsResponse(hqtel, "RDY", 3)) {
    QTEL_BITS_SET(hqtel->events, QTEL_EVENT_ON_STARTING);
  }

  #if QTEL_EN_FEATURE_NET
  else if (QTEL_NET_CheckAsyncResponse(hqtel)) return;
  #endif

  #if QTEL_EN_FEATURE_SOCKET
  else if (QTEL_SockCheckAsyncResponse(hqtel)) return;
  #endif

  #if QTEL_EN_FEATURE_HTTP
  else if (QTEL_HTTP_CheckAsyncResponse(hqtel)) return;
  #endif

  #if QTEL_EN_FEATURE_GPS
  else if (QTEL_GPS_CheckAsyncResponse(hqtel)) return;
  #endif
}


/*
 * Handle async response
 */
void QTEL_HandleEvents(QTEL_HandlerTypeDef *hqtel)
{
  // check async response
  if (hqtel->status == 0 && (QTEL_GetTick() - hqtel->initAt > hqtel->timeout)) {
    if (QTEL_CheckAT(hqtel) != QTEL_OK) {
      hqtel->initAt = QTEL_GetTick();
      return;
    } else {
      QTEL_Echo(hqtel, 0);
      QTEL_BITS_SET(hqtel->events, QTEL_EVENT_ON_STARTED);
    }
  }

  if (QTEL_BITS_IS(hqtel->events, QTEL_EVENT_ON_STARTING)) {
    QTEL_BITS_UNSET(hqtel->events, QTEL_EVENT_ON_STARTING);
    QTEL_reset(hqtel);
    QTEL_Debug("Starting...");
    if (QTEL_CheckAT(hqtel) == QTEL_OK) {
      QTEL_Echo(hqtel, 0);
      QTEL_BITS_SET(hqtel->events, QTEL_EVENT_ON_STARTED);
    }
  }
  if (QTEL_IS_STATUS(hqtel, QTEL_STATUS_ACTIVE) && !QTEL_IS_STATUS(hqtel, QTEL_STATUS_SIMCARD_READY)) {
    if(QTEL_CheckSIMCard(hqtel) != QTEL_OK) {
      QTEL_Delay(3000);
    }
  }
  if (QTEL_IS_STATUS(hqtel, QTEL_STATUS_SIMCARD_READY) && !QTEL_IS_STATUS(hqtel, QTEL_STATUS_REGISTERED)) {
    if(QTEL_ReqisterNetwork(hqtel) != QTEL_OK) {
      QTEL_Delay(3000);
    }
  }
  if (QTEL_BITS_IS(hqtel->events, QTEL_EVENT_ON_STARTED)) {
    QTEL_BITS_UNSET(hqtel->events, QTEL_EVENT_ON_STARTED);
    QTEL_Debug("Started.");
    QTEL_AutoUpdateTZ(hqtel, 1);
    QTEL_SockOnStarted(hqtel);
  }
  if (QTEL_BITS_IS(hqtel->events, QTEL_EVENT_ON_REGISTERED)) {
    QTEL_BITS_UNSET(hqtel->events, QTEL_EVENT_ON_REGISTERED);
    QTEL_Debug("Network Registered%s.", (QTEL_IS_STATUS(hqtel, QTEL_STATUS_ROAMING))? " (Roaming)": "");
  }

#ifdef QTEL_EN_FEATURE_NET
  QTEL_NET_HandleEvents(hqtel);
#endif

#ifdef QTEL_EN_FEATURE_SOCKET
  QTEL_SockHandleEvents(hqtel);
#endif

#ifdef QTEL_EN_FEATURE_HTTP
  QTEL_HTTP_HandleEvents(hqtel);
#endif

#if QTEL_EN_FEATURE_GPS
  QTEL_GPS_HandleEvents(hqtel);
#endif

}


QTEL_Status_t QTEL_Echo(QTEL_HandlerTypeDef *hqtel, uint8_t onoff)
{
  QTEL_Status_t status = QTEL_ERROR;

  QTEL_LOCK(hqtel);
  if (onoff)
    QTEL_SendCMD(hqtel, "ATE1");
  else
    QTEL_SendCMD(hqtel, "ATE0");
  if (!QTEL_IsResponseOK(hqtel)) goto endcmd;

  endcmd:
  QTEL_UNLOCK(hqtel);
  return status;
}


QTEL_Status_t QTEL_CheckAT(QTEL_HandlerTypeDef *hqtel)
{
  QTEL_Status_t status = QTEL_ERROR;
  
  QTEL_LOCK(hqtel);
  
  QTEL_SendCMD(hqtel, "AT");
  if (!QTEL_IsResponseOK(hqtel)) {
    QTEL_UNSET_STATUS(hqtel, QTEL_STATUS_ACTIVE);
    goto endcmd;
  }
  status = QTEL_OK;
  QTEL_SET_STATUS(hqtel, QTEL_STATUS_ACTIVE);

  endcmd:
  QTEL_UNLOCK(hqtel);
  return status;
}


int16_t QTEL_GetSignal(QTEL_HandlerTypeDef *hqtel)
{
  int16_t signal = 0;
  uint8_t *resp = &hqtel->respTmp[0];
  char *signalStr = (char*) &hqtel->respTmp[32];

  if (!QTEL_IS_STATUS(hqtel, QTEL_STATUS_SIMCARD_READY)) return signal;

  // send command then get response;
  QTEL_LOCK(hqtel);
  QTEL_SendCMD(hqtel, "AT+CSQ");

  memset(resp, 0, 16);

  // do with response
  if (QTEL_GetResponse(hqtel, "+CSQ", 4, resp, 16, QTEL_GETRESP_WAIT_OK, 2000) == QTEL_OK) {
    QTEL_ParseStr(resp, ',', 1, (uint8_t*) signalStr);
    signal = (int16_t) atoi((char*)resp);
    hqtel->signal = signal;
  }
  else goto errorHandle;

  QTEL_UNLOCK(hqtel);

  if (signal == 99) {
    signal = 0;
    hqtel->signal = signal;
    QTEL_ReqisterNetwork(hqtel);
  }

  return hqtel->signal;

  errorHandle:
  QTEL_UNLOCK(hqtel);
  return (int16_t) QTEL_ERROR;
}


QTEL_Status_t QTEL_CheckSIMCard(QTEL_HandlerTypeDef *hqtel)
{
  QTEL_Status_t status  = QTEL_ERROR;
  uint8_t       *resp   = &hqtel->respTmp[0];

  QTEL_LOCK(hqtel);

  memset(resp, 0, 11);
  QTEL_SendCMD(hqtel, "AT+CPIN?");
  status = QTEL_GetResponse(hqtel, "+CPIN", 5, resp, 10, QTEL_GETRESP_WAIT_OK, 2000);
  if (status != QTEL_OK) {
    QTEL_Debug("SIM card error.");
    goto endcmd;
  }
  
  if (strcmp((char*) resp, "READY")) {
    QTEL_Debug("SIM Ready.");
    status = QTEL_OK;
    QTEL_SET_STATUS(hqtel, QTEL_STATUS_SIMCARD_READY);
  }

  endcmd:
  QTEL_UNLOCK(hqtel);
  return status;
}


QTEL_Status_t QTEL_ReqisterNetwork(QTEL_HandlerTypeDef *hqtel)
{
  QTEL_Status_t status    = QTEL_ERROR;
  uint8_t       *resp     = &hqtel->respTmp[0];
  uint8_t       resp_stat = 0;
  uint8_t       resp_mode = 0;

  // send command then get response;
  QTEL_LOCK(hqtel);

  memset(resp, 0, 4);
  QTEL_SendCMD(hqtel, "AT+CREG?");
  if (QTEL_GetResponse(hqtel, "+CREG", 5, resp, 3, QTEL_GETRESP_WAIT_OK, 2000) == QTEL_OK) {
    // resp_n = (uint8_t) atoi((char*)&resp[0]);
    resp_stat = (uint8_t) atoi((char*) resp+2);
  }
  else goto endcmd;

  // check response
  if (resp_stat == 1 || resp_stat == 5) {
    QTEL_SET_STATUS(hqtel, QTEL_STATUS_REGISTERED);
    QTEL_BITS_SET(hqtel->events, QTEL_EVENT_ON_REGISTERED);
    status = QTEL_OK;
    if (resp_stat == 5) {
      QTEL_SET_STATUS(hqtel, QTEL_STATUS_ROAMING);
    }
  }
  else {
    QTEL_UNSET_STATUS(hqtel, QTEL_STATUS_REGISTERED);

    if (resp_stat == 0) {
      QTEL_Debug("Registering network....");

      // Select operator automatically
      memset(resp, 0, 16);
      QTEL_SendCMD(hqtel, "AT+COPS?");
      status = QTEL_GetResponse(hqtel, "+COPS", 5, resp, 1, QTEL_GETRESP_WAIT_OK, 2000);
      if (status != QTEL_OK) goto endcmd;

      resp_mode = (uint8_t) atoi((char*) resp);

      QTEL_SendCMD(hqtel, "AT+COPS=?");
      if (!QTEL_IsResponseOK(hqtel)) goto endcmd;

      if (resp_mode != 0) {
        QTEL_SendCMD(hqtel, "AT+COPS=0");
        if (!QTEL_IsResponseOK(hqtel)) goto endcmd;

        QTEL_SendCMD(hqtel, "AT+COPS");
        if (!QTEL_IsResponseOK(hqtel)) goto endcmd;
      }

      status = QTEL_OK;
    }
    else if (resp_stat == 2) {
      status = QTEL_OK;
      QTEL_Debug("Searching network....");
      QTEL_Delay(2000);
    }
  }

  endcmd:
  QTEL_UNLOCK(hqtel);
  return status;
}


QTEL_Status_t QTEL_AutoUpdateTZ(QTEL_HandlerTypeDef *hqtel, uint8_t enable)
{
  QTEL_Status_t status = QTEL_ERROR;

  QTEL_LOCK(hqtel);
  if (enable)
    QTEL_SendCMD(hqtel, "AT+CTZU=3");
  else
    QTEL_SendCMD(hqtel, "AT+CTZU=0");

  if (!QTEL_IsResponseOK(hqtel)) goto endcmd;
  status = QTEL_OK;

  endcmd:
  QTEL_UNLOCK(hqtel);
  return status;
}


QTEL_Datetime QTEL_GetTime(QTEL_HandlerTypeDef *hqtel)
{
  QTEL_Datetime result = {0};
  uint8_t *resp = &hqtel->respTmp[0];

  // send command then get response;
  QTEL_LOCK(hqtel);

  memset(resp, 0, 22);
  QTEL_SendCMD(hqtel, "AT+CCLK?");
  if (QTEL_GetResponse(hqtel, "+CCLK", 5, resp, 22, QTEL_GETRESP_WAIT_OK, 2000) == QTEL_OK) {
    str2Time(&result, (char*)&resp[0]);
  }
  QTEL_UNLOCK(hqtel);

  return result;
}


void QTEL_HashTime(QTEL_HandlerTypeDef *hqtel, char *hashed)
{
  QTEL_Datetime dt;
  uint8_t *dtBytes = (uint8_t *) &dt;
  dt = QTEL_GetTime(hqtel);
  for (uint8_t i = 0; i < 6; i++) {
    *hashed = (*dtBytes) + 0x41 + i;
    if (*hashed > 0x7A) {
      *hashed = 0x7A - i;
    }
    if (*hashed < 0x30) {
      *hashed = 0x30 + i;
    }
    dtBytes++;
    hashed++;
  }
}


QTEL_Status_t QTEL_SendUSSD(QTEL_HandlerTypeDef *hqtel, const char *ussd)
{
  QTEL_Status_t status = QTEL_ERROR;

  if (!QTEL_IS_STATUS(hqtel, QTEL_STATUS_REGISTERED)) return status;

  QTEL_LOCK(hqtel);
  QTEL_SendCMD(hqtel, "AT+CSCS=\"GSM\"");
  if (!QTEL_IsResponseOK(hqtel)) goto endcmd;

  QTEL_SendCMD(hqtel, "AT+CUSD=1,%s,15", ussd);
  if (!QTEL_IsResponseOK(hqtel)) goto endcmd;
  status = QTEL_OK;

  endcmd:
  QTEL_UNLOCK(hqtel);
  return status;
}


static void QTEL_reset(QTEL_HandlerTypeDef *hqtel)
{
  hqtel->signal = 0;
  hqtel->status = 0;
  hqtel->errors = 0;
}


static void str2Time(QTEL_Datetime *dt, const char *str)
{
  uint8_t *dtbytes;
  int8_t mult = 1;

  str++;
  dt->year = (uint8_t) atoi(str);
  dtbytes = ((uint8_t*) dt) + 1;
  while (*str && *str != '\"') {
    if (*str < '0' || *str > '9') {
      if (*str == '-') {
        mult = -1;
      } else {
        mult = 1;
      }
      str++;
      *dtbytes = ((int8_t) atoi(str)) * mult;
      dtbytes++;
    }
    str++;
  }
}
