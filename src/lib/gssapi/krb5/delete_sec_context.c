/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/*
 * Copyright 1993 by OpenVision Technologies, Inc.
 *
 * Permission to use, copy, modify, distribute, and sell this software
 * and its documentation for any purpose is hereby granted without fee,
 * provided that the above copyright notice appears in all copies and
 * that both that copyright notice and this permission notice appear in
 * supporting documentation, and that the name of OpenVision not be used
 * in advertising or publicity pertaining to distribution of the software
 * without specific, written prior permission. OpenVision makes no
 * representations about the suitability of this software for any
 * purpose.  It is provided "as is" without express or implied warranty.
 *
 * OPENVISION DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
 * EVENT SHALL OPENVISION BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF
 * USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR
 * OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

#include "gssapiP_krb5.h"

/*
 * $Id$
 */

OM_uint32 KRB5_CALLCONV
krb5_gss_delete_sec_context(minor_status, context_handle, output_token)
    OM_uint32 *minor_status;
    gss_ctx_id_t *context_handle;
    gss_buffer_t output_token;
{
    krb5_context context;
    krb5_gss_ctx_id_rec *ctx;

    if (output_token) {
        output_token->length = 0;
        output_token->value = NULL;
    }

    /*SUPPRESS 29*/
    if (*context_handle == GSS_C_NO_CONTEXT) {
        *minor_status = 0;
        return(GSS_S_COMPLETE);
    }

    ctx = (krb5_gss_ctx_id_t) *context_handle;
    context = ctx->k5_context;

    /* free all the context state */

    if (ctx->seqstate)
        g_seqstate_free(ctx->seqstate);

    if (ctx->enc)
        krb5_k_free_key(context, ctx->enc);

    if (ctx->seq)
        krb5_k_free_key(context, ctx->seq);

    if (ctx->here)
        kg_release_name(context, &ctx->here);
    if (ctx->there)
        kg_release_name(context, &ctx->there);
    if (ctx->subkey)
        krb5_k_free_key(context, ctx->subkey);
    if (ctx->acceptor_subkey)
        krb5_k_free_key(context, ctx->acceptor_subkey);

    if (ctx->auth_context) {
        if (ctx->cred_rcache)
            (void)krb5_auth_con_setrcache(context, ctx->auth_context, NULL);

        krb5_auth_con_free(context, ctx->auth_context);
    }

    if (ctx->mech_used)
        krb5_gss_release_oid(minor_status, &ctx->mech_used);

    if (ctx->authdata)
        krb5_free_authdata(context, ctx->authdata);

    if (ctx->k5_context)
        krb5_free_context(ctx->k5_context);

    /* Zero out context */
    zap(ctx, sizeof(*ctx));
    xfree(ctx);

    /* zero the handle itself */

    *context_handle = GSS_C_NO_CONTEXT;

    *minor_status = 0;
    return(GSS_S_COMPLETE);
}
