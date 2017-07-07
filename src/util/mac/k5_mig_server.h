/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* util/mac/k5_mig_server.h */
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

#ifndef K5_MIG_SERVER
#define K5_MIG_SERVER

#include "k5-ipc_stream.h"

/* Defined by caller */

int32_t k5_ipc_server_add_client (mach_port_t in_client_port);

int32_t k5_ipc_server_remove_client (mach_port_t in_client_port);

int32_t k5_ipc_server_handle_request (mach_port_t   in_connection_port,
                                      mach_port_t   in_reply_port,
                                      k5_ipc_stream in_request_stream);

/* Server control functions */

/* WARNING: Currently only supports running server loop on a single thread! */
int32_t k5_ipc_server_listen_loop (void);

int32_t k5_ipc_server_send_reply (mach_port_t   in_reply_pipe,
                                  k5_ipc_stream in_reply_stream);

void k5_ipc_server_quit (void);

#endif /* K5_MIG_SERVER */
