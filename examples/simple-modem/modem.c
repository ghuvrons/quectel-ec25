/*
 * modem.c
 *
 *  Created on: Dec 15, 2021
 *      Author: janoko
 */

#include "modem.h"
#include <quectel.h>
#include <quectel/net.h>
#include <quectel/gps.h>
#include <quectel/utils.h>
#include <os.h>
#include <utils/debugger.h>
#include <dma_streamer.h>
#include <string.h>

// STM32 dependency
#include <usart.h>
#include <main.h>

// init
MDM_HandlerTypedef          Mod_Modem;
static bool                 isEventReset;
static STRM_handlerTypeDef  mdm_hdma_streamer = {.huart = &huart3};

// buffers
static uint8_t MDM_TxBuffer[MDM_TX_BUFFER_SZ];
static uint8_t MDM_RxBuffer[MDM_RX_BUFFER_SZ];
static uint8_t MDM_GpsBuffer[MDM_GPS_BUFFER_SZ];

// Serial Methods
static uint8_t  serial_isAvailable(void *serialDev);
static uint16_t serial_read(void *serialDev, uint8_t *dstBuf, uint16_t bufSz, uint32_t timeout);
static uint16_t serial_readline(void *serialDev, uint8_t *dstBuf, uint16_t bufSz, uint32_t timeout);
static uint16_t serial_forwardToBuffer(void *serialDev, Buffer_t *buf, uint16_t bufSz, uint32_t timeout);
static void     serial_unread(void *serialDev, uint16_t len);
static uint16_t serial_write(void *serialDev, const uint8_t *data, uint16_t len);

#if RTC_ENABLE
static void onNTPSynced(QTEL_Datetime);
#endif


// Debug Methods
void QTEL_Printf(const char *format, ...)
{
  va_list args;
  va_start (args, format); DBG_vPrintf(format, args);  va_end (args);
}

void QTEL_Println(const char *format, ...)
{
  va_list args;
  va_start (args, format); DBG_vPrintln(format, args); va_end (args);
}


#if USE_FREERTOS
extern osMutexId_t ModemMutexHandle;
void QTEL_LockCMD(void) {   osMutexAcquire(ModemMutexHandle, osWaitForever); }
void QTEL_UnlockCMD(void) { osMutexRelease(ModemMutexHandle); }
#endif


bool MDM_Init(void)
{
  // reset register memory
  memset(&Mod_Modem, 0, sizeof(MDM_HandlerTypedef));

  // streamer init
  STRM_Init(&mdm_hdma_streamer, &(MDM_TxBuffer[0]), MDM_TX_BUFFER_SZ, &(MDM_RxBuffer[0]), MDM_RX_BUFFER_SZ);
  OS_Delay(10);

  /*** Configure ***/
  // Mutex implement
  Mod_Modem.driver.lock                   = QTEL_LockCMD;
  Mod_Modem.driver.unlock                 = QTEL_UnlockCMD;

  // Serial method implement to modem driver
  Mod_Modem.driver.serial.device          = &mdm_hdma_streamer;
  Mod_Modem.driver.serial.isAvailable     = serial_isAvailable;
  Mod_Modem.driver.serial.read            = serial_read;
  Mod_Modem.driver.serial.readline        = serial_readline;
  Mod_Modem.driver.serial.forwardToBuffer = serial_forwardToBuffer;
  Mod_Modem.driver.serial.unread          = serial_unread;
  Mod_Modem.driver.serial.write           = serial_write;

  // setup NTP
  Mod_Modem.driver.NTP.server                 = CONF_NTP_SERVER;
  Mod_Modem.driver.NTP.config.resyncInterval  = 24 * 3600 * 1000;
  Mod_Modem.driver.NTP.config.retryInterval   = 10 * 1000;

  #if RTC_ENABLE
  Mod_Modem.driver.NTP.onSynced = onNTPSynced;
  #endif

  // setup APN
  QTEL_SetAPN(&Mod_Modem.driver, CONF_APN, CONF_APN_USER, CONF_APN_PASS);

  // setup GPS buffer
  QTEL_GPS_Init(&Mod_Modem.driver, &MDM_GpsBuffer[0], MDM_GPS_BUFFER_SZ);

  // driver init
  if (QTEL_Init(&Mod_Modem.driver) != QTEL_OK)
    Error_Handler();

  // Set power pin on
  HAL_GPIO_WritePin(GPIOB, Modem_Pwr_Pin, GPIO_PIN_SET);
  OS_Delay(100);

  return false;
}


void MDM_Run(void *args)
{
  reset:
  MDM_Init();
  isEventReset = false;

  /* Infinite loop */
  for(;;)
  {
    if (isEventReset) goto reset;
    QTEL_CheckAnyResponse(&(Mod_Modem.driver));

    if (QTEL_IsTimeout(Mod_Modem.delayConfigs.signalUpdate.tick,
                      Mod_Modem.delayConfigs.signalUpdate.timeout))
    {
      Mod_Modem.delayConfigs.signalUpdate.tick = OS_GetTick();
      QTEL_GetSignal(&(Mod_Modem.driver));
      QTEL_Println("signal: %d", Mod_Modem.driver.signal);
    }

    OS_Delay(1);
  }
}


void MDM_Reset(void)
{
  isEventReset = true;
  while (isEventReset) OS_Delay(10);
}

bool MDM_WaitReady(uint32_t timeout)
{
  uint32_t tick = OS_GetTick();

  while(!QTEL_IS_STATUS(&Mod_Modem.driver, QTEL_STATUS_ACTIVE)) {
    if (timeout == 0 || OS_IsTimeout(tick, timeout)) {
      return false;
    }
    OS_Delay(10);
  }

  return true;
}

#if QTEL_EN_FEATURE_GPS
bool MDM_IsGPSWeakSignal(void)
{
  return (Mod_Modem.driver.gps.lwgps.dop_h <= 0.0f || Mod_Modem.driver.gps.lwgps.dop_h > 2.0f);
}
#endif



/** Static methods **/

static uint8_t serial_isAvailable(void *serialDev) {
  return STRM_IsReadable((STRM_handlerTypeDef*)serialDev);
}
static uint16_t serial_read(void *serialDev, uint8_t *dstBuf, uint16_t bufSz, uint32_t timeout) {
  return STRM_Read((STRM_handlerTypeDef*)serialDev, dstBuf, bufSz, timeout);
}
static uint16_t serial_readline(void *serialDev, uint8_t *dstBuf, uint16_t bufSz, uint32_t timeout) {
  return STRM_Readline((STRM_handlerTypeDef*)serialDev, dstBuf, bufSz, timeout);
}
static uint16_t serial_forwardToBuffer(void *serialDev, Buffer_t *buf, uint16_t bufSz, uint32_t timeout) {
  return STRM_ReadToBuffer((STRM_handlerTypeDef*)serialDev, buf, bufSz, timeout);
}
static void serial_unread(void *serialDev, uint16_t len) {
  return STRM_Unread((STRM_handlerTypeDef*)serialDev, len);
}
static uint16_t serial_write(void *serialDev, const uint8_t *data, uint16_t len) {
  return STRM_Write((STRM_handlerTypeDef*)serialDev, data, len, STRM_BREAK_NONE);
}


#if RTC_ENABLE
static void onNTPSynced(QTEL_Datetime simDT)
{
  Datetime_t result;
  memcpy(&result, &simDT, sizeof(Datetime_t));
}
#endif
