/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* include/k5-tls.h - internal pluggable interface for TLS */
/*
 * Copyright (C) 2014 by the Massachusetts Institute of Technology.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * * Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 *
 * * Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer in
 *   the documentation and/or other materials provided with the
 *   distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * This internal pluggable interface allows libkrb5 to load an in-tree module
 * providing TLS support at runtime.  It is currently tailored for the needs of
 * the OpenSSL module as used for HTTP proxy support.  As an internal
 * interface, it can be changed to fit different implementations and consumers
 * without regard for backward compatibility.
 */

#ifndef K5_TLS_H
#define K5_TLS_H

#include "k5-int.h"

/* An abstract type for localauth module data. */
typedef struct k5_tls_handle_st *k5_tls_handle;

typedef enum {
    DATA_READ, DONE, WANT_READ, WANT_WRITE, ERROR_TLS
} k5_tls_status;

/*
 * Create a handle for fd, where the server certificate must match servername
 * and be trusted according to anchors.  anchors is a null-terminated list
 * using the DIR:/FILE:/ENV: syntax borrowed from PKINIT.  If anchors is null,
 * use the system default trust anchors.
 */
typedef krb5_error_code
(*k5_tls_setup_fn)(krb5_context context, SOCKET fd, const char *servername,
                   char **anchors, k5_tls_handle *handle_out);

/*
 * Write len bytes of data using TLS.  Return DONE if writing is complete,
 * WANT_READ or WANT_WRITE if the underlying socket must be readable or
 * writable to continue, and ERROR_TLS if the TLS channel or underlying socket
 * experienced an error.  After WANT_READ or WANT_WRITE, the operation will be
 * retried with the same arguments even if some data has already been written.
 * (OpenSSL makes this contract easy to fulfill.  For other implementations we
 * might want to change it.)
 */
typedef k5_tls_status
(*k5_tls_write_fn)(krb5_context context, k5_tls_handle handle,
                   const void *data, size_t len);

/*
 * Read up to data_size bytes of data using TLS.  Return DATA_READ and set
 * *len_out if any data is read.  Return DONE if there is no more data to be
 * read on the connection, WANT_READ or WANT_WRITE if the underlying socket
 * must be readable or writable to continue, and ERROR_TLS if the TLS channel
 * or underlying socket experienced an error.
 *
 * After DATA_READ, there may still be pending buffered data to read.  The
 * caller must call this method again with additional buffer space before
 * selecting for reading on the underlying socket.
 */
typedef k5_tls_status
(*k5_tls_read_fn)(krb5_context context, k5_tls_handle handle, void *data,
                  size_t data_size, size_t *len_out);

/* Release a handle.  Do not pass a null pointer. */
typedef void
(*k5_tls_free_handle_fn)(krb5_context context, k5_tls_handle handle);

/* All functions are mandatory unless they are all null, in which case the
 * caller should assume that TLS is unsupported. */
typedef struct k5_tls_vtable_st {
    k5_tls_setup_fn setup;
    k5_tls_write_fn write;
    k5_tls_read_fn read;
    k5_tls_free_handle_fn free_handle;
} *k5_tls_vtable;

#endif /* K5_TLS_H */
