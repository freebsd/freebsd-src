/* ccapi/common/win/tls.h */
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

/* Thread local storage for client threads. */

#ifndef _tls_h
#define _tls_h

#include "windows.h"
#include "time.h"
#include "rpc.h"

#include "k5-ipc_stream.h"

#define UUID_SIZE   128

/* The client code can be run in any client thread.
   The thread-specific data is defined here.
 */

struct tspdata {
    BOOL                _listening;
    BOOL                _CCAPI_Connected;
    RPC_ASYNC_STATE*    _rpcState;
    HANDLE              _replyEvent;
    time_t              _sst;
    k5_ipc_stream        _stream;
    char                _uuid[UUID_SIZE];
    };

void            tspdata_setListening (struct tspdata* p, BOOL b);
void            tspdata_setConnected (struct tspdata* p, BOOL b);
void            tspdata_setReplyEvent(struct tspdata* p, HANDLE h);
void            tspdata_setRpcAState (struct tspdata* p, RPC_ASYNC_STATE* rpcState);
void            tspdata_setSST       (struct tspdata* p, time_t t);
void            tspdata_setStream    (struct tspdata* p, k5_ipc_stream s);
void            tspdata_setUUID      (struct tspdata* p, unsigned char __RPC_FAR* uuidString);
HANDLE          tspdata_getReplyEvent(const struct tspdata* p);

BOOL             tspdata_getListening(const struct tspdata* p);
BOOL             tspdata_getConnected(const struct tspdata* p);
RPC_ASYNC_STATE* tspdata_getRpcAState(const struct tspdata* p);
time_t           tspdata_getSST      (const struct tspdata* p);
k5_ipc_stream     tspdata_getStream   (const struct tspdata* p);
char*            tspdata_getUUID     (const struct tspdata* p);

BOOL WINAPI GetTspData(DWORD tlsIndex, struct tspdata** pdw);

#endif _tls_h
