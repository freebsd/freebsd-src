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

#include "process.h"
#include "windows.h"

extern "C" {
#include "ccs_common.h"
#include "ccs_os_notify.h"
#include "ccs_os_server.h"
#include "ccs_reply.h"
#include "ccs_request.h"
#include "win-utils.h"
#include "ccutils.h"
    }

#include "WorkQueue.h"
#include "util.h"
#include "opts.hxx"
#include "init.hxx"

#pragma warning (disable : 4996)

BOOL                bListen             = TRUE; /* Why aren't bool and true defined? */
const char*         sessID              = NULL; /* The logon session we are running on behalf of. */
time_t              _sst                = 0;
unsigned char*      pszNetworkAddress   = NULL;
unsigned char*      pszStringBinding    = NULL;
BOOL                bRpcHandleInited    = FALSE;
_RPC_ASYNC_STATE*    rpcState            = NULL;

/* Thread procedures can take only one void* argument.  We put all the args we want
   to pass into this struct and then pass a pointer to the struct: */
struct RpcRcvArgs {
    char*               networkAddress;
    unsigned char*      protocolSequence;
    unsigned char*      sessID;                     /* Used for this server's endpoint */
    unsigned char*      uuid;                       /* Used for client's UUID */
    ParseOpts::Opts*    opts;
    RPC_STATUS          status;
    } rpcargs = {   NULL,                       /* pszNetworkAddress    */
                    (unsigned char*)"ncalrpc",  /* pszProtocolSequence  */
                    NULL,                       /* sessID placeholder   */
                    NULL,                       /* uuid   placeholder   */
                    NULL };                     /* Opts placeholder     */

/* Command line format:
   argv[0] Program name
   argv[1] session ID to use
   argv[2] "D" Debug: go into infinite loop in ccs_os_server_initialize so process
           can be attached in debugger.
           Any other value: continue
 */
#define N_FIXED_ARGS 3
#define SERVER_REPLY_RPC_HANDLE ccs_reply_IfHandle

/* Forward declarations: */
void            receiveLoop(void* rpcargs);
void            connectionListener(void* rpcargs);
void            Usage(const char* argv0);
void            printError(TCHAR* msg);
void            setMySST()      {_sst = time(&_sst);}
time_t          getMySST()      {return _sst;}
RPC_STATUS      send_connection_reply(ccs_pipe_t in_pipe);
void RPC_ENTRY  clientListener( _RPC_ASYNC_STATE*,
                                void* Context,
                                RPC_ASYNC_EVENT Event);
RPC_STATUS RPC_ENTRY sec_callback(  IN RPC_IF_ID *Interface,
                                    IN void *Context);
RPC_STATUS      send_init(char* clientUUID);
//DWORD alloc_name(LPSTR* pname, LPSTR postfix);


/* The layout of the rest of this module:

   The four entrypoints defined in ccs_os_server.h:
      ccs_os_server_initialize
      cc_int32 ccs_os_server_cleanup
      cc_int32 ccs_os_server_listen_loop
      cc_int32 ccs_os_server_send_reply

   Other routines needed by those four.
 */

/* ------------------------------------------------------------------------ */

cc_int32 ccs_os_server_initialize (int argc, const char *argv[]) {
    cc_int32        err                 = 0;
    ParseOpts::Opts opts                = { 0 };
    ParseOpts       PO;
    BOOL            bAdjustedShutdown   = FALSE;
    HMODULE         hKernel32           = GetModuleHandle("kernel32");

    if (!err) {
        sessID = argv[1];
        setMySST();

        opts.cMinCalls  = 1;
        opts.cMaxCalls  = 20;
        opts.fDontWait  = TRUE;

#ifdef CCAPI_TEST_OPTIONS
        PO.SetValidOpts("kemnfubc");
#else
        PO.SetValidOpts("kc");
#endif

        PO.Parse(opts, argc, (char**)argv);

//        while(*argv[2] == 'D') {}       /* Hang here to attach process with debugger. */

        if (hKernel32) {
            typedef BOOL (WINAPI *FP_SetProcessShutdownParameters)(DWORD, DWORD);
            FP_SetProcessShutdownParameters pSetProcessShutdownParameters =
                (FP_SetProcessShutdownParameters)
                GetProcAddress(hKernel32, "SetProcessShutdownParameters");
            if (pSetProcessShutdownParameters) {
                bAdjustedShutdown = pSetProcessShutdownParameters(100, 0);
                }
            }
        cci_debug_printf("%s Shutdown Parameters",
            bAdjustedShutdown ? "Adjusted" : "Did not adjust");

        err = Init::Initialize();
        }

//    if (!err) {
//        if (opts.bShutdown) {
//            status = shutdown_server(opts.pszEndpoint);
//            }
//        }
//    else {
//        status = startup_server(opts);
//        }

    if (!err) {
        err = worklist_initialize();
        }

    if (err) {
        Init::Cleanup();
        fprintf(    stderr, "An error occurred while %s the server (%u)\n",
                    opts.bShutdown ? "shutting down" : "starting/running",
                    err);
        exit(cci_check_error (err));
        }

    return cci_check_error (err);
    }

/* ------------------------------------------------------------------------ */

cc_int32 ccs_os_server_cleanup (int argc, const char *argv[]) {
    cc_int32 err = 0;

    cci_debug_printf("%s for user <%s> shutting down.", argv[0], argv[1]);

    worklist_cleanup();

    return cci_check_error (err);
    }

/* ------------------------------------------------------------------------ */

/* This function takes work items off the work queue and executes them.
 * This is the one and only place where the multi-threaded Windows code
 * calls into the single-threaded common code.
 *
 * The actual 'listening' for requests from clients happens after receiveloop
 * establishes the RPC endpoint the clients will connect to and the RPC procedures
 * put the work items into the work queue.
 */
cc_int32 ccs_os_server_listen_loop (int argc, const char *argv[]) {
    cc_int32        err = 0;
    uintptr_t       threadStatus;

    ParseOpts::Opts opts         = { 0 };
    ParseOpts       PO;
    BOOL            bQuitIfNoClients = FALSE;

    opts.cMinCalls  = 1;
    opts.cMaxCalls  = 20;
    opts.fDontWait  = TRUE;

#ifdef CCAPI_TEST_OPTIONS
    PO.SetValidOpts("kemnfubc");
#else
    PO.SetValidOpts("kc");
#endif
    PO.Parse(opts, argc, (char**)argv);


    //++ debug stuff
    #define INFO_BUFFER_SIZE 32767
    TCHAR  infoBuf[INFO_BUFFER_SIZE];
    DWORD  bufCharCount = INFO_BUFFER_SIZE;
    // Get and display the user name.
    bufCharCount = INFO_BUFFER_SIZE;
    if( !GetUserName( infoBuf, &bufCharCount ) )  printError( TEXT("GetUserName") );
    //--

    /* Sending the reply from within the request RPC handler doesn't seem to work.
       So we listen for requests in a separate thread and put the requests in a
       queue.  */
    rpcargs.sessID  = (unsigned char*)sessID;
    rpcargs.opts    = &opts;
    /// TODO: check for NULL handle, error, etc.  probably move to initialize func...
    threadStatus    = _beginthread(receiveLoop, 0, (void*)&rpcargs);

    /* We handle the queue entries here.  Work loop: */
    while (ccs_server_client_count() > 0 || !bQuitIfNoClients) {
        worklist_wait();
        while (!worklist_isEmpty()) {
            k5_ipc_stream    buf             = NULL;
            long            rpcmsg          = CCMSG_INVALID;
            time_t          serverStartTime = 0xDEADDEAD;
            RPC_STATUS      status          = 0;
            char*           uuid            = NULL;
            k5_ipc_stream    stream          = NULL;
            ccs_pipe_t     pipe             = NULL;
            ccs_pipe_t     pipe2            = NULL;

            if (worklist_remove(&rpcmsg, &pipe, &buf, &serverStartTime)) {
                uuid = ccs_win_pipe_getUuid(pipe);

                if (serverStartTime <= getMySST()) {
                    switch (rpcmsg) {
                        case CCMSG_CONNECT: {
                            cci_debug_printf("  Processing CONNECT");
                            rpcargs.uuid    = (unsigned char*)uuid;

                            // Even if a disconnect message is received before this code finishes,
                            //  it won't be dequeued and processed until after this code finishes.
                            //  So we can add the client after starting the connection listener.
                            connectionListener((void*)&rpcargs);
                            status  = rpcargs.status;

                            if (!status) {
                                status = ccs_server_add_client(pipe);
                                }
                            if (!status) {status = send_connection_reply(pipe);}
                            break;
                            }
                        case CCMSG_DISCONNECT: {
                            cci_debug_printf("  Processing DISCONNECT");
                            if (!status) {
                                status = ccs_server_remove_client(pipe);
                                }
                            break;
                            }
                        case CCMSG_REQUEST:
                            cci_debug_printf("  Processing REQUEST");
                            ccs_pipe_copy(&pipe2, pipe);
                            // Dispatch message here, setting both pipes to the client UUID:
                            err = ccs_server_handle_request (pipe, pipe2, buf);
                            break;
                        case CCMSG_PING:
                            cci_debug_printf("  Processing PING");
                            err = krb5int_ipc_stream_new  (&stream);
                            err = krb5int_ipc_stream_write(stream, "This is a test of the emergency broadcasting system", 52);
                            err = ccs_os_server_send_reply(pipe, stream);
                            break;
                        case CCMSG_QUIT:
                            bQuitIfNoClients = TRUE;
                            break;
                        default:
                            cci_debug_printf("Huh?  Received invalid message type %ld from UUID:<%s>",
                                rpcmsg, uuid);
                            break;
                        }
                    if (buf)        krb5int_ipc_stream_release(buf);
                    /* Don't free uuid, which was allocated here.  A pointer to it is in the
                       rpcargs struct which was passed to connectionListener which will be
                       received by ccapi_listen when the client exits.  ccapi_listen needs
                       the uuid to know which client to disconnect.
                     */
                    }
                // Server's start time is different from what the client thinks.
                // That means the server has rebooted since the client connected.
                else {
                    cci_debug_printf("Whoops!  Server has rebooted since client established connection.");
                    }
                }
            else {cci_debug_printf("Huh?  Queue not empty but no item to remove.");}
            }
        }
    return cci_check_error (err);
    }

/* ------------------------------------------------------------------------ */

cc_int32 ccs_os_server_send_reply (ccs_pipe_t   in_pipe,
                                   k5_ipc_stream in_reply_stream) {

    /* ccs_pipe_t in_reply_pipe     is a char* reply endpoint.
       k5_ipc_stream in_reply_stream is the data to be sent.
     */

    cc_int32    err     = 0;
    char*       uuid    = ccs_win_pipe_getUuid(in_pipe);
    UINT64      h       = ccs_win_pipe_getHandle(in_pipe);

    if (!err) {
        err = send_init(uuid);      // Sets RPC handle to be used.
        }

    if (!err) {
        RpcTryExcept {
            long    status;
            ccs_rpc_request_reply(                  // make call with user message
                CCMSG_REQUEST_REPLY,                /* Message type */
                (unsigned char*)&h,                 /* client's tspdata* */
                (unsigned char*)uuid,
                getMySST(),
                krb5int_ipc_stream_size(in_reply_stream),   /* Length of buffer */
                (const unsigned char*)krb5int_ipc_stream_data(in_reply_stream),   /* Data buffer */
                &status );                          /* Return code */
            }
        RpcExcept(1) {
            cci_check_error(RpcExceptionCode());
            }
        RpcEndExcept
        }

    /*  The calls to the remote procedures are complete. */
    /*  Free whatever we allocated:                      */
    err = RpcBindingFree(&SERVER_REPLY_RPC_HANDLE);

    return cci_check_error (err);
    }


/* Windows-specific routines: */

void Usage(const char* argv0) {
    printf("Usage:\n");
    printf("%s [m maxcalls] [n mincalls] [f dontwait] [h|?]]\n", argv0);
    printf("    CCAPI server process.\n");
    printf("    h|? whow usage message. <\n");
    }

/* ------------------------------------------------------------------------ */
/* The receive thread repeatedly issues RpcServerListen.
   When a message arrives, it is handled in the RPC procedure.
 */
void    receiveLoop(void* rpcargs) {

    struct RpcRcvArgs*      rcvargs     = (struct RpcRcvArgs*)rpcargs;
    RPC_STATUS              status      = FALSE;
    unsigned char*          pszSecurity = NULL;
    LPSTR                   endpoint    = NULL;
    LPSTR                   event_name  = NULL;
    PSECURITY_DESCRIPTOR    psd         = NULL;
    HANDLE                  hEvent      = 0;
    Init::InitInfo          info;

    cci_debug_printf("THREAD BEGIN: %s", __FUNCTION__);

    status = Init::Info(info);

    /* Build complete RPC endpoint using previous CCAPI implementation: */
    if (!status) {
        if (!rcvargs->opts->pszEndpoint) {
            if (!status) {
                status  = alloc_name(&endpoint,     "ep", isNT());
                }

            if (!status) {
                status  = alloc_name(&event_name,   "startup", isNT());
                }

            if (!status) {
                 hEvent = OpenEvent(EVENT_MODIFY_STATE, FALSE, event_name);
                // We ignore any error opening the event because we do not know who started us.
                //  [Comment paraphrased from previous implementation, whence it was copied.]
                }
            }
        else {
            endpoint = rcvargs->opts->pszEndpoint;
            }
        }

    cci_debug_printf("%s Registering endpoint %s", __FUNCTION__, endpoint);

    if (!status && isNT()) {
        status = alloc_own_security_descriptor_NT(&psd);
        }

    if (!status) {
        status = RpcServerUseProtseqEp(rcvargs->protocolSequence,
                                       rcvargs->opts->cMaxCalls,
                                       (RPC_CSTR)endpoint,
                                       rcvargs->opts->bDontProtect ? 0 : psd);  // SD
        }

    if (!status) {
        status = RpcServerRegisterAuthInfo(0, // server principal
                                           RPC_C_AUTHN_WINNT,
                                           0,
                                           0);
        }

    while (bListen && !status) {
        cci_debug_printf("%s is listening ...", __FUNCTION__);

        if (!info.isNT) {
            status = RpcServerRegisterIf(ccs_request_ServerIfHandle,    // interface
                                         NULL,                          // MgrTypeUuid
                                         NULL);                         // MgrEpv; null means use default
            }
        else {
            status = info.fRpcServerRegisterIfEx(ccs_request_ServerIfHandle,  // interface
                                         NULL,                          // MgrTypeUuid
                                         NULL,                          // MgrEpv; 0 means default
                                         RPC_IF_ALLOW_SECURE_ONLY,
                                         rcvargs->opts->cMaxCalls,
                                         rcvargs->opts->bSecCallback ?
                                         (RPC_IF_CALLBACK_FN*)sec_callback : 0 );
            }

        if (!status) {
            status = RpcServerListen(rcvargs->opts->cMinCalls,
                                     rcvargs->opts->cMaxCalls,
                                     rcvargs->opts->fDontWait);
            }

        if (!status) {
            if (rcvargs->opts->fDontWait) {
                if (hEvent) SetEvent(hEvent);   // Ignore any error -- SetEvent is an optimization.
                status = RpcMgmtWaitServerListen();
                }
            }
        }

    if (status) {           // Cleanup in case of errors:
        if (hEvent) CloseHandle(hEvent);
        free_alloc_p(&event_name);
        free_alloc_p(&psd);
        if (endpoint && (endpoint != rcvargs->opts->pszEndpoint))
            free_alloc_p(&endpoint);
        }

    // tell main thread to shutdown since it won't receive any more messages
    worklist_add(CCMSG_QUIT, NULL, NULL, 0);
    _endthread();
    }   // End receiveLoop



/* ------------------------------------------------------------------------ */
/* The connection listener thread waits forever for a call to the CCAPI_CLIENT_<UUID>
   endpoint, ccapi_listen function to complete.  If the call completes or gets an
   RPC exception, it means the client has disappeared.

   A separate connectionListener is started for each client that has connected to the server.
 */

void    connectionListener(void* rpcargs) {

    struct RpcRcvArgs*  rcvargs     = (struct RpcRcvArgs*)rpcargs;
    RPC_STATUS          status      = FALSE;
    char*               endpoint;
    unsigned char*      pszOptions  = NULL;
    unsigned char *     pszUuid     = NULL;

    endpoint    = clientEndpoint((char*)rcvargs->uuid);
    rpcState    = (RPC_ASYNC_STATE*)malloc(sizeof(RPC_ASYNC_STATE));
    status      = RpcAsyncInitializeHandle(rpcState, sizeof(RPC_ASYNC_STATE));
    cci_debug_printf("");
    cci_debug_printf("%s About to LISTEN to <%s>", __FUNCTION__, endpoint);

    rpcState->UserInfo                  = rcvargs->uuid;
    rpcState->NotificationType          = RpcNotificationTypeApc;
    rpcState->u.APC.NotificationRoutine = clientListener;
    rpcState->u.APC.hThread             = 0;

    /* [If in use] Free previous binding: */
    if (bRpcHandleInited) {
        // Free previous binding (could have been used to call ccapi_listen
        //  in a different client thread).
        // Don't check result or update status.
        RpcStringFree(&pszStringBinding);
        RpcBindingFree(&SERVER_REPLY_RPC_HANDLE);
        bRpcHandleInited  = FALSE;
        }

    /* Set up binding to the client's endpoint: */
    if (!status) {
        status = RpcStringBindingCompose(
                    pszUuid,
                    pszProtocolSequence,
                    pszNetworkAddress,
                    (RPC_CSTR)endpoint,
                    pszOptions,
                    &pszStringBinding);
        }

    /* Set the binding handle that will be used to bind to the server. */
    if (!status) {
        status = RpcBindingFromStringBinding(pszStringBinding, &SERVER_REPLY_RPC_HANDLE);
        }
    if (!status) {bRpcHandleInited  = TRUE;}

    RpcTryExcept {
        cci_debug_printf("  Calling remote procedure ccapi_listen");
        ccapi_listen(rpcState, SERVER_REPLY_RPC_HANDLE, CCMSG_LISTEN, &status);
        /* Asynchronous call will return immediately. */
        }
    RpcExcept(1) {
        status = cci_check_error(RpcExceptionCode());
        }
    RpcEndExcept

    rcvargs->status = status;
    }   // End connectionListener


void RPC_ENTRY clientListener(
    _RPC_ASYNC_STATE* pAsync,
    void* Context,
    RPC_ASYNC_EVENT Event
    ) {

    ccs_pipe_t pipe = ccs_win_pipe_new((char*)pAsync->UserInfo, NULL);

    cci_debug_printf("%s(0x%X, ...) async routine for <0x%X:%s>!",
        __FUNCTION__, pAsync, pAsync->UserInfo, pAsync->UserInfo);

    worklist_add(   CCMSG_DISCONNECT,
                    pipe,
                    NULL,               /* No payload with connect request */
                    (const time_t)0 );  /* No server session number with connect request */
    }


void printError( TCHAR* msg ) {
    DWORD eNum;
    TCHAR sysMsg[256];
    TCHAR* p;

    eNum = GetLastError( );
    FormatMessage( FORMAT_MESSAGE_FROM_SYSTEM |
         FORMAT_MESSAGE_IGNORE_INSERTS,
         NULL, eNum,
         MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
         sysMsg, 256, NULL );

    // Trim the end of the line and terminate it with a null
    p = sysMsg;
    while( ( *p > 31 ) || ( *p == 9 ) )
        ++p;
    do { *p-- = 0; } while( ( p >= sysMsg ) &&
                          ( ( *p == '.' ) || ( *p < 33 ) ) );

    // Display the message
    cci_debug_printf("%s failed with error %d (%s)", msg, eNum, sysMsg);
    }


RPC_STATUS send_init(char* clientUUID) {
    RPC_STATUS      status;
    unsigned char * pszUuid             = NULL;
    unsigned char * pszOptions          = NULL;

    /* Use a convenience function to concatenate the elements of */
    /* the string binding into the proper sequence.              */
    status = RpcStringBindingCompose(pszUuid,
                                     pszProtocolSequence,
                                     pszNetworkAddress,
                                     (unsigned char*)clientEndpoint(clientUUID),
                                     pszOptions,
                                     &pszStringBinding);
    if (status) {return (status);}

    /* Set the binding handle that will be used to bind to the RPC server [the 'client']. */
    status = RpcBindingFromStringBinding(pszStringBinding, &SERVER_REPLY_RPC_HANDLE);
    return (status);
    }

RPC_STATUS send_finish() {
    RPC_STATUS  status;
    /* Can't shut down client -- it runs listen function which  */
    /* server uses to detect the client going away.             */

    /*  The calls to the remote procedures are complete. */
    /*  Free the string and the binding handle           */
    status = RpcStringFree(&pszStringBinding);  // remote calls done; unbind
    if (status) {return (status);}

    status = RpcBindingFree(&SERVER_REPLY_RPC_HANDLE);  // remote calls done; unbind

    return (status);
    }

RPC_STATUS send_connection_reply(ccs_pipe_t in_pipe) {
    char*       uuid    = ccs_win_pipe_getUuid  (in_pipe);
    UINT64      h       = ccs_win_pipe_getHandle(in_pipe);
    RPC_STATUS  status  = send_init(uuid);

    RpcTryExcept {
        ccs_rpc_connect_reply(      // make call with user message
            CCMSG_CONNECT_REPLY,    /* Message type */
            (unsigned char*)&h,      /* client's tspdata* */
            (unsigned char*)uuid,
            getMySST(),             /* Server's session number = its start time */
            &status );              /* Return code */
        }
    RpcExcept(1) {
        cci_check_error(RpcExceptionCode());
        }
    RpcEndExcept

    status  = send_finish();
    return (status);
    }

RPC_STATUS GetPeerName( RPC_BINDING_HANDLE hClient,
                        LPTSTR pszClientName,
                        int iMaxLen) {
    RPC_STATUS Status		= RPC_S_OK;
    RPC_BINDING_HANDLE hServer	= NULL;
    PTBYTE pszStringBinding	= NULL;
    PTBYTE pszClientNetAddr	= NULL;
    PTBYTE pszProtSequence	= NULL;

    memset(pszClientName, 0, iMaxLen * sizeof(TCHAR));

    __try {
        // Create a partially bound server handle from the client handle.
        Status = RpcBindingServerFromClient (hClient, &hServer);
        if (Status != RPC_S_OK) __leave;

        // Get the partially bound server string binding and parse it.
        Status = RpcBindingToStringBinding (hServer,
                                            &pszStringBinding);
        if (Status != RPC_S_OK) __leave;

        // String binding only contains protocol sequence and client
        // address, and is not currently implemented for named pipes.
        Status = RpcStringBindingParse (pszStringBinding, NULL,
                                        &pszProtSequence, &pszClientNetAddr,
                                        NULL, NULL);
        if (Status != RPC_S_OK)
            __leave;
        int iLen = lstrlen(pszClientName) + 1;
        if (iMaxLen < iLen)
            Status = RPC_S_BUFFER_TOO_SMALL;
        lstrcpyn(pszClientName, (LPCTSTR)pszClientNetAddr, iMaxLen);
    }
    __finally {
        if (pszProtSequence)
            RpcStringFree (&pszProtSequence);

        if (pszClientNetAddr)
            RpcStringFree (&pszClientNetAddr);

        if (pszStringBinding)
            RpcStringFree (&pszStringBinding);

        if (hServer)
            RpcBindingFree (&hServer);
    }
    return Status;
}

struct client_auth_info {
    RPC_AUTHZ_HANDLE authz_handle;
    unsigned char* server_principal; // need to RpcFreeString this
    ULONG authn_level;
    ULONG authn_svc;
    ULONG authz_svc;
};

RPC_STATUS
GetClientId(
    RPC_BINDING_HANDLE hClient,
    char* client_id,
    int max_len,
    client_auth_info* info
    )
{
    RPC_AUTHZ_HANDLE authz_handle = 0;
    unsigned char* server_principal = 0;
    ULONG authn_level = 0;
    ULONG authn_svc = 0;
    ULONG authz_svc = 0;
    RPC_STATUS status = 0;

    memset(client_id, 0, max_len);

    if (info) {
        memset(info, 0, sizeof(client_auth_info));
    }

    status = RpcBindingInqAuthClient(hClient, &authz_handle,
                                     info ? &server_principal : 0,
                                     &authn_level, &authn_svc, &authz_svc);
    if (status == RPC_S_OK)
    {
        if (info) {
            info->server_principal = server_principal;
            info->authz_handle = authz_handle;
            info->authn_level = authn_level;
            info->authn_svc = authn_svc;
            info->authz_svc = authz_svc;
        }

        if (authn_svc == RPC_C_AUTHN_WINNT) {
            WCHAR* username = (WCHAR*)authz_handle;
            int len = lstrlenW(username) + 1;
            if (max_len < len)
                status = RPC_S_BUFFER_TOO_SMALL;
            _snprintf(client_id, max_len, "%S", username);
        } else {
            status = RPC_S_UNKNOWN_AUTHN_SERVICE;
        }
    }
    return status;
}

char*
rpc_error_to_string(
    RPC_STATUS status
    )
{
    switch(status) {
    case RPC_S_OK:
        return "OK";
    case RPC_S_INVALID_BINDING:
        return "Invalid binding";
    case RPC_S_WRONG_KIND_OF_BINDING:
        return "Wrong binding";
    case RPC_S_BINDING_HAS_NO_AUTH:
        RpcRaiseException(RPC_S_BINDING_HAS_NO_AUTH);
        return "Binding has no auth";
    default:
        return "BUG: I am confused";
    }
}

void
print_client_info(
    RPC_STATUS peer_status,
    const char* peer_name,
    RPC_STATUS client_status,
    const char* client_id,
    client_auth_info* info
    )
{
    if (peer_status == RPC_S_OK || peer_status == RPC_S_BUFFER_TOO_SMALL) {
        cci_debug_printf("%s Peer Name is \"%s\"", __FUNCTION__, peer_name);
    } else {
        cci_debug_printf("%s Error %u getting Peer Name (%s)",
                     __FUNCTION__, peer_status, rpc_error_to_string(peer_status));
    }

    if (client_status == RPC_S_OK || client_status == RPC_S_BUFFER_TOO_SMALL) {
        if (info) {
            cci_debug_printf("%s Client Auth Info"
                         "\tServer Principal:       %s\n"
                         "\tAuthentication Level:   %d\n"
                         "\tAuthentication Service: %d\n"
                         "\tAuthorization Service:  %d\n",
                         __FUNCTION__,
                         info->server_principal,
                         info->authn_level,
                         info->authn_svc,
                         info->authz_svc);
        }
        cci_debug_printf("%s Client ID is \"%s\"", __FUNCTION__, client_id);
    } else {
        cci_debug_printf("%s Error getting Client Info (%u = %s)",
                     __FUNCTION__, client_status, rpc_error_to_string(client_status));
    }
}

DWORD sid_check() {
    DWORD status = 0;
    HANDLE hToken_c = 0;
    HANDLE hToken_s = 0;
    PTOKEN_USER ptu_c = 0;
    PTOKEN_USER ptu_s = 0;
    DWORD len = 0;
    BOOL bImpersonate = FALSE;

    // Note GetUserName will fail while impersonating at identify
    // level.  The workaround is to impersonate, OpenThreadToken,
    // revert, call GetTokenInformation, and finally, call
    // LookupAccountSid.

    // XXX - Note: This workaround does not appear to work.
    // OpenThreadToken fails with error 1346: "Either a requid
    // impersonation level was not provided or the provided
    // impersonation level is invalid".

    status = RpcImpersonateClient(0);

    if (!status) {
        bImpersonate = TRUE;
        if (!OpenThreadToken(GetCurrentThread(), TOKEN_QUERY, FALSE, &hToken_c))
            status = GetLastError();
        }

    if (!status) {
        status = RpcRevertToSelf();
        }

    if (!status) {
        bImpersonate = FALSE;

        len = 0;
        GetTokenInformation(hToken_c, TokenUser, ptu_c, 0, &len);
        if (len == 0) status = 1;
        }

    if (!status) {
        if (!(ptu_c = (PTOKEN_USER)LocalAlloc(0, len)))
            status = GetLastError();
        }

    if (!status) {
        if (!GetTokenInformation(hToken_c, TokenUser, ptu_c, len, &len))
            status = GetLastError();
        }

    if (!status) {
        if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &hToken_s))
            status = GetLastError();
        }

    if (!status) {
        len = 0;
        GetTokenInformation(hToken_s, TokenUser, ptu_s, 0, &len);
        if (len == 0) status = GetLastError();
        }

    if (!status) {
        if (!(ptu_s = (PTOKEN_USER)LocalAlloc(0, len)))
            status = GetLastError();
        }

    if (!status) {
        if (!GetTokenInformation(hToken_s, TokenUser, ptu_s, len, &len))
            status = GetLastError();
        }

    if (!EqualSid(ptu_s->User.Sid, ptu_c->User.Sid))
        status = RPC_S_ACCESS_DENIED;

/* Cleanup: */
    if (!hToken_c && !bImpersonate)
        cci_debug_printf("%s Cannot impersonate (%u)", __FUNCTION__, status);
    else if (!hToken_c)
        cci_debug_printf("%s Failed to open client token (%u)", __FUNCTION__, status);
    else if (bImpersonate)
        cci_debug_printf("%s Failed to revert (%u)", __FUNCTION__, status);
    else if (!ptu_c)
        cci_debug_printf("%s Failed to get client token user info (%u)",
                     __FUNCTION__, status);
    else if (!hToken_s)
        cci_debug_printf("%s Failed to open server token (%u)", __FUNCTION__, status);
    else if (!ptu_s)
        cci_debug_printf("%s Failed to get server token user info (%u)",
                     __FUNCTION__, status);
    else if (status == RPC_S_ACCESS_DENIED)
        cci_debug_printf("%s SID **does not** match!", __FUNCTION__);
    else if (status == RPC_S_OK)
        cci_debug_printf("%s SID matches!", __FUNCTION__);
    else
        if (status) {
            cci_debug_printf("%s unrecognized error %u", __FUNCTION__, status);
            abort();
            }

    if (bImpersonate)   RpcRevertToSelf();
    if (hToken_c && hToken_c != INVALID_HANDLE_VALUE)
        CloseHandle(hToken_c);
    if (ptu_c)          LocalFree(ptu_c);
    if (hToken_s && hToken_s != INVALID_HANDLE_VALUE)
        CloseHandle(hToken_s);
    if (ptu_s)          LocalFree(ptu_s);
    if (status) cci_debug_printf("%s returning %u", __FUNCTION__, status);
    return status;
    }

RPC_STATUS RPC_ENTRY sec_callback(  IN RPC_IF_ID *Interface,
                                    IN void *Context) {
    char peer_name[1024];
    char client_name[1024];
    RPC_STATUS peer_status;
    RPC_STATUS client_status;

    cci_debug_printf("%s", __FUNCTION__);
    peer_status = GetPeerName(Context, peer_name, sizeof(peer_name));
    client_status = GetClientId(Context, client_name, sizeof(client_name), 0);
    print_client_info(peer_status, peer_name, client_status, client_name, 0);
    DWORD sid_status = sid_check();
    cci_debug_printf("%s returning (%u)", __FUNCTION__, sid_status);
    return sid_status;
    }



/*********************************************************************/
/*                 MIDL allocate and free                            */
/*********************************************************************/

extern "C" void  __RPC_FAR * __RPC_USER midl_user_allocate(size_t len) {
    return(malloc(len));
    }

extern "C" void __RPC_USER midl_user_free(void __RPC_FAR * ptr) {
    free(ptr);
    }

/* stubs */
extern "C" cc_int32
ccs_os_notify_cache_collection_changed (ccs_cache_collection_t cc)
{
    return 0;
}

extern "C" cc_int32
ccs_os_notify_ccache_changed (ccs_cache_collection_t cc, const char *name)
{
    return 0;
}
