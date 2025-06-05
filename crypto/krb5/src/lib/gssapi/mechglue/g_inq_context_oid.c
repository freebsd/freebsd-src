/*
 * Copyright 2008 by the Massachusetts Institute of Technology.
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

/* Glue routine for gss_inquire_sec_context_by_oid */

#include "mglueP.h"

OM_uint32 KRB5_CALLCONV
gss_inquire_sec_context_by_oid (OM_uint32 *minor_status,
	                        const gss_ctx_id_t context_handle,
	                        const gss_OID desired_object,
	                        gss_buffer_set_t *data_set)
{
    OM_uint32		status;
    gss_union_ctx_id_t	ctx;
    gss_mechanism	mech;

    if (minor_status != NULL)
	*minor_status = 0;

    if (data_set != NULL)
	*data_set = GSS_C_NO_BUFFER_SET;

    if (minor_status == NULL || data_set == NULL)
	return GSS_S_CALL_INACCESSIBLE_WRITE;

    if (context_handle == GSS_C_NO_CONTEXT)
	return GSS_S_CALL_INACCESSIBLE_READ | GSS_S_NO_CONTEXT;

    /*
     * select the approprate underlying mechanism routine and
     * call it.
     */

    ctx = (gss_union_ctx_id_t) context_handle;
    mech = gssint_get_mechanism (ctx->mech_type);

    if (mech != NULL) {
	if (mech->gss_inquire_sec_context_by_oid != NULL) {
	    status = mech->gss_inquire_sec_context_by_oid(minor_status,
							  ctx->internal_ctx_id,
							  desired_object,
							  data_set);
	    if (status != GSS_S_COMPLETE)
		map_error(minor_status, mech);
	} else
	    status = GSS_S_UNAVAILABLE;

	return status;
    }

    return GSS_S_BAD_MECH;
}
