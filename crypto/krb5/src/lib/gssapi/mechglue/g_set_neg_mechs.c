/* lib/gssapi/mechglue/g_set_neg_mechs.c - Glue for gss_set_neg_mechs */
/*
 * Copyright (C) 2010 by the Massachusetts Institute of Technology.
 * All rights reserved.
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
 * permission.	Furthermore if you modify this software you must label
 * your software as modified software and not distribute it in such a
 * fashion that it might be confused with the original M.I.T. software.
 * M.I.T. makes no representations about the suitability of
 * this software for any purpose.  It is provided "as is" without express
 * or implied warranty.
 */

#include "mglueP.h"

OM_uint32 KRB5_CALLCONV
gss_set_neg_mechs(OM_uint32 *minor_status,
		  gss_cred_id_t cred_handle,
		  const gss_OID_set mech_set)
{
    gss_union_cred_t	union_cred;
    gss_mechanism	mech;
    int			i, avail;
    OM_uint32		status;

    if (minor_status == NULL)
	return GSS_S_CALL_INACCESSIBLE_WRITE;
    *minor_status = 0;

    if (cred_handle == GSS_C_NO_CREDENTIAL)
	return GSS_S_CALL_INACCESSIBLE_READ | GSS_S_NO_CRED;

    union_cred = (gss_union_cred_t) cred_handle;

    avail = 0;
    status = GSS_S_COMPLETE;
    for (i = 0; i < union_cred->count; i++) {
	mech = gssint_get_mechanism(&union_cred->mechs_array[i]);
	if (mech == NULL) {
	    status = GSS_S_BAD_MECH;
	    break;
	}

	if (mech->gss_set_neg_mechs == NULL)
	    continue;

	avail = 1;
	status = (mech->gss_set_neg_mechs)(minor_status,
					   union_cred->cred_array[i],
					   mech_set);
	if (status != GSS_S_COMPLETE) {
	    map_error(minor_status, mech);
	    break;
	}
    }

    if (status == GSS_S_COMPLETE && !avail)
	return GSS_S_UNAVAILABLE;
    return status;
}
