/* #pragma ident	"@(#)g_rel_name.c	1.11	04/02/23 SMI" */

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
 *  glue routine for gss_release_name
 */

#include "mglueP.h"
#include <stdio.h>
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#include <string.h>

OM_uint32 KRB5_CALLCONV
gss_release_name (minor_status,
		  input_name)

OM_uint32 *		minor_status;
gss_name_t *		input_name;

{
    gss_union_name_t	union_name;

    if (minor_status == NULL)
	return (GSS_S_CALL_INACCESSIBLE_WRITE);
    *minor_status = 0;

    /* if input_name is NULL, return error */
    if (input_name == NULL)
	return (GSS_S_CALL_INACCESSIBLE_READ | GSS_S_BAD_NAME);

    if (*input_name == GSS_C_NO_NAME)
	return GSS_S_COMPLETE;

    /*
     * free up the space for the external_name and then
     * free the union_name descriptor
     */

    union_name = (gss_union_name_t) *input_name;
    if (GSSINT_CHK_LOOP(union_name))
	return (GSS_S_CALL_INACCESSIBLE_READ | GSS_S_BAD_NAME);
    *input_name = 0;
    *minor_status = 0;

    if (union_name->name_type != GSS_C_NO_OID)
	gss_release_oid(minor_status, &union_name->name_type);

    if (union_name->external_name != GSS_C_NO_BUFFER) {
	if (union_name->external_name->value != NULL)
	    gssalloc_free(union_name->external_name->value);
	free(union_name->external_name);
    }

    if (union_name->mech_type) {
	gssint_release_internal_name(minor_status, union_name->mech_type,
				     &union_name->mech_name);
	gss_release_oid(minor_status, &union_name->mech_type);
    }

    free(union_name);

    return(GSS_S_COMPLETE);
}
