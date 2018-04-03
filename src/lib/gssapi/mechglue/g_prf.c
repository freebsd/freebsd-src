/*
 * Copyright 2009 by the Massachusetts Institute of Technology.
 * All Rights Reserved.
 *
 * Export of this software from the United States of America may
 *   require a specific license from the United States Government.
 *   It is the responsibility of any person or organization contemplating
 *   export to obtain such a license before exporting.
 *
 * WITHIN THAT CONSTRAINT, permission to use, copy, modify, and
 * distribute this software and its documentation for any purpose and
 * without fee is hereby granted, provided that the above copyright
 * notice appear in all copies and that both that copyright notice and
 * this permission notice appear in supporting documentation, and that
 * the name of M.I.T. not be used in advertising or publicity pertaining
 * to distribution of the software without specific, written prior
 * permission.  Furthermore if you modify this software you must label
 * your software as modified software and not distribute it in such a
 * fashion that it might be confused with the original M.I.T. software.
 * M.I.T. makes no representations about the suitability of
 * this software for any purpose.  It is provided "as is" without express
 * or implied warranty.
 */

/* Glue routine for gss_pseudo_random */

#include "mglueP.h"

OM_uint32 KRB5_CALLCONV
gss_pseudo_random (OM_uint32 *minor_status,
	           gss_ctx_id_t context_handle,
	           int prf_key,
	           const gss_buffer_t prf_in,
	           ssize_t desired_output_len,
	           gss_buffer_t prf_out)
{
    OM_uint32		status;
    gss_union_ctx_id_t	ctx;
    gss_mechanism	mech;

    if (minor_status == NULL)
	return GSS_S_CALL_INACCESSIBLE_WRITE;

    if (context_handle == GSS_C_NO_CONTEXT)
	return GSS_S_CALL_INACCESSIBLE_READ | GSS_S_NO_CONTEXT;

    if (prf_in == GSS_C_NO_BUFFER)
	return GSS_S_CALL_INACCESSIBLE_READ | GSS_S_NO_CONTEXT;

    if (prf_out == GSS_C_NO_BUFFER)
	return GSS_S_CALL_INACCESSIBLE_WRITE | GSS_S_NO_CONTEXT;

    prf_out->length = 0;
    prf_out->value = NULL;

    /*
     * select the approprate underlying mechanism routine and
     * call it.
     */

    ctx = (gss_union_ctx_id_t) context_handle;
    if (ctx->internal_ctx_id == GSS_C_NO_CONTEXT)
	return GSS_S_NO_CONTEXT;
    mech = gssint_get_mechanism (ctx->mech_type);

    if (mech != NULL) {
	if (mech->gss_pseudo_random != NULL) {
	    status = mech->gss_pseudo_random(minor_status,
					     ctx->internal_ctx_id,
					     prf_key,
					     prf_in,
					     desired_output_len,
					     prf_out);
	    if (status != GSS_S_COMPLETE)
		map_error(minor_status, mech);
	} else
	    status = GSS_S_UNAVAILABLE;

	return status;
    }

    return GSS_S_BAD_MECH;
}
