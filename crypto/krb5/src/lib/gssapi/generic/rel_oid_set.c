/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* #ident  "@(#)gss_release_oid_set.c 1.12     95/08/23 SMI" */

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
 *  glue routine for gss_release_oid_set
 */

#include "gssapiP_generic.h"

#include <stdio.h>
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif

OM_uint32
generic_gss_release_oid_set(
    OM_uint32 *minor_status,
    gss_OID_set *set)
{
    size_t i;
    if (minor_status)
        *minor_status = 0;

    if (set == NULL)
        return(GSS_S_COMPLETE);

    if (*set == GSS_C_NULL_OID_SET)
        return(GSS_S_COMPLETE);

    for (i=0; i<(*set)->count; i++)
        gssalloc_free((*set)->elements[i].elements);

    gssalloc_free((*set)->elements);
    gssalloc_free(*set);

    *set = GSS_C_NULL_OID_SET;

    return(GSS_S_COMPLETE);
}
