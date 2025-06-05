/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* lib/krb5/krb/preauth_encts.c - Encrypted timestamp clpreauth module */
/*
 * Copyright 1995, 2003, 2008, 2011 by the Massachusetts Institute of Technology.  All
 * Rights Reserved.
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

#include <k5-int.h>
#include <krb5/clpreauth_plugin.h>
#include "int-proto.h"
#include "init_creds_ctx.h"

static krb5_error_code
encts_prep_questions(krb5_context context, krb5_clpreauth_moddata moddata,
                     krb5_clpreauth_modreq modreq,
                     krb5_get_init_creds_opt *opt, krb5_clpreauth_callbacks cb,
                     krb5_clpreauth_rock rock, krb5_kdc_req *request,
                     krb5_data *encoded_request_body,
                     krb5_data *encoded_previous_request,
                     krb5_pa_data *pa_data)
{
    krb5_init_creds_context ctx = (krb5_init_creds_context)rock;

    if (!ctx->encts_disabled)
        cb->need_as_key(context, rock);
    return 0;
}

static krb5_error_code
encts_process(krb5_context context, krb5_clpreauth_moddata moddata,
              krb5_clpreauth_modreq modreq, krb5_get_init_creds_opt *opt,
              krb5_clpreauth_callbacks cb, krb5_clpreauth_rock rock,
              krb5_kdc_req *request, krb5_data *encoded_request_body,
              krb5_data *encoded_previous_request, krb5_pa_data *padata,
              krb5_prompter_fct prompter, void *prompter_data,
              krb5_pa_data ***out_padata)
{
    krb5_init_creds_context ctx = (krb5_init_creds_context)rock;
    krb5_error_code ret;
    krb5_pa_enc_ts pa_enc;
    krb5_data *ts = NULL, *enc_ts = NULL;
    krb5_enc_data enc_data;
    krb5_pa_data **pa = NULL;
    krb5_keyblock *as_key;

    enc_data.ciphertext = empty_data();

    if (ctx->encts_disabled) {
        TRACE_PREAUTH_ENC_TS_DISABLED(context);
        k5_setmsg(context, KRB5_PREAUTH_FAILED,
                  _("Encrypted timestamp is disabled"));
        return KRB5_PREAUTH_FAILED;
    }

    ret = cb->get_as_key(context, rock, &as_key);
    if (ret)
        goto cleanup;
    TRACE_PREAUTH_ENC_TS_KEY_GAK(context, as_key);

    /*
     * Try and use the timestamp of the preauth request, even if it's
     * unauthenticated.  We could be fooled into making a preauth response for
     * a future time, but that has no security consequences other than the
     * KDC's audit logs.  If kdc_timesync is not configured, then this will
     * just use local time.
     */
    ret = cb->get_preauth_time(context, rock, TRUE, &pa_enc.patimestamp,
                               &pa_enc.pausec);
    if (ret)
        goto cleanup;

    ret = encode_krb5_pa_enc_ts(&pa_enc, &ts);
    if (ret)
        goto cleanup;

    ret = krb5_encrypt_helper(context, as_key, KRB5_KEYUSAGE_AS_REQ_PA_ENC_TS,
                              ts, &enc_data);
    if (ret)
        goto cleanup;
    TRACE_PREAUTH_ENC_TS(context, pa_enc.patimestamp, pa_enc.pausec,
                         ts, &enc_data.ciphertext);

    ret = encode_krb5_enc_data(&enc_data, &enc_ts);
    if (ret)
        goto cleanup;

    pa = k5calloc(2, sizeof(krb5_pa_data *), &ret);
    if (pa == NULL)
        goto cleanup;

    pa[0] = k5alloc(sizeof(krb5_pa_data), &ret);
    if (pa[0] == NULL)
        goto cleanup;

    pa[0]->magic = KV5M_PA_DATA;
    pa[0]->pa_type = KRB5_PADATA_ENC_TIMESTAMP;
    pa[0]->length = enc_ts->length;
    pa[0]->contents = (krb5_octet *) enc_ts->data;
    enc_ts->data = NULL;
    pa[1] = NULL;
    *out_padata = pa;
    pa = NULL;

    cb->disable_fallback(context, rock);

cleanup:
    krb5_free_data(context, ts);
    krb5_free_data(context, enc_ts);
    free(enc_data.ciphertext.data);
    free(pa);
    return ret;
}

static krb5_preauthtype encts_pa_types[] = {
    KRB5_PADATA_ENC_TIMESTAMP, 0};

krb5_error_code
clpreauth_encrypted_timestamp_initvt(krb5_context context, int maj_ver,
                                     int min_ver, krb5_plugin_vtable vtable)
{
    krb5_clpreauth_vtable vt;

    if (maj_ver != 1)
        return KRB5_PLUGIN_VER_NOTSUPP;
    vt = (krb5_clpreauth_vtable)vtable;
    vt->name = "encrypted_timestamp";
    vt->pa_type_list = encts_pa_types;
    vt->prep_questions = encts_prep_questions;
    vt->process = encts_process;
    return 0;
}
