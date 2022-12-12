/*
 * simnet.c
 *
 *  Created on: Aug 2, 2022
 *      Author: janoko
 */



#include "../include/quectel.h"
#include "../include/quectel/net.h"
#include "../include/quectel/socket.h"
#include "../include/quectel/utils.h"
#include "../include/quectel/debug.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>


#define QTEL_MAX_NUM_OF_SOCKET  12

#if QTEL_EN_FEATURE_SOCKET

// event handlers
static QTEL_Status_t  setTCPDefaultConfiguration(QTEL_HandlerTypeDef*);
static void           resetOpenedSocket(QTEL_HandlerTypeDef*);
static void           receiveData(QTEL_HandlerTypeDef*);
static QTEL_Status_t  sockOpen(QTEL_Socket_t*);

static char* keyStr[QTEL_SOCK_CFG_KEYS_NUM] = {
  "transpktsize",
  "transwaittm",
  "dataformat",
  "viewmode",
  "tcp/retranscfg",
  "dns/cache",
  "qisend/timeout",
  "passiveclosed",
  "udp/readmode",
  "udp/sendmode",
  "tcp/accept",
  "tcp/keepalive",
  "recv/buffersize",
  "recvind",
};

#define Get_Available_LinkNum(hqtel, connId) {\
  for (int16_t i = 0; i < QTEL_NUM_OF_SOCKET && i < QTEL_MAX_NUM_OF_SOCKET; i++) {\
    if ((hqtel)->net.sockets[i] == NULL) {\
      *(connId) = i;\
      break;\
    }\
  }\
}


uint8_t QTEL_SockCheckAsyncResponse(QTEL_HandlerTypeDef *hqtel)
{
  // uint8_t ctxId;
  int8_t        connId;
  QTEL_Socket_t *socket;
  uint8_t       isGet   = 0;
  uint16_t      err;
  char          *strTmp = (char*) &hqtel->respTmp[0];
  const uint8_t *nextBuf;

  // handle URC
  if ((isGet = QTEL_IsResponse(hqtel, "+QIURC", 6))) {
    memset(strTmp, 0, 6);
    nextBuf = QTEL_ParseStr(&hqtel->respBuffer[8], ',', 0, (uint8_t*) strTmp);
    if (strncmp(strTmp, "recv", 4) == 0) {
      receiveData(hqtel);
    }
    else if (strncmp(strTmp, "closed", 6) == 0) {
      memset(strTmp, 0, 3);
      nextBuf = QTEL_ParseStr(nextBuf, ',', 0, (uint8_t*) strTmp);
      connId = (int8_t) atoi(strTmp);
      // int reason  =          atoi((char*)&(hqtel->respBuffer[12]));

      socket = (QTEL_Socket_t*) hqtel->net.sockets[connId];
      if (socket != NULL) {
        QTEL_BITS_SET(socket->events, QTEL_SOCK_EVENT_ON_CLOSED_BY_SVR);
        QTEL_BITS_SET(socket->events, QTEL_SOCK_EVENT_ON_CLOSED);
        QTEL_SOCK_SET_STATE(socket, QTEL_SOCK_STATE_CLOSED);
      }
    }
    else return 0;
  }

  else if ((isGet = (hqtel->respBufferLen >= 11 && QTEL_IsResponse(hqtel, "+QIOPEN", 7))))
  {
    memset(strTmp, 0, 3);
    nextBuf = QTEL_ParseStr(&hqtel->respBuffer[9], ',', 0, (uint8_t*) strTmp);
    connId  = (uint16_t) atoi(strTmp);

    memset(strTmp, 0, 4);
    nextBuf = QTEL_ParseStr(nextBuf, ',', 0, (uint8_t*) strTmp);
    err     = (uint16_t) atoi(strTmp);

    socket = (QTEL_Socket_t*) hqtel->net.sockets[connId];
    if (socket != NULL) {
      if (err == 0) {
        QTEL_BITS_SET(socket->events, QTEL_SOCK_EVENT_ON_OPENED);
        QTEL_SOCK_SET_STATE(socket, QTEL_SOCK_STATE_OPEN);
      } else {
        QTEL_BITS_SET(socket->events, QTEL_SOCK_EVENT_ON_OPENING_ERROR);
        QTEL_SOCK_SET_STATE(socket, QTEL_SOCK_STATE_CLOSED);
      }
    }
  }

  return isGet;
}


void QTEL_SockHandleEvents(QTEL_HandlerTypeDef *hqtel)
{
  int16_t       i;
  QTEL_Socket_t *socket;

  // Socket Event Handler
  for (i = 0; QTEL_NET_IS_STATUS(hqtel, QTEL_NET_STATUS_OPEN) && i < QTEL_NUM_OF_SOCKET; i++)
  {
    if ((socket = hqtel->net.sockets[i]) != NULL) {
      if (QTEL_BITS_IS(socket->events, QTEL_SOCK_EVENT_ON_OPENED)) {
        QTEL_BITS_UNSET(socket->events, QTEL_SOCK_EVENT_ON_OPENED);
        if (socket->listeners.onConnected != NULL)
          socket->listeners.onConnected();
      }

      if (QTEL_BITS_IS(socket->events, QTEL_SOCK_EVENT_ON_OPENING_ERROR)) {
        QTEL_BITS_UNSET(socket->events, QTEL_SOCK_EVENT_ON_OPENING_ERROR);
        if (socket->config.autoReconnect) {
          socket->tick.reconnDelay = QTEL_GetTick();
          if (socket->listeners.onConnectError != NULL)
            socket->listeners.onConnectError();
        }
      }

      if (QTEL_BITS_IS(socket->events, QTEL_SOCK_EVENT_ON_CLOSED_BY_SVR)) {
        QTEL_BITS_UNSET(socket->events, QTEL_SOCK_EVENT_ON_CLOSED_BY_SVR);
        QTEL_SOCK_Close(socket);
      }

      if (QTEL_BITS_IS(socket->events, QTEL_SOCK_EVENT_ON_CLOSED)) {
        QTEL_BITS_UNSET(socket->events, QTEL_SOCK_EVENT_ON_CLOSED);
        if (!socket->config.autoReconnect)
          hqtel->net.sockets[i] = NULL;
        else socket->tick.reconnDelay = QTEL_GetTick();
        if (socket->listeners.onClosed != NULL)
          socket->listeners.onClosed();
      }

      if (QTEL_BITS_IS(socket->events, QTEL_SOCK_EVENT_ON_RECEIVED)) {
        QTEL_BITS_UNSET(socket->events, QTEL_SOCK_EVENT_ON_RECEIVED);
        if (socket->listeners.onReceived != NULL)
          socket->listeners.onReceived(&(socket->buffer));
      }

      // auto reconnect
      if (QTEL_SOCK_IS_STATE(socket, QTEL_SOCK_STATE_CLOSED)) {
        if (QTEL_IsTimeout(socket->tick.reconnDelay, socket->config.reconnectingDelay)) {
          sockOpen(socket);
        }
      }
    }
  }
}


void QTEL_SockOnStarted(QTEL_HandlerTypeDef *hqtel)
{
  QTEL_Socket_t *socket;

  for (uint8_t i = 0; i < QTEL_NUM_OF_SOCKET; i++) {
    if ((socket = hqtel->net.sockets[i]) != NULL) {
      if (QTEL_SOCK_IS_STATE(socket, QTEL_SOCK_STATE_OPENING)) {
        if (socket->config.autoReconnect) {
          socket->tick.reconnDelay = QTEL_GetTick();
          if (socket->listeners.onConnectError != NULL)
            socket->listeners.onConnectError();
        }
      }

      else if (QTEL_SOCK_IS_STATE(socket, QTEL_SOCK_STATE_OPEN)) {
        if (!socket->config.autoReconnect)
          hqtel->net.sockets[i] = NULL;
        if (socket->listeners.onClosed != NULL)
          socket->listeners.onClosed();
      }
      QTEL_SOCK_SET_STATE(socket, 0);
    }
  }
}


void QTEL_SockOnNetOpened(QTEL_HandlerTypeDef *hqtel)
{
  setTCPDefaultConfiguration(hqtel);
  resetOpenedSocket(hqtel);
}


QTEL_Status_t QTEL_SockConfig(QTEL_HandlerTypeDef *hqtel, QTEL_Sock_ConfigKey_t key, void *value)
{
  QTEL_Status_t status = QTEL_ERROR;

  QTEL_LOCK(hqtel);

  switch (key) {
  case QTEL_SockCfg_DataFormat:
    // <send_format>,<recv_format>
    QTEL_SendCMD(hqtel, "AT+QICFG=\"%s\",%u,%u", keyStr[key], *(uint8_t*)value, *(((uint8_t*)value)+1));
  case QTEL_SockCfg_TCP_RetransCfg:
    // <max_backoffs>,<max_rto*100ms>
    QTEL_SendCMD(hqtel, "AT+QICFG=\"%s\",%u,%u", keyStr[key], *(uint16_t*)value, *(((uint16_t*)value)+1));
    break;
  case QTEL_SockCfg_TransPktSZ:
  case QTEL_SockCfg_TransWaitTM:
  case QTEL_SockCfg_Qisend_TO:
  case QTEL_SockCfg_RecvBufferSZ:
    QTEL_SendCMD(hqtel, "AT+QICFG=\"%s\",%u", keyStr[key], *(uint16_t*)value);
    break;
  default:
    QTEL_SendCMD(hqtel, "AT+QICFG=\"%s\",%u", keyStr[key], *(uint8_t*)value);
    break;
  }

  if (!QTEL_IsResponseOK(hqtel)) goto endcmd;
  status = QTEL_OK;

  endcmd:
  QTEL_UNLOCK(hqtel);
  return status;
}


/**
 * return linknum if connected
 * return -1 if not connected
 */
QTEL_Status_t QTEL_SockOpenTCPIP(QTEL_HandlerTypeDef *hqtel, int8_t *connId, const char *host, uint16_t port)
{
  QTEL_Status_t status = QTEL_ERROR;

  if (!QTEL_NET_IS_STATUS(hqtel, QTEL_NET_STATUS_OPEN) || !QTEL_NET_IS_STATUS(hqtel, QTEL_NET_STATUS_AVAILABLE))
  {
    return QTEL_ERROR;
  }
  
  if (*connId == -1 || hqtel->net.sockets[*connId] == NULL) {
    Get_Available_LinkNum(hqtel, connId);
    if (*connId == -1) return QTEL_ERROR;
  }

  QTEL_LOCK(hqtel);
  QTEL_SendCMD(hqtel, 
               "AT+QIOPEN=%u,%d,\"TCP\",\"%s\",%d,0,1", 
               (uint) hqtel->net.contextId, (uint) *connId, host, port);

  QTEL_SOCK_SET_STATE((QTEL_Socket_t*)hqtel->net.sockets[*connId], QTEL_SOCK_STATE_OPENING);

  if (!QTEL_IsResponseOK(hqtel)) {
    QTEL_SOCK_SET_STATE((QTEL_Socket_t*)hqtel->net.sockets[*connId], QTEL_SOCK_STATE_CLOSED);
    goto endcmd;
  }
  status = QTEL_OK;

  endcmd:
  QTEL_UNLOCK(hqtel);
  return status;
}


QTEL_Status_t QTEL_SockClose(QTEL_HandlerTypeDef *hqtel, uint8_t connId)
{
  QTEL_Status_t status  = QTEL_ERROR;
  uint8_t       *resp   = &hqtel->respTmp[0];
  QTEL_Socket_t *socket;

  QTEL_LOCK(hqtel);

  memset(resp, 0, 20);
  QTEL_SendCMD(hqtel, "AT+QICLOSE=%u,2", (uint) connId); // timeout in 2sec

  if (!QTEL_IsResponseOK(hqtel)) goto endcmd;

  socket = (QTEL_Socket_t*) hqtel->net.sockets[connId];
  if (socket != NULL) {
    QTEL_BITS_SET(socket->events, QTEL_SOCK_EVENT_ON_CLOSED);
    QTEL_SOCK_SET_STATE(socket, QTEL_SOCK_STATE_CLOSED);
  }
  status = QTEL_OK;

  endcmd:
  QTEL_UNLOCK(hqtel);
  return status;
}


uint16_t QTEL_SockSendData(QTEL_HandlerTypeDef *hqtel, int8_t connId, const uint8_t *data, uint16_t length)
{
  uint16_t  sendLen = 0;
  uint8_t   *cmdTmp = &hqtel->cmdTmp[0];

  QTEL_LOCK(hqtel);

  sprintf((char*) cmdTmp, "AT+QISEND=%d,%d\r", (int) connId, (int) length);
  QTEL_SendData(hqtel, cmdTmp, strlen((char*)cmdTmp));
  if (!QTEL_WaitResponse(hqtel, ">", 1, 3000))
    goto endcmd;
  if (!QTEL_SendData(hqtel, data, length))
    goto endcmd;
  if (QTEL_GetResponse(hqtel, "SEND OK", 7, 0, 0, QTEL_GETRESP_ONLY_DATA, 5000) == QTEL_OK) {
    sendLen = length;
  }
  else {
    sendLen = 0;
  }

  endcmd:
  QTEL_UNLOCK(hqtel);
  return sendLen;
}


QTEL_Status_t QTEL_SOCK_Init(QTEL_Socket_t *sock, const char *host, uint16_t port)
{
  char *sockIP = sock->host;
  while (*host != '\0') {
    *sockIP = *host;
    host++;
    sockIP++;
  }

  sock->port = port;

  if (sock->config.timeout == 0)
    sock->config.timeout = QTEL_SOCK_DEFAULT_TO;
  if (sock->config.reconnectingDelay == 0)
    sock->config.reconnectingDelay = 5000;

  if (sock->buffer.buffer == NULL || sock->buffer.size == 0)
    return QTEL_ERROR;

  QTEL_SOCK_SET_STATE(sock, QTEL_SOCK_STATE_CLOSED);
  return QTEL_OK;
}


void QTEL_SOCK_SetBuffer(QTEL_Socket_t *sock, uint8_t *buffer, uint16_t size)
{
  sock->buffer.buffer = buffer;
  sock->buffer.size = size;
}


QTEL_Status_t QTEL_SOCK_Open(QTEL_Socket_t *sock, QTEL_HandlerTypeDef *qtelPtr)
{
  QTEL_Status_t status;
  sock->linkNum = -1;

  if (sock->config.autoReconnect) {
    Get_Available_LinkNum(hqtel, &(sock->linkNum));
    if (sock->linkNum < 0) return QTEL_ERROR;
    hqtel->net.sockets[sock->linkNum] = (void*)sock;
    sock->qtel = qtelPtr;
  }

  status = sockOpen(sock);
  if (status != QTEL_OK && !sock->config.autoReconnect) {
    qtelPtr->net.sockets[sock->linkNum] = NULL;
    sock->linkNum = -1;
  }

  return status;
}


static QTEL_Status_t sockOpen(QTEL_Socket_t *sock)
{
  if (QTEL_SockOpenTCPIP(sock->hqtel, &sock->linkNum, sock->host, sock->port) == QTEL_OK) {
    if (sock->listeners.onConnecting != NULL) sock->listeners.onConnecting();
    return QTEL_OK;
  }
  QTEL_SOCK_SET_STATE(sock, QTEL_SOCK_STATE_CLOSED);

  return QTEL_ERROR;
}


void QTEL_SOCK_Close(QTEL_Socket_t *sock)
{
  QTEL_SockClose(sock->hqtel, sock->linkNum);
}


uint16_t QTEL_SOCK_SendData(QTEL_Socket_t *sock, const uint8_t *data, uint16_t length)
{
  if (!QTEL_SOCK_IS_STATE(sock, QTEL_SOCK_STATE_OPEN)) return 0;
  return QTEL_SockSendData(sock->hqtel, sock->linkNum, data, length);
}


static QTEL_Status_t setTCPDefaultConfiguration(QTEL_HandlerTypeDef *hqtel)
{
  QTEL_Status_t status        = QTEL_ERROR;
  uint16_t      transPktSZ    = 1024;
  uint16_t      transWaitTM   = 2;
  uint8_t       dataformat[2] = {0,0};
  uint8_t       viewMode      = 0;
  uint16_t      retransCfg[2] = {12,600};
  uint8_t       dnsCache      = 1;
  uint16_t      qisendTO      = 0;
  uint8_t       passiveClosed = 0;
  uint8_t       udpReadMode   = 0;
  uint8_t       udpSendMode   = 0;
  uint8_t       tcpAccept     = 1;
  uint8_t       tcpKeppAlive  = 0;
  uint16_t      recvBufferSz  = 10240;
  uint8_t       recvInd       = 0;

  status = QTEL_SockConfig(hqtel, QTEL_SockCfg_TransPktSZ,     &transPktSZ);
  if (status != QTEL_OK) goto endcmd;

  status = QTEL_SockConfig(hqtel, QTEL_SockCfg_TransWaitTM,    &transWaitTM);
  if (status != QTEL_OK) goto endcmd;

  status = QTEL_SockConfig(hqtel, QTEL_SockCfg_DataFormat,     &dataformat[0]);
  if (status != QTEL_OK) goto endcmd;

  status = QTEL_SockConfig(hqtel, QTEL_SockCfg_ViewMode,       &viewMode);
  if (status != QTEL_OK) goto endcmd;

  status = QTEL_SockConfig(hqtel, QTEL_SockCfg_TCP_RetransCfg, &retransCfg[0]);
  if (status != QTEL_OK) goto endcmd;

  status = QTEL_SockConfig(hqtel, QTEL_SockCfg_DNS_Cache,      &dnsCache);
  if (status != QTEL_OK) goto endcmd;

  status = QTEL_SockConfig(hqtel, QTEL_SockCfg_Qisend_TO,      &qisendTO);
  if (status != QTEL_OK) goto endcmd;

  status = QTEL_SockConfig(hqtel, QTEL_SockCfg_PassiveClosed,  &passiveClosed);
  if (status != QTEL_OK) goto endcmd;

  status = QTEL_SockConfig(hqtel, QTEL_SockCfg_UDP_ReadMode,   &udpReadMode);
  if (status != QTEL_OK) goto endcmd;

  status = QTEL_SockConfig(hqtel, QTEL_SockCfg_UDP_SendMode,   &udpSendMode);
  if (status != QTEL_OK) goto endcmd;

  status = QTEL_SockConfig(hqtel, QTEL_SockCfg_TCP_Accept,     &tcpAccept);
  if (status != QTEL_OK) goto endcmd;

  status = QTEL_SockConfig(hqtel, QTEL_SockCfg_TCP_KeepAlive,  &tcpKeppAlive);
  if (status != QTEL_OK) goto endcmd;

  status = QTEL_SockConfig(hqtel, QTEL_SockCfg_RecvBufferSZ,   &recvBufferSz);
  if (status != QTEL_OK) goto endcmd;

  status = QTEL_SockConfig(hqtel, QTEL_SockCfg_RecvInd,        &recvInd);
  if (status != QTEL_OK) goto endcmd;
  
  status = QTEL_OK;
  endcmd:
  QTEL_UNLOCK(hqtel);
  return status;
}


static void resetOpenedSocket(QTEL_HandlerTypeDef *hqtel)
{
  uint8_t *resp         = &hqtel->respTmp[0];
  uint8_t respDataSz    = 10;
  uint8_t respSz        = QTEL_TMP_RESP_BUFFER_SIZE/respDataSz;
  uint8_t connId;

  if (respSz > 12) respSz = 12;

  QTEL_LOCK(hqtel);
  memset(resp, 0, 20);

  QTEL_SendCMD(hqtel, "AT+QISTATE");
  if (QTEL_GetMultipleResponse(hqtel, "+QISTATE", 8,
                               resp, respSz, respDataSz,
                               QTEL_GETRESP_WAIT_OK, 1000) == QTEL_OK)
  {
    while(respSz--) {
      if (*resp == 0) break;
      connId = (uint8_t) atoi((char*) resp);
      QTEL_SendCMD(hqtel, "AT+QICLOSE=%u,2", (uint) connId); // timeout in 2sec
      if (!QTEL_IsResponseOK(hqtel)){}
      resp += respDataSz;
    }
    QTEL_NET_SET_STATUS(hqtel, QTEL_NET_STATUS_AVAILABLE);
  }

  QTEL_UNLOCK(hqtel);
}


static void receiveData(QTEL_HandlerTypeDef *hqtel)
{
  const uint8_t *nextBuf      = NULL;
  uint8_t       *connId_str   = &hqtel->respTmp[0];
  uint8_t       *dataLen_str  = &hqtel->respTmp[8];
  uint8_t       connId;
  uint16_t      dataLen;
  uint16_t      writeLen;
  QTEL_Socket_t *socket;

  memset(connId_str, 0, 2);
  memset(dataLen_str, 0, 6);

  // skip string "+QIURC: \"recv\"," and read next data
  nextBuf = QTEL_ParseStr(&hqtel->respBuffer[15], ',', 0, connId_str);
  QTEL_ParseStr(nextBuf, ',', 0, dataLen_str);
  connId = (uint8_t) atoi((char*) connId_str);
  dataLen = (uint16_t) atoi((char*) dataLen_str);

  if (connId < QTEL_NUM_OF_SOCKET && hqtel->net.sockets[connId] != NULL) {
    socket = (QTEL_Socket_t*) hqtel->net.sockets[connId];
    while (dataLen) {
      if (dataLen > socket->buffer.size)  writeLen = socket->buffer.size;
      else                                writeLen = dataLen;

      if (hqtel->serial.forwardToBuffer(hqtel->serial.device, &socket->buffer, writeLen, 5000) > 0)
        break;

      dataLen -= writeLen;

      if (socket->listeners.onReceived != NULL)
        socket->listeners.onReceived(&(socket->buffer));
    }

    QTEL_BITS_SET(socket->events, QTEL_SOCK_EVENT_ON_RECEIVED);
  }
}


#endif /* QTEL_EN_FEATURE_SOCKET */
