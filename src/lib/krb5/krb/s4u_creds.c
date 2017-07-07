/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* lib/krb5/krb/s4u_creds.c */
/*
 * Copyright (C) 2009 by the Massachusetts Institute of Technology.
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

#include "k5-int.h"
#include "int-proto.h"

/* Convert ticket flags to necessary KDC options */
#define FLAGS2OPTS(flags) (flags & KDC_TKT_COMMON_MASK)

/*
 * Implements S4U2Self, by which a service can request a ticket to
 * itself on behalf of an arbitrary principal.
 */

static krb5_error_code
krb5_get_as_key_noop(
    krb5_context context,
    krb5_principal client,
    krb5_enctype etype,
    krb5_prompter_fct prompter,
    void *prompter_data,
    krb5_data *salt,
    krb5_data *params,
    krb5_keyblock *as_key,
    void *gak_data,
    k5_response_items *ritems)
{
    /* force a hard error, we don't actually have the key */
    return KRB5_PREAUTH_FAILED;
}

static krb5_error_code
s4u_identify_user(krb5_context context,
                  krb5_creds *in_creds,
                  krb5_data *subject_cert,
                  krb5_principal *canon_user)
{
    krb5_error_code code;
    krb5_preauthtype ptypes[1] = { KRB5_PADATA_S4U_X509_USER };
    krb5_creds creds;
    int use_master = 0;
    krb5_get_init_creds_opt *opts = NULL;
    krb5_principal_data client_data;
    krb5_principal client;
    krb5_s4u_userid userid;

    *canon_user = NULL;

    if (in_creds->client == NULL && subject_cert == NULL) {
        return EINVAL;
    }

    if (in_creds->client != NULL &&
        in_creds->client->type != KRB5_NT_ENTERPRISE_PRINCIPAL) {
        int anonymous;

        anonymous = krb5_principal_compare(context, in_creds->client,
                                           krb5_anonymous_principal());

        return krb5_copy_principal(context,
                                   anonymous ? in_creds->server
                                   : in_creds->client,
                                   canon_user);
    }

    memset(&creds, 0, sizeof(creds));

    memset(&userid, 0, sizeof(userid));
    if (subject_cert != NULL)
        userid.subject_cert = *subject_cert;

    code = krb5_get_init_creds_opt_alloc(context, &opts);
    if (code != 0)
        goto cleanup;
    krb5_get_init_creds_opt_set_tkt_life(opts, 15);
    krb5_get_init_creds_opt_set_renew_life(opts, 0);
    krb5_get_init_creds_opt_set_forwardable(opts, 0);
    krb5_get_init_creds_opt_set_proxiable(opts, 0);
    krb5_get_init_creds_opt_set_canonicalize(opts, 1);
    krb5_get_init_creds_opt_set_preauth_list(opts, ptypes, 1);

    if (in_creds->client != NULL)
        client = in_creds->client;
    else {
        client_data.magic = KV5M_PRINCIPAL;
        client_data.realm = in_creds->server->realm;
        /* should this be NULL, empty or a fixed string? XXX */
        client_data.data = NULL;
        client_data.length = 0;
        client_data.type = KRB5_NT_ENTERPRISE_PRINCIPAL;
        client = &client_data;
    }

    code = k5_get_init_creds(context, &creds, client, NULL, NULL, 0, NULL,
                             opts, krb5_get_as_key_noop, &userid, &use_master,
                             NULL);
    if (code == 0 || code == KRB5_PREAUTH_FAILED) {
        *canon_user = userid.user;
        userid.user = NULL;
        code = 0;
    }

cleanup:
    krb5_free_cred_contents(context, &creds);
    if (opts != NULL)
        krb5_get_init_creds_opt_free(context, opts);
    if (userid.user != NULL)
        krb5_free_principal(context, userid.user);

    return code;
}

static krb5_error_code
make_pa_for_user_checksum(krb5_context context,
                          krb5_keyblock *key,
                          krb5_pa_for_user *req,
                          krb5_checksum *cksum)
{
    krb5_error_code code;
    int i;
    char *p;
    krb5_data data;

    data.length = 4;
    for (i = 0; i < req->user->length; i++)
        data.length += req->user->data[i].length;
    data.length += req->user->realm.length;
    data.length += req->auth_package.length;

    p = data.data = malloc(data.length);
    if (data.data == NULL)
        return ENOMEM;

    p[0] = (req->user->type >> 0) & 0xFF;
    p[1] = (req->user->type >> 8) & 0xFF;
    p[2] = (req->user->type >> 16) & 0xFF;
    p[3] = (req->user->type >> 24) & 0xFF;
    p += 4;

    for (i = 0; i < req->user->length; i++) {
        if (req->user->data[i].length > 0)
            memcpy(p, req->user->data[i].data, req->user->data[i].length);
        p += req->user->data[i].length;
    }

    if (req->user->realm.length > 0)
        memcpy(p, req->user->realm.data, req->user->realm.length);
    p += req->user->realm.length;

    if (req->auth_package.length > 0)
        memcpy(p, req->auth_package.data, req->auth_package.length);

    /* Per spec, use hmac-md5 checksum regardless of key type. */
    code = krb5_c_make_checksum(context, CKSUMTYPE_HMAC_MD5_ARCFOUR, key,
                                KRB5_KEYUSAGE_APP_DATA_CKSUM, &data,
                                cksum);

    free(data.data);

    return code;
}

static krb5_error_code
build_pa_for_user(krb5_context context,
                  krb5_creds *tgt,
                  krb5_s4u_userid *userid,
                  krb5_pa_data **out_padata)
{
    krb5_error_code code;
    krb5_pa_data *padata;
    krb5_pa_for_user for_user;
    krb5_data *for_user_data = NULL;
    char package[] = "Kerberos";

    if (userid->user == NULL)
        return EINVAL;

    memset(&for_user, 0, sizeof(for_user));
    for_user.user = userid->user;
    for_user.auth_package.data = package;
    for_user.auth_package.length = sizeof(package) - 1;

    code = make_pa_for_user_checksum(context, &tgt->keyblock,
                                     &for_user, &for_user.cksum);
    if (code != 0)
        goto cleanup;

    code = encode_krb5_pa_for_user(&for_user, &for_user_data);
    if (code != 0)
        goto cleanup;

    padata = malloc(sizeof(*padata));
    if (padata == NULL) {
        code = ENOMEM;
        goto cleanup;
    }

    padata->magic = KV5M_PA_DATA;
    padata->pa_type = KRB5_PADATA_FOR_USER;
    padata->length = for_user_data->length;
    padata->contents = (krb5_octet *)for_user_data->data;

    free(for_user_data);
    for_user_data = NULL;

    *out_padata = padata;

cleanup:
    if (for_user.cksum.contents != NULL)
        krb5_free_checksum_contents(context, &for_user.cksum);
    krb5_free_data(context, for_user_data);

    return code;
}

/*
 * This function is invoked by krb5int_make_tgs_request_ext() just before the
 * request is encoded; it gives us access to the nonce and subkey without
 * requiring them to be generated by the caller.
 */
static krb5_error_code
build_pa_s4u_x509_user(krb5_context context,
                       krb5_keyblock *subkey,
                       krb5_kdc_req *tgsreq,
                       void *gcvt_data)
{
    krb5_error_code code;
    krb5_pa_s4u_x509_user *s4u_user = (krb5_pa_s4u_x509_user *)gcvt_data;
    krb5_data *data = NULL;
    krb5_pa_data **padata;
    krb5_cksumtype cksumtype;
    int i;

    assert(s4u_user->cksum.contents == NULL);

    s4u_user->user_id.nonce = tgsreq->nonce;

    code = encode_krb5_s4u_userid(&s4u_user->user_id, &data);
    if (code != 0)
        goto cleanup;

    /* [MS-SFU] 2.2.2: unusual to say the least, but enc_padata secures it */
    if (subkey->enctype == ENCTYPE_ARCFOUR_HMAC ||
        subkey->enctype == ENCTYPE_ARCFOUR_HMAC_EXP) {
        cksumtype = CKSUMTYPE_RSA_MD4;
    } else {
        code = krb5int_c_mandatory_cksumtype(context, subkey->enctype,
                                             &cksumtype);
    }
    if (code != 0)
        goto cleanup;

    code = krb5_c_make_checksum(context, cksumtype, subkey,
                                KRB5_KEYUSAGE_PA_S4U_X509_USER_REQUEST, data,
                                &s4u_user->cksum);
    if (code != 0)
        goto cleanup;

    krb5_free_data(context, data);
    data = NULL;

    code = encode_krb5_pa_s4u_x509_user(s4u_user, &data);
    if (code != 0)
        goto cleanup;

    assert(tgsreq->padata != NULL);

    for (i = 0; tgsreq->padata[i] != NULL; i++)
        ;

    padata = realloc(tgsreq->padata,
                     (i + 2) * sizeof(krb5_pa_data *));
    if (padata == NULL) {
        code = ENOMEM;
        goto cleanup;
    }
    tgsreq->padata = padata;

    padata[i] = malloc(sizeof(krb5_pa_data));
    if (padata[i] == NULL) {
        code = ENOMEM;
        goto cleanup;
    }
    padata[i]->magic = KV5M_PA_DATA;
    padata[i]->pa_type = KRB5_PADATA_S4U_X509_USER;
    padata[i]->length = data->length;
    padata[i]->contents = (krb5_octet *)data->data;

    padata[i + 1] = NULL;

    free(data);
    data = NULL;

cleanup:
    if (code != 0 && s4u_user->cksum.contents != NULL) {
        krb5_free_checksum_contents(context, &s4u_user->cksum);
        s4u_user->cksum.contents = NULL;
    }
    krb5_free_data(context, data);

    return code;
}

static krb5_error_code
verify_s4u2self_reply(krb5_context context,
                      krb5_keyblock *subkey,
                      krb5_pa_s4u_x509_user *req_s4u_user,
                      krb5_pa_data **rep_padata,
                      krb5_pa_data **enc_padata)
{
    krb5_error_code code;
    krb5_pa_data *rep_s4u_padata, *enc_s4u_padata;
    krb5_pa_s4u_x509_user *rep_s4u_user = NULL;
    krb5_data data, *datap = NULL;
    krb5_keyusage usage;
    krb5_boolean valid;
    krb5_boolean not_newer;

    assert(req_s4u_user != NULL);

    switch (subkey->enctype) {
    case ENCTYPE_DES_CBC_CRC:
    case ENCTYPE_DES_CBC_MD4:
    case ENCTYPE_DES_CBC_MD5:
    case ENCTYPE_DES3_CBC_SHA1:
    case ENCTYPE_DES3_CBC_RAW:
    case ENCTYPE_ARCFOUR_HMAC:
    case ENCTYPE_ARCFOUR_HMAC_EXP :
        not_newer = TRUE;
        break;
    default:
        not_newer = FALSE;
        break;
    }

    enc_s4u_padata = krb5int_find_pa_data(context,
                                          enc_padata,
                                          KRB5_PADATA_S4U_X509_USER);

    /* XXX this will break newer enctypes with a MIT 1.7 KDC */
    rep_s4u_padata = krb5int_find_pa_data(context,
                                          rep_padata,
                                          KRB5_PADATA_S4U_X509_USER);
    if (rep_s4u_padata == NULL) {
        if (not_newer == FALSE || enc_s4u_padata != NULL)
            return KRB5_KDCREP_MODIFIED;
        else
            return 0;
    }

    data.length = rep_s4u_padata->length;
    data.data = (char *)rep_s4u_padata->contents;

    code = decode_krb5_pa_s4u_x509_user(&data, &rep_s4u_user);
    if (code != 0)
        goto cleanup;

    if (rep_s4u_user->user_id.nonce != req_s4u_user->user_id.nonce) {
        code = KRB5_KDCREP_MODIFIED;
        goto cleanup;
    }

    code = encode_krb5_s4u_userid(&rep_s4u_user->user_id, &datap);
    if (code != 0)
        goto cleanup;

    if (rep_s4u_user->user_id.options & KRB5_S4U_OPTS_USE_REPLY_KEY_USAGE)
        usage = KRB5_KEYUSAGE_PA_S4U_X509_USER_REPLY;
    else
        usage = KRB5_KEYUSAGE_PA_S4U_X509_USER_REQUEST;

    code = krb5_c_verify_checksum(context, subkey, usage, datap,
                                  &rep_s4u_user->cksum, &valid);
    if (code != 0)
        goto cleanup;
    if (valid == FALSE) {
        code = KRB5_KDCREP_MODIFIED;
        goto cleanup;
    }

    /*
     * KDCs that support KRB5_S4U_OPTS_USE_REPLY_KEY_USAGE also return
     * S4U enc_padata for older (pre-AES) encryption types only.
     */
    if (not_newer) {
        if (enc_s4u_padata == NULL) {
            if (rep_s4u_user->user_id.options &
                KRB5_S4U_OPTS_USE_REPLY_KEY_USAGE) {
                code = KRB5_KDCREP_MODIFIED;
                goto cleanup;
            }
        } else {
            if (enc_s4u_padata->length !=
                req_s4u_user->cksum.length + rep_s4u_user->cksum.length) {
                code = KRB5_KDCREP_MODIFIED;
                goto cleanup;
            }
            if (memcmp(enc_s4u_padata->contents,
                       req_s4u_user->cksum.contents,
                       req_s4u_user->cksum.length) ||
                memcmp(&enc_s4u_padata->contents[req_s4u_user->cksum.length],
                       rep_s4u_user->cksum.contents,
                       rep_s4u_user->cksum.length)) {
                code = KRB5_KDCREP_MODIFIED;
                goto cleanup;
            }
        }
    } else if (!krb5_c_is_keyed_cksum(rep_s4u_user->cksum.checksum_type)) {
        code = KRB5KRB_AP_ERR_INAPP_CKSUM;
        goto cleanup;
    }

cleanup:
    krb5_free_pa_s4u_x509_user(context, rep_s4u_user);
    krb5_free_data(context, datap);

    return code;
}

/* Unparse princ and re-parse it as an enterprise principal. */
static krb5_error_code
convert_to_enterprise(krb5_context context, krb5_principal princ,
                      krb5_principal *eprinc_out)
{
    krb5_error_code code;
    char *str;

    *eprinc_out = NULL;
    code = krb5_unparse_name(context, princ, &str);
    if (code != 0)
        return code;
    code = krb5_parse_name_flags(context, str, KRB5_PRINCIPAL_PARSE_ENTERPRISE,
                                 eprinc_out);
    krb5_free_unparsed_name(context, str);
    return code;
}

static krb5_error_code
krb5_get_self_cred_from_kdc(krb5_context context,
                            krb5_flags options,
                            krb5_ccache ccache,
                            krb5_creds *in_creds,
                            krb5_data *subject_cert,
                            krb5_data *user_realm,
                            krb5_creds **out_creds)
{
    krb5_error_code code;
    krb5_principal tgs = NULL, eprinc = NULL;
    krb5_principal_data sprinc;
    krb5_creds tgtq, s4u_creds, *tgt = NULL, *tgtptr;
    krb5_creds *referral_tgts[KRB5_REFERRAL_MAXHOPS];
    krb5_pa_s4u_x509_user s4u_user;
    int referral_count = 0, i;
    krb5_flags kdcopt;

    memset(&tgtq, 0, sizeof(tgtq));
    memset(referral_tgts, 0, sizeof(referral_tgts));
    *out_creds = NULL;

    memset(&s4u_user, 0, sizeof(s4u_user));

    if (in_creds->client != NULL && in_creds->client->length > 0) {
        if (in_creds->client->type == KRB5_NT_ENTERPRISE_PRINCIPAL) {
            code = krb5_build_principal_ext(context,
                                            &s4u_user.user_id.user,
                                            user_realm->length,
                                            user_realm->data,
                                            in_creds->client->data[0].length,
                                            in_creds->client->data[0].data,
                                            0);
            if (code != 0)
                goto cleanup;
            s4u_user.user_id.user->type = KRB5_NT_ENTERPRISE_PRINCIPAL;
        } else {
            code = krb5_copy_principal(context,
                                       in_creds->client,
                                       &s4u_user.user_id.user);
            if (code != 0)
                goto cleanup;
        }
    } else {
        code = krb5_build_principal_ext(context, &s4u_user.user_id.user,
                                        user_realm->length,
                                        user_realm->data);
        if (code != 0)
            goto cleanup;
        s4u_user.user_id.user->type = KRB5_NT_ENTERPRISE_PRINCIPAL;
    }
    if (subject_cert != NULL)
        s4u_user.user_id.subject_cert = *subject_cert;
    s4u_user.user_id.options = KRB5_S4U_OPTS_USE_REPLY_KEY_USAGE;

    /* First, acquire a TGT to the user's realm. */
    code = krb5int_tgtname(context, user_realm, &in_creds->server->realm,
                           &tgs);
    if (code != 0)
        goto cleanup;

    tgtq.client = in_creds->server;
    tgtq.server = tgs;

    code = krb5_get_credentials(context, options, ccache, &tgtq, &tgt);
    if (code != 0)
        goto cleanup;

    tgtptr = tgt;

    /* Convert the server principal to an enterprise principal, for use with
     * foreign realms. */
    code = convert_to_enterprise(context, in_creds->server, &eprinc);
    if (code != 0)
        goto cleanup;

    /* Make a shallow copy of in_creds with client pointing to the server
     * principal.  We will set s4u_creds.server for each request. */
    s4u_creds = *in_creds;
    s4u_creds.client = in_creds->server;

    /* Then, walk back the referral path to S4U2Self for user */
    kdcopt = 0;
    if (options & KRB5_GC_CANONICALIZE)
        kdcopt |= KDC_OPT_CANONICALIZE;
    if (options & KRB5_GC_FORWARDABLE)
        kdcopt |= KDC_OPT_FORWARDABLE;
    if (options & KRB5_GC_NO_TRANSIT_CHECK)
        kdcopt |= KDC_OPT_DISABLE_TRANSITED_CHECK;

    for (referral_count = 0;
         referral_count < KRB5_REFERRAL_MAXHOPS;
         referral_count++)
    {
        krb5_pa_data **in_padata = NULL;
        krb5_pa_data **out_padata = NULL;
        krb5_pa_data **enc_padata = NULL;
        krb5_keyblock *subkey = NULL;

        if (s4u_user.user_id.user != NULL && s4u_user.user_id.user->length) {
            in_padata = calloc(2, sizeof(krb5_pa_data *));
            if (in_padata == NULL) {
                code = ENOMEM;
                goto cleanup;
            }
            code = build_pa_for_user(context,
                                     tgtptr,
                                     &s4u_user.user_id, &in_padata[0]);
            if (code != 0) {
                krb5_free_pa_data(context, in_padata);
                goto cleanup;
            }
        }

        if (data_eq(tgtptr->server->data[1], in_creds->server->realm)) {
            /* When asking the server realm, use the real principal. */
            s4u_creds.server = in_creds->server;
        } else {
            /* When asking a foreign realm, use the enterprise principal, with
             * the realm set to the TGS realm. */
            sprinc = *eprinc;
            sprinc.realm = tgtptr->server->data[1];
            s4u_creds.server = &sprinc;
        }

        code = krb5_get_cred_via_tkt_ext(context, tgtptr,
                                         KDC_OPT_CANONICALIZE |
                                         FLAGS2OPTS(tgtptr->ticket_flags) |
                                         kdcopt,
                                         tgtptr->addresses,
                                         in_padata, &s4u_creds,
                                         build_pa_s4u_x509_user, &s4u_user,
                                         &out_padata, &enc_padata,
                                         out_creds, &subkey);
        if (code != 0) {
            krb5_free_checksum_contents(context, &s4u_user.cksum);
            krb5_free_pa_data(context, in_padata);
            goto cleanup;
        }

        code = verify_s4u2self_reply(context, subkey, &s4u_user,
                                     out_padata, enc_padata);

        krb5_free_checksum_contents(context, &s4u_user.cksum);
        krb5_free_pa_data(context, in_padata);
        krb5_free_pa_data(context, out_padata);
        krb5_free_pa_data(context, enc_padata);
        krb5_free_keyblock(context, subkey);

        if (code != 0)
            goto cleanup;

        if (krb5_principal_compare(context,
                                   in_creds->server,
                                   (*out_creds)->server)) {
            code = 0;
            goto cleanup;
        } else if (IS_TGS_PRINC((*out_creds)->server)) {
            krb5_data *r1 = &tgtptr->server->data[1];
            krb5_data *r2 = &(*out_creds)->server->data[1];

            if (data_eq(*r1, *r2)) {
                krb5_free_creds(context, *out_creds);
                *out_creds = NULL;
                code = KRB5_ERR_HOST_REALM_UNKNOWN;
                break;
            }
            for (i = 0; i < referral_count; i++) {
                if (krb5_principal_compare(context,
                                           (*out_creds)->server,
                                           referral_tgts[i]->server)) {
                    code = KRB5_KDC_UNREACH;
                    goto cleanup;
                }
            }

            tgtptr = *out_creds;
            referral_tgts[referral_count] = *out_creds;
            *out_creds = NULL;
        } else {
            krb5_free_creds(context, *out_creds);
            *out_creds = NULL;
            code = KRB5KRB_AP_WRONG_PRINC; /* XXX */
            break;
        }
    }

cleanup:
    for (i = 0; i < KRB5_REFERRAL_MAXHOPS; i++) {
        if (referral_tgts[i] != NULL)
            krb5_free_creds(context, referral_tgts[i]);
    }
    krb5_free_principal(context, tgs);
    krb5_free_principal(context, eprinc);
    krb5_free_creds(context, tgt);
    krb5_free_principal(context, s4u_user.user_id.user);
    krb5_free_checksum_contents(context, &s4u_user.cksum);

    return code;
}

krb5_error_code KRB5_CALLCONV
krb5_get_credentials_for_user(krb5_context context, krb5_flags options,
                              krb5_ccache ccache, krb5_creds *in_creds,
                              krb5_data *subject_cert,
                              krb5_creds **out_creds)
{
    krb5_error_code code;
    krb5_principal realm = NULL;

    *out_creds = NULL;

    if (options & KRB5_GC_CONSTRAINED_DELEGATION) {
        code = EINVAL;
        goto cleanup;
    }

    if (in_creds->client != NULL) {
        /* Uncanonicalised check */
        code = krb5_get_credentials(context, options | KRB5_GC_CACHED,
                                    ccache, in_creds, out_creds);
        if (code != KRB5_CC_NOTFOUND && code != KRB5_CC_NOT_KTYPE)
            goto cleanup;

        if ((options & KRB5_GC_CACHED) && !(options & KRB5_GC_CANONICALIZE))
            goto cleanup;
    }

    code = s4u_identify_user(context, in_creds, subject_cert, &realm);
    if (code != 0)
        goto cleanup;

    if (in_creds->client != NULL &&
        in_creds->client->type == KRB5_NT_ENTERPRISE_PRINCIPAL) {
        /* Post-canonicalisation check for enterprise principals */
        krb5_creds mcreds = *in_creds;
        mcreds.client = realm;
        code = krb5_get_credentials(context, options | KRB5_GC_CACHED,
                                    ccache, &mcreds, out_creds);
        if ((code != KRB5_CC_NOTFOUND && code != KRB5_CC_NOT_KTYPE)
            || (options & KRB5_GC_CACHED))
            goto cleanup;
    }

    code = krb5_get_self_cred_from_kdc(context, options, ccache, in_creds,
                                       subject_cert, &realm->realm, out_creds);
    if (code != 0)
        goto cleanup;

    assert(*out_creds != NULL);

    if ((options & KRB5_GC_NO_STORE) == 0) {
        code = krb5_cc_store_cred(context, ccache, *out_creds);
        if (code != 0)
            goto cleanup;
    }

cleanup:
    if (code != 0 && *out_creds != NULL) {
        krb5_free_creds(context, *out_creds);
        *out_creds = NULL;
    }

    krb5_free_principal(context, realm);

    return code;
}

/*
 * Exported API for constrained delegation (S4U2Proxy).
 *
 * This is preferable to using krb5_get_credentials directly because
 * it can perform some additional checks.
 */
krb5_error_code KRB5_CALLCONV
krb5_get_credentials_for_proxy(krb5_context context,
                               krb5_flags options,
                               krb5_ccache ccache,
                               krb5_creds *in_creds,
                               krb5_ticket *evidence_tkt,
                               krb5_creds **out_creds)
{
    krb5_error_code code;
    krb5_creds mcreds;
    krb5_creds *ncreds = NULL;
    krb5_flags fields;
    krb5_data *evidence_tkt_data = NULL;
    krb5_creds s4u_creds;

    *out_creds = NULL;

    if (in_creds == NULL || in_creds->client == NULL ||
        evidence_tkt == NULL || evidence_tkt->enc_part2 == NULL) {
        code = EINVAL;
        goto cleanup;
    }

    /*
     * Caller should have set in_creds->client to match evidence
     * ticket client
     */
    if (!krb5_principal_compare(context, evidence_tkt->enc_part2->client,
                                in_creds->client)) {
        code = EINVAL;
        goto cleanup;
    }

    if ((evidence_tkt->enc_part2->flags & TKT_FLG_FORWARDABLE) == 0) {
        code = KRB5_TKT_NOT_FORWARDABLE;
        goto cleanup;
    }

    code = krb5int_construct_matching_creds(context, options, in_creds,
                                            &mcreds, &fields);
    if (code != 0)
        goto cleanup;

    ncreds = calloc(1, sizeof(*ncreds));
    if (ncreds == NULL) {
        code = ENOMEM;
        goto cleanup;
    }
    ncreds->magic = KV5M_CRED;

    code = krb5_cc_retrieve_cred(context, ccache, fields, &mcreds, ncreds);
    if (code != 0) {
        free(ncreds);
        ncreds = in_creds;
    } else {
        *out_creds = ncreds;
    }

    if ((code != KRB5_CC_NOTFOUND && code != KRB5_CC_NOT_KTYPE)
        || options & KRB5_GC_CACHED)
        goto cleanup;

    code = encode_krb5_ticket(evidence_tkt, &evidence_tkt_data);
    if (code != 0)
        goto cleanup;

    s4u_creds = *in_creds;
    s4u_creds.client = evidence_tkt->server;
    s4u_creds.second_ticket = *evidence_tkt_data;

    code = krb5_get_credentials(context,
                                options | KRB5_GC_CONSTRAINED_DELEGATION,
                                ccache,
                                &s4u_creds,
                                out_creds);
    if (code != 0)
        goto cleanup;

    /*
     * Check client name because we couldn't compare that inside
     * krb5_get_credentials() (enc_part2 is unavailable in clear)
     */
    if (!krb5_principal_compare(context,
                                evidence_tkt->enc_part2->client,
                                (*out_creds)->client)) {
        code = KRB5_KDCREP_MODIFIED;
        goto cleanup;
    }

cleanup:
    if (*out_creds != NULL && code != 0) {
        krb5_free_creds(context, *out_creds);
        *out_creds = NULL;
    }
    if (evidence_tkt_data != NULL)
        krb5_free_data(context, evidence_tkt_data);

    return code;
}
