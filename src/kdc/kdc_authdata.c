/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* kdc/kdc_authdata.c - Authorization data routines for the KDC */
/*
 * Copyright (C) 2007 Apple Inc.  All Rights Reserved.
 * Copyright (C) 2008, 2009 by the Massachusetts Institute of Technology.
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
#include "kdc_util.h"
#include "extern.h"
#include <stdio.h>
#include "adm_proto.h"

#include <syslog.h>

#include <assert.h>
#include <krb5/kdcauthdata_plugin.h>

typedef struct kdcauthdata_handle_st {
    struct krb5_kdcauthdata_vtable_st vt;
    krb5_kdcauthdata_moddata data;
} kdcauthdata_handle;

static kdcauthdata_handle *authdata_modules;
static size_t n_authdata_modules;

/* Load authdata plugin modules. */
krb5_error_code
load_authdata_plugins(krb5_context context)
{
    krb5_error_code ret;
    krb5_plugin_initvt_fn *modules = NULL, *mod;
    kdcauthdata_handle *list, *h;
    size_t count;

    ret = k5_plugin_load_all(context, PLUGIN_INTERFACE_KDCAUTHDATA, &modules);
    if (ret)
        return ret;

    /* Allocate a large enough list of handles. */
    for (count = 0; modules[count] != NULL; count++);
    list = calloc(count + 1, sizeof(*list));
    if (list == NULL) {
        k5_plugin_free_modules(context, modules);
        return ENOMEM;
    }

    /* Initialize each module's vtable and module data. */
    count = 0;
    for (mod = modules; *mod != NULL; mod++) {
        h = &list[count];
        memset(h, 0, sizeof(*h));
        ret = (*mod)(context, 1, 1, (krb5_plugin_vtable)&h->vt);
        if (ret)                /* Version mismatch, keep going. */
            continue;
        if (h->vt.init != NULL) {
            ret = h->vt.init(context, &h->data);
            if (ret) {
                kdc_err(context, ret, _("while loading authdata module %s"),
                        h->vt.name);
                continue;
            }
        }
        count++;
    }

    authdata_modules = list;
    n_authdata_modules = count;
    k5_plugin_free_modules(context, modules);
    return 0;
}

krb5_error_code
unload_authdata_plugins(krb5_context context)
{
    kdcauthdata_handle *h;
    size_t i;

    for (i = 0; i < n_authdata_modules; i++) {
        h = &authdata_modules[i];
        if (h->vt.fini != NULL)
            h->vt.fini(context, h->data);
    }
    free(authdata_modules);
    authdata_modules = NULL;
    return 0;
}

/* Return true if authdata should be filtered when copying from untrusted
 * authdata.  If desired_type is non-zero, look only for that type. */
static krb5_boolean
is_kdc_issued_authdatum(krb5_context context, krb5_authdata *authdata,
                        krb5_authdatatype desired_type)
{
    krb5_boolean result = FALSE;
    krb5_authdatatype ad_type;
    unsigned int i, count = 0;
    krb5_authdatatype *ad_types, *containee_types = NULL;

    if (authdata->ad_type == KRB5_AUTHDATA_IF_RELEVANT) {
        if (krb5int_get_authdata_containee_types(context, authdata, &count,
                                                 &containee_types) != 0)
            goto cleanup;
        ad_types = containee_types;
    } else {
        ad_type = authdata->ad_type;
        count = 1;
        ad_types = &ad_type;
    }

    for (i = 0; i < count; i++) {
        switch (ad_types[i]) {
        case KRB5_AUTHDATA_SIGNTICKET:
        case KRB5_AUTHDATA_KDC_ISSUED:
        case KRB5_AUTHDATA_WIN2K_PAC:
        case KRB5_AUTHDATA_CAMMAC:
        case KRB5_AUTHDATA_AUTH_INDICATOR:
            result = desired_type ? (desired_type == ad_types[i]) : TRUE;
            break;
        default:
            result = FALSE;
            break;
        }
        if (result)
            break;
    }

cleanup:
    free(containee_types);
    return result;
}

/* Return true if authdata contains any elements which should only come from
 * the KDC.  If desired_type is non-zero, look only for that type. */
static krb5_boolean
has_kdc_issued_authdata(krb5_context context, krb5_authdata **authdata,
                        krb5_authdatatype desired_type)
{
    int i;

    if (authdata == NULL)
        return FALSE;
    for (i = 0; authdata[i] != NULL; i++) {
        if (is_kdc_issued_authdatum(context, authdata[i], desired_type))
            return TRUE;
    }
    return FALSE;
}

/* Return true if authdata contains any mandatory-for-KDC elements. */
static krb5_boolean
has_mandatory_for_kdc_authdata(krb5_context context, krb5_authdata **authdata)
{
    int i;

    if (authdata == NULL)
        return FALSE;
    for (i = 0; authdata[i] != NULL; i++) {
        if (authdata[i]->ad_type == KRB5_AUTHDATA_MANDATORY_FOR_KDC)
            return TRUE;
    }
    return FALSE;
}

/*
 * Add the elements of in_authdata to out_authdata.  If copy is false,
 * in_authdata is invalid on successful return.  If ignore_kdc_issued is true,
 * KDC-issued authdata is not copied.
 */
static krb5_error_code
merge_authdata(krb5_context context, krb5_authdata **in_authdata,
               krb5_authdata ***out_authdata, krb5_boolean copy,
               krb5_boolean ignore_kdc_issued)
{
    krb5_error_code ret;
    size_t i, j, nadata = 0;
    krb5_authdata **in_copy = NULL, **authdata = *out_authdata;

    if (in_authdata == NULL || in_authdata[0] == NULL)
        return 0;

    if (authdata != NULL) {
        for (nadata = 0; authdata[nadata] != NULL; nadata++)
            ;
    }

    for (i = 0; in_authdata[i] != NULL; i++)
        ;

    if (copy) {
        ret = krb5_copy_authdata(context, in_authdata, &in_copy);
        if (ret)
            return ret;
        in_authdata = in_copy;
    }

    authdata = realloc(authdata, (nadata + i + 1) * sizeof(krb5_authdata *));
    if (authdata == NULL) {
        krb5_free_authdata(context, in_copy);
        return ENOMEM;
    }

    for (i = 0, j = 0; in_authdata[i] != NULL; i++) {
        if (ignore_kdc_issued &&
            is_kdc_issued_authdatum(context, in_authdata[i], 0)) {
            free(in_authdata[i]->contents);
            free(in_authdata[i]);
        } else {
            authdata[nadata + j++] = in_authdata[i];
        }
    }

    authdata[nadata + j] = NULL;

    free(in_authdata);

    if (authdata[0] == NULL) {
        free(authdata);
        authdata = NULL;
    }

    *out_authdata = authdata;

    return 0;
}

/* Copy TGS-REQ authorization data into the ticket authdata. */
static krb5_error_code
copy_request_authdata(krb5_context context, krb5_keyblock *client_key,
                      krb5_kdc_req *req, krb5_enc_tkt_part *enc_tkt_req,
                      krb5_authdata ***tkt_authdata)
{
    krb5_error_code ret;
    krb5_data plaintext;

    assert(enc_tkt_req != NULL);

    ret = alloc_data(&plaintext, req->authorization_data.ciphertext.length);
    if (ret)
        return ret;

    /*
     * RFC 4120 requires authdata in the TGS body to be encrypted in the subkey
     * with usage 5 if a subkey is present, and in the TGS session key with key
     * usage 4 if it is not.  Prior to krb5 1.7, we got this wrong, always
     * decrypting the authorization data with the TGS session key and usage 4.
     * For the sake of conservatism, try the decryption the old way (wrong if
     * client_key is a subkey) first, and then try again the right way (in the
     * case where client_key is a subkey) if the first way fails.
     */
    ret = krb5_c_decrypt(context, enc_tkt_req->session,
                         KRB5_KEYUSAGE_TGS_REQ_AD_SESSKEY, 0,
                         &req->authorization_data, &plaintext);
    if (ret) {
        ret = krb5_c_decrypt(context, client_key,
                             KRB5_KEYUSAGE_TGS_REQ_AD_SUBKEY, 0,
                             &req->authorization_data, &plaintext);
    }
    if (ret)
        goto cleanup;

    /* Decode the decrypted authdata and make it available to modules in the
     * request. */
    ret = decode_krb5_authdata(&plaintext, &req->unenc_authdata);
    if (ret)
        goto cleanup;

    if (has_mandatory_for_kdc_authdata(context, req->unenc_authdata)) {
        ret = KRB5KDC_ERR_POLICY;
        goto cleanup;
    }

    /* Add a copy of the requested authdata to the ticket, ignoring KDC-issued
     * types. */
    ret = merge_authdata(context, req->unenc_authdata, tkt_authdata, TRUE,
                         TRUE);

cleanup:
    free(plaintext.data);
    return ret;
}

/* Copy TGT authorization data into the ticket authdata. */
static krb5_error_code
copy_tgt_authdata(krb5_context context, krb5_kdc_req *request,
                  krb5_authdata **tgt_authdata, krb5_authdata ***tkt_authdata)
{
    if (has_mandatory_for_kdc_authdata(context, tgt_authdata))
        return KRB5KDC_ERR_POLICY;

    /* Add a copy of the TGT authdata to the ticket, ignoring KDC-issued
     * types. */
    return merge_authdata(context, tgt_authdata, tkt_authdata, TRUE, TRUE);
}

/* Fetch authorization data from KDB module. */
static krb5_error_code
fetch_kdb_authdata(krb5_context context, unsigned int flags,
                   krb5_db_entry *client, krb5_db_entry *server,
                   krb5_db_entry *header_server, krb5_keyblock *client_key,
                   krb5_keyblock *server_key, krb5_keyblock *header_key,
                   krb5_kdc_req *req, krb5_const_principal for_user_princ,
                   krb5_enc_tkt_part *enc_tkt_req,
                   krb5_enc_tkt_part *enc_tkt_reply)
{
    krb5_error_code ret;
    krb5_authdata **tgt_authdata, **db_authdata = NULL;
    krb5_boolean tgs_req = (req->msg_type == KRB5_TGS_REQ);
    krb5_const_principal actual_client;
    krb5_db_entry *krbtgt;
    krb5_keyblock *krbtgt_key;

    /*
     * Check whether KDC issued authorization data should be included.
     * A server can explicitly disable the inclusion of authorization
     * data by setting the KRB5_KDB_NO_AUTH_DATA_REQUIRED flag on its
     * principal entry. Otherwise authorization data will be included
     * if it was present in the TGT, the client is from another realm
     * or protocol transition/constrained delegation was used, or, in
     * the AS-REQ case, if the pre-auth data indicated the PAC should
     * be present.
     */
    if (tgs_req) {
        assert(enc_tkt_req != NULL);

        if (isflagset(server->attributes, KRB5_KDB_NO_AUTH_DATA_REQUIRED))
            return 0;

        if (enc_tkt_req->authorization_data == NULL &&
            !isflagset(flags, KRB5_KDB_FLAG_CROSS_REALM | KRB5_KDB_FLAGS_S4U))
            return 0;

        assert(enc_tkt_reply->times.authtime == enc_tkt_req->times.authtime);
    } else {
        if (!isflagset(flags, KRB5_KDB_FLAG_INCLUDE_PAC))
            return 0;
    }

    /*
     * We have this special case for protocol transition, because for
     * cross-realm protocol transition the ticket reply client will
     * not be changed until the final hop.
     */
    if (isflagset(flags, KRB5_KDB_FLAG_PROTOCOL_TRANSITION))
        actual_client = for_user_princ;
    else
        actual_client = enc_tkt_reply->client;

    /*
     * For DAL major version 5, always pass "krbtgt" and "krbtgt_key"
     * parameters which are usually, but not always, for local or cross-realm
     * TGT principals.  In the future we might rename the parameters and pass
     * NULL for AS requests.
     */
    krbtgt = (header_server != NULL) ? header_server : server;
    krbtgt_key = (header_key != NULL) ? header_key : server_key;

    tgt_authdata = tgs_req ? enc_tkt_req->authorization_data : NULL;
    ret = krb5_db_sign_authdata(context, flags, actual_client, client,
                                server, krbtgt, client_key, server_key,
                                krbtgt_key, enc_tkt_reply->session,
                                enc_tkt_reply->times.authtime, tgt_authdata,
                                &db_authdata);
    if (ret)
        return (ret == KRB5_PLUGIN_OP_NOTSUPP) ? 0 : ret;

    /* Add the KDB authdata to the ticket, without copying or filtering. */
    ret = merge_authdata(context, db_authdata,
                         &enc_tkt_reply->authorization_data, FALSE, FALSE);
    if (ret)
        krb5_free_authdata(context, db_authdata);
    return ret;
}

static krb5_error_code
make_signedpath_data(krb5_context context, krb5_const_principal client,
                     krb5_timestamp authtime, krb5_principal *deleg_path,
                     krb5_pa_data **method_data, krb5_authdata **authdata,
                     krb5_data **data)
{
    krb5_error_code ret;
    krb5_ad_signedpath_data sp_data;
    krb5_authdata **sign_authdata = NULL;
    size_t i, j, count;

    memset(&sp_data, 0, sizeof(sp_data));

    for (count = 0; authdata != NULL && authdata[count] != NULL; count++);
    if (count != 0) {
        /* Make a shallow copy with AD-SIGNTICKET filtered out. */
        sign_authdata = k5calloc(count + 1, sizeof(krb5_authdata *), &ret);
        if (sign_authdata == NULL)
            return ret;

        for (i = 0, j = 0; authdata[i] != NULL; i++) {
            if (is_kdc_issued_authdatum(context, authdata[i],
                                        KRB5_AUTHDATA_SIGNTICKET))
                continue;

            sign_authdata[j++] = authdata[i];
        }

        sign_authdata[j] = NULL;
    }

    sp_data.client = (krb5_principal)client;
    sp_data.authtime = authtime;
    sp_data.delegated = deleg_path;
    sp_data.method_data = method_data;
    sp_data.authorization_data = sign_authdata;

    ret = encode_krb5_ad_signedpath_data(&sp_data, data);

    if (sign_authdata != NULL)
        free(sign_authdata);

    return ret;
}

static krb5_error_code
verify_signedpath_checksum(krb5_context context, krb5_db_entry *local_tgt,
                           krb5_enc_tkt_part *enc_tkt_part,
                           krb5_principal *deleg_path,
                           krb5_pa_data **method_data, krb5_checksum *cksum,
                           krb5_boolean *valid_out)
{
    krb5_error_code ret;
    krb5_data *data;
    krb5_key_data *kd;
    krb5_keyblock tgtkey;
    krb5_kvno kvno;
    krb5_boolean valid = FALSE;
    int tries;

    *valid_out = FALSE;
    memset(&tgtkey, 0, sizeof(tgtkey));

    if (!krb5_c_is_keyed_cksum(cksum->checksum_type))
        return KRB5KRB_AP_ERR_INAPP_CKSUM;

    ret = make_signedpath_data(context, enc_tkt_part->client,
                               enc_tkt_part->times.authtime, deleg_path,
                               method_data, enc_tkt_part->authorization_data,
                               &data);
    if (ret)
        return ret;

    /* There is no kvno in AD-SIGNTICKET, so try the last three versions. */
    kvno = 0;
    tries = 3;
    do {
        /* Get the first local tgt key of this kvno (highest kvno for the first
         * iteration). */
        ret = krb5_dbe_find_enctype(context, local_tgt, -1, -1, kvno, &kd);
        if (ret) {
            ret = 0;
            break;
        }
        ret = krb5_dbe_decrypt_key_data(context, NULL, kd, &tgtkey, NULL);
        if (ret)
            break;

        ret = krb5_c_verify_checksum(context, &tgtkey,
                                     KRB5_KEYUSAGE_AD_SIGNEDPATH, data, cksum,
                                     &valid);
        krb5_free_keyblock_contents(context, &tgtkey);
        if (!ret && valid)
            break;

        /* Try the next lower kvno on the next iteration. */
        kvno = kd->key_data_kvno - 1;
    } while (--tries > 0 && kvno > 0);

    *valid_out = valid;
    krb5_free_data(context, data);
    return ret;
}


static krb5_error_code
verify_signedpath(krb5_context context, krb5_db_entry *local_tgt,
                  krb5_enc_tkt_part *enc_tkt_part,
                  krb5_principal **delegated_out, krb5_boolean *pathsigned_out)
{
    krb5_error_code ret;
    krb5_ad_signedpath *sp = NULL;
    krb5_authdata **sp_authdata = NULL;
    krb5_data enc_sp;

    *delegated_out = NULL;
    *pathsigned_out = FALSE;

    ret = krb5_find_authdata(context, enc_tkt_part->authorization_data, NULL,
                             KRB5_AUTHDATA_SIGNTICKET, &sp_authdata);
    if (ret)
        goto cleanup;

    if (sp_authdata == NULL ||
        sp_authdata[0]->ad_type != KRB5_AUTHDATA_SIGNTICKET ||
        sp_authdata[1] != NULL)
        goto cleanup;

    enc_sp.data = (char *)sp_authdata[0]->contents;
    enc_sp.length = sp_authdata[0]->length;

    ret = decode_krb5_ad_signedpath(&enc_sp, &sp);
    if (ret) {
        /* Treat an invalid signedpath authdata element as a missing one, since
         * we believe MS is using the same number for something else. */
        ret = 0;
        goto cleanup;
    }

    ret = verify_signedpath_checksum(context, local_tgt, enc_tkt_part,
                                     sp->delegated, sp->method_data,
                                     &sp->checksum, pathsigned_out);
    if (ret)
        goto cleanup;

    if (*pathsigned_out) {
        *delegated_out = sp->delegated;
        sp->delegated = NULL;
    }

cleanup:
    krb5_free_ad_signedpath(context, sp);
    krb5_free_authdata(context, sp_authdata);
    return ret;
}

static krb5_error_code
make_signedpath_checksum(krb5_context context,
                         krb5_const_principal for_user_princ,
                         krb5_db_entry *local_tgt,
                         krb5_enc_tkt_part *enc_tkt_part,
                         krb5_principal *deleg_path,
                         krb5_pa_data **method_data, krb5_checksum *cksum_out,
                         krb5_enctype *enctype_out)
{
    krb5_error_code ret;
    krb5_data *data = NULL;
    krb5_const_principal client;
    krb5_key_data *kd;
    krb5_keyblock tgtkey;

    memset(&tgtkey, 0, sizeof(tgtkey));
    memset(cksum_out, 0, sizeof(*cksum_out));
    *enctype_out = ENCTYPE_NULL;

    client = (for_user_princ != NULL) ? for_user_princ : enc_tkt_part->client;

    /* Get the first local tgt key of the highest kvno. */
    ret = krb5_dbe_find_enctype(context, local_tgt, -1, -1, 0, &kd);
    if (ret)
        goto cleanup;
    ret = krb5_dbe_decrypt_key_data(context, NULL, kd, &tgtkey, NULL);
    if (ret)
        goto cleanup;

    ret = make_signedpath_data(context, client, enc_tkt_part->times.authtime,
                               deleg_path, method_data,
                               enc_tkt_part->authorization_data, &data);
    if (ret)
        goto cleanup;

    ret = krb5_c_make_checksum(context, 0, &tgtkey,
                               KRB5_KEYUSAGE_AD_SIGNEDPATH, data, cksum_out);
    *enctype_out = tgtkey.enctype;

cleanup:
    krb5_free_data(context, data);
    krb5_free_keyblock_contents(context, &tgtkey);
    return ret;
}

static krb5_error_code
make_signedpath(krb5_context context, krb5_const_principal for_user_princ,
                krb5_principal server, krb5_db_entry *local_tgt,
                krb5_principal *deleg_path, krb5_enc_tkt_part *enc_tkt_reply)
{
    krb5_error_code ret;
    krb5_ad_signedpath sp;
    krb5_data *data = NULL;
    krb5_authdata ad_datum, *ad_data[2];
    krb5_authdata **if_relevant = NULL;
    size_t count;

    memset(&sp, 0, sizeof(sp));

    for (count = 0; deleg_path != NULL && deleg_path[count] != NULL; count++);

    sp.delegated = k5calloc(count + 2, sizeof(krb5_principal), &ret);
    if (sp.delegated == NULL)
        goto cleanup;

    /* Combine existing and new transited services, if any */
    if (deleg_path != NULL)
        memcpy(sp.delegated, deleg_path, count * sizeof(krb5_principal));
    if (server != NULL)
        sp.delegated[count++] = server;
    sp.delegated[count] = NULL;
    sp.method_data = NULL;

    ret = make_signedpath_checksum(context, for_user_princ, local_tgt,
                                   enc_tkt_reply, sp.delegated, sp.method_data,
                                   &sp.checksum, &sp.enctype);
    if (ret) {
        if (ret == KRB5KRB_AP_ERR_INAPP_CKSUM) {
            /*
             * In the hopefully unlikely case the TGS key enctype has an
             * unkeyed mandatory checksum type, do not fail so we do not
             * prevent the KDC from servicing requests.
             */
            ret = 0;
        }
        goto cleanup;
    }

    ret = encode_krb5_ad_signedpath(&sp, &data);
    if (ret)
        goto cleanup;

    ad_datum.ad_type = KRB5_AUTHDATA_SIGNTICKET;
    ad_datum.contents = (krb5_octet *)data->data;
    ad_datum.length = data->length;

    ad_data[0] = &ad_datum;
    ad_data[1] = NULL;

    ret = krb5_encode_authdata_container(context, KRB5_AUTHDATA_IF_RELEVANT,
                                         ad_data, &if_relevant);
    if (ret)
        goto cleanup;

    /* Add the authdata to the ticket, without copying or filtering. */
    ret = merge_authdata(context, if_relevant,
                         &enc_tkt_reply->authorization_data, FALSE, FALSE);
    if (ret)
        goto cleanup;
    if_relevant = NULL;         /* merge_authdata() freed */

cleanup:
    free(sp.delegated);
    krb5_free_authdata(context, if_relevant);
    krb5_free_data(context, data);
    krb5_free_checksum_contents(context, &sp.checksum);
    krb5_free_pa_data(context, sp.method_data);
    return ret;
}

static void
free_deleg_path(krb5_context context, krb5_principal *deleg_path)
{
    int i;

    for (i = 0; deleg_path != NULL && deleg_path[i] != NULL; i++)
        krb5_free_principal(context, deleg_path[i]);
    free(deleg_path);
}

/* Return true if the Windows 2000 PAC is the only element in the supplied
 * authorization data. */
static krb5_boolean
only_pac_p(krb5_context context, krb5_authdata **authdata)
{
    return has_kdc_issued_authdata(context, authdata,
                                   KRB5_AUTHDATA_WIN2K_PAC) &&
        authdata[1] == NULL;
}

/* Verify AD-SIGNTICKET authdata if we need to, and insert an AD-SIGNEDPATH
 * element if we should. */
static krb5_error_code
handle_signticket(krb5_context context, unsigned int flags,
                  krb5_db_entry *client, krb5_db_entry *server,
                  krb5_db_entry *local_tgt, krb5_kdc_req *req,
                  krb5_const_principal for_user_princ,
                  krb5_enc_tkt_part *enc_tkt_req,
                  krb5_enc_tkt_part *enc_tkt_reply)
{
    krb5_error_code ret = 0;
    krb5_principal *deleg_path = NULL;
    krb5_boolean signed_path = FALSE;
    krb5_boolean s4u2proxy;

    s4u2proxy = isflagset(flags, KRB5_KDB_FLAG_CONSTRAINED_DELEGATION);

    /*
     * The Windows PAC fulfils the same role as the signed path
     * if it is the only authorization data element.
     */
    if (req->msg_type == KRB5_TGS_REQ &&
        !only_pac_p(context, enc_tkt_req->authorization_data)) {
        ret = verify_signedpath(context, local_tgt, enc_tkt_req, &deleg_path,
                                &signed_path);
        if (ret)
            goto cleanup;

        if (s4u2proxy && signed_path == FALSE) {
            ret = KRB5KDC_ERR_BADOPTION;
            goto cleanup;
        }
    }

    /* No point in including signedpath authdata for a cross-realm TGT, since
     * it will be presented to a different KDC. */
    if (!isflagset(server->attributes, KRB5_KDB_NO_AUTH_DATA_REQUIRED) &&
        !is_cross_tgs_principal(server->princ) &&
        !only_pac_p(context, enc_tkt_reply->authorization_data)) {
        ret = make_signedpath(context, for_user_princ,
                              s4u2proxy ? client->princ : NULL, local_tgt,
                              deleg_path, enc_tkt_reply);
        if (ret)
            goto cleanup;
    }

cleanup:
    free_deleg_path(context, deleg_path);
    return ret;
}

/* Add authentication indicator authdata to enc_tkt_reply, wrapped in a CAMMAC
 * and an IF-RELEVANT container. */
static krb5_error_code
add_auth_indicators(krb5_context context, krb5_data *const *auth_indicators,
                    krb5_keyblock *server_key, krb5_db_entry *krbtgt,
                    krb5_enc_tkt_part *enc_tkt_reply)
{
    krb5_error_code ret;
    krb5_data *der_indicators = NULL;
    krb5_authdata ad, *list[2], **cammac = NULL;

    /* Format the authentication indicators into an authdata list. */
    ret = encode_utf8_strings(auth_indicators, &der_indicators);
    if (ret)
        goto cleanup;
    ad.ad_type = KRB5_AUTHDATA_AUTH_INDICATOR;
    ad.length = der_indicators->length;
    ad.contents = (uint8_t *)der_indicators->data;
    list[0] = &ad;
    list[1] = NULL;

    /* Wrap the list in CAMMAC and IF-RELEVANT containers. */
    ret = cammac_create(context, enc_tkt_reply, server_key, krbtgt, list,
                        &cammac);
    if (ret)
        goto cleanup;

    /* Add the wrapped authdata to the ticket, without copying or filtering. */
    ret = merge_authdata(context, cammac, &enc_tkt_reply->authorization_data,
                         FALSE, FALSE);
    if (ret)
        goto cleanup;
    cammac = NULL;              /* merge_authdata() freed */

cleanup:
    krb5_free_data(context, der_indicators);
    krb5_free_authdata(context, cammac);
    return ret;
}

/* Extract any properly verified authentication indicators from the authdata in
 * enc_tkt. */
krb5_error_code
get_auth_indicators(krb5_context context, krb5_enc_tkt_part *enc_tkt,
                    krb5_db_entry *local_tgt, krb5_data ***indicators_out)
{
    krb5_error_code ret;
    krb5_authdata **cammacs = NULL, **adp;
    krb5_cammac *cammac = NULL;
    krb5_data **indicators = NULL, der_cammac;

    *indicators_out = NULL;

    ret = krb5_find_authdata(context, enc_tkt->authorization_data, NULL,
                             KRB5_AUTHDATA_CAMMAC, &cammacs);
    if (ret)
        goto cleanup;

    for (adp = cammacs; adp != NULL && *adp != NULL; adp++) {
        der_cammac = make_data((*adp)->contents, (*adp)->length);
        ret = decode_krb5_cammac(&der_cammac, &cammac);
        if (ret)
            goto cleanup;
        if (cammac_check_kdcver(context, cammac, enc_tkt, local_tgt)) {
            ret = authind_extract(context, cammac->elements, &indicators);
            if (ret)
                goto cleanup;
        }
        k5_free_cammac(context, cammac);
        cammac = NULL;
    }

    *indicators_out = indicators;
    indicators = NULL;

cleanup:
    krb5_free_authdata(context, cammacs);
    k5_free_cammac(context, cammac);
    k5_free_data_ptr_list(indicators);
    return ret;
}

krb5_error_code
handle_authdata(krb5_context context, unsigned int flags,
                krb5_db_entry *client, krb5_db_entry *server,
                krb5_db_entry *header_server, krb5_db_entry *local_tgt,
                krb5_keyblock *client_key, krb5_keyblock *server_key,
                krb5_keyblock *header_key, krb5_data *req_pkt,
                krb5_kdc_req *req, krb5_const_principal for_user_princ,
                krb5_enc_tkt_part *enc_tkt_req,
                krb5_data *const *auth_indicators,
                krb5_enc_tkt_part *enc_tkt_reply)
{
    kdcauthdata_handle *h;
    krb5_error_code ret = 0;
    size_t i;

    if (req->msg_type == KRB5_TGS_REQ &&
        req->authorization_data.ciphertext.data != NULL) {
        /* Copy TGS request authdata.  This must be done first so that modules
         * have access to the unencrypted request authdata. */
        ret = copy_request_authdata(context, client_key, req, enc_tkt_req,
                                    &enc_tkt_reply->authorization_data);
        if (ret)
            return ret;
    }

    /* Invoke loaded module handlers. */
    if (!isflagset(enc_tkt_reply->flags, TKT_FLG_ANONYMOUS)) {
        for (i = 0; i < n_authdata_modules; i++) {
            h = &authdata_modules[i];
            ret = h->vt.handle(context, h->data, flags, client, server,
                               header_server, client_key, server_key,
                               header_key, req_pkt, req, for_user_princ,
                               enc_tkt_req, enc_tkt_reply);
            if (ret)
                kdc_err(context, ret, "from authdata module %s", h->vt.name);
        }
    }

    if (req->msg_type == KRB5_TGS_REQ) {
        /* Copy authdata from the TGT to the issued ticket. */
        ret = copy_tgt_authdata(context, req, enc_tkt_req->authorization_data,
                                &enc_tkt_reply->authorization_data);
        if (ret)
            return ret;
    }

    /* Add auth indicators if any were given. */
    if (auth_indicators != NULL && *auth_indicators != NULL &&
        !isflagset(server->attributes, KRB5_KDB_NO_AUTH_DATA_REQUIRED)) {
        ret = add_auth_indicators(context, auth_indicators, server_key,
                                  local_tgt, enc_tkt_reply);
        if (ret)
            return ret;
    }

    if (!isflagset(enc_tkt_reply->flags, TKT_FLG_ANONYMOUS)) {
        /* Fetch authdata from the KDB if appropriate. */
        ret = fetch_kdb_authdata(context, flags, client, server, header_server,
                                 client_key, server_key, header_key, req,
                                 for_user_princ, enc_tkt_req, enc_tkt_reply);
        if (ret)
            return ret;

        /* Validate and insert AD-SIGNTICKET authdata.  This must happen last
         * since it contains a signature over the other authdata. */
        ret = handle_signticket(context, flags, client, server, local_tgt,
                                req, for_user_princ, enc_tkt_req,
                                enc_tkt_reply);
        if (ret)
            return ret;
    }

    return 0;
}
