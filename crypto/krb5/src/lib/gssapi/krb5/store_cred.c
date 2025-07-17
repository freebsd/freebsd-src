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

static OM_uint32
copy_initiator_creds(OM_uint32 *minor_status,
                     gss_cred_id_t input_cred_handle,
                     const gss_OID desired_mech,
                     OM_uint32 overwrite_cred,
                     OM_uint32 default_cred,
                     gss_const_key_value_set_t cred_store)
{
    OM_uint32 major_status;
    krb5_error_code ret;
    krb5_gss_cred_id_t kcred = NULL;
    krb5_context context = NULL;
    krb5_ccache cache = NULL, defcache = NULL, mcc = NULL;
    krb5_principal princ = NULL;
    krb5_boolean switch_to_cache = FALSE;
    const char *ccache_name, *deftype;

    *minor_status = 0;

    ret = krb5_gss_init_context(&context);
    if (ret)
        goto kerr_cleanup;

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

    major_status = kg_value_from_cred_store(cred_store,
                                            KRB5_CS_CCACHE_URN, &ccache_name);
    if (GSS_ERROR(major_status))
        goto cleanup;

    if (ccache_name != NULL) {
        ret = krb5_cc_set_default_name(context, ccache_name);
        if (ret)
            goto kerr_cleanup;
    } else {
        major_status = kg_sync_ccache_name(context, minor_status);
        if (major_status != GSS_S_COMPLETE)
            goto cleanup;
    }

    /* Resolve the default ccache and get its type. */
    ret = krb5_cc_default(context, &defcache);
    if (ret)
        goto kerr_cleanup;
    deftype = krb5_cc_get_type(context, defcache);

    if (krb5_cc_support_switch(context, deftype)) {
        /* Use an existing or new cache within the collection. */
        ret = krb5_cc_cache_match(context, kcred->name->princ, &cache);
        if (!ret && !overwrite_cred) {
            major_status = GSS_S_DUPLICATE_ELEMENT;
            goto cleanup;
        }
        if (ret == KRB5_CC_NOTFOUND)
            ret = krb5_cc_new_unique(context, deftype, NULL, &cache);
        if (ret)
            goto kerr_cleanup;
        switch_to_cache = default_cred;
    } else {
        /* Use the default cache. */
        cache = defcache;
        defcache = NULL;
        ret = krb5_cc_get_principal(context, cache, &princ);
        krb5_free_principal(context, princ);
        if (!ret && !overwrite_cred) {
            major_status = GSS_S_DUPLICATE_ELEMENT;
            goto cleanup;
        }
    }

    ret = krb5_cc_new_unique(context, "MEMORY", NULL, &mcc);
    if (ret)
        goto kerr_cleanup;
    ret = krb5_cc_initialize(context, mcc, kcred->name->princ);
    if (ret)
        goto kerr_cleanup;
    ret = krb5_cc_copy_creds(context, kcred->ccache, mcc);
    if (ret)
        goto kerr_cleanup;
    ret = krb5_cc_move(context, mcc, cache);
    if (ret)
        goto kerr_cleanup;
    mcc = NULL;

    if (switch_to_cache) {
        ret = krb5_cc_switch(context, cache);
        if (ret)
            goto kerr_cleanup;
    }

    *minor_status = 0;
    major_status = GSS_S_COMPLETE;

cleanup:
    if (kcred != NULL)
        k5_mutex_unlock(&kcred->lock);
    if (defcache != NULL)
        krb5_cc_close(context, defcache);
    if (cache != NULL)
        krb5_cc_close(context, cache);
    if (mcc != NULL)
        krb5_cc_destroy(context, mcc);
    krb5_free_context(context);

    return major_status;

kerr_cleanup:
    *minor_status = ret;
    major_status = GSS_S_FAILURE;
    goto cleanup;
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
