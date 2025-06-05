/*
 * g_mechname.c --- registry of mechanism-specific name types
 *
 * This file contains a registry of mechanism-specific name types.  It
 * is used to determine which name types not should be lazy evaluated,
 * but rather evaluated on the spot.
 */

#include "mglueP.h"
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif

#include <stdio.h>
#include <string.h>
#include <errno.h>

static gss_mech_spec_name name_list = NULL;

/*
 * generic searching helper function.
 */
static gss_mech_spec_name search_mech_spec(name_type)
    gss_OID name_type;
{
    gss_mech_spec_name p;

    for (p = name_list; p; p = p->next) {
	if (g_OID_equal(name_type, p->name_type))
	    return p;
    }
    return NULL;
}

/*
 * Given a name_type, if it is specific to a mechanism, return the
 * mechanism OID.  Otherwise, return NULL.
 */
gss_OID gss_find_mechanism_from_name_type(name_type)
    gss_OID name_type;
{
    gss_mech_spec_name p;

    p = search_mech_spec(name_type);
    if (!p)
	return NULL;
    return p->mech;
}

/*
 * This function adds a (name_type, mechanism) pair to the
 * mechanism-specific name type registry.  If an entry for the
 * name_type already exists, then zero out the mechanism entry.
 * Otherwise, enter the pair into the registry.
 */
OM_uint32
gss_add_mech_name_type(minor_status, name_type, mech)
    OM_uint32	*minor_status;
    gss_OID	name_type;
    gss_OID	mech;
{
    OM_uint32	major_status, tmp;
    gss_mech_spec_name p;

    p = search_mech_spec(name_type);
    if (p) {
	/*
	 * We found an entry for this name type; mark it as not being
	 * a mechanism-specific name type.
	 */
	if (p->mech) {
	    if (!g_OID_equal(mech, p->mech)) {
		generic_gss_release_oid(minor_status, &p->mech);
		p->mech = 0;
	    }
	}
	return GSS_S_COMPLETE;
    }
    p = malloc(sizeof(gss_mech_spec_name_desc));
    if (!p) {
	*minor_status = ENOMEM;
	map_errcode(minor_status);
	goto allocation_failure;
    }
    p->name_type = 0;
    p->mech = 0;

    major_status = generic_gss_copy_oid(minor_status, name_type,
					&p->name_type);
    if (major_status) {
	map_errcode(minor_status);
	goto allocation_failure;
    }
    major_status = generic_gss_copy_oid(minor_status, mech,
					&p->mech);
    if (major_status) {
	map_errcode(minor_status);
	goto allocation_failure;
    }

    p->next = name_list;
    p->prev = 0;
    name_list = p;

    return GSS_S_COMPLETE;

allocation_failure:
    if (p) {
	if (p->mech)
	    generic_gss_release_oid(&tmp, &p->mech);
	if (p->name_type)
	    generic_gss_release_oid(&tmp, &p->name_type);
	free(p);
    }
    return GSS_S_FAILURE;
}
