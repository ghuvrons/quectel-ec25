/*
 * net.c
 *
 *  Created on: Apr 1, 2022
 *      Author: janoko
 */


#include "../include/quectel.h"
#include "../include/quectel/net.h"
#include "../include/quectel/socket.h"
#include "../include/quectel/utils.h"
#include "../include/quectel/debug.h"
#include <stdlib.h>

#if QTEL_EN_FEATURE_NET

static uint8_t        GprsCheck(QTEL_HandlerTypeDef*);
static void           GprsSetAPN(QTEL_HandlerTypeDef*, uint8_t context_id,
                                 const char *APN, const char *user, const char *pass);
static void           GprsSetQoS(QTEL_HandlerTypeDef*);
static QTEL_Status_t  GprsActivatePDP(QTEL_HandlerTypeDef*);
static void           syncNTP(QTEL_HandlerTypeDef*);


uint8_t QTEL_NET_CheckAsyncResponse(QTEL_HandlerTypeDef *hqtel)
{
  uint8_t   isGet = 0;
  uint16_t  err;
  uint8_t   ctxId;

  if ((isGet = QTEL_IsResponse(hqtel, "+QNTP", 5))) {
    err = (uint16_t) atoi((const char *)&hqtel->respBuffer[7]);
    if (err == 0) {
      QTEL_Debug("[NTP] Synced");
      QTEL_NET_SET_STATUS(hqtel, QTEL_NET_STATUS_NTP_WAS_SYNCED);
      QTEL_BITS_SET(hqtel->net.events, QTEL_NET_EVENT_ON_NTP_WAS_SYNCED);
    } else {
      QTEL_Debug("[NTP] error - %d", err);
    }
    QTEL_NET_UNSET_STATUS(hqtel, QTEL_NET_STATUS_NTP_WAS_SYNCING);
  }

  else if ((isGet = QTEL_IsResponse(hqtel, "+QIURC", 6))) {
    if (strncmp((const char *)&(hqtel->respBuffer[9]), "pdpdeact", 8) == 0) {
      ctxId = (uint8_t) atoi((const char *)&hqtel->respBuffer[19]);
      if (ctxId == hqtel->net.contextId) {
        QTEL_NET_UNSET_STATUS(hqtel, QTEL_NET_STATUS_OPEN|QTEL_NET_STATUS_OPENING);
        QTEL_BITS_SET(hqtel->net.events, QTEL_NET_EVENT_ON_CLOSED);
      }
    } else {
      return 0;
    }
  }

  return isGet;
}


void QTEL_NET_HandleEvents(QTEL_HandlerTypeDef *hqtel)
{

  if (!QTEL_NET_IS_STATUS(hqtel, QTEL_NET_STATUS_GPRS_REGISTERED)
      && QTEL_IS_STATUS(hqtel, QTEL_STATUS_REGISTERED))
  {
    GprsCheck(hqtel);
  }

  if (!QTEL_NET_IS_STATUS(hqtel, QTEL_NET_STATUS_APN_WAS_SET)
      && QTEL_NET_IS_STATUS(hqtel, QTEL_NET_STATUS_GPRS_REGISTERED)
      && QTEL_IS_STATUS(hqtel, QTEL_STATUS_REGISTERED))
  {
    if (hqtel->net.APN.APN != NULL) {
      GprsSetAPN(hqtel,
                 1,
                 hqtel->net.APN.APN,
                 hqtel->net.APN.user,
                 hqtel->net.APN.pass);
    }
    GprsSetQoS(hqtel);
  }

  if (!QTEL_NET_IS_STATUS(hqtel, QTEL_NET_STATUS_OPEN)
      && !QTEL_NET_IS_STATUS(hqtel, QTEL_NET_STATUS_OPENING)
      && QTEL_IS_STATUS(hqtel, QTEL_STATUS_REGISTERED))
  {
    GprsActivatePDP(hqtel);
  }

  #if QTEL_EN_FEATURE_NTP

  if (!QTEL_NET_IS_STATUS(hqtel, QTEL_NET_STATUS_NTP_WAS_SYNCING)
      && QTEL_NET_IS_STATUS(hqtel, QTEL_NET_STATUS_OPEN))
  {
    if (!QTEL_NET_IS_STATUS(hqtel, QTEL_NET_STATUS_NTP_WAS_SYNCED)) {
      if (hqtel->NTP.syncTick == 0 || QTEL_IsTimeout(hqtel->NTP.syncTick, hqtel->NTP.config.retryInterval)) {
        syncNTP(hqtel);
      }
    } else {
      if (hqtel->NTP.syncTick != 0 && QTEL_IsTimeout(hqtel->NTP.syncTick, hqtel->NTP.config.resyncInterval)) {
        syncNTP(hqtel);
      }
    }
  }
  #endif /* QTEL_EN_FEATURE_NTP */

  if (QTEL_BITS_IS(hqtel->net.events, QTEL_NET_EVENT_ON_GPRS_REGISTERED)) {
    QTEL_BITS_UNSET(hqtel->net.events, QTEL_NET_EVENT_ON_GPRS_REGISTERED);
    QTEL_Debug("[GPRS] Registered%s.", (QTEL_NET_SET_STATUS(hqtel, QTEL_NET_STATUS_GPRS_ROAMING))? " (Roaming)":"");
  }

  if (QTEL_BITS_IS(hqtel->net.events, QTEL_NET_EVENT_ON_OPENED)) {
    QTEL_BITS_UNSET(hqtel->net.events, QTEL_NET_EVENT_ON_OPENED);
    QTEL_Debug("Data online");
    QTEL_SockOnNetOpened(hqtel);
  }

  if (QTEL_BITS_IS(hqtel->net.events, QTEL_NET_EVENT_ON_CLOSED)) {
    QTEL_BITS_UNSET(hqtel->net.events, QTEL_NET_EVENT_ON_CLOSED);
    QTEL_Debug("Data offline");
  }

  if (QTEL_BITS_IS(hqtel->net.events, QTEL_NET_EVENT_ON_NTP_WAS_SYNCED)) {
    QTEL_BITS_UNSET(hqtel->net.events, QTEL_NET_EVENT_ON_NTP_WAS_SYNCED);
    if (hqtel->NTP.onSynced != 0) {
      hqtel->NTP.onSynced(QTEL_GetTime(hqtel));
    }
  }
}


QTEL_Status_t QTEL_NET_WaitOnline(QTEL_HandlerTypeDef *hqtel, uint32_t timeout)
{
  uint32_t tick = QTEL_GetTick();

  while (!QTEL_NET_IS_STATUS(hqtel, QTEL_NET_STATUS_OPEN)) {
    if (QTEL_IsTimeout(tick, timeout)) return QTEL_TIMEOUT;
    QTEL_Delay(1);
  }

  return QTEL_OK;
}

void QTEL_SetAPN(QTEL_HandlerTypeDef *hqtel,
                const char *APN, const char *user, const char *pass)
{
  hqtel->net.APN.APN   = APN;
  hqtel->net.APN.user  = 0;
  hqtel->net.APN.pass  = 0;

  if (strlen(user) > 0)
    hqtel->net.APN.user = user;
  if (strlen(pass) > 0)
    hqtel->net.APN.pass = pass;

  QTEL_NET_UNSET_STATUS(hqtel, QTEL_NET_STATUS_GPRS_REGISTERED);
  QTEL_NET_UNSET_STATUS(hqtel, QTEL_NET_STATUS_APN_WAS_SET);
}


#if QTEL_EN_FEATURE_NTP
void QTEL_SetNTP(QTEL_HandlerTypeDef *hqtel, const char *server)
{
  hqtel->NTP.server = server;
}
#endif /* QTEL_EN_FEATURE_NTP */


static uint8_t GprsCheck(QTEL_HandlerTypeDef *hqtel)
{
  uint8_t *resp = &hqtel->respTmp[0];
  // uint8_t resp_n = 0;
  uint8_t resp_stat = 0;
  uint8_t isOK = 0;

  // send command then get response;
  QTEL_LOCK(hqtel);

  memset(resp, 0, 16);
  QTEL_SendCMD(hqtel, "AT+CGREG?");
  if (QTEL_GetResponse(hqtel, "+CGREG", 5, resp, 3, QTEL_GETRESP_WAIT_OK, 2000) == QTEL_OK) {
    // resp_n = (uint8_t) atoi((char*)&resp[0]);
    resp_stat = (uint8_t) atoi((char*) resp+2);
  }
  else goto endcmd;

  // check response
  if (resp_stat == 1 || resp_stat == 5) {
    QTEL_NET_SET_STATUS(hqtel, QTEL_NET_STATUS_GPRS_REGISTERED);
    QTEL_BITS_SET(hqtel->net.events, QTEL_NET_EVENT_ON_GPRS_REGISTERED);
    if (resp_stat == 5) {
      QTEL_NET_SET_STATUS(hqtel, QTEL_NET_STATUS_GPRS_ROAMING);
    }
    isOK = 1;
  } else {
    QTEL_NET_UNSET_STATUS(hqtel, QTEL_NET_STATUS_GPRS_REGISTERED);
  }

  endcmd:
  QTEL_UNLOCK(hqtel);
  return isOK;
}


static void GprsSetAPN(QTEL_HandlerTypeDef *hqtel, uint8_t contextId,
                       const char *APN, const char *user, const char *pass)
{
  QTEL_LOCK(hqtel);

  if (user == NULL)       QTEL_SendCMD(hqtel, "AT+QICSGP=%d,3,\"%s\",\"\",\"\",0", (int) contextId, APN);
  else if (pass == NULL)  QTEL_SendCMD(hqtel, "AT+QICSGP=%d,3,\"%s\",\"%s\",\"\",3", (int) contextId, APN, user);
  else                    QTEL_SendCMD(hqtel, "AT+QICSGP=%d,3,\"%s\",\"%s\",\"%s\",3", (int) contextId, APN, user, pass);

  if (!QTEL_IsResponseOK(hqtel)) {
    goto endcmd;
  }

  hqtel->net.contextId = contextId;
  QTEL_NET_SET_STATUS(hqtel, QTEL_NET_STATUS_APN_WAS_SET);
  QTEL_NET_UNSET_STATUS(hqtel, QTEL_NET_STATUS_GPRS_REGISTERED);
  endcmd:
  QTEL_UNLOCK(hqtel);
}


static void GprsSetQoS(QTEL_HandlerTypeDef *hqtel)
{
  QTEL_LOCK(hqtel);
  QTEL_UNLOCK(hqtel);
}


static QTEL_Status_t GprsActivatePDP(QTEL_HandlerTypeDef *hqtel)
{
  QTEL_Status_t status;
  uint8_t       *resp = &hqtel->respTmp[0];
  uint8_t       respDataSz = 3;
  uint8_t       respSz = QTEL_TMP_RESP_BUFFER_SIZE/respDataSz;
  uint8_t       activedCtxId;

  QTEL_LOCK(hqtel);
  if (hqtel->net.contextId == 0) {
    status = QTEL_ERROR;
    goto endcmd;
  }
  if (respSz > 16) respSz = 16;

  QTEL_Debug("getting online data");
  QTEL_SendCMD(hqtel, "AT+QIACT?");
  status = QTEL_GetMultipleResponse(hqtel, "+QIACT", 6,
                                    resp, respSz, respDataSz,
                                    QTEL_GETRESP_WAIT_OK, 1000);
  if (status == QTEL_OK) {
    while(respSz--) {
      if (*resp == 0) break;
      activedCtxId = (uint8_t) atoi((char*) resp);
      if (activedCtxId == hqtel->net.contextId) {
        QTEL_NET_SET_STATUS(hqtel, QTEL_NET_STATUS_OPEN);
        QTEL_BITS_SET(hqtel->net.events, QTEL_NET_EVENT_ON_OPENED);
        goto endcmd;
      }
      resp += respDataSz;
    }
  }

  QTEL_NET_SET_STATUS(hqtel, QTEL_NET_STATUS_OPENING);
  QTEL_SendCMD(hqtel, "AT+QIACT=%d", (int) hqtel->net.contextId);
  status = QTEL_GetResponse(hqtel, NULL, 0, NULL, 0, QTEL_GETRESP_WAIT_OK, 150000);
  if (status != QTEL_OK) {
    QTEL_NET_UNSET_STATUS(hqtel, QTEL_NET_STATUS_OPENING);
    goto endcmd;
  }
  QTEL_NET_SET_STATUS(hqtel, QTEL_NET_STATUS_OPEN);
  QTEL_BITS_SET(hqtel->net.events, QTEL_NET_EVENT_ON_OPENED);

  endcmd:
  QTEL_UNLOCK(hqtel);
  return status;
}


#if QTEL_EN_FEATURE_NTP
static void syncNTP(QTEL_HandlerTypeDef *hqtel)
{
  uint8_t *resp = &hqtel->respTmp[0];

  hqtel->NTP.syncTick = QTEL_GetTick();
  QTEL_NET_UNSET_STATUS(hqtel, QTEL_NET_STATUS_NTP_WAS_SYNCED);
  QTEL_NET_SET_STATUS(hqtel, QTEL_NET_STATUS_NTP_WAS_SYNCING);

  if (hqtel->NTP.server == NULL) return;

  QTEL_LOCK(hqtel);

  memset(resp, 0, 5);
  QTEL_SendCMD(hqtel, "AT+QNTP=%d,\"%s\",123", (int) hqtel->net.contextId, hqtel->NTP.server);
  if (!QTEL_IsResponseOK(hqtel)) {
    goto endcmd;
  }

  endcmd:
  QTEL_UNLOCK(hqtel);
}
#endif /* QTEL_EN_FEATURE_NTP */
#endif /* QTEL_EN_FEATURE_NET */
