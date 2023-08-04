/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* lib/krb5/krb/mk_req_ext.c */
/*
 * Copyright 1990,1991 by the Massachusetts Institute of Technology.
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
 *
 * krb5_mk_req_extended()
 */


#include "k5-int.h"
#include "int-proto.h"
#include "auth_con.h"

/*
  Formats a KRB_AP_REQ message into outbuf, with more complete options than
  krb_mk_req.

  outbuf, ap_req_options, checksum, and ccache are used in the
  same fashion as for krb5_mk_req.

  creds is used to supply the credentials (ticket and session key) needed
  to form the request.

  if creds->ticket has no data (length == 0), then a ticket is obtained
  from either the cache or the TGS, passing creds to krb5_get_credentials().
  kdc_options specifies the options requested for the ticket to be used.
  If a ticket with appropriate flags is not found in the cache, then these
  options are passed on in a request to an appropriate KDC.

  ap_req_options specifies the KRB_AP_REQ options desired.

  if ap_req_options specifies AP_OPTS_USE_SESSION_KEY, then creds->ticket
  must contain the appropriate ENC-TKT-IN-SKEY ticket.

  checksum specifies the checksum to be used in the authenticator.

  The outbuf buffer storage is allocated, and should be freed by the
  caller when finished.

  On an error return, the credentials pointed to by creds might have been
  augmented with additional fields from the obtained credentials; the entire
  credentials should be released by calling krb5_free_creds().

  returns system errors
*/

static krb5_error_code
make_ap_authdata(krb5_context context, krb5_enctype *desired_enctypes,
                 krb5_enctype tkt_enctype, krb5_boolean client_aware_cb,
                 krb5_authdata ***authdata_out);

static krb5_error_code
generate_authenticator(krb5_context,
                       krb5_authenticator *, krb5_principal,
                       krb5_checksum *, krb5_key,
                       krb5_ui_4, krb5_authdata **,
                       krb5_authdata_context ad_context,
                       krb5_enctype *desired_etypes,
                       krb5_enctype tkt_enctype);

krb5_error_code KRB5_CALLCONV
krb5_mk_req_extended(krb5_context context, krb5_auth_context *auth_context,
                     krb5_flags ap_req_options, krb5_data *in_data,
                     krb5_creds *in_creds, krb5_data *outbuf)
{
    krb5_error_code       retval;
    krb5_checksum         checksum;
    krb5_checksum         *checksump = 0;
    krb5_auth_context     new_auth_context;
    krb5_enctype          *desired_etypes = NULL;

    krb5_ap_req request;
    krb5_data *scratch = 0;
    krb5_data *toutbuf;

    request.ap_options = ap_req_options & AP_OPTS_WIRE_MASK;
    request.authenticator.ciphertext.data = NULL;
    request.ticket = 0;

    if (!in_creds->ticket.length)
        return(KRB5_NO_TKT_SUPPLIED);

    if ((ap_req_options & AP_OPTS_ETYPE_NEGOTIATION) &&
        !(ap_req_options & AP_OPTS_MUTUAL_REQUIRED))
        return(EINVAL);

    /* we need a native ticket */
    if ((retval = decode_krb5_ticket(&(in_creds)->ticket, &request.ticket)))
        return(retval);

    /* verify that the ticket is not expired */
    if ((retval = krb5int_validate_times(context, &in_creds->times)) != 0)
        goto cleanup;

    /* generate auth_context if needed */
    if (*auth_context == NULL) {
        if ((retval = krb5_auth_con_init(context, &new_auth_context)))
            goto cleanup;
        *auth_context = new_auth_context;
    }

    if ((*auth_context)->key != NULL) {
        krb5_k_free_key(context, (*auth_context)->key);
        (*auth_context)->key = NULL;
    }

    /* set auth context keyblock */
    if ((retval = krb5_k_create_key(context, &in_creds->keyblock,
                                    &((*auth_context)->key))))
        goto cleanup;

    /* generate seq number if needed */
    if ((((*auth_context)->auth_context_flags & KRB5_AUTH_CONTEXT_DO_SEQUENCE)
         || ((*auth_context)->auth_context_flags & KRB5_AUTH_CONTEXT_RET_SEQUENCE))
        && ((*auth_context)->local_seq_number == 0)) {
        if ((retval = krb5_generate_seq_number(context, &in_creds->keyblock,
                                               &(*auth_context)->local_seq_number)))
            goto cleanup;
    }

    /* generate subkey if needed */
    if ((ap_req_options & AP_OPTS_USE_SUBKEY)&&(!(*auth_context)->send_subkey)) {
        retval = k5_generate_and_save_subkey(context, *auth_context,
                                             &in_creds->keyblock,
                                             in_creds->keyblock.enctype);
        if (retval)
            goto cleanup;
    }


    if (!in_data && (*auth_context)->checksum_func) {
        retval = (*auth_context)->checksum_func( context,
                                                 *auth_context,
                                                 (*auth_context)->checksum_func_data,
                                                 &in_data);
        if (retval)
            goto cleanup;
    }

    if (in_data) {
        if ((*auth_context)->req_cksumtype == 0x8003) {
            /* XXX Special hack for GSSAPI */
            checksum.checksum_type = 0x8003;
            checksum.length = in_data->length;
            checksum.contents = (krb5_octet *) in_data->data;
        } else {
            retval = krb5_k_make_checksum(context, 0, (*auth_context)->key,
                                          KRB5_KEYUSAGE_AP_REQ_AUTH_CKSUM,
                                          in_data, &checksum);
            if (retval)
                goto cleanup_cksum;
        }
        checksump = &checksum;
    }

    /* Generate authenticator */
    if (((*auth_context)->authentp = (krb5_authenticator *)malloc(sizeof(
                                                                      krb5_authenticator))) == NULL) {
        retval = ENOMEM;
        goto cleanup_cksum;
    }

    if (ap_req_options & AP_OPTS_ETYPE_NEGOTIATION) {
        if ((*auth_context)->permitted_etypes == NULL) {
            retval = krb5_get_tgs_ktypes(context, in_creds->server, &desired_etypes);
            if (retval)
                goto cleanup_cksum;
        } else
            desired_etypes = (*auth_context)->permitted_etypes;
    }

    TRACE_MK_REQ(context, in_creds, (*auth_context)->local_seq_number,
                 (*auth_context)->send_subkey, &in_creds->keyblock);
    if ((retval = generate_authenticator(context,
                                         (*auth_context)->authentp,
                                         in_creds->client, checksump,
                                         (*auth_context)->send_subkey,
                                         (*auth_context)->local_seq_number,
                                         in_creds->authdata,
                                         (*auth_context)->ad_context,
                                         desired_etypes,
                                         in_creds->keyblock.enctype)))
        goto cleanup_cksum;

    /* encode the authenticator */
    if ((retval = encode_krb5_authenticator((*auth_context)->authentp,
                                            &scratch)))
        goto cleanup_cksum;

    /* call the encryption routine */
    if ((retval = krb5_encrypt_helper(context, &in_creds->keyblock,
                                      KRB5_KEYUSAGE_AP_REQ_AUTH,
                                      scratch, &request.authenticator)))
        goto cleanup_cksum;

    if ((retval = encode_krb5_ap_req(&request, &toutbuf)))
        goto cleanup_cksum;
    *outbuf = *toutbuf;

    free(toutbuf);

cleanup_cksum:
    /* Null out these fields, to prevent pointer sharing problems;
     * they were supplied by the caller
     */
    if ((*auth_context)->authentp != NULL) {
        (*auth_context)->authentp->client = NULL;
        (*auth_context)->authentp->checksum = NULL;
    }
    if (checksump && checksump->checksum_type != 0x8003)
        free(checksump->contents);

cleanup:
    if (desired_etypes &&
        desired_etypes != (*auth_context)->permitted_etypes)
        free(desired_etypes);
    if (request.ticket)
        krb5_free_ticket(context, request.ticket);
    if (request.authenticator.ciphertext.data) {
        (void) memset(request.authenticator.ciphertext.data, 0,
                      request.authenticator.ciphertext.length);
        free(request.authenticator.ciphertext.data);
    }
    if (scratch) {
        memset(scratch->data, 0, scratch->length);
        free(scratch->data);
        free(scratch);
    }
    return retval;
}

static krb5_error_code
generate_authenticator(krb5_context context, krb5_authenticator *authent,
                       krb5_principal client, krb5_checksum *cksum,
                       krb5_key key, krb5_ui_4 seq_number,
                       krb5_authdata **authorization,
                       krb5_authdata_context ad_context,
                       krb5_enctype *desired_etypes,
                       krb5_enctype tkt_enctype)
{
    krb5_error_code retval;
    krb5_authdata **ext_authdata = NULL, **ap_authdata, **combined;
    int client_aware_cb;

    authent->client = client;
    authent->checksum = cksum;
    if (key) {
        retval = krb5_k_key_keyblock(context, key, &authent->subkey);
        if (retval)
            return retval;
    } else
        authent->subkey = 0;
    authent->seq_number = seq_number;
    authent->authorization_data = NULL;

    if (ad_context != NULL) {
        retval = krb5_authdata_export_authdata(context,
                                               ad_context,
                                               AD_USAGE_AP_REQ,
                                               &ext_authdata);
        if (retval)
            return retval;
    }

    if (authorization != NULL || ext_authdata != NULL) {
        retval = krb5_merge_authdata(context,
                                     authorization,
                                     ext_authdata,
                                     &authent->authorization_data);
        if (retval) {
            krb5_free_authdata(context, ext_authdata);
            return retval;
        }
        krb5_free_authdata(context, ext_authdata);
    }

    retval = profile_get_boolean(context->profile, KRB5_CONF_LIBDEFAULTS,
                                 KRB5_CONF_CLIENT_AWARE_GSS_BINDINGS, NULL,
                                 FALSE, &client_aware_cb);
    if (retval)
        return retval;

    /* Add etype negotiation or channel-binding awareness authdata to the
     * front, if appropriate. */
    retval = make_ap_authdata(context, desired_etypes, tkt_enctype,
                              client_aware_cb, &ap_authdata);
    if (retval)
        return retval;
    if (ap_authdata != NULL) {
        retval = krb5_merge_authdata(context, ap_authdata,
                                     authent->authorization_data, &combined);
        krb5_free_authdata(context, ap_authdata);
        if (retval)
            return retval;
        krb5_free_authdata(context, authent->authorization_data);
        authent->authorization_data = combined;
    }

    return(krb5_us_timeofday(context, &authent->ctime, &authent->cusec));
}

/* Set *out to a DER-encoded RFC 4537 etype list, or to NULL if no etype list
 * should be sent. */
static krb5_error_code
make_etype_list(krb5_context context, krb5_enctype *desired_enctypes,
                krb5_enctype tkt_enctype, krb5_data **out)
{
    krb5_etype_list etlist;
    int count;

    *out = NULL;

    /* Only send a list if we prefer another enctype to tkt_enctype. */
    if (desired_enctypes == NULL || desired_enctypes[0] == tkt_enctype)
        return 0;

    /* Count elements of desired_etypes, stopping at tkt_enctypes if present.
     * (Per RFC 4537, it must be the last option if it is included.) */
    for (count = 0; desired_enctypes[count] != ENCTYPE_NULL; count++) {
        if (count > 0 && desired_enctypes[count - 1] == tkt_enctype)
            break;
    }

    etlist.etypes = desired_enctypes;
    etlist.length = count;
    return encode_krb5_etype_list(&etlist, out);
}

/* Set *authdata_out to appropriate authenticator authdata for the request,
 * encoded in a single AD_IF_RELEVANT element. */
static krb5_error_code
make_ap_authdata(krb5_context context, krb5_enctype *desired_enctypes,
                 krb5_enctype tkt_enctype, krb5_boolean client_aware_cb,
                 krb5_authdata ***authdata_out)
{
    krb5_error_code ret;
    krb5_authdata etypes_ad, flags_ad, *list[3];
    krb5_data *der_etypes = NULL;
    size_t count = 0;
    uint8_t flagbuf[4];
    const uint32_t KERB_AP_OPTIONS_CBT = 0x4000;

    *authdata_out = NULL;

    /* Include an ETYPE_NEGOTIATION element if appropriate. */
    ret = make_etype_list(context, desired_enctypes, tkt_enctype, &der_etypes);
    if (ret)
        goto cleanup;
    if (der_etypes != NULL) {
        etypes_ad.magic = KV5M_AUTHDATA;
        etypes_ad.ad_type = KRB5_AUTHDATA_ETYPE_NEGOTIATION;
        etypes_ad.length = der_etypes->length;
        etypes_ad.contents = (uint8_t *)der_etypes->data;
        list[count++] = &etypes_ad;
    }

    /* Include an AP_OPTIONS element if the CBT flag is configured. */
    if (client_aware_cb != 0) {
        store_32_le(KERB_AP_OPTIONS_CBT, flagbuf);
        flags_ad.magic = KV5M_AUTHDATA;
        flags_ad.ad_type = KRB5_AUTHDATA_AP_OPTIONS;
        flags_ad.length = 4;
        flags_ad.contents = flagbuf;
        list[count++] = &flags_ad;
    }

    if (count > 0) {
        list[count] = NULL;
        ret = krb5_encode_authdata_container(context,
                                             KRB5_AUTHDATA_IF_RELEVANT,
                                             list, authdata_out);
    }

cleanup:
    krb5_free_data(context, der_etypes);
    return ret;
}
