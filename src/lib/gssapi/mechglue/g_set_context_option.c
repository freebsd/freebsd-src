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

/* Glue routine for gss_set_sec_context_option */

#include "mglueP.h"
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#include <string.h>
#include <errno.h>

OM_uint32 KRB5_CALLCONV
gss_set_sec_context_option (OM_uint32 *minor_status,
			    gss_ctx_id_t *context_handle,
			    const gss_OID desired_object,
			    const gss_buffer_t value)
{
    OM_uint32		status, minor;
    gss_union_ctx_id_t	ctx;
    gss_mechanism	mech;
    gss_ctx_id_t	internal_ctx = GSS_C_NO_CONTEXT;

    if (minor_status == NULL)
	return GSS_S_CALL_INACCESSIBLE_WRITE;

    if (context_handle == NULL)
	return GSS_S_CALL_INACCESSIBLE_WRITE;

    *minor_status = 0;

    /*
     * select the approprate underlying mechanism routine and
     * call it.
     */

    ctx = (gss_union_ctx_id_t) *context_handle;
    if (ctx == NULL) {
	mech = gssint_get_mechanism (GSS_C_NO_OID);
    } else {
	mech = gssint_get_mechanism (ctx->mech_type);
    }

    if (mech == NULL)
	return GSS_S_BAD_MECH;
    if (mech->gss_set_sec_context_option == NULL)
	return GSS_S_UNAVAILABLE;

    status = mech->gss_set_sec_context_option(minor_status,
					      ctx ? &ctx->internal_ctx_id :
					      &internal_ctx,
					      desired_object,
					      value);
    if (status == GSS_S_COMPLETE) {
	if (ctx == NULL && internal_ctx != GSS_C_NO_CONTEXT) {
	    /* Allocate a union context handle to wrap new context */
	    ctx = (gss_union_ctx_id_t)malloc(sizeof(*ctx));
	    if (ctx == NULL) {
		*minor_status = ENOMEM;
		gssint_delete_internal_sec_context(&minor,
						   &mech->mech_type,
						   &internal_ctx,
						   GSS_C_NO_BUFFER);
		return GSS_S_FAILURE;
	    }

	    status = generic_gss_copy_oid(minor_status,
					  &mech->mech_type,
					  &ctx->mech_type);
	    if (status != GSS_S_COMPLETE) {
		gssint_delete_internal_sec_context(&minor,
						   ctx->mech_type,
						   &internal_ctx,
						   GSS_C_NO_BUFFER);
		free(ctx);
		return status;
	    }

	    ctx->internal_ctx_id = internal_ctx;
	    *context_handle = (gss_ctx_id_t)ctx;
	}
    } else
	map_error(minor_status, mech);

    return status;
}
