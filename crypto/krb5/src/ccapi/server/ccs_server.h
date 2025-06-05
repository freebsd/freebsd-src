/* ccapi/server/ccs_server.h */
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

#ifndef CCS_SERVER_H
#define CCS_SERVER_H

#include "ccs_types.h"

cc_int32 ccs_server_new_identifier (cci_identifier_t *out_identifier);

cc_int32 ccs_server_add_client (ccs_pipe_t in_connection_pipe);

cc_int32 ccs_server_remove_client (ccs_pipe_t in_connection_pipe);

cc_int32 ccs_server_client_for_pipe (ccs_pipe_t    in_client_pipe,
                                     ccs_client_t *out_client);

cc_int32 ccs_server_client_is_valid (ccs_pipe_t  in_client_pipe,
                                     cc_uint32  *out_client_is_valid);

cc_int32 ccs_server_handle_request (ccs_pipe_t     in_client_pipe,
                                    ccs_pipe_t     in_reply_pipe,
                                    k5_ipc_stream   in_request);

cc_int32 ccs_server_send_reply (ccs_pipe_t     in_reply_pipe,
                                cc_int32       in_reply_err,
                                k5_ipc_stream   in_reply_data);

cc_uint64 ccs_server_client_count ();

#endif /* CCS_SERVER_H */
