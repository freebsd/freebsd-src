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

/* Glue routine for gss_inquire_cred_by_oid */

#include "mglueP.h"
#include <stdio.h>
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#include <string.h>
#include <time.h>

static OM_uint32 append_to_buffer_set(OM_uint32 *minor_status,
				      gss_buffer_set_t *dst,
				      const gss_buffer_set_t src)
{
    size_t i;
    OM_uint32 status;

    if (src == GSS_C_NO_BUFFER_SET)
	return GSS_S_COMPLETE;

    if (*dst == GSS_C_NO_BUFFER_SET) {
	status = gss_create_empty_buffer_set(minor_status, dst);
	if (status != GSS_S_COMPLETE)
	    return status;
    }

    status = GSS_S_COMPLETE;

    for (i = 0; i < src->count; i++) {
	status = gss_add_buffer_set_member(minor_status,
					   &src->elements[i],
					   dst);
	if (status != GSS_S_COMPLETE)
	    break;
    }

    return status;
}

OM_uint32 KRB5_CALLCONV
gss_inquire_cred_by_oid(OM_uint32 *minor_status,
	                const gss_cred_id_t cred_handle,
	                const gss_OID desired_object,
	                gss_buffer_set_t *data_set)
{
    gss_union_cred_t	union_cred;
    gss_mechanism	mech;
    int			i;
    gss_buffer_set_t	union_set = GSS_C_NO_BUFFER_SET;
    gss_buffer_set_t	ret_set = GSS_C_NO_BUFFER_SET;
    OM_uint32		status, minor;

    if (minor_status != NULL)
	*minor_status = 0;

    if (data_set != NULL)
	*data_set = GSS_C_NO_BUFFER_SET;

    if (minor_status == NULL || data_set == NULL)
	return GSS_S_CALL_INACCESSIBLE_WRITE;

    if (cred_handle == GSS_C_NO_CREDENTIAL)
	return GSS_S_CALL_INACCESSIBLE_READ | GSS_S_NO_CRED;

    if (desired_object == GSS_C_NO_OID)
	return GSS_S_CALL_INACCESSIBLE_READ;

    union_cred = (gss_union_cred_t) cred_handle;

    status = GSS_S_UNAVAILABLE;

    for (i = 0; i < union_cred->count; i++) {
	mech = gssint_get_mechanism(&union_cred->mechs_array[i]);
	if (mech == NULL) {
	    status = GSS_S_BAD_MECH;
	    break;
	}

	if (mech->gss_inquire_cred_by_oid == NULL) {
	    status = GSS_S_UNAVAILABLE;
	    continue;
	}

	status = (mech->gss_inquire_cred_by_oid)(minor_status,
						 union_cred->cred_array[i],
						 desired_object,
						 &ret_set);
	if (status != GSS_S_COMPLETE) {
	    map_error(minor_status, mech);
	    continue;
	}

	if (union_cred->count == 1) {
	    union_set = ret_set;
	    break;
	}

	status = append_to_buffer_set(minor_status, &union_set, ret_set);
	gss_release_buffer_set(&minor, &ret_set);
	if (status != GSS_S_COMPLETE)
	    break;
    }

    if (status != GSS_S_COMPLETE)
	gss_release_buffer_set(&minor, &union_set);

    *data_set = union_set;

    return status;
}
