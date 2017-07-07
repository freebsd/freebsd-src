/* ccapi/lib/mac/ccapi_os_ipc.c */
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

#include "ccapi_os_ipc.h"

#include "k5_mig_client.h"

#define cci_server_bundle_id "edu.mit.Kerberos.CCacheServer"

/* ------------------------------------------------------------------------ */

cc_int32 cci_os_ipc_thread_init (void)
{
    /* k5_ipc_send_request handles all thread data for us */
    return 0;
}

/* ------------------------------------------------------------------------ */

cc_int32 cci_os_ipc (cc_int32      in_launch_server,
                     k5_ipc_stream in_request_stream,
                     k5_ipc_stream *out_reply_stream)
{
    return cci_check_error (k5_ipc_send_request (cci_server_bundle_id,
                                                 in_launch_server,
                                                 in_request_stream,
                                                 out_reply_stream));
}
