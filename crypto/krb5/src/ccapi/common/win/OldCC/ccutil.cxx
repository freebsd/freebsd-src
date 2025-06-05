/*
 * $Header$
 *
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
#include "init.hxx"
#include "secure.hxx"

extern "C" {
#include "cci_debugging.h"
    }


CcOsLock Init::s_lock;
DWORD Init::s_refcount = 0;
DWORD Init::s_error = ERROR_INVALID_HANDLE;
bool Init::s_init = false;
Init::InitInfo Init::s_info = { 0 };
HINSTANCE Init::s_hRpcDll = 0;

#define INIT "INIT: "

static
void
ShowInfo(
    Init::InitInfo& info
    );

DWORD
Init::Info(
    InitInfo& info
    )
{
    // This function will not do automatic initialization.
    CcAutoLock AL(s_lock);
    if (!s_init) {
        memset(&info, 0, sizeof(info));
        return s_error ? s_error : ERROR_INVALID_HANDLE;
    } else {
        info = s_info;
        return 0;
    }
}

DWORD
Init::Initialize() {
    CcAutoLock AL(s_lock);
    cci_debug_printf("%s s_init:%d", __FUNCTION__, s_init);
    if (s_init) {
        s_refcount++;
        return 0;
        }
    SecureClient s;
    DWORD status = 0;
    OSVERSIONINFO osvi;
    BOOL isSupportedVersion = FALSE;
    memset(&s_info, 0, sizeof(s_info));
    memset(&osvi, 0, sizeof(osvi));
    osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);

    status = !GetVersionEx(&osvi);       // Returns a boolean.  Invert to 0 is OK.

    if (!status) {
        switch(osvi.dwPlatformId) {
        case VER_PLATFORM_WIN32_WINDOWS:
            s_info.isNT = FALSE;
            isSupportedVersion = TRUE;
            break;
        case VER_PLATFORM_WIN32_NT:
            s_info.isNT = TRUE;
            isSupportedVersion = TRUE;
            break;
        case VER_PLATFORM_WIN32s:
        default:
            s_info.isNT = FALSE;
            break;
            }
    
        if (!isSupportedVersion) {
            cci_debug_printf("%s Trying to run on an unsupported version of Windows", __FUNCTION__);
            status  = 1;
            }
        }

    if (!status) {status  = !s_info.isNT;}

    if (!status) {status = !(s_hRpcDll = LoadLibrary(TEXT("rpcrt4.dll")));}

    if (!status) {
        s_info.fRpcBindingSetAuthInfoEx = (FP_RpcBindingSetAuthInfoEx)
            GetProcAddress(s_hRpcDll, TEXT(FN_RpcBindingSetAuthInfoEx));
        if (!s_info.fRpcBindingSetAuthInfoEx) {
            cci_debug_printf("  Running on NT but could not find RpcBindinSetAuthInfoEx");
            status = 1;
            }
        }
    
    if (!status) {
        s_info.fRpcServerRegisterIfEx = (FP_RpcServerRegisterIfEx)
            GetProcAddress(s_hRpcDll, TEXT(FN_RpcServerRegisterIfEx));
        if (!s_info.fRpcServerRegisterIfEx) {
            cci_debug_printf("  Running on NT but could not find RpcServerRegisterIfEx");
            status = 1;
            }
        }

    if (!status) {
        status = SecureClient::Attach();
        if (status) {
            cci_debug_printf("  SecureClient::Attach() failed (%u)", status);
            }
        }

    if (status) {
        memset(&s_info, 0, sizeof(s_info));
        if (s_hRpcDll) {
            FreeLibrary(s_hRpcDll);
            s_hRpcDll = 0;
        }
        cci_debug_printf("  Init::Attach() failed (%u)", status);
    } else {
        s_refcount++;
        s_init = true;
        ShowInfo(s_info);
    }
    s_error = status;
    return status;
}

DWORD
Init::Cleanup(
    )
{
    CcAutoLock AL(s_lock);
    s_refcount--;
    if (s_refcount) return 0;
    if (!s_init) return 0;
    DWORD error = 0;
    if (s_hRpcDll) {
        FreeLibrary(s_hRpcDll);
        s_hRpcDll = 0;
    }
    error = SecureClient::Detach();
    memset(&s_info, 0, sizeof(s_info));
    s_init = false;
    s_error = 0;
    if (error) {
        cci_debug_printf("  Init::Detach() had an error (%u)", error);
        }
    return error;
}

static
void
ShowInfo(
    Init::InitInfo& info
    )
{
    if (info.isNT) {
        cci_debug_printf("  Running on Windows NT using secure mode");
    } else {
        cci_debug_printf("  Running insecurely on non-NT Windows");
    }
    return;
}
