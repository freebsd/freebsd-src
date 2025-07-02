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

#include "mglueP.h"
#include <stdio.h>
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#include <string.h>
#include <errno.h>

OM_uint32 KRB5_CALLCONV gss_create_empty_buffer_set
	   (OM_uint32 * minor_status,
	    gss_buffer_set_t *buffer_set)
{
    return generic_gss_create_empty_buffer_set(minor_status, buffer_set);
}

OM_uint32 KRB5_CALLCONV gss_add_buffer_set_member
	   (OM_uint32 * minor_status,
	    const gss_buffer_t member_buffer,
	    gss_buffer_set_t *buffer_set)
{
    return generic_gss_add_buffer_set_member(minor_status,
					     member_buffer,
					     buffer_set);
}

OM_uint32 KRB5_CALLCONV gss_release_buffer_set
	   (OM_uint32 * minor_status,
	    gss_buffer_set_t *buffer_set)
{
    return generic_gss_release_buffer_set(minor_status, buffer_set);
}
