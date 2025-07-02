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
#include "secure.hxx"

extern "C" {
#include "cci_debugging.h"
    }

CcOsLock SecureClient::s_lock;
DWORD SecureClient::s_refcount = 0;
DWORD SecureClient::s_error = 0;
HANDLE SecureClient::s_hToken = 0;

#include "util.h"

#define SC "SecureClient::"

DWORD
SecureClient::Attach(
    )
{
    CcAutoLock AL(s_lock);
    if (s_hToken) {
        s_refcount++;
        return 0;
    }
    if (OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, 
                         &s_hToken)) {
        s_refcount++;
        s_error = 0;
    } else {
        s_hToken = 0;
        s_error = GetLastError();
    }
    return s_error;
}

DWORD
SecureClient::Detach(
    )
{
    CcAutoLock AL(s_lock);
    s_refcount--;
    if (s_refcount) return 0;
    if (!s_hToken) return 0;
    DWORD error = 0;
    if (!CloseHandle(s_hToken))
        error = GetLastError();
    s_hToken = 0;
    s_error = 0;
    return error;
}

DWORD SecureClient::Token(HANDLE& hToken) {
    // This function will not do automatic initialization.
    CcAutoLock AL(s_lock);
    hToken = 0;
    if (!s_hToken) {
        cci_debug_printf("%s no process token initialized (%u)", __FUNCTION__, s_error);
        return s_error ? s_error : ERROR_INVALID_HANDLE;
        } 
    else {
        DWORD status = 0;
        if (!DuplicateHandle(GetCurrentProcess(), s_hToken, 
                             GetCurrentProcess(), &hToken, 0, FALSE, 
                             DUPLICATE_SAME_ACCESS)) {
            status = GetLastError();
            cci_debug_printf("  Could not duplicate handle (%u)", status);
            }
        return status;
        }
    }

void
SecureClient::Start(SecureClient*& s) {
    s = new SecureClient;
}

void
SecureClient::Stop(SecureClient*& s) {
    delete s;
    s = 0;
}

///////////////////////////////////////////////////////////////////////////////

/* This constructor turns off impersonation.
 * It is OK for OpenThreadToken to return an error -- that just means impersonation
 * is off.
 */
SecureClient::SecureClient():
    m_Error(0),
    m_hToken(0),
    m_NeedRestore(false) {

    HANDLE hThread = GetCurrentThread();
    HANDLE hThDuplicate;
    
    int status  = DuplicateHandle(  GetCurrentProcess(),
                                    hThread,
                                    GetCurrentProcess(),
                                    &hThDuplicate,
                                    TOKEN_ALL_ACCESS,
                                    FALSE,
                                    0);
    if (!status) return;

    if (!OpenThreadToken(hThDuplicate, TOKEN_ALL_ACCESS, FALSE, &m_hToken)) {
        m_Error = GetLastError();
        return;
        }
    if (SetThreadToken(&hThDuplicate, NULL)) {
        m_NeedRestore = true;
    } else {
        m_Error = GetLastError();
        }
    CloseHandle(hThDuplicate);
    }

SecureClient::~SecureClient() {
    if (m_NeedRestore) {
        HANDLE hThread = GetCurrentThread();
        if (!SetThreadToken(&hThread, m_hToken)) {
            m_Error = cci_check_error(GetLastError());
            }
        }
    if (m_hToken) {
        if (!CloseHandle(m_hToken)) {
            m_Error = cci_check_error(GetLastError());
            }
        }
    }

DWORD SecureClient::Error() {
    return m_Error;
    }