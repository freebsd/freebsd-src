/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* lib/krad/attrset.c - RADIUS attribute set functions for libkrad */
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

K5_TAILQ_HEAD(attr_head, attr_st);

typedef struct attr_st attr;
struct attr_st {
    K5_TAILQ_ENTRY(attr_st) list;
    krad_attr type;
    krb5_data attr;
    char buffer[MAX_ATTRSIZE];
};

struct krad_attrset_st {
    krb5_context ctx;
    struct attr_head list;
};

krb5_error_code
krad_attrset_new(krb5_context ctx, krad_attrset **set)
{
    krad_attrset *tmp;

    tmp = calloc(1, sizeof(krad_attrset));
    if (tmp == NULL)
        return ENOMEM;
    tmp->ctx = ctx;
    K5_TAILQ_INIT(&tmp->list);

    *set = tmp;
    return 0;
}

void
krad_attrset_free(krad_attrset *set)
{
    attr *a;

    if (set == NULL)
        return;

    while (!K5_TAILQ_EMPTY(&set->list)) {
        a = K5_TAILQ_FIRST(&set->list);
        K5_TAILQ_REMOVE(&set->list, a, list);
        zap(a->buffer, sizeof(a->buffer));
        free(a);
    }

    free(set);
}

krb5_error_code
krad_attrset_add(krad_attrset *set, krad_attr type, const krb5_data *data)
{
    krb5_error_code retval;
    attr *tmp;

    retval = kr_attr_valid(type, data);
    if (retval != 0)
        return retval;

    tmp = calloc(1, sizeof(attr));
    if (tmp == NULL)
        return ENOMEM;

    tmp->type = type;
    tmp->attr = make_data(tmp->buffer, data->length);
    memcpy(tmp->attr.data, data->data, data->length);

    K5_TAILQ_INSERT_TAIL(&set->list, tmp, list);
    return 0;
}

krb5_error_code
krad_attrset_add_number(krad_attrset *set, krad_attr type, krb5_ui_4 num)
{
    krb5_data data;

    num = htonl(num);
    data = make_data(&num, sizeof(num));
    return krad_attrset_add(set, type, &data);
}

void
krad_attrset_del(krad_attrset *set, krad_attr type, size_t indx)
{
    attr *a;

    K5_TAILQ_FOREACH(a, &set->list, list) {
        if (a->type == type && indx-- == 0) {
            K5_TAILQ_REMOVE(&set->list, a, list);
            zap(a->buffer, sizeof(a->buffer));
            free(a);
            return;
        }
    }
}

const krb5_data *
krad_attrset_get(const krad_attrset *set, krad_attr type, size_t indx)
{
    attr *a;

    K5_TAILQ_FOREACH(a, &set->list, list) {
        if (a->type == type && indx-- == 0)
            return &a->attr;
    }

    return NULL;
}

krb5_error_code
krad_attrset_copy(const krad_attrset *set, krad_attrset **copy)
{
    krb5_error_code retval;
    krad_attrset *tmp;
    attr *a;

    retval = krad_attrset_new(set->ctx, &tmp);
    if (retval != 0)
        return retval;

    K5_TAILQ_FOREACH(a, &set->list, list) {
        retval = krad_attrset_add(tmp, a->type, &a->attr);
        if (retval != 0) {
            krad_attrset_free(tmp);
            return retval;
        }
    }

    *copy = tmp;
    return 0;
}

krb5_error_code
kr_attrset_encode(const krad_attrset *set, const char *secret,
                  const unsigned char *auth,
                  unsigned char outbuf[MAX_ATTRSETSIZE], size_t *outlen)
{
    unsigned char buffer[MAX_ATTRSIZE];
    krb5_error_code retval;
    size_t i = 0, attrlen;
    attr *a;

    if (set == NULL) {
        *outlen = 0;
        return 0;
    }

    K5_TAILQ_FOREACH(a, &set->list, list) {
        retval = kr_attr_encode(set->ctx, secret, auth, a->type, &a->attr,
                                buffer, &attrlen);
        if (retval != 0)
            return retval;

        if (i + attrlen + 2 > MAX_ATTRSETSIZE)
            return EMSGSIZE;

        outbuf[i++] = a->type;
        outbuf[i++] = attrlen + 2;
        memcpy(&outbuf[i], buffer, attrlen);
        i += attrlen;
    }

    *outlen = i;
    return 0;
}

krb5_error_code
kr_attrset_decode(krb5_context ctx, const krb5_data *in, const char *secret,
                  const unsigned char *auth, krad_attrset **set_out)
{
    unsigned char buffer[MAX_ATTRSIZE];
    krb5_data tmp;
    krb5_error_code retval;
    krad_attr type;
    krad_attrset *set;
    size_t i, len;

    *set_out = NULL;

    retval = krad_attrset_new(ctx, &set);
    if (retval != 0)
        return retval;

    for (i = 0; i + 2 < in->length; ) {
        type = in->data[i++];
        tmp = make_data(&in->data[i + 1], (uint8_t)in->data[i] - 2);
        i += tmp.length + 1;

        retval = (in->length < i) ? EBADMSG : 0;
        if (retval != 0)
            goto cleanup;

        retval = kr_attr_decode(ctx, secret, auth, type, &tmp, buffer, &len);
        if (retval != 0)
            goto cleanup;

        tmp = make_data(buffer, len);
        retval = krad_attrset_add(set, type, &tmp);
        if (retval != 0)
            goto cleanup;
    }

    *set_out = set;
    set = NULL;

cleanup:
    zap(buffer, sizeof(buffer));
    krad_attrset_free(set);
    return retval;
}
