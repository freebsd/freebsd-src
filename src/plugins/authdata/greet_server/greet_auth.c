/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* plugins/authdata/greet_server/greet_auth.c */
/*
 * Copyright 2009 by the Massachusetts Institute of Technology.
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
 *
 * Sample authorization data plugin
 */

#include <k5-int.h>
#include <krb5/kdcauthdata_plugin.h>

static krb5_error_code greet_hello(krb5_context context, krb5_data **ret)
{
    krb5_data tmp;

    tmp.data = "Hello, KDC issued acceptor world!";
    tmp.length = strlen(tmp.data);

    return krb5_copy_data(context, &tmp, ret);
}

static krb5_error_code
greet_kdc_sign(krb5_context context,
               krb5_enc_tkt_part *enc_tkt_reply,
               krb5_const_principal tgs,
               krb5_data *greeting)
{
    krb5_error_code code;
    krb5_authdata ad_datum, *ad_data[2], **kdc_issued = NULL;
    krb5_authdata **if_relevant = NULL;
    krb5_authdata **tkt_authdata;

    ad_datum.ad_type = -42;
    ad_datum.contents = (krb5_octet *)greeting->data;
    ad_datum.length = greeting->length;

    ad_data[0] = &ad_datum;
    ad_data[1] = NULL;

    code = krb5_make_authdata_kdc_issued(context,
                                         enc_tkt_reply->session,
                                         tgs,
                                         ad_data,
                                         &kdc_issued);
    if (code != 0)
        return code;

    code = krb5_encode_authdata_container(context,
                                          KRB5_AUTHDATA_IF_RELEVANT,
                                          kdc_issued,
                                          &if_relevant);
    if (code != 0) {
        krb5_free_authdata(context, kdc_issued);
        return code;
    }

    code = krb5_merge_authdata(context,
                               if_relevant,
                               enc_tkt_reply->authorization_data,
                               &tkt_authdata);
    if (code == 0) {
        krb5_free_authdata(context, enc_tkt_reply->authorization_data);
        enc_tkt_reply->authorization_data = tkt_authdata;
    } else {
        krb5_free_authdata(context, if_relevant);
    }

    krb5_free_authdata(context, kdc_issued);

    return code;
}

static krb5_error_code
greet_authdata(krb5_context context,
               krb5_kdcauthdata_moddata moddata,
               unsigned int flags,
               krb5_db_entry *client,
               krb5_db_entry *server,
               krb5_db_entry *tgs,
               krb5_keyblock *client_key,
               krb5_keyblock *server_key,
               krb5_keyblock *krbtgt_key,
               krb5_data *req_pkt,
               krb5_kdc_req *request,
               krb5_const_principal for_user_princ,
               krb5_enc_tkt_part *enc_tkt_request,
               krb5_enc_tkt_part *enc_tkt_reply)
{
    krb5_error_code code;
    krb5_data *greeting = NULL;

    if (request->msg_type != KRB5_TGS_REQ)
        return 0;

    code = greet_hello(context, &greeting);
    if (code != 0)
        return code;

    code = greet_kdc_sign(context, enc_tkt_reply, tgs->princ, greeting);

    krb5_free_data(context, greeting);

    return code;
}

krb5_error_code
kdcauthdata_greet_initvt(krb5_context context, int maj_ver, int min_ver,
                         krb5_plugin_vtable vtable);

krb5_error_code
kdcauthdata_greet_initvt(krb5_context context, int maj_ver, int min_ver,
                         krb5_plugin_vtable vtable)
{
    krb5_kdcauthdata_vtable vt = (krb5_kdcauthdata_vtable)vtable;

    vt->name = "greet";
    vt->handle = greet_authdata;
    return 0;
}
