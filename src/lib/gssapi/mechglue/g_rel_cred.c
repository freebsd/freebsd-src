/* #pragma ident	"@(#)g_rel_cred.c	1.14	04/02/23 SMI" */

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

/* Glue routine for gss_release_cred */

#include "mglueP.h"
#include <stdio.h>
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif

OM_uint32 KRB5_CALLCONV
gss_release_cred(minor_status,
                 cred_handle)

OM_uint32 *		minor_status;
gss_cred_id_t *		cred_handle;

{
    OM_uint32		status, temp_status;
    int			j;
    gss_union_cred_t	union_cred;
    gss_mechanism	mech;

    if (minor_status == NULL)
	return (GSS_S_CALL_INACCESSIBLE_WRITE);

    *minor_status = 0;

    if (cred_handle == NULL)
	return (GSS_S_NO_CRED | GSS_S_CALL_INACCESSIBLE_READ);

    /*
     * Loop through the union_cred struct, selecting the approprate
     * underlying mechanism routine and calling it. At the end,
     * release all of the storage taken by the union_cred struct.
     */

    union_cred = (gss_union_cred_t) *cred_handle;
    if (union_cred == (gss_union_cred_t)GSS_C_NO_CREDENTIAL)
	return (GSS_S_COMPLETE);

    if (GSSINT_CHK_LOOP(union_cred))
	return (GSS_S_NO_CRED | GSS_S_CALL_INACCESSIBLE_READ);
    *cred_handle = NULL;

    status = GSS_S_COMPLETE;

    for(j=0; j < union_cred->count; j++) {

	mech = gssint_get_mechanism (&union_cred->mechs_array[j]);

	if (union_cred->mechs_array[j].elements)
		free(union_cred->mechs_array[j].elements);
	if (mech) {
	    if (mech->gss_release_cred) {
		temp_status = mech->gss_release_cred
		    (
		     minor_status,
		     &union_cred->cred_array[j]);

		if (temp_status != GSS_S_COMPLETE) {
		    map_error(minor_status, mech);
		    status = GSS_S_NO_CRED;
		}

	    } else
		status = GSS_S_UNAVAILABLE;
	} else
	    status = GSS_S_DEFECTIVE_CREDENTIAL;
    }

    free(union_cred->cred_array);
    free(union_cred->mechs_array);
    free(union_cred);

    return(status);
}
