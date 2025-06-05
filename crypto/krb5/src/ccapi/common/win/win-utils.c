/* ccapi/common/win/win-utils.c */
/*
 * Copyright 2008 Massachusetts Institute of Technology.
 * All Rights Reserved.
 *
 * Export of this software from the United States of America may
 * require a specific license from the United States Government.
 * It is the responsibility of any person or organization contemplating
 * export to obtain such a license before exporting.
 *
 * WITHIN THAT CONSTRAINT, permission to use, copy, modify, and
 * distribute this software and its documentation for any purpose and
 * without fee is hereby granted, provided that the above copyright
 * notice appear in all copies and that both that copyright notice and
 * this permission notice appear in supporting documentation, and that
 * the name of M.I.T. not be used in advertising or publicity pertaining
 * to distribution of the software without specific, written prior
 * permission.  Furthermore if you modify this software you must label
 * your software as modified software and not distribute it in such a
 * fashion that it might be confused with the original M.I.T. software.
 * M.I.T. makes no representations about the suitability of
 * this software for any purpose.  It is provided "as is" without express
 * or implied warranty.
 */

#include <stdio.h>
#include <string.h>
#include <time.h>
#include "windows.h"
#include <stdlib.h>
#include <malloc.h>


#include "win-utils.h"
#include "cci_debugging.h"

#pragma warning (disable : 4996)

#define UUID_SIZE   128

char*           clientPrefix        = "CCAPI_CLIENT_";
char*           serverPrefix        = "CCS_LISTEN_";
unsigned char*  pszProtocolSequence = "ncalrpc";

#define         MAX_TIMESTAMP   40
char            _ts[MAX_TIMESTAMP];

char* clientEndpoint(const char* UUID) {
    char* _clientEndpoint   = (char*)malloc(strlen(UUID) + strlen(clientPrefix) + 2);
    strcpy(_clientEndpoint, clientPrefix);
    strncat(_clientEndpoint, UUID, UUID_SIZE);
//    cci_debug_printf("%s returning %s", __FUNCTION__, _clientEndpoint);
    return _clientEndpoint;
    }

char* serverEndpoint(const char* user) {
    char* _serverEndpoint   = (char*)malloc(strlen(user) + strlen(serverPrefix) + 2);
    strcpy(_serverEndpoint, serverPrefix);
    strncat(_serverEndpoint, user, UUID_SIZE);
    return _serverEndpoint;
    }

char* timestamp() {
    SYSTEMTIME  _stime;
    GetSystemTime(&_stime);
    GetTimeFormat(LOCALE_SYSTEM_DEFAULT, 0, &_stime, "HH:mm:ss", _ts, sizeof(_ts)-1);
    return _ts;
    }
