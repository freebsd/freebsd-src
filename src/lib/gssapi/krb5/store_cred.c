/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* lib/gssapi/krb5/store_cred.c */
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

#include "k5-int.h"
#include "gssapiP_krb5.h"

static int
has_unexpired_creds(krb5_gss_cred_id_t kcred,
                    const gss_OID desired_mech,
                    int default_cred,
                    gss_const_key_value_set_t cred_store)
{
    OM_uint32 major_status, minor;
    gss_name_t cred_name;
    gss_OID_set_desc desired_mechs;
    gss_cred_id_t tmp_cred = GSS_C_NO_CREDENTIAL;
    OM_uint32 time_rec;

    desired_mechs.count = 1;
    desired_mechs.elements = (gss_OID)desired_mech;

    if (default_cred)
        cred_name = GSS_C_NO_NAME;
    else
        cred_name = (gss_name_t)kcred->name;

    major_status = krb5_gss_acquire_cred_from(&minor, cred_name, 0,
                                              &desired_mechs, GSS_C_INITIATE,
                                              cred_store, &tmp_cred, NULL,
                                              &time_rec);

    krb5_gss_release_cred(&minor, &tmp_cred);

    return (GSS_ERROR(major_status) || time_rec);
}

static OM_uint32
copy_initiator_creds(OM_uint32 *minor_status,
                     gss_cred_id_t input_cred_handle,
                     const gss_OID desired_mech,
                     OM_uint32 overwrite_cred,
                     OM_uint32 default_cred,
                     gss_const_key_value_set_t cred_store)
{
    OM_uint32 major_status;
    krb5_error_code code;
    krb5_gss_cred_id_t kcred = NULL;
    krb5_context context = NULL;
    krb5_ccache ccache = NULL;
    const char *ccache_name;

    *minor_status = 0;

    if (!default_cred && cred_store == GSS_C_NO_CRED_STORE) {
        *minor_status = G_STORE_NON_DEFAULT_CRED_NOSUPP;
        major_status = GSS_S_FAILURE;
        goto cleanup;
    }

    code = krb5_gss_init_context(&context);
    if (code != 0) {
        *minor_status = code;
        major_status = GSS_S_FAILURE;
        goto cleanup;
    }

    major_status = krb5_gss_validate_cred_1(minor_status,
                                            input_cred_handle,
                                            context);
    if (GSS_ERROR(major_status))
        goto cleanup;

    kcred = (krb5_gss_cred_id_t)input_cred_handle;

    if (kcred->ccache == NULL) {
        *minor_status = KG_CCACHE_NOMATCH;
        major_status = GSS_S_DEFECTIVE_CREDENTIAL;
        goto cleanup;
    }

    if (!overwrite_cred &&
        has_unexpired_creds(kcred, desired_mech, default_cred, cred_store)) {
        major_status = GSS_S_DUPLICATE_ELEMENT;
        goto cleanup;
    }

    major_status = kg_value_from_cred_store(cred_store,
                                            KRB5_CS_CCACHE_URN, &ccache_name);
    if (GSS_ERROR(major_status))
        goto cleanup;

    if (ccache_name != NULL) {
        code = krb5_cc_resolve(context, ccache_name, &ccache);
        if (code != 0) {
            *minor_status = code;
            major_status = GSS_S_CRED_UNAVAIL;
            goto cleanup;
        }
        code = krb5_cc_initialize(context, ccache,
                                  kcred->name->princ);
        if (code != 0) {
            *minor_status = code;
            major_status = GSS_S_CRED_UNAVAIL;
            goto cleanup;
        }
    }

    if (ccache == NULL) {
        if (!default_cred) {
            *minor_status = G_STORE_NON_DEFAULT_CRED_NOSUPP;
            major_status = GSS_S_FAILURE;
            goto cleanup;
        }
        code = krb5int_cc_default(context, &ccache);
        if (code != 0) {
            *minor_status = code;
            major_status = GSS_S_FAILURE;
            goto cleanup;
        }
    }

    code = krb5_cc_copy_creds(context, kcred->ccache, ccache);
    if (code != 0) {
        *minor_status = code;
        major_status = GSS_S_FAILURE;
        goto cleanup;
    }

    *minor_status = 0;
    major_status = GSS_S_COMPLETE;

cleanup:
    if (kcred != NULL)
        k5_mutex_unlock(&kcred->lock);
    if (ccache != NULL)
        krb5_cc_close(context, ccache);
    krb5_free_context(context);

    return major_status;
}

OM_uint32 KRB5_CALLCONV
krb5_gss_store_cred(OM_uint32 *minor_status,
                    gss_cred_id_t input_cred_handle,
                    gss_cred_usage_t cred_usage,
                    const gss_OID desired_mech,
                    OM_uint32 overwrite_cred,
                    OM_uint32 default_cred,
                    gss_OID_set *elements_stored,
                    gss_cred_usage_t *cred_usage_stored)
{
    return krb5_gss_store_cred_into(minor_status, input_cred_handle,
                                    cred_usage, desired_mech,
                                    overwrite_cred, default_cred,
                                    GSS_C_NO_CRED_STORE,
                                    elements_stored, cred_usage_stored);
}

OM_uint32 KRB5_CALLCONV
krb5_gss_store_cred_into(OM_uint32 *minor_status,
                         gss_cred_id_t input_cred_handle,
                         gss_cred_usage_t cred_usage,
                         const gss_OID desired_mech,
                         OM_uint32 overwrite_cred,
                         OM_uint32 default_cred,
                         gss_const_key_value_set_t cred_store,
                         gss_OID_set *elements_stored,
                         gss_cred_usage_t *cred_usage_stored)
{
    OM_uint32 major_status;
    gss_cred_usage_t actual_usage;
    OM_uint32 lifetime;

    if (input_cred_handle == GSS_C_NO_CREDENTIAL)
        return GSS_S_NO_CRED;

    major_status = GSS_S_FAILURE;

    if (cred_usage == GSS_C_ACCEPT) {
        *minor_status = G_STORE_ACCEPTOR_CRED_NOSUPP;
        return GSS_S_FAILURE;
    } else if (cred_usage != GSS_C_INITIATE && cred_usage != GSS_C_BOTH) {
        *minor_status = G_BAD_USAGE;
        return GSS_S_FAILURE;
    }

    major_status = krb5_gss_inquire_cred(minor_status, input_cred_handle,
                                         NULL, &lifetime,
                                         &actual_usage, elements_stored);
    if (GSS_ERROR(major_status))
        return major_status;

    if (lifetime == 0)
        return GSS_S_CREDENTIALS_EXPIRED;

    if (actual_usage != GSS_C_INITIATE && actual_usage != GSS_C_BOTH) {
        *minor_status = G_STORE_ACCEPTOR_CRED_NOSUPP;
        return GSS_S_FAILURE;
    }

    major_status = copy_initiator_creds(minor_status, input_cred_handle,
                                        desired_mech, overwrite_cred,
                                        default_cred, cred_store);
    if (GSS_ERROR(major_status))
        return major_status;

    if (cred_usage_stored != NULL)
        *cred_usage_stored = GSS_C_INITIATE;

    return GSS_S_COMPLETE;
}
