/* #pragma ident	"@(#)g_exp_sec_context.c	1.14	04/02/23 SMI" */

/*
 * Copyright 1996 by Sun Microsystems, Inc.
 *
 * Permission to use, copy, modify, distribute, and sell this software
 * and its documentation for any purpose is hereby granted without fee,
 * provided that the above copyright notice appears in all copies and
 * that both that copyright notice and this permission notice appear in
 * supporting documentation, and that the name of Sun Microsystems not be used
 * in advertising or publicity pertaining to distribution of the software
 * without specific, written prior permission. Sun Microsystems makes no
 * representations about the suitability of this software for any
 * purpose.  It is provided "as is" without express or implied warranty.
 *
 * SUN MICROSYSTEMS DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
 * EVENT SHALL SUN MICROSYSTEMS BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF
 * USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR
 * OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

/*
 *  glue routine for gss_export_sec_context
 */
#ifndef LEAN_CLIENT

#include "mglueP.h"
#include <stdio.h>
#include <errno.h>
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#include <string.h>

static OM_uint32
val_exp_sec_ctx_args(
    OM_uint32 *minor_status,
    gss_ctx_id_t *context_handle,
    gss_buffer_t interprocess_token)
{

    /* Initialize outputs. */

    if (minor_status != NULL)
	*minor_status = 0;

    if (interprocess_token != GSS_C_NO_BUFFER) {
	interprocess_token->length = 0;
	interprocess_token->value = NULL;
    }

    /* Validate arguments. */

    if (minor_status == NULL)
	return (GSS_S_CALL_INACCESSIBLE_WRITE);

    if (context_handle == NULL || *context_handle == GSS_C_NO_CONTEXT)
	return (GSS_S_CALL_INACCESSIBLE_READ | GSS_S_NO_CONTEXT);

    if (interprocess_token == GSS_C_NO_BUFFER)
	return (GSS_S_CALL_INACCESSIBLE_WRITE);

    return (GSS_S_COMPLETE);
}


OM_uint32 KRB5_CALLCONV
gss_export_sec_context(minor_status,
                       context_handle,
                       interprocess_token)

OM_uint32 *		minor_status;
gss_ctx_id_t *		context_handle;
gss_buffer_t		interprocess_token;

{
    OM_uint32		status;
    OM_uint32 		length;
    gss_union_ctx_id_t	ctx = NULL;
    gss_mechanism	mech;
    gss_buffer_desc	token = GSS_C_EMPTY_BUFFER;
    char		*buf;

    status = val_exp_sec_ctx_args(minor_status,
				  context_handle, interprocess_token);
    if (status != GSS_S_COMPLETE)
	return (status);

    /*
     * select the approprate underlying mechanism routine and
     * call it.
     */

    ctx = (gss_union_ctx_id_t) *context_handle;
    mech = gssint_get_mechanism (ctx->mech_type);
    if (!mech)
	return GSS_S_BAD_MECH;
    if (!mech->gss_export_sec_context)
	return (GSS_S_UNAVAILABLE);

    status = mech->gss_export_sec_context(minor_status,
					  &ctx->internal_ctx_id, &token);
    if (status != GSS_S_COMPLETE) {
	map_error(minor_status, mech);
	goto cleanup;
    }

    length = token.length + 4 + ctx->mech_type->length;
    interprocess_token->length = length;
    interprocess_token->value = malloc(length);
    if (interprocess_token->value == 0) {
	*minor_status = ENOMEM;
	status = GSS_S_FAILURE;
	goto cleanup;
    }
    buf = interprocess_token->value;
    length = ctx->mech_type->length;
    buf[3] = (unsigned char) (length & 0xFF);
    length >>= 8;
    buf[2] = (unsigned char) (length & 0xFF);
    length >>= 8;
    buf[1] = (unsigned char) (length & 0xFF);
    length >>= 8;
    buf[0] = (unsigned char) (length & 0xFF);
    memcpy(buf+4, ctx->mech_type->elements, (size_t) ctx->mech_type->length);
    memcpy(buf+4+ctx->mech_type->length, token.value, token.length);

    status = GSS_S_COMPLETE;

cleanup:
    (void) gss_release_buffer(minor_status, &token);
    if (ctx != NULL && ctx->internal_ctx_id == GSS_C_NO_CONTEXT) {
	/* If the mech deleted its context, delete the union context. */
	free(ctx->mech_type->elements);
	free(ctx->mech_type);
	free(ctx);
	*context_handle = GSS_C_NO_CONTEXT;
    }
    return status;
}
#endif /*LEAN_CLIENT */
