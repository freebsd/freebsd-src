/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* lib/gssapi/mechglue/g_imp_cred.c - gss_import_cred definition */
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

#include "mglueP.h"

static OM_uint32
val_imp_cred_args(OM_uint32 *minor_status, gss_buffer_t token,
                  gss_cred_id_t *cred_handle)
{
    /* Initialize outputs. */
    if (minor_status != NULL)
        *minor_status = 0;
    if (cred_handle != NULL)
        *cred_handle = GSS_C_NO_CREDENTIAL;

    /* Validate arguments. */
    if (minor_status == NULL)
        return GSS_S_CALL_INACCESSIBLE_WRITE;
    if (cred_handle == NULL)
        return GSS_S_CALL_INACCESSIBLE_WRITE;
    if (token == GSS_C_NO_BUFFER)
        return GSS_S_CALL_INACCESSIBLE_READ | GSS_S_DEFECTIVE_TOKEN;
    if (GSS_EMPTY_BUFFER(token))
        return GSS_S_DEFECTIVE_TOKEN;
    return GSS_S_COMPLETE;
}

/* Populate mech_oid and mech_token with the next entry in token, using aliased
 * memory from token.  Advance token by the amount consumed. */
static OM_uint32
get_entry(OM_uint32 *minor_status, gss_buffer_t token, gss_OID mech_oid,
          gss_buffer_t mech_token)
{
    OM_uint32 len;

    /* Get the mechanism OID. */
    if (token->length < 4)
        return GSS_S_DEFECTIVE_TOKEN;
    len = load_32_be(token->value);
    if (token->length - 4 < len)
        return GSS_S_DEFECTIVE_TOKEN;
    mech_oid->length = len;
    mech_oid->elements = (char *)token->value + 4;
    token->value = (char *)token->value + 4 + len;
    token->length -= 4 + len;

    /* Get the mechanism token. */
    if (token->length < 4)
        return GSS_S_DEFECTIVE_TOKEN;
    len = load_32_be(token->value);
    if (token->length - 4 < len)
        return GSS_S_DEFECTIVE_TOKEN;
    mech_token->length = len;
    mech_token->value = (char *)token->value + 4;
    token->value = (char *)token->value + 4 + len;
    token->length -= 4 + len;

    return GSS_S_COMPLETE;
}

OM_uint32 KRB5_CALLCONV
gss_import_cred(OM_uint32 *minor_status, gss_buffer_t token,
                gss_cred_id_t *cred_handle)
{
    OM_uint32 status, tmpmin, count;
    gss_union_cred_t cred = NULL;
    gss_mechanism mech;
    gss_buffer_desc tok, mech_token;
    gss_OID_desc mech_oid;
    gss_OID selected_mech;
    gss_cred_id_t mech_cred;
    void *elemcopy;

    status = val_imp_cred_args(minor_status, token, cred_handle);
    if (status != GSS_S_COMPLETE)
        return status;

    /* Count the entries in token. */
    for (tok = *token, count = 0; tok.length > 0; count++) {
        status = get_entry(minor_status, &tok, &mech_oid, &mech_token);
        if (status != GSS_S_COMPLETE)
            return status;
    }

    /* Allocate a union credential. */
    cred = calloc(1, sizeof(*cred));
    if (cred == NULL)
        goto oom;
    cred->loopback = cred;
    cred->count = 0;
    cred->mechs_array = calloc(count, sizeof(*cred->mechs_array));
    if (cred->mechs_array == NULL)
        goto oom;
    cred->cred_array = calloc(count, sizeof(*cred->cred_array));
    if (cred->cred_array == NULL)
        goto oom;

    tok = *token;
    while (tok.length > 0) {
        (void)get_entry(minor_status, &tok, &mech_oid, &mech_token);

        /* Import this entry's mechanism token. */
        status = gssint_select_mech_type(minor_status, &mech_oid,
                                         &selected_mech);
        if (status != GSS_S_COMPLETE)
            goto error;
        mech = gssint_get_mechanism(selected_mech);
        if (mech == NULL || (mech->gss_import_cred == NULL &&
                             mech->gssspi_import_cred_by_mech == NULL)) {
            status = GSS_S_DEFECTIVE_TOKEN;
            goto error;
        }
        if (mech->gssspi_import_cred_by_mech) {
            status = mech->gssspi_import_cred_by_mech(minor_status,
                                        gssint_get_public_oid(selected_mech),
                                        &mech_token, &mech_cred);
        } else {
            status = mech->gss_import_cred(minor_status, &mech_token,
                                           &mech_cred);
        }
        if (status != GSS_S_COMPLETE) {
            map_error(minor_status, mech);
            goto error;
        }

        /* Add the resulting mechanism cred to the union cred. */
        elemcopy = malloc(selected_mech->length);
        if (elemcopy == NULL) {
            if (mech->gss_release_cred != NULL)
                mech->gss_release_cred(&tmpmin, &mech_cred);
            goto oom;
        }
        memcpy(elemcopy, selected_mech->elements, selected_mech->length);
        cred->mechs_array[cred->count].length = selected_mech->length;
        cred->mechs_array[cred->count].elements = elemcopy;
        cred->cred_array[cred->count++] = mech_cred;
    }

    *cred_handle = cred;
    return GSS_S_COMPLETE;

oom:
    status = GSS_S_FAILURE;
    *minor_status = ENOMEM;
error:
    (void)gss_release_cred(&tmpmin, &cred);
    return status;
}
