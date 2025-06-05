/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/*
 * Copyright 2000, 2007 by the Massachusetts Institute of Technology.
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
/*
 * Copyright 1993 by OpenVision Technologies, Inc.
 *
 * Permission to use, copy, modify, distribute, and sell this software
 * and its documentation for any purpose is hereby granted without fee,
 * provided that the above copyright notice appears in all copies and
 * that both that copyright notice and this permission notice appear in
 * supporting documentation, and that the name of OpenVision not be used
 * in advertising or publicity pertaining to distribution of the software
 * without specific, written prior permission. OpenVision makes no
 * representations about the suitability of this software for any
 * purpose.  It is provided "as is" without express or implied warranty.
 *
 * OPENVISION DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
 * EVENT SHALL OPENVISION BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF
 * USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR
 * OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

/*
 * Copyright (C) 1998 by the FundsXpress, INC.
 *
 * All rights reserved.
 *
 * Export of this software from the United States of America may require
 * a specific license from the United States Government.  It is the
 * responsibility of any person or organization contemplating export to
 * obtain such a license before exporting.
 *
 * WITHIN THAT CONSTRAINT, permission to use, copy, modify, and
 * distribute this software and its documentation for any purpose and
 * without fee is hereby granted, provided that the above copyright
 * notice appear in all copies and that both that copyright notice and
 * this permission notice appear in supporting documentation, and that
 * the name of FundsXpress. not be used in advertising or publicity pertaining
 * to distribution of the software without specific, written prior
 * permission.  FundsXpress makes no representations about the suitability of
 * this software for any purpose.  It is provided "as is" without express
 * or implied warranty.
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */

#include "gssapiP_krb5.h"

OM_uint32 KRB5_CALLCONV
krb5_gss_inquire_cred(minor_status, cred_handle, name, lifetime_ret,
                      cred_usage, mechanisms)
    OM_uint32 *minor_status;
    gss_cred_id_t cred_handle;
    gss_name_t *name;
    OM_uint32 *lifetime_ret;
    gss_cred_usage_t *cred_usage;
    gss_OID_set *mechanisms;
{
    krb5_context context;
    gss_cred_id_t defcred = GSS_C_NO_CREDENTIAL;
    krb5_gss_cred_id_t cred = NULL;
    krb5_error_code code;
    krb5_timestamp now;
    krb5_deltat lifetime;
    krb5_gss_name_t ret_name;
    krb5_principal princ;
    gss_OID_set mechs = GSS_C_NO_OID_SET;
    OM_uint32 major, tmpmin, ret;

    ret = GSS_S_FAILURE;
    ret_name = NULL;

    code = krb5_gss_init_context(&context);
    if (code) {
        *minor_status = code;
        return GSS_S_FAILURE;
    }

    if (name) *name = NULL;
    if (mechanisms) *mechanisms = NULL;

    /* check for default credential */
    /*SUPPRESS 29*/
    if (cred_handle == GSS_C_NO_CREDENTIAL) {
        major = kg_get_defcred(minor_status, &defcred);
        if (GSS_ERROR(major)) {
            krb5_free_context(context);
            return(major);
        }
        cred_handle = defcred;
    }

    major = kg_cred_resolve(minor_status, context, cred_handle, GSS_C_NO_NAME);
    if (GSS_ERROR(major)) {
        krb5_gss_release_cred(minor_status, &defcred);
        krb5_free_context(context);
        return(major);
    }
    cred = (krb5_gss_cred_id_t)cred_handle;

    if ((code = krb5_timeofday(context, &now))) {
        *minor_status = code;
        ret = GSS_S_FAILURE;
        goto cleanup;
    }

    if (cred->expire != 0) {
        lifetime = ts_interval(now, cred->expire);
        if (lifetime < 0)
            lifetime = 0;
    }
    else
        lifetime = GSS_C_INDEFINITE;

    if (name) {
        if (cred->name) {
            code = kg_duplicate_name(context, cred->name, &ret_name);
        } else if ((cred->usage == GSS_C_ACCEPT || cred->usage == GSS_C_BOTH)
                   && cred->keytab != NULL) {
            /* This is a default acceptor cred; use a name from the keytab if
             * we can. */
            code = k5_kt_get_principal(context, cred->keytab, &princ);
            if (code == 0) {
                code = kg_init_name(context, princ, NULL, NULL, NULL,
                                    KG_INIT_NAME_NO_COPY, &ret_name);
                if (code)
                    krb5_free_principal(context, princ);
            } else if (code == KRB5_KT_NOTFOUND)
                code = 0;
        }
        if (code) {
            *minor_status = code;
            save_error_info(*minor_status, context);
            ret = GSS_S_FAILURE;
            goto cleanup;
        }
    }

    if (mechanisms) {
        if (GSS_ERROR(ret = generic_gss_create_empty_oid_set(minor_status,
                                                             &mechs)) ||
            GSS_ERROR(ret = generic_gss_add_oid_set_member(minor_status,
                                                           gss_mech_krb5_old,
                                                           &mechs)) ||
            GSS_ERROR(ret = generic_gss_add_oid_set_member(minor_status,
                                                           gss_mech_krb5,
                                                           &mechs))) {
            if (ret_name)
                kg_release_name(context, &ret_name);
            /* *minor_status set above */
            goto cleanup;
        }
    }

    if (name) {
        if (ret_name != NULL)
            *name = (gss_name_t) ret_name;
        else
            *name = GSS_C_NO_NAME;
    }

    if (lifetime_ret)
        *lifetime_ret = lifetime;

    if (cred_usage)
        *cred_usage = cred->usage;

    if (mechanisms) {
        *mechanisms = mechs;
        mechs = GSS_C_NO_OID_SET;
    }

    *minor_status = 0;
    ret = (lifetime == 0) ? GSS_S_CREDENTIALS_EXPIRED : GSS_S_COMPLETE;

cleanup:
    k5_mutex_unlock(&cred->lock);
    krb5_gss_release_cred(&tmpmin, &defcred);
    krb5_free_context(context);
    (void)generic_gss_release_oid_set(&tmpmin, &mechs);
    return ret;
}

/* V2 interface */
OM_uint32 KRB5_CALLCONV
krb5_gss_inquire_cred_by_mech(minor_status, cred_handle,
                              mech_type, name, initiator_lifetime,
                              acceptor_lifetime, cred_usage)
    OM_uint32           *minor_status;
    gss_cred_id_t       cred_handle;
    gss_OID             mech_type;
    gss_name_t          *name;
    OM_uint32           *initiator_lifetime;
    OM_uint32           *acceptor_lifetime;
    gss_cred_usage_t *cred_usage;
{
    krb5_gss_cred_id_t  cred;
    OM_uint32           lifetime;
    OM_uint32           mstat;

    cred = (krb5_gss_cred_id_t) cred_handle;
    mstat = krb5_gss_inquire_cred(minor_status,
                                  cred_handle,
                                  name,
                                  &lifetime,
                                  cred_usage,
                                  (gss_OID_set *) NULL);
    if (mstat == GSS_S_COMPLETE) {
        if (cred &&
            ((cred->usage == GSS_C_INITIATE) ||
             (cred->usage == GSS_C_BOTH)) &&
            initiator_lifetime)
            *initiator_lifetime = lifetime;
        if (cred &&
            ((cred->usage == GSS_C_ACCEPT) ||
             (cred->usage == GSS_C_BOTH)) &&
            acceptor_lifetime)
            *acceptor_lifetime = lifetime;
    }
    return(mstat);
}

OM_uint32
gss_krb5int_get_cred_impersonator(OM_uint32 *minor_status,
                                  const gss_cred_id_t cred_handle,
                                  const gss_OID desired_object,
                                  gss_buffer_set_t *data_set)
{
    krb5_gss_cred_id_t cred = (krb5_gss_cred_id_t)cred_handle;
    gss_buffer_desc rep = GSS_C_EMPTY_BUFFER;
    krb5_context context = NULL;
    char *impersonator = NULL;
    krb5_error_code ret;
    OM_uint32 major;

    *data_set = GSS_C_NO_BUFFER_SET;

    /* Return an empty buffer set if no impersonator is present */
    if (cred->impersonator == NULL)
        return generic_gss_create_empty_buffer_set(minor_status, data_set);

    ret = krb5_gss_init_context(&context);
    if (ret) {
        *minor_status = ret;
        return GSS_S_FAILURE;
    }

    ret = krb5_unparse_name(context, cred->impersonator, &impersonator);
    if (ret) {
        krb5_free_context(context);
        *minor_status = ret;
        return GSS_S_FAILURE;
    }

    rep.value = impersonator;
    rep.length = strlen(impersonator);
    major = generic_gss_add_buffer_set_member(minor_status, &rep, data_set);

    krb5_free_unparsed_name(context, impersonator);
    krb5_free_context(context);
    return major;
}
