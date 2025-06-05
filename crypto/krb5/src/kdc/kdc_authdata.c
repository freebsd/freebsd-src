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
is_kdc_issued_authdatum(krb5_authdata *authdata,
                        krb5_authdatatype desired_type)
{
    krb5_boolean result = FALSE;
    krb5_authdatatype ad_type;
    unsigned int i, count = 0;
    krb5_authdatatype *ad_types, *containee_types = NULL;

    if (authdata->ad_type == KRB5_AUTHDATA_IF_RELEVANT) {
        if (krb5int_get_authdata_containee_types(NULL, authdata, &count,
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

/* Add elements from *new_elements to *existing_list, reallocating as
 * necessary.  On success, release *new_elements and set it to NULL. */
static krb5_error_code
merge_authdata(krb5_authdata ***existing_list, krb5_authdata ***new_elements)
{
    size_t count = 0, ncount = 0;
    krb5_authdata **list = *existing_list, **nlist = *new_elements;

    if (nlist == NULL)
        return 0;

    for (count = 0; list != NULL && list[count] != NULL; count++);
    for (ncount = 0; nlist[ncount] != NULL; ncount++);

    list = realloc(list, (count + ncount + 1) * sizeof(*list));
    if (list == NULL)
        return ENOMEM;

    memcpy(list + count, nlist, ncount * sizeof(*nlist));
    list[count + ncount] = NULL;
    free(nlist);

    if (list[0] == NULL) {
        free(list);
        list = NULL;
    }

    *new_elements = NULL;
    *existing_list = list;
    return 0;
}

/* Add a copy of new_elements to *existing_list, omitting KDC-issued
 * authdata. */
static krb5_error_code
add_filtered_authdata(krb5_authdata ***existing_list,
                      krb5_authdata **new_elements)
{
    krb5_error_code ret;
    krb5_authdata **copy;
    size_t i, j;

    if (new_elements == NULL)
        return 0;

    ret = krb5_copy_authdata(NULL, new_elements, &copy);
    if (ret)
        return ret;

    /* Remove KDC-issued elements from copy. */
    j = 0;
    for (i = 0; copy[i] != NULL; i++) {
        if (is_kdc_issued_authdatum(copy[i], 0)) {
            free(copy[i]->contents);
            free(copy[i]);
        } else {
            copy[j++] = copy[i];
        }
    }
    copy[j] = NULL;

    /* Destructively merge the filtered copy into existing_list. */
    ret = merge_authdata(existing_list, &copy);
    krb5_free_authdata(NULL, copy);
    return ret;
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

    ret = add_filtered_authdata(tkt_authdata, req->unenc_authdata);

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

    return add_filtered_authdata(tkt_authdata, tgt_authdata);
}

/* Add authentication indicator authdata to enc_tkt_reply, wrapped in a CAMMAC
 * and an IF-RELEVANT container. */
static krb5_error_code
add_auth_indicators(krb5_context context, krb5_data *const *auth_indicators,
                    krb5_keyblock *server_key, krb5_db_entry *krbtgt,
                    krb5_keyblock *krbtgt_key,
                    krb5_enc_tkt_part *enc_tkt_reply)
{
    krb5_error_code ret;
    krb5_data *der_indicators = NULL;
    krb5_authdata ad, *list[2], **cammac = NULL;

    if (auth_indicators == NULL || *auth_indicators == NULL)
        return 0;

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
    ret = cammac_create(context, enc_tkt_reply, server_key, krbtgt, krbtgt_key,
                        list, &cammac);
    if (ret)
        goto cleanup;

    /* Add the wrapped authdata to the ticket, without copying or filtering. */
    ret = merge_authdata(&enc_tkt_reply->authorization_data, &cammac);

cleanup:
    krb5_free_data(context, der_indicators);
    krb5_free_authdata(context, cammac);
    return ret;
}

/* Extract any properly verified authentication indicators from the authdata in
 * enc_tkt. */
krb5_error_code
get_auth_indicators(krb5_context context, krb5_enc_tkt_part *enc_tkt,
                    krb5_db_entry *local_tgt, krb5_keyblock *local_tgt_key,
                    krb5_data ***indicators_out)
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
        if (cammac_check_kdcver(context, cammac, enc_tkt, local_tgt,
                                local_tgt_key)) {
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

static krb5_error_code
update_delegation_info(krb5_context context, krb5_kdc_req *req,
                       krb5_pac old_pac, krb5_pac new_pac)
{
    krb5_error_code ret;
    krb5_data ndr_di_in = empty_data(), ndr_di_out = empty_data();
    struct pac_s4u_delegation_info *di = NULL;
    char *namestr = NULL;

    ret = krb5_pac_get_buffer(context, old_pac, KRB5_PAC_DELEGATION_INFO,
                              &ndr_di_in);
    if (ret && ret != ENOENT)
        goto cleanup;
    if (ret) {
        /* Create new delegation info. */
        di = k5alloc(sizeof(*di), &ret);
        if (di == NULL)
            goto cleanup;
        di->transited_services = k5calloc(1, sizeof(char *), &ret);
        if (di->transited_services == NULL)
            goto cleanup;
    } else {
        /* Decode and modify old delegation info. */
        ret = ndr_dec_delegation_info(&ndr_di_in, &di);
        if (ret)
            goto cleanup;
    }

    /* Set proxy_target to the requested server, without realm. */
    ret = krb5_unparse_name_flags(context, req->server,
                                  KRB5_PRINCIPAL_UNPARSE_DISPLAY |
                                  KRB5_PRINCIPAL_UNPARSE_NO_REALM,
                                  &namestr);
    if (ret)
        goto cleanup;
    free(di->proxy_target);
    di->proxy_target = namestr;

    /* Add a transited entry for the requesting service, with realm. */
    assert(req->second_ticket != NULL && req->second_ticket[0] != NULL);
    ret = krb5_unparse_name(context, req->second_ticket[0]->server, &namestr);
    if (ret)
        goto cleanup;
    di->transited_services[di->transited_services_length++] = namestr;

    ret = ndr_enc_delegation_info(di, &ndr_di_out);
    if (ret)
        goto cleanup;

    ret = krb5_pac_add_buffer(context, new_pac, KRB5_PAC_DELEGATION_INFO,
                              &ndr_di_out);

cleanup:
    krb5_free_data_contents(context, &ndr_di_in);
    krb5_free_data_contents(context, &ndr_di_out);
    ndr_free_delegation_info(di);
    return ret;
}

static krb5_error_code
copy_pac_buffer(krb5_context context, uint32_t buffer_type, krb5_pac old_pac,
                krb5_pac new_pac)
{
    krb5_error_code ret;
    krb5_data data;

    ret = krb5_pac_get_buffer(context, old_pac, buffer_type, &data);
    if (ret)
        return ret;
    ret = krb5_pac_add_buffer(context, new_pac, buffer_type, &data);
    krb5_free_data_contents(context, &data);
    return ret;
}

/*
 * Possibly add a signed PAC to enc_tkt_reply.  Also possibly add auth
 * indicators; these are handled here so that the KDB module's issue_pac()
 * method can alter the auth indicator list.
 */
static krb5_error_code
handle_pac(kdc_realm_t *realm, unsigned int flags, krb5_db_entry *client,
           krb5_db_entry *server, krb5_db_entry *subject_server,
           krb5_db_entry *local_tgt, krb5_keyblock *local_tgt_key,
           krb5_keyblock *server_key, krb5_keyblock *subject_key,
           krb5_keyblock *replaced_reply_key, krb5_enc_tkt_part *subject_tkt,
           krb5_pac subject_pac, krb5_kdc_req *req,
           krb5_const_principal altcprinc, krb5_timestamp authtime,
           krb5_enc_tkt_part *enc_tkt_reply, krb5_data ***auth_indicators)
{
    krb5_context context = realm->realm_context;
    krb5_error_code ret;
    krb5_pac new_pac = NULL;
    krb5_const_principal pac_client = NULL;
    krb5_boolean with_realm, is_as_req = (req->msg_type == KRB5_AS_REQ);
    krb5_db_entry *signing_tgt;
    krb5_keyblock *privsvr_key = NULL;

    /* Don't add a PAC or auth indicators if the server disables authdata. */
    if (server->attributes & KRB5_KDB_NO_AUTH_DATA_REQUIRED)
        return 0;

    /*
     * Don't add a PAC if the realm disables them, or to an anonymous ticket,
     * or for an AS-REQ if the client requested not to get one, or for a
     * TGS-REQ if the subject ticket didn't contain one.
     */
    if (realm->realm_disable_pac ||
        (enc_tkt_reply->flags & TKT_FLG_ANONYMOUS) ||
        (is_as_req && !include_pac_p(context, req)) ||
        (!is_as_req && subject_pac == NULL)) {
        return add_auth_indicators(context, *auth_indicators, server_key,
                                   local_tgt, local_tgt_key, enc_tkt_reply);
    }

    ret = krb5_pac_init(context, &new_pac);
    if (ret)
        goto cleanup;

    if (subject_pac == NULL)
        signing_tgt = NULL;
    else if (krb5_is_tgs_principal(subject_server->princ))
        signing_tgt = subject_server;
    else
        signing_tgt = local_tgt;

    ret = krb5_db_issue_pac(context, flags, client, replaced_reply_key, server,
                            signing_tgt, authtime, subject_pac, new_pac,
                            auth_indicators);
    if (ret) {
        if (ret == KRB5_PLUGIN_OP_NOTSUPP)
            ret = 0;
        if (ret)
            goto cleanup;
    }

    ret = add_auth_indicators(context, *auth_indicators, server_key,
                              local_tgt, local_tgt_key, enc_tkt_reply);

    if ((flags & KRB5_KDB_FLAG_CONSTRAINED_DELEGATION) &&
        !(flags & KRB5_KDB_FLAG_CROSS_REALM)) {
        /* Add delegation info for the first S4U2Proxy request. */
        ret = update_delegation_info(context, req, subject_pac, new_pac);
        if (ret)
            goto cleanup;
    } else if (subject_pac != NULL) {
        /* Copy delegation info if it was present in the subject PAC. */
        ret = copy_pac_buffer(context, KRB5_PAC_DELEGATION_INFO, subject_pac,
                              new_pac);
        if (ret && ret != ENOENT)
            goto cleanup;
    }

    if ((flags & KRB5_KDB_FLAGS_S4U) &&
        (flags & KRB5_KDB_FLAG_ISSUING_REFERRAL)) {
        /* When issuing a referral for either kind of S4U request, add client
         * info for the subject with realm. */
        pac_client = altcprinc;
        with_realm = TRUE;
    } else if (subject_pac == NULL || (flags & KRB5_KDB_FLAGS_S4U)) {
        /* For a new PAC or when issuing a final ticket for either kind of S4U
         * request, add client info for the ticket client without the realm. */
        pac_client = enc_tkt_reply->client;
        with_realm = FALSE;
    } else {
        /*
         * For regular TGS and transitive RBCD requests, copy the client info
         * from the incoming PAC, and don't add client info during signing.  We
         * validated the incoming client info in validate_tgs_request().
         */
        ret = copy_pac_buffer(context, KRB5_PAC_CLIENT_INFO, subject_pac,
                              new_pac);
        if (ret)
            goto cleanup;
        pac_client = NULL;
        with_realm = FALSE;
    }

    ret = pac_privsvr_key(context, server, local_tgt_key, &privsvr_key);
    if (ret)
        goto cleanup;
    ret = krb5_kdc_sign_ticket(context, enc_tkt_reply, new_pac, server->princ,
                               pac_client, server_key, privsvr_key,
                               with_realm);
    if (ret)
        goto cleanup;

    ret = 0;

cleanup:
    krb5_pac_free(context, new_pac);
    krb5_free_keyblock(context, privsvr_key);
    return ret;
}

krb5_error_code
handle_authdata(kdc_realm_t *realm, unsigned int flags, krb5_db_entry *client,
                krb5_db_entry *server, krb5_db_entry *subject_server,
                krb5_db_entry *local_tgt, krb5_keyblock *local_tgt_key,
                krb5_keyblock *client_key, krb5_keyblock *server_key,
                krb5_keyblock *subject_key, krb5_keyblock *replaced_reply_key,
                krb5_data *req_pkt, krb5_kdc_req *req,
                krb5_const_principal altcprinc, krb5_pac subject_pac,
                krb5_enc_tkt_part *enc_tkt_req, krb5_data ***auth_indicators,
                krb5_enc_tkt_part *enc_tkt_reply)
{
    krb5_context context = realm->realm_context;
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
                               subject_server, client_key, server_key,
                               subject_key, req_pkt, req, altcprinc,
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

    return handle_pac(realm, flags, client, server, subject_server, local_tgt,
                      local_tgt_key, server_key, subject_key,
                      replaced_reply_key, enc_tkt_req, subject_pac, req,
                      altcprinc, enc_tkt_reply->times.authtime, enc_tkt_reply,
                      auth_indicators);
}
