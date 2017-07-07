/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* lib/krad/remote.c - Protocol code for libkrad */
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

#include <k5-int.h>
#include <k5-queue.h>
#include "internal.h"

#include <string.h>
#include <unistd.h>

#include <sys/un.h>

#define FLAGS_NONE  VERTO_EV_FLAG_NONE
#define FLAGS_READ  VERTO_EV_FLAG_IO_READ
#define FLAGS_WRITE VERTO_EV_FLAG_IO_WRITE
#define FLAGS_BASE  VERTO_EV_FLAG_PERSIST | VERTO_EV_FLAG_IO_ERROR

K5_TAILQ_HEAD(request_head, request_st);

typedef struct request_st request;
struct request_st {
    K5_TAILQ_ENTRY(request_st) list;
    krad_remote *rr;
    krad_packet *request;
    krad_cb cb;
    void *data;
    verto_ev *timer;
    int timeout;
    size_t retries;
    size_t sent;
};

struct krad_remote_st {
    krb5_context kctx;
    verto_ctx *vctx;
    int fd;
    verto_ev *io;
    char *secret;
    struct addrinfo *info;
    struct request_head list;
    char buffer_[KRAD_PACKET_SIZE_MAX];
    krb5_data buffer;
};

static void
on_io(verto_ctx *ctx, verto_ev *ev);

static void
on_timeout(verto_ctx *ctx, verto_ev *ev);

/* Iterate over the set of outstanding packets. */
static const krad_packet *
iterator(request **out)
{
    request *tmp = *out;

    if (tmp == NULL)
        return NULL;

    *out = K5_TAILQ_NEXT(tmp, list);
    return tmp->request;
}

/* Create a new request. */
static krb5_error_code
request_new(krad_remote *rr, krad_packet *rqst, int timeout, size_t retries,
            krad_cb cb, void *data, request **out)
{
    request *tmp;

    tmp = calloc(1, sizeof(request));
    if (tmp == NULL)
        return ENOMEM;

    tmp->rr = rr;
    tmp->request = rqst;
    tmp->cb = cb;
    tmp->data = data;
    tmp->timeout = timeout;
    tmp->retries = retries;

    *out = tmp;
    return 0;
}

/* Finish a request, calling the callback and freeing it. */
static inline void
request_finish(request *req, krb5_error_code retval,
               const krad_packet *response)
{
    if (retval != ETIMEDOUT)
        K5_TAILQ_REMOVE(&req->rr->list, req, list);

    req->cb(retval, req->request, response, req->data);

    if (retval != ETIMEDOUT) {
        krad_packet_free(req->request);
        verto_del(req->timer);
        free(req);
    }
}

/* Start the timeout timer for the request. */
static krb5_error_code
request_start_timer(request *r, verto_ctx *vctx)
{
    verto_del(r->timer);

    r->timer = verto_add_timeout(vctx, VERTO_EV_FLAG_NONE, on_timeout,
                                 r->timeout);
    if (r->timer != NULL)
        verto_set_private(r->timer, r, NULL);

    return (r->timer == NULL) ? ENOMEM : 0;
}

/* Disconnect from the remote host. */
static void
remote_disconnect(krad_remote *rr)
{
    if (rr->fd >= 0)
        close(rr->fd);
    verto_del(rr->io);
    rr->fd = -1;
    rr->io = NULL;
}

/* Add the specified flags to the remote. This automatically manages the
 * lifecyle of the underlying event. Also connects if disconnected. */
static krb5_error_code
remote_add_flags(krad_remote *remote, verto_ev_flag flags)
{
    verto_ev_flag curflags = VERTO_EV_FLAG_NONE;
    int i;

    flags &= (FLAGS_READ | FLAGS_WRITE);
    if (remote == NULL || flags == FLAGS_NONE)
        return EINVAL;

    /* If there is no connection, connect. */
    if (remote->fd < 0) {
        verto_del(remote->io);
        remote->io = NULL;

        remote->fd = socket(remote->info->ai_family, remote->info->ai_socktype,
                            remote->info->ai_protocol);
        if (remote->fd < 0)
            return errno;

        i = connect(remote->fd, remote->info->ai_addr,
                    remote->info->ai_addrlen);
        if (i < 0) {
            i = errno;
            remote_disconnect(remote);
            return i;
        }
    }

    if (remote->io == NULL) {
        remote->io = verto_add_io(remote->vctx, FLAGS_BASE | flags,
                                  on_io, remote->fd);
        if (remote->io == NULL)
            return ENOMEM;
        verto_set_private(remote->io, remote, NULL);
    }

    curflags = verto_get_flags(remote->io);
    if ((curflags & flags) != flags)
        verto_set_flags(remote->io, FLAGS_BASE | curflags | flags);

    return 0;
}

/* Remove the specified flags to the remote. This automatically manages the
 * lifecyle of the underlying event. */
static void
remote_del_flags(krad_remote *remote, verto_ev_flag flags)
{
    if (remote == NULL || remote->io == NULL)
        return;

    flags = verto_get_flags(remote->io) & (FLAGS_READ | FLAGS_WRITE) & ~flags;
    if (flags == FLAGS_NONE) {
        verto_del(remote->io);
        remote->io = NULL;
        return;
    }

    verto_set_flags(remote->io, FLAGS_BASE | flags);
}

/* Close the connection and start the timers of all outstanding requests. */
static void
remote_shutdown(krad_remote *rr)
{
    krb5_error_code retval;
    request *r;

    remote_disconnect(rr);

    /* Start timers for all unsent packets. */
    K5_TAILQ_FOREACH(r, &rr->list, list) {
        if (r->timer == NULL) {
            retval = request_start_timer(r, rr->vctx);
            if (retval != 0)
                request_finish(r, retval, NULL);
        }
    }
}

/* Handle when packets receive no response within their alloted time. */
static void
on_timeout(verto_ctx *ctx, verto_ev *ev)
{
    request *req = verto_get_private(ev);
    krb5_error_code retval = ETIMEDOUT;

    req->timer = NULL;          /* Void the timer event. */

    /* If we have more retries to perform, resend the packet. */
    if (req->retries-- > 0) {
        req->sent = 0;
        retval = remote_add_flags(req->rr, FLAGS_WRITE);
        if (retval == 0)
            return;
    }

    request_finish(req, retval, NULL);
}

/* Write data to the socket. */
static void
on_io_write(krad_remote *rr)
{
    const krb5_data *tmp;
    ssize_t written;
    request *r;

    K5_TAILQ_FOREACH(r, &rr->list, list) {
        tmp = krad_packet_encode(r->request);

        /* If the packet has already been sent, do nothing. */
        if (r->sent == tmp->length)
            continue;

        /* Send the packet. */
        written = sendto(verto_get_fd(rr->io), tmp->data + r->sent,
                         tmp->length - r->sent, 0, NULL, 0);
        if (written < 0) {
            /* Should we try again? */
            if (errno == EWOULDBLOCK || errno == EAGAIN || errno == ENOBUFS ||
                errno == EINTR)
                return;

            /* This error can't be worked around. */
            remote_shutdown(rr);
            return;
        }

        /* If the packet was completely sent, set a timeout. */
        r->sent += written;
        if (r->sent == tmp->length) {
            if (request_start_timer(r, rr->vctx) != 0) {
                request_finish(r, ENOMEM, NULL);
                return;
            }

            if (remote_add_flags(rr, FLAGS_READ) != 0) {
                remote_shutdown(rr);
                return;
            }
        }

        return;
    }

    remote_del_flags(rr, FLAGS_WRITE);
    return;
}

/* Read data from the socket. */
static void
on_io_read(krad_remote *rr)
{
    const krad_packet *req = NULL;
    krad_packet *rsp = NULL;
    krb5_error_code retval;
    ssize_t pktlen;
    request *tmp, *r;
    int i;

    pktlen = sizeof(rr->buffer_) - rr->buffer.length;
    if (rr->info->ai_socktype == SOCK_STREAM) {
        pktlen = krad_packet_bytes_needed(&rr->buffer);
        if (pktlen < 0) {
            /* If we received a malformed packet on a stream socket,
             * assume the socket to be unrecoverable. */
            remote_shutdown(rr);
            return;
        }
    }

    /* Read the packet. */
    i = recv(verto_get_fd(rr->io), rr->buffer.data + rr->buffer.length,
             pktlen, 0);

    /* On these errors, try again. */
    if (i < 0 && (errno == EWOULDBLOCK || errno == EAGAIN || errno == EINTR))
        return;

    /* On any other errors or on EOF, the socket is unrecoverable. */
    if (i <= 0) {
        remote_shutdown(rr);
        return;
    }

    /* If we have a partial read or just the header, try again. */
    rr->buffer.length += i;
    pktlen = krad_packet_bytes_needed(&rr->buffer);
    if (rr->info->ai_socktype == SOCK_STREAM && pktlen > 0)
        return;

    /* Decode the packet. */
    tmp = K5_TAILQ_FIRST(&rr->list);
    retval = krad_packet_decode_response(rr->kctx, rr->secret, &rr->buffer,
                                         (krad_packet_iter_cb)iterator, &tmp,
                                         &req, &rsp);
    rr->buffer.length = 0;
    if (retval != 0)
        return;

    /* Match the response with an outstanding request. */
    if (req != NULL) {
        K5_TAILQ_FOREACH(r, &rr->list, list) {
            if (r->request == req &&
                r->sent == krad_packet_encode(req)->length) {
                request_finish(r, 0, rsp);
                break;
            }
        }
    }

    krad_packet_free(rsp);
}

/* Handle when IO is ready on the socket. */
static void
on_io(verto_ctx *ctx, verto_ev *ev)
{
    krad_remote *rr;

    rr = verto_get_private(ev);

    if (verto_get_fd_state(ev) & VERTO_EV_FLAG_IO_WRITE)
        on_io_write(rr);
    else
        on_io_read(rr);
}

krb5_error_code
kr_remote_new(krb5_context kctx, verto_ctx *vctx, const struct addrinfo *info,
              const char *secret, krad_remote **rr)
{
    krb5_error_code retval = ENOMEM;
    krad_remote *tmp = NULL;

    tmp = calloc(1, sizeof(krad_remote));
    if (tmp == NULL)
        goto error;
    tmp->kctx = kctx;
    tmp->vctx = vctx;
    tmp->buffer = make_data(tmp->buffer_, 0);
    K5_TAILQ_INIT(&tmp->list);
    tmp->fd = -1;

    tmp->secret = strdup(secret);
    if (tmp->secret == NULL)
        goto error;

    tmp->info = k5memdup(info, sizeof(*info), &retval);
    if (tmp->info == NULL)
        goto error;

    tmp->info->ai_addr = k5memdup(info->ai_addr, info->ai_addrlen, &retval);
    if (tmp->info == NULL)
        goto error;
    tmp->info->ai_next = NULL;
    tmp->info->ai_canonname = NULL;

    *rr = tmp;
    return 0;

error:
    kr_remote_free(tmp);
    return retval;
}

void
kr_remote_free(krad_remote *rr)
{
    if (rr == NULL)
        return;

    while (!K5_TAILQ_EMPTY(&rr->list))
        request_finish(K5_TAILQ_FIRST(&rr->list), ECANCELED, NULL);

    free(rr->secret);
    if (rr->info != NULL)
        free(rr->info->ai_addr);
    free(rr->info);
    remote_disconnect(rr);
    free(rr);
}

krb5_error_code
kr_remote_send(krad_remote *rr, krad_code code, krad_attrset *attrs,
               krad_cb cb, void *data, int timeout, size_t retries,
               const krad_packet **pkt)
{
    krad_packet *tmp = NULL;
    krb5_error_code retval;
    request *r;

    if (rr->info->ai_socktype == SOCK_STREAM)
        retries = 0;

    r = K5_TAILQ_FIRST(&rr->list);
    retval = krad_packet_new_request(rr->kctx, rr->secret, code, attrs,
                                     (krad_packet_iter_cb)iterator, &r, &tmp);
    if (retval != 0)
        goto error;

    K5_TAILQ_FOREACH(r, &rr->list, list) {
        if (r->request == tmp) {
            retval = EALREADY;
            goto error;
        }
    }

    timeout = timeout / (retries + 1);
    retval = request_new(rr, tmp, timeout, retries, cb, data, &r);
    if (retval != 0)
        goto error;

    retval = remote_add_flags(rr, FLAGS_WRITE);
    if (retval != 0)
        goto error;

    K5_TAILQ_INSERT_TAIL(&rr->list, r, list);
    if (pkt != NULL)
        *pkt = tmp;
    return 0;

error:
    krad_packet_free(tmp);
    return retval;
}

void
kr_remote_cancel(krad_remote *rr, const krad_packet *pkt)
{
    request *r;

    K5_TAILQ_FOREACH(r, &rr->list, list) {
        if (r->request == pkt) {
            request_finish(r, ECANCELED, NULL);
            return;
        }
    }
}

krb5_boolean
kr_remote_equals(const krad_remote *rr, const struct addrinfo *info,
                 const char *secret)
{
    struct sockaddr_un *a, *b;

    if (strcmp(rr->secret, secret) != 0)
        return FALSE;

    if (info->ai_addrlen != rr->info->ai_addrlen)
        return FALSE;

    if (info->ai_family != rr->info->ai_family)
        return FALSE;

    if (info->ai_socktype != rr->info->ai_socktype)
        return FALSE;

    if (info->ai_protocol != rr->info->ai_protocol)
        return FALSE;

    if (info->ai_flags != rr->info->ai_flags)
        return FALSE;

    if (memcmp(rr->info->ai_addr, info->ai_addr, info->ai_addrlen) != 0) {
        /* AF_UNIX fails the memcmp() test due to uninitialized bytes after the
         * socket name. */
        if (info->ai_family != AF_UNIX)
            return FALSE;

        a = (struct sockaddr_un *)info->ai_addr;
        b = (struct sockaddr_un *)rr->info->ai_addr;
        if (strncmp(a->sun_path, b->sun_path, sizeof(a->sun_path)) != 0)
            return FALSE;
    }

    return TRUE;
}
