/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* lib/gssapi/mechglue/g_export_cred.c - gss_export_cred definition */
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
val_exp_cred_args(OM_uint32 *minor_status, gss_cred_id_t cred_handle,
                  gss_buffer_t token)
{

    /* Initialize outputs. */
    if (minor_status != NULL)
        *minor_status = 0;
    if (token != GSS_C_NO_BUFFER) {
        token->length = 0;
        token->value = NULL;
    }

    /* Validate arguments. */
    if (minor_status == NULL)
        return GSS_S_CALL_INACCESSIBLE_WRITE;
    if (cred_handle == GSS_C_NO_CREDENTIAL)
        return GSS_S_CALL_INACCESSIBLE_READ | GSS_S_NO_CRED;
    if (token == GSS_C_NO_BUFFER)
        return GSS_S_CALL_INACCESSIBLE_WRITE;
    return GSS_S_COMPLETE;
}

OM_uint32 KRB5_CALLCONV
gss_export_cred(OM_uint32 * minor_status, gss_cred_id_t cred_handle,
                gss_buffer_t token)
{
    OM_uint32 status, tmpmin;
    gss_union_cred_t cred;
    gss_OID mech_oid;
    gss_OID public_oid;
    gss_mechanism mech;
    gss_buffer_desc mech_token;
    struct k5buf buf;
    int i;

    status = val_exp_cred_args(minor_status, cred_handle, token);
    if (status != GSS_S_COMPLETE)
        return status;

    k5_buf_init_dynamic(&buf);

    cred = (gss_union_cred_t) cred_handle;
    for (i = 0; i < cred->count; i++) {
        /* Get an export token for this mechanism. */
        mech_oid = &cred->mechs_array[i];
        public_oid = gssint_get_public_oid(mech_oid);
        mech = gssint_get_mechanism(mech_oid);
        if (public_oid == GSS_C_NO_OID || mech == NULL) {
            status = GSS_S_DEFECTIVE_CREDENTIAL;
            goto error;
        }
        if (mech->gss_export_cred == NULL) {
            status = GSS_S_UNAVAILABLE;
            goto error;
        }
        status = mech->gss_export_cred(minor_status, cred->cred_array[i],
                                       &mech_token);
        if (status != GSS_S_COMPLETE) {
            map_error(minor_status, mech);
            goto error;
        }

        /* Append the mech OID and token to buf. */
        k5_buf_add_uint32_be(&buf, public_oid->length);
        k5_buf_add_len(&buf, public_oid->elements, public_oid->length);
        k5_buf_add_uint32_be(&buf, mech_token.length);
        k5_buf_add_len(&buf, mech_token.value, mech_token.length);
        gss_release_buffer(&tmpmin, &mech_token);
    }

    return k5buf_to_gss(minor_status, &buf, token);

error:
    k5_buf_free(&buf);
    return status;
}
