/* #pragma ident	"@(#)g_imp_sec_context.c	1.18	04/02/23 SMI" */

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
 *  glue routine gss_export_sec_context
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
val_imp_sec_ctx_args(
    OM_uint32 *minor_status,
    gss_buffer_t interprocess_token,
    gss_ctx_id_t *context_handle)
{

    /* Initialize outputs. */
    if (minor_status != NULL)
	*minor_status = 0;

    if (context_handle != NULL)
	*context_handle = GSS_C_NO_CONTEXT;

    /* Validate arguments. */

    if (minor_status == NULL)
	return (GSS_S_CALL_INACCESSIBLE_WRITE);

    if (context_handle == NULL)
	return (GSS_S_CALL_INACCESSIBLE_WRITE);

    if (interprocess_token == GSS_C_NO_BUFFER)
	return (GSS_S_CALL_INACCESSIBLE_READ | GSS_S_DEFECTIVE_TOKEN);

    if (GSS_EMPTY_BUFFER(interprocess_token))
	return (GSS_S_CALL_INACCESSIBLE_READ | GSS_S_DEFECTIVE_TOKEN);

    return (GSS_S_COMPLETE);
}


OM_uint32 KRB5_CALLCONV
gss_import_sec_context(minor_status,
                       interprocess_token,
                       context_handle)

OM_uint32 *		minor_status;
gss_buffer_t		interprocess_token;
gss_ctx_id_t *		context_handle;

{
    OM_uint32		length = 0;
    OM_uint32		status;
    char		*p;
    gss_union_ctx_id_t	ctx;
    gss_ctx_id_t	mctx;
    gss_buffer_desc	token;
    gss_OID_desc	token_mech;
    gss_OID		selected_mech = GSS_C_NO_OID;
    gss_OID		public_mech;
    gss_mechanism	mech;

    status = val_imp_sec_ctx_args(minor_status,
				  interprocess_token, context_handle);
    if (status != GSS_S_COMPLETE)
	return (status);

    /* Initial value needed below. */
    status = GSS_S_FAILURE;

    if (interprocess_token->length >= sizeof (OM_uint32)) {
	p = interprocess_token->value;
	length = (OM_uint32)*p++;
	length = (OM_uint32)(length << 8) + *p++;
	length = (OM_uint32)(length << 8) + *p++;
	length = (OM_uint32)(length << 8) + *p++;
    }

    if (length == 0 ||
	length > (interprocess_token->length - sizeof (OM_uint32))) {
	return (GSS_S_CALL_BAD_STRUCTURE | GSS_S_DEFECTIVE_TOKEN);
    }

    token_mech.length = length;
    token_mech.elements = p;

    p += length;

    token.length = interprocess_token->length - sizeof (OM_uint32) - length;
    token.value = p;

    /*
     * select the approprate underlying mechanism routine and
     * call it.
     */

    status = gssint_select_mech_type(minor_status, &token_mech,
				     &selected_mech);
    if (status != GSS_S_COMPLETE)
	return status;

    mech = gssint_get_mechanism(selected_mech);
    if (!mech)
	return GSS_S_BAD_MECH;
    if (!mech->gssspi_import_sec_context_by_mech &&
	!mech->gss_import_sec_context)
	return GSS_S_UNAVAILABLE;

    status = gssint_create_union_context(minor_status, selected_mech, &ctx);
    if (status != GSS_S_COMPLETE)
	return status;

    if (mech->gssspi_import_sec_context_by_mech) {
	public_mech = gssint_get_public_oid(selected_mech);
	status = mech->gssspi_import_sec_context_by_mech(minor_status,
							 public_mech,
							 &token, &mctx);
    } else {
	status = mech->gss_import_sec_context(minor_status, &token, &mctx);
    }
    if (status == GSS_S_COMPLETE) {
	ctx->internal_ctx_id = mctx;
	*context_handle = (gss_ctx_id_t)ctx;
	return (GSS_S_COMPLETE);
    }
    map_error(minor_status, mech);
    free(ctx->mech_type->elements);
    free(ctx->mech_type);
    free(ctx);
    return status;
}
#endif /* LEAN_CLIENT */
