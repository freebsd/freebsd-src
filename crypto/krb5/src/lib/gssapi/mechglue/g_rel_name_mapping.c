/* -*- mode: c; indent-tabs-mode: nil -*- */
/*
 * Copyright 2009 by the Massachusetts Institute of Technology.
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

/* Glue routine for gss_release_any_name_mapping */

#include "mglueP.h"

OM_uint32 KRB5_CALLCONV
gss_release_any_name_mapping(OM_uint32 *minor_status,
                             gss_name_t name,
                             gss_buffer_t type_id,
                             gss_any_t *input)
{
    OM_uint32           status;
    gss_union_name_t    union_name;
    gss_mechanism       mech;

    if (minor_status == NULL)
        return GSS_S_CALL_INACCESSIBLE_WRITE;
    *minor_status = 0;

    if (name == GSS_C_NO_NAME)
        return GSS_S_CALL_INACCESSIBLE_READ | GSS_S_BAD_NAME;

    if (type_id == GSS_C_NO_BUFFER)
        return GSS_S_CALL_INACCESSIBLE_READ;

    if (input == NULL)
        return GSS_S_CALL_INACCESSIBLE_READ;

    union_name = (gss_union_name_t)name;

    if (union_name->mech_type == GSS_C_NO_OID)
        return GSS_S_UNAVAILABLE;

    mech = gssint_get_mechanism(name->mech_type);
    if (mech == NULL)
        return GSS_S_BAD_NAME;

    if (mech->gss_release_any_name_mapping == NULL)
        return GSS_S_UNAVAILABLE;

    status = (*mech->gss_release_any_name_mapping)(minor_status,
                                                   union_name->mech_name,
                                                   type_id,
                                                   input);
    if (status != GSS_S_COMPLETE)
        map_error(minor_status, mech);

    return status;
}
