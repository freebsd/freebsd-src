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

#include <string.h>
#include "assert.h"

#pragma warning (disable : 4996)

#include "win-utils.h"
#include "WorkItem.h"

extern "C" {
#include "cci_debugging.h"
    }

// CountedBuffer makes a copy of the data.  Each CountedBuffer must be deleted.

void deleteBuffer(char** buf) {
    if (*buf) {
        delete [](*buf);
        *buf = NULL;
        }
    }

// WorkItem contains a CountedBuffer which must be deleted,
//  so each WorkItem must be deleted.
WorkItem::WorkItem(k5_ipc_stream buf, WIN_PIPE* pipe, const long type, const long sst)
: _buf(buf), _rpcmsg(type), _pipe(pipe), _sst(sst) { }

WorkItem::WorkItem(const WorkItem& item) : _buf(NULL), _rpcmsg(0), _pipe(NULL), _sst(0) {

    k5_ipc_stream    _buf = NULL;
    krb5int_ipc_stream_new(&_buf);
    krb5int_ipc_stream_write(_buf,
                     krb5int_ipc_stream_data(item.payload()),
                     krb5int_ipc_stream_size(item.payload()) );
    WorkItem(_buf, item._pipe, item._rpcmsg, item._sst);
    }

WorkItem::WorkItem() : _buf(NULL), _rpcmsg(CCMSG_INVALID), _pipe(NULL), _sst(0) { }

WorkItem::~WorkItem() {
    if (_buf)   krb5int_ipc_stream_release(_buf);
    if (_pipe)  ccs_win_pipe_release(_pipe);
    }

const k5_ipc_stream WorkItem::take_payload() {
    k5_ipc_stream temp  = payload();
    _buf                = NULL;
    return temp;
    }

WIN_PIPE* WorkItem::take_pipe() {
    WIN_PIPE* temp  = pipe();
    _pipe           = NULL;
    return temp;
    }

WorkList::WorkList() {
    assert(InitializeCriticalSectionAndSpinCount(&cs, 0x80000400));
    }

WorkList::~WorkList() {
    // Delete any WorkItems in the queue:
    WorkItem*   item;
    cci_debug_printf("%s", __FUNCTION__);
    char        buf[2048];
    char*       pbuf        = (char*)buf;
    while (remove(&item)) {
        cci_debug_printf("WorkList::~WorkList() deleting %s", item->print(pbuf));
        delete item;
        }

    DeleteCriticalSection(&cs);
    }

char* WorkItem::print(char* buf) {
    sprintf(buf, "WorkItem msg#:%d sst:%ld pipe:<%s>/0x%X", _rpcmsg, _sst,
        ccs_win_pipe_getUuid(_pipe), ccs_win_pipe_getHandle(_pipe));
    return buf;
    }

int WorkList::initialize() {
    hEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
    return 0;
    }

int WorkList::cleanup() {
    CloseHandle(hEvent);
    hEvent = INVALID_HANDLE_VALUE;
    return 0;
    }

void WorkList::wait() {
    WaitForSingleObject(hEvent, INFINITE);
    }

int WorkList::add(WorkItem* item) {
    EnterCriticalSection(&cs);
        wl.push_front(item);
    LeaveCriticalSection(&cs);
    SetEvent(hEvent);
    return 1;
    }

int WorkList::remove(WorkItem** item) {
    bool    bEmpty;

    bEmpty = wl.empty() & 1;

    if (!bEmpty) {
        EnterCriticalSection(&cs);
            *item    = wl.back();
            wl.pop_back();
        LeaveCriticalSection(&cs);
        }

    return !bEmpty;
    }
