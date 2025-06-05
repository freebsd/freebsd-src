/* ccapi/server/win/ccs_os_pipe.c */
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

#include "ccs_common.h"
#include "ccs_os_pipe.h"
#include "ccs_win_pipe.h"

/* ------------------------------------------------------------------------ */

/* On Windows, a pipe is a struct.  See ccs_win_pipe.h for details.         */


/* ------------------------------------------------------------------------ */

cc_int32 ccs_os_pipe_valid (ccs_pipe_t in_pipe) {
    return ccs_win_pipe_valid(in_pipe);
    }

/* ------------------------------------------------------------------------ */

cc_int32 ccs_os_pipe_copy (ccs_pipe_t* out_pipe, ccs_pipe_t  in_pipe) {
    return ccs_win_pipe_copy(
        out_pipe,
        in_pipe);
    }

/* ------------------------------------------------------------------------ */

cc_int32 ccs_os_pipe_release (ccs_pipe_t io_pipe) {
    return ccs_win_pipe_release(io_pipe);
    }

/* ------------------------------------------------------------------------ */

cc_int32 ccs_os_pipe_compare (ccs_pipe_t  pipe_1,
                              ccs_pipe_t  pipe_2,
                              cc_uint32  *out_equal) {

    return ccs_win_pipe_compare(pipe_1, pipe_2, out_equal);
    }
