/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/*
 * Copyright 2009  by the Massachusetts Institute of Technology.
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
#ifdef HAVE_MEMORY_H
#include <memory.h>
#endif
#include <assert.h>

static int
kg_is_initiator_cred(krb5_gss_cred_id_t cred)
{
    return (cred->usage == GSS_C_INITIATE || cred->usage == GSS_C_BOTH) &&
        (cred->ccache != NULL);
}

static OM_uint32
kg_impersonate_name(OM_uint32 *minor_status,
                    const krb5_gss_cred_id_t impersonator_cred,
                    const krb5_gss_name_t user,
                    OM_uint32 time_req,
                    krb5_gss_cred_id_t *output_cred,
                    OM_uint32 *time_rec,
                    krb5_context context)
{
    OM_uint32 major_status;
    krb5_error_code code;
    krb5_creds in_creds, *out_creds = NULL;

    *output_cred = NULL;
    memset(&in_creds, 0, sizeof(in_creds));

    in_creds.client = user->princ;
    in_creds.server = impersonator_cred->name->princ;

    if (impersonator_cred->req_enctypes != NULL)
        in_creds.keyblock.enctype = impersonator_cred->req_enctypes[0];

    k5_mutex_lock(&user->lock);

    if (user->ad_context != NULL) {
        code = krb5_authdata_export_authdata(context,
                                             user->ad_context,
                                             AD_USAGE_TGS_REQ,
                                             &in_creds.authdata);
        if (code != 0) {
            k5_mutex_unlock(&user->lock);
            *minor_status = code;
            return GSS_S_FAILURE;
        }
    }

    k5_mutex_unlock(&user->lock);

    code = krb5_get_credentials_for_user(context,
                                         KRB5_GC_CANONICALIZE | KRB5_GC_NO_STORE,
                                         impersonator_cred->ccache,
                                         &in_creds,
                                         NULL, &out_creds);
    if (code != 0) {
        krb5_free_authdata(context, in_creds.authdata);
        *minor_status = code;
        return GSS_S_FAILURE;
    }

    major_status = kg_compose_deleg_cred(minor_status,
                                         impersonator_cred,
                                         out_creds,
                                         time_req,
                                         output_cred,
                                         time_rec,
                                         context);

    krb5_free_authdata(context, in_creds.authdata);
    krb5_free_creds(context, out_creds);

    return major_status;
}

/* The mechglue always passes null desired_mechs and actual_mechs. */
OM_uint32 KRB5_CALLCONV
krb5_gss_acquire_cred_impersonate_name(OM_uint32 *minor_status,
                                       const gss_cred_id_t impersonator_cred_handle,
                                       const gss_name_t desired_name,
                                       OM_uint32 time_req,
                                       const gss_OID_set desired_mechs,
                                       gss_cred_usage_t cred_usage,
                                       gss_cred_id_t *output_cred_handle,
                                       gss_OID_set *actual_mechs,
                                       OM_uint32 *time_rec)
{
    OM_uint32 major_status;
    krb5_error_code code;
    krb5_gss_cred_id_t imp_cred = (krb5_gss_cred_id_t)impersonator_cred_handle;
    krb5_gss_cred_id_t cred;
    krb5_context context;

    if (impersonator_cred_handle == GSS_C_NO_CREDENTIAL)
        return GSS_S_CALL_INACCESSIBLE_READ;

    if (desired_name == GSS_C_NO_NAME)
        return GSS_S_CALL_INACCESSIBLE_READ;

    if (output_cred_handle == NULL)
        return GSS_S_CALL_INACCESSIBLE_WRITE;

    if (cred_usage != GSS_C_INITIATE) {
        *minor_status = (OM_uint32)G_BAD_USAGE;
        return GSS_S_FAILURE;
    }

    if (imp_cred->usage != GSS_C_INITIATE && imp_cred->usage != GSS_C_BOTH) {
        *minor_status = 0;
        return GSS_S_NO_CRED;
    }

    *output_cred_handle = GSS_C_NO_CREDENTIAL;
    if (time_rec != NULL)
        *time_rec = 0;

    code = krb5_gss_init_context(&context);
    if (code != 0) {
        *minor_status = code;
        return GSS_S_FAILURE;
    }

    major_status = kg_cred_resolve(minor_status, context,
                                   impersonator_cred_handle, NULL);
    if (GSS_ERROR(major_status)) {
        krb5_free_context(context);
        return major_status;
    }

    major_status = kg_impersonate_name(minor_status,
                                       imp_cred,
                                       (krb5_gss_name_t)desired_name,
                                       time_req,
                                       &cred,
                                       time_rec,
                                       context);

    if (!GSS_ERROR(major_status))
        *output_cred_handle = (gss_cred_id_t)cred;

    k5_mutex_unlock(&imp_cred->lock);
    krb5_free_context(context);

    return major_status;

}

/*
 * Set up cred to be an S4U2Proxy credential by copying in the impersonator's
 * creds, setting a cache config variable with the impersonator principal name,
 * and saving the impersonator principal name in the cred structure.
 */
static krb5_error_code
make_proxy_cred(krb5_context context, krb5_gss_cred_id_t cred,
                krb5_gss_cred_id_t impersonator_cred)
{
    krb5_error_code code;
    krb5_data data;
    char *str;

    code = krb5_cc_copy_creds(context, impersonator_cred->ccache,
                              cred->ccache);
    if (code)
        return code;

    code = krb5_unparse_name(context, impersonator_cred->name->princ, &str);
    if (code)
        return code;

    data = string2data(str);
    code = krb5_cc_set_config(context, cred->ccache, NULL,
                              KRB5_CC_CONF_PROXY_IMPERSONATOR, &data);
    krb5_free_unparsed_name(context, str);
    if (code)
        return code;

    return krb5_copy_principal(context, impersonator_cred->name->princ,
                               &cred->impersonator);
}

OM_uint32
kg_compose_deleg_cred(OM_uint32 *minor_status,
                      krb5_gss_cred_id_t impersonator_cred,
                      krb5_creds *subject_creds,
                      OM_uint32 time_req,
                      krb5_gss_cred_id_t *output_cred,
                      OM_uint32 *time_rec,
                      krb5_context context)
{
    OM_uint32 major_status;
    krb5_error_code code;
    krb5_gss_cred_id_t cred = NULL;

    *output_cred = NULL;
    k5_mutex_assert_locked(&impersonator_cred->lock);

    if (!kg_is_initiator_cred(impersonator_cred) ||
        impersonator_cred->name == NULL ||
        impersonator_cred->impersonator != NULL) {
        code = G_BAD_USAGE;
        goto cleanup;
    }

    assert(impersonator_cred->name->princ != NULL);

    assert(subject_creds != NULL);
    assert(subject_creds->client != NULL);

    cred = xmalloc(sizeof(*cred));
    if (cred == NULL) {
        code = ENOMEM;
        goto cleanup;
    }
    memset(cred, 0, sizeof(*cred));

    code = k5_mutex_init(&cred->lock);
    if (code != 0)
        goto cleanup;

    cred->usage = GSS_C_INITIATE;

    cred->expire = subject_creds->times.endtime;

    code = kg_init_name(context, subject_creds->client, NULL, NULL, NULL, 0,
                        &cred->name);
    if (code != 0)
        goto cleanup;

    code = krb5_cc_new_unique(context, "MEMORY", NULL, &cred->ccache);
    if (code != 0)
        goto cleanup;
    cred->destroy_ccache = 1;

    code = krb5_cc_initialize(context, cred->ccache, subject_creds->client);
    if (code != 0)
        goto cleanup;

    /*
     * Only return a "proxy" credential for use with constrained
     * delegation if the subject credentials are forwardable.
     * Submitting non-forwardable credentials to the KDC for use
     * with constrained delegation will only return an error.
     */
    if (subject_creds->ticket_flags & TKT_FLG_FORWARDABLE) {
        code = make_proxy_cred(context, cred, impersonator_cred);
        if (code != 0)
            goto cleanup;
    }

    code = krb5_cc_store_cred(context, cred->ccache, subject_creds);
    if (code != 0)
        goto cleanup;

    if (time_rec != NULL) {
        krb5_timestamp now;

        code = krb5_timeofday(context, &now);
        if (code != 0)
            goto cleanup;

        *time_rec = cred->expire - now;
    }

    major_status = GSS_S_COMPLETE;
    *minor_status = 0;
    *output_cred = cred;

cleanup:
    if (code != 0) {
        *minor_status = code;
        major_status = GSS_S_FAILURE;
    }

    if (GSS_ERROR(major_status) && cred != NULL) {
        k5_mutex_destroy(&cred->lock);
        krb5_cc_destroy(context, cred->ccache);
        kg_release_name(context, &cred->name);
        xfree(cred);
    }

    return major_status;
}
