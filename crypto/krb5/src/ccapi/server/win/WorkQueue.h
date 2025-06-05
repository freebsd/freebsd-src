/* ccapi/server/win/WorkQueue.h */
/*
 * Copyright 2007 Massachusetts Institute of Technology.
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

#ifndef _work_queue_h
#define _work_queue_h

#include "windows.h"
#include "ccs_pipe.h"

EXTERN_C    int worklist_initialize();

EXTERN_C    int worklist_cleanup();

/* Wait for work to be added to the list (via worklist_add) from another thread */
EXTERN_C    void worklist_wait();

EXTERN_C    BOOL worklist_isEmpty();

EXTERN_C    int worklist_add(  const long          rpcmsg,
                                const ccs_pipe_t    pipe,
                                const k5_ipc_stream stream,
                                const time_t        serverStartTime);

EXTERN_C    int  worklist_remove(long*              rpcmsg,
                                 ccs_pipe_t*        pipe,
                                 k5_ipc_stream*      stream,
                                 time_t*            serverStartTime);

#endif // _work_queue_h
