/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* kdc/kdc_util.c - Utility functions for the KDC implementation */
/*
 * Copyright 1990,1991,2007,2008,2009 by the Massachusetts Institute of Technology.
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
 * Copyright (c) 2006-2008, Novell, Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *   * Redistributions of source code must retain the above copyright notice,
 *       this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *   * The copyright holder's name is not used to endorse or promote products
 *       derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include "k5-int.h"
#include "kdc_util.h"
#include "extern.h"
#include <stdio.h>
#include <ctype.h>
#include <syslog.h>
#include <kadm5/admin.h>
#include "adm_proto.h"
#include "net-server.h"
#include <limits.h>

#ifdef KRBCONF_VAGUE_ERRORS
const int vague_errors = 1;
#else
const int vague_errors = 0;
#endif

static krb5_error_code kdc_rd_ap_req(kdc_realm_t *realm, krb5_ap_req *apreq,
                                     krb5_auth_context auth_context,
                                     krb5_db_entry **server,
                                     krb5_keyblock **tgskey);
static krb5_error_code find_server_key(krb5_context,
                                       krb5_db_entry *, krb5_enctype,
                                       krb5_kvno, krb5_keyblock **,
                                       krb5_kvno *);

/*
 * Returns TRUE if the kerberos principal is the name of a Kerberos ticket
 * service.
 */
krb5_boolean
krb5_is_tgs_principal(krb5_const_principal principal)
{
    if (krb5_princ_size(kdc_context, principal) != 2)
        return FALSE;
    if (data_eq_string(*krb5_princ_component(kdc_context, principal, 0),
                       KRB5_TGS_NAME))
        return TRUE;
    else
        return FALSE;
}

/* Returns TRUE if principal is the name of a cross-realm TGS. */
krb5_boolean
is_cross_tgs_principal(krb5_const_principal principal)
{
    return krb5_is_tgs_principal(principal) &&
        !data_eq(principal->data[1], principal->realm);
}

/* Return true if princ is the name of a local TGS for any realm. */
krb5_boolean
is_local_tgs_principal(krb5_const_principal principal)
{
    return krb5_is_tgs_principal(principal) &&
        data_eq(principal->data[1], principal->realm);
}

/*
 * given authentication data (provides seed for checksum), verify checksum
 * for source data.
 */
static krb5_error_code
comp_cksum(krb5_context kcontext, krb5_data *source, krb5_ticket *ticket,
           krb5_checksum *his_cksum)
{
    krb5_error_code       retval;
    krb5_boolean          valid;

    if (!krb5_c_valid_cksumtype(his_cksum->checksum_type))
        return KRB5KDC_ERR_SUMTYPE_NOSUPP;

    /* must be collision proof */
    if (!krb5_c_is_coll_proof_cksum(his_cksum->checksum_type))
        return KRB5KRB_AP_ERR_INAPP_CKSUM;

    /* verify checksum */
    if ((retval = krb5_c_verify_checksum(kcontext, ticket->enc_part2->session,
                                         KRB5_KEYUSAGE_TGS_REQ_AUTH_CKSUM,
                                         source, his_cksum, &valid)))
        return(retval);

    if (!valid)
        return(KRB5KRB_AP_ERR_BAD_INTEGRITY);

    return(0);
}

/* If a header ticket is decrypted, *ticket_out is filled in even on error. */
krb5_error_code
kdc_process_tgs_req(kdc_realm_t *realm, krb5_kdc_req *request,
                    const krb5_fulladdr *from, krb5_data *pkt,
                    krb5_ticket **ticket_out, krb5_db_entry **krbtgt_ptr,
                    krb5_keyblock **tgskey, krb5_keyblock **subkey,
                    krb5_pa_data **pa_tgs_req)
{
    krb5_context context = realm->realm_context;
    krb5_pa_data        * tmppa;
    krb5_ap_req         * apreq;
    krb5_error_code       retval;
    krb5_authdata **authdata = NULL;
    krb5_data             scratch1;
    krb5_data           * scratch = NULL;
    krb5_auth_context     auth_context = NULL;
    krb5_authenticator  * authenticator = NULL;
    krb5_checksum       * his_cksum = NULL;
    krb5_db_entry       * krbtgt = NULL;
    krb5_ticket         * ticket;

    *ticket_out = NULL;
    *krbtgt_ptr = NULL;
    *tgskey = NULL;

    tmppa = krb5int_find_pa_data(context, request->padata, KRB5_PADATA_AP_REQ);
    if (!tmppa)
        return KRB5KDC_ERR_PADATA_TYPE_NOSUPP;

    scratch1.length = tmppa->length;
    scratch1.data = (char *)tmppa->contents;
    if ((retval = decode_krb5_ap_req(&scratch1, &apreq)))
        return retval;
    ticket = apreq->ticket;

    if (isflagset(apreq->ap_options, AP_OPTS_USE_SESSION_KEY) ||
        isflagset(apreq->ap_options, AP_OPTS_MUTUAL_REQUIRED)) {
        krb5_klog_syslog(LOG_INFO, _("TGS_REQ: SESSION KEY or MUTUAL"));
        retval = KRB5KDC_ERR_POLICY;
        goto cleanup;
    }

    retval = krb5_auth_con_init(context, &auth_context);
    if (retval)
        goto cleanup;

    /* Don't use a replay cache. */
    retval = krb5_auth_con_setflags(context, auth_context, 0);
    if (retval)
        goto cleanup;

    retval = krb5_auth_con_setaddrs(context, auth_context, NULL,
                                    from->address);
    if (retval)
        goto cleanup_auth_context;

    retval = kdc_rd_ap_req(realm, apreq, auth_context, &krbtgt, tgskey);
    if (retval)
        goto cleanup_auth_context;

    retval = krb5_auth_con_getrecvsubkey(context, auth_context, subkey);
    if (retval)
        goto cleanup_auth_context;

    retval = krb5_auth_con_getauthenticator(context, auth_context,
                                            &authenticator);
    if (retval)
        goto cleanup_auth_context;

    retval = krb5_find_authdata(context, ticket->enc_part2->authorization_data,
                                authenticator->authorization_data,
                                KRB5_AUTHDATA_FX_ARMOR, &authdata);
    if (retval != 0)
        goto cleanup_authenticator;
    if (authdata&& authdata[0]) {
        k5_setmsg(context, KRB5KDC_ERR_POLICY,
                  "ticket valid only as FAST armor");
        retval = KRB5KDC_ERR_POLICY;
        krb5_free_authdata(context, authdata);
        goto cleanup_authenticator;
    }
    krb5_free_authdata(context, authdata);


    /* Check for a checksum */
    if (!(his_cksum = authenticator->checksum)) {
        retval = KRB5KRB_AP_ERR_INAPP_CKSUM;
        goto cleanup_authenticator;
    }

    /*
     * Check application checksum vs. tgs request
     *
     * We try checksumming the req-body two different ways: first we
     * try reaching into the raw asn.1 stream (if available), and
     * checksum that directly; if that fails, then we try encoding
     * using our local asn.1 library.
     */
    if (pkt && (fetch_asn1_field((unsigned char *) pkt->data,
                                 1, 4, &scratch1) >= 0)) {
        if (comp_cksum(context, &scratch1, ticket, his_cksum)) {
            if (!(retval = encode_krb5_kdc_req_body(request, &scratch)))
                retval = comp_cksum(context, scratch, ticket, his_cksum);
            krb5_free_data(context, scratch);
            if (retval)
                goto cleanup_authenticator;
        }
    }

    *pa_tgs_req = tmppa;
    *krbtgt_ptr = krbtgt;
    krbtgt = NULL;

cleanup_authenticator:
    krb5_free_authenticator(context, authenticator);

cleanup_auth_context:
    krb5_auth_con_free(context, auth_context);

cleanup:
    if (retval != 0) {
        krb5_free_keyblock(context, *tgskey);
        *tgskey = NULL;
    }
    if (apreq->ticket->enc_part2 != NULL) {
        /* Steal the decrypted ticket pointer, even on error. */
        *ticket_out = apreq->ticket;
        apreq->ticket = NULL;
    }
    krb5_free_ap_req(context, apreq);
    krb5_db_free_principal(context, krbtgt);
    return retval;
}

/*
 * This is a KDC wrapper around krb5_rd_req_decoded_anyflag().
 *
 * We can't depend on KDB-as-keytab for handling the AP-REQ here for
 * optimization reasons: we want to minimize the number of KDB lookups.  We'll
 * need the KDB entry for the TGS principal, and the TGS key used to decrypt
 * the TGT, elsewhere in the TGS code.
 *
 * This function also implements key rollover support for kvno 0 cross-realm
 * TGTs issued by AD.
 */
static
krb5_error_code
kdc_rd_ap_req(kdc_realm_t *realm, krb5_ap_req *apreq,
              krb5_auth_context auth_context, krb5_db_entry **server,
              krb5_keyblock **tgskey)
{
    krb5_context context = realm->realm_context;
    krb5_error_code     retval;
    krb5_enctype        search_enctype = apreq->ticket->enc_part.enctype;
    krb5_boolean        match_enctype = 1;
    krb5_kvno           kvno;
    size_t              tries = 3;

    /*
     * When we issue tickets we use the first key in the principals' highest
     * kvno keyset.  For non-cross-realm krbtgt principals we want to only
     * allow the use of the first key of the principal's keyset that matches
     * the given kvno.
     */
    if (krb5_is_tgs_principal(apreq->ticket->server) &&
        !is_cross_tgs_principal(apreq->ticket->server)) {
        search_enctype = -1;
        match_enctype = 0;
    }

    retval = kdc_get_server_key(context, apreq->ticket, 0, match_enctype,
                                server, NULL, NULL);
    if (retval)
        return retval;

    *tgskey = NULL;
    kvno = apreq->ticket->enc_part.kvno;
    do {
        krb5_free_keyblock(context, *tgskey);
        retval = find_server_key(context, *server, search_enctype, kvno,
                                 tgskey, &kvno);
        if (retval)
            continue;

        /* Make the TGS key available to krb5_rd_req_decoded_anyflag() */
        retval = krb5_auth_con_setuseruserkey(context, auth_context, *tgskey);
        if (retval)
            return retval;

        retval = krb5_rd_req_decoded_anyflag(context, &auth_context, apreq,
                                             apreq->ticket->server,
                                             realm->realm_keytab, NULL, NULL);

        /* If the ticket was decrypted, don't try any more keys. */
        if (apreq->ticket->enc_part2 != NULL)
            break;

    } while (retval && apreq->ticket->enc_part.kvno == 0 && kvno-- > 1 &&
             --tries > 0);

    return retval;
}

/*
 * The KDC should take the keytab associated with the realm and pass
 * that to the krb5_rd_req_decoded_anyflag(), but we still need to use
 * the service (TGS, here) key elsewhere.  This approach is faster than
 * the KDB keytab approach too.
 *
 * This is also used by do_tgs_req() for u2u auth.
 */
krb5_error_code
kdc_get_server_key(krb5_context context,
                   krb5_ticket *ticket, unsigned int flags,
                   krb5_boolean match_enctype, krb5_db_entry **server_ptr,
                   krb5_keyblock **key, krb5_kvno *kvno)
{
    krb5_error_code       retval;
    krb5_db_entry       * server = NULL;
    krb5_enctype          search_enctype = -1;
    krb5_kvno             search_kvno = -1;

    if (match_enctype)
        search_enctype = ticket->enc_part.enctype;
    if (ticket->enc_part.kvno)
        search_kvno = ticket->enc_part.kvno;

    *server_ptr = NULL;

    retval = krb5_db_get_principal(context, ticket->server, flags,
                                   &server);
    if (retval == KRB5_KDB_NOENTRY) {
        char *sname;
        if (!krb5_unparse_name(context, ticket->server, &sname)) {
            limit_string(sname);
            krb5_klog_syslog(LOG_ERR,
                             _("TGS_REQ: UNKNOWN SERVER: server='%s'"), sname);
            free(sname);
        }
        return KRB5KDC_ERR_S_PRINCIPAL_UNKNOWN;
    } else if (retval)
        return retval;
    if (server->attributes & KRB5_KDB_DISALLOW_SVR ||
        server->attributes & KRB5_KDB_DISALLOW_ALL_TIX) {
        retval = KRB5KDC_ERR_S_PRINCIPAL_UNKNOWN;
        goto errout;
    }

    if (key) {
        retval = find_server_key(context, server, search_enctype, search_kvno,
                                 key, kvno);
        if (retval)
            goto errout;
    }
    *server_ptr = server;
    server = NULL;
    return 0;

errout:
    krb5_db_free_principal(context, server);
    return retval;
}

/*
 * A utility function to get the right key from a KDB entry.  Used in handling
 * of kvno 0 TGTs, for example.
 */
static
krb5_error_code
find_server_key(krb5_context context,
                krb5_db_entry *server, krb5_enctype enctype, krb5_kvno kvno,
                krb5_keyblock **key_out, krb5_kvno *kvno_out)
{
    krb5_error_code       retval;
    krb5_key_data       * server_key;
    krb5_keyblock       * key;

    *key_out = NULL;
    retval = krb5_dbe_find_enctype(context, server, enctype, -1,
                                   kvno ? (krb5_int32)kvno : -1, &server_key);
    if (retval)
        return retval;
    if (!server_key)
        return KRB5KDC_ERR_S_PRINCIPAL_UNKNOWN;
    if ((key = (krb5_keyblock *)malloc(sizeof *key)) == NULL)
        return ENOMEM;
    retval = krb5_dbe_decrypt_key_data(context, NULL, server_key,
                                       key, NULL);
    if (retval)
        goto errout;
    if (enctype != -1) {
        krb5_boolean similar;
        retval = krb5_c_enctype_compare(context, enctype, key->enctype,
                                        &similar);
        if (retval)
            goto errout;
        if (!similar) {
            retval = KRB5_KDB_NO_PERMITTED_KEY;
            goto errout;
        }
        key->enctype = enctype;
    }
    *key_out = key;
    key = NULL;
    if (kvno_out)
        *kvno_out = server_key->key_data_kvno;
errout:
    krb5_free_keyblock(context, key);
    return retval;
}

/* Find the first key data entry (of a valid enctype) of the highest kvno in
 * entry, and decrypt it into *key_out. */
krb5_error_code
get_first_current_key(krb5_context context, krb5_db_entry *entry,
                      krb5_keyblock *key_out)
{
    krb5_error_code ret;
    krb5_key_data *kd;

    memset(key_out, 0, sizeof(*key_out));
    ret = krb5_dbe_find_enctype(context, entry, -1, -1, 0, &kd);
    if (ret)
        return ret;
    return krb5_dbe_decrypt_key_data(context, NULL, kd, key_out, NULL);
}

/*
 * If candidate is the local TGT for realm, set *alias_out to candidate and
 * *storage_out to NULL.  Otherwise, load the local TGT into *storage_out and
 * set *alias_out to *storage_out.  In either case, set *key_out to the
 * decrypted first key of the local TGT.
 *
 * In the future we might generalize this to a small per-request principal
 * cache.  For now, it saves a load operation in the common case where the AS
 * server or TGS header ticket server is the local TGT.
 */
krb5_error_code
get_local_tgt(krb5_context context, const krb5_data *realm,
              krb5_db_entry *candidate, krb5_db_entry **alias_out,
              krb5_db_entry **storage_out, krb5_keyblock *key_out)
{
    krb5_error_code ret;
    krb5_principal princ;
    krb5_db_entry *storage = NULL, *tgt;

    *alias_out = NULL;
    *storage_out = NULL;
    memset(key_out, 0, sizeof(*key_out));

    ret = krb5_build_principal_ext(context, &princ, realm->length, realm->data,
                                   KRB5_TGS_NAME_SIZE, KRB5_TGS_NAME,
                                   realm->length, realm->data, 0);
    if (ret)
        goto cleanup;

    if (!krb5_principal_compare(context, candidate->princ, princ)) {
        ret = krb5_db_get_principal(context, princ, 0, &storage);
        if (ret)
            goto cleanup;
        tgt = storage;
    } else {
        tgt = candidate;
    }

    ret = get_first_current_key(context, tgt, key_out);
    if (ret)
        goto cleanup;

    *alias_out = tgt;
    *storage_out = storage;
    storage = NULL;

cleanup:
    krb5_db_free_principal(context, storage);
    krb5_free_principal(context, princ);
    return ret;
}

/* If server has a pac_privsvr_enctype attribute and it differs from tgt_key's
 * enctype, derive a key of the specified enctype.  Otherwise copy tgt_key. */
krb5_error_code
pac_privsvr_key(krb5_context context, krb5_db_entry *server,
                const krb5_keyblock *tgt_key, krb5_keyblock **key_out)
{
    krb5_error_code ret;
    char *attrval = NULL;
    krb5_enctype privsvr_enctype;
    krb5_data prf_input = string2data("pac_privsvr");

    ret = krb5_dbe_get_string(context, server, KRB5_KDB_SK_PAC_PRIVSVR_ENCTYPE,
                              &attrval);
    if (ret)
        return ret;
    if (attrval == NULL)
        return krb5_copy_keyblock(context, tgt_key, key_out);

    ret = krb5_string_to_enctype(attrval, &privsvr_enctype);
    if (ret) {
        k5_setmsg(context, ret, _("Invalid pac_privsvr_enctype value %s"),
                  attrval);
        goto cleanup;
    }

    if (tgt_key->enctype == privsvr_enctype) {
        ret = krb5_copy_keyblock(context, tgt_key, key_out);
    } else {
        ret = krb5_c_derive_prfplus(context, tgt_key, &prf_input,
                                    privsvr_enctype, key_out);
    }

cleanup:
    krb5_dbe_free_string(context, attrval);
    return ret;
}

/* Try verifying a ticket's PAC using a privsvr key either equal to or derived
 * from tgt_key, respecting the server's pac_privsvr_enctype value if set. */
static krb5_error_code
try_verify_pac(krb5_context context, const krb5_enc_tkt_part *enc_tkt,
               krb5_db_entry *server, krb5_keyblock *server_key,
               const krb5_keyblock *tgt_key, krb5_pac *pac_out)
{
    krb5_error_code ret;
    krb5_keyblock *privsvr_key;

    ret = pac_privsvr_key(context, server, tgt_key, &privsvr_key);
    if (ret)
        return ret;
    ret = krb5_kdc_verify_ticket(context, enc_tkt, server->princ, server_key,
                                 privsvr_key, pac_out);
    krb5_free_keyblock(context, privsvr_key);
    return ret;
}

/*
 * If a PAC is present in enc_tkt, verify it and place it in *pac_out.  sprinc
 * is the canonical name of the server principal entry used to decrypt enc_tkt.
 * server_key is the ticket decryption key.  tgt is the local krbtgt entry for
 * the ticket server realm, and tgt_key is its first key.
 */
krb5_error_code
get_verified_pac(krb5_context context, const krb5_enc_tkt_part *enc_tkt,
                 krb5_db_entry *server, krb5_keyblock *server_key,
                 krb5_db_entry *tgt, krb5_keyblock *tgt_key, krb5_pac *pac_out)
{
    krb5_error_code ret;
    krb5_key_data *kd;
    krb5_keyblock old_key;
    krb5_kvno kvno;
    int tries;

    *pac_out = NULL;

    /* For local or cross-realm TGTs we only check the server signature. */
    if (krb5_is_tgs_principal(server->princ)) {
        return krb5_kdc_verify_ticket(context, enc_tkt, server->princ,
                                      server_key, NULL, pac_out);
    }

    ret = try_verify_pac(context, enc_tkt, server, server_key, tgt_key,
                         pac_out);
    if (ret != KRB5KRB_AP_ERR_MODIFIED && ret != KRB5_BAD_ENCTYPE)
        return ret;

    /* There is no kvno in PAC signatures, so try two previous versions. */
    kvno = tgt->key_data[0].key_data_kvno - 1;
    for (tries = 2; tries > 0 && kvno > 0; tries--, kvno--) {
        ret = krb5_dbe_find_enctype(context, tgt, -1, -1, kvno, &kd);
        if (ret)
            return KRB5KRB_AP_ERR_MODIFIED;
        ret = krb5_dbe_decrypt_key_data(context, NULL, kd, &old_key, NULL);
        if (ret)
            return ret;
        ret = try_verify_pac(context, enc_tkt, server, server_key, &old_key,
                             pac_out);
        krb5_free_keyblock_contents(context, &old_key);
        if (!ret)
            return 0;
    }

    return KRB5KRB_AP_ERR_MODIFIED;
}

/*
 * Fetch the client info from pac and parse it into a principal name, expecting
 * a realm in the string.  Set *authtime_out to the client info authtime if it
 * is not null.
 */
krb5_error_code
get_pac_princ_with_realm(krb5_context context, krb5_pac pac,
                         krb5_principal *princ_out,
                         krb5_timestamp *authtime_out)
{
    krb5_error_code ret;
    int n_atsigns, flags = KRB5_PRINCIPAL_PARSE_REQUIRE_REALM;
    char *client_str = NULL;
    const char *p;

    *princ_out = NULL;

    ret = krb5_pac_get_client_info(context, pac, authtime_out, &client_str);
    if (ret)
        return ret;

    n_atsigns = 0;
    for (p = client_str; *p != '\0'; p++) {
        if (*p == '@')
            n_atsigns++;
    }

    if (n_atsigns == 2) {
        flags |= KRB5_PRINCIPAL_PARSE_ENTERPRISE;
    } else if (n_atsigns != 1) {
        ret = KRB5_PARSE_MALFORMED;
        goto cleanup;
    }

    ret = krb5_parse_name_flags(context, client_str, flags, princ_out);
    if (ret)
        return ret;

    (*princ_out)->type = KRB5_NT_MS_PRINCIPAL;

cleanup:
    free(client_str);
    return 0;
}

/* This probably wants to be updated if you support last_req stuff */

static krb5_last_req_entry nolrentry = { KV5M_LAST_REQ_ENTRY, KRB5_LRQ_NONE, 0 };
static krb5_last_req_entry *nolrarray[] = { &nolrentry, 0 };

krb5_error_code
fetch_last_req_info(krb5_db_entry *dbentry, krb5_last_req_entry ***lrentry)
{
    *lrentry = nolrarray;
    return 0;
}


/* Convert an API error code to a protocol error code. */
int
errcode_to_protocol(krb5_error_code code)
{
    int protcode;

    protcode = code - ERROR_TABLE_BASE_krb5;
    return (protcode >= 0 && protcode <= 128) ? protcode : KRB_ERR_GENERIC;
}

/* Return -1 if the AS or TGS request is disallowed due to KDC policy on
 * anonymous tickets. */
int
check_anon(kdc_realm_t *realm, krb5_principal client, krb5_principal server)
{
    /* If restrict_anon is set, reject requests from anonymous clients to
     * server principals other than local TGTs. */
    if (realm->realm_restrict_anon &&
        krb5_principal_compare_any_realm(realm->realm_context, client,
                                         krb5_anonymous_principal()) &&
        !is_local_tgs_principal(server))
        return -1;
    return 0;
}

krb5_error_code
validate_as_request(kdc_realm_t *realm, krb5_kdc_req *request,
                    krb5_db_entry *client, krb5_db_entry *server,
                    krb5_timestamp kdc_time, const char **status,
                    krb5_pa_data ***e_data)
{
    krb5_context context = realm->realm_context;
    krb5_error_code ret;

    /*
     * If an option is set that is only allowed in TGS requests, complain.
     */
    if (request->kdc_options & AS_INVALID_OPTIONS) {
        *status = "INVALID AS OPTIONS";
        return KRB5KDC_ERR_BADOPTION;
    }

    /* The client must not be expired */
    if (client->expiration && ts_after(kdc_time, client->expiration)) {
        *status = "CLIENT EXPIRED";
        if (vague_errors)
            return KRB5KRB_ERR_GENERIC;
        else
            return KRB5KDC_ERR_NAME_EXP;
    }

    /* The client's password must not be expired, unless the server is
       a KRB5_KDC_PWCHANGE_SERVICE. */
    if (client->pw_expiration && ts_after(kdc_time, client->pw_expiration) &&
        !isflagset(server->attributes, KRB5_KDB_PWCHANGE_SERVICE)) {
        *status = "CLIENT KEY EXPIRED";
        if (vague_errors)
            return KRB5KRB_ERR_GENERIC;
        else
            return KRB5KDC_ERR_KEY_EXP;
    }

    /* The server must not be expired */
    if (server->expiration && ts_after(kdc_time, server->expiration)) {
        *status = "SERVICE EXPIRED";
        return KRB5KDC_ERR_SERVICE_EXP;
    }

    /*
     * If the client requires password changing, then only allow the
     * pwchange service.
     */
    if (isflagset(client->attributes, KRB5_KDB_REQUIRES_PWCHANGE) &&
        !isflagset(server->attributes, KRB5_KDB_PWCHANGE_SERVICE)) {
        *status = "REQUIRED PWCHANGE";
        return KRB5KDC_ERR_KEY_EXP;
    }

    /* Client and server must allow postdating tickets */
    if ((isflagset(request->kdc_options, KDC_OPT_ALLOW_POSTDATE) ||
         isflagset(request->kdc_options, KDC_OPT_POSTDATED)) &&
        (isflagset(client->attributes, KRB5_KDB_DISALLOW_POSTDATED) ||
         isflagset(server->attributes, KRB5_KDB_DISALLOW_POSTDATED))) {
        *status = "POSTDATE NOT ALLOWED";
        return KRB5KDC_ERR_CANNOT_POSTDATE;
    }

    /* Check to see if client is locked out */
    if (isflagset(client->attributes, KRB5_KDB_DISALLOW_ALL_TIX)) {
        *status = "CLIENT LOCKED OUT";
        return KRB5KDC_ERR_CLIENT_REVOKED;
    }

    /* Check to see if server is locked out */
    if (isflagset(server->attributes, KRB5_KDB_DISALLOW_ALL_TIX)) {
        *status = "SERVICE LOCKED OUT";
        return KRB5KDC_ERR_S_PRINCIPAL_UNKNOWN;
    }

    /* Check to see if server is allowed to be a service */
    if (isflagset(server->attributes, KRB5_KDB_DISALLOW_SVR)) {
        *status = "SERVICE NOT ALLOWED";
        return KRB5KDC_ERR_MUST_USE_USER2USER;
    }

    if (check_anon(realm, client->princ, request->server) != 0) {
        *status = "ANONYMOUS NOT ALLOWED";
        return KRB5KDC_ERR_POLICY;
    }

    /* Perform KDB module policy checks. */
    ret = krb5_db_check_policy_as(context, request, client, server, kdc_time,
                                  status, e_data);
    return (ret == KRB5_PLUGIN_OP_NOTSUPP) ? 0 : ret;
}

/*
 * Compute ticket flags based on the request, the client and server DB entry
 * (which may prohibit forwardable or proxiable tickets), and the header
 * ticket.  client may be NULL for a TGS request (although it may be set, such
 * as for an S4U2Self request).  header_enc may be NULL for an AS request.
 */
krb5_flags
get_ticket_flags(krb5_flags reqflags, krb5_db_entry *client,
                 krb5_db_entry *server, krb5_enc_tkt_part *header_enc)
{
    krb5_flags flags;

    /* Validation and renewal TGS requests preserve the header ticket flags. */
    if ((reqflags & (KDC_OPT_VALIDATE | KDC_OPT_RENEW)) && header_enc != NULL)
        return header_enc->flags & ~TKT_FLG_INVALID;

    /* Indicate support for encrypted padata (RFC 6806), and set flags based on
     * request options and the header ticket. */
    flags = OPTS2FLAGS(reqflags) | TKT_FLG_ENC_PA_REP;
    if (reqflags & KDC_OPT_POSTDATED)
        flags |= TKT_FLG_INVALID;
    if (header_enc != NULL)
        flags |= COPY_TKT_FLAGS(header_enc->flags);
    if (header_enc == NULL)
        flags |= TKT_FLG_INITIAL;

    /* For TGS requests, indicate if the service is marked ok-as-delegate. */
    if (header_enc != NULL && (server->attributes & KRB5_KDB_OK_AS_DELEGATE))
        flags |= TKT_FLG_OK_AS_DELEGATE;

    /* Unset PROXIABLE if it is disallowed. */
    if (client != NULL && (client->attributes & KRB5_KDB_DISALLOW_PROXIABLE))
        flags &= ~TKT_FLG_PROXIABLE;
    if (server->attributes & KRB5_KDB_DISALLOW_PROXIABLE)
        flags &= ~TKT_FLG_PROXIABLE;
    if (header_enc != NULL && !(header_enc->flags & TKT_FLG_PROXIABLE))
        flags &= ~TKT_FLG_PROXIABLE;

    /* Unset FORWARDABLE if it is disallowed. */
    if (client != NULL && (client->attributes & KRB5_KDB_DISALLOW_FORWARDABLE))
        flags &= ~TKT_FLG_FORWARDABLE;
    if (server->attributes & KRB5_KDB_DISALLOW_FORWARDABLE)
        flags &= ~TKT_FLG_FORWARDABLE;
    if (header_enc != NULL && !(header_enc->flags & TKT_FLG_FORWARDABLE))
        flags &= ~TKT_FLG_FORWARDABLE;

    /* We don't currently handle issuing anonymous tickets based on
     * non-anonymous ones. */
    if (header_enc != NULL && !(header_enc->flags & TKT_FLG_ANONYMOUS))
        flags &= ~TKT_FLG_ANONYMOUS;

    return flags;
}

/* Return KRB5KDC_ERR_POLICY if indicators does not contain the required auth
 * indicators for server, ENOMEM on allocation error, 0 otherwise. */
krb5_error_code
check_indicators(krb5_context context, krb5_db_entry *server,
                 krb5_data *const *indicators)
{
    krb5_error_code ret;
    char *str = NULL, *copy = NULL, *save, *ind;

    ret = krb5_dbe_get_string(context, server, KRB5_KDB_SK_REQUIRE_AUTH, &str);
    if (ret || str == NULL)
        goto cleanup;
    copy = strdup(str);
    if (copy == NULL) {
        ret = ENOMEM;
        goto cleanup;
    }

    /* Look for any of the space-separated strings in indicators. */
    ind = strtok_r(copy, " ", &save);
    while (ind != NULL) {
        if (authind_contains(indicators, ind))
            goto cleanup;
        ind = strtok_r(NULL, " ", &save);
    }

    ret = KRB5KDC_ERR_POLICY;
    k5_setmsg(context, ret,
              _("Required auth indicators not present in ticket: %s"), str);

cleanup:
    krb5_dbe_free_string(context, str);
    free(copy);
    return ret;
}

#define ASN1_ID_CLASS   (0xc0)
#define ASN1_ID_TYPE    (0x20)
#define ASN1_ID_TAG     (0x1f)
#define ASN1_CLASS_UNIV (0)
#define ASN1_CLASS_APP  (1)
#define ASN1_CLASS_CTX  (2)
#define ASN1_CLASS_PRIV (3)
#define asn1_id_constructed(x)  (x & ASN1_ID_TYPE)
#define asn1_id_primitive(x)    (!asn1_id_constructed(x))
#define asn1_id_class(x)        ((x & ASN1_ID_CLASS) >> 6)
#define asn1_id_tag(x)          (x & ASN1_ID_TAG)

/*
 * asn1length - return encoded length of value.
 *
 * passed a pointer into the asn.1 stream, which is updated
 * to point right after the length bits.
 *
 * returns -1 on failure.
 */
static int
asn1length(unsigned char **astream)
{
    int length;         /* resulting length */
    int sublen;         /* sublengths */
    int blen;           /* bytes of length */
    unsigned char *p;   /* substring searching */

    if (**astream & 0x80) {
        blen = **astream & 0x7f;
        if (blen > 3) {
            return(-1);
        }
        for (++*astream, length = 0; blen; ++*astream, blen--) {
            length = (length << 8) | **astream;
        }
        if (length == 0) {
            /* indefinite length, figure out by hand */
            p = *astream;
            p++;
            while (1) {
                /* compute value length. */
                if ((sublen = asn1length(&p)) < 0) {
                    return(-1);
                }
                p += sublen;
                /* check for termination */
                if ((!*p++) && (!*p)) {
                    p++;
                    break;
                }
            }
            length = p - *astream;
        }
    } else {
        length = **astream;
        ++*astream;
    }
    return(length);
}

/*
 * fetch_asn1_field - return raw asn.1 stream of subfield.
 *
 * this routine is passed a context-dependent tag number and "level" and returns
 * the size and length of the corresponding level subfield.
 *
 * levels and are numbered starting from 1.
 *
 * returns 0 on success, -1 otherwise.
 */
int
fetch_asn1_field(unsigned char *astream, unsigned int level,
                 unsigned int field, krb5_data *data)
{
    unsigned char *estream;     /* end of stream */
    int classes;                /* # classes seen so far this level */
    unsigned int levels = 0;            /* levels seen so far */
    int lastlevel = 1000;       /* last level seen */
    int length;                 /* various lengths */
    int tag;                    /* tag number */
    unsigned char savelen;      /* saved length of our field */

    classes = -1;
    /* we assume that the first identifier/length will tell us
       how long the entire stream is. */
    astream++;
    estream = astream;
    if ((length = asn1length(&astream)) < 0) {
        return(-1);
    }
    estream += length;
    /* search down the stream, checking identifiers.  we process identifiers
       until we hit the "level" we want, and then process that level for our
       subfield, always making sure we don't go off the end of the stream.  */
    while (astream < estream) {
        if (!asn1_id_constructed(*astream)) {
            return(-1);
        }
        if (asn1_id_class(*astream) == ASN1_CLASS_CTX) {
            if ((tag = (int)asn1_id_tag(*astream)) <= lastlevel) {
                levels++;
                classes = -1;
            }
            lastlevel = tag;
            if (levels == level) {
                /* in our context-dependent class, is this the one we're looking for ? */
                if (tag == (int)field) {
                    /* return length and data */
                    astream++;
                    savelen = *astream;
                    if ((length = asn1length(&astream)) < 0) {
                        return(-1);
                    }
                    data->length = length;
                    /* if the field length is indefinite, we will have to subtract two
                       (terminating octets) from the length returned since we don't want
                       to pass any info from the "wrapper" back.  asn1length will always return
                       the *total* length of the field, not just what's contained in it */
                    if ((savelen & 0xff) == 0x80) {
                        data->length -=2 ;
                    }
                    data->data = (char *)astream;
                    return(0);
                } else if (tag <= classes) {
                    /* we've seen this class before, something must be wrong */
                    return(-1);
                } else {
                    classes = tag;
                }
            }
        }
        /* if we're not on our level yet, process this value.  otherwise skip over it */
        astream++;
        if ((length = asn1length(&astream)) < 0) {
            return(-1);
        }
        if (levels == level) {
            astream += length;
        }
    }
    return(-1);
}

/* Return true if we believe server can support enctype as a session key. */
static krb5_boolean
dbentry_supports_enctype(krb5_context context, krb5_db_entry *server,
                         krb5_enctype enctype)
{
    krb5_error_code     retval;
    krb5_key_data       *datap;
    char                *etypes_str = NULL;
    krb5_enctype        default_enctypes[1] = { 0 };
    krb5_enctype        *etypes = NULL;
    krb5_boolean        in_list;

    /* Look up the supported session key enctypes list in the KDB. */
    retval = krb5_dbe_get_string(context, server, KRB5_KDB_SK_SESSION_ENCTYPES,
                                 &etypes_str);
    if (retval == 0 && etypes_str != NULL && *etypes_str != '\0') {
        /* Pass a fake profile key for tracing of unrecognized tokens. */
        retval = krb5int_parse_enctype_list(context, "KDB-session_etypes",
                                            etypes_str, default_enctypes,
                                            &etypes);
        if (retval == 0 && etypes != NULL && etypes[0]) {
            in_list = k5_etypes_contains(etypes, enctype);
            free(etypes_str);
            free(etypes);
            return in_list;
        }
        /* Fall through on error or empty list */
    }
    free(etypes_str);
    free(etypes);

    /* Assume every server without a session_enctypes attribute supports
     * aes256-cts-hmac-sha1-96. */
    if (enctype == ENCTYPE_AES256_CTS_HMAC_SHA1_96)
        return TRUE;
    /* Assume the server supports any enctype it has a long-term key for. */
    return !krb5_dbe_find_enctype(context, server, enctype, -1, 0, &datap);
}

/*
 * This function returns the keytype which should be selected for the
 * session key.  It is based on the ordered list which the user
 * requested, and what the KDC and the application server can support.
 */
krb5_enctype
select_session_keytype(krb5_context context, krb5_db_entry *server,
                       int nktypes, krb5_enctype *ktype)
{
    int         i;

    for (i = 0; i < nktypes; i++) {
        if (!krb5_c_valid_enctype(ktype[i]))
            continue;

        if (!krb5_is_permitted_enctype(context, ktype[i]))
            continue;

        /*
         * Prevent these deprecated enctypes from being used as session keys
         * unless they are explicitly allowed.  In the future they will be more
         * comprehensively disabled and eventually removed.
         */
        if (ktype[i] == ENCTYPE_DES3_CBC_SHA1 && !context->allow_des3)
            continue;
        if (ktype[i] == ENCTYPE_ARCFOUR_HMAC && !context->allow_rc4)
            continue;

        if (dbentry_supports_enctype(context, server, ktype[i]))
            return ktype[i];
    }

    return 0;
}

/*
 * Limit strings to a "reasonable" length to prevent crowding out of
 * other useful information in the log entry
 */
#define NAME_LENGTH_LIMIT 128

void limit_string(char *name)
{
    int     i;

    if (!name)
        return;

    if (strlen(name) < NAME_LENGTH_LIMIT)
        return;

    i = NAME_LENGTH_LIMIT-4;
    name[i++] = '.';
    name[i++] = '.';
    name[i++] = '.';
    name[i] = '\0';
    return;
}

/* Wrapper of krb5_enctype_to_name() to include the PKINIT types. */
static krb5_error_code
enctype_name(krb5_enctype ktype, char *buf, size_t buflen)
{
    const char *name, *prefix = "";
    size_t len;

    if (buflen == 0)
        return EINVAL;
    *buf = '\0'; /* ensure these are always valid C-strings */

    if (!krb5_c_valid_enctype(ktype))
        prefix = "UNSUPPORTED:";
    else if (krb5int_c_deprecated_enctype(ktype))
        prefix = "DEPRECATED:";
    len = strlcpy(buf, prefix, buflen);
    if (len >= buflen)
        return ENOMEM;
    buflen -= len;
    buf += len;

    /* rfc4556 recommends that clients wishing to indicate support for these
     * pkinit algorithms include them in the etype field of the AS-REQ. */
    if (ktype == ENCTYPE_DSA_SHA1_CMS)
        name = "id-dsa-with-sha1-CmsOID";
    else if (ktype == ENCTYPE_MD5_RSA_CMS)
        name = "md5WithRSAEncryption-CmsOID";
    else if (ktype == ENCTYPE_SHA1_RSA_CMS)
        name = "sha-1WithRSAEncryption-CmsOID";
    else if (ktype == ENCTYPE_RC2_CBC_ENV)
        name = "rc2-cbc-EnvOID";
    else if (ktype == ENCTYPE_RSA_ENV)
        name = "rsaEncryption-EnvOID";
    else if (ktype == ENCTYPE_RSA_ES_OAEP_ENV)
        name = "id-RSAES-OAEP-EnvOID";
    else if (ktype == ENCTYPE_DES3_CBC_ENV)
        name = "des-ede3-cbc-EnvOID";
    else
        return krb5_enctype_to_name(ktype, FALSE, buf, buflen);

    if (strlcpy(buf, name, buflen) >= buflen)
        return ENOMEM;
    return 0;
}

char *
ktypes2str(krb5_enctype *ktype, int nktypes)
{
    struct k5buf buf;
    int i;
    char name[64];

    if (nktypes < 0)
        return NULL;

    k5_buf_init_dynamic(&buf);
    k5_buf_add_fmt(&buf, "%d etypes {", nktypes);
    for (i = 0; i < nktypes; i++) {
        enctype_name(ktype[i], name, sizeof(name));
        k5_buf_add_fmt(&buf, "%s%s(%ld)", i ? ", " : "", name, (long)ktype[i]);
    }
    k5_buf_add(&buf, "}");
    return k5_buf_cstring(&buf);
}

char *
rep_etypes2str(krb5_kdc_rep *rep)
{
    struct k5buf buf;
    char name[64];
    krb5_enctype etype;

    k5_buf_init_dynamic(&buf);
    k5_buf_add(&buf, "etypes {rep=");
    enctype_name(rep->enc_part.enctype, name, sizeof(name));
    k5_buf_add_fmt(&buf, "%s(%ld)", name, (long)rep->enc_part.enctype);

    if (rep->ticket != NULL) {
        etype = rep->ticket->enc_part.enctype;
        enctype_name(etype, name, sizeof(name));
        k5_buf_add_fmt(&buf, ", tkt=%s(%ld)", name, (long)etype);
    }

    if (rep->ticket != NULL && rep->ticket->enc_part2 != NULL &&
        rep->ticket->enc_part2->session != NULL) {
        etype = rep->ticket->enc_part2->session->enctype;
        enctype_name(etype, name, sizeof(name));
        k5_buf_add_fmt(&buf, ", ses=%s(%ld)", name, (long)etype);
    }

    k5_buf_add(&buf, "}");
    return k5_buf_cstring(&buf);
}

static krb5_error_code
verify_for_user_checksum(krb5_context context,
                         krb5_keyblock *key,
                         krb5_pa_for_user *req)
{
    krb5_error_code             code;
    int                         i;
    krb5_int32                  name_type;
    char                        *p;
    krb5_data                   data;
    krb5_boolean                valid = FALSE;

    if (!krb5_c_is_keyed_cksum(req->cksum.checksum_type)) {
        return KRB5KRB_AP_ERR_INAPP_CKSUM;
    }

    /*
     * Checksum is over name type and string components of
     * client principal name and auth_package.
     */
    data.length = 4;
    for (i = 0; i < krb5_princ_size(context, req->user); i++) {
        data.length += krb5_princ_component(context, req->user, i)->length;
    }
    data.length += krb5_princ_realm(context, req->user)->length;
    data.length += req->auth_package.length;

    p = data.data = malloc(data.length);
    if (data.data == NULL) {
        return ENOMEM;
    }

    name_type = krb5_princ_type(context, req->user);
    p[0] = (name_type >> 0 ) & 0xFF;
    p[1] = (name_type >> 8 ) & 0xFF;
    p[2] = (name_type >> 16) & 0xFF;
    p[3] = (name_type >> 24) & 0xFF;
    p += 4;

    for (i = 0; i < krb5_princ_size(context, req->user); i++) {
        if (krb5_princ_component(context, req->user, i)->length > 0) {
            memcpy(p, krb5_princ_component(context, req->user, i)->data,
                   krb5_princ_component(context, req->user, i)->length);
        }
        p += krb5_princ_component(context, req->user, i)->length;
    }

    if (krb5_princ_realm(context, req->user)->length > 0) {
        memcpy(p, krb5_princ_realm(context, req->user)->data,
               krb5_princ_realm(context, req->user)->length);
    }
    p += krb5_princ_realm(context, req->user)->length;

    if (req->auth_package.length > 0)
        memcpy(p, req->auth_package.data, req->auth_package.length);
    p += req->auth_package.length;

    code = krb5_c_verify_checksum(context,
                                  key,
                                  KRB5_KEYUSAGE_APP_DATA_CKSUM,
                                  &data,
                                  &req->cksum,
                                  &valid);

    if (code == 0 && valid == FALSE)
        code = KRB5KRB_AP_ERR_MODIFIED;

    free(data.data);

    return code;
}

/*
 * Legacy protocol transition (Windows 2003 and above)
 */
static krb5_error_code
kdc_process_for_user(krb5_context context, krb5_pa_data *pa_data,
                     krb5_keyblock *tgs_session,
                     krb5_pa_s4u_x509_user **s4u_x509_user,
                     const char **status)
{
    krb5_error_code             code;
    krb5_pa_for_user            *for_user;
    krb5_data                   req_data;

    req_data.length = pa_data->length;
    req_data.data = (char *)pa_data->contents;

    code = decode_krb5_pa_for_user(&req_data, &for_user);
    if (code) {
        *status = "DECODE_PA_FOR_USER";
        return code;
    }

    code = verify_for_user_checksum(context, tgs_session, for_user);
    if (code) {
        *status = "INVALID_S4U2SELF_CHECKSUM";
        krb5_free_pa_for_user(context, for_user);
        return code;
    }

    *s4u_x509_user = calloc(1, sizeof(krb5_pa_s4u_x509_user));
    if (*s4u_x509_user == NULL) {
        krb5_free_pa_for_user(context, for_user);
        return ENOMEM;
    }

    (*s4u_x509_user)->user_id.user = for_user->user;
    for_user->user = NULL;
    krb5_free_pa_for_user(context, for_user);

    return 0;
}

static krb5_error_code
verify_s4u_x509_user_checksum(krb5_context context,
                              krb5_keyblock *key,
                              krb5_data *req_data,
                              krb5_int32 kdc_req_nonce,
                              krb5_pa_s4u_x509_user *req)
{
    krb5_error_code             code;
    krb5_data                   scratch;
    krb5_boolean                valid = FALSE;

    if (enctype_requires_etype_info_2(key->enctype) &&
        !krb5_c_is_keyed_cksum(req->cksum.checksum_type))
        return KRB5KRB_AP_ERR_INAPP_CKSUM;

    if (req->user_id.nonce != kdc_req_nonce)
        return KRB5KRB_AP_ERR_MODIFIED;

    /*
     * Verify checksum over the encoded userid. If that fails,
     * re-encode, and verify that. This is similar to the
     * behaviour in kdc_process_tgs_req().
     */
    if (fetch_asn1_field((unsigned char *)req_data->data, 1, 0, &scratch) < 0)
        return ASN1_PARSE_ERROR;

    code = krb5_c_verify_checksum(context,
                                  key,
                                  KRB5_KEYUSAGE_PA_S4U_X509_USER_REQUEST,
                                  &scratch,
                                  &req->cksum,
                                  &valid);
    if (code != 0)
        return code;

    if (valid == FALSE) {
        krb5_data *data;

        code = encode_krb5_s4u_userid(&req->user_id, &data);
        if (code != 0)
            return code;

        code = krb5_c_verify_checksum(context,
                                      key,
                                      KRB5_KEYUSAGE_PA_S4U_X509_USER_REQUEST,
                                      data,
                                      &req->cksum,
                                      &valid);

        krb5_free_data(context, data);

        if (code != 0)
            return code;
    }

    return valid ? 0 : KRB5KRB_AP_ERR_MODIFIED;
}

/*
 * New protocol transition request (Windows 2008 and above)
 */
static krb5_error_code
kdc_process_s4u_x509_user(krb5_context context,
                          krb5_kdc_req *request,
                          krb5_pa_data *pa_data,
                          krb5_keyblock *tgs_subkey,
                          krb5_keyblock *tgs_session,
                          krb5_pa_s4u_x509_user **s4u_x509_user,
                          const char **status)
{
    krb5_error_code             code;
    krb5_data                   req_data;

    req_data.length = pa_data->length;
    req_data.data = (char *)pa_data->contents;

    code = decode_krb5_pa_s4u_x509_user(&req_data, s4u_x509_user);
    if (code) {
        *status = "DECODE_PA_S4U_X509_USER";
        return code;
    }

    code = verify_s4u_x509_user_checksum(context,
                                         tgs_subkey ? tgs_subkey :
                                         tgs_session,
                                         &req_data,
                                         request->nonce, *s4u_x509_user);

    if (code) {
        *status = "INVALID_S4U2SELF_CHECKSUM";
        krb5_free_pa_s4u_x509_user(context, *s4u_x509_user);
        *s4u_x509_user = NULL;
        return code;
    }

    if (krb5_princ_size(context, (*s4u_x509_user)->user_id.user) == 0 &&
        (*s4u_x509_user)->user_id.subject_cert.length == 0) {
        *status = "INVALID_S4U2SELF_REQUEST";
        krb5_free_pa_s4u_x509_user(context, *s4u_x509_user);
        *s4u_x509_user = NULL;
        return KRB5KDC_ERR_C_PRINCIPAL_UNKNOWN;
    }

    return 0;
}

krb5_error_code
kdc_make_s4u2self_rep(krb5_context context,
                      krb5_keyblock *tgs_subkey,
                      krb5_keyblock *tgs_session,
                      krb5_pa_s4u_x509_user *req_s4u_user,
                      krb5_kdc_rep *reply,
                      krb5_enc_kdc_rep_part *reply_encpart)
{
    krb5_error_code             code;
    krb5_data                   *der_user_id = NULL, *der_s4u_x509_user = NULL;
    krb5_pa_s4u_x509_user       rep_s4u_user;
    krb5_pa_data                *pa = NULL;
    krb5_enctype                enctype;
    krb5_keyusage               usage;

    memset(&rep_s4u_user, 0, sizeof(rep_s4u_user));

    rep_s4u_user.user_id.nonce   = req_s4u_user->user_id.nonce;
    rep_s4u_user.user_id.user    = req_s4u_user->user_id.user;
    rep_s4u_user.user_id.options =
        req_s4u_user->user_id.options & KRB5_S4U_OPTS_USE_REPLY_KEY_USAGE;

    code = encode_krb5_s4u_userid(&rep_s4u_user.user_id, &der_user_id);
    if (code != 0)
        goto cleanup;

    if (req_s4u_user->user_id.options & KRB5_S4U_OPTS_USE_REPLY_KEY_USAGE)
        usage = KRB5_KEYUSAGE_PA_S4U_X509_USER_REPLY;
    else
        usage = KRB5_KEYUSAGE_PA_S4U_X509_USER_REQUEST;

    code = krb5_c_make_checksum(context, req_s4u_user->cksum.checksum_type,
                                tgs_subkey != NULL ? tgs_subkey : tgs_session,
                                usage, der_user_id, &rep_s4u_user.cksum);
    if (code != 0)
        goto cleanup;

    code = encode_krb5_pa_s4u_x509_user(&rep_s4u_user, &der_s4u_x509_user);
    if (code != 0)
        goto cleanup;

    code = k5_add_pa_data_from_data(&reply->padata, KRB5_PADATA_S4U_X509_USER,
                                    der_s4u_x509_user);
    if (code != 0)
        goto cleanup;

    if (tgs_subkey != NULL)
        enctype = tgs_subkey->enctype;
    else
        enctype = tgs_session->enctype;

    /*
     * Owing to a bug in Windows, unkeyed checksums were used for older
     * enctypes, including rc4-hmac. A forthcoming workaround for this
     * includes the checksum bytes in the encrypted padata.
     */
    if (enctype_requires_etype_info_2(enctype) == FALSE) {
        code = k5_alloc_pa_data(KRB5_PADATA_S4U_X509_USER,
                                req_s4u_user->cksum.length +
                                rep_s4u_user.cksum.length, &pa);
        if (code != 0)
            goto cleanup;
        memcpy(pa->contents,
               req_s4u_user->cksum.contents, req_s4u_user->cksum.length);
        memcpy(&pa->contents[req_s4u_user->cksum.length],
               rep_s4u_user.cksum.contents, rep_s4u_user.cksum.length);

        code = k5_add_pa_data_element(&reply_encpart->enc_padata, &pa);
        if (code != 0)
            goto cleanup;
    }

cleanup:
    if (rep_s4u_user.cksum.contents != NULL)
        krb5_free_checksum_contents(context, &rep_s4u_user.cksum);
    krb5_free_data(context, der_user_id);
    krb5_free_data(context, der_s4u_x509_user);
    k5_free_pa_data_element(pa);
    return code;
}

/* Return true if princ canonicalizes to the same principal as entry's. */
krb5_boolean
is_client_db_alias(krb5_context context, const krb5_db_entry *entry,
                   krb5_const_principal princ)
{
    krb5_error_code ret;
    krb5_db_entry *self;
    krb5_boolean is_self = FALSE;

    ret = krb5_db_get_principal(context, princ, KRB5_KDB_FLAG_CLIENT, &self);
    if (!ret) {
        is_self = krb5_principal_compare(context, entry->princ, self->princ);
        krb5_db_free_principal(context, self);
    }

    return is_self;
}

/*
 * If S4U2Self padata is present in request, verify the checksum and set
 * *s4u_x509_user to the S4U2Self request.  If the requested client realm is
 * local, look up the client and set *princ_ptr to its DB entry.
 */
krb5_error_code
kdc_process_s4u2self_req(krb5_context context, krb5_kdc_req *request,
                         const krb5_db_entry *server,
                         krb5_keyblock *tgs_subkey, krb5_keyblock *tgs_session,
                         krb5_pa_s4u_x509_user **s4u_x509_user,
                         krb5_db_entry **princ_ptr, const char **status)
{
    krb5_error_code             code;
    krb5_pa_data                *pa_data;
    krb5_db_entry               *princ;
    krb5_s4u_userid             *id;

    *princ_ptr = NULL;

    pa_data = krb5int_find_pa_data(context, request->padata,
                                   KRB5_PADATA_S4U_X509_USER);
    if (pa_data != NULL) {
        code = kdc_process_s4u_x509_user(context, request, pa_data, tgs_subkey,
                                         tgs_session, s4u_x509_user, status);
        if (code != 0)
            return code;
    } else {
        pa_data = krb5int_find_pa_data(context, request->padata,
                                       KRB5_PADATA_FOR_USER);
        if (pa_data != NULL) {
            code = kdc_process_for_user(context, pa_data, tgs_session,
                                        s4u_x509_user, status);
            if (code != 0)
                return code;
        } else
            return 0;
    }
    id = &(*s4u_x509_user)->user_id;

    if (data_eq(server->princ->realm, id->user->realm)) {
        if (id->subject_cert.length != 0) {
            code = krb5_db_get_s4u_x509_principal(context,
                                                  &id->subject_cert, id->user,
                                                  KRB5_KDB_FLAG_CLIENT,
                                                  &princ);
            if (code == 0 && id->user->length == 0) {
                krb5_free_principal(context, id->user);
                code = krb5_copy_principal(context, princ->princ, &id->user);
            }
        } else {
            code = krb5_db_get_principal(context, id->user,
                                         KRB5_KDB_FLAG_CLIENT, &princ);
        }
        if (code == KRB5_KDB_NOENTRY) {
            *status = "UNKNOWN_S4U2SELF_PRINCIPAL";
            return KRB5KDC_ERR_C_PRINCIPAL_UNKNOWN;
        } else if (code) {
            *status = "LOOKING_UP_S4U2SELF_PRINCIPAL";
            return code; /* caller can free for_user */
        }

        /* Ignore password expiration and needchange attributes (as Windows
         * does), since S4U2Self is not password authentication. */
        princ->pw_expiration = 0;
        clear(princ->attributes, KRB5_KDB_REQUIRES_PWCHANGE);

        *princ_ptr = princ;
    }

    return 0;
}

/* Clear the forwardable flag in tkt if server cannot obtain forwardable
 * S4U2Self tickets according to [MS-SFU] 3.2.5.1.2. */
krb5_error_code
s4u2self_forwardable(krb5_context context, krb5_db_entry *server,
                     krb5_flags *tktflags)
{
    krb5_error_code ret;

    /* Allow the forwardable flag if server has ok-to-auth-as-delegate set. */
    if (server->attributes & KRB5_KDB_OK_TO_AUTH_AS_DELEGATE)
        return 0;

    /* Deny the forwardable flag if server has any authorized delegation
     * targets for traditional S4U2Proxy. */
    ret = krb5_db_check_allowed_to_delegate(context, NULL, server, NULL);
    if (!ret)
        *tktflags &= ~TKT_FLG_FORWARDABLE;

    if (ret == KRB5KDC_ERR_BADOPTION || ret == KRB5_PLUGIN_OP_NOTSUPP)
        return 0;
    return ret;
}

krb5_error_code
kdc_check_transited_list(krb5_context context, const krb5_data *trans,
                         const krb5_data *realm1, const krb5_data *realm2)
{
    krb5_error_code             code;

    /* Check against the KDB module.  Treat this answer as authoritative if the
     * method is supported and doesn't explicitly pass control. */
    code = krb5_db_check_transited_realms(context, trans, realm1, realm2);
    if (code != KRB5_PLUGIN_OP_NOTSUPP && code != KRB5_PLUGIN_NO_HANDLE)
        return code;

    /* Check using krb5.conf [capaths] or hierarchical relationships. */
    return krb5_check_transited_list(context, trans, realm1, realm2);
}

krb5_boolean
enctype_requires_etype_info_2(krb5_enctype enctype)
{
    switch(enctype) {
    case ENCTYPE_DES3_CBC_SHA1:
    case ENCTYPE_DES3_CBC_RAW:
    case ENCTYPE_ARCFOUR_HMAC:
    case ENCTYPE_ARCFOUR_HMAC_EXP :
        return 0;
    default:
        return krb5_c_valid_enctype(enctype);
    }
}

void
kdc_get_ticket_endtime(kdc_realm_t *realm, krb5_timestamp starttime,
                       krb5_timestamp endtime, krb5_timestamp till,
                       krb5_db_entry *client, krb5_db_entry *server,
                       krb5_timestamp *out_endtime)
{
    krb5_timestamp until;
    krb5_deltat life;

    if (till == 0)
        till = kdc_infinity;

    until = ts_min(till, endtime);

    /* Determine the requested lifetime, capped at the maximum valid time
     * interval. */
    life = ts_delta(until, starttime);
    if (ts_after(until, starttime) && life < 0)
        life = INT32_MAX;

    if (client != NULL && client->max_life != 0)
        life = min(life, client->max_life);
    if (server->max_life != 0)
        life = min(life, server->max_life);
    if (realm->realm_maxlife != 0)
        life = min(life, realm->realm_maxlife);

    *out_endtime = ts_incr(starttime, life);
}

/*
 * Set times->renew_till to the requested renewable lifetime as modified by
 * policy.  Set the TKT_FLG_RENEWABLE bit in *tktflags if we set a nonzero
 * renew_till.  *times must be filled in except for renew_till.  client and tgt
 * may be NULL.
 */
void
kdc_get_ticket_renewtime(kdc_realm_t *realm, krb5_kdc_req *request,
                         krb5_enc_tkt_part *tgt, krb5_db_entry *client,
                         krb5_db_entry *server, krb5_flags *tktflags,
                         krb5_ticket_times *times)
{
    krb5_timestamp rtime, max_rlife;

    *tktflags &= ~TKT_FLG_RENEWABLE;
    times->renew_till = 0;

    /* Don't issue renewable tickets if the client or server don't allow it,
     * or if this is a TGS request and the TGT isn't renewable. */
    if (server->attributes & KRB5_KDB_DISALLOW_RENEWABLE)
        return;
    if (client != NULL && (client->attributes & KRB5_KDB_DISALLOW_RENEWABLE))
        return;
    if (tgt != NULL && !(tgt->flags & TKT_FLG_RENEWABLE))
        return;

    /* Determine the requested renewable time. */
    if (isflagset(request->kdc_options, KDC_OPT_RENEWABLE))
        rtime = request->rtime ? request->rtime : kdc_infinity;
    else if (isflagset(request->kdc_options, KDC_OPT_RENEWABLE_OK) &&
             ts_after(request->till, times->endtime))
        rtime = request->till;
    else
        return;

    /* Truncate it to the allowable renewable time. */
    if (tgt != NULL)
        rtime = ts_min(rtime, tgt->times.renew_till);
    max_rlife = min(server->max_renewable_life, realm->realm_maxrlife);
    if (client != NULL)
        max_rlife = min(max_rlife, client->max_renewable_life);
    rtime = ts_min(rtime, ts_incr(times->starttime, max_rlife));

    /* If the client only specified renewable-ok, don't issue a renewable
     * ticket unless the truncated renew time exceeds the ticket end time. */
    if (!isflagset(request->kdc_options, KDC_OPT_RENEWABLE) &&
        !ts_after(rtime, times->endtime))
        return;

    *tktflags |= TKT_FLG_RENEWABLE;
    times->renew_till = rtime;
}

/**
 * Handle protected negotiation of FAST using enc_padata
 * - If ENCPADATA_REQ_ENC_PA_REP is present, then:
 * - Return ENCPADATA_REQ_ENC_PA_REP with checksum of AS-REQ from client
 * - Include PADATA_FX_FAST in the enc_padata to indicate FAST
 * @pre @c out_enc_padata has space for at least two more padata
 * @param index in/out index into @c out_enc_padata for next item
 */
krb5_error_code
kdc_handle_protected_negotiation(krb5_context context,
                                 krb5_data *req_pkt, krb5_kdc_req *request,
                                 const krb5_keyblock *reply_key,
                                 krb5_pa_data ***out_enc_padata)
{
    krb5_error_code retval = 0;
    krb5_checksum checksum;
    krb5_data *der_cksum = NULL;
    krb5_pa_data *pa_in;

    memset(&checksum, 0, sizeof(checksum));

    pa_in = krb5int_find_pa_data(context, request->padata,
                                 KRB5_ENCPADATA_REQ_ENC_PA_REP);
    if (pa_in == NULL)
        return 0;

    /* Compute and encode a checksum over the AS-REQ. */
    retval = krb5_c_make_checksum(context, 0, reply_key, KRB5_KEYUSAGE_AS_REQ,
                                  req_pkt, &checksum);
    if (retval != 0)
        goto cleanup;
    retval = encode_krb5_checksum(&checksum, &der_cksum);
    if (retval != 0)
        goto cleanup;

    retval = k5_add_pa_data_from_data(out_enc_padata,
                                      KRB5_ENCPADATA_REQ_ENC_PA_REP,
                                      der_cksum);
    if (retval)
        goto cleanup;

    /* Add a zero-length PA-FX-FAST element to the list. */
    retval = k5_add_empty_pa_data(out_enc_padata, KRB5_PADATA_FX_FAST);

cleanup:
    krb5_free_checksum_contents(context, &checksum);
    krb5_free_data(context, der_cksum);
    return retval;
}

krb5_error_code
kdc_get_pa_pac_options(krb5_context context, krb5_pa_data **in_padata,
                       krb5_pa_pac_options **pac_options_out)
{
    krb5_pa_data *pa;
    krb5_data der_pac_options;

    *pac_options_out = NULL;

    pa = krb5int_find_pa_data(context, in_padata, KRB5_PADATA_PAC_OPTIONS);
    if (pa == NULL)
        return 0;

    der_pac_options = make_data(pa->contents, pa->length);
    return decode_krb5_pa_pac_options(&der_pac_options, pac_options_out);
}

krb5_error_code
kdc_add_pa_pac_options(krb5_context context, krb5_kdc_req *request,
                       krb5_pa_data ***out_enc_padata)
{
    krb5_error_code ret;
    krb5_pa_pac_options *pac_options = NULL;
    krb5_data *der_pac_options;

    ret = kdc_get_pa_pac_options(context, request->padata, &pac_options);
    if (ret || pac_options == NULL)
        return ret;

    /* Only return supported PAC options (currently only resource-based
     * constrained delegation support). */
    pac_options->options &= KRB5_PA_PAC_OPTIONS_RBCD;
    if (pac_options->options == 0) {
        free(pac_options);
        return 0;
    }

    ret = encode_krb5_pa_pac_options(pac_options, &der_pac_options);
    free(pac_options);
    if (ret)
        return ret;

    ret = k5_add_pa_data_from_data(out_enc_padata, KRB5_PADATA_PAC_OPTIONS,
                                   der_pac_options);
    krb5_free_data(context, der_pac_options);
    return ret;
}

krb5_error_code
kdc_get_pa_pac_rbcd(krb5_context context, krb5_pa_data **in_padata,
                    krb5_boolean *supported)
{
    krb5_error_code retval;
    krb5_pa_pac_options *pac_options = NULL;

    *supported = FALSE;

    retval = kdc_get_pa_pac_options(context, in_padata, &pac_options);
    if (retval || !pac_options)
        return retval;

    if (pac_options->options & KRB5_PA_PAC_OPTIONS_RBCD)
        *supported = TRUE;

    free(pac_options);
    return 0;
}

/*
 * Although the KDC doesn't call this function directly,
 * process_tcp_connection_read() in net-server.c does call it.
 */
krb5_error_code
make_toolong_error (void *handle, krb5_data **out)
{
    krb5_error errpkt;
    krb5_error_code retval;
    krb5_data *scratch;
    struct server_handle *h = handle;

    retval = krb5_us_timeofday(h->kdc_err_context,
                               &errpkt.stime, &errpkt.susec);
    if (retval)
        return retval;
    errpkt.error = KRB_ERR_FIELD_TOOLONG;
    errpkt.server = h->kdc_realmlist[0]->realm_tgsprinc;
    errpkt.client = NULL;
    errpkt.cusec = 0;
    errpkt.ctime = 0;
    errpkt.text.length = 0;
    errpkt.text.data = 0;
    errpkt.e_data.length = 0;
    errpkt.e_data.data = 0;
    scratch = malloc(sizeof(*scratch));
    if (scratch == NULL)
        return ENOMEM;
    retval = krb5_mk_error(h->kdc_err_context, &errpkt, scratch);
    if (retval) {
        free(scratch);
        return retval;
    }

    *out = scratch;
    return 0;
}

void reset_for_hangup(void *ctx)
{
    int k;
    struct server_handle *h = ctx;

    for (k = 0; k < h->kdc_numrealms; k++)
        krb5_db_refresh_config(h->kdc_realmlist[k]->realm_context);
}
