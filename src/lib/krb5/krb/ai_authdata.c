/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* src/lib/krb5/krb/ai_authdata.c - auth-indicator authdata module */
/*
 * Copyright (C) 2016 by Red Hat, Inc.
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
#include "authdata.h"
#include "auth_con.h"
#include "int-proto.h"

struct authind_context {
    krb5_data **indicators;
};

static krb5_error_code
authind_init(krb5_context kcontext, void **plugin_context)
{
    *plugin_context = NULL;
    return 0;
}

static void
authind_flags(krb5_context kcontext, void *plugin_context,
              krb5_authdatatype ad_type, krb5_flags *flags)
{
    *flags = AD_CAMMAC_PROTECTED;
}

static krb5_error_code
authind_request_init(krb5_context kcontext, krb5_authdata_context context,
                     void *plugin_context, void **request_context)
{
    krb5_error_code ret = 0;
    struct authind_context *aictx;

    *request_context = NULL;

    aictx = k5alloc(sizeof(*aictx), &ret);
    if (aictx == NULL)
        return ret;
    aictx->indicators = NULL;
    *request_context = aictx;
    return ret;
}

static krb5_error_code
authind_import_authdata(krb5_context kcontext, krb5_authdata_context context,
                        void *plugin_context, void *request_context,
                        krb5_authdata **authdata, krb5_boolean kdc_issued,
                        krb5_const_principal kdc_issuer)
{
    struct authind_context *aictx = request_context;
    krb5_error_code ret = 0;
    krb5_data **indps = NULL;
    int i;

    for (i = 0; authdata != NULL && authdata[i] != NULL; i++) {
        ret = k5_authind_decode(authdata[i], &indps);
        if (ret)
            goto cleanup;
    }

    if (indps != NULL && *indps != NULL) {
        aictx->indicators = indps;
        indps = NULL;
    }

cleanup:
    k5_free_data_ptr_list(indps);
    return ret;
}

static void
authind_request_fini(krb5_context kcontext, krb5_authdata_context context,
                     void *plugin_context, void *request_context)
{
    struct authind_context *aictx = request_context;

    if (aictx != NULL) {
        k5_free_data_ptr_list(aictx->indicators);
        free(aictx);
    }
}

/* This is a non-URI "local attribute" that is implementation defined. */
static krb5_data authind_attr = {
    KV5M_DATA,
    sizeof("auth-indicators") - 1,
    "auth-indicators"
};

static krb5_error_code
authind_get_attribute_types(krb5_context kcontext,
                            krb5_authdata_context context,
                            void *plugin_context, void *request_context,
                            krb5_data **out_attrs)
{
    struct authind_context *aictx = request_context;
    krb5_error_code ret;
    krb5_data *attrs;

    *out_attrs = NULL;

    if (aictx->indicators == NULL || *aictx->indicators == NULL)
        return ENOENT;

    attrs = k5calloc(2, sizeof(*attrs), &ret);
    if (attrs == NULL)
        return ENOMEM;

    ret = krb5int_copy_data_contents(kcontext, &authind_attr, &attrs[0]);
    if (ret)
        goto cleanup;

    attrs[1].data = NULL;
    attrs[1].length = 0;

    *out_attrs = attrs;
    attrs = NULL;

cleanup:
    krb5int_free_data_list(kcontext, attrs);
    return ret;
}

static krb5_error_code
authind_get_attribute(krb5_context kcontext, krb5_authdata_context context,
                      void *plugin_context, void *request_context,
                      const krb5_data *attribute, krb5_boolean *authenticated,
                      krb5_boolean *complete, krb5_data *value,
                      krb5_data *display_value, int *more)
{
    struct authind_context *aictx = request_context;
    krb5_error_code ret;
    int ind;

    if (!data_eq(*attribute, authind_attr))
        return ENOENT;

    /* *more will be -1 on the first call, or the next index on subsequent
     * calls. */
    ind = (*more < 0) ? 0 : *more;
    if (aictx->indicators == NULL || aictx->indicators[ind] == NULL)
        return ENOENT;

    ret = krb5int_copy_data_contents(kcontext, aictx->indicators[ind], value);
    if (ret)
        return ret;

    /* Set *more to the next index, or to 0 if there are no more. */
    *more = (aictx->indicators[ind + 1] == NULL) ? 0 : ind + 1;

    /* Indicators are delivered in a CAMMAC verified outside of this module,
     * so these are authenticated values. */
    *authenticated = TRUE;
    *complete = TRUE;

    return ret;
}

static krb5_error_code
authind_set_attribute(krb5_context kcontext, krb5_authdata_context context,
                      void *plugin_context, void *request_context,
                      krb5_boolean complete, const krb5_data *attribute,
                      const krb5_data *value)
{
    /* Indicators are imported from ticket authdata, not set by this module. */
    if (!data_eq(*attribute, authind_attr))
        return ENOENT;

    return EPERM;
}

static krb5_error_code
authind_size(krb5_context kcontext, krb5_authdata_context context,
             void *plugin_context, void *request_context, size_t *sizep)
{
    struct authind_context *aictx = request_context;
    int i;

    /* Add the indicator count. */
    *sizep += sizeof(int32_t);

    /* Add each indicator's length and value. */
    for (i = 0; aictx->indicators != NULL && aictx->indicators[i] != NULL; i++)
        *sizep += sizeof(int32_t) + aictx->indicators[i]->length;

    return 0;
}

static krb5_error_code
authind_externalize(krb5_context kcontext, krb5_authdata_context context,
                    void *plugin_context, void *request_context,
                    uint8_t **buffer, size_t *lenremain)
{
    struct authind_context *aictx = request_context;
    krb5_error_code ret = 0;
    uint8_t *bp = *buffer;
    size_t remain = *lenremain;
    int i, count;

    if (aictx->indicators == NULL)
        return krb5_ser_pack_int32(0, buffer, lenremain);

    /* Serialize the indicator count. */
    for (count = 0; aictx->indicators[count] != NULL; count++);
    ret = krb5_ser_pack_int32(count, &bp, &remain);
    if (ret)
        return ret;

    for (i = 0; aictx->indicators[i] != NULL; i++) {
        /* Serialize the length and indicator value. */
        ret = krb5_ser_pack_int32(aictx->indicators[i]->length, &bp, &remain);
        if (ret)
            return ret;
        ret = krb5_ser_pack_bytes((uint8_t *)aictx->indicators[i]->data,
                                  aictx->indicators[i]->length, &bp, &remain);
        if (ret)
            return ret;
    }

    *buffer = bp;
    *lenremain = remain;
    return ret;
}


static krb5_error_code
authind_internalize(krb5_context kcontext, krb5_authdata_context context,
                    void *plugin_context, void *request_context,
                    uint8_t **buffer, size_t *lenremain)
{
    struct authind_context *aictx = request_context;
    krb5_error_code ret;
    int32_t count, len, i;
    uint8_t *bp = *buffer;
    size_t remain = *lenremain;
    krb5_data **inds = NULL;

    /* Get the count. */
    ret = krb5_ser_unpack_int32(&count, &bp, &remain);
    if (ret)
        return ret;

    if (count < 0 || (size_t)count > remain)
        return ERANGE;

    if (count > 0) {
        inds = k5calloc(count + 1, sizeof(*inds), &ret);
        if (inds == NULL)
            return errno;
    }

    for (i = 0; i < count; i++) {
        /* Get the length. */
        ret = krb5_ser_unpack_int32(&len, &bp, &remain);
        if (ret)
            goto cleanup;
        if (len < 0 || (size_t)len > remain) {
            ret = ERANGE;
            goto cleanup;
        }

        /* Get the indicator. */
        inds[i] = k5alloc(sizeof(*inds[i]), &ret);
        if (inds[i] == NULL)
            goto cleanup;
        ret = alloc_data(inds[i], len);
        if (ret)
            goto cleanup;
        ret = krb5_ser_unpack_bytes((uint8_t *)inds[i]->data, len, &bp,
                                    &remain);
        if (ret)
            goto cleanup;
    }

    k5_free_data_ptr_list(aictx->indicators);
    aictx->indicators = inds;
    inds = NULL;

    *buffer = bp;
    *lenremain = remain;

cleanup:
    k5_free_data_ptr_list(inds);
    return ret;
}

static krb5_authdatatype authind_ad_types[] = {
    KRB5_AUTHDATA_AUTH_INDICATOR, 0
};

krb5plugin_authdata_client_ftable_v0 k5_authind_ad_client_ftable = {
    "authentication-indicators",
    authind_ad_types,
    authind_init,
    NULL, /* fini */
    authind_flags,
    authind_request_init,
    authind_request_fini,
    authind_get_attribute_types,
    authind_get_attribute,
    authind_set_attribute,
    NULL, /* delete_attribute_proc */
    NULL, /* export_authdata */
    authind_import_authdata,
    NULL, /* export_internal */
    NULL, /* free_internal */
    NULL, /* verify */
    authind_size,
    authind_externalize,
    authind_internalize,
    NULL /* authind_copy */
};
