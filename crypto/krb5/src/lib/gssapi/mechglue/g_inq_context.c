/* #pragma ident	"@(#)g_inquire_context.c	1.15	04/02/23 SMI" */

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
 *  glue routine for gss_inquire_context
 */

#include "mglueP.h"
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif

static OM_uint32
val_inq_ctx_args(
    OM_uint32 *minor_status,
    gss_ctx_id_t context_handle,
    gss_name_t *src_name,
    gss_name_t *targ_name,
    OM_uint32 *lifetime_rec,
    gss_OID *mech_type,
    OM_uint32 *ctx_flags,
    int *locally_initiated,
    int *opened)
{

    /* Initialize outputs. */

    if (minor_status != NULL)
	*minor_status = 0;

    if (src_name != NULL)
	*src_name = GSS_C_NO_NAME;

    if (targ_name != NULL)
	*targ_name = GSS_C_NO_NAME;

    if (mech_type != NULL)
	*mech_type = GSS_C_NO_OID;

    /* Validate arguments. */

    if (minor_status == NULL)
	return (GSS_S_CALL_INACCESSIBLE_WRITE);

    if (context_handle == GSS_C_NO_CONTEXT)
	return (GSS_S_CALL_INACCESSIBLE_READ | GSS_S_NO_CONTEXT);

    return (GSS_S_COMPLETE);
}


/* Last argument new for V2 */
OM_uint32 KRB5_CALLCONV
gss_inquire_context(
	    OM_uint32 *minor_status,
	    gss_ctx_id_t context_handle,
	    gss_name_t *src_name,
	    gss_name_t *targ_name,
	    OM_uint32 *lifetime_rec,
	    gss_OID *mech_type,
	    OM_uint32 *ctx_flags,
	    int *locally_initiated,
	    int *opened)
{
    gss_union_ctx_id_t	ctx;
    gss_mechanism	mech;
    OM_uint32		status, temp_minor;
    gss_OID		actual_mech;
    gss_name_t localTargName = NULL, localSourceName = NULL;

    status = val_inq_ctx_args(minor_status,
			      context_handle,
			      src_name, targ_name,
			      lifetime_rec,
			      mech_type, ctx_flags,
			      locally_initiated, opened);
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

    if (!mech || !mech->gss_inquire_context || !mech->gss_display_name ||
	!mech->gss_release_name) {
	return (GSS_S_UNAVAILABLE);
    }

    status = mech->gss_inquire_context(
			minor_status,
			ctx->internal_ctx_id,
			(src_name ? &localSourceName : NULL),
			(targ_name ? &localTargName : NULL),
			lifetime_rec,
			&actual_mech,
			ctx_flags,
			locally_initiated,
			opened);

    if (status != GSS_S_COMPLETE) {
	map_error(minor_status, mech);
	return status;
    }

    /* need to convert names */

    if (src_name) {
	if (localSourceName) {
	    status = gssint_convert_name_to_union_name(minor_status, mech,
						      localSourceName, src_name);

	    if (status != GSS_S_COMPLETE) {
		if (localTargName)
		    mech->gss_release_name(&temp_minor, &localTargName);
		return (status);
	    }
	} else {
	    *src_name = GSS_C_NO_NAME;
	}
    }

    if (targ_name) {
        if (localTargName) {
	    status = gssint_convert_name_to_union_name(minor_status, mech,
						      localTargName, targ_name);

	    if (status != GSS_S_COMPLETE) {
		if (src_name)
		    (void) gss_release_name(&temp_minor, src_name);

		return (status);
	    }
        }
        else {
            *targ_name = GSS_C_NO_NAME;
        }
    }

    if (mech_type)
	*mech_type = gssint_get_public_oid(actual_mech);

    return(GSS_S_COMPLETE);
}
