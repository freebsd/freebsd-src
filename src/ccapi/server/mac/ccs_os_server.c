/* ccapi/server/mac/ccs_os_server.c */
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

#include "ccs_common.h"

#include <syslog.h>
#include "k5_mig_server.h"
#include "ccs_os_server.h"

/* ------------------------------------------------------------------------ */

int32_t k5_ipc_server_add_client (mach_port_t in_client_port)
{
    return cci_check_error (ccs_server_add_client (in_client_port));
}

/* ------------------------------------------------------------------------ */

int32_t k5_ipc_server_remove_client (mach_port_t in_client_port)
{
    return cci_check_error (ccs_server_remove_client (in_client_port));
}


/* ------------------------------------------------------------------------ */

kern_return_t k5_ipc_server_handle_request (mach_port_t    in_connection_port,
                                            mach_port_t    in_reply_port,
                                            k5_ipc_stream  in_request_stream)
{
    return cci_check_error (ccs_server_handle_request (in_connection_port,
                                                       in_reply_port,
                                                       in_request_stream));
}

#pragma mark -

/* ------------------------------------------------------------------------ */

cc_int32 ccs_os_server_initialize (int argc, const char *argv[])
{
    cc_int32 err = 0;

    openlog (argv[0], LOG_CONS | LOG_PID, LOG_AUTH);
    syslog (LOG_INFO, "Starting up.");

    return cci_check_error (err);
}

/* ------------------------------------------------------------------------ */

cc_int32 ccs_os_server_cleanup (int argc, const char *argv[])
{
    cc_int32 err = 0;

    syslog (LOG_NOTICE, "Exiting.");

    return cci_check_error (err);
}

/* ------------------------------------------------------------------------ */

cc_int32 ccs_os_server_listen_loop (int argc, const char *argv[])
{
    return cci_check_error (k5_ipc_server_listen_loop ());
}

/* ------------------------------------------------------------------------ */

cc_int32 ccs_os_server_send_reply (ccs_pipe_t   in_reply_pipe,
                                   k5_ipc_stream in_reply_stream)
{
    return cci_check_error (k5_ipc_server_send_reply (in_reply_pipe,
                                                      in_reply_stream));
}
