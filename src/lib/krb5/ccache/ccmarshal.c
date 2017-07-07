/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* lib/krb5/ccache/ccmarshal.c - Functions for serializing creds */
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
 * This file implements marshalling and unmarshalling of krb5 credentials and
 * principals in versions 1 through 4 of the FILE ccache format.  Version 4 is
 * also used for the KEYRING ccache type.
 *
 * The FILE credential cache format uses fixed 16-bit or 32-bit representations
 * of integers.  In versions 1 and 2 these are in host byte order; in later
 * versions they are in big-endian byte order.  Variable-length fields are
 * represented with a 32-bit length followed by the field value.  There is no
 * type tagging; field representations are simply concatenated together.
 *
 * A psuedo-BNF grammar for the credential and principal formats is:
 *
 * credential ::=
 *   client (principal)
 *   server (principal)
 *   keyblock (keyblock)
 *   authtime (32 bits)
 *   starttime (32 bits)
 *   endtime (32 bits)
 *   renew_till (32 bits)
 *   is_skey (1 byte, 0 or 1)
 *   ticket_flags (32 bits)
 *   addresses (addresses)
 *   authdata (authdata)
 *   ticket (data)
 *   second_ticket (data)
 *
 * principal ::=
 *   name type (32 bits) [omitted in version 1]
 *   count of components (32 bits) [includes realm in version 1]
 *   realm (data)
 *   component1 (data)
 *   component2 (data)
 *   ...
 *
 * keyblock ::=
 *   enctype (16 bits) [repeated twice in version 3; see below]
 *   data
 *
 * addresses ::=
 *   count (32 bits)
 *   address1
 *   address2
 *   ...
 *
 * address ::=
 *   addrtype (16 bits)
 *   data
 *
 * authdata ::=
 *   count (32 bits)
 *   authdata1
 *   authdata2
 *   ...
 *
 * authdata ::=
 *   ad_type (16 bits)
 *   data
 *
 * data ::=
 *   length (32 bits)
 *   value (length bytes)
 *
 * When version 3 was current (before release 1.0), the keyblock had separate
 * key type and enctype fields, and both were recorded.  At present we record
 * the enctype field twice when writing the version 3 format and ignore the
 * second value when reading it.
 */

#include "k5-input.h"
#include "cc-int.h"

/* Read a 16-bit integer in host byte order for versions 1 and 2, or in
 * big-endian byte order for later versions.*/
static uint16_t
get16(struct k5input *in, int version)
{
    return (version < 3) ? k5_input_get_uint16_n(in) :
        k5_input_get_uint16_be(in);
}

/* Read a 32-bit integer in host byte order for versions 1 and 2, or in
 * big-endian byte order for later versions.*/
static uint32_t
get32(struct k5input *in, int version)
{
    return (version < 3) ? k5_input_get_uint32_n(in) :
        k5_input_get_uint32_be(in);
}

/* Read a 32-bit length and make a copy of that many bytes.  Return NULL on
 * error. */
static void *
get_len_bytes(struct k5input *in, int version, unsigned int *len_out)
{
    krb5_error_code ret;
    unsigned int len = get32(in, version);
    const void *bytes = k5_input_get_bytes(in, len);
    void *copy;

    *len_out = 0;
    if (bytes == NULL)
        return NULL;
    copy = k5memdup0(bytes, len, &ret);
    if (copy == NULL) {
        k5_input_set_status(in, ret);
        return NULL;
    }
    *len_out = len;
    return copy;
}

/* Like get_len_bytes, but put the result in data. */
static void
get_data(struct k5input *in, int version, krb5_data *data)
{
    unsigned int len;
    void *bytes = get_len_bytes(in, version, &len);

    *data = (bytes == NULL) ? empty_data() : make_data(bytes, len);
}

static krb5_principal
unmarshal_princ(struct k5input *in, int version)
{
    krb5_error_code ret;
    krb5_principal princ;
    uint32_t i, ncomps;

    princ = k5alloc(sizeof(krb5_principal_data), &ret);
    if (princ == NULL) {
        k5_input_set_status(in, ret);
        return NULL;
    }
    princ->magic = KV5M_PRINCIPAL;
    /* Version 1 does not store the principal name type, and counts the realm
     * in the number of components. */
    princ->type = (version == 1) ? KRB5_NT_UNKNOWN : get32(in, version);
    ncomps = get32(in, version);
    if (version == 1)
        ncomps--;
    if (ncomps > in->len) {     /* Sanity check to avoid large allocations */
        ret = EINVAL;
        goto error;
    }
    if (ncomps != 0) {
        princ->data = k5calloc(ncomps, sizeof(krb5_data), &ret);
        if (princ->data == NULL)
            goto error;
        princ->length = ncomps;
    }
    get_data(in, version, &princ->realm);
    for (i = 0; i < ncomps; i++)
        get_data(in, version, &princ->data[i]);
    return princ;

error:
    k5_input_set_status(in, ret);
    krb5_free_principal(NULL, princ);
    return NULL;
}

static void
unmarshal_keyblock(struct k5input *in, int version, krb5_keyblock *kb)
{
    memset(kb, 0, sizeof(*kb));
    kb->magic = KV5M_KEYBLOCK;
    /* enctypes can be negative, so sign-extend the 16-bit result. */
    kb->enctype = (int16_t)get16(in, version);
    /* Version 3 stores the enctype twice. */
    if (version == 3)
        (void)get16(in, version);
    kb->contents = get_len_bytes(in, version, &kb->length);
}

static krb5_address *
unmarshal_addr(struct k5input *in, int version)
{
    krb5_address *addr;

    addr = calloc(1, sizeof(*addr));
    if (addr == NULL) {
        k5_input_set_status(in, ENOMEM);
        return NULL;
    }
    addr->magic = KV5M_ADDRESS;
    addr->addrtype = get16(in, version);
    addr->contents = get_len_bytes(in, version, &addr->length);
    return addr;
}

static krb5_address **
unmarshal_addrs(struct k5input *in, int version)
{
    krb5_address **addrs;
    size_t i, count;

    count = get32(in, version);
    if (count > in->len) {      /* Sanity check to avoid large allocations */
        k5_input_set_status(in, EINVAL);
        return NULL;
    }
    addrs = calloc(count + 1, sizeof(*addrs));
    if (addrs == NULL) {
        k5_input_set_status(in, ENOMEM);
        return NULL;
    }
    for (i = 0; i < count; i++)
        addrs[i] = unmarshal_addr(in, version);
    return addrs;
}

static krb5_authdata *
unmarshal_authdatum(struct k5input *in, int version)
{
    krb5_authdata *ad;

    ad = calloc(1, sizeof(*ad));
    if (ad == NULL) {
        k5_input_set_status(in, ENOMEM);
        return NULL;
    }
    ad->magic = KV5M_ADDRESS;
    /* Authdata types can be negative, so sign-extend the get16 result. */
    ad->ad_type = (int16_t)get16(in, version);
    ad->contents = get_len_bytes(in, version, &ad->length);
    return ad;
}

static krb5_authdata **
unmarshal_authdata(struct k5input *in, int version)
{
    krb5_authdata **authdata;
    size_t i, count;

    count = get32(in, version);
    if (count > in->len) {      /* Sanity check to avoid large allocations */
        k5_input_set_status(in, EINVAL);
        return NULL;
    }
    authdata = calloc(count + 1, sizeof(*authdata));
    if (authdata == NULL) {
        k5_input_set_status(in, ENOMEM);
        return NULL;
    }
    for (i = 0; i < count; i++)
        authdata[i] = unmarshal_authdatum(in, version);
    return authdata;
}

/* Unmarshal a credential using the specified file ccache version (expressed as
 * an integer from 1 to 4).  Does not check for trailing garbage. */
krb5_error_code
k5_unmarshal_cred(const unsigned char *data, size_t len, int version,
                  krb5_creds *creds)
{
    struct k5input in;

    k5_input_init(&in, data, len);
    creds->client = unmarshal_princ(&in, version);
    creds->server = unmarshal_princ(&in, version);
    unmarshal_keyblock(&in, version, &creds->keyblock);
    creds->times.authtime = get32(&in, version);
    creds->times.starttime = get32(&in, version);
    creds->times.endtime = get32(&in, version);
    creds->times.renew_till = get32(&in, version);
    creds->is_skey = k5_input_get_byte(&in);
    creds->ticket_flags = get32(&in, version);
    creds->addresses = unmarshal_addrs(&in, version);
    creds->authdata = unmarshal_authdata(&in, version);
    get_data(&in, version, &creds->ticket);
    get_data(&in, version, &creds->second_ticket);
    if (in.status) {
        krb5_free_cred_contents(NULL, creds);
        memset(creds, 0, sizeof(*creds));
    }
    return (in.status == EINVAL) ? KRB5_CC_FORMAT : in.status;
}

/* Unmarshal a principal using the specified file ccache version (expressed as
 * an integer from 1 to 4).  Does not check for trailing garbage. */
krb5_error_code
k5_unmarshal_princ(const unsigned char *data, size_t len, int version,
                   krb5_principal *princ_out)
{
    struct k5input in;
    krb5_principal princ;

    *princ_out = NULL;
    k5_input_init(&in, data, len);
    princ = unmarshal_princ(&in, version);
    if (in.status)
        krb5_free_principal(NULL, princ);
    else
        *princ_out = princ;
    return (in.status == EINVAL) ? KRB5_CC_FORMAT : in.status;
}

/* Store a 16-bit integer in host byte order for versions 1 and 2, or in
 * big-endian byte order for later versions.*/
static void
put16(struct k5buf *buf, int version, uint16_t num)
{
    char n[2];

    if (version < 3)
        store_16_n(num, n);
    else
        store_16_be(num, n);
    k5_buf_add_len(buf, n, 2);
}

/* Store a 32-bit integer in host byte order for versions 1 and 2, or in
 * big-endian byte order for later versions.*/
static void
put32(struct k5buf *buf, int version, uint32_t num)
{
    char n[4];

    if (version < 3)
        store_32_n(num, n);
    else
        store_32_be(num, n);
    k5_buf_add_len(buf, n, 4);
}

static void
put_len_bytes(struct k5buf *buf, int version, const void *bytes,
              unsigned int len)
{
    put32(buf, version, len);
    k5_buf_add_len(buf, bytes, len);
}

static void
put_data(struct k5buf *buf, int version, krb5_data *data)
{
    put_len_bytes(buf, version, data->data, data->length);
}

void
k5_marshal_princ(struct k5buf *buf, int version, krb5_principal princ)
{
    int32_t i, ncomps;

    /* Version 1 does not store the principal name type, and counts the realm
     * in the number of components. */
    if (version != 1)
        put32(buf, version, princ->type);
    ncomps = princ->length + ((version == 1) ? 1 : 0);
    put32(buf, version, ncomps);
    put_data(buf, version, &princ->realm);
    for (i = 0; i < princ->length; i++)
        put_data(buf, version, &princ->data[i]);
}

static void
marshal_keyblock(struct k5buf *buf, int version, krb5_keyblock *kb)
{
    put16(buf, version, kb->enctype);
    /* Version 3 stores the enctype twice. */
    if (version == 3)
        put16(buf, version, kb->enctype);
    put_len_bytes(buf, version, kb->contents, kb->length);
}

static void
marshal_addrs(struct k5buf *buf, int version, krb5_address **addrs)
{
    size_t i, count;

    for (count = 0; addrs != NULL && addrs[count] != NULL; count++);
    put32(buf, version, count);
    for (i = 0; i < count; i++) {
        put16(buf, version, addrs[i]->addrtype);
        put_len_bytes(buf, version, addrs[i]->contents, addrs[i]->length);
    }
}

static void
marshal_authdata(struct k5buf *buf, int version, krb5_authdata **authdata)
{
    size_t i, count;

    for (count = 0; authdata != NULL && authdata[count] != NULL; count++);
    put32(buf, version, count);
    for (i = 0; i < count; i++) {
        put16(buf, version, authdata[i]->ad_type);
        put_len_bytes(buf, version, authdata[i]->contents,
                      authdata[i]->length);
    }
}

/* Marshal a credential using the specified file ccache version (expressed as
 * an integer from 1 to 4). */
void
k5_marshal_cred(struct k5buf *buf, int version, krb5_creds *creds)
{
    char is_skey;

    k5_marshal_princ(buf, version, creds->client);
    k5_marshal_princ(buf, version, creds->server);
    marshal_keyblock(buf, version, &creds->keyblock);
    put32(buf, version, creds->times.authtime);
    put32(buf, version, creds->times.starttime);
    put32(buf, version, creds->times.endtime);
    put32(buf, version, creds->times.renew_till);
    is_skey = creds->is_skey;
    k5_buf_add_len(buf, &is_skey, 1);
    put32(buf, version, creds->ticket_flags);
    marshal_addrs(buf, version, creds->addresses);
    marshal_authdata(buf, version, creds->authdata);
    put_data(buf, version, &creds->ticket);
    put_data(buf, version, &creds->second_ticket);
}

#define SC_CLIENT_PRINCIPAL  0x0001
#define SC_SERVER_PRINCIPAL  0x0002
#define SC_SESSION_KEY       0x0004
#define SC_TICKET            0x0008
#define SC_SECOND_TICKET     0x0010
#define SC_AUTHDATA          0x0020
#define SC_ADDRESSES         0x0040

/* Construct the header flags field for a matching credential for the Heimdal
 * KCM format. */
static uint32_t
mcred_header(krb5_creds *mcred)
{
    uint32_t header = 0;

    if (mcred->client != NULL)
        header |= SC_CLIENT_PRINCIPAL;
    if (mcred->server != NULL)
        header |= SC_SERVER_PRINCIPAL;
    if (mcred->keyblock.enctype != ENCTYPE_NULL)
        header |= SC_SESSION_KEY;
    if (mcred->ticket.length > 0)
        header |= SC_TICKET;
    if (mcred->second_ticket.length > 0)
        header |= SC_SECOND_TICKET;
    if (mcred->authdata != NULL && *mcred->authdata != NULL)
        header |= SC_AUTHDATA;
    if (mcred->addresses != NULL && *mcred->addresses != NULL)
        header |= SC_ADDRESSES;
    return header;
}

/*
 * Marshal a matching credential in the Heimdal KCM format.  Matching
 * credentials are used to identify an existing credential to retrieve or
 * remove from a cache.
 */
void
k5_marshal_mcred(struct k5buf *buf, krb5_creds *mcred)
{
    const int version = 4;      /* subfields use v4 file format */
    uint32_t header;
    char is_skey;

    header = mcred_header(mcred);
    put32(buf, version, header);
    if (mcred->client != NULL)
        k5_marshal_princ(buf, version, mcred->client);
    if (mcred->server != NULL)
        k5_marshal_princ(buf, version, mcred->server);
    if (mcred->keyblock.enctype != ENCTYPE_NULL)
        marshal_keyblock(buf, version, &mcred->keyblock);
    put32(buf, version, mcred->times.authtime);
    put32(buf, version, mcred->times.starttime);
    put32(buf, version, mcred->times.endtime);
    put32(buf, version, mcred->times.renew_till);
    is_skey = mcred->is_skey;
    k5_buf_add_len(buf, &is_skey, 1);
    put32(buf, version, mcred->ticket_flags);
    if (mcred->addresses != NULL && *mcred->addresses != NULL)
        marshal_addrs(buf, version, mcred->addresses);
    if (mcred->authdata != NULL && *mcred->authdata != NULL)
        marshal_authdata(buf, version, mcred->authdata);
    if (mcred->ticket.length > 0)
        put_data(buf, version, &mcred->ticket);
    if (mcred->second_ticket.length > 0)
        put_data(buf, version, &mcred->second_ticket);
}
