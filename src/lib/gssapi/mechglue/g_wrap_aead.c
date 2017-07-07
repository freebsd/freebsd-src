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
 *  glue routine for gss_wrap_aead
 */

#include "mglueP.h"

static OM_uint32
val_wrap_aead_args(
    OM_uint32 *minor_status,
    gss_ctx_id_t context_handle,
    int conf_req_flag,
    gss_qop_t qop_req,
    gss_buffer_t input_assoc_buffer,
    gss_buffer_t input_payload_buffer,
    int *conf_state,
    gss_buffer_t output_message_buffer)
{

    /* Initialize outputs. */

    if (minor_status != NULL)
	*minor_status = 0;

    /* Validate arguments. */

    if (minor_status == NULL)
	return (GSS_S_CALL_INACCESSIBLE_WRITE);

    if (context_handle == GSS_C_NO_CONTEXT)
	return (GSS_S_CALL_INACCESSIBLE_READ | GSS_S_NO_CONTEXT);

    if (input_payload_buffer == GSS_C_NO_BUFFER)
	return (GSS_S_CALL_INACCESSIBLE_READ);

    if (output_message_buffer == GSS_C_NO_BUFFER)
	return (GSS_S_CALL_INACCESSIBLE_WRITE);

    return (GSS_S_COMPLETE);
}

static OM_uint32
gssint_wrap_aead_iov_shim(gss_mechanism mech,
			  OM_uint32 *minor_status,
			  gss_ctx_id_t context_handle,
			  int conf_req_flag,
			  gss_qop_t qop_req,
			  gss_buffer_t input_assoc_buffer,
			  gss_buffer_t input_payload_buffer,
			  int *conf_state,
			  gss_buffer_t output_message_buffer)
{
    gss_iov_buffer_desc	iov[5];
    OM_uint32		status;
    size_t		offset;
    int			i = 0, iov_count;

    /* HEADER | SIGN_ONLY_DATA | DATA | PADDING | TRAILER */

    iov[i].type = GSS_IOV_BUFFER_TYPE_HEADER;
    iov[i].buffer.value = NULL;
    iov[i].buffer.length = 0;
    i++;

    if (input_assoc_buffer != GSS_C_NO_BUFFER) {
	iov[i].type = GSS_IOV_BUFFER_TYPE_SIGN_ONLY;
	iov[i].buffer = *input_assoc_buffer;
	i++;
    }

    iov[i].type = GSS_IOV_BUFFER_TYPE_DATA;
    iov[i].buffer = *input_payload_buffer;
    i++;

    iov[i].type = GSS_IOV_BUFFER_TYPE_PADDING;
    iov[i].buffer.value = NULL;
    iov[i].buffer.length = 0;
    i++;

    iov[i].type = GSS_IOV_BUFFER_TYPE_TRAILER;
    iov[i].buffer.value = NULL;
    iov[i].buffer.length = 0;
    i++;

    iov_count = i;

    assert(mech->gss_wrap_iov_length);

    status = mech->gss_wrap_iov_length(minor_status, context_handle,
				       conf_req_flag, qop_req,
				       NULL, iov, iov_count);
    if (status != GSS_S_COMPLETE) {
	map_error(minor_status, mech);
	return status;
    }

    /* Format output token (does not include associated data) */
    for (i = 0, output_message_buffer->length = 0; i < iov_count; i++) {
	if (GSS_IOV_BUFFER_TYPE(iov[i].type) == GSS_IOV_BUFFER_TYPE_SIGN_ONLY)
	    continue;

	output_message_buffer->length += iov[i].buffer.length;
    }

    output_message_buffer->value = gssalloc_malloc(output_message_buffer->length);
    if (output_message_buffer->value == NULL) {
	*minor_status = ENOMEM;
	return GSS_S_FAILURE;
    }

    i = 0, offset = 0;

    /* HEADER */
    iov[i].buffer.value = (unsigned char *)output_message_buffer->value + offset;
    offset += iov[i].buffer.length;
    i++;

    /* SIGN_ONLY_DATA */
    if (input_assoc_buffer != GSS_C_NO_BUFFER)
	i++;

    /* DATA */
    iov[i].buffer.value = (unsigned char *)output_message_buffer->value + offset;
    offset += iov[i].buffer.length;

    memcpy(iov[i].buffer.value, input_payload_buffer->value, iov[i].buffer.length);
    i++;

    /* PADDING */
    iov[i].buffer.value = (unsigned char *)output_message_buffer->value + offset;
    offset += iov[i].buffer.length;
    i++;

    /* TRAILER */
    iov[i].buffer.value = (unsigned char *)output_message_buffer->value + offset;
    offset += iov[i].buffer.length;
    i++;

    assert(offset == output_message_buffer->length);

    assert(mech->gss_wrap_iov);

    status = mech->gss_wrap_iov(minor_status, context_handle,
				conf_req_flag, qop_req,
				conf_state, iov, iov_count);
    if (status != GSS_S_COMPLETE) {
	OM_uint32 minor;

	map_error(minor_status, mech);
	gss_release_buffer(&minor, output_message_buffer);
    }

    return status;
}

OM_uint32
gssint_wrap_aead (gss_mechanism mech,
		  OM_uint32 *minor_status,
		  gss_union_ctx_id_t ctx,
		  int conf_req_flag,
		  gss_qop_t qop_req,
		  gss_buffer_t input_assoc_buffer,
		  gss_buffer_t input_payload_buffer,
		  int *conf_state,
		  gss_buffer_t output_message_buffer)
{
 /* EXPORT DELETE START */
    OM_uint32		status;

    assert(ctx != NULL);
    assert(mech != NULL);

    if (mech->gss_wrap_aead) {
	status = mech->gss_wrap_aead(minor_status,
				     ctx->internal_ctx_id,
				     conf_req_flag,
				     qop_req,
				     input_assoc_buffer,
				     input_payload_buffer,
				     conf_state,
				     output_message_buffer);
	if (status != GSS_S_COMPLETE)
	    map_error(minor_status, mech);
    } else if (mech->gss_wrap_iov && mech->gss_wrap_iov_length) {
	status = gssint_wrap_aead_iov_shim(mech,
					   minor_status,
					   ctx->internal_ctx_id,
					   conf_req_flag,
					   qop_req,
					   input_assoc_buffer,
					   input_payload_buffer,
					   conf_state,
					   output_message_buffer);
    } else
	status = GSS_S_UNAVAILABLE;

 /* EXPORT DELETE END */

    return status;
}

OM_uint32 KRB5_CALLCONV
gss_wrap_aead (minor_status,
               context_handle,
               conf_req_flag,
               qop_req,
	       input_assoc_buffer,
	       input_payload_buffer,
               conf_state,
               output_message_buffer)
OM_uint32 *		minor_status;
gss_ctx_id_t		context_handle;
int			conf_req_flag;
gss_qop_t		qop_req;
gss_buffer_t		input_assoc_buffer;
gss_buffer_t		input_payload_buffer;
int *			conf_state;
gss_buffer_t		output_message_buffer;
{
    OM_uint32		status;
    gss_mechanism	mech;
    gss_union_ctx_id_t	ctx;

    status = val_wrap_aead_args(minor_status, context_handle,
				conf_req_flag, qop_req,
				input_assoc_buffer, input_payload_buffer,
				conf_state, output_message_buffer);
    if (status != GSS_S_COMPLETE)
	return (status);

    /*
     * select the approprate underlying mechanism routine and
     * call it.
     */
    ctx = (gss_union_ctx_id_t)context_handle;
    mech = gssint_get_mechanism (ctx->mech_type);
    if (!mech)
	return (GSS_S_BAD_MECH);

    return gssint_wrap_aead(mech, minor_status, ctx,
			    conf_req_flag, qop_req,
			    input_assoc_buffer, input_payload_buffer,
			    conf_state, output_message_buffer);
}
