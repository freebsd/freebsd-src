/* ccapi/common/win/OldCC/ccutils.c */
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

#include <windows.h>
#include <stdlib.h>
#include <malloc.h>

#include "cci_debugging.h"
#include "util.h"

BOOL isNT() {
    OSVERSIONINFO osvi;
    DWORD   status              = 0;
    BOOL    bSupportedVersion   = FALSE;
    BOOL    bIsNT               = FALSE;

    memset(&osvi, 0, sizeof(osvi));
    osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);

    status = !GetVersionEx(&osvi);       // Returns a boolean.  Invert to 0 is OK.

    if (!status) {
        switch(osvi.dwPlatformId) {
        case VER_PLATFORM_WIN32_WINDOWS:
            bIsNT = FALSE;
            bSupportedVersion = TRUE;
            break;
        case VER_PLATFORM_WIN32_NT:
            bIsNT = TRUE;
            bSupportedVersion = TRUE;
            break;
        case VER_PLATFORM_WIN32s:
        default:
            bIsNT = FALSE;
            break;
            }

        if (!bSupportedVersion) {
            cci_debug_printf("%s Running on an unsupported version of Windows", __FUNCTION__);
            status  = 1;
            }
        }

    return (!status && bIsNT && bSupportedVersion);
    }

char*   allocEventName(char* uuid_string, char* suffix) {
    LPSTR       event_name      = NULL;
    cc_int32    err             = ccNoError;

    event_name = malloc(strlen(uuid_string) + strlen(suffix) + 3);
    if (!event_name) err = cci_check_error(ccErrNoMem);

    if (!err) {
        strcpy(event_name, uuid_string);
        strcat(event_name, "_");
        strcat(event_name, suffix);
        }

    return event_name;
    }

HANDLE createThreadEvent(char* uuid, char* suffix) {
    LPSTR                   event_name  = NULL;
    HANDLE                  hEvent      = NULL;
    PSECURITY_ATTRIBUTES    psa         = 0;        // Everything having to do with
    SECURITY_ATTRIBUTES     sa          = { 0 };    // sa, psa, security is copied
    DWORD                   status      = 0;        // from the previous implementation.

    psa = isNT() ? &sa : 0;

    if (isNT()) {
        sa.nLength = sizeof(sa);
        status = alloc_own_security_descriptor_NT(&sa.lpSecurityDescriptor);
        cci_check_error(status);
        }

    if (!status) {
        event_name = allocEventName(uuid, suffix);
        if (!event_name) status = cci_check_error(ccErrNoMem);
        }
    if (!status) {
        hEvent = CreateEvent(psa, FALSE, FALSE, event_name);
        if (!hEvent)     status = cci_check_error(GetLastError());
        }

    if (!status) ResetEvent(hEvent);


    if (event_name) free(event_name);
    if (isNT())     free(sa.lpSecurityDescriptor);

    return hEvent;
    }

HANDLE openThreadEvent(char* uuid, char* suffix) {
    LPSTR   event_name  = NULL;
    HANDLE  hEvent      = NULL;
    DWORD   status      = 0;

    event_name = allocEventName(uuid, suffix);
    if (!event_name) status = cci_check_error(ccErrNoMem);
    if (!status) {
        hEvent = OpenEvent(EVENT_MODIFY_STATE, FALSE, event_name);
        if (!hEvent) status = cci_check_error(GetLastError());
        }

    if (event_name) free(event_name);

    return hEvent;
    }
