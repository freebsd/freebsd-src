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

/* Glue routine for gssspi_mech_invoke */

#include "mglueP.h"
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#include <string.h>
#include <errno.h>

OM_uint32 KRB5_CALLCONV
gssspi_mech_invoke (OM_uint32 *minor_status,
		    const gss_OID desired_mech,
		    const gss_OID desired_object,
		    gss_buffer_t value)
{
    OM_uint32		status;
    gss_OID		selected_mech = GSS_C_NO_OID;
    gss_mechanism	mech;

    if (minor_status == NULL)
	return GSS_S_CALL_INACCESSIBLE_WRITE;

    *minor_status = 0;

    /*
     * select the approprate underlying mechanism routine and
     * call it.
     */

    status = gssint_select_mech_type(minor_status, desired_mech,
				     &selected_mech);
    if (status != GSS_S_COMPLETE)
	return status;

    mech = gssint_get_mechanism(selected_mech);
    if (mech == NULL || mech->gssspi_mech_invoke == NULL) {
	return GSS_S_BAD_MECH;
    }

    status = mech->gssspi_mech_invoke(minor_status,
				      gssint_get_public_oid(selected_mech),
				      desired_object,
				      value);
    if (status != GSS_S_COMPLETE)
	map_error(minor_status, mech);

    return status;
}
