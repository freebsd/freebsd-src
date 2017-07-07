/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* kdc/kdc_preauth_encts.c - Encrypted timestamp kdcpreauth module */
/*
 * Copyright (C) 1995, 2003, 2007, 2011 by the Massachusetts Institute of Technology.
 * All rights reserved.
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

#include <k5-int.h>
#include <krb5/kdcpreauth_plugin.h>
#include "kdc_util.h"

static void
enc_ts_get(krb5_context context, krb5_kdc_req *request,
           krb5_kdcpreauth_callbacks cb, krb5_kdcpreauth_rock rock,
           krb5_kdcpreauth_moddata moddata, krb5_preauthtype pa_type,
           krb5_kdcpreauth_edata_respond_fn respond, void *arg)
{
    krb5_keyblock *armor_key = cb->fast_armor(context, rock);

    /* Encrypted timestamp must not be used with FAST, and requires a key. */
    if (armor_key != NULL || !cb->have_client_keys(context, rock))
        (*respond)(arg, ENOENT, NULL);
    else
        (*respond)(arg, 0, NULL);
}

static void
enc_ts_verify(krb5_context context, krb5_data *req_pkt, krb5_kdc_req *request,
              krb5_enc_tkt_part *enc_tkt_reply, krb5_pa_data *pa,
              krb5_kdcpreauth_callbacks cb, krb5_kdcpreauth_rock rock,
              krb5_kdcpreauth_moddata moddata,
              krb5_kdcpreauth_verify_respond_fn respond, void *arg)
{
    krb5_pa_enc_ts *            pa_enc = 0;
    krb5_error_code             retval;
    krb5_data                   scratch;
    krb5_data                   enc_ts_data;
    krb5_enc_data               *enc_data = 0;
    krb5_keyblock               key;
    krb5_key_data *             client_key;
    krb5_int32                  start;
    krb5_timestamp              timenow;

    scratch.data = (char *)pa->contents;
    scratch.length = pa->length;

    enc_ts_data.data = 0;

    if ((retval = decode_krb5_enc_data(&scratch, &enc_data)) != 0)
        goto cleanup;

    enc_ts_data.length = enc_data->ciphertext.length;
    if ((enc_ts_data.data = (char *) malloc(enc_ts_data.length)) == NULL)
        goto cleanup;

    start = 0;
    while (1) {
        if ((retval = krb5_dbe_search_enctype(context, rock->client,
                                              &start, enc_data->enctype,
                                              -1, 0, &client_key)))
            goto cleanup;

        if ((retval = krb5_dbe_decrypt_key_data(context, NULL, client_key,
                                                &key, NULL)))
            goto cleanup;

        key.enctype = enc_data->enctype;

        retval = krb5_c_decrypt(context, &key, KRB5_KEYUSAGE_AS_REQ_PA_ENC_TS,
                                0, enc_data, &enc_ts_data);
        krb5_free_keyblock_contents(context, &key);
        if (retval == 0)
            break;
    }

    if ((retval = decode_krb5_pa_enc_ts(&enc_ts_data, &pa_enc)) != 0)
        goto cleanup;

    if ((retval = krb5_timeofday(context, &timenow)) != 0)
        goto cleanup;

    if (labs(timenow - pa_enc->patimestamp) > context->clockskew) {
        retval = KRB5KRB_AP_ERR_SKEW;
        goto cleanup;
    }

    setflag(enc_tkt_reply->flags, TKT_FLG_PRE_AUTH);

    retval = 0;

cleanup:
    if (enc_data) {
        krb5_free_data_contents(context, &enc_data->ciphertext);
        free(enc_data);
    }
    krb5_free_data_contents(context, &enc_ts_data);
    if (pa_enc)
        free(pa_enc);
    /* If we get NO_MATCHING_KEY, it probably means that the password was
     * incorrect. */
    if (retval == KRB5_KDB_NO_MATCHING_KEY)
        retval = KRB5KDC_ERR_PREAUTH_FAILED;

    (*respond)(arg, retval, NULL, NULL, NULL);
}

static krb5_preauthtype enc_ts_types[] = {
    KRB5_PADATA_ENC_TIMESTAMP, 0 };

krb5_error_code
kdcpreauth_encrypted_timestamp_initvt(krb5_context context, int maj_ver,
                                      int min_ver, krb5_plugin_vtable vtable)
{
    krb5_kdcpreauth_vtable vt;

    if (maj_ver != 1)
        return KRB5_PLUGIN_VER_NOTSUPP;
    vt = (krb5_kdcpreauth_vtable)vtable;
    vt->name = "encrypted_timestamp";
    vt->pa_type_list = enc_ts_types;
    vt->edata = enc_ts_get;
    vt->verify = enc_ts_verify;
    return 0;
}
