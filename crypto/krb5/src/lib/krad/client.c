/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* lib/krad/client.c - Client request code for libkrad */
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

#include <k5-queue.h>
#include "internal.h"

#include <string.h>
#include <sys/un.h>
#include <unistd.h>
#include <stdio.h>
#include <limits.h>

K5_LIST_HEAD(server_head, server_st);

typedef struct remote_state_st remote_state;
typedef struct request_st request;
typedef struct server_st server;

struct remote_state_st {
    const krad_packet *packet;
    krad_remote *remote;
};

struct request_st {
    krad_client *rc;

    krad_code code;
    krad_attrset *attrs;
    int timeout;
    size_t retries;
    krad_cb cb;
    void *data;

    remote_state *remotes;
    ssize_t current;
    ssize_t count;
};

struct server_st {
    krad_remote *serv;
    K5_LIST_ENTRY(server_st) list;
};

struct krad_client_st {
    krb5_context kctx;
    verto_ctx *vctx;
    struct server_head servers;
};

/* Return either a pre-existing server that matches the address info and the
 * secret, or create a new one. */
static krb5_error_code
get_server(krad_client *rc, const struct addrinfo *ai, const char *secret,
           krad_remote **out)
{
    krb5_error_code retval;
    server *srv;

    K5_LIST_FOREACH(srv, &rc->servers, list) {
        if (kr_remote_equals(srv->serv, ai, secret)) {
            *out = srv->serv;
            return 0;
        }
    }

    srv = calloc(1, sizeof(server));
    if (srv == NULL)
        return ENOMEM;

    retval = kr_remote_new(rc->kctx, rc->vctx, ai, secret, &srv->serv);
    if (retval != 0) {
        free(srv);
        return retval;
    }

    K5_LIST_INSERT_HEAD(&rc->servers, srv, list);
    *out = srv->serv;
    return 0;
}

/* Free a request. */
static void
request_free(request *req)
{
    krad_attrset_free(req->attrs);
    free(req->remotes);
    free(req);
}

/* Create a request. */
static krb5_error_code
request_new(krad_client *rc, krad_code code, const krad_attrset *attrs,
            const struct addrinfo *ai, const char *secret, int timeout,
            size_t retries, krad_cb cb, void *data, request **req)
{
    const struct addrinfo *tmp;
    krb5_error_code retval;
    request *rqst;
    size_t i;

    if (ai == NULL)
        return EINVAL;

    rqst = calloc(1, sizeof(request));
    if (rqst == NULL)
        return ENOMEM;

    for (tmp = ai; tmp != NULL; tmp = tmp->ai_next)
        rqst->count++;

    rqst->rc = rc;
    rqst->code = code;
    rqst->cb = cb;
    rqst->data = data;
    rqst->timeout = timeout / rqst->count;
    rqst->retries = retries;

    retval = krad_attrset_copy(attrs, &rqst->attrs);
    if (retval != 0) {
        request_free(rqst);
        return retval;
    }

    rqst->remotes = calloc(rqst->count + 1, sizeof(remote_state));
    if (rqst->remotes == NULL) {
        request_free(rqst);
        return ENOMEM;
    }

    i = 0;
    for (tmp = ai; tmp != NULL; tmp = tmp->ai_next) {
        retval = get_server(rc, tmp, secret, &rqst->remotes[i++].remote);
        if (retval != 0) {
            request_free(rqst);
            return retval;
        }
    }

    *req = rqst;
    return 0;
}

/* Handle a response from a server (or related errors). */
static void
on_response(krb5_error_code retval, const krad_packet *reqp,
            const krad_packet *rspp, void *data)
{
    request *req = data;
    size_t i;

    /* Do nothing if we are already completed. */
    if (req->count < 0)
        return;

    /* If we have timed out and have more remotes to try, do so. */
    if (retval == ETIMEDOUT && req->remotes[++req->current].remote != NULL) {
        retval = kr_remote_send(req->remotes[req->current].remote, req->code,
                                req->attrs, on_response, req, req->timeout,
                                req->retries,
                                &req->remotes[req->current].packet);
        if (retval == 0)
            return;
    }

    /* Mark the request as complete. */
    req->count = -1;

    /* Inform the callback. */
    req->cb(retval, reqp, rspp, req->data);

    /* Cancel the outstanding packets. */
    for (i = 0; req->remotes[i].remote != NULL; i++)
        kr_remote_cancel(req->remotes[i].remote, req->remotes[i].packet);

    request_free(req);
}

krb5_error_code
krad_client_new(krb5_context kctx, verto_ctx *vctx, krad_client **out)
{
    krad_client *tmp;

    tmp = calloc(1, sizeof(krad_client));
    if (tmp == NULL)
        return ENOMEM;

    tmp->kctx = kctx;
    tmp->vctx = vctx;

    *out = tmp;
    return 0;
}

void
krad_client_free(krad_client *rc)
{
    server *srv;

    if (rc == NULL)
        return;

    /* Cancel all requests before freeing any remotes, since each request's
     * callback data may contain references to multiple remotes. */
    K5_LIST_FOREACH(srv, &rc->servers, list)
        kr_remote_cancel_all(srv->serv);

    while (!K5_LIST_EMPTY(&rc->servers)) {
        srv = K5_LIST_FIRST(&rc->servers);
        K5_LIST_REMOVE(srv, list);
        kr_remote_free(srv->serv);
        free(srv);
    }

    free(rc);
}

static krb5_error_code
resolve_remote(const char *remote, struct addrinfo **ai)
{
    const char *svc = "radius";
    krb5_error_code retval;
    struct addrinfo hints;
    char *sep, *srv;

    /* Isolate the port number if it exists. */
    srv = strdup(remote);
    if (srv == NULL)
        return ENOMEM;

    if (srv[0] == '[') {
        /* IPv6 */
        sep = strrchr(srv, ']');
        if (sep != NULL && sep[1] == ':') {
            sep[1] = '\0';
            svc = &sep[2];
        }
    } else {
        /* IPv4 or DNS */
        sep = strrchr(srv, ':');
        if (sep != NULL && sep[1] != '\0') {
            sep[0] = '\0';
            svc = &sep[1];
        }
    }

    /* Perform the lookup. */
    memset(&hints, 0, sizeof(hints));
    hints.ai_socktype = SOCK_DGRAM;
    retval = gai_error_code(getaddrinfo(srv, svc, &hints, ai));
    free(srv);
    return retval;
}

krb5_error_code
krad_client_send(krad_client *rc, krad_code code, const krad_attrset *attrs,
                 const char *remote, const char *secret, int timeout,
                 size_t retries, krad_cb cb, void *data)
{
    struct addrinfo usock, *ai = NULL;
    krb5_error_code retval;
    struct sockaddr_un ua;
    request *req;

    if (remote[0] == '/') {
        ua.sun_family = AF_UNIX;
        snprintf(ua.sun_path, sizeof(ua.sun_path), "%s", remote);
        memset(&usock, 0, sizeof(usock));
        usock.ai_family = AF_UNIX;
        usock.ai_socktype = SOCK_STREAM;
        usock.ai_addr = (struct sockaddr *)&ua;
        usock.ai_addrlen = sizeof(ua);

        retval = request_new(rc, code, attrs, &usock, secret, timeout, retries,
                             cb, data, &req);
    } else {
        retval = resolve_remote(remote, &ai);
        if (retval == 0) {
            retval = request_new(rc, code, attrs, ai, secret, timeout, retries,
                                 cb, data, &req);
            freeaddrinfo(ai);
        }
    }
    if (retval != 0)
        return retval;

    retval = kr_remote_send(req->remotes[req->current].remote, req->code,
                            req->attrs, on_response, req, req->timeout,
                            req->retries, &req->remotes[req->current].packet);
    if (retval != 0) {
        request_free(req);
        return retval;
    }

    return 0;
}
