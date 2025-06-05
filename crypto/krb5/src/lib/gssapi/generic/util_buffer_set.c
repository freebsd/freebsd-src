/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
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
 *
 */

#include "gssapiP_generic.h"
#include <stdio.h>
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#include <string.h>
#include <errno.h>

OM_uint32
generic_gss_create_empty_buffer_set(OM_uint32 * minor_status,
                                    gss_buffer_set_t *buffer_set)
{
    gss_buffer_set_t set;

    set = (gss_buffer_set_desc *) gssalloc_malloc(sizeof(*set));
    if (set == GSS_C_NO_BUFFER_SET) {
        *minor_status = ENOMEM;
        return GSS_S_FAILURE;
    }

    set->count = 0;
    set->elements = NULL;

    *buffer_set = set;

    *minor_status = 0;
    return GSS_S_COMPLETE;
}

OM_uint32
generic_gss_add_buffer_set_member(OM_uint32 * minor_status,
                                  const gss_buffer_t member_buffer,
                                  gss_buffer_set_t *buffer_set)
{
    gss_buffer_set_t set;
    gss_buffer_t p;
    OM_uint32 ret;

    if (*buffer_set == GSS_C_NO_BUFFER_SET) {
        ret = generic_gss_create_empty_buffer_set(minor_status,
                                                  buffer_set);
        if (ret) {
            return ret;
        }
    }

    set = *buffer_set;
    set->elements = (gss_buffer_desc *)gssalloc_realloc(set->elements,
                                                        (set->count + 1) *
                                                        sizeof(gss_buffer_desc));
    if (set->elements == NULL) {
        *minor_status = ENOMEM;
        return GSS_S_FAILURE;
    }

    p = &set->elements[set->count];

    p->value = gssalloc_malloc(member_buffer->length);
    if (p->value == NULL) {
        *minor_status = ENOMEM;
        return GSS_S_FAILURE;
    }
    memcpy(p->value, member_buffer->value, member_buffer->length);
    p->length = member_buffer->length;

    set->count++;

    *minor_status = 0;
    return GSS_S_COMPLETE;
}

OM_uint32
generic_gss_release_buffer_set(OM_uint32 * minor_status,
                               gss_buffer_set_t *buffer_set)
{
    size_t i;
    OM_uint32 minor;

    *minor_status = 0;

    if (*buffer_set == GSS_C_NO_BUFFER_SET) {
        return GSS_S_COMPLETE;
    }

    for (i = 0; i < (*buffer_set)->count; i++) {
        generic_gss_release_buffer(&minor, &((*buffer_set)->elements[i]));
    }

    if ((*buffer_set)->elements != NULL) {
        gssalloc_free((*buffer_set)->elements);
        (*buffer_set)->elements = NULL;
    }

    (*buffer_set)->count = 0;

    gssalloc_free(*buffer_set);
    *buffer_set = GSS_C_NO_BUFFER_SET;

    return GSS_S_COMPLETE;
}
