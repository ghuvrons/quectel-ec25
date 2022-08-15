/*
 * net.c
 *
 *  Created on: Apr 1, 2022
 *      Author: janoko
 */


#include "../include/quectel.h"
#include "../include/quectel/net.h"
#include "../include/quectel/gps.h"
#include "../include/quectel/file.h"
#include "../include/quectel/http.h"
#include "../include/quectel/utils.h"
#include "../include/quectel/debug.h"
#include <stdlib.h>
#include <string.h>
#include <buffer.h>

#if QTEL_EN_FEATURE_GPS
#define QTEL_GPS_IS_STATUS(hqtel, stat)     QTEL_BITS_IS_ALL((hqtel)->gps.status, stat)
#define QTEL_GPS_SET_STATUS(hqtel, stat)    QTEL_BITS_SET((hqtel)->gps.status, stat)
#define QTEL_GPS_UNSET_STATUS(hqtel, stat)  QTEL_BITS_UNSET((hqtel)->gps.status, stat)


static char* keyStr[QTEL_GPS_CFG_KEYS_NUM] = {
  "outport",
  "nmeasrc",
  "gpsnmeatype",
  "glonassnmeatype",
  "galileonmeatype",
  "beidounmeatype",
  "gnssconfig",
  "odpcontrol",
  "dpoenable",
  "gsvextnmeatype",
  "plane",
  "autogps",
  "suplver",
  "agpsposmode",
  "agnssprotocol",
  "fixfreq",
};

static QTEL_Status_t setGPSDefaultConfiguration(QTEL_HandlerTypeDef*);
static void gpsProcessBuffer(QTEL_HandlerTypeDef*);

#if QTEL_EN_FEATURE_HTTP
static void writeXTraFile(QTEL_HandlerTypeDef*,
                          const uint8_t *data, uint16_t dataLen, uint32_t maxDataLen);
#endif /* QTEL_EN_FEATURE_HTTP */


uint8_t QTEL_GPS_CheckAsyncResponse(QTEL_HandlerTypeDef *hqtel)
{
  uint8_t isGet = 0;

  if ((isGet = (hqtel->respBufferLen >= 12 && QTEL_IsResponse(hqtel, "+QGPSGNMEA", 10)))) {
    QTEL_BITS_SET(hqtel->gps.events, QTEL_GPS_STATE_NMEA_AVAILABLE);
    Buffer_Write(&hqtel->gps.buffer, &hqtel->respBuffer[12], hqtel->respBufferLen-12);
  }

  return isGet;
}


void QTEL_GPS_HandleEvents(QTEL_HandlerTypeDef *hqtel)
{
  if (QTEL_IS_STATUS(hqtel, QTEL_STATUS_ACTIVE)
      #if QTEL_EN_FEATURE_NET
      && QTEL_NET_IS_STATUS(hqtel, QTEL_NET_STATUS_OPEN)
      #endif
      && !QTEL_GPS_IS_STATUS(hqtel, QTEL_GPS_STATUS_ACTIVE))
  {
    if (QTEL_GPS_Deactivate(hqtel) != QTEL_OK) {
      return;
    }
    if (setGPSDefaultConfiguration(hqtel) == QTEL_OK) {
      if (QTEL_GPS_Activate(hqtel, QTEL_GPS_MODE_MS_BASED) == QTEL_OK) {
        QTEL_GPS_SET_STATUS(hqtel, QTEL_GPS_STATUS_ACTIVE);
      }
    }
  }

  if (QTEL_GPS_IS_STATUS(hqtel, QTEL_GPS_STATUS_ACTIVE) && QTEL_IsTimeout(hqtel->gps.nmeaTick, 5000)) {
    hqtel->gps.nmeaTick = QTEL_GetTick();
    QTEL_GPS_AcquireNMEA(hqtel, "GGA");
    QTEL_GPS_AcquireNMEA(hqtel, "RMC");
    QTEL_GPS_AcquireNMEA(hqtel, "GSV");
    QTEL_GPS_AcquireNMEA(hqtel, "GSA");
    QTEL_GPS_AcquireNMEA(hqtel, "VTG");
    QTEL_GPS_AcquireNMEA(hqtel, "GNS");
  }

  if (QTEL_BITS_IS(hqtel->gps.events, QTEL_GPS_STATE_NMEA_AVAILABLE)) {
    QTEL_BITS_UNSET(hqtel->gps.events, QTEL_GPS_STATE_NMEA_AVAILABLE);
    gpsProcessBuffer(hqtel);
  }
}


QTEL_Status_t QTEL_GPS_Config(QTEL_HandlerTypeDef *hqtel, QTEL_GPS_ConfigKey_t key, void *value)
{
  QTEL_Status_t status = QTEL_ERROR;

  QTEL_LOCK(hqtel);

  switch (key) {
  case QTEL_GPS_CFG_OutPort:
    QTEL_SendCMD(hqtel, "AT+QGPSCFG=\"%s\",\"%s\"", keyStr[key], (char*)value);
    break;
  case QTEL_GPS_CFG_AGNSS_Protocol:
    QTEL_SendCMD(hqtel, "AT+QGPSCFG=\"%s\",%u,%u", keyStr[key], *(uint16_t*)value, *(((uint16_t*)value)+1));
    break;
  case QTEL_GPS_CFG_AGPS_Posmode:
    QTEL_SendCMD(hqtel, "AT+QGPSCFG=\"%s\",%u", keyStr[key], *(uint32_t*)value);
    break;
  default:
    QTEL_SendCMD(hqtel, "AT+QGPSCFG=\"%s\",%u", keyStr[key], *(uint8_t*)value);
    break;
  }

  if (!QTEL_IsResponseOK(hqtel)) goto endcmd;
  status = QTEL_OK;

  endcmd:
  QTEL_UNLOCK(hqtel);
  return status;
}


void QTEL_GPS_Init(QTEL_HandlerTypeDef *hqtel, uint8_t *buffer, uint16_t bufferSize)
{
  memset(&hqtel->gps.buffer, 0, sizeof(Buffer_t));
  hqtel->gps.buffer.buffer = buffer;
  hqtel->gps.buffer.size = bufferSize;
  lwgps_init(&hqtel->gps.lwgps);
}


QTEL_Status_t QTEL_GPS_Activate(QTEL_HandlerTypeDef *hqtel, QTEL_GPS_Mode_t mode)
{
  QTEL_Status_t status = QTEL_ERROR;

  QTEL_LOCK(hqtel);
  QTEL_SendCMD(hqtel, "AT+QGPS=%d", mode);
  if (!QTEL_IsResponseOK(hqtel)) goto endcmd;
  status = QTEL_OK;

  endcmd:
  QTEL_UNLOCK(hqtel);
  return status;
}


QTEL_Status_t QTEL_GPS_Deactivate(QTEL_HandlerTypeDef *hqtel)
{
  QTEL_Status_t status = QTEL_ERROR;
  uint8_t resp;

  QTEL_LOCK(hqtel);

  QTEL_SendCMD(hqtel, "AT+QGPS?");
  if (QTEL_GetResponse(hqtel, "+QGPS", 5, &resp, 1, QTEL_GETRESP_WAIT_OK, 1000) == QTEL_OK) {
    if (resp == '1') {
      QTEL_SendCMD(hqtel, "AT+QGPSEND");
      if (!QTEL_IsResponseOK(hqtel)) goto endcmd;
    }
  }

  status = QTEL_OK;
  QTEL_GPS_UNSET_STATUS(hqtel, QTEL_GPS_STATUS_ACTIVE);
  endcmd:
  QTEL_UNLOCK(hqtel);
  return status;
}


QTEL_Status_t QTEL_GPS_SetupOneXTra(QTEL_HandlerTypeDef *hqtel)
{
  QTEL_Status_t status;
  QTEL_File_t   xtraFile;
  char          *xtraFilename   = "xtra2.bin";
  QTEL_Datetime dt;
  uint16_t      durtime = 0;
  uint8_t       *resp   = &hqtel->respTmp[0];
  uint8_t       *strTmp = &hqtel->respTmp[32];

  // activate gpsXtra
  QTEL_LOCK(hqtel);
  QTEL_SendCMD(hqtel, "AT+QGPSXTRA=1");
  if (!QTEL_IsResponseOK(hqtel)) goto endcmd;

  // check whether ctrAData was already
  memset(resp, 0, 32);
  memset(strTmp, 0, 8);
  QTEL_SendCMD(hqtel, "AT+QGPSXTRADATA?");
  status = QTEL_GetResponse(hqtel, "+QGPSXTRADATA", 13, resp, 32, QTEL_GETRESP_WAIT_OK, 2000);
  if (status == QTEL_OK) {
    QTEL_ParseStr(resp, ',', 0, strTmp);
    durtime = (uint16_t) atoi((char*)strTmp);
    if (durtime > 60) goto endcmd;
  }
  QTEL_UNLOCK(hqtel);

  #if QTEL_EN_FEATURE_HTTP
  {
    // prepare oneXtra file
    // delete existing file
    QTEL_File_Delete(hqtel, QTEL_File_Storage_UFS, xtraFilename);

    // create file
    status = QTEL_File_Open(hqtel, &xtraFile, QTEL_File_Storage_UFS, xtraFilename);
    if (status != QTEL_OK) return status;

    hqtel->gps.xtraFileTmpPtr = &xtraFile;
    for (uint8_t i = 4; i <= 6; i++) {
      sprintf((char*)strTmp, "http://xtrapath%u.izatcloud.net/xtra2.bin", i);
      status = QTEL_HTTP_Request(hqtel, QTEL_HTTP_GET, (char*)strTmp,
                                 writeXTraFile, hqtel->gps.buffer.buffer, hqtel->gps.buffer.size, 10000);
      if (status == QTEL_OK) break;
    }
    status = QTEL_File_Close(&xtraFile);
    hqtel->gps.xtraFileTmpPtr = 0;
    if (status != QTEL_OK) return status;

    //set XtraData:
    dt = QTEL_GetTime(hqtel);

    QTEL_LOCK(hqtel);
    QTEL_SendCMD(hqtel, "AT+QGPSXTRATIME=0,\"%04u/%02u/%02u,%02u:%02u:%02u\",1,1,3500",
                 2000+dt.year, dt.month, dt.day, dt.hour, dt.minute, dt.second); // SUPL server
    if (!QTEL_IsResponseOK(hqtel)) goto endcmd;

    QTEL_SendCMD(hqtel, "AT+QGPSXTRADATA=\"%s\"", xtraFilename); // SUPL server
    if (!QTEL_IsResponseOK(hqtel)) goto endcmd;

    QTEL_Delay(500);

    // recheck uploaded binary
    memset(resp, 0, 32);
    memset(strTmp, 0, 8);
    QTEL_SendCMD(hqtel, "AT+QGPSXTRADATA?"); // SUPL server
    if (QTEL_GetResponse(hqtel, "+QGPSXTRADATA", 13, resp, 32, QTEL_GETRESP_WAIT_OK, 2000)
        == QTEL_OK) {
      QTEL_ParseStr(resp, ',', 0, strTmp);
      durtime = (uint16_t) atoi((char*)strTmp);
      if (durtime > 60) goto endcmd;
      status = QTEL_ERROR;
    }

    QTEL_File_Delete(hqtel, QTEL_File_Storage_UFS, xtraFilename);
  }

  #else  // if not /* QTEL_EN_FEATURE_HTTP */
  {
    QTEL_LOCK(hqtel);
    QTEL_SendCMD(hqtel, "AT+QGPSXTRA=1");
    if (!QTEL_IsResponseOK(hqtel)) goto endcmd;
  }
  #endif /* QTEL_EN_FEATURE_HTTP */

  endcmd:
  QTEL_UNLOCK(hqtel);
  return status;
}


QTEL_Status_t QTEL_GPS_DeleteOneXTra(QTEL_HandlerTypeDef *hqtel)
{
  QTEL_Status_t status = QTEL_ERROR;

  QTEL_LOCK(hqtel);
  QTEL_SendCMD(hqtel, "AT+QGPSDEL=3");
  if (!QTEL_IsResponseOK(hqtel)) goto endcmd;
  status = QTEL_OK;

  endcmd:
  QTEL_UNLOCK(hqtel);
  return status;
}


QTEL_Status_t QTEL_GPS_getLocation(QTEL_HandlerTypeDef *hqtel)
{
  QTEL_Status_t status  = QTEL_ERROR;
  uint8_t       *resp   = &hqtel->respTmp[0];

  QTEL_LOCK(hqtel);
  memset(resp, 0, 64);

  QTEL_SendCMD(hqtel, "AT+QGPSLOC=2");
  if (QTEL_GetResponse(hqtel, "+QGPSLOC", 8, resp, 64, QTEL_GETRESP_WAIT_OK, 2000) != QTEL_OK)
    goto endcmd;
  status = QTEL_OK;

  endcmd:
  QTEL_UNLOCK(hqtel);
  return status;
}


QTEL_Status_t QTEL_GPS_AcquireNMEA(QTEL_HandlerTypeDef *hqtel, const char* nmea_type)
{
  QTEL_Status_t status = QTEL_ERROR;

  QTEL_LOCK(hqtel);
  QTEL_SendCMD(hqtel, "AT+QGPSGNMEA=\"%s\"", nmea_type);
  if (!QTEL_IsResponseOK(hqtel)) goto endcmd;
  status = QTEL_OK;

  endcmd:
  QTEL_UNLOCK(hqtel);
  return status;
}


static QTEL_Status_t setGPSDefaultConfiguration(QTEL_HandlerTypeDef *hqtel)
{
  QTEL_Status_t status  = QTEL_ERROR;

  // values
  char      *outPort          = "usbnmea";
  uint8_t   nmeaSrc           = 1;
  uint8_t   gpsNMEAType       = 0x1F;
  uint8_t   glonassNMEAType   = 0;
  uint8_t   galileoNMEAType   = 0;
  uint8_t   beidouNMEAType    = 0;
  uint8_t   gnssConfig        = 1;
  uint8_t   odpControl        = 0;
  uint8_t   dpoEnable         = 2;
  uint8_t   gsvextNMEAType    = 0;
  uint8_t   plane             = 0;
  uint8_t   autoGPS           = 0;
  uint8_t   suplVer           = 2;
  uint32_t  agpsPosmode       = 0x01FEFF7F;
  uint16_t  agnssProtocol[2]  = {0x03, 0x0507};
  uint8_t   fixFreq           = 1;


  QTEL_GPS_Config(hqtel, QTEL_GPS_CFG_OutPort, outPort);
  QTEL_GPS_Config(hqtel, QTEL_GPS_CFG_NMEA_Src, &nmeaSrc);
  QTEL_GPS_Config(hqtel, QTEL_GPS_CFG_GpsNMEA_Type, &gpsNMEAType);
  QTEL_GPS_Config(hqtel, QTEL_GPS_CFG_GlonassNMEA_Type, &glonassNMEAType);
  QTEL_GPS_Config(hqtel, QTEL_GPS_CFG_GalileoNMEA_Type, &galileoNMEAType);
  QTEL_GPS_Config(hqtel, QTEL_GPS_CFG_BeidouNMEA_Type, &beidouNMEAType);
  QTEL_GPS_Config(hqtel, QTEL_GPS_CFG_GnssConfig, &gnssConfig);
  QTEL_GPS_Config(hqtel, QTEL_GPS_CFG_ODP_Control, &odpControl);
  QTEL_GPS_Config(hqtel, QTEL_GPS_CFG_DPO_Enable, &dpoEnable);
  QTEL_GPS_Config(hqtel, QTEL_GPS_CFG_GsvextNMEA_Type, &gsvextNMEAType);
  QTEL_GPS_Config(hqtel, QTEL_GPS_CFG_Plane, &plane);
  QTEL_GPS_Config(hqtel, QTEL_GPS_CFG_AutoGps, &autoGPS);
  QTEL_GPS_Config(hqtel, QTEL_GPS_CFG_SUPL_ver, &suplVer);
  QTEL_GPS_Config(hqtel, QTEL_GPS_CFG_AGPS_Posmode, &agpsPosmode);
  QTEL_GPS_Config(hqtel, QTEL_GPS_CFG_AGNSS_Protocol, agnssProtocol);
  QTEL_GPS_Config(hqtel, QTEL_GPS_CFG_FixFreq, &fixFreq);

  QTEL_LOCK(hqtel);
  QTEL_SendCMD(hqtel, "AT+QGPSSUPLURL=\"%s\"", "supl.google.com:7276"); // SUPL server
  if (QTEL_IsResponseOK(hqtel)) {}
  QTEL_Delay(10);

  QTEL_UNLOCK(hqtel);

  status = QTEL_GPS_SetupOneXTra(hqtel);

  status = QTEL_OK;
  QTEL_UNLOCK(hqtel);
  return status;
}


static void gpsProcessBuffer(QTEL_HandlerTypeDef *hqtel)
{
  uint16_t readLen = 0;

  while (Buffer_IsAvailable(&hqtel->gps.buffer)) {
    readLen = Buffer_Read(&hqtel->gps.buffer, &hqtel->gps.readBuffer[0], QTEL_GPS_TMP_BUF_SIZE);
    lwgps_process(&hqtel->gps.lwgps, &hqtel->gps.readBuffer[0], readLen);
  }
}


#if QTEL_EN_FEATURE_HTTP
static void writeXTraFile(QTEL_HandlerTypeDef *hqtel, const uint8_t *data, uint16_t dataLen, uint32_t maxDataLen)
{
  QTEL_File_t *xtraFilePtr = (QTEL_File_t*)hqtel->gps.xtraFileTmpPtr;
  if (xtraFilePtr == NULL) return;

  QTEL_File_Write(xtraFilePtr, data, dataLen);
}
#endif /* QTEL_EN_FEATURE_HTTP */

#endif /* QTEL_EN_FEATURE_GPS */
