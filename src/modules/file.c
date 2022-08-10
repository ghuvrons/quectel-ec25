/*
 * file.c
 *
 *  Created on: Aug 5, 2022
 *      Author: janoko
 */

#include "../include/quectel/file.h"
#include "../include/quectel/utils.h"
#include <stdlib.h>

static char* StorageStr[QTEL_File_Storage_MAX]= {
    "",
    "RAM",
    "SD"
};


QTEL_Status_t QTEL_File_Upload(QTEL_HandlerTypeDef *hqtel,
                               QTEL_File_Storage_t storage,
                               const char *filename,
                               uint8_t *data,
                               uint16_t dataLen)
{
  QTEL_Status_t status = QTEL_ERROR;
  uint8_t *resp = &hqtel->respTmp[0];

  QTEL_LOCK(hqtel);
  QTEL_SendCMD(hqtel,
               "AT+QFUPL=\"%s%s%s\",%d,60,1",
               StorageStr[storage], (storage>0)?":":"", filename,
               dataLen);

  if (!QTEL_WaitResponse(hqtel, "CONNECT", 7, 5000))
    goto endcmd;

  while (dataLen) {
    if (!QTEL_SendData(hqtel, data, (dataLen < 1024)? dataLen:1024))
      goto endcmd;

    if (dataLen < 1024) {
      memset(resp, 0, 24);
      if (QTEL_GetResponse(hqtel, "+QFUPL", 6, resp, 24, QTEL_GETRESP_WAIT_OK, 5000) != QTEL_OK)
        goto endcmd;
      break;
    }
    else {
      if (!QTEL_WaitResponse(hqtel, "A", 1, 5000))
        goto endcmd;
      dataLen -= 1024;
      data += 1024;
    }
  }
  status = QTEL_OK;

  endcmd:
  QTEL_UNLOCK(hqtel);
  return status;
}


QTEL_Status_t QTEL_File_Open(QTEL_HandlerTypeDef *hqtel, QTEL_File_t *hfile,
                             QTEL_File_Storage_t storage, const char *filename)
{
  QTEL_Status_t status  = QTEL_ERROR;
  uint8_t       *resp   = &hqtel->respTmp[0];
  char          *strTmp = (char*) &hqtel->respTmp[24];

  QTEL_LOCK(hqtel);

  hfile->length = 0;

  // get file description
  QTEL_SendCMD(hqtel,
               "AT+QFLST=\"%s%s%s\"",
               StorageStr[storage], (storage>0)?":":"", filename);

  memset(resp, 0, 24);
  memset(strTmp, 0, 8);
  status = QTEL_GetResponse(hqtel, "+QFLST", 6, resp, 24, QTEL_GETRESP_WAIT_OK, 5000);
  if (status == QTEL_OK) {
    QTEL_ParseStr(resp, ',', 1, (uint8_t*) strTmp);
    hfile->length = (uint32_t) atoi(strTmp);
  }

  // open file
  QTEL_SendCMD(hqtel,
               "AT+QFOPEN=\"%s%s%s\",0",
               StorageStr[storage], (storage>0)?":":"", filename);

  memset(resp, 0, 24);
  status = QTEL_GetResponse(hqtel, "+QFOPEN", 7, resp, 24, QTEL_GETRESP_WAIT_OK, 5000);
  if (status != QTEL_OK)
    goto endcmd;

  hfile->hqtel = hqtel;
  hfile->fileno = (uint32_t) atoi((char*)resp);
  hfile->pos = 0;
  status = QTEL_OK;

  endcmd:
  QTEL_UNLOCK(hqtel);
  return status;
}


int32_t QTEL_File_Write(QTEL_File_t *hfile, const uint8_t *srcData, uint16_t dataLen)
{
  QTEL_Status_t status    = QTEL_ERROR;
  uint16_t      writelen  = 0;
  uint8_t       *resp     = &hfile->hqtel->respTmp[0];
  char          *strTmp   = (char*)&hfile->hqtel->respTmp[16];
  const uint8_t *nextBuf;

  if (hfile->hqtel == NULL) return -1;

  QTEL_LOCK(hfile->hqtel);
  QTEL_SendCMD(hfile->hqtel,"AT+QFWRITE=%u,%u", hfile->fileno, dataLen);

  status = QTEL_GetResponse(hfile->hqtel, "CONNECT", 7, NULL, 0, QTEL_GETRESP_ONLY_DATA, 0);
  if (status != QTEL_OK)
    goto endcmd;

  memset(resp, 0, 16);
  QTEL_SendData(hfile->hqtel, srcData, dataLen);
  status = QTEL_GetResponse(hfile->hqtel, "+QFWRITE", 8, resp, 16, QTEL_GETRESP_WAIT_OK, 0);
  if (status != QTEL_OK)
    goto endcmd;

  memset(strTmp, 0, 16);
  nextBuf = QTEL_ParseStr(resp, ',', 0, (uint8_t*)strTmp);
  writelen = (uint16_t) atoi(strTmp);

  QTEL_ParseStr(nextBuf, ',', 0, (uint8_t*)strTmp);
  hfile->length = (uint16_t) atoi(strTmp);

  status = QTEL_OK;

  endcmd:
  QTEL_UNLOCK(hfile->hqtel);
  if (status != QTEL_OK) return -1;
  return (int32_t) writelen;
}


int32_t QTEL_File_Read(QTEL_File_t *hfile, uint8_t *dstBuf, uint16_t bufSz)
{
  QTEL_Status_t status  = QTEL_ERROR;
  uint16_t      readLen = 0;
  uint16_t      availableLen = 0;
  char          *strTmp = (char*) &hfile->hqtel->respTmp[0];

  if (hfile->hqtel == NULL) return -1;
  if (hfile->pos >= hfile->length) return 0;

  QTEL_LOCK(hfile->hqtel);
  QTEL_SendCMD(hfile->hqtel,"AT+QFREAD=%u,%u", hfile->fileno, bufSz);

  status = QTEL_GetResponse(hfile->hqtel, "CONNECT", 7, NULL, 0, QTEL_GETRESP_ONLY_DATA, 0);
  if (status != QTEL_OK)
    goto endcmd;

  memset(strTmp, 0, 24);
  QTEL_ParseStr(&hfile->hqtel->respBuffer[8], ',', 0, (uint8_t*) strTmp);
  availableLen = (uint16_t) atoi(strTmp);

  readLen = QTEL_GetData(hfile->hqtel, dstBuf, (availableLen < bufSz)? availableLen:bufSz, 1000);
  if (!QTEL_IsResponseOK(hfile->hqtel)) goto endcmd;
  hfile->pos += readLen;

  endcmd:
  QTEL_UNLOCK(hfile->hqtel);
  if (status != QTEL_OK) return -1;
  return (int32_t) readLen;
}


QTEL_Status_t QTEL_File_Seek(QTEL_File_t *hfile, uint32_t offset)
{
  QTEL_Status_t status = QTEL_ERROR;

  if (hfile->hqtel == NULL) return status;

  QTEL_LOCK(hfile->hqtel);
  QTEL_SendCMD(hfile->hqtel, "AT+QFSEEK=%u,%u,0", hfile->fileno, offset);
  if (!QTEL_IsResponseOK(hfile->hqtel)) goto endcmd;
  hfile->pos = offset;
  status = QTEL_OK;

  endcmd:
  QTEL_UNLOCK(hfile->hqtel);
  return status;
}


QTEL_Status_t QTEL_File_Close(QTEL_File_t *hfile)
{
  QTEL_Status_t status = QTEL_ERROR;

  if (hfile->hqtel == NULL) return status;

  QTEL_LOCK(hfile->hqtel);
  QTEL_SendCMD(hfile->hqtel,"AT+QFCLOSE=%u", hfile->fileno);
  if (!QTEL_IsResponseOK(hfile->hqtel)) goto endcmd;
  status = QTEL_OK;

  endcmd:
  QTEL_UNLOCK(hfile->hqtel);
  return status;
}


QTEL_Status_t QTEL_File_Delete(QTEL_HandlerTypeDef *hqtel, QTEL_File_Storage_t storage, const char *filename)
{
  QTEL_Status_t status = QTEL_ERROR;

  QTEL_LOCK(hqtel);
  QTEL_SendCMD(hqtel, "AT+QFDEL=\"%s%s%s\"", StorageStr[storage], (storage>0)?":":"", filename);
  if (!QTEL_IsResponseOK(hqtel)) goto endcmd;
  status = QTEL_OK;

  endcmd:
  QTEL_UNLOCK(hqtel);
  return status;
}
