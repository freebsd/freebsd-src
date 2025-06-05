/* ccapi/common/win/tls.c */
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

#include "string.h"
#include <stdlib.h>
#include <malloc.h>

#include "tls.h"

void tspdata_setUUID(struct tspdata* p, unsigned char __RPC_FAR* uuidString) {
    strncpy(p->_uuid, uuidString, UUID_SIZE-1);
    };

void         tspdata_setListening (struct tspdata* p, BOOL b)           {p->_listening = b;}

void         tspdata_setConnected (struct tspdata* p, BOOL b)           {p->_CCAPI_Connected = b;}

void         tspdata_setReplyEvent(struct tspdata* p, HANDLE h)         {p->_replyEvent = h;}

void         tspdata_setRpcAState (struct tspdata* p, RPC_ASYNC_STATE* rpcState) {
    p->_rpcState = rpcState;}

void         tspdata_setSST       (struct tspdata* p, time_t t)         {p->_sst = t;}

void         tspdata_setStream    (struct tspdata* p, k5_ipc_stream s)   {p->_stream = s;}

BOOL         tspdata_getListening (const struct tspdata* p)         {return p->_listening;}

BOOL         tspdata_getConnected (const struct tspdata* p)         {return p->_CCAPI_Connected;}

HANDLE       tspdata_getReplyEvent(const struct tspdata* p)         {return p->_replyEvent;}

time_t       tspdata_getSST       (const struct tspdata* p)         {return p->_sst;}

k5_ipc_stream tspdata_getStream    (const struct tspdata* p)         {return p->_stream;}

char*        tspdata_getUUID      (const struct tspdata* p)         {return p->_uuid;}

RPC_ASYNC_STATE* tspdata_getRpcAState (const struct tspdata* p)     {return p->_rpcState;}


BOOL WINAPI GetTspData(DWORD dwTlsIndex, struct tspdata**  pdw) {
    struct tspdata*  pData;      // The stored memory pointer

    pData = (struct tspdata*)TlsGetValue(dwTlsIndex);
    if (pData == NULL) {
        pData = malloc(sizeof(*pData));
        if (pData == NULL)
            return FALSE;
        memset(pData, 0, sizeof(*pData));
        TlsSetValue(dwTlsIndex, pData);
    }
    (*pdw) = pData;
    return TRUE;
    }
