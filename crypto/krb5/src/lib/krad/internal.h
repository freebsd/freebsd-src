/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* lib/krad/internal.h - Internal declarations for libkrad */
/*
 * Copyright 2013 Red Hat, Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *    1. Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *
 *    2. Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS
 * IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER
 * OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef INTERNAL_H_
#define INTERNAL_H_

#include <k5-int.h>
#include "krad.h"

#include <errno.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

#ifndef UCHAR_MAX
#define UCHAR_MAX 255
#endif

/* RFC 2865 */
#define MAX_ATTRSIZE (UCHAR_MAX - 2)
#define MAX_ATTRSETSIZE (KRAD_PACKET_SIZE_MAX - 20)

typedef struct krad_remote_st krad_remote;

/* Validate constraints of an attribute. */
krb5_error_code
kr_attr_valid(krad_attr type, const krb5_data *data);

/* Encode an attribute. */
krb5_error_code
kr_attr_encode(krb5_context ctx, const char *secret, const unsigned char *auth,
               krad_attr type, const krb5_data *in,
               unsigned char outbuf[MAX_ATTRSIZE], size_t *outlen);

/* Decode an attribute. */
krb5_error_code
kr_attr_decode(krb5_context ctx, const char *secret, const unsigned char *auth,
               krad_attr type, const krb5_data *in,
               unsigned char outbuf[MAX_ATTRSIZE], size_t *outlen);

/* Encode the attributes into the buffer. */
krb5_error_code
kr_attrset_encode(const krad_attrset *set, const char *secret,
                  const unsigned char *auth,
                  unsigned char outbuf[MAX_ATTRSETSIZE], size_t *outlen);

/* Decode attributes from a buffer. */
krb5_error_code
kr_attrset_decode(krb5_context ctx, const krb5_data *in, const char *secret,
                  const unsigned char *auth, krad_attrset **set);

/* Create a new remote object which manages a socket and the state of
 * outstanding requests. */
krb5_error_code
kr_remote_new(krb5_context kctx, verto_ctx *vctx, const struct addrinfo *info,
              const char *secret, krad_remote **rr);

/* Free a remote object. */
void
kr_remote_free(krad_remote *rr);

/*
 * Send the packet to the remote. The cb will be called when a response is
 * received, the request times out, the request is canceled or an error occurs.
 *
 * The timeout parameter is the total timeout across all retries in
 * milliseconds.
 *
 * If the cb is called with a retval of ETIMEDOUT it indicates that the
 * allotted time has elapsed. However, in the case of a timeout, we continue to
 * listen for the packet until krad_remote_cancel() is called or a response is
 * received. This means that cb will always be called twice in the event of a
 * timeout. This permits you to pursue other remotes while still listening for
 * a response from the first one.
 */
krb5_error_code
kr_remote_send(krad_remote *rr, krad_code code, krad_attrset *attrs,
               krad_cb cb, void *data, int timeout, size_t retries,
               const krad_packet **pkt);

/* Remove packet from the queue of requests awaiting responses. */
void
kr_remote_cancel(krad_remote *rr, const krad_packet *pkt);

/* Cancel all requests awaiting responses. */
void
kr_remote_cancel_all(krad_remote *rr);

/* Determine if this remote object refers to the remote resource identified
 * by the addrinfo struct and the secret. */
krb5_boolean
kr_remote_equals(const krad_remote *rr, const struct addrinfo *info,
                 const char *secret);

/* Adapted from lib/krb5/os/sendto_kdc.c. */
static inline krb5_error_code
gai_error_code(int err)
{
    switch (err) {
    case 0:
        return 0;
    case EAI_BADFLAGS:
    case EAI_FAMILY:
    case EAI_SOCKTYPE:
    case EAI_SERVICE:
#ifdef EAI_ADDRFAMILY
    case EAI_ADDRFAMILY:
#endif
        return EINVAL;
    case EAI_AGAIN:
        return EAGAIN;
    case EAI_MEMORY:
        return ENOMEM;
#if defined(EAI_NODATA) && EAI_NODATA != EAI_NONAME
    case EAI_NODATA:
#endif
    case EAI_NONAME:
        return EADDRNOTAVAIL;
#ifdef EAI_OVERFLOW
    case EAI_OVERFLOW:
        return EOVERFLOW;
#endif
#ifdef EAI_SYSTEM
    case EAI_SYSTEM:
        return errno;
#endif
    default:
        return EINVAL;
    }
}

#endif /* INTERNAL_H_ */
