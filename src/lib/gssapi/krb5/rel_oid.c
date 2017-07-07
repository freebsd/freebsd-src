/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* lib/gssapi/krb5/rel_oid.c - Release an OID */
/*
 * Copyright 1995, 2007 by the Massachusetts Institute of Technology.
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

OM_uint32
krb5_gss_release_oid(minor_status, oid)
    OM_uint32   *minor_status;
    gss_OID     *oid;
{
    /*
     * The V2 API says the following!
     *
     * gss_release_oid[()] will recognize any of the GSSAPI's own OID values,
     * and will silently ignore attempts to free these OIDs; for other OIDs
     * it will call the C free() routine for both the OID data and the
     * descriptor.  This allows applications to freely mix their own heap-
     * allocated OID values with OIDs returned by GSS-API.
     */
    if (krb5_gss_internal_release_oid(minor_status, oid) != GSS_S_COMPLETE) {
        /* Pawn it off on the generic routine */
        return(generic_gss_release_oid(minor_status, oid));
    }
    else {
        *oid = GSS_C_NO_OID;
        *minor_status = 0;
        return(GSS_S_COMPLETE);
    }
}

OM_uint32 KRB5_CALLCONV
krb5_gss_internal_release_oid(minor_status, oid)
    OM_uint32   *minor_status;
    gss_OID     *oid;
{
    /*
     * This function only knows how to release internal OIDs. It will
     * return GSS_S_CONTINUE_NEEDED for any OIDs it does not recognize.
     */

    *minor_status = 0;
    if ((*oid != gss_mech_krb5) &&
        (*oid != gss_mech_krb5_old) &&
        (*oid != gss_mech_krb5_wrong) &&
        (*oid != gss_mech_iakerb) &&
        (*oid != gss_nt_krb5_name) &&
        (*oid != gss_nt_krb5_principal)) {
        /* We don't know about this OID */
        return(GSS_S_CONTINUE_NEEDED);
    }
    else {
        *oid = GSS_C_NO_OID;
        return(GSS_S_COMPLETE);
    }
}
