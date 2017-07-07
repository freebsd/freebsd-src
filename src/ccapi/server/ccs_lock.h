/* ccapi/server/ccs_lock.h */
/*
 * Copyright 2006 Massachusetts Institute of Technology.
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

#ifndef CCS_LOCK_H
#define CCS_LOCK_H

#include "ccs_types.h"

cc_int32 ccs_lock_new (ccs_lock_t       *out_lock,
                       cc_uint32         in_type,
                       cc_int32          in_invalid_object_err,
                       ccs_pipe_t        in_client_pipe,
                       ccs_pipe_t        in_reply_pipe,
                       ccs_lock_state_t  in_lock_state_owner);

cc_int32 ccs_lock_release (ccs_lock_t io_lock);

cc_int32 ccs_lock_grant_lock (ccs_lock_t io_lock);

cc_uint32 ccs_lock_is_pending (ccs_lock_t  in_lock,
                               cc_uint32  *out_pending);

cc_int32 ccs_lock_type (ccs_lock_t  in_lock,
                        cc_uint32  *out_lock_type);

cc_int32 ccs_lock_is_read_lock (ccs_lock_t  in_lock,
                                cc_uint32  *out_is_read_lock);

cc_int32 ccs_lock_is_write_lock (ccs_lock_t  in_lock,
                                 cc_uint32  *out_is_write_lock);

cc_int32 ccs_lock_is_for_client_pipe (ccs_lock_t     in_lock,
                                      ccs_pipe_t     in_client_pipe,
                                      cc_uint32     *out_is_for_client_pipe);

cc_int32 ccs_lock_client_pipe (ccs_lock_t  in_lock,
                               ccs_pipe_t *out_client_pipe);

#endif /* CCS_LOCK_H */
