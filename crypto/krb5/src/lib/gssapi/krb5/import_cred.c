/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* lib/gssapi/krb5/import_cred.c - krb5 import_cred implementation */
/*
 * Copyright (C) 2012 by the Massachusetts Institute of Technology.
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

#include "k5-int.h"
#include "k5-json.h"
#include "gssapiP_krb5.h"

/* Return the idx element of array if it is of type tid; otherwise return
 * NULL.  The caller is responsible for checking the array length. */
static k5_json_value
check_element(k5_json_array array, size_t idx, k5_json_tid tid)
{
    k5_json_value v;

    v = k5_json_array_get(array, idx);
    return (k5_json_get_tid(v) == tid) ? v : NULL;
}

/* All of the json_to_x functions return 0 on success, -1 on failure (either
 * from running out of memory or from defective input). */

/* Convert a JSON value to a C string or to NULL. */
static int
json_to_optional_string(k5_json_value v, char **string_out)
{
    *string_out = NULL;
    if (k5_json_get_tid(v) == K5_JSON_TID_NULL)
        return 0;
    if (k5_json_get_tid(v) != K5_JSON_TID_STRING)
        return -1;
    *string_out = strdup(k5_json_string_utf8(v));
    return (*string_out == NULL) ? -1 : 0;
}

/* Convert a JSON value to a principal or to NULL. */
static int
json_to_principal(krb5_context context, k5_json_value v,
                  krb5_principal *princ_out)
{
    *princ_out = NULL;
    if (k5_json_get_tid(v) == K5_JSON_TID_NULL)
        return 0;
    if (k5_json_get_tid(v) != K5_JSON_TID_STRING)
        return -1;
    if (krb5_parse_name(context, k5_json_string_utf8(v), princ_out))
        return -1;
    return 0;
}

/* Convert a JSON value to a zero-terminated enctypes list or to NULL. */
static int
json_to_etypes(k5_json_value v, krb5_enctype **etypes_out)
{
    krb5_enctype *etypes = NULL;
    k5_json_array array;
    k5_json_number n;
    size_t len, i;

    *etypes_out = NULL;
    if (k5_json_get_tid(v) == K5_JSON_TID_NULL)
        return 0;
    if (k5_json_get_tid(v) != K5_JSON_TID_ARRAY)
        return -1;
    array = v;
    len = k5_json_array_length(array);
    etypes = calloc(len + 1, sizeof(*etypes));
    for (i = 0; i < len; i++) {
        n = check_element(array, i, K5_JSON_TID_NUMBER);
        if (n == NULL)
            goto invalid;
        etypes[i] = k5_json_number_value(n);
    }
    *etypes_out = etypes;
    return 0;

invalid:
    free(etypes);
    return -1;
}

/* Convert a JSON value to a krb5 GSS name or to NULL. */
static int
json_to_kgname(krb5_context context, k5_json_value v,
               krb5_gss_name_t *name_out)
{
    k5_json_array array;
    krb5_gss_name_t name = NULL;

    *name_out = NULL;
    if (k5_json_get_tid(v) == K5_JSON_TID_NULL)
        return 0;
    if (k5_json_get_tid(v) != K5_JSON_TID_ARRAY)
        return -1;
    array = v;
    if (k5_json_array_length(array) != 3)
        return -1;
    name = calloc(1, sizeof(*name));
    if (name == NULL)
        return -1;
    if (k5_mutex_init(&name->lock)) {
        free(name);
        return -1;
    }

    if (json_to_principal(context, k5_json_array_get(array, 0), &name->princ))
        goto invalid;
    if (json_to_optional_string(k5_json_array_get(array, 1), &name->service))
        goto invalid;
    if (json_to_optional_string(k5_json_array_get(array, 2), &name->host))
        goto invalid;

    *name_out = name;
    return 0;

invalid:
    kg_release_name(context, &name);
    return -1;
}

/* Convert a JSON value to a keytab handle or to NULL. */
static int
json_to_keytab(krb5_context context, k5_json_value v, krb5_keytab *keytab_out)
{
    *keytab_out = NULL;
    if (k5_json_get_tid(v) == K5_JSON_TID_NULL)
        return 0;
    if (k5_json_get_tid(v) != K5_JSON_TID_STRING)
        return -1;
    if (krb5_kt_resolve(context, k5_json_string_utf8(v), keytab_out))
        return -1;
    return 0;
}

/* Convert a JSON value to an rcache handle or to NULL. */
static int
json_to_rcache(krb5_context context, k5_json_value v, krb5_rcache *rcache_out)
{
    krb5_rcache rcache;

    *rcache_out = NULL;
    if (k5_json_get_tid(v) == K5_JSON_TID_NULL)
        return 0;
    if (k5_json_get_tid(v) != K5_JSON_TID_STRING)
        return -1;
    if (k5_rc_resolve(context, (char *)k5_json_string_utf8(v), &rcache))
        return -1;
    *rcache_out = rcache;
    return 0;
}

/* Convert a JSON value to a keyblock, filling in keyblock. */
static int
json_to_keyblock(k5_json_value v, krb5_keyblock *keyblock)
{
    k5_json_array array;
    k5_json_number n;
    k5_json_string s;
    size_t len;

    memset(keyblock, 0, sizeof(*keyblock));
    if (k5_json_get_tid(v) != K5_JSON_TID_ARRAY)
        return -1;
    array = v;
    if (k5_json_array_length(array) != 2)
        return -1;

    n = check_element(array, 0, K5_JSON_TID_NUMBER);
    if (n == NULL)
        return -1;
    keyblock->enctype = k5_json_number_value(n);

    s = check_element(array, 1, K5_JSON_TID_STRING);
    if (s == NULL)
        return -1;
    if (k5_json_string_unbase64(s, &keyblock->contents, &len))
        return -1;
    keyblock->length = len;
    keyblock->magic = KV5M_KEYBLOCK;
    return 0;
}

/* Convert a JSON value to a krb5 address. */
static int
json_to_address(k5_json_value v, krb5_address **addr_out)
{
    k5_json_array array;
    krb5_address *addr = NULL;
    k5_json_number n;
    k5_json_string s;
    size_t len;

    *addr_out = NULL;
    if (k5_json_get_tid(v) != K5_JSON_TID_ARRAY)
        return -1;
    array = v;
    if (k5_json_array_length(array) != 2)
        return -1;

    n = check_element(array, 0, K5_JSON_TID_NUMBER);
    if (n == NULL)
        return -1;
    s = check_element(array, 1, K5_JSON_TID_STRING);
    if (s == NULL)
        return -1;

    addr = malloc(sizeof(*addr));
    if (addr == NULL)
        return -1;
    addr->addrtype = k5_json_number_value(n);
    if (k5_json_string_unbase64(s, &addr->contents, &len)) {
        free(addr);
        return -1;
    }
    addr->length = len;
    addr->magic = KV5M_ADDRESS;
    *addr_out = addr;
    return 0;
}

/* Convert a JSON value to a null-terminated list of krb5 addresses or to
 * NULL. */
static int
json_to_addresses(krb5_context context, k5_json_value v,
                  krb5_address ***addresses_out)
{
    k5_json_array array;
    krb5_address **addrs = NULL;
    size_t len, i;

    *addresses_out = NULL;
    if (k5_json_get_tid(v) == K5_JSON_TID_NULL)
        return 0;
    if (k5_json_get_tid(v) != K5_JSON_TID_ARRAY)
        return -1;
    array = v;
    len = k5_json_array_length(array);
    addrs = calloc(len + 1, sizeof(*addrs));
    for (i = 0; i < len; i++) {
        if (json_to_address(k5_json_array_get(array, i), &addrs[i]))
            goto invalid;
    }
    addrs[i] = NULL;
    *addresses_out = addrs;
    return 0;

invalid:
    krb5_free_addresses(context, addrs);
    return -1;
}

/* Convert a JSON value to an authdata element. */
static int
json_to_authdata_element(k5_json_value v, krb5_authdata **ad_out)
{
    k5_json_array array;
    krb5_authdata *ad = NULL;
    k5_json_number n;
    k5_json_string s;
    size_t len;

    *ad_out = NULL;
    if (k5_json_get_tid(v) != K5_JSON_TID_ARRAY)
        return -1;
    array = v;
    if (k5_json_array_length(array) != 2)
        return -1;

    n = check_element(array, 0, K5_JSON_TID_NUMBER);
    if (n == NULL)
        return -1;
    s = check_element(array, 1, K5_JSON_TID_STRING);
    if (s == NULL)
        return -1;

    ad = malloc(sizeof(*ad));
    if (ad == NULL)
        return -1;
    ad->ad_type = k5_json_number_value(n);
    if (k5_json_string_unbase64(s, &ad->contents, &len)) {
        free(ad);
        return -1;
    }
    ad->length = len;
    ad->magic = KV5M_AUTHDATA;
    *ad_out = ad;
    return 0;
}

/* Convert a JSON value to a null-terminated authdata list or to NULL. */
static int
json_to_authdata(krb5_context context, k5_json_value v,
                 krb5_authdata ***authdata_out)
{
    k5_json_array array;
    krb5_authdata **authdata = NULL;
    size_t len, i;

    *authdata_out = NULL;
    if (k5_json_get_tid(v) == K5_JSON_TID_NULL)
        return 0;
    if (k5_json_get_tid(v) != K5_JSON_TID_ARRAY)
        return -1;
    array = v;
    len = k5_json_array_length(array);
    authdata = calloc(len + 1, sizeof(*authdata));
    for (i = 0; i < len; i++) {
        if (json_to_authdata_element(k5_json_array_get(array, i),
                                     &authdata[i]))
            goto invalid;
    }
    authdata[i] = NULL;
    *authdata_out = authdata;
    return 0;

invalid:
    krb5_free_authdata(context, authdata);
    return -1;
}

/* Convert a JSON value to a krb5 credential structure, filling in creds. */
static int
json_to_creds(krb5_context context, k5_json_value v, krb5_creds *creds)
{
    k5_json_array array;
    k5_json_number n;
    k5_json_bool b;
    k5_json_string s;
    unsigned char *data;
    size_t len;

    memset(creds, 0, sizeof(*creds));
    if (k5_json_get_tid(v) != K5_JSON_TID_ARRAY)
        return -1;
    array = v;
    if (k5_json_array_length(array) != 13)
        return -1;

    if (json_to_principal(context, k5_json_array_get(array, 0),
                          &creds->client))
        goto invalid;

    if (json_to_principal(context, k5_json_array_get(array, 1),
                          &creds->server))
        goto invalid;

    if (json_to_keyblock(k5_json_array_get(array, 2), &creds->keyblock))
        goto invalid;

    n = check_element(array, 3, K5_JSON_TID_NUMBER);
    if (n == NULL)
        goto invalid;
    creds->times.authtime = k5_json_number_value(n);

    n = check_element(array, 4, K5_JSON_TID_NUMBER);
    if (n == NULL)
        goto invalid;
    creds->times.starttime = k5_json_number_value(n);

    n = check_element(array, 5, K5_JSON_TID_NUMBER);
    if (n == NULL)
        goto invalid;
    creds->times.endtime = k5_json_number_value(n);

    n = check_element(array, 6, K5_JSON_TID_NUMBER);
    if (n == NULL)
        goto invalid;
    creds->times.renew_till = k5_json_number_value(n);

    b = check_element(array, 7, K5_JSON_TID_BOOL);
    if (b == NULL)
        goto invalid;
    creds->is_skey = k5_json_bool_value(b);

    n = check_element(array, 8, K5_JSON_TID_NUMBER);
    if (n == NULL)
        goto invalid;
    creds->ticket_flags = k5_json_number_value(n);

    if (json_to_addresses(context, k5_json_array_get(array, 9),
                          &creds->addresses))
        goto invalid;

    s = check_element(array, 10, K5_JSON_TID_STRING);
    if (s == NULL)
        goto invalid;
    if (k5_json_string_unbase64(s, &data, &len))
        goto invalid;
    creds->ticket.data = (char *)data;
    creds->ticket.length = len;

    s = check_element(array, 11, K5_JSON_TID_STRING);
    if (s == NULL)
        goto invalid;
    if (k5_json_string_unbase64(s, &data, &len))
        goto invalid;
    creds->second_ticket.data = (char *)data;
    creds->second_ticket.length = len;

    if (json_to_authdata(context, k5_json_array_get(array, 12),
                         &creds->authdata))
        goto invalid;

    creds->magic = KV5M_CREDS;
    return 0;

invalid:
    krb5_free_cred_contents(context, creds);
    memset(creds, 0, sizeof(*creds));
    return -1;
}

/* Convert a JSON value to a ccache handle or to NULL.  Set *new_out to true if
 * the ccache handle is a newly created memory ccache, false otherwise. */
static int
json_to_ccache(krb5_context context, k5_json_value v, krb5_ccache *ccache_out,
               krb5_boolean *new_out)
{
    krb5_error_code ret;
    krb5_ccache ccache = NULL;
    krb5_principal princ;
    krb5_creds creds;
    k5_json_array array;
    size_t i, len;

    *ccache_out = NULL;
    *new_out = FALSE;
    if (k5_json_get_tid(v) == K5_JSON_TID_NULL)
        return 0;
    if (k5_json_get_tid(v) == K5_JSON_TID_STRING) {
        /* We got a reference to an external ccache; just resolve it. */
        return krb5_cc_resolve(context, k5_json_string_utf8(v), ccache_out) ?
            -1 : 0;
    }

    /* We got the contents of a memory ccache. */
    if (k5_json_get_tid(v) != K5_JSON_TID_ARRAY)
        return -1;
    array = v;
    len = k5_json_array_length(array);
    if (len < 1)
        return -1;

    /* Initialize a new memory ccache using the principal in the first array
     * entry.*/
    if (krb5_cc_new_unique(context, "MEMORY", NULL, &ccache))
        return -1;
    if (json_to_principal(context, k5_json_array_get(array, 0), &princ))
        goto invalid;
    ret = krb5_cc_initialize(context, ccache, princ);
    krb5_free_principal(context, princ);
    if (ret)
        goto invalid;

    /* Add remaining array entries to the ccache as credentials. */
    for (i = 1; i < len; i++) {
        if (json_to_creds(context, k5_json_array_get(array, i), &creds))
            goto invalid;
        ret = krb5_cc_store_cred(context, ccache, &creds);
        krb5_free_cred_contents(context, &creds);
        if (ret)
            goto invalid;
    }

    *ccache_out = ccache;
    *new_out = TRUE;
    return 0;

invalid:
    (void)krb5_cc_destroy(context, ccache);
    return -1;
}

/* Convert a JSON array value to a krb5 GSS credential. */
static int
json_to_kgcred(krb5_context context, k5_json_array array,
               krb5_gss_cred_id_t *cred_out)
{
    krb5_gss_cred_id_t cred;
    k5_json_number n;
    k5_json_bool b;
    krb5_boolean is_new;
    OM_uint32 tmp;

    *cred_out = NULL;
    if (k5_json_array_length(array) != 14)
        return -1;

    cred = calloc(1, sizeof(*cred));
    if (cred == NULL)
        return -1;
    if (k5_mutex_init(&cred->lock)) {
        free(cred);
        return -1;
    }

    n = check_element(array, 0, K5_JSON_TID_NUMBER);
    if (n == NULL)
        goto invalid;
    cred->usage = k5_json_number_value(n);

    if (json_to_kgname(context, k5_json_array_get(array, 1), &cred->name))
        goto invalid;

    if (json_to_principal(context, k5_json_array_get(array, 2),
                          &cred->impersonator))
        goto invalid;

    b = check_element(array, 3, K5_JSON_TID_BOOL);
    if (b == NULL)
        goto invalid;
    cred->default_identity = k5_json_bool_value(b);

    b = check_element(array, 4, K5_JSON_TID_BOOL);
    if (b == NULL)
        goto invalid;
    cred->iakerb_mech = k5_json_bool_value(b);

    if (json_to_keytab(context, k5_json_array_get(array, 5), &cred->keytab))
        goto invalid;

    if (json_to_rcache(context, k5_json_array_get(array, 6), &cred->rcache))
        goto invalid;

    if (json_to_ccache(context, k5_json_array_get(array, 7), &cred->ccache,
                       &is_new))
        goto invalid;
    cred->destroy_ccache = is_new;

    if (json_to_keytab(context, k5_json_array_get(array, 8),
                       &cred->client_keytab))
        goto invalid;

    b = check_element(array, 9, K5_JSON_TID_BOOL);
    if (b == NULL)
        goto invalid;
    cred->have_tgt = k5_json_bool_value(b);

    n = check_element(array, 10, K5_JSON_TID_NUMBER);
    if (n == NULL)
        goto invalid;
    cred->expire = k5_json_number_value(n);

    n = check_element(array, 11, K5_JSON_TID_NUMBER);
    if (n == NULL)
        goto invalid;
    cred->refresh_time = k5_json_number_value(n);

    if (json_to_etypes(k5_json_array_get(array, 12), &cred->req_enctypes))
        goto invalid;

    if (json_to_optional_string(k5_json_array_get(array, 13), &cred->password))
        goto invalid;

    *cred_out = cred;
    return 0;

invalid:
    (void)krb5_gss_release_cred(&tmp, (gss_cred_id_t *)&cred);
    return -1;
}

OM_uint32 KRB5_CALLCONV
krb5_gss_import_cred(OM_uint32 *minor_status, gss_buffer_t token,
                     gss_cred_id_t *cred_handle)
{
    OM_uint32 status = GSS_S_COMPLETE;
    krb5_context context;
    krb5_error_code ret;
    krb5_gss_cred_id_t cred;
    k5_json_value v = NULL;
    k5_json_array array;
    k5_json_string str;
    char *copy = NULL;

    ret = krb5_gss_init_context(&context);
    if (ret) {
        *minor_status = ret;
        return GSS_S_FAILURE;
    }

    /* Decode token. */
    copy = k5memdup0(token->value, token->length, &ret);
    if (copy == NULL) {
        status = GSS_S_FAILURE;
        *minor_status = ret;
        goto cleanup;
    }
    if (k5_json_decode(copy, &v))
        goto invalid;

    /* Decode the CRED_EXPORT_MAGIC array wrapper. */
    if (k5_json_get_tid(v) != K5_JSON_TID_ARRAY)
        goto invalid;
    array = v;
    if (k5_json_array_length(array) != 2)
        goto invalid;
    str = check_element(array, 0, K5_JSON_TID_STRING);
    if (str == NULL ||
        strcmp(k5_json_string_utf8(str), CRED_EXPORT_MAGIC) != 0)
        goto invalid;
    if (json_to_kgcred(context, k5_json_array_get(array, 1), &cred))
        goto invalid;

    *cred_handle = (gss_cred_id_t)cred;

cleanup:
    free(copy);
    k5_json_release(v);
    krb5_free_context(context);
    return status;

invalid:
    status = GSS_S_DEFECTIVE_TOKEN;
    goto cleanup;
}
