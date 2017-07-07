/*
 * Copyright 2004 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

/* #pragma ident	"@(#)g_canon_name.c	1.15	04/02/23 SMI" */

/*
 * routine gss_canonicalize_name
 *
 * This routine is used to produce a mechanism specific
 * representation of name that has been previously
 * imported with gss_import_name.  The routine uses the mechanism
 * specific implementation of gss_import_name to implement this
 * function.
 *
 * We allow a NULL output_name, in which case we modify the
 * input_name to include the mechanism specific name.
 */

#include <mglueP.h>
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#include <string.h>
#include <errno.h>

static OM_uint32
val_canon_name_args(
	OM_uint32 *minor_status,
	const gss_name_t input_name,
	const gss_OID mech_type,
	gss_name_t *output_name)
{

	/* Initialize outputs. */

	if (minor_status != NULL)
		*minor_status = 0;

	if (output_name != NULL)
		*output_name = GSS_C_NO_NAME;

	/* Validate arguments. */

	if (minor_status == NULL)
		return (GSS_S_CALL_INACCESSIBLE_WRITE);

	if (input_name == GSS_C_NO_NAME || mech_type == GSS_C_NULL_OID)
		return (GSS_S_CALL_INACCESSIBLE_READ);

	return (GSS_S_COMPLETE);
}


OM_uint32 KRB5_CALLCONV
gss_canonicalize_name(minor_status,
				input_name,
				mech_type,
				output_name)
OM_uint32 *minor_status;
const gss_name_t input_name;
const gss_OID mech_type;
gss_name_t *output_name;
{
	gss_union_name_t in_union, out_union = NULL, dest_union = NULL;
	OM_uint32 major_status = GSS_S_FAILURE, tmpmin;
	gss_OID selected_mech;

	major_status = val_canon_name_args(minor_status,
					   input_name,
					   mech_type,
					   output_name);
	if (major_status != GSS_S_COMPLETE)
		return (major_status);

	major_status = gssint_select_mech_type(minor_status, mech_type,
					       &selected_mech);
	if (major_status != GSS_S_COMPLETE)
		return (major_status);

	/* Initial value needed below. */
	major_status = GSS_S_FAILURE;

	in_union = (gss_union_name_t)input_name;
	/*
	 * If the caller wants to reuse the name, and the name has already
	 * been converted, then there is nothing for us to do.
	 */
	if (!output_name && in_union->mech_type &&
	    g_OID_equal(in_union->mech_type, selected_mech))
		return (GSS_S_COMPLETE);

	/* ok, then we need to do something - start by creating data struct */
	if (output_name) {
		out_union =
			(gss_union_name_t)malloc(sizeof (gss_union_name_desc));
		if (!out_union)
			goto allocation_failure;

		out_union->mech_type = 0;
		out_union->mech_name = 0;
		out_union->name_type = 0;
		out_union->external_name = 0;
		out_union->loopback = out_union;

		/* Allocate the buffer for the user specified representation */
		if (gssint_create_copy_buffer(in_union->external_name,
				&out_union->external_name, 1))
			goto allocation_failure;

		if (in_union->name_type != GSS_C_NULL_OID) {
		    major_status = generic_gss_copy_oid(minor_status,
							in_union->name_type,
							&out_union->name_type);
		    if (major_status) {
			map_errcode(minor_status);
			goto allocation_failure;
		    }
		}

	}

	/*
	 * might need to delete any old mechanism names if we are
	 * reusing the buffer.
	 */
	if (!output_name) {
		if (in_union->mech_type) {
			(void) gssint_release_internal_name(minor_status,
							in_union->mech_type,
							&in_union->mech_name);
			(void) gss_release_oid(minor_status,
					    &in_union->mech_type);
			in_union->mech_type = 0;
		}
		dest_union = in_union;
	} else
		dest_union = out_union;

	/* now let's create the new mech name */
	if ((major_status = generic_gss_copy_oid(minor_status, selected_mech,
						 &dest_union->mech_type))) {
	    map_errcode(minor_status);
	    goto allocation_failure;
	}

	if ((major_status =
		gssint_import_internal_name(minor_status, selected_mech,
						in_union,
						&dest_union->mech_name)))
		goto allocation_failure;

	if (output_name)
		*output_name = (gss_name_t)dest_union;

	return (GSS_S_COMPLETE);

allocation_failure:
	if (out_union) {
	    /* Release the partly constructed out_union. */
	    gss_name_t name = (gss_name_t)out_union;
	    (void) gss_release_name(&tmpmin, &name);
	} else if (!output_name) {
	    /* Release only the mech name fields in in_union. */
	    if (in_union->mech_name) {
		(void) gssint_release_internal_name(&tmpmin,
						    dest_union->mech_type,
						    &dest_union->mech_name);
	    }
	    if (in_union->mech_type)
		(void) gss_release_oid(&tmpmin, &dest_union->mech_type);
	}

	return (major_status);
} /**********  gss_canonicalize_name ********/
