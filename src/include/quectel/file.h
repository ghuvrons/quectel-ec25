/*
 * file.h
 *
 *  Created on: Aug 5, 2022
 *      Author: janoko
 */

#ifndef QTEL_QUECTEL_EC25_FILE_H
#define QTEL_QUECTEL_EC25_FILE_H

#include "../quectel.h"

typedef enum {
  QTEL_File_Storage_UFS,
  QTEL_File_Storage_RAM,
  QTEL_File_Storage_SD,
  QTEL_File_Storage_MAX,
} QTEL_File_Storage_t;

typedef struct{
  QTEL_HandlerTypeDef* hqtel;
  uint32_t fileno;
  uint32_t length;
  uint32_t pos;
} QTEL_File_t;


QTEL_Status_t QTEL_File_Upload(QTEL_HandlerTypeDef*, QTEL_File_Storage_t, const char *filename,
                               uint8_t *data, uint16_t dataLen);
QTEL_Status_t QTEL_File_Open(QTEL_HandlerTypeDef*, QTEL_File_t*, QTEL_File_Storage_t, const char *filename);
int32_t       QTEL_File_Write(QTEL_File_t*, const uint8_t *srcData, uint16_t dataLen);
int32_t       QTEL_File_Read(QTEL_File_t*, uint8_t *dstBuf, uint16_t bufSz);
QTEL_Status_t QTEL_File_Seek(QTEL_File_t*, uint32_t offset);
QTEL_Status_t QTEL_File_Close(QTEL_File_t*);
QTEL_Status_t QTEL_File_Delete(QTEL_HandlerTypeDef*, QTEL_File_Storage_t, const char *filename);

#endif /* QTEL_QUECTEL_EC25_FILE_H */
