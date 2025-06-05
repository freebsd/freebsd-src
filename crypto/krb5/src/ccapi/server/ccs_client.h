/* ccapi/server/ccs_client.h */
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

#ifndef CCS_CLIENT_H
#define CCS_CLIENT_H

#include "ccs_types.h"

cc_int32 ccs_client_new (ccs_client_t *out_client,
                         ccs_pipe_t    in_client_pipe);

cc_int32 ccs_client_release (ccs_client_t io_client);

cc_int32 ccs_client_add_callback (ccs_client_t   io_client,
				  ccs_callback_t in_lock);

cc_int32 ccs_client_remove_callback (ccs_client_t   io_client,
				     ccs_callback_t in_lock);

cc_int32 ccs_client_add_iterator (ccs_client_t                io_client,
				  ccs_generic_list_iterator_t in_iterator);

cc_int32 ccs_client_remove_iterator (ccs_client_t                io_client,
				     ccs_generic_list_iterator_t in_iterator);

cc_int32 ccs_client_uses_pipe (ccs_client_t  in_client,
                               ccs_pipe_t    in_pipe,
                               cc_uint32    *out_uses_pipe);

#endif /* CCS_CLIENT_H */
