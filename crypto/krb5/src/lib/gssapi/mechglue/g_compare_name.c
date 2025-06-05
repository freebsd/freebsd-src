/* #pragma ident	"@(#)g_compare_name.c	1.16	04/02/23 SMI" */

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
 *  glue routine for gss_compare_name
 *
 */

#include "mglueP.h"
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#include <string.h>

static OM_uint32
val_comp_name_args(
    OM_uint32 *minor_status,
    gss_name_t name1,
    gss_name_t name2,
    int *name_equal)
{

    /* Initialize outputs. */

    if (minor_status != NULL)
	*minor_status = 0;

    /* Validate arguments. */

    if (name1 == GSS_C_NO_NAME || name2 == GSS_C_NO_NAME)
	return (GSS_S_CALL_INACCESSIBLE_READ | GSS_S_BAD_NAME);

    if (name_equal == NULL)
	return (GSS_S_CALL_INACCESSIBLE_WRITE);

    return (GSS_S_COMPLETE);
}


OM_uint32 KRB5_CALLCONV
gss_compare_name (minor_status,
                  name1,
                  name2,
                  name_equal)

OM_uint32 *		minor_status;
gss_name_t		name1;
gss_name_t		name2;
int *			name_equal;

{
    OM_uint32		major_status, temp_minor;
    gss_union_name_t	union_name1, union_name2;
    gss_mechanism	mech = NULL;
    gss_name_t		internal_name;

    major_status = val_comp_name_args(minor_status,
				      name1, name2, name_equal);
    if (major_status != GSS_S_COMPLETE)
	return (major_status);

    union_name1 = (gss_union_name_t) name1;
    union_name2 = (gss_union_name_t) name2;
    /*
     * Try our hardest to make union_name1 be the mechanism-specific
     * name.  (Of course we can't if both names aren't
     * mechanism-specific.)
     */
    if (union_name1->mech_type == 0) {
	union_name1 = (gss_union_name_t) name2;
	union_name2 = (gss_union_name_t) name1;
    }
    /*
     * If union_name1 is mechanism specific, then fetch its mechanism
     * information.
     */
    if (union_name1->mech_type) {
	mech = gssint_get_mechanism (union_name1->mech_type);
	if (!mech)
	    return (GSS_S_BAD_MECH);
	if (!mech->gss_compare_name)
			return (GSS_S_UNAVAILABLE);
    }

    *name_equal = 0;		/* Default to *not* equal.... */

    /*
     * First case... both names are mechanism-specific
     */
    if (union_name1->mech_type && union_name2->mech_type) {
	if (!g_OID_equal(union_name1->mech_type, union_name2->mech_type))
	    return (GSS_S_COMPLETE);
	if ((union_name1->mech_name == 0) || (union_name2->mech_name == 0))
	    /* should never happen */
	    return (GSS_S_BAD_NAME);
	if (!mech)
	    return (GSS_S_BAD_MECH);
	if (!mech->gss_compare_name)
	    return (GSS_S_UNAVAILABLE);
	major_status = mech->gss_compare_name(minor_status,
					      union_name1->mech_name,
					      union_name2->mech_name,
					      name_equal);
	if (major_status != GSS_S_COMPLETE)
	    map_error(minor_status, mech);
	return major_status;
    }

    /*
     * Second case... both names are NOT mechanism specific.
     *
     * All we do here is make sure the two name_types are equal and then
     * that the external_names are equal. Note the we do not take care
     * of the case where two different external names map to the same
     * internal name. We cannot determine this, since we as yet do not
     * know what mechanism to use for calling the underlying
     * gss_import_name().
     */
    if (!union_name1->mech_type && !union_name2->mech_type) {
		/*
		 * Second case, first sub-case... one name has null
		 * name_type, the other doesn't.
		 *
		 * Not knowing a mech_type we can't import the name with
		 * null name_type so we can't compare.
		 */
		if ((union_name1->name_type == GSS_C_NULL_OID &&
		    union_name2->name_type != GSS_C_NULL_OID) ||
		    (union_name1->name_type != GSS_C_NULL_OID &&
		    union_name2->name_type == GSS_C_NULL_OID))
			return (GSS_S_COMPLETE);
		/*
		 * Second case, second sub-case... both names have
		 * name_types, but they are different.
		 */
		if ((union_name1->name_type != GSS_C_NULL_OID &&
		    union_name2->name_type != GSS_C_NULL_OID) &&
		    !g_OID_equal(union_name1->name_type,
					union_name2->name_type))
	    return (GSS_S_COMPLETE);
		/*
		 * Second case, third sub-case... both names have equal
		 * name_types (and both have no mech_types) so we just
		 * compare the external_names.
		 */
	if ((union_name1->external_name->length !=
	     union_name2->external_name->length) ||
	    (memcmp(union_name1->external_name->value,
		    union_name2->external_name->value,
		    union_name1->external_name->length) != 0))
	    return (GSS_S_COMPLETE);
	*name_equal = 1;
	return (GSS_S_COMPLETE);
    }

    /*
     * Final case... one name is mechanism specific, the other isn't.
     *
     * We attempt to convert the general name to the mechanism type of
     * the mechanism-specific name, and then do the compare.  If we
     * can't import the general name, then we return that the name is
     * _NOT_ equal.
     */
    if (union_name2->mech_type) {
	/* We make union_name1 the mechanism specific name. */
	union_name1 = (gss_union_name_t) name2;
	union_name2 = (gss_union_name_t) name1;
    }
    major_status = gssint_import_internal_name(minor_status,
					      union_name1->mech_type,
					      union_name2,
					      &internal_name);
    if (major_status != GSS_S_COMPLETE)
	return (GSS_S_COMPLETE); /* return complete, but not equal */

    if (!mech)
	return (GSS_S_BAD_MECH);
    if (!mech->gss_compare_name)
	return (GSS_S_UNAVAILABLE);
    major_status = mech->gss_compare_name(minor_status,
					  union_name1->mech_name,
					  internal_name, name_equal);
    if (major_status != GSS_S_COMPLETE)
	map_error(minor_status, mech);
    gssint_release_internal_name(&temp_minor, union_name1->mech_type,
				&internal_name);
    return (major_status);

}
