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
krb5_gss_process_context_token(minor_status, context_handle,
                               token_buffer)
    OM_uint32 *minor_status;
    gss_ctx_id_t context_handle;
    gss_buffer_t token_buffer;
{
    krb5_gss_ctx_id_rec *ctx;
    OM_uint32 majerr;

    ctx = (krb5_gss_ctx_id_t) context_handle;

    if (ctx->terminated || !ctx->established) {
        *minor_status = KG_CTX_INCOMPLETE;
        return(GSS_S_NO_CONTEXT);
    }

    /* We only support context deletion tokens for now, and RFC 4121 does not
     * define a context deletion token. */
    if (ctx->proto) {
        *minor_status = 0;
        return(GSS_S_DEFECTIVE_TOKEN);
    }

    /* "unseal" the token */

    if (GSS_ERROR(majerr = kg_unseal(minor_status, context_handle,
                                     token_buffer,
                                     GSS_C_NO_BUFFER, NULL, NULL,
                                     KG_TOK_DEL_CTX)))
        return(majerr);

    /* Mark the context as terminated, but do not delete it (as that would
     * leave the caller with a dangling context handle). */
    ctx->terminated = 1;
    return(GSS_S_COMPLETE);
}
