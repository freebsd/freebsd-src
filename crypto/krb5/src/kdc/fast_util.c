/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* kdc/fast_util.c */
/*
 * Copyright (C) 2009, 2015 by the Massachusetts Institute of Technology.
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

#include "kdc_util.h"
#include "extern.h"

/* Let cookies be valid for ten minutes. */
#define COOKIE_LIFETIME 600

static krb5_error_code armor_ap_request
(struct kdc_request_state *state, krb5_fast_armor *armor)
{
    krb5_error_code retval = 0;
    krb5_auth_context authcontext = NULL;
    krb5_ticket *ticket = NULL;
    krb5_keyblock *subkey = NULL;
    kdc_realm_t *realm = state->realm_data;
    krb5_context context = realm->realm_context;

    assert(armor->armor_type == KRB5_FAST_ARMOR_AP_REQUEST);
    krb5_clear_error_message(context);
    retval = krb5_auth_con_init(context, &authcontext);
    /*disable replay cache*/
    if (retval == 0)
        retval = krb5_auth_con_setflags(context, authcontext, 0);
    if (retval == 0)
        retval = krb5_rd_req(context, &authcontext, &armor->armor_value,
                             NULL /*server*/, realm->realm_keytab,
                             NULL, &ticket);
    if (retval != 0) {
        const char * errmsg = krb5_get_error_message(context, retval);
        k5_setmsg(context, retval, _("%s while handling ap-request armor"),
                  errmsg);
        krb5_free_error_message(context, errmsg);
    }
    if (retval == 0) {
        if (!krb5_principal_compare_any_realm(context, realm->realm_tgsprinc,
                                              ticket->server)) {
            k5_setmsg(context, KRB5KDC_ERR_SERVER_NOMATCH,
                      _("ap-request armor for something other than the local "
                        "TGS"));
            retval = KRB5KDC_ERR_SERVER_NOMATCH;
        }
    }
    if (retval == 0) {
        retval = krb5_auth_con_getrecvsubkey(context, authcontext, &subkey);
        if (retval != 0 || subkey == NULL) {
            k5_setmsg(context, KRB5KDC_ERR_POLICY,
                      _("ap-request armor without subkey"));
            retval = KRB5KDC_ERR_POLICY;
        }
    }
    if (retval == 0)
        retval = krb5_c_fx_cf2_simple(context,
                                      subkey, "subkeyarmor",
                                      ticket->enc_part2->session, "ticketarmor",
                                      &state->armor_key);
    if (ticket)
        krb5_free_ticket(context, ticket);
    if (subkey)
        krb5_free_keyblock(context, subkey);
    if (authcontext)
        krb5_auth_con_free(context, authcontext);
    return retval;
}

static krb5_error_code
encrypt_fast_reply(struct kdc_request_state *state,
                   const krb5_fast_response *response,
                   krb5_data **fx_fast_reply)
{
    krb5_context context = state->realm_data->realm_context;
    krb5_error_code retval = 0;
    krb5_enc_data encrypted_reply;
    krb5_data *encoded_response = NULL;

    assert(state->armor_key);
    retval = encode_krb5_fast_response(response, &encoded_response);
    if (retval== 0)
        retval = krb5_encrypt_helper(context, state->armor_key,
                                     KRB5_KEYUSAGE_FAST_REP,
                                     encoded_response, &encrypted_reply);
    if (encoded_response)
        krb5_free_data(context, encoded_response);
    encoded_response = NULL;
    if (retval == 0) {
        retval = encode_krb5_pa_fx_fast_reply(&encrypted_reply,
                                              fx_fast_reply);
        krb5_free_data_contents(context, &encrypted_reply.ciphertext);
    }
    return retval;
}


/*
 * This function will find the FAST padata and, if FAST is successfully
 * processed, will free the outer request and update the pointer to point to
 * the inner request.  checksummed_data points to the data that is in the
 * armored_fast_request checksum; either the pa-tgs-req or the kdc-req-body.
 */
krb5_error_code
kdc_find_fast(krb5_kdc_req **requestptr,
              krb5_data *checksummed_data,
              krb5_keyblock *tgs_subkey,
              krb5_keyblock *tgs_session,
              struct kdc_request_state *state,
              krb5_data **inner_body_out)
{
    krb5_context context = state->realm_data->realm_context;
    krb5_error_code retval = 0;
    krb5_pa_data *fast_padata;
    krb5_data scratch, plaintext, *inner_body = NULL;
    krb5_fast_req * fast_req = NULL;
    krb5_kdc_req *request = *requestptr;
    krb5_fast_armored_req *fast_armored_req = NULL;
    krb5_checksum *cksum;
    krb5_boolean cksum_valid;
    krb5_keyblock empty_keyblock;

    if (inner_body_out != NULL)
        *inner_body_out = NULL;
    scratch.data = NULL;
    krb5_clear_error_message(context);
    memset(&empty_keyblock, 0, sizeof(krb5_keyblock));
    fast_padata = krb5int_find_pa_data(context, request->padata,
                                       KRB5_PADATA_FX_FAST);
    if (fast_padata !=  NULL){
        scratch.length = fast_padata->length;
        scratch.data = (char *) fast_padata->contents;
        retval = decode_krb5_pa_fx_fast_request(&scratch, &fast_armored_req);
        if (retval == 0 &&fast_armored_req->armor) {
            switch (fast_armored_req->armor->armor_type) {
            case KRB5_FAST_ARMOR_AP_REQUEST:
                if (tgs_subkey) {
                    retval = KRB5KDC_ERR_PREAUTH_FAILED;
                    k5_setmsg(context, retval,
                              _("Ap-request armor not permitted with TGS"));
                    break;
                }
                retval = armor_ap_request(state, fast_armored_req->armor);
                break;
            default:
                k5_setmsg(context, KRB5KDC_ERR_PREAUTH_FAILED,
                          _("Unknown FAST armor type %d"),
                          fast_armored_req->armor->armor_type);
                retval = KRB5KDC_ERR_PREAUTH_FAILED;
            }
        }
        if (retval == 0 && !state->armor_key) {
            if (tgs_subkey)
                retval = krb5_c_fx_cf2_simple(context,
                                              tgs_subkey, "subkeyarmor",
                                              tgs_session, "ticketarmor",
                                              &state->armor_key);
            else {
                retval = KRB5KDC_ERR_PREAUTH_FAILED;
                k5_setmsg(context, retval,
                          _("No armor key but FAST armored request present"));
            }
        }
        if (retval == 0) {
            plaintext.length = fast_armored_req->enc_part.ciphertext.length;
            plaintext.data = k5alloc(plaintext.length, &retval);
        }
        if (retval == 0) {
            retval = krb5_c_decrypt(context, state->armor_key,
                                    KRB5_KEYUSAGE_FAST_ENC, NULL,
                                    &fast_armored_req->enc_part,
                                    &plaintext);
            if (retval == 0)
                retval = decode_krb5_fast_req(&plaintext, &fast_req);
            if (retval == 0 && inner_body_out != NULL) {
                retval = fetch_asn1_field((unsigned char *)plaintext.data,
                                          1, 2, &scratch);
                if (retval == 0) {
                    retval = krb5_copy_data(context, &scratch, &inner_body);
                }
            }
            if (plaintext.data)
                free(plaintext.data);
        }
        cksum = &fast_armored_req->req_checksum;
        if (retval == 0)
            retval = krb5_c_verify_checksum(context, state->armor_key,
                                            KRB5_KEYUSAGE_FAST_REQ_CHKSUM,
                                            checksummed_data, cksum,
                                            &cksum_valid);
        if (retval == 0 && !cksum_valid) {
            retval = KRB5KRB_AP_ERR_MODIFIED;
            k5_setmsg(context, retval,
                      _("FAST req_checksum invalid; request modified"));
        }
        if (retval == 0) {
            if (!krb5_c_is_keyed_cksum(cksum->checksum_type)) {
                retval = KRB5KDC_ERR_POLICY;
                k5_setmsg(context, retval,
                          _("Unkeyed checksum used in fast_req"));
            }
        }
        if (retval == 0) {
            if ((fast_req->fast_options & UNSUPPORTED_CRITICAL_FAST_OPTIONS) != 0)
                retval = KRB5KDC_ERR_UNKNOWN_CRITICAL_FAST_OPTION;
        }
        if (retval == 0) {
            state->fast_options = fast_req->fast_options;
            fast_req->req_body->msg_type = request->msg_type;
            krb5_free_kdc_req(context, request);
            *requestptr = fast_req->req_body;
            fast_req->req_body = NULL;
        }
    }
    if (retval == 0 && inner_body_out != NULL) {
        *inner_body_out = inner_body;
        inner_body = NULL;
    }
    krb5_free_data(context, inner_body);
    if (fast_req)
        krb5_free_fast_req(context, fast_req);
    if (fast_armored_req)
        krb5_free_fast_armored_req(context, fast_armored_req);
    return retval;
}


krb5_error_code
kdc_make_rstate(kdc_realm_t *active_realm, struct kdc_request_state **out)
{
    struct kdc_request_state *state = malloc( sizeof(struct kdc_request_state));
    if (state == NULL)
        return ENOMEM;
    memset( state, 0, sizeof(struct kdc_request_state));
    state->realm_data = active_realm;
    *out = state;
    return 0;
}

void
kdc_free_rstate (struct kdc_request_state *s)
{
    if (s == NULL)
        return;
    if (s->armor_key)
        krb5_free_keyblock(s->realm_data->realm_context, s->armor_key);
    if (s->strengthen_key)
        krb5_free_keyblock(s->realm_data->realm_context, s->strengthen_key);
    k5_zapfree_pa_data(s->in_cookie_padata);
    k5_zapfree_pa_data(s->out_cookie_padata);
    free(s);
}

krb5_error_code
kdc_fast_response_handle_padata(struct kdc_request_state *state,
                                krb5_kdc_req *request,
                                krb5_kdc_rep *rep, krb5_enctype enctype)
{
    krb5_context context = state->realm_data->realm_context;
    krb5_error_code retval = 0;
    krb5_fast_finished finish;
    krb5_fast_response fast_response;
    krb5_data *encoded_ticket = NULL;
    krb5_data *encrypted_reply = NULL;
    krb5_pa_data *pa = NULL, **pa_array = NULL;
    krb5_cksumtype cksumtype = CKSUMTYPE_RSA_MD5;
    krb5_pa_data *empty_padata[] = {NULL};
    krb5_keyblock *strengthen_key = NULL;

    if (!state->armor_key)
        return 0;
    memset(&finish, 0, sizeof(finish));
    retval = krb5_init_keyblock(context, enctype, 0, &strengthen_key);
    if (retval == 0)
        retval = krb5_c_make_random_key(context, enctype, strengthen_key);
    if (retval == 0) {
        state->strengthen_key = strengthen_key;
        strengthen_key = NULL;
    }

    fast_response.padata = rep->padata;
    if (fast_response.padata == NULL)
        fast_response.padata = &empty_padata[0];
    fast_response.strengthen_key = state->strengthen_key;
    fast_response.nonce = request->nonce;
    fast_response.finished = &finish;
    finish.client = rep->client;
    pa_array = calloc(3, sizeof(*pa_array));
    if (pa_array == NULL)
        retval = ENOMEM;
    pa = calloc(1, sizeof(krb5_pa_data));
    if (retval == 0 && pa == NULL)
        retval = ENOMEM;
    if (retval == 0)
        retval = krb5_us_timeofday(context, &finish.timestamp, &finish.usec);
    if (retval == 0)
        retval = encode_krb5_ticket(rep->ticket, &encoded_ticket);
    if (retval == 0)
        retval = krb5int_c_mandatory_cksumtype(context,
                                               state->armor_key->enctype,
                                               &cksumtype);
    if (retval == 0)
        retval = krb5_c_make_checksum(context, cksumtype, state->armor_key,
                                      KRB5_KEYUSAGE_FAST_FINISHED,
                                      encoded_ticket, &finish.ticket_checksum);
    if (retval == 0)
        retval = encrypt_fast_reply(state, &fast_response, &encrypted_reply);
    if (retval == 0) {
        pa[0].pa_type = KRB5_PADATA_FX_FAST;
        pa[0].length = encrypted_reply->length;
        pa[0].contents = (unsigned char *)  encrypted_reply->data;
        pa_array[0] = &pa[0];
        krb5_free_pa_data(context, rep->padata);
        rep->padata = pa_array;
        pa_array = NULL;
        free(encrypted_reply);
        encrypted_reply = NULL;
        pa = NULL;
    }
    if (pa)
        free(pa);
    if (pa_array)
        free(pa_array);
    if (encrypted_reply)
        krb5_free_data(context, encrypted_reply);
    if (encoded_ticket)
        krb5_free_data(context, encoded_ticket);
    if (strengthen_key != NULL)
        krb5_free_keyblock(context, strengthen_key);
    if (finish.ticket_checksum.contents)
        krb5_free_checksum_contents(context, &finish.ticket_checksum);
    return retval;
}


/*
 * We assume the caller is responsible for passing us an in_padata
 * sufficient to include in a FAST error.  In the FAST case we will
 * set *fast_edata_out to the edata to be included in the error; in
 * the non-FAST case we will set it to NULL.
 */
krb5_error_code
kdc_fast_handle_error(krb5_context context,
                      struct kdc_request_state *state,
                      krb5_kdc_req *request,
                      krb5_pa_data  **in_padata, krb5_error *err,
                      krb5_data **fast_edata_out)
{
    krb5_error_code retval = 0;
    krb5_fast_response resp;
    krb5_error fx_error;
    krb5_data *encoded_fx_error = NULL, *encrypted_reply = NULL;
    krb5_pa_data pa[1];
    krb5_pa_data *outer_pa[3];
    krb5_pa_data **inner_pa = NULL;
    size_t size = 0;

    *fast_edata_out = NULL;
    memset(outer_pa, 0, sizeof(outer_pa));
    if (state->armor_key == NULL)
        return 0;
    fx_error = *err;
    fx_error.e_data.data = NULL;
    fx_error.e_data.length = 0;
    for (size = 0; in_padata&&in_padata[size]; size++);
    inner_pa = calloc(size + 2, sizeof(krb5_pa_data *));
    if (inner_pa == NULL)
        retval = ENOMEM;
    if (retval == 0)
        for (size=0; in_padata&&in_padata[size]; size++)
            inner_pa[size] = in_padata[size];
    if (retval == 0)
        retval = encode_krb5_error(&fx_error, &encoded_fx_error);
    if (retval == 0) {
        pa[0].pa_type = KRB5_PADATA_FX_ERROR;
        pa[0].length = encoded_fx_error->length;
        pa[0].contents = (unsigned char *) encoded_fx_error->data;
        inner_pa[size++] = &pa[0];
    }
    if (retval == 0) {
        resp.padata = inner_pa;
        resp.nonce = request->nonce;
        resp.strengthen_key = NULL;
        resp.finished = NULL;
    }
    if (retval == 0)
        retval = encrypt_fast_reply(state, &resp, &encrypted_reply);
    if (inner_pa)
        free(inner_pa); /*contained storage from caller and our stack*/
    if (retval == 0) {
        pa[0].pa_type = KRB5_PADATA_FX_FAST;
        pa[0].length = encrypted_reply->length;
        pa[0].contents = (unsigned char *) encrypted_reply->data;
        outer_pa[0] = &pa[0];
    }
    retval = encode_krb5_padata_sequence(outer_pa, fast_edata_out);
    if (encrypted_reply)
        krb5_free_data(context, encrypted_reply);
    if (encoded_fx_error)
        krb5_free_data(context, encoded_fx_error);
    return retval;
}

krb5_error_code
kdc_fast_handle_reply_key(struct kdc_request_state *state,
                          krb5_keyblock *existing_key,
                          krb5_keyblock **out_key)
{
    krb5_context context = state->realm_data->realm_context;
    krb5_error_code retval = 0;

    if (state->armor_key)
        retval = krb5_c_fx_cf2_simple(context,
                                      state->strengthen_key, "strengthenkey",
                                      existing_key, "replykey", out_key);
    else
        retval = krb5_copy_keyblock(context, existing_key, out_key);
    return retval;
}

krb5_boolean
kdc_fast_hide_client(struct kdc_request_state *state)
{
    return (state->fast_options & KRB5_FAST_OPTION_HIDE_CLIENT_NAMES) != 0;
}

/* Create a pa-data entry with the specified type and contents. */
static krb5_error_code
make_padata(krb5_preauthtype pa_type, const void *contents, size_t len,
            krb5_pa_data **out)
{
    if (k5_alloc_pa_data(pa_type, len, out) != 0)
        return ENOMEM;
    memcpy((*out)->contents, contents, len);
    return 0;
}

/*
 * Derive the secure cookie encryption key from tgt_key and client_princ.  The
 * cookie key is derived with PRF+ using the concatenation of "COOKIE" and the
 * unparsed client principal name as input.
 */
static krb5_error_code
derive_cookie_key(krb5_context context, krb5_keyblock *tgt_key,
                  krb5_const_principal client_princ, krb5_keyblock **key_out)
{
    krb5_error_code ret;
    krb5_data d;
    char *princstr = NULL, *derive_input = NULL;

    *key_out = NULL;

    /* Construct the input string and derive the cookie key. */
    ret = krb5_unparse_name(context, client_princ, &princstr);
    if (ret)
        goto cleanup;
    if (asprintf(&derive_input, "COOKIE%s", princstr) < 0) {
        ret = ENOMEM;
        goto cleanup;
    }
    d = string2data(derive_input);
    ret = krb5_c_derive_prfplus(context, tgt_key, &d, ENCTYPE_NULL, key_out);

cleanup:
    krb5_free_unparsed_name(context, princstr);
    free(derive_input);
    return ret;
}

/* Derive the cookie key for the specified kvno in tgt.  tgt_key must be the
 * decrypted first key data entry in tgt. */
static krb5_error_code
get_cookie_key(krb5_context context, krb5_db_entry *tgt,
               krb5_keyblock *tgt_key, krb5_kvno kvno,
               krb5_const_principal client_princ, krb5_keyblock **key_out)
{
    krb5_error_code ret;
    krb5_keyblock storage, *key;
    krb5_key_data *kd;

    *key_out = NULL;
    memset(&storage, 0, sizeof(storage));

    if (kvno == current_kvno(tgt)) {
        /* Use the already-decrypted first key. */
        key = tgt_key;
    } else {
        /* The cookie used an older TGT key; find and decrypt it. */
        ret = krb5_dbe_find_enctype(context, tgt, -1, -1, kvno, &kd);
        if (ret)
            return ret;
        ret = krb5_dbe_decrypt_key_data(context, NULL, kd, &storage, NULL);
        if (ret)
            return ret;
        key = &storage;
    }

    ret = derive_cookie_key(context, key, client_princ, key_out);
    krb5_free_keyblock_contents(context, &storage);
    return ret;
}

/* Return true if there is any overlap between padata types in cpadata
 * (from the cookie) and rpadata (from the request). */
static krb5_boolean
is_relevant(krb5_pa_data *const *cpadata, krb5_pa_data *const *rpadata)
{
    krb5_pa_data *const *p;

    for (p = cpadata; p != NULL && *p != NULL; p++) {
        if (krb5int_find_pa_data(NULL, rpadata, (*p)->pa_type) != NULL)
            return TRUE;
    }
    return FALSE;
}

/*
 * Locate and decode the FAST cookie in req, storing its contents in state for
 * later access by preauth modules.  If the cookie is expired, return
 * KRB5KDC_ERR_PREAUTH_EXPIRED if its contents are relevant to req, and ignore
 * it if they aren't.
 */
krb5_error_code
kdc_fast_read_cookie(krb5_context context, struct kdc_request_state *state,
                     krb5_kdc_req *req, krb5_db_entry *local_tgt,
                     krb5_keyblock *local_tgt_key)
{
    krb5_error_code ret;
    krb5_secure_cookie *cookie = NULL;
    krb5_timestamp now;
    krb5_keyblock *key = NULL;
    krb5_enc_data enc;
    krb5_pa_data *pa;
    krb5_kvno kvno;
    krb5_data plain = empty_data();

    pa = krb5int_find_pa_data(context, req->padata, KRB5_PADATA_FX_COOKIE);
    if (pa == NULL)
        return 0;

    /* If it's not an MIT version 1 cookie, ignore it.  It may be an empty
     * "MIT" cookie or a cookie generated by a different KDC implementation. */
    if (pa->length <= 8 || memcmp(pa->contents, "MIT1", 4) != 0)
        return 0;

    /* Extract the kvno and generate the corresponding cookie key. */
    kvno = load_32_be(pa->contents + 4);
    ret = get_cookie_key(context, local_tgt, local_tgt_key, kvno, req->client,
                         &key);
    if (ret)
        goto cleanup;

    /* Decrypt and decode the cookie. */
    memset(&enc, 0, sizeof(enc));
    enc.enctype = key->enctype;
    enc.ciphertext = make_data(pa->contents + 8, pa->length - 8);
    ret = alloc_data(&plain, pa->length - 8);
    if (ret)
        goto cleanup;
    ret = krb5_c_decrypt(context, key, KRB5_KEYUSAGE_PA_FX_COOKIE, NULL, &enc,
                         &plain);
    if (ret)
        goto cleanup;
    ret = decode_krb5_secure_cookie(&plain, &cookie);
    if (ret)
        goto cleanup;

    /* Check if the cookie is expired. */
    ret = krb5_timeofday(context, &now);
    if (ret)
        goto cleanup;
    if (ts2tt(now) > cookie->time + COOKIE_LIFETIME) {
        /* Don't accept the cookie contents.  Only return an error if the
         * cookie is relevant to the request. */
        if (is_relevant(cookie->data, req->padata))
            ret = KRB5KDC_ERR_PREAUTH_EXPIRED;
        goto cleanup;
    }

    /* Steal the pa-data list pointer from the cookie and store it in state. */
    state->in_cookie_padata = cookie->data;
    cookie->data = NULL;

cleanup:
    zapfree(plain.data, plain.length);
    krb5_free_keyblock(context, key);
    k5_free_secure_cookie(context, cookie);
    return 0;
}

/* If state contains a cookie value for pa_type, set *out to the corresponding
 * data and return true.  Otherwise set *out to empty and return false. */
krb5_boolean
kdc_fast_search_cookie(struct kdc_request_state *state,
                       krb5_preauthtype pa_type, krb5_data *out)
{
    krb5_pa_data *pa;

    pa = krb5int_find_pa_data(NULL, state->in_cookie_padata, pa_type);
    if (pa == NULL) {
        *out = empty_data();
        return FALSE;
    } else {
        *out = make_data(pa->contents, pa->length);
        return TRUE;
    }
}

/* Set a cookie value in state for data, to be included in the outgoing
 * cookie.  Duplicate values are ignored. */
krb5_error_code
kdc_fast_set_cookie(struct kdc_request_state *state, krb5_preauthtype pa_type,
                    const krb5_data *data)
{
    krb5_pa_data **list = state->out_cookie_padata;
    size_t count;

    for (count = 0; list != NULL && list[count] != NULL; count++) {
        if (list[count]->pa_type == pa_type)
            return 0;
    }

    list = realloc(list, (count + 2) * sizeof(*list));
    if (list == NULL)
        return ENOMEM;
    state->out_cookie_padata = list;
    list[count] = list[count + 1] = NULL;
    return make_padata(pa_type, data->data, data->length, &list[count]);
}

/* Construct a cookie pa-data item using the cookie values from state, or a
 * trivial "MIT" cookie if no values are set. */
krb5_error_code
kdc_fast_make_cookie(krb5_context context, struct kdc_request_state *state,
                     krb5_db_entry *local_tgt, krb5_keyblock *local_tgt_key,
                     krb5_const_principal client_princ,
                     krb5_pa_data **cookie_out)
{
    krb5_error_code ret;
    krb5_secure_cookie cookie;
    krb5_pa_data **contents = state->out_cookie_padata, *pa;
    krb5_keyblock *key = NULL;
    krb5_timestamp now;
    krb5_enc_data enc;
    krb5_data *der_cookie = NULL;
    size_t ctlen;

    *cookie_out = NULL;
    memset(&enc, 0, sizeof(enc));

    /* Make a trivial cookie if there are no contents to marshal or we don't
     * have a TGT entry to encrypt them. */
    if (contents == NULL || *contents == NULL || local_tgt_key == NULL)
        return make_padata(KRB5_PADATA_FX_COOKIE, "MIT", 3, cookie_out);

    ret = derive_cookie_key(context, local_tgt_key, client_princ, &key);
    if (ret)
        goto cleanup;

    /* Encode the cookie. */
    ret = krb5_timeofday(context, &now);
    if (ret)
        goto cleanup;
    cookie.time = ts2tt(now);
    cookie.data = contents;
    ret = encode_krb5_secure_cookie(&cookie, &der_cookie);
    if (ret)
        goto cleanup;

    /* Encrypt the cookie in key. */
    ret = krb5_c_encrypt_length(context, key->enctype, der_cookie->length,
                                &ctlen);
    if (ret)
        goto cleanup;
    ret = alloc_data(&enc.ciphertext, ctlen);
    if (ret)
        goto cleanup;
    ret = krb5_c_encrypt(context, key, KRB5_KEYUSAGE_PA_FX_COOKIE, NULL,
                         der_cookie, &enc);
    if (ret)
        goto cleanup;

    /* Construct the cookie pa-data entry. */
    ret = k5_alloc_pa_data(KRB5_PADATA_FX_COOKIE, 8 + enc.ciphertext.length,
                           &pa);
    memcpy(pa->contents, "MIT1", 4);
    store_32_be(current_kvno(local_tgt), pa->contents + 4);
    memcpy(pa->contents + 8, enc.ciphertext.data, enc.ciphertext.length);
    *cookie_out = pa;

cleanup:
    krb5_free_keyblock(context, key);
    if (der_cookie != NULL) {
        zapfree(der_cookie->data, der_cookie->length);
        free(der_cookie);
    }
    krb5_free_data_contents(context, &enc.ciphertext);
    return ret;
}
