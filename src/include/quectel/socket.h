/*
 * simnet.h
 *
 *  Created on: Aug 2, 2022
 *      Author: janoko
 */

#ifndef QTEL_QUECTEL_EC25_SIMSOCK_H_
#define QTEL_QUECTEL_EC25_SIMSOCK_H_

#include "conf.h"
#if QTEL_EN_FEATURE_SOCKET

#include "../quectel.h"
#include <buffer.h>

#define QTEL_SOCK_DEFAULT_TO 2000

#define QTEL_SOCK_UDP    0
#define QTEL_SOCK_TCPIP  1

#define QTEL_SOCK_STATE_CLOSED   0x00
#define QTEL_SOCK_STATE_OPENING  0x01
#define QTEL_SOCK_STATE_OPEN     0x02

#define QTEL_SOCK_SUCCESS 0
#define QTEL_SOCK_ERROR   1

#define QTEL_SOCK_EVENT_ON_OPENED        0x01
#define QTEL_SOCK_EVENT_ON_OPENING_ERROR 0x02
#define QTEL_SOCK_EVENT_ON_RECEIVED      0x04
#define QTEL_SOCK_EVENT_ON_CLOSED        0x08
#define QTEL_SOCK_EVENT_ON_CLOSED_BY_SVR 0x10

#define QTEL_SOCK_IS_STATE(sock, stat)    ((sock)->state == stat)
#define QTEL_SOCK_SET_STATE(sock, stat)   ((sock)->state = stat)

#define QTEL_SOCK_CFG_KEYS_NUM 14

typedef enum {
  // key                        // <valueTypedata>/*factor/unit
  QTEL_SockCfg_TransPktSZ,      // <uint16_t> byte
  QTEL_SockCfg_TransWaitTM,     // <uint16_t> *100 ms
  QTEL_SockCfg_DataFormat,      // <byte[<send_format>,<send_format>]>
  QTEL_SockCfg_ViewMode,        // <boolean>  | 0: recv data: <dataheader>\\r\\n<data> / 1: recv data: <dataheader>,<data>   
  QTEL_SockCfg_TCP_RetransCfg,  // <uint16_t[<max_backoffs>,<max_rto *100ms]>
  QTEL_SockCfg_DNS_Cache,       // <boolean>  | 0: disable/1: enable DNS cache
  QTEL_SockCfg_Qisend_TO,       // <uint16_t> | <cmd_send> '>' [wait time: ms] <data>
  QTEL_SockCfg_PassiveClosed,   // <boolean>  | 0: disable/1: enable DNS cache
  QTEL_SockCfg_UDP_ReadMode,    // <byte>     | 0: disable blockmode / 1: enable stream mode
  QTEL_SockCfg_UDP_SendMode,    // <byte>     | 0: disable blockmode / 1: enable stream mode
  QTEL_SockCfg_TCP_Accept,      // <boolean>  | 0: disable/1: enable auto accepting incoming client connection
  QTEL_SockCfg_TCP_KeepAlive,   // <byte[<0: disable/1 : enable>,<idle_time>,<interval_time>,<probe_cnt>]>
  QTEL_SockCfg_RecvBufferSZ,    // <uint16_t> byte
  QTEL_SockCfg_RecvInd,         // <boolean>  | 0: disable/1: enable
} QTEL_Sock_ConfigKey_t;


typedef struct {
  QTEL_HandlerTypeDef  *hqtel;
  uint8_t             state;
  uint8_t             events;               // Events flag
  int8_t              linkNum;
  uint8_t             type;                 // QTEL_SOCK_UDP or QTEL_SOCK_TCPIP

  // configuration
  struct {
    uint32_t timeout;
    uint8_t  autoReconnect;
    uint16_t reconnectingDelay;
  } config;

  // tick register for delay and timeout
  struct {
    uint32_t reconnDelay;
    uint32_t connecting;
  } tick;

  // server
  char     host[64];
  uint16_t port;

  // listener
  struct {
    void (*onConnecting)(void);
    void (*onConnected)(void);
    void (*onConnectError)(void);
    void (*onClosed)(void);
    void (*onReceived)(Buffer_t*);
  } listeners;

  // buffer
  Buffer_t buffer;
} QTEL_Socket_t;

uint8_t QTEL_SockCheckAsyncResponse(QTEL_HandlerTypeDef*);
void    QTEL_SockHandleEvents(QTEL_HandlerTypeDef*);

// glabal event handler
void    QTEL_SockOnStarted(QTEL_HandlerTypeDef*);
void    QTEL_SockOnNetOpened(QTEL_HandlerTypeDef*);

// quectel feature net and socket
QTEL_Status_t QTEL_SockConfig(QTEL_HandlerTypeDef*, QTEL_Sock_ConfigKey_t, void *value);
QTEL_Status_t QTEL_SockOpenTCPIP(QTEL_HandlerTypeDef*, int8_t *linkNum, const char *host, uint16_t port);
QTEL_Status_t QTEL_SockClose(QTEL_HandlerTypeDef*, uint8_t linkNum);
void          QTEL_SockRemoveListener(QTEL_HandlerTypeDef*, uint8_t linkNum);
uint16_t      QTEL_SockSendData(QTEL_HandlerTypeDef*, int8_t linkNum, const uint8_t *data, uint16_t length);

// socket method
QTEL_Status_t  QTEL_SOCK_Init(QTEL_Socket_t*, const char *host, uint16_t port);
void          QTEL_SOCK_SetBuffer(QTEL_Socket_t*, uint8_t *buffer, uint16_t size);
QTEL_Status_t  QTEL_SOCK_Open(QTEL_Socket_t*, QTEL_HandlerTypeDef*);
void          QTEL_SOCK_Close(QTEL_Socket_t*);
uint16_t      QTEL_SOCK_SendData(QTEL_Socket_t*, const uint8_t *data, uint16_t length);

#endif /* QTEL_EN_FEATURE_SOCKET */
#endif /* QTEL_QUECTEL_EC25_SIMSOCK_H_ */
