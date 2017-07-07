/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* lib/gssapi/krb5/canon_name.c */
/*
 * Copyright 1997 by the Massachusetts Institute of Technology.
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

#include "gssapiP_krb5.h"

/* This is trivial since we're a single mechanism implementation */

OM_uint32 krb5_gss_canonicalize_name(OM_uint32  *minor_status,
                                     const gss_name_t input_name,
                                     const gss_OID mech_type,
                                     gss_name_t *output_name)
{
    if ((mech_type != GSS_C_NULL_OID) &&
        !g_OID_equal(gss_mech_krb5, mech_type) &&
        !g_OID_equal(gss_mech_krb5_old, mech_type)) {
        *minor_status = 0;
        return(GSS_S_BAD_MECH);
    }

    return(krb5_gss_duplicate_name(minor_status, input_name, output_name));
}
