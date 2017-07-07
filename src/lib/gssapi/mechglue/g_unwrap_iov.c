/* #pragma ident	"@(#)g_seal.c	1.19	98/04/21 SMI" */

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
 *  glue routine for gss_unwrap_iov
 */

#include "mglueP.h"

static OM_uint32
val_unwrap_iov_args(
    OM_uint32 *minor_status,
    gss_ctx_id_t context_handle,
    int *conf_state,
    gss_qop_t *qop_state,
    gss_iov_buffer_desc *iov,
    int iov_count)
{

    /* Initialize outputs. */

    if (minor_status != NULL)
	*minor_status = 0;

    /* Validate arguments. */

    if (minor_status == NULL)
	return (GSS_S_CALL_INACCESSIBLE_WRITE);

    if (context_handle == GSS_C_NO_CONTEXT)
	return (GSS_S_CALL_INACCESSIBLE_READ | GSS_S_NO_CONTEXT);

    if (iov == GSS_C_NO_IOV_BUFFER)
	return (GSS_S_CALL_INACCESSIBLE_READ);

    return (GSS_S_COMPLETE);
}


OM_uint32 KRB5_CALLCONV
gss_unwrap_iov (minor_status,
                context_handle,
                conf_state,
                qop_state,
                iov,
                iov_count)
OM_uint32 *		minor_status;
gss_ctx_id_t		context_handle;
int *			conf_state;
gss_qop_t		*qop_state;
gss_iov_buffer_desc  *	iov;
int			iov_count;
{
 /* EXPORT DELETE START */

    OM_uint32		status;
    gss_union_ctx_id_t	ctx;
    gss_mechanism	mech;

    status = val_unwrap_iov_args(minor_status, context_handle,
				 conf_state, qop_state, iov, iov_count);
    if (status != GSS_S_COMPLETE)
	return (status);

    /*
     * select the approprate underlying mechanism routine and
     * call it.
     */

    ctx = (gss_union_ctx_id_t) context_handle;
    mech = gssint_get_mechanism (ctx->mech_type);

    if (mech) {
	if (mech->gss_unwrap_iov) {
	    status = mech->gss_unwrap_iov(
				 	  minor_status,
					  ctx->internal_ctx_id,
					  conf_state,
					  qop_state,
					  iov,
					  iov_count);
	    if (status != GSS_S_COMPLETE)
		map_error(minor_status, mech);
	} else
	    status = GSS_S_UNAVAILABLE;

	return(status);
    }
 /* EXPORT DELETE END */

    return (GSS_S_BAD_MECH);
}

OM_uint32 KRB5_CALLCONV
gss_verify_mic_iov(OM_uint32 *minor_status, gss_ctx_id_t context_handle,
		   gss_qop_t *qop_state, gss_iov_buffer_desc *iov,
		   int iov_count)
{
    OM_uint32 status;
    gss_union_ctx_id_t ctx;
    gss_mechanism mech;

    status = val_unwrap_iov_args(minor_status, context_handle, NULL,
				 qop_state, iov, iov_count);
    if (status != GSS_S_COMPLETE)
	return status;

    /* Select the approprate underlying mechanism routine and call it. */
    ctx = (gss_union_ctx_id_t)context_handle;
    mech = gssint_get_mechanism(ctx->mech_type);
    if (mech == NULL)
	return GSS_S_BAD_MECH;
    if (mech->gss_verify_mic_iov == NULL)
	return GSS_S_UNAVAILABLE;
    status = mech->gss_verify_mic_iov(minor_status, ctx->internal_ctx_id,
				      qop_state, iov, iov_count);
    if (status != GSS_S_COMPLETE)
	map_error(minor_status, mech);
    return status;
}
