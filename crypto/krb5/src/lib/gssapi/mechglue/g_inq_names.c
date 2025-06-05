/* #pragma ident	"@(#)g_inquire_names.c	1.16	04/02/23 SMI" */

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

#define	MAX_MECH_OID_PAIRS 32

/* Last argument new for V2 */
OM_uint32 KRB5_CALLCONV
gss_inquire_names_for_mech(minor_status, mechanism, name_types)

OM_uint32 *	minor_status;
gss_OID 	mechanism;
gss_OID_set *	name_types;

{
    OM_uint32		status;
    gss_OID		selected_mech = GSS_C_NO_OID, public_mech;
    gss_mechanism	mech;

    /* Initialize outputs. */

    if (minor_status != NULL)
	*minor_status = 0;

    if (name_types != NULL)
	*name_types = GSS_C_NO_OID_SET;

    /* Validate arguments. */

    if (minor_status == NULL)
	return (GSS_S_CALL_INACCESSIBLE_WRITE);

    if (name_types == NULL)
	return (GSS_S_CALL_INACCESSIBLE_WRITE);

    /*
     * select the approprate underlying mechanism routine and
     * call it.
     */

    status = gssint_select_mech_type(minor_status, mechanism,
				     &selected_mech);
    if (status != GSS_S_COMPLETE)
	return (status);

    mech = gssint_get_mechanism(selected_mech);
    if (mech == NULL)
	return GSS_S_BAD_MECH;
    else if (mech->gss_inquire_names_for_mech == NULL)
	return GSS_S_UNAVAILABLE;
    public_mech = gssint_get_public_oid(selected_mech);
    status = mech->gss_inquire_names_for_mech(minor_status, public_mech,
					      name_types);
    if (status != GSS_S_COMPLETE)
	map_error(minor_status, mech);

    return status;
}

static OM_uint32
val_inq_mechs4name_args(
    OM_uint32 *minor_status,
    const gss_name_t input_name,
    gss_OID_set *mech_set)
{

    /* Initialize outputs. */
    if (minor_status != NULL)
	*minor_status = 0;

    if (mech_set != NULL)
	*mech_set = GSS_C_NO_OID_SET;

    /* Validate arguments.e
 */
    if (minor_status == NULL)
	return (GSS_S_CALL_INACCESSIBLE_WRITE);

    if (input_name == GSS_C_NO_NAME)
	return (GSS_S_BAD_NAME);

    return (GSS_S_COMPLETE);
}

static int
mech_supports_nametype(gss_OID mech_oid, gss_OID name_type)
{
    OM_uint32		status, minor;
    gss_OID_set		types = GSS_C_NO_OID_SET;
    int 		present;

    status = gss_inquire_names_for_mech(&minor, mech_oid, &types);
    if (status != GSS_S_COMPLETE)
	return (0);
    status = gss_test_oid_set_member(&minor, name_type, types, &present);
    (void) gss_release_oid_set(&minor, &types);
    return (status == GSS_S_COMPLETE && present);
}

OM_uint32 KRB5_CALLCONV
gss_inquire_mechs_for_name(OM_uint32 *minor_status,
			   const gss_name_t input_name, gss_OID_set *mech_set)
{
    OM_uint32		status, tmpmin;
    gss_OID_set		all_mechs = GSS_C_NO_OID_SET;
    gss_OID_set		mechs = GSS_C_NO_OID_SET;
    gss_OID 		mech_oid, name_type;
    gss_buffer_desc	name_buffer = GSS_C_EMPTY_BUFFER;
    size_t		i;

    status = val_inq_mechs4name_args(minor_status, input_name, mech_set);
    if (status != GSS_S_COMPLETE)
	return (status);

    status = gss_display_name(minor_status, input_name, &name_buffer,
			      &name_type);
    if (status != GSS_S_COMPLETE)
	goto cleanup;
    status = gss_indicate_mechs(minor_status, &all_mechs);
    if (status != GSS_S_COMPLETE)
	goto cleanup;
    status = gss_create_empty_oid_set(minor_status, &mechs);
    if (status != GSS_S_COMPLETE)
	goto cleanup;
    for (i = 0; i < all_mechs->count; i++) {
	mech_oid = &all_mechs->elements[i];
	if (mech_supports_nametype(mech_oid, name_type)) {
	    status = gss_add_oid_set_member(minor_status, mech_oid, &mechs);
	    if (status != GSS_S_COMPLETE)
		goto cleanup;
	}
    }

    *mech_set = mechs;
    mechs = GSS_C_NO_OID_SET;

cleanup:
    (void) gss_release_buffer(&tmpmin, &name_buffer);
    (void) gss_release_oid_set(&tmpmin, &all_mechs);
    (void) gss_release_oid_set(&tmpmin, &mechs);
    return (status);
}
