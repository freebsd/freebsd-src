/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* lib/krb5/krb/get_etype_salt_s2kp.c - Retrieve enctype, salt and s2kparams */
/*
 * Copyright (C) 2017 by Cloudera, Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * * Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 *
 * * Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer in
 *   the documentation and/or other materials provided with the
 *   distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "k5-int.h"
#include "fast.h"
#include "init_creds_ctx.h"

/* Extract etype info from the error message pkt into icc, if it is a
 * PREAUTH_REQUIRED error.  Otherwise return the protocol error code. */
static krb5_error_code
get_from_error(krb5_context context, krb5_data *pkt,
               krb5_init_creds_context icc)
{
    krb5_error *error = NULL;
    krb5_pa_data **padata = NULL;
    krb5_error_code ret;

    ret = decode_krb5_error(pkt, &error);
    if (ret)
        return ret;
    ret = krb5int_fast_process_error(context, icc->fast_state, &error, &padata,
                                     NULL);
    if (ret)
        goto cleanup;
    if (error->error != KDC_ERR_PREAUTH_REQUIRED) {
        ret = ERROR_TABLE_BASE_krb5 + error->error;
        goto cleanup;
    }
    ret = k5_get_etype_info(context, icc, padata);

cleanup:
    krb5_free_pa_data(context, padata);
    krb5_free_error(context, error);
    return ret;
}

/* Extract etype info from the AS reply pkt into icc. */
static krb5_error_code
get_from_reply(krb5_context context, krb5_data *pkt,
               krb5_init_creds_context icc)
{
    krb5_kdc_rep *asrep = NULL;
    krb5_error_code ret;
    krb5_keyblock *strengthen_key = NULL;

    ret = decode_krb5_as_rep(pkt, &asrep);
    if (ret)
        return ret;
    ret = krb5int_fast_process_response(context, icc->fast_state, asrep,
                                        &strengthen_key);
    if (ret)
        goto cleanup;
    ret = k5_get_etype_info(context, icc, asrep->padata);

cleanup:
    krb5_free_kdc_rep(context, asrep);
    krb5_free_keyblock(context, strengthen_key);
    return ret;
}

krb5_error_code KRB5_CALLCONV
krb5_get_etype_info(krb5_context context, krb5_principal principal,
                    krb5_get_init_creds_opt *opt, krb5_enctype *enctype_out,
                    krb5_data *salt_out, krb5_data *s2kparams_out)
{
    krb5_init_creds_context icc = NULL;
    krb5_data reply = empty_data(), req = empty_data(), realm = empty_data();
    krb5_data salt = empty_data(), s2kparams = empty_data();
    unsigned int flags;
    int primary, tcp_only;
    krb5_error_code ret;

    *enctype_out = ENCTYPE_NULL;
    *salt_out = empty_data();
    *s2kparams_out = empty_data();

    /* Create an initial creds context and get the initial request packet. */
    ret = krb5_init_creds_init(context, principal, NULL, NULL, 0, opt, &icc);
    if (ret)
        goto cleanup;
    ret = krb5_init_creds_step(context, icc, &reply, &req, &realm, &flags);
    if (ret)
        goto cleanup;
    if (flags != KRB5_INIT_CREDS_STEP_FLAG_CONTINUE) {
        ret = KRB5KRB_AP_ERR_MSG_TYPE;
        goto cleanup;
    }

    /* Send the packet (possibly once with UDP and again with TCP). */
    tcp_only = 0;
    for (;;) {
        primary = 0;
        ret = krb5_sendto_kdc(context, &req, &realm, &reply, &primary,
                              tcp_only);
        if (ret)
            goto cleanup;

        icc->etype = ENCTYPE_NULL;
        if (krb5_is_krb_error(&reply)) {
            ret = get_from_error(context, &reply, icc);
            if (ret) {
                if (!tcp_only && ret == KRB5KRB_ERR_RESPONSE_TOO_BIG) {
                    tcp_only = 1;
                    krb5_free_data_contents(context, &reply);
                    continue;
                }
                goto cleanup;
            }
        } else if (krb5_is_as_rep(&reply)) {
            ret = get_from_reply(context, &reply, icc);
            if (ret)
                goto cleanup;
        } else {
            ret = KRB5KRB_AP_ERR_MSG_TYPE;
            goto cleanup;
        }
        break;
    }

    /* If we found no etype-info, return successfully with all null values. */
    if (icc->etype == ENCTYPE_NULL)
        goto cleanup;

    if (icc->default_salt)
        ret = krb5_principal2salt(context, principal, &salt);
    else if (icc->salt.length > 0)
        ret = krb5int_copy_data_contents(context, &icc->salt, &salt);
    if (ret)
        goto cleanup;

    if (icc->s2kparams.length > 0) {
        ret = krb5int_copy_data_contents(context, &icc->s2kparams, &s2kparams);
        if (ret)
            goto cleanup;
    }

    *salt_out = salt;
    *s2kparams_out = s2kparams;
    *enctype_out = icc->etype;
    salt = empty_data();
    s2kparams = empty_data();

cleanup:
    krb5_free_data_contents(context, &req);
    krb5_free_data_contents(context, &reply);
    krb5_free_data_contents(context, &realm);
    krb5_free_data_contents(context, &salt);
    krb5_free_data_contents(context, &s2kparams);
    krb5_init_creds_free(context, icc);
    return ret;
}
