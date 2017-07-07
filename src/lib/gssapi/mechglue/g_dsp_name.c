/* #pragma ident	"@(#)g_dsp_name.c	1.13	04/02/23 SMI" */
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
 *  glue routine for gss_display_name()
 *
 */

#include "mglueP.h"
#include <stdio.h>
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#include <string.h>

static OM_uint32
val_dsp_name_args(
    OM_uint32 *minor_status,
    gss_name_t input_name,
    gss_buffer_t output_name_buffer,
    gss_OID *output_name_type)
{

    /* Initialize outputs. */

    if (minor_status != NULL)
	*minor_status = 0;

    if (output_name_buffer != GSS_C_NO_BUFFER) {
	output_name_buffer->length = 0;
	output_name_buffer->value = NULL;
    }

    if (output_name_type != NULL)
	*output_name_type = GSS_C_NO_OID;

    /* Validate arguments. */

    if (minor_status == NULL)
	return (GSS_S_CALL_INACCESSIBLE_WRITE);

    if (output_name_buffer == GSS_C_NO_BUFFER)
	return (GSS_S_CALL_INACCESSIBLE_WRITE);

    if (input_name == GSS_C_NO_NAME)
	return (GSS_S_CALL_INACCESSIBLE_READ | GSS_S_BAD_NAME);

    return (GSS_S_COMPLETE);
}


OM_uint32 KRB5_CALLCONV
gss_display_name (minor_status,
                  input_name,
                  output_name_buffer,
                  output_name_type)

OM_uint32 *		minor_status;
gss_name_t		input_name;
gss_buffer_t		output_name_buffer;
gss_OID *		output_name_type;

{
    OM_uint32		major_status;
    gss_union_name_t	union_name;

    major_status = val_dsp_name_args(minor_status, input_name,
				     output_name_buffer, output_name_type);
    if (major_status != GSS_S_COMPLETE)
	return (major_status);

    union_name = (gss_union_name_t) input_name;

    if (union_name->mech_type) {
	/*
	 * OK, we have a mechanism-specific name; let's use it!
	 */
	return (gssint_display_internal_name(minor_status,
					    union_name->mech_type,
					    union_name->mech_name,
					    output_name_buffer,
					    output_name_type));
    }

    if ((output_name_buffer->value =
	 gssalloc_malloc(union_name->external_name->length + 1)) == NULL)
	return (GSS_S_FAILURE);
    output_name_buffer->length = union_name->external_name->length;
    (void) memcpy(output_name_buffer->value,
		  union_name->external_name->value,
		  union_name->external_name->length);
    ((char *)output_name_buffer->value)[output_name_buffer->length] = '\0';

    if (output_name_type != NULL)
	*output_name_type = union_name->name_type;

    return(GSS_S_COMPLETE);
}
