/*
 * debug.h
 *
 *  Created on: Aug 2, 2022
 *      Author: janoko
 */


#ifndef QTEL_QUECTEL_EC25_DEBUG_H_
#define QTEL_QUECTEL_EC25_DEBUG_H_

#include "conf.h"

#if QTEL_DEBUG
#include <stdio.h>


#define QTEL_Debug(...) {QTEL_Printf("QTEL: ");QTEL_Println(__VA_ARGS__);}

void QTEL_Printf(const char *format, ...);
void QTEL_Println(const char *format, ...);

#else
#define QTEL_Debug(...) {}
#endif /* QTEL_DEBUG */
#endif /* QTEL_QUECTEL_EC25_DEBUG_H_ */
