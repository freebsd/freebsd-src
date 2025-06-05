/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* lib/krad/attr.c - RADIUS attribute functions for libkrad */
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
#include "internal.h"

#include <string.h>

/* RFC 2865 */
#define BLOCKSIZE 16

typedef krb5_error_code
(*attribute_transform_fn)(krb5_context ctx, const char *secret,
                          const unsigned char *auth, const krb5_data *in,
                          unsigned char outbuf[MAX_ATTRSIZE], size_t *outlen);

typedef struct {
    const char *name;
    unsigned char minval;
    unsigned char maxval;
    attribute_transform_fn encode;
    attribute_transform_fn decode;
} attribute_record;

static krb5_error_code
user_password_encode(krb5_context ctx, const char *secret,
                     const unsigned char *auth, const krb5_data *in,
                     unsigned char outbuf[MAX_ATTRSIZE], size_t *outlen);

static krb5_error_code
user_password_decode(krb5_context ctx, const char *secret,
                     const unsigned char *auth, const krb5_data *in,
                     unsigned char outbuf[MAX_ATTRSIZE], size_t *outlen);

static const attribute_record attributes[UCHAR_MAX] = {
    {"User-Name", 1, MAX_ATTRSIZE, NULL, NULL},
    {"User-Password", 1, 128, user_password_encode, user_password_decode},
    {"CHAP-Password", 17, 17, NULL, NULL},
    {"NAS-IP-Address", 4, 4, NULL, NULL},
    {"NAS-Port", 4, 4, NULL, NULL},
    {"Service-Type", 4, 4, NULL, NULL},
    {"Framed-Protocol", 4, 4, NULL, NULL},
    {"Framed-IP-Address", 4, 4, NULL, NULL},
    {"Framed-IP-Netmask", 4, 4, NULL, NULL},
    {"Framed-Routing", 4, 4, NULL, NULL},
    {"Filter-Id", 1, MAX_ATTRSIZE, NULL, NULL},
    {"Framed-MTU", 4, 4, NULL, NULL},
    {"Framed-Compression", 4, 4, NULL, NULL},
    {"Login-IP-Host", 4, 4, NULL, NULL},
    {"Login-Service", 4, 4, NULL, NULL},
    {"Login-TCP-Port", 4, 4, NULL, NULL},
    {NULL, 0, 0, NULL, NULL}, /* Unassigned */
    {"Reply-Message", 1, MAX_ATTRSIZE, NULL, NULL},
    {"Callback-Number", 1, MAX_ATTRSIZE, NULL, NULL},
    {"Callback-Id", 1, MAX_ATTRSIZE, NULL, NULL},
    {NULL, 0, 0, NULL, NULL}, /* Unassigned */
    {"Framed-Route", 1, MAX_ATTRSIZE, NULL, NULL},
    {"Framed-IPX-Network", 4, 4, NULL, NULL},
    {"State", 1, MAX_ATTRSIZE, NULL, NULL},
    {"Class", 1, MAX_ATTRSIZE, NULL, NULL},
    {"Vendor-Specific", 5, MAX_ATTRSIZE, NULL, NULL},
    {"Session-Timeout", 4, 4, NULL, NULL},
    {"Idle-Timeout", 4, 4, NULL, NULL},
    {"Termination-Action", 4, 4, NULL, NULL},
    {"Called-Station-Id", 1, MAX_ATTRSIZE, NULL, NULL},
    {"Calling-Station-Id", 1, MAX_ATTRSIZE, NULL, NULL},
    {"NAS-Identifier", 1, MAX_ATTRSIZE, NULL, NULL},
    {"Proxy-State", 1, MAX_ATTRSIZE, NULL, NULL},
    {"Login-LAT-Service", 1, MAX_ATTRSIZE, NULL, NULL},
    {"Login-LAT-Node", 1, MAX_ATTRSIZE, NULL, NULL},
    {"Login-LAT-Group", 32, 32, NULL, NULL},
    {"Framed-AppleTalk-Link", 4, 4, NULL, NULL},
    {"Framed-AppleTalk-Network", 4, 4, NULL, NULL},
    {"Framed-AppleTalk-Zone", 1, MAX_ATTRSIZE, NULL, NULL},
    {NULL, 0, 0, NULL, NULL}, /* Reserved for accounting */
    {NULL, 0, 0, NULL, NULL}, /* Reserved for accounting */
    {NULL, 0, 0, NULL, NULL}, /* Reserved for accounting */
    {NULL, 0, 0, NULL, NULL}, /* Reserved for accounting */
    {NULL, 0, 0, NULL, NULL}, /* Reserved for accounting */
    {NULL, 0, 0, NULL, NULL}, /* Reserved for accounting */
    {NULL, 0, 0, NULL, NULL}, /* Reserved for accounting */
    {NULL, 0, 0, NULL, NULL}, /* Reserved for accounting */
    {NULL, 0, 0, NULL, NULL}, /* Reserved for accounting */
    {NULL, 0, 0, NULL, NULL}, /* Reserved for accounting */
    {NULL, 0, 0, NULL, NULL}, /* Reserved for accounting */
    {NULL, 0, 0, NULL, NULL}, /* Reserved for accounting */
    {NULL, 0, 0, NULL, NULL}, /* Reserved for accounting */
    {NULL, 0, 0, NULL, NULL}, /* Reserved for accounting */
    {NULL, 0, 0, NULL, NULL}, /* Reserved for accounting */
    {NULL, 0, 0, NULL, NULL}, /* Reserved for accounting */
    {NULL, 0, 0, NULL, NULL}, /* Reserved for accounting */
    {NULL, 0, 0, NULL, NULL}, /* Reserved for accounting */
    {NULL, 0, 0, NULL, NULL}, /* Reserved for accounting */
    {NULL, 0, 0, NULL, NULL}, /* Reserved for accounting */
    {"CHAP-Challenge", 5, MAX_ATTRSIZE, NULL, NULL},
    {"NAS-Port-Type", 4, 4, NULL, NULL},
    {"Port-Limit", 4, 4, NULL, NULL},
    {"Login-LAT-Port", 1, MAX_ATTRSIZE, NULL, NULL},
};

/* Encode User-Password attribute. */
static krb5_error_code
user_password_encode(krb5_context ctx, const char *secret,
                     const unsigned char *auth, const krb5_data *in,
                     unsigned char outbuf[MAX_ATTRSIZE], size_t *outlen)
{
    const unsigned char *indx;
    krb5_error_code retval;
    unsigned int seclen;
    krb5_checksum sum;
    size_t blck, len, i;
    krb5_data tmp;

    /* Copy the input buffer to the (zero-padded) output buffer. */
    len = (in->length + BLOCKSIZE - 1) / BLOCKSIZE * BLOCKSIZE;
    if (len > MAX_ATTRSIZE)
        return ENOBUFS;
    memset(outbuf, 0, len);
    memcpy(outbuf, in->data, in->length);

    /* Create our temporary space for processing each block. */
    seclen = strlen(secret);
    retval = alloc_data(&tmp, seclen + BLOCKSIZE);
    if (retval != 0)
        return retval;

    memcpy(tmp.data, secret, seclen);
    for (blck = 0, indx = auth; blck * BLOCKSIZE < len; blck++) {
        memcpy(tmp.data + seclen, indx, BLOCKSIZE);

        retval = krb5_c_make_checksum(ctx, CKSUMTYPE_RSA_MD5, NULL, 0, &tmp,
                                      &sum);
        if (retval != 0) {
            zap(tmp.data, tmp.length);
            zap(outbuf, len);
            krb5_free_data_contents(ctx, &tmp);
            return retval;
        }

        for (i = 0; i < BLOCKSIZE; i++)
            outbuf[blck * BLOCKSIZE + i] ^= sum.contents[i];
        krb5_free_checksum_contents(ctx, &sum);

        indx = &outbuf[blck * BLOCKSIZE];
    }

    zap(tmp.data, tmp.length);
    krb5_free_data_contents(ctx, &tmp);
    *outlen = len;
    return 0;
}

/* Decode User-Password attribute. */
static krb5_error_code
user_password_decode(krb5_context ctx, const char *secret,
                     const unsigned char *auth, const krb5_data *in,
                     unsigned char outbuf[MAX_ATTRSIZE], size_t *outlen)
{
    const unsigned char *indx;
    krb5_error_code retval;
    unsigned int seclen;
    krb5_checksum sum;
    ssize_t blck, i;
    krb5_data tmp;

    if (in->length % BLOCKSIZE != 0)
        return EINVAL;
    if (in->length > MAX_ATTRSIZE)
        return ENOBUFS;

    /* Create our temporary space for processing each block. */
    seclen = strlen(secret);
    retval = alloc_data(&tmp, seclen + BLOCKSIZE);
    if (retval != 0)
        return retval;

    memcpy(tmp.data, secret, seclen);
    for (blck = 0, indx = auth; blck * BLOCKSIZE < in->length; blck++) {
        memcpy(tmp.data + seclen, indx, BLOCKSIZE);

        retval = krb5_c_make_checksum(ctx, CKSUMTYPE_RSA_MD5, NULL, 0,
                                      &tmp, &sum);
        if (retval != 0) {
            zap(tmp.data, tmp.length);
            zap(outbuf, in->length);
            krb5_free_data_contents(ctx, &tmp);
            return retval;
        }

        for (i = 0; i < BLOCKSIZE; i++) {
            outbuf[blck * BLOCKSIZE + i] = in->data[blck * BLOCKSIZE + i] ^
                sum.contents[i];
        }
        krb5_free_checksum_contents(ctx, &sum);

        indx = (const unsigned char *)&in->data[blck * BLOCKSIZE];
    }

    /* Strip off trailing NULL bytes. */
    *outlen = in->length;
    while (*outlen > 0 && outbuf[*outlen - 1] == '\0')
        (*outlen)--;

    krb5_free_data_contents(ctx, &tmp);
    return 0;
}

krb5_error_code
kr_attr_valid(krad_attr type, const krb5_data *data)
{
    const attribute_record *ar;

    if (type == 0)
        return EINVAL;

    ar = &attributes[type - 1];
    return (data->length >= ar->minval && data->length <= ar->maxval) ? 0 :
        EMSGSIZE;
}

krb5_error_code
kr_attr_encode(krb5_context ctx, const char *secret,
               const unsigned char *auth, krad_attr type,
               const krb5_data *in, unsigned char outbuf[MAX_ATTRSIZE],
               size_t *outlen)
{
    krb5_error_code retval;

    retval = kr_attr_valid(type, in);
    if (retval != 0)
        return retval;

    if (attributes[type - 1].encode == NULL) {
        if (in->length > MAX_ATTRSIZE)
            return ENOBUFS;

        *outlen = in->length;
        memcpy(outbuf, in->data, in->length);
        return 0;
    }

    return attributes[type - 1].encode(ctx, secret, auth, in, outbuf, outlen);
}

krb5_error_code
kr_attr_decode(krb5_context ctx, const char *secret, const unsigned char *auth,
               krad_attr type, const krb5_data *in,
               unsigned char outbuf[MAX_ATTRSIZE], size_t *outlen)
{
    krb5_error_code retval;

    retval = kr_attr_valid(type, in);
    if (retval != 0)
        return retval;

    if (attributes[type - 1].encode == NULL) {
        if (in->length > MAX_ATTRSIZE)
            return ENOBUFS;

        *outlen = in->length;
        memcpy(outbuf, in->data, in->length);
        return 0;
    }

    return attributes[type - 1].decode(ctx, secret, auth, in, outbuf, outlen);
}

krad_attr
krad_attr_name2num(const char *name)
{
    unsigned char i;

    for (i = 0; i < UCHAR_MAX; i++) {
        if (attributes[i].name == NULL)
            continue;

        if (strcmp(attributes[i].name, name) == 0)
            return i + 1;
    }

    return 0;
}

const char *
krad_attr_num2name(krad_attr type)
{
    if (type == 0)
        return NULL;

    return attributes[type - 1].name;
}
