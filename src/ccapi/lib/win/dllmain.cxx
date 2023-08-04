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

extern "C" {
#include <windows.h>
#include <LMCons.h>

#include "dllmain.h"
#include "tls.h"
#include "cci_debugging.h"
#include "ccapi_context.h"
#include "ccapi_ipc.h"
#include "client.h"

void cci_process_init__auxinit();
    }


#define CCAPI_V2_MUTEX_NAME     TEXT("MIT_CCAPI_V4_MUTEX")

// Process-specific data:
static DWORD    dwTlsIndex;
static char     _user[UNLEN+1];     // Username is used as part of the server and client endpoints.
static  HANDLE  sessionToken;
static char*    ep_prefices[]   = {"CCS", "CCAPI"};
HANDLE          hCCAPIv2Mutex   = NULL;
DWORD           firstThreadID   = 0;

// These data structures are used by the old CCAPI implementation 
//  to keep track of the state of the RPC connection.  All data is static.
static Init     init;
static Client   client;

DWORD    GetTlsIndex()  {return dwTlsIndex;}

// DllMain() is the entry-point function for this DLL. 
BOOL WINAPI DllMain(HINSTANCE hinstDLL,     // DLL module handle
                    DWORD fdwReason,        // reason called
                    LPVOID lpvReserved) {   // reserved 

    struct tspdata* ptspdata;
    BOOL            fIgnore;
    BOOL            bStatus;
    DWORD           status      = 0;        // 0 is success.
    DWORD           maxUN       = sizeof(_user);
    unsigned int    i           = 0;
    unsigned int    j           = 0;
 
    switch (fdwReason) { 
        // The DLL is loading due to process initialization or a call to LoadLibrary:
        case DLL_PROCESS_ATTACH: 
            cci_debug_printf("%s DLL_PROCESS_ATTACH", __FUNCTION__);
            // Process-wide mutex used to allow only one thread at a time into the RPC code:
            hCCAPIv2Mutex = CreateMutex(NULL, FALSE, CCAPI_V2_MUTEX_NAME);

            // Figure out our username; it's process-wide:
            bStatus = GetUserName(_user, &maxUN);
            if (!bStatus) return bStatus;

            // Remove any characters that aren't valid endpoint characters:
            while (_user[j] != 0) {
                if (isalnum(_user[j])) _user[i++] = _user[j];
                j++;
                }
            _user[i]    = '\0';

            // Our logon session is determined in client.cxx, old CCAPI code carried
            //  over to this implementation.

            // Allocate a TLS index:
            if ((dwTlsIndex = TlsAlloc()) == TLS_OUT_OF_INDEXES) return FALSE; 

            cci_process_init__auxinit();
            // Don't break; fallthrough: Initialize the TLS index for first thread.
 
        // The attached process creates a new thread:
        case DLL_THREAD_ATTACH:
            cci_debug_printf("%s DLL_THREAD_ATTACH", __FUNCTION__);
            // Don't actually rely on this case for allocation of resources.
            // Applications (like SecureCRT) may have threads already
            // created (say 'A' and 'B') before the dll is loaded. If the dll
            // is loaded in thread 'A' but then used in thread 'B', thread 'B'
            // will never execute this code.
            fIgnore     = TlsSetValue(dwTlsIndex, NULL);

            // Do not call cci_ipc_thread_init() yet; defer until we actually
            // need it.  On XP, cci_ipc_thread_init() will cause additional
            // threads to be immediately spawned, which will bring us right
            // back here again ad infinitum, until windows
            // resources are exhausted.
            break;
 
        // The thread of the attached process terminates:
        case DLL_THREAD_DETACH: 
            cci_debug_printf("%s DLL_THREAD_DETACH", __FUNCTION__);
            // Release the allocated memory for this thread
            ptspdata = (struct tspdata*)TlsGetValue(dwTlsIndex); 
            if (ptspdata != NULL) {
                free(ptspdata);
                TlsSetValue(dwTlsIndex, NULL); 
                }
            break; 
 
        // DLL unload due to process termination or FreeLibrary:
        case DLL_PROCESS_DETACH: 
            cci_debug_printf("%s DLL_PROCESS_DETACH", __FUNCTION__);
            //++ Copied from previous implementation:
            // Process Teardown "Problem"
            //
            // There are two problems that occur during process teardown:
            //
            // 1) Windows (NT/9x/2000) does not keep track of load/unload
            //    ordering dependencies for use in process teardown.
            //
            // 2) The RPC exception handling in the RPC calls do not work
            //    during process shutdown in Win9x.
            //
            // When a process is being torn down in Windows, the krbcc DLL
            // may get a DLL_PROCESS_DETACH before other DLLs are done
            // with it.  Thus, it may disconnect from the RPC server
            // before the last shutdown RPC call.
            //
            // On NT/2000, this is ok because the RPC call will fail and just
            // return an error.
            //
            // On Win9x/Me, the RPC exception will not be caught.
            // However, Win9x ignores exceptions during process shutdown,
            // so the exception will never be seen unless a debugger is
            // attached to the process.
            //
            // A good potential workaround would be to have a global
            // variable that denotes whether the DLL is attached to the
            // process.  If it is not, all entrypoints into the DLL should
            // return failure.
            //
            // A not as good workaround is below but ifdefed out.
            //
            // However, we can safely ignore this problem since it can
            // only affects people running debuggers under 9x/Me who are
            // using multiple DLLs that use this DLL.
            //
            WaitForSingleObject( hCCAPIv2Mutex, INFINITE );

            // return value is ignored, so we set status for debugging purposes
            status = Client::Cleanup();
            status = Init::Cleanup();
            ReleaseMutex( hCCAPIv2Mutex );
            CloseHandle( hCCAPIv2Mutex );
            //-- Copied from previous implementation.

            // Release the allocated memory for this thread:
            ptspdata = (struct tspdata*)TlsGetValue(dwTlsIndex); 
            if (ptspdata != NULL)
                free(ptspdata);
            TlsFree(dwTlsIndex);    // Release the TLS index.
            // Ideally, we would enumerate all other threads here and
            // release their thread local storage as well.
            break; 
 
        default:
            cci_debug_printf("%s unexpected reason %d", __FUNCTION__, fdwReason);
            break;
        } 
 
    UNREFERENCED_PARAMETER(hinstDLL);       // no whining!
    UNREFERENCED_PARAMETER(lpvReserved); 
    return status ? FALSE : TRUE;
}


#ifdef __cplusplus    // If used by C++ code, 
extern "C" {          // we need to export the C interface
#endif

#ifdef __cplusplus
}
#endif

/*********************************************************************/
/*                 MIDL allocate and free                            */
/*********************************************************************/

extern "C" void  __RPC_FAR * __RPC_USER MIDL_user_allocate(size_t len) {
    return(malloc(len));
    }

extern "C" void __RPC_USER MIDL_user_free(void __RPC_FAR * ptr) {
    free(ptr);
    }
