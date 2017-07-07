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

static krb5_error_code kdc_rd_ap_req(kdc_realm_t *kdc_active_realm,
                                     krb5_ap_req *apreq,
                                     krb5_auth_context auth_context,
                                     krb5_db_entry **server,
                                     krb5_keyblock **tgskey);
static krb5_error_code find_server_key(krb5_context,
                                       krb5_db_entry *, krb5_enctype,
                                       krb5_kvno, krb5_keyblock **,
                                       krb5_kvno *);

/*
 * concatenate first two authdata arrays, returning an allocated replacement.
 * The replacement should be freed with krb5_free_authdata().
 */
krb5_error_code
concat_authorization_data(krb5_context context,
                          krb5_authdata **first, krb5_authdata **second,
                          krb5_authdata ***output)
{
    register int i, j;
    register krb5_authdata **ptr, **retdata;

    /* count up the entries */
    i = 0;
    if (first)
        for (ptr = first; *ptr; ptr++)
            i++;
    if (second)
        for (ptr = second; *ptr; ptr++)
            i++;

    retdata = (krb5_authdata **)malloc((i+1)*sizeof(*retdata));
    if (!retdata)
        return ENOMEM;
    retdata[i] = 0;                     /* null-terminated array */
    for (i = 0, j = 0, ptr = first; j < 2 ; ptr = second, j++)
        while (ptr && *ptr) {
            /* now walk & copy */
            retdata[i] = (krb5_authdata *)malloc(sizeof(*retdata[i]));
            if (!retdata[i]) {
                krb5_free_authdata(context, retdata);
                return ENOMEM;
            }
            *retdata[i] = **ptr;
            if (!(retdata[i]->contents =
                  (krb5_octet *)malloc(retdata[i]->length))) {
                free(retdata[i]);
                retdata[i] = 0;
                krb5_free_authdata(context, retdata);
                return ENOMEM;
            }
            memcpy(retdata[i]->contents, (*ptr)->contents, retdata[i]->length);

            ptr++;
            i++;
        }
    *output = retdata;
    return 0;
}

krb5_boolean
is_local_principal(kdc_realm_t *kdc_active_realm, krb5_const_principal princ1)
{
    return krb5_realm_compare(kdc_context, princ1, tgs_server);
}

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
    if (!krb5_is_tgs_principal(principal))
        return FALSE;
    if (!data_eq(*krb5_princ_component(kdc_context, principal, 1),
                 *krb5_princ_realm(kdc_context, principal)))
        return TRUE;
    else
        return FALSE;
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
kdc_process_tgs_req(kdc_realm_t *kdc_active_realm,
                    krb5_kdc_req *request, const krb5_fulladdr *from,
                    krb5_data *pkt, krb5_ticket **ticket_out,
                    krb5_db_entry **krbtgt_ptr,
                    krb5_keyblock **tgskey,
                    krb5_keyblock **subkey,
                    krb5_pa_data **pa_tgs_req)
{
    krb5_pa_data        * tmppa;
    krb5_ap_req         * apreq;
    krb5_error_code       retval;
    krb5_authdata **authdata = NULL;
    krb5_data             scratch1;
    krb5_data           * scratch = NULL;
    krb5_boolean          foreign_server = FALSE;
    krb5_auth_context     auth_context = NULL;
    krb5_authenticator  * authenticator = NULL;
    krb5_checksum       * his_cksum = NULL;
    krb5_db_entry       * krbtgt = NULL;
    krb5_ticket         * ticket;

    *ticket_out = NULL;
    *krbtgt_ptr = NULL;
    *tgskey = NULL;

    tmppa = krb5int_find_pa_data(kdc_context,
                                 request->padata, KRB5_PADATA_AP_REQ);
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

    /* If the "server" principal in the ticket is not something
       in the local realm, then we must refuse to service the request
       if the client claims to be from the local realm.

       If we don't do this, then some other realm's nasty KDC can
       claim to be authenticating a client from our realm, and we'll
       give out tickets concurring with it!

       we set a flag here for checking below.
    */
    foreign_server = !is_local_principal(kdc_active_realm,
                                         apreq->ticket->server);

    if ((retval = krb5_auth_con_init(kdc_context, &auth_context)))
        goto cleanup;

    /* Don't use a replay cache. */
    if ((retval = krb5_auth_con_setflags(kdc_context, auth_context, 0)))
        goto cleanup;

    if ((retval = krb5_auth_con_setaddrs(kdc_context, auth_context, NULL,
                                         from->address)) )
        goto cleanup_auth_context;

    retval = kdc_rd_ap_req(kdc_active_realm,
                           apreq, auth_context, &krbtgt, tgskey);
    if (retval)
        goto cleanup_auth_context;

    /* "invalid flag" tickets can must be used to validate */
    if (isflagset(ticket->enc_part2->flags, TKT_FLG_INVALID) &&
        !isflagset(request->kdc_options, KDC_OPT_VALIDATE)) {
        retval = KRB5KRB_AP_ERR_TKT_INVALID;
        goto cleanup_auth_context;
    }

    if ((retval = krb5_auth_con_getrecvsubkey(kdc_context,
                                              auth_context, subkey)))
        goto cleanup_auth_context;

    if ((retval = krb5_auth_con_getauthenticator(kdc_context, auth_context,
                                                 &authenticator)))
        goto cleanup_auth_context;

    retval = krb5_find_authdata(kdc_context,
                                ticket->enc_part2->authorization_data,
                                authenticator->authorization_data,
                                KRB5_AUTHDATA_FX_ARMOR, &authdata);
    if (retval != 0)
        goto cleanup_authenticator;
    if (authdata&& authdata[0]) {
        k5_setmsg(kdc_context, KRB5KDC_ERR_POLICY,
                  "ticket valid only as FAST armor");
        retval = KRB5KDC_ERR_POLICY;
        krb5_free_authdata(kdc_context, authdata);
        goto cleanup_authenticator;
    }
    krb5_free_authdata(kdc_context, authdata);


    /* Check for a checksum */
    if (!(his_cksum = authenticator->checksum)) {
        retval = KRB5KRB_AP_ERR_INAPP_CKSUM;
        goto cleanup_authenticator;
    }

    /* make sure the client is of proper lineage (see above) */
    if (foreign_server &&
        !krb5int_find_pa_data(kdc_context,
                              request->padata, KRB5_PADATA_FOR_USER)) {
        if (is_local_principal(kdc_active_realm,
                               ticket->enc_part2->client)) {
            /* someone in a foreign realm claiming to be local */
            krb5_klog_syslog(LOG_INFO, _("PROCESS_TGS: failed lineage check"));
            retval = KRB5KDC_ERR_POLICY;
            goto cleanup_authenticator;
        }
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
        if (comp_cksum(kdc_context, &scratch1, ticket, his_cksum)) {
            if (!(retval = encode_krb5_kdc_req_body(request, &scratch)))
                retval = comp_cksum(kdc_context, scratch, ticket, his_cksum);
            krb5_free_data(kdc_context, scratch);
            if (retval)
                goto cleanup_authenticator;
        }
    }

    *pa_tgs_req = tmppa;
    *krbtgt_ptr = krbtgt;
    krbtgt = NULL;

cleanup_authenticator:
    krb5_free_authenticator(kdc_context, authenticator);

cleanup_auth_context:
    krb5_auth_con_free(kdc_context, auth_context);

cleanup:
    if (retval != 0) {
        krb5_free_keyblock(kdc_context, *tgskey);
        *tgskey = NULL;
    }
    if (apreq->ticket->enc_part2 != NULL) {
        /* Steal the decrypted ticket pointer, even on error. */
        *ticket_out = apreq->ticket;
        apreq->ticket = NULL;
    }
    krb5_free_ap_req(kdc_context, apreq);
    krb5_db_free_principal(kdc_context, krbtgt);
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
kdc_rd_ap_req(kdc_realm_t *kdc_active_realm,
              krb5_ap_req *apreq, krb5_auth_context auth_context,
              krb5_db_entry **server, krb5_keyblock **tgskey)
{
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

    retval = kdc_get_server_key(kdc_context, apreq->ticket,
                                KRB5_KDB_FLAG_ALIAS_OK, match_enctype, server,
                                NULL, NULL);
    if (retval)
        return retval;

    *tgskey = NULL;
    kvno = apreq->ticket->enc_part.kvno;
    do {
        krb5_free_keyblock(kdc_context, *tgskey);
        retval = find_server_key(kdc_context,
                                 *server, search_enctype, kvno, tgskey, &kvno);
        if (retval)
            continue;

        /* Make the TGS key available to krb5_rd_req_decoded_anyflag() */
        retval = krb5_auth_con_setuseruserkey(kdc_context, auth_context,
                                              *tgskey);
        if (retval)
            return retval;

        retval = krb5_rd_req_decoded_anyflag(kdc_context, &auth_context, apreq,
                                             apreq->ticket->server,
                                             kdc_active_realm->realm_keytab,
                                             NULL, NULL);

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

/*
 * If candidate is the local TGT for realm, set *alias_out to candidate and
 * *storage_out to NULL.  Otherwise, load the local TGT into *storage_out and
 * set *alias_out to *storage_out.
 *
 * In the future we might generalize this to a small per-request principal
 * cache.  For now, it saves a load operation in the common case where the AS
 * server or TGS header ticket server is the local TGT.
 */
krb5_error_code
get_local_tgt(krb5_context context, const krb5_data *realm,
              krb5_db_entry *candidate, krb5_db_entry **alias_out,
              krb5_db_entry **storage_out)
{
    krb5_error_code ret;
    krb5_principal princ;
    krb5_db_entry *tgt;

    *alias_out = NULL;
    *storage_out = NULL;

    ret = krb5_build_principal_ext(context, &princ, realm->length, realm->data,
                                   KRB5_TGS_NAME_SIZE, KRB5_TGS_NAME,
                                   realm->length, realm->data, 0);
    if (ret)
        return ret;

    if (!krb5_principal_compare(context, candidate->princ, princ)) {
        ret = krb5_db_get_principal(context, princ, 0, &tgt);
        if (!ret)
            *storage_out = *alias_out = tgt;
    } else {
        *alias_out = candidate;
    }

    krb5_free_principal(context, princ);
    return ret;
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


/* XXX!  This is a temporary place-holder */

krb5_error_code
check_hot_list(krb5_ticket *ticket)
{
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
check_anon(kdc_realm_t *kdc_active_realm,
           krb5_principal client, krb5_principal server)
{
    /* If restrict_anon is set, reject requests from anonymous to principals
     * other than the local TGT. */
    if (kdc_active_realm->realm_restrict_anon &&
        krb5_principal_compare_any_realm(kdc_context, client,
                                         krb5_anonymous_principal()) &&
        !krb5_principal_compare(kdc_context, server, tgs_server))
        return -1;
    return 0;
}

/*
 * Routines that validate a AS request; checks a lot of things.  :-)
 *
 * Returns a Kerberos protocol error number, which is _not_ the same
 * as a com_err error number!
 */
#define AS_INVALID_OPTIONS (KDC_OPT_FORWARDED | KDC_OPT_PROXY |         \
                            KDC_OPT_VALIDATE | KDC_OPT_RENEW |          \
                            KDC_OPT_ENC_TKT_IN_SKEY | KDC_OPT_CNAME_IN_ADDL_TKT)
int
validate_as_request(kdc_realm_t *kdc_active_realm,
                    register krb5_kdc_req *request, krb5_db_entry client,
                    krb5_db_entry server, krb5_timestamp kdc_time,
                    const char **status, krb5_pa_data ***e_data)
{
    int errcode;
    krb5_error_code ret;

    /*
     * If an option is set that is only allowed in TGS requests, complain.
     */
    if (request->kdc_options & AS_INVALID_OPTIONS) {
        *status = "INVALID AS OPTIONS";
        return KDC_ERR_BADOPTION;
    }

    /* The client must not be expired */
    if (client.expiration && client.expiration < kdc_time) {
        *status = "CLIENT EXPIRED";
        if (vague_errors)
            return(KRB_ERR_GENERIC);
        else
            return(KDC_ERR_NAME_EXP);
    }

    /* The client's password must not be expired, unless the server is
       a KRB5_KDC_PWCHANGE_SERVICE. */
    if (client.pw_expiration && client.pw_expiration < kdc_time &&
        !isflagset(server.attributes, KRB5_KDB_PWCHANGE_SERVICE)) {
        *status = "CLIENT KEY EXPIRED";
        if (vague_errors)
            return(KRB_ERR_GENERIC);
        else
            return(KDC_ERR_KEY_EXP);
    }

    /* The server must not be expired */
    if (server.expiration && server.expiration < kdc_time) {
        *status = "SERVICE EXPIRED";
        return(KDC_ERR_SERVICE_EXP);
    }

    /*
     * If the client requires password changing, then only allow the
     * pwchange service.
     */
    if (isflagset(client.attributes, KRB5_KDB_REQUIRES_PWCHANGE) &&
        !isflagset(server.attributes, KRB5_KDB_PWCHANGE_SERVICE)) {
        *status = "REQUIRED PWCHANGE";
        return(KDC_ERR_KEY_EXP);
    }

    /* Client and server must allow postdating tickets */
    if ((isflagset(request->kdc_options, KDC_OPT_ALLOW_POSTDATE) ||
         isflagset(request->kdc_options, KDC_OPT_POSTDATED)) &&
        (isflagset(client.attributes, KRB5_KDB_DISALLOW_POSTDATED) ||
         isflagset(server.attributes, KRB5_KDB_DISALLOW_POSTDATED))) {
        *status = "POSTDATE NOT ALLOWED";
        return(KDC_ERR_CANNOT_POSTDATE);
    }

    /*
     * A Windows KDC will return KDC_ERR_PREAUTH_REQUIRED instead of
     * KDC_ERR_POLICY in the following case:
     *
     *   - KDC_OPT_FORWARDABLE is set in KDCOptions but local
     *     policy has KRB5_KDB_DISALLOW_FORWARDABLE set for the
     *     client, and;
     *   - KRB5_KDB_REQUIRES_PRE_AUTH is set for the client but
     *     preauthentication data is absent in the request.
     *
     * Hence, this check most be done after the check for preauth
     * data, and is now performed by validate_forwardable() (the
     * contents of which were previously below).
     */

    /* Client and server must allow proxiable tickets */
    if (isflagset(request->kdc_options, KDC_OPT_PROXIABLE) &&
        (isflagset(client.attributes, KRB5_KDB_DISALLOW_PROXIABLE) ||
         isflagset(server.attributes, KRB5_KDB_DISALLOW_PROXIABLE))) {
        *status = "PROXIABLE NOT ALLOWED";
        return(KDC_ERR_POLICY);
    }

    /* Check to see if client is locked out */
    if (isflagset(client.attributes, KRB5_KDB_DISALLOW_ALL_TIX)) {
        *status = "CLIENT LOCKED OUT";
        return(KDC_ERR_CLIENT_REVOKED);
    }

    /* Check to see if server is locked out */
    if (isflagset(server.attributes, KRB5_KDB_DISALLOW_ALL_TIX)) {
        *status = "SERVICE LOCKED OUT";
        return(KDC_ERR_S_PRINCIPAL_UNKNOWN);
    }

    /* Check to see if server is allowed to be a service */
    if (isflagset(server.attributes, KRB5_KDB_DISALLOW_SVR)) {
        *status = "SERVICE NOT ALLOWED";
        return(KDC_ERR_MUST_USE_USER2USER);
    }

    if (check_anon(kdc_active_realm, client.princ, request->server) != 0) {
        *status = "ANONYMOUS NOT ALLOWED";
        return(KDC_ERR_POLICY);
    }

    /* Perform KDB module policy checks. */
    ret = krb5_db_check_policy_as(kdc_context, request, &client, &server,
                                  kdc_time, status, e_data);
    if (ret && ret != KRB5_PLUGIN_OP_NOTSUPP)
        return errcode_to_protocol(ret);

    /* Check against local policy. */
    errcode = against_local_policy_as(request, client, server,
                                      kdc_time, status, e_data);
    if (errcode)
        return errcode;

    return 0;
}

int
validate_forwardable(krb5_kdc_req *request, krb5_db_entry client,
                     krb5_db_entry server, krb5_timestamp kdc_time,
                     const char **status)
{
    *status = NULL;
    if (isflagset(request->kdc_options, KDC_OPT_FORWARDABLE) &&
        (isflagset(client.attributes, KRB5_KDB_DISALLOW_FORWARDABLE) ||
         isflagset(server.attributes, KRB5_KDB_DISALLOW_FORWARDABLE))) {
        *status = "FORWARDABLE NOT ALLOWED";
        return(KDC_ERR_POLICY);
    } else
        return 0;
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
dbentry_supports_enctype(kdc_realm_t *kdc_active_realm, krb5_db_entry *server,
                         krb5_enctype enctype)
{
    krb5_error_code     retval;
    krb5_key_data       *datap;
    char                *etypes_str = NULL;
    krb5_enctype        default_enctypes[1] = { 0 };
    krb5_enctype        *etypes = NULL;
    krb5_boolean        in_list;

    /* Look up the supported session key enctypes list in the KDB. */
    retval = krb5_dbe_get_string(kdc_context, server,
                                 KRB5_KDB_SK_SESSION_ENCTYPES,
                                 &etypes_str);
    if (retval == 0 && etypes_str != NULL && *etypes_str != '\0') {
        /* Pass a fake profile key for tracing of unrecognized tokens. */
        retval = krb5int_parse_enctype_list(kdc_context, "KDB-session_etypes",
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

    /* If configured to, assume every server without a session_enctypes
     * attribute supports DES_CBC_CRC. */
    if (kdc_active_realm->realm_assume_des_crc_sess &&
        enctype == ENCTYPE_DES_CBC_CRC)
        return TRUE;

    /* Due to an ancient interop problem, assume nothing supports des-cbc-md5
     * unless there's a session_enctypes explicitly saying that it does. */
    if (enctype == ENCTYPE_DES_CBC_MD5)
        return FALSE;

    /* Assume the server supports any enctype it has a long-term key for. */
    return !krb5_dbe_find_enctype(kdc_context, server, enctype, -1, 0, &datap);
}

/*
 * This function returns the keytype which should be selected for the
 * session key.  It is based on the ordered list which the user
 * requested, and what the KDC and the application server can support.
 */
krb5_enctype
select_session_keytype(kdc_realm_t *kdc_active_realm, krb5_db_entry *server,
                       int nktypes, krb5_enctype *ktype)
{
    int         i;

    for (i = 0; i < nktypes; i++) {
        if (!krb5_c_valid_enctype(ktype[i]))
            continue;

        if (!krb5_is_permitted_enctype(kdc_context, ktype[i]))
            continue;

        if (dbentry_supports_enctype(kdc_active_realm, server, ktype[i]))
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

/*
 * L10_2 = log10(2**x), rounded up; log10(2) ~= 0.301.
 */
#define L10_2(x) ((int)(((x * 301) + 999) / 1000))

/*
 * Max length of sprintf("%ld") for an int of type T; includes leading
 * minus sign and terminating NUL.
 */
#define D_LEN(t) (L10_2(sizeof(t) * CHAR_BIT) + 2)

void
ktypes2str(char *s, size_t len, int nktypes, krb5_enctype *ktype)
{
    int i;
    char stmp[D_LEN(krb5_enctype) + 1];
    char *p;

    if (nktypes < 0
        || len < (sizeof(" etypes {...}") + D_LEN(int))) {
        *s = '\0';
        return;
    }

    snprintf(s, len, "%d etypes {", nktypes);
    for (i = 0; i < nktypes; i++) {
        snprintf(stmp, sizeof(stmp), "%s%ld", i ? " " : "", (long)ktype[i]);
        if (strlen(s) + strlen(stmp) + sizeof("}") > len)
            break;
        strlcat(s, stmp, len);
    }
    if (i < nktypes) {
        /*
         * We broke out of the loop. Try to truncate the list.
         */
        p = s + strlen(s);
        while (p - s + sizeof("...}") > len) {
            while (p > s && *p != ' ' && *p != '{')
                *p-- = '\0';
            if (p > s && *p == ' ') {
                *p-- = '\0';
                continue;
            }
        }
        strlcat(s, "...", len);
    }
    strlcat(s, "}", len);
    return;
}

void
rep_etypes2str(char *s, size_t len, krb5_kdc_rep *rep)
{
    char stmp[sizeof("ses=") + D_LEN(krb5_enctype)];

    if (len < (3 * D_LEN(krb5_enctype)
               + sizeof("etypes {rep= tkt= ses=}"))) {
        *s = '\0';
        return;
    }

    snprintf(s, len, "etypes {rep=%ld", (long)rep->enc_part.enctype);

    if (rep->ticket != NULL) {
        snprintf(stmp, sizeof(stmp),
                 " tkt=%ld", (long)rep->ticket->enc_part.enctype);
        strlcat(s, stmp, len);
    }

    if (rep->ticket != NULL
        && rep->ticket->enc_part2 != NULL
        && rep->ticket->enc_part2->session != NULL) {
        snprintf(stmp, sizeof(stmp), " ses=%ld",
                 (long)rep->ticket->enc_part2->session->enctype);
        strlcat(s, stmp, len);
    }
    strlcat(s, "}", len);
    return;
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
kdc_process_for_user(kdc_realm_t *kdc_active_realm,
                     krb5_pa_data *pa_data,
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
    if (code)
        return code;

    code = verify_for_user_checksum(kdc_context, tgs_session, for_user);
    if (code) {
        *status = "INVALID_S4U2SELF_CHECKSUM";
        krb5_free_pa_for_user(kdc_context, for_user);
        return code;
    }

    *s4u_x509_user = calloc(1, sizeof(krb5_pa_s4u_x509_user));
    if (*s4u_x509_user == NULL) {
        krb5_free_pa_for_user(kdc_context, for_user);
        return ENOMEM;
    }

    (*s4u_x509_user)->user_id.user = for_user->user;
    for_user->user = NULL;
    krb5_free_pa_for_user(kdc_context, for_user);

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
    if (code)
        return code;

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

    if (krb5_princ_size(context, (*s4u_x509_user)->user_id.user) == 0 ||
        (*s4u_x509_user)->user_id.subject_cert.length != 0) {
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
    krb5_data                   *data = NULL;
    krb5_pa_s4u_x509_user       rep_s4u_user;
    krb5_pa_data                padata;
    krb5_enctype                enctype;
    krb5_keyusage               usage;

    memset(&rep_s4u_user, 0, sizeof(rep_s4u_user));

    rep_s4u_user.user_id.nonce   = req_s4u_user->user_id.nonce;
    rep_s4u_user.user_id.user    = req_s4u_user->user_id.user;
    rep_s4u_user.user_id.options =
        req_s4u_user->user_id.options & KRB5_S4U_OPTS_USE_REPLY_KEY_USAGE;

    code = encode_krb5_s4u_userid(&rep_s4u_user.user_id, &data);
    if (code != 0)
        goto cleanup;

    if (req_s4u_user->user_id.options & KRB5_S4U_OPTS_USE_REPLY_KEY_USAGE)
        usage = KRB5_KEYUSAGE_PA_S4U_X509_USER_REPLY;
    else
        usage = KRB5_KEYUSAGE_PA_S4U_X509_USER_REQUEST;

    code = krb5_c_make_checksum(context, req_s4u_user->cksum.checksum_type,
                                tgs_subkey != NULL ? tgs_subkey : tgs_session,
                                usage, data,
                                &rep_s4u_user.cksum);
    if (code != 0)
        goto cleanup;

    krb5_free_data(context, data);
    data = NULL;

    code = encode_krb5_pa_s4u_x509_user(&rep_s4u_user, &data);
    if (code != 0)
        goto cleanup;

    padata.magic = KV5M_PA_DATA;
    padata.pa_type = KRB5_PADATA_S4U_X509_USER;
    padata.length = data->length;
    padata.contents = (krb5_octet *)data->data;

    code = add_pa_data_element(context, &padata, &reply->padata, FALSE);
    if (code != 0)
        goto cleanup;

    free(data);
    data = NULL;

    if (tgs_subkey != NULL)
        enctype = tgs_subkey->enctype;
    else
        enctype = tgs_session->enctype;

    /*
     * Owing to a bug in Windows, unkeyed checksums were used for older
     * enctypes, including rc4-hmac. A forthcoming workaround for this
     * includes the checksum bytes in the encrypted padata.
     */
    if ((req_s4u_user->user_id.options & KRB5_S4U_OPTS_USE_REPLY_KEY_USAGE) &&
        enctype_requires_etype_info_2(enctype) == FALSE) {
        padata.length = req_s4u_user->cksum.length +
            rep_s4u_user.cksum.length;
        padata.contents = malloc(padata.length);
        if (padata.contents == NULL) {
            code = ENOMEM;
            goto cleanup;
        }

        memcpy(padata.contents,
               req_s4u_user->cksum.contents,
               req_s4u_user->cksum.length);
        memcpy(&padata.contents[req_s4u_user->cksum.length],
               rep_s4u_user.cksum.contents,
               rep_s4u_user.cksum.length);

        code = add_pa_data_element(context,&padata,
                                   &reply_encpart->enc_padata, FALSE);
        if (code != 0) {
            free(padata.contents);
            goto cleanup;
        }
    }

cleanup:
    if (rep_s4u_user.cksum.contents != NULL)
        krb5_free_checksum_contents(context, &rep_s4u_user.cksum);
    krb5_free_data(context, data);

    return code;
}

/*
 * Protocol transition (S4U2Self)
 */
krb5_error_code
kdc_process_s4u2self_req(kdc_realm_t *kdc_active_realm,
                         krb5_kdc_req *request,
                         krb5_const_principal client_princ,
                         const krb5_db_entry *server,
                         krb5_keyblock *tgs_subkey,
                         krb5_keyblock *tgs_session,
                         krb5_timestamp kdc_time,
                         krb5_pa_s4u_x509_user **s4u_x509_user,
                         krb5_db_entry **princ_ptr,
                         const char **status)
{
    krb5_error_code             code;
    krb5_pa_data                *pa_data;
    int                         flags;
    krb5_db_entry               *princ;

    *princ_ptr = NULL;

    pa_data = krb5int_find_pa_data(kdc_context,
                                   request->padata, KRB5_PADATA_S4U_X509_USER);
    if (pa_data != NULL) {
        code = kdc_process_s4u_x509_user(kdc_context,
                                         request,
                                         pa_data,
                                         tgs_subkey,
                                         tgs_session,
                                         s4u_x509_user,
                                         status);
        if (code != 0)
            return code;
    } else {
        pa_data = krb5int_find_pa_data(kdc_context,
                                       request->padata, KRB5_PADATA_FOR_USER);
        if (pa_data != NULL) {
            code = kdc_process_for_user(kdc_active_realm,
                                        pa_data,
                                        tgs_session,
                                        s4u_x509_user,
                                        status);
            if (code != 0)
                return code;
        } else
            return 0;
    }

    /*
     * We need to compare the client name in the TGT with the requested
     * server name. Supporting server name aliases without assuming a
     * global name service makes this difficult to do.
     *
     * The comparison below handles the following cases (note that the
     * term "principal name" below excludes the realm).
     *
     * (1) The requested service is a host-based service with two name
     *     components, in which case we assume the principal name to
     *     contain sufficient qualifying information. The realm is
     *     ignored for the purpose of comparison.
     *
     * (2) The requested service name is an enterprise principal name:
     *     the service principal name is compared with the unparsed
     *     form of the client name (including its realm).
     *
     * (3) The requested service is some other name type: an exact
     *     match is required.
     *
     * An alternative would be to look up the server once again with
     * FLAG_CANONICALIZE | FLAG_CLIENT_REFERRALS_ONLY set, do an exact
     * match between the returned name and client_princ. However, this
     * assumes that the client set FLAG_CANONICALIZE when requesting
     * the TGT and that we have a global name service.
     */
    flags = 0;
    switch (krb5_princ_type(kdc_context, request->server)) {
    case KRB5_NT_SRV_HST:                   /* (1) */
        if (krb5_princ_size(kdc_context, request->server) == 2)
            flags |= KRB5_PRINCIPAL_COMPARE_IGNORE_REALM;
        break;
    case KRB5_NT_ENTERPRISE_PRINCIPAL:      /* (2) */
        flags |= KRB5_PRINCIPAL_COMPARE_ENTERPRISE;
        break;
    default:                                /* (3) */
        break;
    }

    if (!krb5_principal_compare_flags(kdc_context,
                                      request->server,
                                      client_princ,
                                      flags)) {
        *status = "INVALID_S4U2SELF_REQUEST";
        return KRB5KDC_ERR_C_PRINCIPAL_UNKNOWN; /* match Windows error code */
    }

    /*
     * Protocol transition is mutually exclusive with renew/forward/etc
     * as well as user-to-user and constrained delegation. This check
     * is also made in validate_as_request().
     *
     * We can assert from this check that the header ticket was a TGT, as
     * that is validated previously in validate_tgs_request().
     */
    if (request->kdc_options & AS_INVALID_OPTIONS) {
        *status = "INVALID AS OPTIONS";
        return KRB5KDC_ERR_BADOPTION;
    }

    /*
     * Do not attempt to lookup principals in foreign realms.
     */
    if (is_local_principal(kdc_active_realm,
                           (*s4u_x509_user)->user_id.user)) {
        krb5_db_entry no_server;
        krb5_pa_data **e_data = NULL;

        code = krb5_db_get_principal(kdc_context,
                                     (*s4u_x509_user)->user_id.user,
                                     KRB5_KDB_FLAG_INCLUDE_PAC, &princ);
        if (code == KRB5_KDB_NOENTRY) {
            *status = "UNKNOWN_S4U2SELF_PRINCIPAL";
            return KRB5KDC_ERR_C_PRINCIPAL_UNKNOWN;
        } else if (code) {
            *status = "LOOKING_UP_S4U2SELF_PRINCIPAL";
            return code; /* caller can free for_user */
        }

        memset(&no_server, 0, sizeof(no_server));

        code = validate_as_request(kdc_active_realm, request, *princ,
                                   no_server, kdc_time, status, &e_data);
        if (code) {
            krb5_db_free_principal(kdc_context, princ);
            krb5_free_pa_data(kdc_context, e_data);
            return code;
        }

        *princ_ptr = princ;
    }

    return 0;
}

static krb5_error_code
check_allowed_to_delegate_to(krb5_context context, krb5_const_principal client,
                             const krb5_db_entry *server,
                             krb5_const_principal proxy)
{
    /* Can't get a TGT (otherwise it would be unconstrained delegation) */
    if (krb5_is_tgs_principal(proxy))
        return KRB5KDC_ERR_POLICY;

    /* Must be in same realm */
    if (!krb5_realm_compare(context, server->princ, proxy))
        return KRB5KDC_ERR_POLICY;

    return krb5_db_check_allowed_to_delegate(context, client, server, proxy);
}

krb5_error_code
kdc_process_s4u2proxy_req(kdc_realm_t *kdc_active_realm,
                          krb5_kdc_req *request,
                          const krb5_enc_tkt_part *t2enc,
                          const krb5_db_entry *server,
                          krb5_const_principal server_princ,
                          krb5_const_principal proxy_princ,
                          const char **status)
{
    krb5_error_code errcode;

    /*
     * Constrained delegation is mutually exclusive with renew/forward/etc.
     * We can assert from this check that the header ticket was a TGT, as
     * that is validated previously in validate_tgs_request().
     */
    if (request->kdc_options & (NON_TGT_OPTION | KDC_OPT_ENC_TKT_IN_SKEY)) {
        return KRB5KDC_ERR_BADOPTION;
    }

    /* Ensure that evidence ticket server matches TGT client */
    if (!krb5_principal_compare(kdc_context,
                                server->princ, /* after canon */
                                server_princ)) {
        return KRB5KDC_ERR_SERVER_NOMATCH;
    }

    if (!isflagset(t2enc->flags, TKT_FLG_FORWARDABLE)) {
        *status = "EVIDENCE_TKT_NOT_FORWARDABLE";
        return KRB5_TKT_NOT_FORWARDABLE;
    }

    /* Backend policy check */
    errcode = check_allowed_to_delegate_to(kdc_context,
                                           t2enc->client,
                                           server,
                                           proxy_princ);
    if (errcode) {
        *status = "NOT_ALLOWED_TO_DELEGATE";
        return errcode;
    }

    return 0;
}

krb5_error_code
kdc_check_transited_list(kdc_realm_t *kdc_active_realm,
                         const krb5_data *trans,
                         const krb5_data *realm1,
                         const krb5_data *realm2)
{
    krb5_error_code             code;

    /* Check against the KDB module.  Treat this answer as authoritative if the
     * method is supported and doesn't explicitly pass control. */
    code = krb5_db_check_transited_realms(kdc_context, trans, realm1, realm2);
    if (code != KRB5_PLUGIN_OP_NOTSUPP && code != KRB5_PLUGIN_NO_HANDLE)
        return code;

    /* Check using krb5.conf [capaths] or hierarchical relationships. */
    return krb5_check_transited_list(kdc_context, trans, realm1, realm2);
}

krb5_error_code
validate_transit_path(krb5_context context,
                      krb5_const_principal client,
                      krb5_db_entry *server,
                      krb5_db_entry *header_srv)
{
    /* Incoming */
    if (isflagset(server->attributes, KRB5_KDB_XREALM_NON_TRANSITIVE)) {
        return KRB5KDC_ERR_PATH_NOT_ACCEPTED;
    }

    /* Outgoing */
    if (isflagset(header_srv->attributes, KRB5_KDB_XREALM_NON_TRANSITIVE) &&
        (!krb5_principal_compare(context, server->princ, header_srv->princ) ||
         !krb5_realm_compare(context, client, header_srv->princ))) {
        return KRB5KDC_ERR_PATH_NOT_ACCEPTED;
    }

    return 0;
}

krb5_boolean
enctype_requires_etype_info_2(krb5_enctype enctype)
{
    switch(enctype) {
    case ENCTYPE_DES_CBC_CRC:
    case ENCTYPE_DES_CBC_MD4:
    case ENCTYPE_DES_CBC_MD5:
    case ENCTYPE_DES3_CBC_SHA1:
    case ENCTYPE_DES3_CBC_RAW:
    case ENCTYPE_ARCFOUR_HMAC:
    case ENCTYPE_ARCFOUR_HMAC_EXP :
        return 0;
    default:
        return krb5_c_valid_enctype(enctype);
    }
}

/* XXX where are the generic helper routines for this? */
krb5_error_code
add_pa_data_element(krb5_context context,
                    krb5_pa_data *padata,
                    krb5_pa_data ***inout_padata,
                    krb5_boolean copy)
{
    int                         i;
    krb5_pa_data                **p;

    if (*inout_padata != NULL) {
        for (i = 0; (*inout_padata)[i] != NULL; i++)
            ;
    } else
        i = 0;

    p = realloc(*inout_padata, (i + 2) * sizeof(krb5_pa_data *));
    if (p == NULL)
        return ENOMEM;

    *inout_padata = p;

    p[i] = (krb5_pa_data *)malloc(sizeof(krb5_pa_data));
    if (p[i] == NULL)
        return ENOMEM;
    *(p[i]) = *padata;

    p[i + 1] = NULL;

    if (copy) {
        p[i]->contents = (krb5_octet *)malloc(padata->length);
        if (p[i]->contents == NULL) {
            free(p[i]);
            p[i] = NULL;
            return ENOMEM;
        }

        memcpy(p[i]->contents, padata->contents, padata->length);
    }

    return 0;
}

void
kdc_get_ticket_endtime(kdc_realm_t *kdc_active_realm,
                       krb5_timestamp starttime,
                       krb5_timestamp endtime,
                       krb5_timestamp till,
                       krb5_db_entry *client,
                       krb5_db_entry *server,
                       krb5_timestamp *out_endtime)
{
    krb5_timestamp until, life;

    if (till == 0)
        till = kdc_infinity;

    until = min(till, endtime);

    life = until - starttime;

    if (client != NULL && client->max_life != 0)
        life = min(life, client->max_life);
    if (server->max_life != 0)
        life = min(life, server->max_life);
    if (kdc_active_realm->realm_maxlife != 0)
        life = min(life, kdc_active_realm->realm_maxlife);

    *out_endtime = starttime + life;
}

/*
 * Set tkt->renew_till to the requested renewable lifetime as modified by
 * policy.  Set the TKT_FLG_RENEWABLE flag if we set a nonzero renew_till.
 * client and tgt may be NULL.
 */
void
kdc_get_ticket_renewtime(kdc_realm_t *realm, krb5_kdc_req *request,
                         krb5_enc_tkt_part *tgt, krb5_db_entry *client,
                         krb5_db_entry *server, krb5_enc_tkt_part *tkt)
{
    krb5_timestamp rtime, max_rlife;

    tkt->times.renew_till = 0;

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
             tkt->times.endtime < request->till)
        rtime = request->till;
    else
        return;

    /* Truncate it to the allowable renewable time. */
    if (tgt != NULL)
        rtime = min(rtime, tgt->times.renew_till);
    max_rlife = min(server->max_renewable_life, realm->realm_maxrlife);
    if (client != NULL)
        max_rlife = min(max_rlife, client->max_renewable_life);
    rtime = min(rtime, tkt->times.starttime + max_rlife);

    /* Make the ticket renewable if the truncated requested time is larger than
     * the ticket end time. */
    if (rtime > tkt->times.endtime) {
        setflag(tkt->flags, TKT_FLG_RENEWABLE);
        tkt->times.renew_till = rtime;
    }
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
    krb5_data *out = NULL;
    krb5_pa_data pa, *pa_in;
    pa_in = krb5int_find_pa_data(context, request->padata,
                                 KRB5_ENCPADATA_REQ_ENC_PA_REP);
    if (pa_in == NULL)
        return 0;
    pa.magic = KV5M_PA_DATA;
    pa.pa_type = KRB5_ENCPADATA_REQ_ENC_PA_REP;
    memset(&checksum, 0, sizeof(checksum));
    retval = krb5_c_make_checksum(context,0, reply_key,
                                  KRB5_KEYUSAGE_AS_REQ, req_pkt, &checksum);
    if (retval != 0)
        goto cleanup;
    retval = encode_krb5_checksum(&checksum, &out);
    if (retval != 0)
        goto cleanup;
    pa.contents = (krb5_octet *) out->data;
    pa.length = out->length;
    retval = add_pa_data_element(context, &pa, out_enc_padata, FALSE);
    if (retval)
        goto cleanup;
    out->data = NULL;
    pa.magic = KV5M_PA_DATA;
    pa.pa_type = KRB5_PADATA_FX_FAST;
    pa.length = 0;
    pa.contents = NULL;
    retval = add_pa_data_element(context, &pa, out_enc_padata, FALSE);
cleanup:
    if (checksum.contents)
        krb5_free_checksum_contents(context, &checksum);
    if (out != NULL)
        krb5_free_data(context, out);
    return retval;
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
