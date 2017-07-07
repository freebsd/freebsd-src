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

#include "WorkQueue.h"
extern "C" {
    #include "cci_debugging.h"
    }

#include "WorkItem.h"

WorkList    worklist;

EXTERN_C    int worklist_initialize() {
        return worklist.initialize();
        }

EXTERN_C    int worklist_cleanup() {
        return worklist.cleanup();
        }

EXTERN_C    void worklist_wait() {
        worklist.wait();
        }

/* C interfaces: */
EXTERN_C    BOOL worklist_isEmpty() {
        return worklist.isEmpty() ? TRUE : FALSE;
        }

EXTERN_C    int worklist_add(   const long          rpcmsg,
                                const ccs_pipe_t    pipe,
                                const k5_ipc_stream stream,
                                const time_t        serverStartTime) {
        return worklist.add(new WorkItem(stream, pipe, rpcmsg, serverStartTime) );
        }

EXTERN_C    int  worklist_remove(long*              rpcmsg,
                                 ccs_pipe_t*        pipe,
                                 k5_ipc_stream*      stream,
                                 time_t*            sst) {
        WorkItem*   item    = NULL;
        cc_int32    err     = worklist.remove(&item);

        *rpcmsg         = item->type();
        *pipe           = item->take_pipe();
        *stream         = item->take_payload();
        *sst            = item->sst();
        delete item;
        return err;
        }

