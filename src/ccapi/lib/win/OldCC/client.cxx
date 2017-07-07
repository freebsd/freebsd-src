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

#include "stdio.h"    // KPKDBG

#include "ccs_request.h"

#include "ccapi.h"
#include "util.h"

extern "C" {
#include "cci_debugging.h"
#include "tls.h"    // KPKDBG
    }

#include "client.h"
#include "init.hxx"
#include "name.h"
#include "secure.hxx"

#define SECONDS_TO_WAIT 10

#define STARTUP "CLIENT STARTUP: "
#define DISCONNECT "CLIENT DISCONNECT: "

bool Client::s_init = false;
CcOsLock Client::sLock;

static DWORD bind_client(char* ep OPTIONAL, Init::InitInfo& info, LPSTR* endpoint) {
    DWORD status = 0;
    unsigned char * pszStringBinding = NULL;

    if (!ep) {
        status = alloc_name(endpoint, "ep", isNT());
        } 
    else {
        *endpoint = ep;
        }

    if (!status) {
        /* Use a convenience function to concatenate the elements of */
        /* the string binding into the proper sequence.              */
        status = RpcStringBindingCompose(0,            // uuid
                                         (unsigned char*)"ncalrpc",    // protseq
                                         0,            // address
                                         (unsigned char*)(*endpoint),  // endpoint
                                         0,            // options
                                         &pszStringBinding);
        cci_check_error(status);
        }
            
    if (!status) {
        /* Set the binding handle that will be used to bind to the server. */
        status = RpcBindingFromStringBinding(pszStringBinding, &ccs_request_IfHandle);
        cci_check_error(status);
        }

    if (!status) {
        // Win9x might call RpcBindingSetAuthInfo (not Ex), but it does not
        // quite work on Win9x...
        if (isNT()) {
            RPC_SECURITY_QOS qos;
            qos.Version = RPC_C_SECURITY_QOS_VERSION;
            qos.Capabilities = RPC_C_QOS_CAPABILITIES_DEFAULT;
            qos.IdentityTracking = RPC_C_QOS_IDENTITY_STATIC;
            qos.ImpersonationType = RPC_C_IMP_LEVEL_IDENTIFY;

            status = info.fRpcBindingSetAuthInfoEx(ccs_request_IfHandle,
                                                   0, // principal
                                                   RPC_C_AUTHN_LEVEL_CONNECT,
                                                   RPC_C_AUTHN_WINNT,
                                                   0, // current address space
                                                   RPC_C_AUTHZ_NAME,
                                                   &qos);
            cci_check_error(status);
            }
        }

    if (pszStringBinding) {
        DWORD status = RpcStringFree(&pszStringBinding); 
        cci_check_error(status);
        }
    return cci_check_error(status);
    }

DWORD find_server(Init::InitInfo& info, LPSTR endpoint) {
    DWORD               status      = 0;
    LPSTR               event_name  = 0;
    HANDLE              hEvent      = 0;
    SECURITY_ATTRIBUTES sa          = { 0 };
    PSECURITY_ATTRIBUTES psa        = 0;
    STARTUPINFO         si          = { 0 };
    PROCESS_INFORMATION pi          = { 0 };
    char*               szExe       = 0;
    char*               szDir       = 0;
    BOOL                bRes        = FALSE;
    char*               cmdline     = NULL;
#if 0
    HANDLE hToken = 0;
#endif

    psa = isNT() ? &sa : 0;

    cci_debug_printf("%s Looking for server; ccs_request_IfHandle:0x%X", __FUNCTION__, ccs_request_IfHandle);
    status = cci_check_error(RpcMgmtIsServerListening(ccs_request_IfHandle));
    if (status == RPC_S_NOT_LISTENING) {
        cci_debug_printf("  Server *NOT* found!");
        si.cb = sizeof(si);

        status = alloc_module_dir_name(CCAPI_DLL, &szDir);

        if (!status) {
            status = alloc_module_dir_name_with_file(CCAPI_DLL, CCAPI_EXE, &szExe);
            }

        if (!status) {
            status = alloc_name(&event_name, "startup", isNT());
            cci_check_error(status);
            }

        if (!status) {
            if (isNT()) {
                sa.nLength = sizeof(sa);
                status = alloc_own_security_descriptor_NT(&sa.lpSecurityDescriptor);
                cci_check_error(status);
                }
            }

        if (!status) {
            hEvent = CreateEvent(psa, FALSE, FALSE, event_name);
            cci_debug_printf("  CreateEvent(... %s) returned hEvent 0x%X", event_name, hEvent);
            if (!hEvent) status = GetLastError();
            }

        if (!status) {

#if 0
            if (SecureClient::IsImp()) {
                cci_debug_printf(STARTUP "Token is impersonation token"));
                SecureClient::DuplicateImpAsPrimary(hToken);
                } 
            else {
                cci_debug_printf(STARTUP "Token is NOT impersonation token"));
                }
#endif

#if 0
            if (hToken)
                bRes = CreateProcessAsUser(hToken,
                                       szExe, // app name
                                       NULL, // cmd line
                                       psa, // SA
                                       psa, // SA
                                       FALSE, 
                                       CREATE_NEW_PROCESS_GROUP | 
                                       //CREATE_NEW_CONSOLE |
                                       NORMAL_PRIORITY_CLASS |
                                       // CREATE_NO_WINDOW |
                                       DETACHED_PROCESS |
                                       0
                                       ,
                                       NULL, // environment
                                       szDir, // current dir
                                       &si,
                                       &pi);
            else
#endif
                alloc_cmdline_2_args(szExe, endpoint, "-D", &cmdline);
                bRes = CreateProcess(  szExe,       // app name
                                       NULL, //cmdline,     // cmd line is <server endpoint -[DC]>
                                       psa,         // SA
                                       psa,         // SA
                                       FALSE, 
                                       CREATE_NEW_PROCESS_GROUP | 
                                       NORMAL_PRIORITY_CLASS |
#ifdef CCAPI_LAUNCH_SERVER_WITH_CONSOLE
                                       CREATE_NEW_CONSOLE |
#else
                                       DETACHED_PROCESS |
#endif
                                       0,
                                       NULL,        // environment
                                       szDir,       // current dir
                                       &si,
                                       &pi);
            if (!bRes) {
                status = GetLastError();
                cci_debug_printf("  CreateProcess returned %d; LastError: %d", bRes, status);
                }
            cci_debug_printf("  Waiting...");
            }
        cci_check_error(status);

        if (!status) {
            status = WaitForSingleObject(hEvent, (SECONDS_TO_WAIT)*1000);
            status = RpcMgmtIsServerListening(ccs_request_IfHandle);
            }
        } 
    else if (status) {
            cci_debug_printf("  unexpected error while looking for server: 0D%d / 0U%u / 0X%X", status, status, status);
            } 

#if 0
    if (hToken)
        CloseHandle(hToken);
#endif
    if (szDir)                      free_alloc_p(&szDir);
    if (szExe)                      free_alloc_p(&szExe);
    if (hEvent)                     CloseHandle(hEvent);
    if (pi.hThread)                 CloseHandle(pi.hThread);
    if (pi.hProcess)                CloseHandle(pi.hProcess);
    if (sa.lpSecurityDescriptor)    free_alloc_p(&sa.lpSecurityDescriptor);
    return cci_check_error(status);

}

static
DWORD
make_random_challenge(DWORD *challenge_out) {
    HCRYPTPROV provider;
    DWORD status = 0;
    *challenge_out = 0;
    if (!CryptAcquireContext(&provider, NULL, NULL, PROV_RSA_FULL,
                             CRYPT_VERIFYCONTEXT)) {
        status = GetLastError();
        cci_check_error(status);
        return status;
        }
    if (!CryptGenRandom(provider, sizeof(*challenge_out),
                        (BYTE *)challenge_out)) {
        status = GetLastError();
        cci_check_error(status);
        return status;
        }
    if (!CryptReleaseContext(provider, 0)) {
        /*
         * Note: even though CryptReleaseContext() failed, we don't really
         * care since a) we've already successfully obtained our challenge
         * anyway and b) at least one of the potential errors, "ERROR_BUSY"
         * does not really seem to be an error at all.  So GetLastError() is
         * logged for informational purposes only and should not be returned.
         */
        cci_check_error(GetLastError());
        }
    return status;
}

static
DWORD
authenticate_server(Init::InitInfo& info) {
    DWORD               challenge, desired_response;
    HANDLE              hMap            = 0;
    LPSTR               mem_name        = 0;
    PDWORD              pvalue          = 0;
    CC_UINT32           response        = 0;
    SECURITY_ATTRIBUTES sa              = { 0 };
    DWORD               status          = 0;

    cci_debug_printf("%s entry", __FUNCTION__);

    status = alloc_name(&mem_name, "auth", isNT());
    cci_check_error(status);

    if (!status) {
        status = make_random_challenge(&challenge);
        desired_response = challenge + 1;
        cci_check_error(status);
        }

    if (!status) {
        if (isNT()) {
            sa.nLength = sizeof(sa);
            status = alloc_own_security_descriptor_NT(&sa.lpSecurityDescriptor);
            }
        }
    cci_check_error(status);

    if (!status) {
        hMap = CreateFileMapping(INVALID_HANDLE_VALUE, isNT() ? &sa : 0, 
                                 PAGE_READWRITE, 0, sizeof(DWORD), mem_name);
        if (!hMap)
        status = GetLastError();
        }
    cci_check_error(status);

    if (!status) {
        pvalue = (PDWORD)MapViewOfFile(hMap, FILE_MAP_ALL_ACCESS, 0, 0, 0);
        if (!pvalue) status = GetLastError();
        }
    cci_check_error(status);

    if (!status) {
        *pvalue = challenge;

        RpcTryExcept {
            response = ccs_authenticate( (CC_CHAR*)mem_name );
            }
        RpcExcept(1) {
            status = RpcExceptionCode();
            cci_check_error(status);
            }
        RpcEndExcept;
        }
    cci_check_error(status);

    if (!status) {
        // Check response
        if ((response != desired_response) && (*pvalue != desired_response)) {
            cci_debug_printf("  Could not authenticate server.");
            status = ERROR_ACCESS_DENIED; // XXX - CO_E_NOMATCHINGSIDFOUND?
            } 
        else {
            cci_debug_printf("  Server authenticated!");
            }
        cci_check_error(status);
        }

    free_alloc_p(&mem_name);
    free_alloc_p(&sa.lpSecurityDescriptor);
    if (pvalue) {
        BOOL ok = UnmapViewOfFile(pvalue);
//        DEBUG_ASSERT(ok);
        }
    if (hMap) CloseHandle(hMap);
    return status;
}

DWORD
Client::Disconnect() {
    DWORD status = 0;
    if (ccs_request_IfHandle) {
        /*  The calls to the remote procedures are complete. */
        /*  Free the binding handle           */
        status = RpcBindingFree(&ccs_request_IfHandle);
        }
    s_init  = false;
    return status;
    }

DWORD
Client::Connect(char* ep OPTIONAL) {
    LPSTR   endpoint    = 0;
    DWORD   status      = 0;

    if (!ccs_request_IfHandle) {
        Init::InitInfo info;

        status = Init::Info(info);
        cci_check_error(status);

        if (!status) {
            status = bind_client(ep, info, &endpoint);
            cci_check_error(status);
            }

        if (!status) {
            status = find_server(info, endpoint);
            cci_check_error(status);
            }

        if (!status) {
            status = authenticate_server(info);
            cci_check_error(status);
            }
        }


    if (endpoint && (endpoint != ep)) free_alloc_p(&endpoint);
    
    if (status) Client::Disconnect();
    return status;
    }

DWORD Client::Initialize(char* ep OPTIONAL) {
    CcAutoTryLock AL(Client::sLock);
    if (!AL.IsLocked() || s_init)
        return 0;
    SecureClient s;
    ccs_request_IfHandle  = NULL;
    DWORD status = Client::Connect(ep);
    if (!status) s_init = true;
    return status;
    }

DWORD Client::Cleanup() {
    CcAutoLock AL(Client::sLock);
    SecureClient s;
    return Client::Disconnect();
    }

DWORD Client::Reconnect(char* ep OPTIONAL) {
    CcAutoLock AL(Client::sLock);
    SecureClient s;
    DWORD status = 0;

    if (Initialized()) {
        DWORD status = Client::Cleanup();
        }
    if ( (!status) ) {
        status = Client::Initialize(ep);
        }

    return status;
    }
