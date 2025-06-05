/* ccapi/server/ccs_callback.h */
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

#ifndef CCS_CALLBACK_H
#define CCS_CALLBACK_H

#include "ccs_types.h"

struct ccs_callback_owner_d;
typedef struct ccs_callback_owner_d *ccs_callback_owner_t;

typedef cc_int32 (*ccs_callback_owner_invalidate_t) (ccs_callback_owner_t, ccs_callback_t);


cc_int32 ccs_callback_new (ccs_callback_t                  *out_callback,
			   cc_int32                         in_invalid_object_err,
			   ccs_pipe_t                       in_client_pipe,
			   ccs_pipe_t                       in_reply_pipe,
			   ccs_callback_owner_t             in_owner,
			   ccs_callback_owner_invalidate_t  in_owner_invalidate_function);

cc_int32 ccs_callback_release (ccs_callback_t io_callback);

cc_int32 ccs_callback_invalidate (ccs_callback_t io_callback);

cc_int32 ccs_callback_reply_to_client (ccs_callback_t io_callback,
				       k5_ipc_stream   in_stream);

cc_uint32 ccs_callback_is_pending (ccs_callback_t  in_callback,
				   cc_uint32      *out_pending);

cc_int32 ccs_callback_is_for_client_pipe (ccs_callback_t  in_callback,
					  ccs_pipe_t      in_client_pipe,
					  cc_uint32      *out_is_for_client_pipe);

cc_int32 ccs_callback_client_pipe (ccs_callback_t  in_callback,
				   ccs_pipe_t     *out_client_pipe);

#endif /* CCS_CALLBACK_H */
