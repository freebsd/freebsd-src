// pingtest.c
//
// Test RPC to server, with PING message, which exists for no other purpose than this test.

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <malloc.h>

#include "cci_debugging.h"
#include "CredentialsCache.h"
#include "win-utils.h"

#include "ccs_request.h"
#define CLIENT_REQUEST_RPC_HANDLE ccs_request_IfHandle


extern cc_int32 cci_os_ipc_thread_init (void);
extern cc_int32 cci_os_ipc_msg( cc_int32        in_launch_server,
                                k5_ipc_stream   in_request_stream,
                                cc_int32        in_msg,
                                k5_ipc_stream*  out_reply_stream);

static DWORD    dwTlsIndex;

DWORD GetTlsIndex()    {return dwTlsIndex;}

RPC_STATUS send_test(char* endpoint) {
    unsigned char*  pszNetworkAddress   = NULL;
    unsigned char*  pszOptions          = NULL;
    unsigned char*  pszStringBinding    = NULL;
    unsigned char*  pszUuid             = NULL;
    RPC_STATUS      status;

    status = RpcStringBindingCompose(pszUuid,
                                     (RPC_CSTR)"ncalrpc",
                                     pszNetworkAddress,
                                     (unsigned char*)endpoint,
                                     pszOptions,
                                     &pszStringBinding);
    cci_debug_printf("%s pszStringBinding = %s", __FUNCTION__, pszStringBinding);
    if (status) {return cci_check_error(status);}

    /* Set the binding handle that will be used to bind to the RPC server [the 'client']. */
    status = RpcBindingFromStringBinding(pszStringBinding, &CLIENT_REQUEST_RPC_HANDLE);
    if (status) {return cci_check_error(status);}

    status = RpcStringFree(&pszStringBinding);  // Temp var no longer needed.

    if (!status) {
        RpcTryExcept {
            cci_debug_printf("%s calling remote procedure 'ccs_authenticate'", __FUNCTION__);
            status = ccs_authenticate((CC_CHAR*)"DLLMAIN TEST!");
            cci_debug_printf("  ccs_authenticate returned %d", status);
            }
        RpcExcept(1) {
            status = cci_check_error(RpcExceptionCode());
            }
        RpcEndExcept
        }

    cci_check_error(RpcBindingFree(&CLIENT_REQUEST_RPC_HANDLE));

    return (status);
    }

int main(   int argc, char *argv[]) {
    cc_int32        err             = 0;
    cc_context_t    context         = NULL;
    k5_ipc_stream   send_stream     = NULL;
    k5_ipc_stream   reply_stream    = NULL;
    char*           message         = "Hello, RPC!";


    if ((dwTlsIndex = TlsAlloc()) == TLS_OUT_OF_INDEXES) return FALSE;

    if (!err) {
        err = cci_os_ipc_thread_init();
        }
    if (!err) {
        err = krb5int_ipc_stream_new  (&send_stream);
        err = krb5int_ipc_stream_write(send_stream, message,
				       1+strlen(message));
        }

    if (!err) {
        err = cci_os_ipc_msg(TRUE, send_stream, CCMSG_PING, &reply_stream);
        }
    Sleep(10*1000);
    cci_debug_printf("Try finishing async call.");

    Sleep(INFINITE);
    cci_debug_printf("main: return. err == %d", err);

    return 0;
    }



/*********************************************************************/
/*                 MIDL allocate and free                            */
/*********************************************************************/

void  __RPC_FAR * __RPC_USER midl_user_allocate(size_t len) {
    return(malloc(len));
    }

void __RPC_USER midl_user_free(void __RPC_FAR * ptr) {
    free(ptr);
    }
