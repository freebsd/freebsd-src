/* ccapi/server/win/ccs_win_pipe.c */
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

#include "assert.h"
#include <stdlib.h>
#include <malloc.h>

#include "ccs_win_pipe.h"
#include "cci_debugging.h"

/* Ref:
struct ccs_win_pipe_t {
    char*   uuid;
    UINT64  clientHandle;
    }
 */

/* ------------------------------------------------------------------------ */

struct ccs_win_pipe_t* ccs_win_pipe_new (const char* uuid, const UINT64 h) {

    cc_int32                err         = ccNoError;
    struct ccs_win_pipe_t*  out_pipe    = NULL;
    char*                   uuidCopy    = NULL;

    if (!err) {
        if (!uuid)      {err = cci_check_error(ccErrBadParam);}
        }

    if (!err) {
        uuidCopy = (char*)malloc(1+strlen(uuid));
        if (!uuidCopy)  {err = cci_check_error(ccErrBadParam);}
        strcpy(uuidCopy, uuid);
        }

    if (!err) {
        out_pipe = (struct ccs_win_pipe_t*)malloc(sizeof(struct ccs_win_pipe_t));
        if (!out_pipe)  {err = cci_check_error(ccErrBadParam);}
        out_pipe->uuid          = uuidCopy;
        out_pipe->clientHandle  = h;
        }
#if 0
    cci_debug_printf("0x%X = %s(%s, 0x%X)", out_pipe, __FUNCTION__, uuid, h);
#endif
    return out_pipe;
    }

/* ------------------------------------------------------------------------ */

cc_int32 ccs_win_pipe_copy (WIN_PIPE** out_pipe,
                            const WIN_PIPE* in_pipe) {

    *out_pipe =
        ccs_win_pipe_new(
            ccs_win_pipe_getUuid  (in_pipe),
            ccs_win_pipe_getHandle(in_pipe) );

    return (*out_pipe) ? ccNoError : ccErrBadParam;
    }

/* ------------------------------------------------------------------------ */

cc_int32 ccs_win_pipe_release(const WIN_PIPE* in_pipe) {

    cc_int32 err = ccNoError;

    if (!ccs_win_pipe_valid(in_pipe))   {err = cci_check_error(ccErrBadParam);}

    if (!err) {
        if (!in_pipe->uuid) free(in_pipe->uuid);
        if (!in_pipe)       free(in_pipe);
        }

    return err;
    }

/* ------------------------------------------------------------------------ */

cc_int32 ccs_win_pipe_valid (const WIN_PIPE* in_pipe) {

    if (!in_pipe) {
        cci_check_error(ccErrBadParam);
        return FALSE;
        }

    if (!in_pipe->uuid) {
        cci_check_error(ccErrBadParam);
        return FALSE;
        }

    return TRUE;
    }

/* ------------------------------------------------------------------------ */

cc_int32 ccs_win_pipe_compare    (const WIN_PIPE*   in_pipe_1,
                                  const WIN_PIPE*   in_pipe_2,
                                  cc_uint32         *out_equal) {

    cc_int32 err    = ccNoError;
    int      seq    = 0;
    *out_equal      = FALSE;

    if (!ccs_win_pipe_valid(in_pipe_1)) {err = cci_check_error(ccErrBadParam);}
    if (!ccs_win_pipe_valid(in_pipe_2)) {err = cci_check_error(ccErrBadParam);}
    if (!out_equal)                     {err = cci_check_error(ccErrBadParam);}

    /* A disconnect doesn't have a tls* with it -- only the uuid.  SO only
       compare the uuids.
     */
    if (!err) {
        seq = strcmp(   ccs_win_pipe_getUuid(in_pipe_1),
                        ccs_win_pipe_getUuid(in_pipe_2) );
        *out_equal = (seq == 0);
        }

    return err;
    }

/* ------------------------------------------------------------------------ */

char* ccs_win_pipe_getUuid    (const WIN_PIPE* in_pipe) {

    char*   result = NULL;

    if (!ccs_win_pipe_valid(in_pipe)) {cci_check_error(ccErrBadParam);}
    else                              {result = in_pipe->uuid;}

    return result;
    }

/* ------------------------------------------------------------------------ */

UINT64 ccs_win_pipe_getHandle  (const WIN_PIPE* in_pipe) {

    UINT64 result = 0;

    if (!ccs_win_pipe_valid(in_pipe)) {cci_check_error(ccErrBadParam);}
    else                              {result = in_pipe->clientHandle;}

    return result;
    }
