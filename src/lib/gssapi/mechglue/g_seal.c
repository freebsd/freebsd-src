/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
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
 *  glue routine for gss_wrap
 */

#include "mglueP.h"

static OM_uint32
val_wrap_args(OM_uint32 *minor_status,
              gss_ctx_id_t context_handle,
              int conf_req_flag,
              gss_qop_t qop_req,
              gss_buffer_t input_message_buffer,
              int *conf_state,
              gss_buffer_t output_message_buffer)
{
    /* Initialize outputs. */

    if (minor_status != NULL)
        *minor_status = 0;

    if (output_message_buffer != GSS_C_NO_BUFFER) {
        output_message_buffer->length = 0;
        output_message_buffer->value = NULL;
    }

    /* Validate arguments. */

    if (minor_status == NULL)
        return (GSS_S_CALL_INACCESSIBLE_WRITE);

    if (context_handle == GSS_C_NO_CONTEXT)
        return (GSS_S_CALL_INACCESSIBLE_READ | GSS_S_NO_CONTEXT);

    if (input_message_buffer == GSS_C_NO_BUFFER)
        return (GSS_S_CALL_INACCESSIBLE_READ);

    if (output_message_buffer == GSS_C_NO_BUFFER)
        return (GSS_S_CALL_INACCESSIBLE_WRITE);

    return (GSS_S_COMPLETE);
}

OM_uint32 KRB5_CALLCONV
gss_wrap( OM_uint32 *minor_status,
          gss_ctx_id_t context_handle,
          int conf_req_flag,
          gss_qop_t qop_req,
          gss_buffer_t input_message_buffer,
          int *conf_state,
          gss_buffer_t output_message_buffer)
{

    /* EXPORT DELETE START */

    OM_uint32           status;
    gss_union_ctx_id_t  ctx;
    gss_mechanism       mech;

    status = val_wrap_args(minor_status, context_handle,
                           conf_req_flag, qop_req,
                           input_message_buffer, conf_state,
                           output_message_buffer);
    if (status != GSS_S_COMPLETE)
        return (status);

    /*
     * select the approprate underlying mechanism routine and
     * call it.
     */

    ctx = (gss_union_ctx_id_t) context_handle;
    mech = gssint_get_mechanism (ctx->mech_type);

    if (mech) {
        if (mech->gss_wrap) {
            status = mech->gss_wrap(minor_status,
                                    ctx->internal_ctx_id,
                                    conf_req_flag,
                                    qop_req,
                                    input_message_buffer,
                                    conf_state,
                                    output_message_buffer);
            if (status != GSS_S_COMPLETE)
                map_error(minor_status, mech);
        } else if (mech->gss_wrap_aead ||
                   (mech->gss_wrap_iov && mech->gss_wrap_iov_length)) {
            status = gssint_wrap_aead(mech,
                                      minor_status,
                                      ctx,
                                      conf_req_flag,
                                      (gss_qop_t)qop_req,
                                      GSS_C_NO_BUFFER,
                                      input_message_buffer,
                                      conf_state,
                                      output_message_buffer);
        } else
            status = GSS_S_UNAVAILABLE;

        return(status);
    }
    /* EXPORT DELETE END */

    return (GSS_S_BAD_MECH);
}

OM_uint32 KRB5_CALLCONV
gss_seal(OM_uint32 *minor_status,
         gss_ctx_id_t context_handle,
         int conf_req_flag,
         int qop_req,
         gss_buffer_t input_message_buffer,
         int *conf_state,
         gss_buffer_t output_message_buffer)
{

    return gss_wrap(minor_status, context_handle,
                    conf_req_flag, (gss_qop_t) qop_req,
                    input_message_buffer, conf_state,
                    output_message_buffer);
}

/*
 * It is only possible to implement gss_wrap_size_limit() on top
 * of gss_wrap_iov_length() for mechanisms that do not use any
 * padding and have fixed length headers/trailers.
 */
static OM_uint32
gssint_wrap_size_limit_iov_shim(gss_mechanism mech,
                                OM_uint32 *minor_status,
                                gss_ctx_id_t context_handle,
                                int conf_req_flag,
                                gss_qop_t qop_req,
                                OM_uint32 req_output_size,
                                OM_uint32 *max_input_size)
{
    gss_iov_buffer_desc iov[4];
    OM_uint32           status;
    OM_uint32           ohlen;

    iov[0].type = GSS_IOV_BUFFER_TYPE_HEADER;
    iov[0].buffer.value = NULL;
    iov[0].buffer.length = 0;

    iov[1].type = GSS_IOV_BUFFER_TYPE_DATA;
    iov[1].buffer.length = req_output_size;
    iov[1].buffer.value = NULL;

    iov[2].type = GSS_IOV_BUFFER_TYPE_PADDING;
    iov[2].buffer.value = NULL;
    iov[2].buffer.length = 0;

    iov[3].type = GSS_IOV_BUFFER_TYPE_TRAILER;
    iov[3].buffer.value = NULL;
    iov[3].buffer.length = 0;

    assert(mech->gss_wrap_iov_length);

    status = mech->gss_wrap_iov_length(minor_status, context_handle,
                                       conf_req_flag, qop_req,
                                       NULL, iov,
                                       sizeof(iov)/sizeof(iov[0]));
    if (status != GSS_S_COMPLETE) {
        map_error(minor_status, mech);
        return status;
    }

    ohlen = iov[0].buffer.length + iov[3].buffer.length;

    if (iov[2].buffer.length == 0 && ohlen < req_output_size)
        *max_input_size = req_output_size - ohlen;
    else
        *max_input_size = 0;

    return GSS_S_COMPLETE;
}

/*
 * New for V2
 */
OM_uint32 KRB5_CALLCONV
gss_wrap_size_limit(OM_uint32  *minor_status,
                    gss_ctx_id_t context_handle,
                    int conf_req_flag,
                    gss_qop_t qop_req, OM_uint32 req_output_size, OM_uint32 *max_input_size)
{
    gss_union_ctx_id_t  ctx;
    gss_mechanism       mech;
    OM_uint32           major_status;

    if (minor_status == NULL)
        return (GSS_S_CALL_INACCESSIBLE_WRITE);
    *minor_status = 0;

    if (context_handle == GSS_C_NO_CONTEXT)
        return (GSS_S_CALL_INACCESSIBLE_READ | GSS_S_NO_CONTEXT);

    if (max_input_size == NULL)
        return (GSS_S_CALL_INACCESSIBLE_WRITE);

    /*
     * select the approprate underlying mechanism routine and
     * call it.
     */

    ctx = (gss_union_ctx_id_t) context_handle;
    mech = gssint_get_mechanism (ctx->mech_type);

    if (!mech)
        return (GSS_S_BAD_MECH);

    if (mech->gss_wrap_size_limit)
        major_status = mech->gss_wrap_size_limit(minor_status,
                                                 ctx->internal_ctx_id,
                                                 conf_req_flag, qop_req,
                                                 req_output_size, max_input_size);
    else if (mech->gss_wrap_iov_length)
        major_status = gssint_wrap_size_limit_iov_shim(mech, minor_status,
                                                       ctx->internal_ctx_id,
                                                       conf_req_flag, qop_req,
                                                       req_output_size, max_input_size);
    else
        major_status = GSS_S_UNAVAILABLE;
    if (major_status != GSS_S_COMPLETE)
        map_error(minor_status, mech);
    return major_status;
}
