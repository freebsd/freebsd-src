/* ccapi/server/ccs_os_server.h */
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

#ifndef CCS_OS_SERVER_H
#define CCS_OS_SERVER_H

#include "ccs_types.h"

cc_int32 ccs_os_server_initialize (int argc, const char *argv[]);

cc_int32 ccs_os_server_cleanup (int argc, const char *argv[]);

cc_int32 ccs_os_server_listen_loop (int argc, const char *argv[]);

cc_int32 ccs_os_server_send_reply (ccs_pipe_t   in_reply_pipe,
                                   k5_ipc_stream in_reply_stream);

#endif /* CCS_OS_SERVER_H */
