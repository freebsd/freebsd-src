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
 *  glue routine for gss_unwrap_aead
 */

#include "mglueP.h"

static OM_uint32
val_unwrap_aead_args(
    OM_uint32 *minor_status,
    gss_ctx_id_t context_handle,
    gss_buffer_t input_message_buffer,
    gss_buffer_t input_assoc_buffer,
    gss_buffer_t output_payload_buffer,
    int *conf_state,
    gss_qop_t *qop_state)
{

    /* Initialize outputs. */

    if (minor_status != NULL)
	*minor_status = 0;

    /* Validate arguments. */

    if (minor_status == NULL)
	return (GSS_S_CALL_INACCESSIBLE_WRITE);

    if (context_handle == GSS_C_NO_CONTEXT)
	return (GSS_S_CALL_INACCESSIBLE_READ | GSS_S_NO_CONTEXT);

    if (input_message_buffer == GSS_C_NO_BUFFER)
	return (GSS_S_CALL_INACCESSIBLE_READ);

    if (output_payload_buffer == GSS_C_NO_BUFFER)
	return (GSS_S_CALL_INACCESSIBLE_WRITE);

    return (GSS_S_COMPLETE);
}

static OM_uint32
gssint_unwrap_aead_iov_shim(gss_mechanism mech,
			    OM_uint32 *minor_status,
			    gss_ctx_id_t context_handle,
			    gss_buffer_t input_message_buffer,
			    gss_buffer_t input_assoc_buffer,
			    gss_buffer_t output_payload_buffer,
			    int *conf_state,
			    gss_qop_t *qop_state)
{
    OM_uint32		    status;
    gss_iov_buffer_desc	    iov[3];
    int			    i = 0;

    iov[i].type = GSS_IOV_BUFFER_TYPE_STREAM;
    iov[i].buffer = *input_message_buffer;
    i++;

    if (input_assoc_buffer != NULL) {
	iov[i].type = GSS_IOV_BUFFER_TYPE_SIGN_ONLY;
	iov[i].buffer = *input_assoc_buffer;
	i++;
    }

    iov[i].type = GSS_IOV_BUFFER_TYPE_DATA | GSS_IOV_BUFFER_FLAG_ALLOCATE;
    iov[i].buffer.value = NULL;
    iov[i].buffer.length = 0;
    i++;

    assert(mech->gss_unwrap_iov);

    status = mech->gss_unwrap_iov(minor_status, context_handle, conf_state,
				  qop_state, iov, i);
    if (status == GSS_S_COMPLETE) {
	*output_payload_buffer = iov[i - 1].buffer;
    } else {
	OM_uint32 minor;

	map_error(minor_status, mech);

	if (iov[i - 1].type & GSS_IOV_BUFFER_FLAG_ALLOCATED) {
	    gss_release_buffer(&minor, &iov[i - 1].buffer);
	    iov[i - 1].type &= ~(GSS_IOV_BUFFER_FLAG_ALLOCATED);
	}
    }

    return status;
}

OM_uint32
gssint_unwrap_aead (gss_mechanism mech,
		    OM_uint32 *minor_status,
		    gss_union_ctx_id_t ctx,
		    gss_buffer_t input_message_buffer,
		    gss_buffer_t input_assoc_buffer,
		    gss_buffer_t output_payload_buffer,
		    int *conf_state,
		    gss_qop_t *qop_state)
{
    OM_uint32		    status;

    assert(mech != NULL);
    assert(ctx != NULL);

 /* EXPORT DELETE START */

    if (mech->gss_unwrap_aead) {
	status = mech->gss_unwrap_aead(minor_status,
				       ctx->internal_ctx_id,
				       input_message_buffer,
				       input_assoc_buffer,
				       output_payload_buffer,
				       conf_state,
				       qop_state);
	if (status != GSS_S_COMPLETE)
	    map_error(minor_status, mech);
    } else if (mech->gss_unwrap_iov) {
	status = gssint_unwrap_aead_iov_shim(mech,
					     minor_status,
					     ctx->internal_ctx_id,
					     input_message_buffer,
					     input_assoc_buffer,
					     output_payload_buffer,
					     conf_state,
					     qop_state);
    } else
	status = GSS_S_UNAVAILABLE;
 /* EXPORT DELETE END */

    return (status);
}

OM_uint32 KRB5_CALLCONV
gss_unwrap_aead (minor_status,
                 context_handle,
		 input_message_buffer,
		 input_assoc_buffer,
		 output_payload_buffer,
                 conf_state,
                 qop_state)
OM_uint32 *		minor_status;
gss_ctx_id_t		context_handle;
gss_buffer_t		input_message_buffer;
gss_buffer_t		input_assoc_buffer;
gss_buffer_t		output_payload_buffer;
int 			*conf_state;
gss_qop_t		*qop_state;
{

    OM_uint32		status;
    gss_union_ctx_id_t	ctx;
    gss_mechanism	mech;

    status = val_unwrap_aead_args(minor_status, context_handle,
				  input_message_buffer, input_assoc_buffer,
				  output_payload_buffer,
				  conf_state, qop_state);
    if (status != GSS_S_COMPLETE)
	return (status);

    /*
     * select the approprate underlying mechanism routine and
     * call it.
     */
    ctx = (gss_union_ctx_id_t) context_handle;
    if (ctx->internal_ctx_id == GSS_C_NO_CONTEXT)
	return (GSS_S_NO_CONTEXT);
    mech = gssint_get_mechanism (ctx->mech_type);

    if (!mech)
	return (GSS_S_BAD_MECH);

    return gssint_unwrap_aead(mech, minor_status, ctx,
			      input_message_buffer, input_assoc_buffer,
			      output_payload_buffer, conf_state, qop_state);
}
