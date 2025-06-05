/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/*
 * Copyright (C) 2011 by the Massachusetts Institute of Technology.
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
 * This file contains dispatch functions for the three GSSAPI extensions
 * described in draft-zhu-negoex-04, renamed to use the gssspi_ prefix.  Since
 * the only caller of these functions is SPNEGO, argument validation is
 * omitted.
 */

#include "mglueP.h"

OM_uint32 KRB5_CALLCONV
gssspi_query_meta_data(OM_uint32 *minor_status, gss_const_OID mech_oid,
                       gss_cred_id_t cred_handle, gss_ctx_id_t *context_handle,
                       const gss_name_t targ_name, OM_uint32 req_flags,
                       gss_buffer_t meta_data)
{
    OM_uint32 status, minor;
    gss_union_ctx_id_t ctx = (gss_union_ctx_id_t)*context_handle;
    gss_union_cred_t cred = (gss_union_cred_t)cred_handle;
    gss_union_name_t union_name = (gss_union_name_t)targ_name;
    gss_mechanism mech;
    gss_OID selected_mech, public_mech;
    gss_cred_id_t internal_cred = GSS_C_NO_CREDENTIAL;
    gss_name_t internal_name = GSS_C_NO_NAME, imported_name = GSS_C_NO_NAME;
    gss_ctx_id_t new_ctx = GSS_C_NO_CONTEXT, *internal_ctx;

    *minor_status = 0;
    meta_data->length = 0;
    meta_data->value = NULL;

    status = gssint_select_mech_type(minor_status, mech_oid, &selected_mech);
    if (status != GSS_S_COMPLETE)
        return status;
    public_mech = gssint_get_public_oid(selected_mech);

    mech = gssint_get_mechanism(selected_mech);
    if (mech == NULL)
        return GSS_S_BAD_MECH;
    if (mech->gssspi_query_meta_data == NULL)
        return GSS_S_UNAVAILABLE;

    if (cred != NULL) {
        internal_cred = gssint_get_mechanism_cred(cred, selected_mech);
        if (internal_cred == GSS_C_NO_CREDENTIAL)
            return GSS_S_NO_CRED;
    }

    if (union_name != NULL) {
        if (union_name->mech_type != GSS_C_NO_OID &&
            g_OID_equal(union_name->mech_type, selected_mech)) {
            internal_name = union_name->mech_name;
        } else {
            status = gssint_import_internal_name(minor_status, selected_mech,
                                                 union_name, &imported_name);
            if (status != GSS_S_COMPLETE)
                goto cleanup;
            internal_name = imported_name;
        }
    }

    internal_ctx = (ctx != NULL) ? &ctx->internal_ctx_id : &new_ctx;
    status = mech->gssspi_query_meta_data(minor_status, public_mech,
                                          internal_cred, internal_ctx,
                                          internal_name, req_flags, meta_data);
    if (status != GSS_S_COMPLETE) {
        map_error(minor_status, mech);
        goto cleanup;
    }

    /* If the mech created a context, wrap it in a union context. */
    if (new_ctx != GSS_C_NO_CONTEXT) {
        assert(ctx == NULL);
        status = gssint_create_union_context(minor_status, selected_mech,
                                             &ctx);
        if (status != GSS_S_COMPLETE)
            goto cleanup;

        ctx->internal_ctx_id = new_ctx;
        new_ctx = GSS_C_NO_CONTEXT;
        *context_handle = (gss_ctx_id_t)ctx;
    }

cleanup:
    if (imported_name != GSS_C_NO_NAME) {
        (void)gssint_release_internal_name(&minor, selected_mech,
                                           &imported_name);
    }
    if (new_ctx != GSS_C_NO_CONTEXT) {
        (void)gssint_delete_internal_sec_context(&minor, &mech->mech_type,
                                                 &new_ctx, GSS_C_NO_BUFFER);
    }
    return status;
}

OM_uint32 KRB5_CALLCONV
gssspi_exchange_meta_data(OM_uint32 *minor_status, gss_const_OID mech_oid,
                          gss_cred_id_t cred_handle,
                          gss_ctx_id_t *context_handle,
                          const gss_name_t targ_name, OM_uint32 req_flags,
                          gss_const_buffer_t meta_data)
{
    OM_uint32 status, minor;
    gss_union_ctx_id_t ctx = (gss_union_ctx_id_t)*context_handle;
    gss_union_cred_t cred = (gss_union_cred_t)cred_handle;
    gss_union_name_t union_name = (gss_union_name_t)targ_name;
    gss_mechanism mech;
    gss_OID selected_mech, public_mech;
    gss_cred_id_t internal_cred = GSS_C_NO_CREDENTIAL;
    gss_name_t internal_name = GSS_C_NO_NAME, imported_name = GSS_C_NO_NAME;
    gss_ctx_id_t new_ctx = GSS_C_NO_CONTEXT, *internal_ctx;

    *minor_status = 0;

    status = gssint_select_mech_type(minor_status, mech_oid, &selected_mech);
    if (status != GSS_S_COMPLETE)
        return status;
    public_mech = gssint_get_public_oid(selected_mech);

    mech = gssint_get_mechanism(selected_mech);
    if (mech == NULL)
        return GSS_S_BAD_MECH;
    if (mech->gssspi_exchange_meta_data == NULL)
        return GSS_S_UNAVAILABLE;

    if (cred != NULL) {
        internal_cred = gssint_get_mechanism_cred(cred, selected_mech);
        if (internal_cred == GSS_C_NO_CREDENTIAL)
            return GSS_S_NO_CRED;
    }

    if (union_name != NULL) {
        if (union_name->mech_type != GSS_C_NO_OID &&
            g_OID_equal(union_name->mech_type, selected_mech)) {
            internal_name = union_name->mech_name;
        } else {
            status = gssint_import_internal_name(minor_status, selected_mech,
                                                 union_name, &imported_name);
            if (GSS_ERROR(status))
                return status;
            internal_name = imported_name;
        }
    }

    internal_ctx = (ctx != NULL) ? &ctx->internal_ctx_id : &new_ctx;
    status = mech->gssspi_exchange_meta_data(minor_status, public_mech,
                                             internal_cred, internal_ctx,
                                             internal_name, req_flags,
                                             meta_data);
    if (status != GSS_S_COMPLETE) {
        map_error(minor_status, mech);
        goto cleanup;
    }

    /* If the mech created a context, wrap it in a union context. */
    if (new_ctx != GSS_C_NO_CONTEXT) {
        assert(ctx == NULL);
        status = gssint_create_union_context(minor_status, selected_mech,
                                             &ctx);
        if (status != GSS_S_COMPLETE)
            goto cleanup;

        ctx->internal_ctx_id = new_ctx;
        new_ctx = GSS_C_NO_CONTEXT;
        *context_handle = (gss_ctx_id_t)ctx;
    }

cleanup:
    if (imported_name != GSS_C_NO_NAME) {
        (void)gssint_release_internal_name(&minor, selected_mech,
                                           &imported_name);
    }
    if (new_ctx != GSS_C_NO_CONTEXT) {
        (void)gssint_delete_internal_sec_context(&minor, &mech->mech_type,
                                                 &new_ctx, GSS_C_NO_BUFFER);
    }
    return status;
}

OM_uint32 KRB5_CALLCONV
gssspi_query_mechanism_info(OM_uint32 *minor_status, gss_const_OID mech_oid,
                            unsigned char auth_scheme[16])
{
    OM_uint32 status;
    gss_OID selected_mech, public_mech;
    gss_mechanism mech;

    *minor_status = 0;
    memset(auth_scheme, 0, 16);

    status = gssint_select_mech_type(minor_status, mech_oid, &selected_mech);
    if (status != GSS_S_COMPLETE)
        return status;
    public_mech = gssint_get_public_oid(selected_mech);

    mech = gssint_get_mechanism(selected_mech);
    if (mech == NULL)
        return GSS_S_BAD_MECH;
    if (mech->gssspi_query_mechanism_info == NULL)
        return GSS_S_UNAVAILABLE;

    status = mech->gssspi_query_mechanism_info(minor_status, public_mech,
                                               auth_scheme);
    if (GSS_ERROR(status))
        map_error(minor_status, mech);

    return status;
}
