/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* lib/krb5/ccache/cc_kcm.c - KCM cache type (client side) */
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
 * This cache type contacts a daemon for each cache operation, using Heimdal's
 * KCM protocol.  On OS X, the preferred transport is Mach RPC; on other
 * Unix-like platforms or if the daemon is not available via RPC, Unix domain
 * sockets are used instead.
 */

#ifndef _WIN32
#include "k5-int.h"
#include "k5-input.h"
#include "cc-int.h"
#include "kcm.h"
#include <sys/socket.h>
#include <sys/un.h>
#ifdef __APPLE__
#include <mach/mach.h>
#include <servers/bootstrap.h>
#include "kcmrpc.h"
#endif

#define MAX_REPLY_SIZE (10 * 1024 * 1024)

const krb5_cc_ops krb5_kcm_ops;

struct uuid_list {
    unsigned char *uuidbytes;   /* all of the uuids concatenated together */
    size_t count;
    size_t pos;
};

struct kcmio {
    int fd;
#ifdef __APPLE__
    mach_port_t mport;
#endif
};

/* This structure bundles together a KCM request and reply, to minimize how
 * much we have to declare and clean up in each method. */
struct kcmreq {
    struct k5buf reqbuf;
    struct k5input reply;
    void *reply_mem;
};
#define EMPTY_KCMREQ { EMPTY_K5BUF }

struct kcm_cache_data {
    char *residual;             /* immutable; may be accessed without lock */
    k5_cc_mutex lock;           /* protects io and changetime */
    struct kcmio *io;
    krb5_timestamp changetime;
};

struct kcm_ptcursor {
    char *residual;             /* primary or singleton subsidiary */
    struct uuid_list *uuids;    /* NULL for singleton subsidiary */
    struct kcmio *io;
    krb5_boolean first;
};

/* Map EINVAL or KRB5_CC_FORMAT to KRB5_KCM_MALFORMED_REPLY; pass through all
 * other codes. */
static inline krb5_error_code
map_invalid(krb5_error_code code)
{
    return (code == EINVAL || code == KRB5_CC_FORMAT) ?
        KRB5_KCM_MALFORMED_REPLY : code;
}

/* Begin a request for the given opcode.  If cache is non-null, supply the
 * cache name as a request parameter. */
static void
kcmreq_init(struct kcmreq *req, kcm_opcode opcode, krb5_ccache cache)
{
    unsigned char bytes[4];
    const char *name;

    memset(req, 0, sizeof(*req));

    bytes[0] = KCM_PROTOCOL_VERSION_MAJOR;
    bytes[1] = KCM_PROTOCOL_VERSION_MINOR;
    store_16_be(opcode, bytes + 2);

    k5_buf_init_dynamic(&req->reqbuf);
    k5_buf_add_len(&req->reqbuf, bytes, 4);
    if (cache != NULL) {
        name = ((struct kcm_cache_data *)cache->data)->residual;
        k5_buf_add_len(&req->reqbuf, name, strlen(name) + 1);
    }
}

/* Add a 32-bit value to the request in big-endian byte order. */
static void
kcmreq_put32(struct kcmreq *req, uint32_t val)
{
    unsigned char bytes[4];

    store_32_be(val, bytes);
    k5_buf_add_len(&req->reqbuf, bytes, 4);
}

#ifdef __APPLE__

/* The maximum length of an in-band request or reply as defined by the RPC
 * protocol. */
#define MAX_INBAND_SIZE 2048

/* Connect or reconnect to the KCM daemon via Mach RPC, if possible. */
static krb5_error_code
kcmio_mach_connect(krb5_context context, struct kcmio *io)
{
    krb5_error_code ret;
    kern_return_t st;
    mach_port_t mport;
    char *service;

    ret = profile_get_string(context->profile, KRB5_CONF_LIBDEFAULTS,
                             KRB5_CONF_KCM_MACH_SERVICE, NULL,
                             DEFAULT_KCM_MACH_SERVICE, &service);
    if (ret)
        return ret;
    if (strcmp(service, "-") == 0) {
        profile_release_string(service);
        return KRB5_KCM_NO_SERVER;
    }

    st = bootstrap_look_up(bootstrap_port, service, &mport);
    profile_release_string(service);
    if (st)
        return KRB5_KCM_NO_SERVER;
    if (io->mport != MACH_PORT_NULL)
        mach_port_deallocate(mach_task_self(), io->mport);
    io->mport = mport;
    return 0;
}

/* Invoke the Mach RPC to get a reply from the KCM daemon. */
static krb5_error_code
kcmio_mach_call(krb5_context context, struct kcmio *io, void *data,
                size_t len, void **reply_out, size_t *len_out)
{
    krb5_error_code ret;
    size_t inband_req_len = 0, outband_req_len = 0, reply_len;
    char *inband_req = NULL, *outband_req = NULL, *outband_reply, *copy;
    char inband_reply[MAX_INBAND_SIZE];
    mach_msg_type_number_t inband_reply_len, outband_reply_len;
    const void *reply;
    kern_return_t st;
    int code;

    *reply_out = NULL;
    *len_out = 0;

    /* Use the in-band or out-of-band request buffer depending on len. */
    if (len <= MAX_INBAND_SIZE) {
        inband_req = data;
        inband_req_len = len;
    } else {
        outband_req = data;
        outband_req_len = len;
    }

    st = k5_kcmrpc_call(io->mport, inband_req, inband_req_len, outband_req,
                        outband_req_len, &code, inband_reply,
                        &inband_reply_len, &outband_reply, &outband_reply_len);
    if (st == MACH_SEND_INVALID_DEST) {
        /* Get a new port and try again. */
        st = kcmio_mach_connect(context, io);
        if (st)
            return KRB5_KCM_RPC_ERROR;
        st = k5_kcmrpc_call(io->mport, inband_req, inband_req_len, outband_req,
                            outband_req_len, &code, inband_reply,
                            &inband_reply_len, &outband_reply,
                            &outband_reply_len);
    }
    if (st)
        return KRB5_KCM_RPC_ERROR;

    if (code) {
        ret = code;
        goto cleanup;
    }

    /* The reply could be in the in-band or out-of-band reply buffer. */
    reply = outband_reply_len ? outband_reply : inband_reply;
    reply_len = outband_reply_len ? outband_reply_len : inband_reply_len;
    copy = k5memdup(reply, reply_len, &ret);
    if (copy == NULL)
        goto cleanup;

    *reply_out = copy;
    *len_out = reply_len;

cleanup:
    if (outband_reply_len) {
        vm_deallocate(mach_task_self(), (vm_address_t)outband_reply,
                      outband_reply_len);
    }
    return ret;
}

/* Release any Mach RPC state within io. */
static void
kcmio_mach_close(struct kcmio *io)
{
    if (io->mport != MACH_PORT_NULL)
        mach_port_deallocate(mach_task_self(), io->mport);
}

#else /* __APPLE__ */

#define kcmio_mach_connect(context, io) EINVAL
#define kcmio_mach_call(context, io, data, len, reply_out, len_out) EINVAL
#define kcmio_mach_close(io)

#endif

/* Connect to the KCM daemon via a Unix domain socket. */
static krb5_error_code
kcmio_unix_socket_connect(krb5_context context, struct kcmio *io)
{
    krb5_error_code ret;
    int fd = -1;
    struct sockaddr_un addr;
    char *path = NULL;

    ret = profile_get_string(context->profile, KRB5_CONF_LIBDEFAULTS,
                             KRB5_CONF_KCM_SOCKET, NULL,
                             DEFAULT_KCM_SOCKET_PATH, &path);
    if (ret)
        goto cleanup;
    if (strcmp(path, "-") == 0) {
        ret = KRB5_KCM_NO_SERVER;
        goto cleanup;
    }

    fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd == -1) {
        ret = errno;
        goto cleanup;
    }

    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strlcpy(addr.sun_path, path, sizeof(addr.sun_path));
    if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) != 0) {
        ret = (errno == ENOENT) ? KRB5_KCM_NO_SERVER : errno;
        goto cleanup;
    }

    io->fd = fd;
    fd = -1;

cleanup:
    if (fd != -1)
        close(fd);
    profile_release_string(path);
    return ret;
}

/* Write a KCM request: 4-byte big-endian length, then the marshalled
 * request. */
static krb5_error_code
kcmio_unix_socket_write(krb5_context context, struct kcmio *io, void *request,
                        size_t len)
{
    char lenbytes[4];

    store_32_be(len, lenbytes);
    if (krb5_net_write(context, io->fd, lenbytes, 4) < 0)
        return errno;
    if (krb5_net_write(context, io->fd, request, len) < 0)
        return errno;
    return 0;
}

/* Read a KCM reply: 4-byte big-endian length, 4-byte big-endian status code,
 * then the marshalled reply. */
static krb5_error_code
kcmio_unix_socket_read(krb5_context context, struct kcmio *io,
                       void **reply_out, size_t *len_out)
{
    krb5_error_code code;
    char lenbytes[4], codebytes[4], *reply;
    size_t len;
    int st;

    *reply_out = NULL;
    *len_out = 0;

    st = krb5_net_read(context, io->fd, lenbytes, 4);
    if (st != 4)
        return (st == -1) ? errno : KRB5_CC_IO;
    len = load_32_be(lenbytes);
    if (len > MAX_REPLY_SIZE)
        return KRB5_KCM_REPLY_TOO_BIG;

    st = krb5_net_read(context, io->fd, codebytes, 4);
    if (st != 4)
        return (st == -1) ? errno : KRB5_CC_IO;
    code = load_32_be(codebytes);
    if (code != 0)
        return code;

    reply = malloc(len);
    if (reply == NULL)
        return ENOMEM;
    st = krb5_net_read(context, io->fd, reply, len);
    if (st == -1 || (size_t)st != len) {
        free(reply);
        return (st < 0) ? errno : KRB5_CC_IO;
    }

    *reply_out = reply;
    *len_out = len;
    return 0;
}

static krb5_error_code
kcmio_connect(krb5_context context, struct kcmio **io_out)
{
    krb5_error_code ret;
    struct kcmio *io;

    *io_out = NULL;
    io = calloc(1, sizeof(*io));
    if (io == NULL)
        return ENOMEM;
    io->fd = -1;

    /* Try Mach RPC (OS X only), then fall back to Unix domain sockets */
    ret = kcmio_mach_connect(context, io);
    if (ret)
        ret = kcmio_unix_socket_connect(context, io);
    if (ret) {
        free(io);
        return ret;
    }

    *io_out = io;
    return 0;
}

/* Check req->reqbuf for an error condition and return it.  Otherwise, send the
 * request to the KCM daemon and get a response. */
static krb5_error_code
kcmio_call(krb5_context context, struct kcmio *io, struct kcmreq *req)
{
    krb5_error_code ret;
    size_t reply_len = 0;

    if (k5_buf_status(&req->reqbuf) != 0)
        return ENOMEM;

    if (io->fd != -1) {
        ret = kcmio_unix_socket_write(context, io, req->reqbuf.data,
                                      req->reqbuf.len);
        if (ret)
            return ret;
        ret = kcmio_unix_socket_read(context, io, &req->reply_mem, &reply_len);
        if (ret)
            return ret;
    } else {
        /* We must be using Mach RPC. */
        ret = kcmio_mach_call(context, io, req->reqbuf.data, req->reqbuf.len,
                              &req->reply_mem, &reply_len);
        if (ret)
            return ret;
    }

    /* Read the status code from the marshalled reply. */
    k5_input_init(&req->reply, req->reply_mem, reply_len);
    ret = k5_input_get_uint32_be(&req->reply);
    return req->reply.status ? KRB5_KCM_MALFORMED_REPLY : ret;
}

static void
kcmio_close(struct kcmio *io)
{
    if (io != NULL) {
        kcmio_mach_close(io);
        if (io->fd != -1)
            close(io->fd);
        free(io);
    }
}

/* Fetch a zero-terminated name string from req->reply.  The returned pointer
 * is an alias and must not be freed by the caller. */
static krb5_error_code
kcmreq_get_name(struct kcmreq *req, const char **name_out)
{
    const unsigned char *end;
    struct k5input *in = &req->reply;

    *name_out = NULL;
    end = memchr(in->ptr, '\0', in->len);
    if (end == NULL)
        return KRB5_KCM_MALFORMED_REPLY;
    *name_out = (const char *)in->ptr;
    (void)k5_input_get_bytes(in, end + 1 - in->ptr);
    return 0;
}

/* Fetch a UUID list from req->reply.  UUID lists are not delimited, so we
 * consume the rest of the input. */
static krb5_error_code
kcmreq_get_uuid_list(struct kcmreq *req, struct uuid_list **uuids_out)
{
    struct uuid_list *uuids;

    *uuids_out = NULL;

    if (req->reply.len % KCM_UUID_LEN != 0)
        return KRB5_KCM_MALFORMED_REPLY;

    uuids = malloc(sizeof(*uuids));
    if (uuids == NULL)
        return ENOMEM;
    uuids->count = req->reply.len / KCM_UUID_LEN;
    uuids->pos = 0;

    if (req->reply.len > 0) {
        uuids->uuidbytes = malloc(req->reply.len);
        if (uuids->uuidbytes == NULL) {
            free(uuids);
            return ENOMEM;
        }
        memcpy(uuids->uuidbytes, req->reply.ptr, req->reply.len);
        (void)k5_input_get_bytes(&req->reply, req->reply.len);
    } else {
        uuids->uuidbytes = NULL;
    }

    *uuids_out = uuids;
    return 0;
}

static void
free_uuid_list(struct uuid_list *uuids)
{
    if (uuids != NULL)
        free(uuids->uuidbytes);
    free(uuids);
}

static void
kcmreq_free(struct kcmreq *req)
{
    k5_buf_free(&req->reqbuf);
    free(req->reply_mem);
}

/* Create a krb5_ccache structure.  If io is NULL, make a new connection for
 * the cache.  Otherwise, always take ownership of io. */
static krb5_error_code
make_cache(krb5_context context, const char *residual, struct kcmio *io,
           krb5_ccache *cache_out)
{
    krb5_error_code ret;
    krb5_ccache cache = NULL;
    struct kcm_cache_data *data = NULL;
    char *residual_copy = NULL;

    *cache_out = NULL;

    if (io == NULL) {
        ret = kcmio_connect(context, &io);
        if (ret)
            return ret;
    }

    cache = malloc(sizeof(*cache));
    if (cache == NULL)
        goto oom;
    data = calloc(1, sizeof(*data));
    if (data == NULL)
        goto oom;
    residual_copy = strdup(residual);
    if (residual_copy == NULL)
        goto oom;
    if (k5_cc_mutex_init(&data->lock) != 0)
        goto oom;

    data->residual = residual_copy;
    data->io = io;
    data->changetime = 0;
    cache->ops = &krb5_kcm_ops;
    cache->data = data;
    cache->magic = KV5M_CCACHE;
    *cache_out = cache;
    return 0;

oom:
    free(cache);
    free(data);
    free(residual_copy);
    kcmio_close(io);
    return ENOMEM;
}

/* Lock cache's I/O structure and use it to call the KCM daemon.  If modify is
 * true, update the last change time. */
static krb5_error_code
cache_call(krb5_context context, krb5_ccache cache, struct kcmreq *req,
           krb5_boolean modify)
{
    krb5_error_code ret;
    struct kcm_cache_data *data = cache->data;

    k5_cc_mutex_lock(context, &data->lock);
    ret = kcmio_call(context, data->io, req);
    if (modify && !ret)
        data->changetime = time(NULL);
    k5_cc_mutex_unlock(context, &data->lock);
    return ret;
}

/* Try to propagate the KDC time offset from the cache to the krb5 context. */
static void
get_kdc_offset(krb5_context context, krb5_ccache cache)
{
    struct kcmreq req = EMPTY_KCMREQ;
    int32_t time_offset;

    kcmreq_init(&req, KCM_OP_GET_KDC_OFFSET, cache);
    if (cache_call(context, cache, &req, FALSE) != 0)
        goto cleanup;
    time_offset = k5_input_get_uint32_be(&req.reply);
    if (!req.reply.status)
        goto cleanup;
    context->os_context.time_offset = time_offset;
    context->os_context.usec_offset = 0;
    context->os_context.os_flags &= ~KRB5_OS_TOFFSET_TIME;
    context->os_context.os_flags |= KRB5_OS_TOFFSET_VALID;

cleanup:
    kcmreq_free(&req);
}

/* Try to propagate the KDC offset from the krb5 context to the cache. */
static void
set_kdc_offset(krb5_context context, krb5_ccache cache)
{
    struct kcmreq req;

    if (context->os_context.os_flags & KRB5_OS_TOFFSET_VALID) {
        kcmreq_init(&req, KCM_OP_SET_KDC_OFFSET, cache);
        kcmreq_put32(&req, context->os_context.time_offset);
        (void)cache_call(context, cache, &req, TRUE);
        kcmreq_free(&req);
    }
}

static const char * KRB5_CALLCONV
kcm_get_name(krb5_context context, krb5_ccache cache)
{
    return ((struct kcm_cache_data *)cache->data)->residual;
}

static krb5_error_code KRB5_CALLCONV
kcm_resolve(krb5_context context, krb5_ccache *cache_out, const char *residual)
{
    krb5_error_code ret;
    struct kcmreq req = EMPTY_KCMREQ;
    struct kcmio *io = NULL;
    const char *defname = NULL;

    *cache_out = NULL;

    ret = kcmio_connect(context, &io);
    if (ret)
        goto cleanup;

    if (*residual == '\0') {
        kcmreq_init(&req, KCM_OP_GET_DEFAULT_CACHE, NULL);
        ret = kcmio_call(context, io, &req);
        if (ret)
            goto cleanup;
        ret = kcmreq_get_name(&req, &defname);
        if (ret)
            goto cleanup;
        residual = defname;
    }

    ret = make_cache(context, residual, io, cache_out);
    io = NULL;

cleanup:
    kcmio_close(io);
    kcmreq_free(&req);
    return ret;
}

static krb5_error_code KRB5_CALLCONV
kcm_gen_new(krb5_context context, krb5_ccache *cache_out)
{
    krb5_error_code ret;
    struct kcmreq req = EMPTY_KCMREQ;
    struct kcmio *io = NULL;
    const char *name;

    *cache_out = NULL;

    ret = kcmio_connect(context, &io);
    if (ret)
        goto cleanup;
    kcmreq_init(&req, KCM_OP_GEN_NEW, NULL);
    ret = kcmio_call(context, io, &req);
    if (ret)
        goto cleanup;
    ret = kcmreq_get_name(&req, &name);
    if (ret)
        goto cleanup;
    ret = make_cache(context, name, io, cache_out);
    io = NULL;

cleanup:
    kcmreq_free(&req);
    kcmio_close(io);
    return ret;
}

static krb5_error_code KRB5_CALLCONV
kcm_initialize(krb5_context context, krb5_ccache cache, krb5_principal princ)
{
    krb5_error_code ret;
    struct kcmreq req;

    kcmreq_init(&req, KCM_OP_INITIALIZE, cache);
    k5_marshal_princ(&req.reqbuf, 4, princ);
    ret = cache_call(context, cache, &req, TRUE);
    kcmreq_free(&req);
    set_kdc_offset(context, cache);
    return ret;
}

static krb5_error_code KRB5_CALLCONV
kcm_close(krb5_context context, krb5_ccache cache)
{
    struct kcm_cache_data *data = cache->data;

    k5_cc_mutex_destroy(&data->lock);
    kcmio_close(data->io);
    free(data->residual);
    free(data);
    free(cache);
    return 0;
}

static krb5_error_code KRB5_CALLCONV
kcm_destroy(krb5_context context, krb5_ccache cache)
{
    krb5_error_code ret;
    struct kcmreq req;

    kcmreq_init(&req, KCM_OP_DESTROY, cache);
    ret = cache_call(context, cache, &req, TRUE);
    kcmreq_free(&req);
    (void)kcm_close(context, cache);
    return ret;
}

static krb5_error_code KRB5_CALLCONV
kcm_store(krb5_context context, krb5_ccache cache, krb5_creds *cred)
{
    krb5_error_code ret;
    struct kcmreq req;

    kcmreq_init(&req, KCM_OP_STORE, cache);
    k5_marshal_cred(&req.reqbuf, 4, cred);
    ret = cache_call(context, cache, &req, TRUE);
    kcmreq_free(&req);
    return ret;
}

static krb5_error_code KRB5_CALLCONV
kcm_retrieve(krb5_context context, krb5_ccache cache, krb5_flags flags,
             krb5_creds *mcred, krb5_creds *cred_out)
{
    /* There is a KCM opcode for retrieving creds, but Heimdal's client doesn't
     * use it.  It causes the KCM daemon to actually make a TGS request. */
    return k5_cc_retrieve_cred_default(context, cache, flags, mcred, cred_out);
}

static krb5_error_code KRB5_CALLCONV
kcm_get_princ(krb5_context context, krb5_ccache cache,
              krb5_principal *princ_out)
{
    krb5_error_code ret;
    struct kcmreq req;

    kcmreq_init(&req, KCM_OP_GET_PRINCIPAL, cache);
    ret = cache_call(context, cache, &req, FALSE);
    /* Heimdal KCM can respond with code 0 and no principal. */
    if (!ret && req.reply.len == 0)
        ret = KRB5_FCC_NOFILE;
    if (!ret)
        ret = k5_unmarshal_princ(req.reply.ptr, req.reply.len, 4, princ_out);
    kcmreq_free(&req);
    return map_invalid(ret);
}

static krb5_error_code KRB5_CALLCONV
kcm_start_seq_get(krb5_context context, krb5_ccache cache,
                  krb5_cc_cursor *cursor_out)
{
    krb5_error_code ret;
    struct kcmreq req = EMPTY_KCMREQ;
    struct uuid_list *uuids;

    *cursor_out = NULL;

    get_kdc_offset(context, cache);

    kcmreq_init(&req, KCM_OP_GET_CRED_UUID_LIST, cache);
    ret = cache_call(context, cache, &req, FALSE);
    if (ret)
        goto cleanup;
    ret = kcmreq_get_uuid_list(&req, &uuids);
    if (ret)
        goto cleanup;
    *cursor_out = (krb5_cc_cursor)uuids;

cleanup:
    kcmreq_free(&req);
    return ret;
}

static krb5_error_code KRB5_CALLCONV
kcm_next_cred(krb5_context context, krb5_ccache cache, krb5_cc_cursor *cursor,
              krb5_creds *cred_out)
{
    krb5_error_code ret;
    struct kcmreq req;
    struct uuid_list *uuids = (struct uuid_list *)*cursor;

    memset(cred_out, 0, sizeof(*cred_out));

    if (uuids->pos >= uuids->count)
        return KRB5_CC_END;

    kcmreq_init(&req, KCM_OP_GET_CRED_BY_UUID, cache);
    k5_buf_add_len(&req.reqbuf, uuids->uuidbytes + (uuids->pos * KCM_UUID_LEN),
                   KCM_UUID_LEN);
    uuids->pos++;
    ret = cache_call(context, cache, &req, FALSE);
    if (!ret)
        ret = k5_unmarshal_cred(req.reply.ptr, req.reply.len, 4, cred_out);
    kcmreq_free(&req);
    return map_invalid(ret);
}

static krb5_error_code KRB5_CALLCONV
kcm_end_seq_get(krb5_context context, krb5_ccache cache,
                krb5_cc_cursor *cursor)
{
    free_uuid_list((struct uuid_list *)*cursor);
    *cursor = NULL;
    return 0;
}

static krb5_error_code KRB5_CALLCONV
kcm_remove_cred(krb5_context context, krb5_ccache cache, krb5_flags flags,
                krb5_creds *mcred)
{
    krb5_error_code ret;
    struct kcmreq req;

    kcmreq_init(&req, KCM_OP_REMOVE_CRED, cache);
    kcmreq_put32(&req, flags);
    k5_marshal_mcred(&req.reqbuf, mcred);
    ret = cache_call(context, cache, &req, TRUE);
    kcmreq_free(&req);
    return ret;
}

static krb5_error_code KRB5_CALLCONV
kcm_set_flags(krb5_context context, krb5_ccache cache, krb5_flags flags)
{
    /* We don't currently care about any flags for this type. */
    return 0;
}

static krb5_error_code KRB5_CALLCONV
kcm_get_flags(krb5_context context, krb5_ccache cache, krb5_flags *flags_out)
{
    /* We don't currently have any operational flags for this type. */
    *flags_out = 0;
    return 0;
}

/* Construct a per-type cursor, always taking ownership of io and uuids. */
static krb5_error_code
make_ptcursor(const char *residual, struct uuid_list *uuids, struct kcmio *io,
              krb5_cc_ptcursor *cursor_out)
{
    krb5_cc_ptcursor cursor = NULL;
    struct kcm_ptcursor *data = NULL;
    char *residual_copy = NULL;

    *cursor_out = NULL;

    if (residual != NULL) {
        residual_copy = strdup(residual);
        if (residual_copy == NULL)
            goto oom;
    }
    cursor = malloc(sizeof(*cursor));
    if (cursor == NULL)
        goto oom;
    data = malloc(sizeof(*data));
    if (data == NULL)
        goto oom;

    data->residual = residual_copy;
    data->uuids = uuids;
    data->io = io;
    data->first = TRUE;
    cursor->ops = &krb5_kcm_ops;
    cursor->data = data;
    *cursor_out = cursor;
    return 0;

oom:
    kcmio_close(io);
    free_uuid_list(uuids);
    free(residual_copy);
    free(data);
    free(cursor);
    return ENOMEM;
}

static krb5_error_code KRB5_CALLCONV
kcm_ptcursor_new(krb5_context context, krb5_cc_ptcursor *cursor_out)
{
    krb5_error_code ret;
    struct kcmreq req = EMPTY_KCMREQ;
    struct kcmio *io = NULL;
    struct uuid_list *uuids = NULL;
    const char *defname, *primary;

    *cursor_out = NULL;

    /* Don't try to use KCM for the cache collection unless the default cache
     * name has the KCM type. */
    defname = krb5_cc_default_name(context);
    if (defname == NULL || strncmp(defname, "KCM:", 4) != 0)
        return make_ptcursor(NULL, NULL, NULL, cursor_out);

    ret = kcmio_connect(context, &io);
    if (ret)
        return ret;

    /* If defname is a subsidiary cache, return a singleton cursor. */
    if (strlen(defname) > 4)
        return make_ptcursor(defname + 4, NULL, io, cursor_out);

    kcmreq_init(&req, KCM_OP_GET_CACHE_UUID_LIST, NULL);
    ret = kcmio_call(context, io, &req);
    if (ret == KRB5_FCC_NOFILE) {
        /* There are no accessible caches; return an empty cursor. */
        ret = make_ptcursor(NULL, NULL, NULL, cursor_out);
        goto cleanup;
    }
    if (ret)
        goto cleanup;
    ret = kcmreq_get_uuid_list(&req, &uuids);
    if (ret)
        goto cleanup;

    kcmreq_free(&req);
    kcmreq_init(&req, KCM_OP_GET_DEFAULT_CACHE, NULL);
    ret = kcmio_call(context, io, &req);
    if (ret)
        goto cleanup;
    ret = kcmreq_get_name(&req, &primary);
    if (ret)
        goto cleanup;

    ret = make_ptcursor(primary, uuids, io, cursor_out);
    uuids = NULL;
    io = NULL;

cleanup:
    free_uuid_list(uuids);
    kcmio_close(io);
    kcmreq_free(&req);
    return ret;
}

/* Return true if name is an initialized cache. */
static krb5_boolean
name_exists(krb5_context context, struct kcmio *io, const char *name)
{
    krb5_error_code ret;
    struct kcmreq req;

    kcmreq_init(&req, KCM_OP_GET_PRINCIPAL, NULL);
    k5_buf_add_len(&req.reqbuf, name, strlen(name) + 1);
    ret = kcmio_call(context, io, &req);
    kcmreq_free(&req);
    return ret == 0;
}

static krb5_error_code KRB5_CALLCONV
kcm_ptcursor_next(krb5_context context, krb5_cc_ptcursor cursor,
                  krb5_ccache *cache_out)
{
    krb5_error_code ret = 0;
    struct kcmreq req = EMPTY_KCMREQ;
    struct kcm_ptcursor *data = cursor->data;
    struct uuid_list *uuids;
    const unsigned char *id;
    const char *name;

    *cache_out = NULL;

    /* Return the primary or specified subsidiary cache if we haven't yet. */
    if (data->first && data->residual != NULL) {
        data->first = FALSE;
        if (name_exists(context, data->io, data->residual))
            return make_cache(context, data->residual, NULL, cache_out);
    }

    uuids = data->uuids;
    if (uuids == NULL)
        return 0;

    while (uuids->pos < uuids->count) {
        /* Get the name of the next cache. */
        id = &uuids->uuidbytes[KCM_UUID_LEN * uuids->pos++];
        kcmreq_free(&req);
        kcmreq_init(&req, KCM_OP_GET_CACHE_BY_UUID, NULL);
        k5_buf_add_len(&req.reqbuf, id, KCM_UUID_LEN);
        ret = kcmio_call(context, data->io, &req);
        if (ret)
            goto cleanup;
        ret = kcmreq_get_name(&req, &name);
        if (ret)
            goto cleanup;

        /* Don't yield the primary cache twice. */
        if (strcmp(name, data->residual) == 0)
            continue;

        ret = make_cache(context, name, NULL, cache_out);
        break;
    }

cleanup:
    kcmreq_free(&req);
    return ret;
}

static krb5_error_code KRB5_CALLCONV
kcm_ptcursor_free(krb5_context context, krb5_cc_ptcursor *cursor)
{
    struct kcm_ptcursor *data = (*cursor)->data;

    free(data->residual);
    free_uuid_list(data->uuids);
    kcmio_close(data->io);
    free(data);
    free(*cursor);
    *cursor = NULL;
    return 0;
}

static krb5_error_code KRB5_CALLCONV
kcm_lastchange(krb5_context context, krb5_ccache cache,
               krb5_timestamp *time_out)
{
    struct kcm_cache_data *data = cache->data;

    /*
     * KCM has no support for retrieving the last change time.  Return the time
     * of the last change made through this handle, which isn't very useful,
     * but is the best we can do for now.
     */
    k5_cc_mutex_lock(context, &data->lock);
    *time_out = data->changetime;
    k5_cc_mutex_unlock(context, &data->lock);
    return 0;
}

static krb5_error_code KRB5_CALLCONV
kcm_lock(krb5_context context, krb5_ccache cache)
{
    k5_cc_mutex_lock(context, &((struct kcm_cache_data *)cache->data)->lock);
    return 0;
}

static krb5_error_code KRB5_CALLCONV
kcm_unlock(krb5_context context, krb5_ccache cache)
{
    k5_cc_mutex_unlock(context, &((struct kcm_cache_data *)cache->data)->lock);
    return 0;
}

static krb5_error_code KRB5_CALLCONV
kcm_switch_to(krb5_context context, krb5_ccache cache)
{
    krb5_error_code ret;
    struct kcmreq req;

    kcmreq_init(&req, KCM_OP_SET_DEFAULT_CACHE, cache);
    ret = cache_call(context, cache, &req, FALSE);
    kcmreq_free(&req);
    return ret;
}

const krb5_cc_ops krb5_kcm_ops = {
    0,
    "KCM",
    kcm_get_name,
    kcm_resolve,
    kcm_gen_new,
    kcm_initialize,
    kcm_destroy,
    kcm_close,
    kcm_store,
    kcm_retrieve,
    kcm_get_princ,
    kcm_start_seq_get,
    kcm_next_cred,
    kcm_end_seq_get,
    kcm_remove_cred,
    kcm_set_flags,
    kcm_get_flags,
    kcm_ptcursor_new,
    kcm_ptcursor_next,
    kcm_ptcursor_free,
    NULL, /* move */
    kcm_lastchange,
    NULL, /* wasdefault */
    kcm_lock,
    kcm_unlock,
    kcm_switch_to,
};

#endif /* not _WIN32 */
