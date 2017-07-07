/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/*
 * Copyright 1997, 2007 by Massachusetts Institute of Technology
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

/* Check to see whether or not a GSSAPI krb5 credential is valid.  If
 * it is not, return an error. */

OM_uint32
krb5_gss_validate_cred_1(OM_uint32 *minor_status, gss_cred_id_t cred_handle,
                         krb5_context context)
{
    krb5_gss_cred_id_t cred;
    krb5_error_code code;
    krb5_principal princ;

    cred = (krb5_gss_cred_id_t) cred_handle;
    k5_mutex_lock(&cred->lock);

    if (cred->ccache && cred->expire != 0) {
        if ((code = krb5_cc_get_principal(context, cred->ccache, &princ))) {
            k5_mutex_unlock(&cred->lock);
            *minor_status = code;
            return(GSS_S_DEFECTIVE_CREDENTIAL);
        }
        if (!krb5_principal_compare(context, princ, cred->name->princ)) {
            k5_mutex_unlock(&cred->lock);
            *minor_status = KG_CCACHE_NOMATCH;
            return(GSS_S_DEFECTIVE_CREDENTIAL);
        }
        (void)krb5_free_principal(context, princ);
    }
    *minor_status = 0;
    return GSS_S_COMPLETE;
}

OM_uint32
krb5_gss_validate_cred(minor_status, cred_handle)
    OM_uint32 *minor_status;
    gss_cred_id_t cred_handle;
{
    krb5_context context;
    krb5_error_code code;
    OM_uint32 maj;

    code = krb5_gss_init_context(&context);
    if (code) {
        *minor_status = code;
        return GSS_S_FAILURE;
    }

    maj = krb5_gss_validate_cred_1(minor_status, cred_handle, context);
    if (maj == 0) {
        krb5_gss_cred_id_t cred = (krb5_gss_cred_id_t) cred_handle;
        k5_mutex_assert_locked(&cred->lock);
        k5_mutex_unlock(&cred->lock);
    }
    save_error_info(*minor_status, context);
    krb5_free_context(context);
    return maj;
}
