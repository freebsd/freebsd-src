/*
 * Copyright (c) 1996,1997, by Sun Microsystems, Inc.
 * All rights reserved.
 */

/* #pragma ident	"@(#)g_export_name.c	1.11	00/07/17 SMI" */

/*
 * glue routine gss_export_name
 *
 * Will either call the mechanism defined gss_export_name, or if one is
 * not defined will call a generic_gss_export_name routine.
 */

#include <mglueP.h>
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#include <string.h>
#include <errno.h>

OM_uint32 KRB5_CALLCONV
gss_export_name(minor_status,
			input_name,
			exported_name)
OM_uint32 *		minor_status;
const gss_name_t	input_name;
gss_buffer_t		exported_name;
{
	gss_union_name_t		union_name;

	/* Initialize outputs. */

	if (minor_status != NULL)
		*minor_status = 0;

	if (exported_name != GSS_C_NO_BUFFER) {
		exported_name->value = NULL;
		exported_name->length = 0;
	}

	/* Validate arguments. */

	if (minor_status == NULL || exported_name == GSS_C_NO_BUFFER)
		return (GSS_S_CALL_INACCESSIBLE_WRITE);

	if (input_name == GSS_C_NO_NAME)
		return (GSS_S_CALL_INACCESSIBLE_READ | GSS_S_BAD_NAME);

	union_name = (gss_union_name_t)input_name;

	/* the name must be in mechanism specific format */
	if (!union_name->mech_type)
		return (GSS_S_NAME_NOT_MN);

	return gssint_export_internal_name(minor_status, union_name->mech_type,
					union_name->mech_name, exported_name);
}
