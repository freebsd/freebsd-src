/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* include/net-server.h */
/*
 * Copyright (C) 2010 by the Massachusetts Institute of Technology.
 * All rights reserved.
 *
 * Export of this software from the United States of America may
 *   require a specific license from the United States Government.
 *   It is the responsibility of any person or organization contemplating
 *   export to obtain such a license before exporting.
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

/* Declarations for "API" of network listener/dispatcher in libapputils. */

#ifndef NET_SERVER_H
#define NET_SERVER_H

#include <verto.h>

/* The delimiter characters supported by the addresses string. */
#define ADDRESSES_DELIM ",; "

typedef struct _krb5_fulladdr {
    krb5_address *      address;
    krb5_ui_4           port;
} krb5_fulladdr;

/* exported from network.c */
void init_addr(krb5_fulladdr *, struct sockaddr *);

/* exported from net-server.c */
verto_ctx *loop_init(verto_ev_type types);

/*
 * Add listener addresses to the loop configuration.
 *
 * Arguments:
 *
 * - default_port
 *      The port for the sockets if not specified in addresses.
 * - addresses
 *      The optional addresses for the listener sockets.  Pass NULL for the
 *      wildcard address.  Addresses may be delimited by the characters in
 *      ADDRESSES_DELIM.  Addresses are parsed with k5_parse_host_string().
 * - prognum, versnum, dispatchfn
 *      For RPC listener sockets, the svc_register() arguments to use when new
 *      TCP connections are created.
 */
krb5_error_code loop_add_udp_address(int default_port, const char *addresses);
krb5_error_code loop_add_tcp_address(int default_port, const char *addresses);
krb5_error_code loop_add_rpc_service(int default_port, const char *addresses,
                                     u_long prognum, u_long versnum,
                                     void (*dispatchfn)());

krb5_error_code loop_setup_network(verto_ctx *ctx, void *handle,
                                   const char *progname,
                                   int tcp_listen_backlog);
krb5_error_code loop_setup_signals(verto_ctx *ctx, void *handle,
                                   void (*reset)());
void loop_free(verto_ctx *ctx);

/* to be supplied by the server application */

/*
 * Two routines for processing an incoming message and getting a
 * result to send back.
 *
 * The first, dispatch(), is for normal processing of a request.  The
 * second, make_toolong_error(), is obviously for generating an error
 * to send back when the incoming message is bigger than
 * the main loop can accept.
 */
typedef void (*loop_respond_fn)(void *arg, krb5_error_code code,
                                krb5_data *response);
void dispatch(void *handle, const krb5_fulladdr *local_addr,
              const krb5_fulladdr *remote_addr, krb5_data *request,
              int is_tcp, verto_ctx *vctx, loop_respond_fn respond, void *arg);
krb5_error_code make_toolong_error (void *handle, krb5_data **);

/*
 * Contexts are needed in lots of places.  Opaque application-provided
 * handles are passed around in lots of place, but contexts are not.
 * For now, we'll require that the application provide us an easy way
 * to get at a context; eventually it should probably be explicitly.
 */
krb5_context get_context(void *handle);

#endif /* NET_SERVER_H */
