/* ccapi/server/win/ccs_win_pipe.h */
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

#ifndef _ccs_win_pipe_h_
#define _ccs_win_pipe_h_

#include "windows.h"

#include "CredentialsCache.h"

/* ------------------------------------------------------------------------ */

/* On Windows, a pipe is a struct containing a UUID and a handle.  Both the
   UUID and handle are supplied by the client.

   The UUID is used to build the client's reply endpoint.

   The handle is to the requesting client thread's thread local storage struct,
   so that the client's one and only reply handler can put reply data where
   the requesting thread will be able to see it.
 */

struct ccs_win_pipe_t {
    char*   uuid;
    UINT64  clientHandle;
    };

typedef struct ccs_win_pipe_t WIN_PIPE;

struct ccs_win_pipe_t*  ccs_win_pipe_new(const char* uuid, const UINT64 h);

cc_int32    ccs_win_pipe_release    (const WIN_PIPE* io_pipe);

cc_int32    ccs_win_pipe_compare    (const WIN_PIPE* win_pipe_1,
                                     const WIN_PIPE* win_pipe_2,
                                     cc_uint32  *out_equal);

cc_int32    ccs_win_pipe_copy       (WIN_PIPE** out_pipe,
                                     const WIN_PIPE* in_pipe);

cc_int32    ccs_win_pipe_valid      (const WIN_PIPE* in_pipe);

char*       ccs_win_pipe_getUuid    (const WIN_PIPE* in_pipe);
UINT64      ccs_win_pipe_getHandle  (const WIN_PIPE* in_pipe);

#endif // _ccs_win_pipe_h_
