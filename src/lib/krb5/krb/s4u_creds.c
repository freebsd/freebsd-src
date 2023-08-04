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
#include "os-proto.h"

/* Convert ticket flags to necessary KDC options */
#define FLAGS2OPTS(flags) (flags & KDC_TKT_COMMON_MASK)

/*
 * Implements S4U2Self, by which a service can request a ticket to
 * itself on behalf of an arbitrary principal.
 */

static krb5_error_code
s4u_identify_user(krb5_context context,
                  krb5_creds *in_creds,
                  krb5_data *subject_cert,
                  krb5_principal *canon_user)
{
    krb5_principal_data client;
    krb5_data empty_name = empty_data();

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

    if (in_creds->client != NULL) {
        client = *in_creds->client;
        client.realm = in_creds->server->realm;

        /* Don't send subject_cert if we have an enterprise principal. */
        return k5_identify_realm(context, &client, NULL, canon_user);
    }

    client.magic = KV5M_PRINCIPAL;
    client.realm = in_creds->server->realm;

    /*
     * Windows clients send the certificate subject as the client name.
     * However, Windows KDC seem to be happy with an empty string as long as
     * the name-type is NT-X500-PRINCIPAL.
     */
    client.data = &empty_name;
    client.length = 1;
    client.type = KRB5_NT_X500_PRINCIPAL;

    return k5_identify_realm(context, &client, subject_cert, canon_user);
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

    /* Find the empty PA-S4U-X509-USER element placed in the TGS request padata
     * by krb5_get_self_cred_from_kdc() and replace it with the encoding. */
    assert(tgsreq->padata != NULL);
    for (i = 0; tgsreq->padata[i] != NULL; i++) {
        if (tgsreq->padata[i]->pa_type == KRB5_PADATA_S4U_X509_USER)
            break;
    }
    assert(tgsreq->padata[i] != NULL);
    free(tgsreq->padata[i]->contents);
    tgsreq->padata[i]->length = data->length;
    tgsreq->padata[i]->contents = (krb5_octet *)data->data;
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

/*
 * Validate the S4U2Self padata in the KDC reply.  If update_req_user is true
 * and the KDC sent S4U-X509-USER padata, replace req_s4u_user->user_id.user
 * with the checksum-protected client name from the KDC.  If update_req_user is
 * false, verify that the client name has not changed.
 */
static krb5_error_code
verify_s4u2self_reply(krb5_context context,
                      krb5_keyblock *subkey,
                      krb5_pa_s4u_x509_user *req_s4u_user,
                      krb5_pa_data **rep_padata,
                      krb5_pa_data **enc_padata,
                      krb5_boolean update_req_user)
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

    rep_s4u_padata = krb5int_find_pa_data(context,
                                          rep_padata,
                                          KRB5_PADATA_S4U_X509_USER);
    if (rep_s4u_padata == NULL)
        return (enc_s4u_padata != NULL) ? KRB5_KDCREP_MODIFIED : 0;

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

    if (rep_s4u_user->user_id.user == NULL ||
        rep_s4u_user->user_id.user->length == 0) {
        code = KRB5_KDCREP_MODIFIED;
        goto cleanup;
    }

    if (update_req_user) {
        krb5_free_principal(context, req_s4u_user->user_id.user);
        code = krb5_copy_principal(context, rep_s4u_user->user_id.user,
                                   &req_s4u_user->user_id.user);
        if (code != 0)
            goto cleanup;
    } else if (!krb5_principal_compare(context, rep_s4u_user->user_id.user,
                                       req_s4u_user->user_id.user)) {
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
    code = krb5_parse_name_flags(context, str,
                                 KRB5_PRINCIPAL_PARSE_ENTERPRISE |
                                 KRB5_PRINCIPAL_PARSE_IGNORE_REALM,
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
                                        user_realm->length, user_realm->data,
                                        0);
        if (code != 0)
            goto cleanup;
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

        in_padata = k5calloc(3, sizeof(krb5_pa_data *), &code);
        if (in_padata == NULL)
            goto cleanup;

        in_padata[0] = k5alloc(sizeof(krb5_pa_data), &code);
        if (in_padata[0] == NULL) {
            krb5_free_pa_data(context, in_padata);
            goto cleanup;
        }

        in_padata[0]->magic = KV5M_PA_DATA;
        in_padata[0]->pa_type = KRB5_PADATA_S4U_X509_USER;
        in_padata[0]->length = 0;
        in_padata[0]->contents = NULL;

        if (s4u_user.user_id.user != NULL && s4u_user.user_id.user->length) {
            code = build_pa_for_user(context, tgtptr, &s4u_user.user_id,
                                     &in_padata[1]);
            /*
             * If we couldn't compute the hmac-md5 checksum, send only the
             * KRB5_PADATA_S4U_X509_USER; this will still work against modern
             * Windows and MIT KDCs.
             */
            if (code == KRB5_CRYPTO_INTERNAL)
                code = 0;
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

        /* Update s4u_user.user_id.user if this is the initial request to the
         * client realm; otherwise verify that it doesn't change. */
        code = verify_s4u2self_reply(context, subkey, &s4u_user, out_padata,
                                     enc_padata, referral_count == 0);

        krb5_free_checksum_contents(context, &s4u_user.cksum);
        krb5_free_pa_data(context, in_padata);
        krb5_free_pa_data(context, out_padata);
        krb5_free_pa_data(context, enc_padata);
        krb5_free_keyblock(context, subkey);

        if (code != 0)
            goto cleanup;

        /* The authdata in this referral TGT will be copied into the final
         * credentials, so we don't need to request it again. */
        s4u_creds.authdata = NULL;

        /* Only include a cert in the initial request to the client realm. */
        s4u_user.user_id.subject_cert = empty_data();

        if (krb5_principal_compare_any_realm(context, in_creds->server,
                                             (*out_creds)->server)) {
            /* Verify that the unprotected client name in the reply matches the
             * checksum-protected one from the client realm's KDC padata. */
            if (!krb5_principal_compare(context, (*out_creds)->client,
                                        s4u_user.user_id.user))
                code = KRB5_KDCREP_MODIFIED;
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
        if ((code != KRB5_CC_NOTFOUND && code != KRB5_CC_NOT_KTYPE) ||
            (options & KRB5_GC_CACHED))
            goto cleanup;
    } else if (options & KRB5_GC_CACHED) {
        /* Fail immediately, since we can't check the cache by certificate. */
        code = KRB5_CC_NOTFOUND;
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
        if (code != KRB5_CC_NOTFOUND && code != KRB5_CC_NOT_KTYPE)
            goto cleanup;
    }

    code = krb5_get_self_cred_from_kdc(context, options, ccache, in_creds,
                                       subject_cert, &realm->realm, out_creds);
    if (code != 0)
        goto cleanup;

    assert(*out_creds != NULL);

    /* If we canonicalized the client name or discovered it using subject_cert,
     * check if we had cached credentials and return them if found. */
    if (in_creds->client == NULL ||
        !krb5_principal_compare(context, in_creds->client,
                                (*out_creds)->client)) {
        krb5_creds *old_creds;
        krb5_creds mcreds = *in_creds;
        mcreds.client = (*out_creds)->client;
        code = krb5_get_credentials(context, options | KRB5_GC_CACHED, ccache,
                                    &mcreds, &old_creds);
        if (code == 0) {
            krb5_free_creds(context, *out_creds);
            *out_creds = old_creds;
            options |= KRB5_GC_NO_STORE;
        } else if (code != KRB5_CC_NOTFOUND && code != KRB5_CC_NOT_KTYPE) {
            goto cleanup;
        }
    }

    /* Note the authdata we asked for in the output creds. */
    code = krb5_copy_authdata(context, in_creds->authdata,
                              &(*out_creds)->authdata);
    if (code)
        goto cleanup;

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

static krb5_error_code
check_rbcd_support(krb5_context context, krb5_pa_data **padata)
{
    krb5_error_code code;
    krb5_pa_data *pa;
    krb5_pa_pac_options *pac_options;
    krb5_data der_pac_options;

    pa = krb5int_find_pa_data(context, padata, KRB5_PADATA_PAC_OPTIONS);
    if (pa == NULL)
        return KRB5KDC_ERR_PADATA_TYPE_NOSUPP;

    der_pac_options = make_data(pa->contents, pa->length);
    code = decode_krb5_pa_pac_options(&der_pac_options, &pac_options);
    if (code)
        return code;

    if (!(pac_options->options & KRB5_PA_PAC_OPTIONS_RBCD))
        code = KRB5KDC_ERR_PADATA_TYPE_NOSUPP;

    free(pac_options);
    return code;
}

static krb5_error_code
add_rbcd_padata(krb5_context context, krb5_pa_data ***in_padata)
{
    krb5_error_code code;
    krb5_pa_pac_options pac_options;
    krb5_data *der_pac_options = NULL;

    memset(&pac_options, 0, sizeof(pac_options));
    pac_options.options |= KRB5_PA_PAC_OPTIONS_RBCD;

    code = encode_krb5_pa_pac_options(&pac_options, &der_pac_options);
    if (code)
        return code;

    code = k5_add_pa_data_from_data(in_padata, KRB5_PADATA_PAC_OPTIONS,
                                    der_pac_options);
    krb5_free_data(context, der_pac_options);
    return code;
}

/* Set *tgt_out to a local TGT for the client realm retrieved from ccache. */
static krb5_error_code
get_client_tgt(krb5_context context, krb5_flags options, krb5_ccache ccache,
               krb5_principal client, krb5_creds **tgt_out)
{
    krb5_error_code code;
    krb5_principal tgs;
    krb5_creds mcreds;

    *tgt_out = NULL;

    code = krb5int_tgtname(context, &client->realm, &client->realm, &tgs);
    if (code)
        return code;

    memset(&mcreds, 0, sizeof(mcreds));
    mcreds.client = client;
    mcreds.server = tgs;
    code = krb5_get_credentials(context, options, ccache, &mcreds, tgt_out);
    krb5_free_principal(context, tgs);
    return code;
}

/*
 * Copy req_server to *out_server.  If req_server has the referral realm, set
 * the realm of *out_server to realm.  Otherwise the S4U2Proxy request will
 * fail unless the specified realm is the same as the TGT (or an alias to it).
 */
static krb5_error_code
normalize_server_princ(krb5_context context, const krb5_data *realm,
                       krb5_principal req_server, krb5_principal *out_server)
{
    krb5_error_code code;
    krb5_principal server;

    *out_server = NULL;

    code = krb5_copy_principal(context, req_server, &server);
    if (code)
        return code;

    if (krb5_is_referral_realm(&server->realm)) {
        krb5_free_data_contents(context, &server->realm);
        code = krb5int_copy_data_contents(context, realm, &server->realm);
        if (code) {
            krb5_free_principal(context, server);
            return code;
        }
    }

    *out_server = server;
    return 0;
}

/* Return an error if server is present in referral_list. */
static krb5_error_code
check_referral_path(krb5_context context, krb5_principal server,
                    krb5_creds **referral_list, int referral_count)
{
    int i;

    for (i = 0; i < referral_count; i++) {
        if (krb5_principal_compare(context, server, referral_list[i]->server))
            return KRB5_KDC_UNREACH;
    }
    return 0;
}

/*
 * Make TGS requests for in_creds using *tgt_inout, following referrals until
 * the requested service ticket is issued.  Replace *tgt_inout with the final
 * TGT used, or free it and set it to NULL on error.  Place the final creds
 * received in *creds_out.
 */
static krb5_error_code
chase_referrals(krb5_context context, krb5_creds *in_creds, krb5_flags kdcopt,
                krb5_creds **tgt_inout, krb5_creds **creds_out)
{
    krb5_error_code code;
    krb5_creds *referral_tgts[KRB5_REFERRAL_MAXHOPS] = { NULL };
    krb5_creds mcreds, *tgt, *tkt = NULL;
    krb5_principal_data server;
    int referral_count = 0, i;

    tgt = *tgt_inout;
    *tgt_inout = NULL;
    *creds_out = NULL;

    mcreds = *in_creds;
    server = *in_creds->server;
    mcreds.server = &server;

    for (referral_count = 0; referral_count < KRB5_REFERRAL_MAXHOPS;
         referral_count++) {
        code = krb5_get_cred_via_tkt(context, tgt, kdcopt, tgt->addresses,
                                     &mcreds, &tkt);
        if (code)
            goto cleanup;

        if (krb5_principal_compare_any_realm(context, mcreds.server,
                                             tkt->server)) {
            *creds_out = tkt;
            *tgt_inout = tgt;
            tkt = tgt = NULL;
            goto cleanup;
        }

        if (!IS_TGS_PRINC(tkt->server)) {
            code = KRB5KRB_AP_WRONG_PRINC;
            goto cleanup;
        }

        if (data_eq(tgt->server->data[1], tkt->server->data[1])) {
            code = KRB5_ERR_HOST_REALM_UNKNOWN;
            goto cleanup;
        }

        code = check_referral_path(context, tkt->server, referral_tgts,
                                   referral_count);
        if (code)
            goto cleanup;

        referral_tgts[referral_count] = tgt;
        tgt = tkt;
        tkt = NULL;
        server.realm = tgt->server->data[1];
    }

    /* Max hop count exceeded. */
    code = KRB5_KDCREP_MODIFIED;

cleanup:
    for (i = 0; i < KRB5_REFERRAL_MAXHOPS; i++)
        krb5_free_creds(context, referral_tgts[i]);
    krb5_free_creds(context, tkt);
    krb5_free_creds(context, tgt);
    return code;
}

/*
 * Make non-S4U2Proxy TGS requests for in_creds using *tgt_inout, following
 * referrals until the requested service ticket is returned.  Discard the
 * service ticket, but replace *tgt_inout with the final referral TGT.
 */
static krb5_error_code
get_tgt_to_target_realm(krb5_context context, krb5_creds *in_creds,
                        krb5_flags req_kdcopt, krb5_creds **tgt_inout)
{
    krb5_error_code code;
    krb5_flags kdcopt;
    krb5_creds mcreds, *out;

    mcreds = *in_creds;
    mcreds.second_ticket = empty_data();
    kdcopt = FLAGS2OPTS((*tgt_inout)->ticket_flags) | req_kdcopt;

    code = chase_referrals(context, &mcreds, kdcopt, tgt_inout, &out);
    krb5_free_creds(context, out);

    return code;
}

/*
 * Make TGS requests for a cross-TGT to realm using *tgt_inout, following
 * alternate TGS replies until the requested TGT is issued.  Replace *tgt_inout
 * with the result.  Do nothing if *tgt_inout is already a cross-TGT for realm.
 */
static krb5_error_code
get_target_realm_proxy_tgt(krb5_context context, const krb5_data *realm,
                           krb5_flags req_kdcopt, krb5_creds **tgt_inout)
{
    krb5_error_code code;
    krb5_creds mcreds, *out;
    krb5_principal tgs;
    krb5_flags flags;

    if (data_eq(*realm, (*tgt_inout)->server->data[1]))
        return 0;

    code = krb5int_tgtname(context, realm, &(*tgt_inout)->server->data[1],
                           &tgs);
    if (code)
        return code;

    memset(&mcreds, 0, sizeof(mcreds));
    mcreds.client = (*tgt_inout)->client;
    mcreds.server = tgs;
    flags = req_kdcopt | FLAGS2OPTS((*tgt_inout)->ticket_flags);

    code = chase_referrals(context, &mcreds, flags, tgt_inout, &out);
    krb5_free_principal(context, tgs);
    if (code)
        return code;

    krb5_free_creds(context, *tgt_inout);
    *tgt_inout = out;

    return 0;
}

static krb5_error_code
get_proxy_cred_from_kdc(krb5_context context, krb5_flags options,
                        krb5_ccache ccache, krb5_creds *in_creds,
                        krb5_creds **out_creds)
{
    krb5_error_code code;
    krb5_flags flags, req_kdcopt = 0;
    krb5_principal server = NULL;
    krb5_pa_data **in_padata = NULL;
    krb5_pa_data **enc_padata = NULL;
    krb5_creds mcreds, *tgt = NULL, *tkt = NULL;

    *out_creds = NULL;

    if (in_creds->second_ticket.length == 0 ||
        (options & KRB5_GC_CONSTRAINED_DELEGATION) == 0)
        return EINVAL;

    options &= ~KRB5_GC_CONSTRAINED_DELEGATION;

    code = get_client_tgt(context, options, ccache, in_creds->client, &tgt);
    if (code)
        goto cleanup;

    code = normalize_server_princ(context, &in_creds->client->realm,
                                  in_creds->server, &server);
    if (code)
        goto cleanup;

    code = add_rbcd_padata(context, &in_padata);
    if (code)
        goto cleanup;

    if (options & KRB5_GC_CANONICALIZE)
        req_kdcopt |= KDC_OPT_CANONICALIZE;
    if (options & KRB5_GC_FORWARDABLE)
        req_kdcopt |= KDC_OPT_FORWARDABLE;
    if (options & KRB5_GC_NO_TRANSIT_CHECK)
        req_kdcopt |= KDC_OPT_DISABLE_TRANSITED_CHECK;

    mcreds = *in_creds;
    mcreds.server = server;

    flags = req_kdcopt | FLAGS2OPTS(tgt->ticket_flags) |
        KDC_OPT_CNAME_IN_ADDL_TKT | KDC_OPT_CANONICALIZE;
    code = krb5_get_cred_via_tkt_ext(context, tgt, flags, tgt->addresses,
                                     in_padata, &mcreds, NULL, NULL, NULL,
                                     &enc_padata, &tkt, NULL);

    /*
     * If the server principal name included a foreign realm which wasn't an
     * alias for the local realm, the KDC won't be able to decrypt the TGT.
     * Windows KDCs will return a BAD_INTEGRITY error in this case, while MIT
     * KDCs will return S_PRINCIPAL_UNKNOWN.  We cannot distinguish the latter
     * error from the service principal actually being unknown in the realm,
     * but set a comprehensible error message for the BAD_INTEGRITY error.
     */
    if (code == KRB5KRB_AP_ERR_BAD_INTEGRITY &&
        !krb5_realm_compare(context, in_creds->client, server)) {
        k5_setmsg(context, code, _("Realm specified but S4U2Proxy must use "
                                   "referral realm"));
    }

    if (code)
        goto cleanup;

    if (!krb5_principal_compare_any_realm(context, server, tkt->server)) {
        /* Make sure we got a referral. */
        if (!IS_TGS_PRINC(tkt->server)) {
            code = KRB5KRB_AP_WRONG_PRINC;
            goto cleanup;
        }

        /* The authdata in this referral TGT will be copied into the final
         * credentials, so we don't need to request it again. */
        mcreds.authdata = NULL;

        /*
         * Make sure the KDC supports S4U and resource-based constrained
         * delegation; otherwise we might have gotten a regular TGT referral
         * rather than a proxy TGT referral.
         */
        code = check_rbcd_support(context, enc_padata);
        if (code)
            goto cleanup;

        krb5_free_pa_data(context, enc_padata);
        enc_padata = NULL;

        /*
         * Replace tgt with a regular (not proxy) TGT to the target realm, by
         * making a normal TGS request and following referrals.  Per [MS-SFU]
         * 3.1.5.2.2, we need this TGT to make the final TGS request.
         */
        code = get_tgt_to_target_realm(context, &mcreds, req_kdcopt, &tgt);
        if (code)
            goto cleanup;

        /*
         * Replace tkt with a proxy TGT (meaning, one obtained using the
         * referral TGT we got from the first S4U2Proxy request) to the target
         * realm, if it isn't already one.
         */
        code = get_target_realm_proxy_tgt(context, &tgt->server->data[1],
                                          req_kdcopt, &tkt);
        if (code)
            goto cleanup;

        krb5_free_data_contents(context, &server->realm);
        code = krb5int_copy_data_contents(context, &tgt->server->data[1],
                                          &server->realm);
        if (code)
            goto cleanup;

        /* Make an S4U2Proxy request to the target realm using the regular TGT,
         * with the proxy TGT as the evidence ticket. */
        mcreds.second_ticket = tkt->ticket;
        tkt->ticket = empty_data();
        krb5_free_creds(context, tkt);
        tkt = NULL;
        flags = req_kdcopt | FLAGS2OPTS(tgt->ticket_flags) |
            KDC_OPT_CNAME_IN_ADDL_TKT | KDC_OPT_CANONICALIZE;
        code = krb5_get_cred_via_tkt_ext(context, tgt, flags, tgt->addresses,
                                         in_padata, &mcreds, NULL, NULL, NULL,
                                         &enc_padata, &tkt, NULL);
        free(mcreds.second_ticket.data);
        if (code)
            goto cleanup;

        code = check_rbcd_support(context, enc_padata);
        if (code)
            goto cleanup;

        if (!krb5_principal_compare(context, server, tkt->server)) {
            code = KRB5KRB_AP_WRONG_PRINC;
            goto cleanup;
        }

        /* Put the original evidence ticket in the output creds. */
        krb5_free_data_contents(context, &tkt->second_ticket);
        code = krb5int_copy_data_contents(context, &in_creds->second_ticket,
                                          &tkt->second_ticket);
        if (code)
            goto cleanup;
    }

    /* Note the authdata we asked for in the output creds. */
    code = krb5_copy_authdata(context, in_creds->authdata, &tkt->authdata);
    if (code)
        goto cleanup;

    *out_creds = tkt;
    tkt = NULL;

cleanup:
    krb5_free_creds(context, tgt);
    krb5_free_creds(context, tkt);
    krb5_free_principal(context, server);
    krb5_free_pa_data(context, in_padata);
    krb5_free_pa_data(context, enc_padata);
    return code;
}

krb5_error_code
k5_get_proxy_cred_from_kdc(krb5_context context, krb5_flags options,
                           krb5_ccache ccache, krb5_creds *in_creds,
                           krb5_creds **out_creds)
{
    krb5_error_code code;
    krb5_const_principal canonprinc;
    krb5_creds copy, *creds;
    struct canonprinc iter = { in_creds->server, .no_hostrealm = TRUE };

    *out_creds = NULL;

    code = k5_get_cached_cred(context, options, ccache, in_creds, out_creds);
    if ((code != KRB5_CC_NOTFOUND && code != KRB5_CC_NOT_KTYPE) ||
        options & KRB5_GC_CACHED)
        return code;

    copy = *in_creds;
    while ((code = k5_canonprinc(context, &iter, &canonprinc)) == 0 &&
           canonprinc != NULL) {
        copy.server = (krb5_principal)canonprinc;
        code = get_proxy_cred_from_kdc(context, options, ccache, &copy,
                                       &creds);
        if (code != KRB5KDC_ERR_S_PRINCIPAL_UNKNOWN)
            break;
    }
    if (!code && canonprinc == NULL)
        code = KRB5KDC_ERR_S_PRINCIPAL_UNKNOWN;
    free_canonprinc(&iter);
    if (code)
        return code;

    krb5_free_principal(context, creds->server);
    creds->server = NULL;
    code = krb5_copy_principal(context, in_creds->server, &creds->server);
    if (code) {
        krb5_free_creds(context, creds);
        return code;
    }

    if (!(options & KRB5_GC_NO_STORE))
        (void)krb5_cc_store_cred(context, ccache, creds);

    *out_creds = creds;
    return 0;
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
    krb5_data *evidence_tkt_data = NULL;
    krb5_creds s4u_creds;

    *out_creds = NULL;

    if (in_creds == NULL || in_creds->client == NULL || evidence_tkt == NULL) {
        code = EINVAL;
        goto cleanup;
    }

    /*
     * Caller should have set in_creds->client to match evidence
     * ticket client.  If we can, verify it before issuing the request.
     */
    if (evidence_tkt->enc_part2 != NULL &&
        !krb5_principal_compare(context, evidence_tkt->enc_part2->client,
                                in_creds->client)) {
        code = EINVAL;
        goto cleanup;
    }

    code = encode_krb5_ticket(evidence_tkt, &evidence_tkt_data);
    if (code != 0)
        goto cleanup;

    s4u_creds = *in_creds;
    s4u_creds.client = evidence_tkt->server;
    s4u_creds.second_ticket = *evidence_tkt_data;

    code = k5_get_proxy_cred_from_kdc(context,
                                      options | KRB5_GC_CONSTRAINED_DELEGATION,
                                      ccache, &s4u_creds, out_creds);
    if (code != 0)
        goto cleanup;

    /*
     * Check client name because we couldn't compare that inside
     * krb5_get_credentials() (enc_part2 is unavailable in clear)
     */
    if (!krb5_principal_compare(context, in_creds->client,
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
