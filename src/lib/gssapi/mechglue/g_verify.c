/* #pragma ident	"@(#)g_verify.c	1.13	98/04/23 SMI" */

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
 *  glue routine for gss_verify_mic
 */

#include "mglueP.h"

OM_uint32 KRB5_CALLCONV
gss_verify_mic (minor_status,
		context_handle,
		message_buffer,
		token_buffer,
		qop_state)

OM_uint32 *		minor_status;
gss_ctx_id_t		context_handle;
gss_buffer_t		message_buffer;
gss_buffer_t		token_buffer;
gss_qop_t *		qop_state;

{
    OM_uint32		status;
    gss_union_ctx_id_t	ctx;
    gss_mechanism	mech;


    if (minor_status == NULL)
	return (GSS_S_CALL_INACCESSIBLE_WRITE);
    *minor_status = 0;

    if (context_handle == GSS_C_NO_CONTEXT)
	return (GSS_S_CALL_INACCESSIBLE_READ | GSS_S_NO_CONTEXT);

    if ((message_buffer == GSS_C_NO_BUFFER) ||
	GSS_EMPTY_BUFFER(token_buffer))

	return (GSS_S_CALL_INACCESSIBLE_READ);

    /*
     * select the approprate underlying mechanism routine and
     * call it.
     */

    ctx = (gss_union_ctx_id_t) context_handle;
    if (ctx->internal_ctx_id == GSS_C_NO_CONTEXT)
	return (GSS_S_NO_CONTEXT);
    mech = gssint_get_mechanism (ctx->mech_type);

    if (mech) {
	if (mech->gss_verify_mic) {
	    status = mech->gss_verify_mic(
					  minor_status,
					  ctx->internal_ctx_id,
					  message_buffer,
					  token_buffer,
					  qop_state);
	    if (status != GSS_S_COMPLETE)
		map_error(minor_status, mech);
	} else
	    status = GSS_S_UNAVAILABLE;

	return(status);
    }

    return (GSS_S_BAD_MECH);
}

OM_uint32 KRB5_CALLCONV
gss_verify (minor_status,
            context_handle,
            message_buffer,
            token_buffer,
            qop_state)

OM_uint32 *		minor_status;
gss_ctx_id_t		context_handle;
gss_buffer_t		message_buffer;
gss_buffer_t		token_buffer;
int *			qop_state;

{
	return (gss_verify_mic(minor_status, context_handle,
			       message_buffer, token_buffer,
			       (gss_qop_t *) qop_state));
}
