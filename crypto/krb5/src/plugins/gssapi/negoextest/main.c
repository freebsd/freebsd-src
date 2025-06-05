/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* plugins/gssapi/negoextest/main.c - GSS test module for NegoEx */
/*
 * Copyright (C) 2019 by the Massachusetts Institute of Technology.
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
#include <gssapi/gssapi.h>
#include <gssapi/gssapi_ext.h>
#include <gssapi/gssapi_alloc.h>

struct test_context {
    int initiator;
    uint8_t hops;               /* hops remaining; 0 means established */
};

OM_uint32 KRB5_CALLCONV
gss_init_sec_context(OM_uint32 *minor_status,
                     gss_cred_id_t claimant_cred_handle,
                     gss_ctx_id_t *context_handle, gss_name_t target_name,
                     gss_OID mech_type, OM_uint32 req_flags,
                     OM_uint32 time_req,
                     gss_channel_bindings_t input_chan_bindings,
                     gss_buffer_t input_token, gss_OID *actual_mech,
                     gss_buffer_t output_token, OM_uint32 *ret_flags,
                     OM_uint32 *time_rec)
{
    struct test_context *ctx = (struct test_context *)*context_handle;
    OM_uint32 major;
    gss_buffer_desc tok;
    const char *envstr;
    uint8_t hops, mech_last_octet;

    envstr = getenv("GSS_INIT_BINDING");
    if (envstr != NULL) {
        assert(strlen(envstr) > 0);
        assert(input_chan_bindings != GSS_C_NO_CHANNEL_BINDINGS);
        assert(strlen(envstr) == input_chan_bindings->application_data.length);
        assert(strcmp((char *)input_chan_bindings->application_data.value,
                      envstr) == 0);
    }

    if (input_token == GSS_C_NO_BUFFER || input_token->length == 0) {
        envstr = getenv("HOPS");
        hops = (envstr != NULL) ? atoi(envstr) : 1;
        assert(hops > 0);
    } else if (input_token->length == 4 &&
               memcmp(input_token->value, "fail", 4) == 0) {
        *minor_status = 12345;
        return GSS_S_FAILURE;
    } else {
        hops = ((uint8_t *)input_token->value)[0];
    }

    mech_last_octet = ((uint8_t *)mech_type->elements)[mech_type->length - 1];
    envstr = getenv("INIT_FAIL");
    if (envstr != NULL && atoi(envstr) == mech_last_octet)
        return GSS_S_FAILURE;

    if (ctx == NULL) {
        ctx = malloc(sizeof(*ctx));
        assert(ctx != NULL);
        ctx->initiator = 1;
        ctx->hops = hops;
        *context_handle = (gss_ctx_id_t)ctx;
    } else if (ctx != NULL) {
        assert(ctx->initiator);
        ctx->hops--;
        assert(ctx->hops == hops);
    }

    if (ctx->hops > 0) {
        /* Generate a token containing the remaining hop count. */
        ctx->hops--;
        tok.value = &ctx->hops;
        tok.length = 1;
        major = gss_encapsulate_token(&tok, mech_type, output_token);
        assert(major == GSS_S_COMPLETE);
    }

    return (ctx->hops > 0) ? GSS_S_CONTINUE_NEEDED : GSS_S_COMPLETE;
}

OM_uint32 KRB5_CALLCONV
gss_accept_sec_context(OM_uint32 *minor_status, gss_ctx_id_t *context_handle,
                       gss_cred_id_t verifier_cred_handle,
                       gss_buffer_t input_token,
                       gss_channel_bindings_t input_chan_bindings,
                       gss_name_t *src_name, gss_OID *mech_type,
                       gss_buffer_t output_token, OM_uint32 *ret_flags,
                       OM_uint32 *time_rec,
                       gss_cred_id_t *delegated_cred_handle)
{
    struct test_context *ctx = (struct test_context *)*context_handle;
    uint8_t hops, mech_last_octet;
    const char *envstr;

    envstr = getenv("GSS_ACCEPT_BINDING");
    if (envstr != NULL) {
        assert(strlen(envstr) > 0);
        assert(input_chan_bindings != GSS_C_NO_CHANNEL_BINDINGS);
        assert(strlen(envstr) == input_chan_bindings->application_data.length);
        assert(strcmp((char *)input_chan_bindings->application_data.value,
                      envstr) == 0);
    }

    /*
     * The unwrapped token sits at the end and is just one byte giving the
     * remaining number of hops.  The final octet of the mech encoding should
     * be just prior to it.
     */
    assert(input_token->length >= 2);
    hops = ((uint8_t *)input_token->value)[input_token->length - 1];
    mech_last_octet = ((uint8_t *)input_token->value)[input_token->length - 2];

    envstr = getenv("ACCEPT_FAIL");
    if (envstr != NULL && atoi(envstr) == mech_last_octet) {
        output_token->value = gssalloc_strdup("fail");
        assert(output_token->value != NULL);
        output_token->length = 4;
        return GSS_S_FAILURE;
    }

    if (*context_handle == GSS_C_NO_CONTEXT) {
        ctx = malloc(sizeof(*ctx));
        assert(ctx != NULL);
        ctx->initiator = 0;
        ctx->hops = hops;
        *context_handle = (gss_ctx_id_t)ctx;
    } else {
        assert(!ctx->initiator);
        ctx->hops--;
        assert(ctx->hops == hops);
    }

    if (ctx->hops > 0) {
        /* Generate a token containing the remaining hop count. */
        ctx->hops--;
        output_token->value = gssalloc_malloc(1);
        assert(output_token->value != NULL);
        memcpy(output_token->value, &ctx->hops, 1);
        output_token->length = 1;
    }

    return (ctx->hops > 0) ? GSS_S_CONTINUE_NEEDED : GSS_S_COMPLETE;
}

OM_uint32 KRB5_CALLCONV
gss_delete_sec_context(OM_uint32 *minor_status, gss_ctx_id_t *context_handle,
                       gss_buffer_t output_token)
{
    free(*context_handle);
    *context_handle = GSS_C_NO_CONTEXT;
    return GSS_S_COMPLETE;
}

OM_uint32 KRB5_CALLCONV
gss_acquire_cred(OM_uint32 *minor_status, gss_name_t desired_name,
                 OM_uint32 time_req, gss_OID_set desired_mechs,
                 gss_cred_usage_t cred_usage,
                 gss_cred_id_t *output_cred_handle, gss_OID_set *actual_mechs,
                 OM_uint32 *time_rec)
{
    return GSS_S_COMPLETE;
}

OM_uint32 KRB5_CALLCONV
gss_acquire_cred_with_password(OM_uint32 *minor_status,
                               const gss_name_t desired_name,
                               const gss_buffer_t password, OM_uint32 time_req,
                               const gss_OID_set desired_mechs,
                               gss_cred_usage_t cred_usage,
                               gss_cred_id_t *output_cred_handle,
                               gss_OID_set *actual_mechs, OM_uint32 *time_rec)
{
    return GSS_S_COMPLETE;
}

OM_uint32 KRB5_CALLCONV
gss_release_cred(OM_uint32 *minor_status, gss_cred_id_t *cred_handle)
{
    return GSS_S_COMPLETE;
}

OM_uint32 KRB5_CALLCONV
gss_import_name(OM_uint32 *minor_status, gss_buffer_t input_name_buffer,
                gss_OID input_name_type, gss_name_t *output_name)
{
    static int dummy;

    /*
     * We don't need to remember anything about names, but we do need to
     * distinguish them from GSS_C_NO_NAME (to determine the direction of
     * gss_query_meta_data() and gss_exchange_meta_data()), so assign an
     * arbitrary data pointer.
     */
    *output_name = (gss_name_t)&dummy;
    return GSS_S_COMPLETE;
}

OM_uint32 KRB5_CALLCONV
gss_release_name(OM_uint32 *minor_status, gss_name_t *input_name)
{
    return GSS_S_COMPLETE;
}

OM_uint32 KRB5_CALLCONV
gss_display_status(OM_uint32 *minor_status, OM_uint32 status_value,
                   int status_type, gss_OID mech_type,
                   OM_uint32 *message_context, gss_buffer_t status_string)
{
    if (status_type == GSS_C_MECH_CODE && status_value == 12345) {
        status_string->value = gssalloc_strdup("failure from acceptor");
        assert(status_string->value != NULL);
        status_string->length = strlen(status_string->value);
        return GSS_S_COMPLETE;
    }
    return GSS_S_BAD_STATUS;
}

OM_uint32 KRB5_CALLCONV
gssspi_query_meta_data(OM_uint32 *minor_status, gss_const_OID mech_oid,
                       gss_cred_id_t cred_handle, gss_ctx_id_t *context_handle,
                       const gss_name_t targ_name, OM_uint32 req_flags,
                       gss_buffer_t meta_data)
{
    const char *envstr;
    uint8_t mech_last_octet;
    int initiator = (targ_name != GSS_C_NO_NAME);

    mech_last_octet = ((uint8_t *)mech_oid->elements)[mech_oid->length - 1];
    envstr = getenv(initiator ? "INIT_QUERY_FAIL" : "ACCEPT_QUERY_FAIL");
    if (envstr != NULL && atoi(envstr) == mech_last_octet)
        return GSS_S_FAILURE;
    envstr = getenv(initiator ? "INIT_QUERY_NONE" : "ACCEPT_QUERY_NONE");
    if (envstr != NULL && atoi(envstr) == mech_last_octet)
        return GSS_S_COMPLETE;

    meta_data->value = gssalloc_strdup("X");
    meta_data->length = 1;
    return GSS_S_COMPLETE;
}

OM_uint32 KRB5_CALLCONV
gssspi_exchange_meta_data(OM_uint32 *minor_status, gss_const_OID mech_oid,
                          gss_cred_id_t cred_handle,
                          gss_ctx_id_t *context_handle,
                          const gss_name_t targ_name, OM_uint32 req_flags,
                          gss_const_buffer_t meta_data)
{
    const char *envstr;
    uint8_t mech_last_octet;
    int initiator = (targ_name != GSS_C_NO_NAME);

    mech_last_octet = ((uint8_t *)mech_oid->elements)[mech_oid->length - 1];
    envstr = getenv(initiator ? "INIT_EXCHANGE_FAIL" : "ACCEPT_EXCHANGE_FAIL");
    if (envstr != NULL && atoi(envstr) == mech_last_octet)
        return GSS_S_FAILURE;

    assert(meta_data->length == 1 && memcmp(meta_data->value, "X", 1) == 0);
    return GSS_S_COMPLETE;
}

OM_uint32 KRB5_CALLCONV
gssspi_query_mechanism_info(OM_uint32 *minor_status, gss_const_OID mech_oid,
                            unsigned char auth_scheme[16])
{
    /* Copy the mech OID encoding and right-pad it with zeros. */
    memset(auth_scheme, 0, 16);
    assert(mech_oid->length <= 16);
    memcpy(auth_scheme, mech_oid->elements, mech_oid->length);
    return GSS_S_COMPLETE;
}

OM_uint32 KRB5_CALLCONV
gss_inquire_sec_context_by_oid(OM_uint32 *minor_status,
                               const gss_ctx_id_t context_handle,
                               const gss_OID desired_object,
                               gss_buffer_set_t *data_set)
{
    struct test_context *ctx = (struct test_context *)context_handle;
    OM_uint32 major;
    uint8_t keybytes[32] = { 0 };
    uint8_t typebytes[4];
    gss_buffer_desc key, type;
    const char *envstr;
    int ask_verify;

    if (gss_oid_equal(desired_object, GSS_C_INQ_NEGOEX_KEY))
        ask_verify = 0;
    else if (gss_oid_equal(desired_object, GSS_C_INQ_NEGOEX_VERIFY_KEY))
        ask_verify = 1;
    else
        return GSS_S_UNAVAILABLE;

    /*
     * By default, make a key available only if the context is established.
     * This can be overridden to "always", "init-always", "accept-always",
     * or "never".
     */
    envstr = getenv("KEY");
    if (envstr != NULL && strcmp(envstr, "never") == 0) {
        return GSS_S_UNAVAILABLE;
    } else if (ctx->hops > 0) {
        if (envstr == NULL)
            return GSS_S_UNAVAILABLE;
        else if (strcmp(envstr, "init-always") == 0 && !ctx->initiator)
            return GSS_S_UNAVAILABLE;
        else if (strcmp(envstr, "accept-always") == 0 && ctx->initiator)
            return GSS_S_UNAVAILABLE;
    }

    /* Perturb the key so that each side's verifier key is equal to the other's
     * checksum key. */
    keybytes[0] = ask_verify ^ ctx->initiator;

    /* Supply an all-zeros aes256-sha1 negoex key. */
    if (gss_oid_equal(desired_object, GSS_C_INQ_NEGOEX_KEY) ||
        gss_oid_equal(desired_object, GSS_C_INQ_NEGOEX_VERIFY_KEY)) {
        store_32_le(ENCTYPE_AES256_CTS_HMAC_SHA1_96, typebytes);
        key.value = keybytes;
        key.length = sizeof(keybytes);
        type.value = typebytes;
        type.length = sizeof(typebytes);
        major = gss_add_buffer_set_member(minor_status, &key, data_set);
        if (major != GSS_S_COMPLETE)
            return major;
        return gss_add_buffer_set_member(minor_status, &type, data_set);
    }

    return GSS_S_UNAVAILABLE;
}
