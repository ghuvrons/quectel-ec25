/*
 * conf.h
 *
 *  Created on: Aug 2, 2022
 *      Author: janoko
 */

#ifndef QTEL_QUECTEL_EC25_CONF_H_
#define QTEL_QUECTEL_EC25_CONF_H_

#ifndef QTEL_DEBUG
#define QTEL_DEBUG 1
#endif


#ifndef QTEL_EN_FEATURE_SOCKET
#define QTEL_EN_FEATURE_SOCKET 1

#ifndef QTEL_NUM_OF_SOCKET
#define QTEL_NUM_OF_SOCKET  4
#endif

#endif /* QTEL_EN_FEATURE_SOCKET */

#ifndef QTEL_EN_FEATURE_NTP
#define QTEL_EN_FEATURE_NTP 1
#endif

#ifndef QTEL_EN_FEATURE_HTTP
#define QTEL_EN_FEATURE_HTTP 1
#endif

#define QTEL_EN_FEATURE_NET QTEL_EN_FEATURE_NTP|QTEL_EN_FEATURE_SOCKET|QTEL_EN_FEATURE_HTTP

#ifndef QTEL_EN_FEATURE_GPS
#define QTEL_EN_FEATURE_GPS 1
#endif

#ifndef QTEL_DEBUG
#define QTEL_DEBUG 1
#endif

#ifndef QTEL_CMD_BUFFER_SIZE
#define QTEL_CMD_BUFFER_SIZE  256
#endif

#ifndef QTEL_RESP_BUFFER_SIZE
#define QTEL_RESP_BUFFER_SIZE  256
#endif

#ifndef QTEL_TMP_CMD_BUFFER_SIZE
#define QTEL_TMP_CMD_BUFFER_SIZE  128
#endif

#ifndef QTEL_TMP_RESP_BUFFER_SIZE
#define QTEL_TMP_RESP_BUFFER_SIZE  128
#endif

#if QTEL_EN_FEATURE_NTP
#ifndef QTEL_NTP_SYNC_DELAY_TIMEOUT
#define QTEL_NTP_SYNC_DELAY_TIMEOUT 10000
#endif
#endif

#ifndef LWGPS_IGNORE_USER_OPTS
#define LWGPS_IGNORE_USER_OPTS
#endif

#if QTEL_EN_FEATURE_GPS
#ifndef QTEL_GPS_TMP_BUF_SIZE
#define QTEL_GPS_TMP_BUF_SIZE  64
#endif
#endif /* QTEL_EN_FEATURE_GPS */

#endif /* QTEL_QUECTEL_EC25_CONF_H_ */
