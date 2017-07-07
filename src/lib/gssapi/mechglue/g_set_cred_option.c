/*
 * Copyright 2008-2010 by the Massachusetts Institute of Technology.
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

/* Glue routine for gssspi_set_cred_option */

#include "mglueP.h"
#include <stdio.h>
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#include <string.h>
#include <time.h>

static OM_uint32
alloc_union_cred(OM_uint32 *minor_status,
		 gss_mechanism mech,
		 gss_cred_id_t mech_cred,
		 gss_union_cred_t *pcred)
{
    OM_uint32		status;
    OM_uint32		temp_minor_status;
    gss_union_cred_t	cred = NULL;

    *pcred = NULL;

    status = GSS_S_FAILURE;

    cred = calloc(1, sizeof(*cred));
    if (cred == NULL) {
	*minor_status = ENOMEM;
	goto cleanup;
    }

    cred->loopback = cred;
    cred->count = 1;

    cred->cred_array = calloc(cred->count, sizeof(gss_cred_id_t));
    if (cred->cred_array == NULL) {
	*minor_status = ENOMEM;
	goto cleanup;
    }
    cred->cred_array[0] = mech_cred;

    status = generic_gss_copy_oid(minor_status,
                                  &mech->mech_type,
                                  &cred->mechs_array);
    if (status != GSS_S_COMPLETE)
        goto cleanup;

    status = GSS_S_COMPLETE;
    *pcred = cred;

cleanup:
    if (status != GSS_S_COMPLETE)
	gss_release_cred(&temp_minor_status, (gss_cred_id_t *)&cred);

    return status;
}

/*
 * This differs from gssspi_set_cred_option() as shipped in 1.7, in that
 * it can return a cred handle. To denote this change we have changed the
 * name of the function from gssspi_set_cred_option() to gss_set_cred_option().
 * However, the dlsym() entry point is still gssspi_set_cred_option(). This
 * fixes a separate issue, namely that a dynamically loaded mechanism could
 * not itself call set_cred_option() without calling its own implementation
 * instead of the mechanism glue's. (This is useful where a mechanism wishes
 * to export a mechanism-specific API that is a wrapper around this function.)
 */
OM_uint32 KRB5_CALLCONV
gss_set_cred_option(OM_uint32 *minor_status,
	            gss_cred_id_t *cred_handle,
	            const gss_OID desired_object,
	            const gss_buffer_t value)
{
    gss_union_cred_t	union_cred;
    gss_mechanism	mech;
    int			i;
    OM_uint32		status;
    OM_uint32		mech_status;
    OM_uint32		mech_minor_status;

    if (minor_status == NULL)
	return GSS_S_CALL_INACCESSIBLE_WRITE;

    if (cred_handle == NULL)
	return GSS_S_CALL_INACCESSIBLE_WRITE;

    *minor_status = 0;

    status = GSS_S_UNAVAILABLE;

    if (*cred_handle == GSS_C_NO_CREDENTIAL) {
	gss_cred_id_t mech_cred = GSS_C_NO_CREDENTIAL;

	/*
	 * We need to give a mechanism the opportunity to allocate a
	 * credentials handle. Unfortunately this does mean that only
	 * the default mechanism can allocate a credentials handle.
	 */
        mech = gssint_get_mechanism(NULL);
        if (mech == NULL)
            return GSS_S_BAD_MECH;

	if (mech->gssspi_set_cred_option == NULL)
	    return GSS_S_UNAVAILABLE;

	status = mech->gssspi_set_cred_option(minor_status,
					      &mech_cred,
					      desired_object,
					      value);
	if (status != GSS_S_COMPLETE) {
	    map_error(minor_status, mech);
	    return status;
	}

	if (mech_cred != GSS_C_NO_CREDENTIAL) {
	    status = alloc_union_cred(minor_status,
				      mech,
				      mech_cred,
				      &union_cred);
	    if (status != GSS_S_COMPLETE)
		return status;
	    *cred_handle = (gss_cred_id_t)union_cred;
	}
    } else {
	union_cred = (gss_union_cred_t)*cred_handle;

	for (i = 0; i < union_cred->count; i++) {
	    mech = gssint_get_mechanism(&union_cred->mechs_array[i]);
	    if (mech == NULL) {
		status = GSS_S_BAD_MECH;
		break;
	    }

	    if (mech->gssspi_set_cred_option == NULL)
		continue;

	    mech_status = mech->gssspi_set_cred_option(&mech_minor_status,
						       &union_cred->cred_array[i],
						       desired_object,
						       value);
	    if (mech_status == GSS_S_UNAVAILABLE)
		continue;
	    else {
		status = mech_status;
		*minor_status = mech_minor_status;
	    }
	    if (status != GSS_S_COMPLETE) {
		map_error(minor_status, mech);
		break;
	    }
	}
    }

    return status;
}

/*
 * Provide this for backward ABI compatibility, but remove it from the
 * header.
 */
OM_uint32 KRB5_CALLCONV
gssspi_set_cred_option(OM_uint32 *minor_status,
	               gss_cred_id_t cred,
	               const gss_OID desired_object,
	               const gss_buffer_t value);

OM_uint32 KRB5_CALLCONV
gssspi_set_cred_option(OM_uint32 *minor_status,
	               gss_cred_id_t cred,
	               const gss_OID desired_object,
	               const gss_buffer_t value)
{
    return gss_set_cred_option(minor_status, &cred,
                               desired_object, value);
}
