/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/*
 * COPYRIGHT (C) 2006,2007
 * THE REGENTS OF THE UNIVERSITY OF MICHIGAN
 * ALL RIGHTS RESERVED
 *
 * Permission is granted to use, copy, create derivative works
 * and redistribute this software and such derivative works
 * for any purpose, so long as the name of The University of
 * Michigan is not used in any advertising or publicity
 * pertaining to the use of distribution of this software
 * without specific, written prior authorization.  If the
 * above copyright notice or any other identification of the
 * University of Michigan is included in any copy of any
 * portion of this software, then the disclaimer below must
 * also be included.
 *
 * THIS SOFTWARE IS PROVIDED AS IS, WITHOUT REPRESENTATION
 * FROM THE UNIVERSITY OF MICHIGAN AS TO ITS FITNESS FOR ANY
 * PURPOSE, AND WITHOUT WARRANTY BY THE UNIVERSITY OF
 * MICHIGAN OF ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING
 * WITHOUT LIMITATION THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE. THE
 * REGENTS OF THE UNIVERSITY OF MICHIGAN SHALL NOT BE LIABLE
 * FOR ANY DAMAGES, INCLUDING SPECIAL, INDIRECT, INCIDENTAL, OR
 * CONSEQUENTIAL DAMAGES, WITH RESPECT TO ANY CLAIM ARISING
 * OUT OF OR IN CONNECTION WITH THE USE OF THE SOFTWARE, EVEN
 * IF IT HAS BEEN OR IS HEREAFTER ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGES.
 */

#include <k5-int.h>
#include "pkinit.h"
#include "krb5/certauth_plugin.h"

/* Aliases used by the built-in certauth modules */
struct certauth_req_opts {
    krb5_kdcpreauth_callbacks cb;
    krb5_kdcpreauth_rock rock;
    pkinit_kdc_context plgctx;
    pkinit_kdc_req_context reqctx;
};

typedef struct certauth_module_handle_st {
    struct krb5_certauth_vtable_st vt;
    krb5_certauth_moddata moddata;
} *certauth_handle;

struct krb5_kdcpreauth_moddata_st {
    pkinit_kdc_context *realm_contexts;
    certauth_handle *certauth_modules;
};

static krb5_error_code
pkinit_init_kdc_req_context(krb5_context, pkinit_kdc_req_context *blob);

static void
pkinit_fini_kdc_req_context(krb5_context context, void *blob);

static void
pkinit_server_plugin_fini_realm(krb5_context context,
                                pkinit_kdc_context plgctx);

static void
pkinit_server_plugin_fini(krb5_context context,
                          krb5_kdcpreauth_moddata moddata);

static pkinit_kdc_context
pkinit_find_realm_context(krb5_context context,
                          krb5_kdcpreauth_moddata moddata,
                          krb5_principal princ);

static void
free_realm_contexts(krb5_context context, pkinit_kdc_context *realm_contexts)
{
    int i;

    if (realm_contexts == NULL)
        return;
    for (i = 0; realm_contexts[i] != NULL; i++)
        pkinit_server_plugin_fini_realm(context, realm_contexts[i]);
    pkiDebug("%s: freeing context at %p\n", __FUNCTION__, realm_contexts);
    free(realm_contexts);
}

static void
free_certauth_handles(krb5_context context, certauth_handle *list)
{
    int i;

    if (list == NULL)
        return;
    for (i = 0; list[i] != NULL; i++) {
        if (list[i]->vt.fini != NULL)
            list[i]->vt.fini(context, list[i]->moddata);
        free(list[i]);
    }
    free(list);
}

static krb5_error_code
pkinit_create_edata(krb5_context context,
                    pkinit_plg_crypto_context plg_cryptoctx,
                    pkinit_req_crypto_context req_cryptoctx,
                    pkinit_identity_crypto_context id_cryptoctx,
                    pkinit_plg_opts *opts,
                    krb5_error_code err_code,
                    krb5_pa_data ***e_data_out)
{
    krb5_error_code retval = KRB5KRB_ERR_GENERIC;

    pkiDebug("pkinit_create_edata: creating edata for error %d (%s)\n",
             err_code, error_message(err_code));
    switch(err_code) {
    case KRB5KDC_ERR_CANT_VERIFY_CERTIFICATE:
        retval = pkinit_create_td_trusted_certifiers(context,
                                                     plg_cryptoctx, req_cryptoctx, id_cryptoctx, e_data_out);
        break;
    case KRB5KDC_ERR_DH_KEY_PARAMETERS_NOT_ACCEPTED:
        retval = pkinit_create_td_dh_parameters(context, plg_cryptoctx,
                                                req_cryptoctx, id_cryptoctx, opts, e_data_out);
        break;
    case KRB5KDC_ERR_INVALID_CERTIFICATE:
    case KRB5KDC_ERR_REVOKED_CERTIFICATE:
        retval = pkinit_create_td_invalid_certificate(context,
                                                      plg_cryptoctx, req_cryptoctx, id_cryptoctx, e_data_out);
        break;
    default:
        pkiDebug("no edata needed for error %d (%s)\n",
                 err_code, error_message(err_code));
        retval = 0;
        goto cleanup;
    }

cleanup:

    return retval;
}

static void
pkinit_server_get_edata(krb5_context context,
                        krb5_kdc_req *request,
                        krb5_kdcpreauth_callbacks cb,
                        krb5_kdcpreauth_rock rock,
                        krb5_kdcpreauth_moddata moddata,
                        krb5_preauthtype pa_type,
                        krb5_kdcpreauth_edata_respond_fn respond,
                        void *arg)
{
    krb5_error_code retval = 0;
    pkinit_kdc_context plgctx = NULL;

    pkiDebug("pkinit_server_get_edata: entered!\n");


    /*
     * If we don't have a realm context for the given realm,
     * don't tell the client that we support pkinit!
     */
    plgctx = pkinit_find_realm_context(context, moddata, request->server);
    if (plgctx == NULL)
        retval = EINVAL;

    /* Send a freshness token if the client requested one. */
    if (!retval)
        cb->send_freshness_token(context, rock);

    (*respond)(arg, retval, NULL);
}

static krb5_error_code
verify_client_san(krb5_context context,
                  pkinit_kdc_context plgctx,
                  pkinit_kdc_req_context reqctx,
                  krb5_kdcpreauth_callbacks cb,
                  krb5_kdcpreauth_rock rock,
                  krb5_const_principal client,
                  int *valid_san)
{
    krb5_error_code retval;
    krb5_principal *princs = NULL, upn;
    krb5_boolean match;
    char **upns = NULL;
    int i;
#ifdef DEBUG_SAN_INFO
    char *client_string = NULL, *san_string;
#endif

    *valid_san = 0;
    retval = crypto_retrieve_cert_sans(context, plgctx->cryptoctx,
                                       reqctx->cryptoctx, plgctx->idctx,
                                       &princs,
                                       plgctx->opts->allow_upn ? &upns : NULL,
                                       NULL);
    if (retval) {
        pkiDebug("%s: error from retrieve_certificate_sans()\n", __FUNCTION__);
        retval = KRB5KDC_ERR_CLIENT_NAME_MISMATCH;
        goto out;
    }

    if (princs == NULL && upns == NULL) {
        TRACE_PKINIT_SERVER_NO_SAN(context);
        retval = ENOENT;
        goto out;
    }

#ifdef DEBUG_SAN_INFO
    krb5_unparse_name(context, client, &client_string);
#endif
    pkiDebug("%s: Checking pkinit sans\n", __FUNCTION__);
    for (i = 0; princs != NULL && princs[i] != NULL; i++) {
#ifdef DEBUG_SAN_INFO
        krb5_unparse_name(context, princs[i], &san_string);
        pkiDebug("%s: Comparing client '%s' to pkinit san value '%s'\n",
                 __FUNCTION__, client_string, san_string);
        krb5_free_unparsed_name(context, san_string);
#endif
        if (cb->match_client(context, rock, princs[i])) {
            TRACE_PKINIT_SERVER_MATCHING_SAN_FOUND(context);
            *valid_san = 1;
            retval = 0;
            goto out;
        }
    }
    pkiDebug("%s: no pkinit san match found\n", __FUNCTION__);
    /*
     * XXX if cert has names but none match, should we
     * be returning KRB5KDC_ERR_CLIENT_NAME_MISMATCH here?
     */

    if (upns == NULL) {
        pkiDebug("%s: no upn sans (or we wouldn't accept them anyway)\n",
                 __FUNCTION__);
        retval = KRB5KDC_ERR_CLIENT_NAME_MISMATCH;
        goto out;
    }

    pkiDebug("%s: Checking upn sans\n", __FUNCTION__);
    for (i = 0; upns[i] != NULL; i++) {
#ifdef DEBUG_SAN_INFO
        pkiDebug("%s: Comparing client '%s' to upn san value '%s'\n",
                 __FUNCTION__, client_string, upns[i]);
#endif
        retval = krb5_parse_name_flags(context, upns[i],
                                       KRB5_PRINCIPAL_PARSE_ENTERPRISE, &upn);
        if (retval) {
            TRACE_PKINIT_SERVER_UPN_PARSE_FAIL(context, upns[i], retval);
            continue;
        }
        match = cb->match_client(context, rock, upn);
        krb5_free_principal(context, upn);
        if (match) {
            TRACE_PKINIT_SERVER_MATCHING_UPN_FOUND(context);
            *valid_san = 1;
            retval = 0;
            goto out;
        }
    }
    pkiDebug("%s: no upn san match found\n", __FUNCTION__);

    retval = 0;
out:
    if (princs != NULL) {
        for (i = 0; princs[i] != NULL; i++)
            krb5_free_principal(context, princs[i]);
        free(princs);
    }
    if (upns != NULL) {
        for (i = 0; upns[i] != NULL; i++)
            free(upns[i]);
        free(upns);
    }
#ifdef DEBUG_SAN_INFO
    if (client_string != NULL)
        krb5_free_unparsed_name(context, client_string);
#endif
    pkiDebug("%s: returning retval %d, valid_san %d\n",
             __FUNCTION__, retval, *valid_san);
    return retval;
}

static krb5_error_code
verify_client_eku(krb5_context context,
                  pkinit_kdc_context plgctx,
                  pkinit_kdc_req_context reqctx,
                  int *eku_accepted)
{
    krb5_error_code retval;

    *eku_accepted = 0;

    if (plgctx->opts->require_eku == 0) {
        TRACE_PKINIT_SERVER_EKU_SKIP(context);
        *eku_accepted = 1;
        retval = 0;
        goto out;
    }

    retval = crypto_check_cert_eku(context, plgctx->cryptoctx,
                                   reqctx->cryptoctx, plgctx->idctx,
                                   0, /* kdc cert */
                                   plgctx->opts->accept_secondary_eku,
                                   eku_accepted);
    if (retval) {
        pkiDebug("%s: Error from crypto_check_cert_eku %d (%s)\n",
                 __FUNCTION__, retval, error_message(retval));
        goto out;
    }

out:
    pkiDebug("%s: returning retval %d, eku_accepted %d\n",
             __FUNCTION__, retval, *eku_accepted);
    return retval;
}


/* Run the received, verified certificate through certauth modules, to verify
 * that it is authorized to authenticate as client. */
static krb5_error_code
authorize_cert(krb5_context context, certauth_handle *certauth_modules,
               pkinit_kdc_context plgctx, pkinit_kdc_req_context reqctx,
               krb5_kdcpreauth_callbacks cb, krb5_kdcpreauth_rock rock,
               krb5_principal client, krb5_boolean *hwauth_out)
{
    krb5_error_code ret;
    certauth_handle h;
    struct certauth_req_opts opts;
    krb5_boolean accepted = FALSE, hwauth = FALSE;
    uint8_t *cert;
    size_t i, cert_len;
    void *db_ent = NULL;
    char **ais = NULL, **ai = NULL;

    /* Re-encode the received certificate into DER, which is extra work, but
     * avoids creating an X.509 library dependency in the interface. */
    ret = crypto_encode_der_cert(context, reqctx->cryptoctx, &cert, &cert_len);
    if (ret)
        goto cleanup;

    /* Set options for the builtin module. */
    opts.plgctx = plgctx;
    opts.reqctx = reqctx;
    opts.cb = cb;
    opts.rock = rock;

    db_ent = cb->client_entry(context, rock);

    /*
     * Check the certificate against each certauth module.  For the certificate
     * to be authorized at least one module must return 0 or
     * KRB5_CERTAUTH_HWAUTH, and no module can return an error code other than
     * KRB5_PLUGIN_NO_HANDLE (pass) or KRB5_CERTAUTH_HWAUTH_PASS (pass but
     * set hw-authent).  Add indicators from all modules.
     */
    ret = KRB5_PLUGIN_NO_HANDLE;
    for (i = 0; certauth_modules != NULL && certauth_modules[i] != NULL; i++) {
        h = certauth_modules[i];
        TRACE_PKINIT_SERVER_CERT_AUTH(context, h->vt.name);
        ret = h->vt.authorize(context, h->moddata, cert, cert_len, client,
                              &opts, db_ent, &ais);
        if (ret == 0)
            accepted = TRUE;
        else if (ret == KRB5_CERTAUTH_HWAUTH)
            accepted = hwauth = TRUE;
        else if (ret == KRB5_CERTAUTH_HWAUTH_PASS)
            hwauth = TRUE;
        else if (ret != KRB5_PLUGIN_NO_HANDLE)
            goto cleanup;

        if (ais != NULL) {
            /* Assert authentication indicators from the module. */
            for (ai = ais; *ai != NULL; ai++) {
                ret = cb->add_auth_indicator(context, rock, *ai);
                if (ret)
                    goto cleanup;
            }
            h->vt.free_ind(context, h->moddata, ais);
            ais = NULL;
        }
    }

    *hwauth_out = hwauth;
    ret = accepted ? 0 : KRB5KDC_ERR_CLIENT_NAME_MISMATCH;

cleanup:
    free(cert);
    return ret;
}

/* Return an error if freshness tokens are required and one was not received.
 * Log an appropriate message indicating whether a valid token was received. */
static krb5_error_code
check_log_freshness(krb5_context context, pkinit_kdc_context plgctx,
                    krb5_kdc_req *request, krb5_boolean valid_freshness_token)
{
    krb5_error_code ret;
    char *name = NULL;

    ret = krb5_unparse_name(context, request->client, &name);
    if (ret)
        return ret;
    if (plgctx->opts->require_freshness && !valid_freshness_token) {
        com_err("", 0, _("PKINIT: no freshness token, rejecting auth from %s"),
                name);
        ret = KRB5KDC_ERR_PREAUTH_FAILED;
    } else if (valid_freshness_token) {
        com_err("", 0, _("PKINIT: freshness token received from %s"), name);
    } else {
        com_err("", 0, _("PKINIT: no freshness token received from %s"), name);
    }
    krb5_free_unparsed_name(context, name);
    return ret;
}

static void
pkinit_server_verify_padata(krb5_context context,
                            krb5_data *req_pkt,
                            krb5_kdc_req * request,
                            krb5_enc_tkt_part * enc_tkt_reply,
                            krb5_pa_data * data,
                            krb5_kdcpreauth_callbacks cb,
                            krb5_kdcpreauth_rock rock,
                            krb5_kdcpreauth_moddata moddata,
                            krb5_kdcpreauth_verify_respond_fn respond,
                            void *arg)
{
    krb5_error_code retval = 0;
    krb5_data authp_data = {0, 0, NULL}, krb5_authz = {0, 0, NULL};
    krb5_pa_pk_as_req *reqp = NULL;
    krb5_auth_pack *auth_pack = NULL;
    pkinit_kdc_context plgctx = NULL;
    pkinit_kdc_req_context reqctx = NULL;
    krb5_checksum cksum = {0, 0, 0, NULL};
    krb5_data *der_req = NULL;
    krb5_data k5data, *ftoken;
    int is_signed = 1;
    krb5_pa_data **e_data = NULL;
    krb5_kdcpreauth_modreq modreq = NULL;
    krb5_boolean valid_freshness_token = FALSE, hwauth = FALSE;
    char **sp;

    pkiDebug("pkinit_verify_padata: entered!\n");
    if (data == NULL || data->length <= 0 || data->contents == NULL) {
        (*respond)(arg, EINVAL, NULL, NULL, NULL);
        return;
    }


    if (moddata == NULL) {
        (*respond)(arg, EINVAL, NULL, NULL, NULL);
        return;
    }

    plgctx = pkinit_find_realm_context(context, moddata, request->server);
    if (plgctx == NULL) {
        (*respond)(arg, EINVAL, NULL, NULL, NULL);
        return;
    }

#ifdef DEBUG_ASN1
    print_buffer_bin(data->contents, data->length, "/tmp/kdc_as_req");
#endif
    /* create a per-request context */
    retval = pkinit_init_kdc_req_context(context, &reqctx);
    if (retval)
        goto cleanup;
    reqctx->pa_type = data->pa_type;

    PADATA_TO_KRB5DATA(data, &k5data);

    if (data->pa_type != KRB5_PADATA_PK_AS_REQ) {
        pkiDebug("unrecognized pa_type = %d\n", data->pa_type);
        retval = EINVAL;
        goto cleanup;
    }

    TRACE_PKINIT_SERVER_PADATA_VERIFY(context);
    retval = k5int_decode_krb5_pa_pk_as_req(&k5data, &reqp);
    if (retval) {
        pkiDebug("decode_krb5_pa_pk_as_req failed\n");
        goto cleanup;
    }
#ifdef DEBUG_ASN1
    print_buffer_bin(reqp->signedAuthPack.data, reqp->signedAuthPack.length,
                     "/tmp/kdc_signed_data");
#endif
    retval = cms_signeddata_verify(context, plgctx->cryptoctx,
                                   reqctx->cryptoctx, plgctx->idctx,
                                   CMS_SIGN_CLIENT,
                                   plgctx->opts->require_crl_checking,
                                   (unsigned char *)reqp->signedAuthPack.data,
                                   reqp->signedAuthPack.length,
                                   (unsigned char **)&authp_data.data,
                                   &authp_data.length,
                                   (unsigned char **)&krb5_authz.data,
                                   &krb5_authz.length, &is_signed);
    if (retval) {
        TRACE_PKINIT_SERVER_PADATA_VERIFY_FAIL(context);
        goto cleanup;
    }
    if (is_signed) {
        retval = authorize_cert(context, moddata->certauth_modules, plgctx,
                                reqctx, cb, rock, request->client, &hwauth);
        if (retval)
            goto cleanup;

    } else { /* !is_signed */
        if (!krb5_principal_compare(context, request->client,
                                    krb5_anonymous_principal())) {
            retval = KRB5KDC_ERR_PREAUTH_FAILED;
            krb5_set_error_message(context, retval,
                                   _("Pkinit request not signed, but client "
                                     "not anonymous."));
            goto cleanup;
        }
    }
#ifdef DEBUG_ASN1
    print_buffer_bin(authp_data.data, authp_data.length, "/tmp/kdc_auth_pack");
#endif

    OCTETDATA_TO_KRB5DATA(&authp_data, &k5data);
    retval = k5int_decode_krb5_auth_pack(&k5data, &auth_pack);
    if (retval) {
        pkiDebug("failed to decode krb5_auth_pack\n");
        goto cleanup;
    }

    retval = krb5_check_clockskew(context, auth_pack->pkAuthenticator.ctime);
    if (retval)
        goto cleanup;

    /* check dh parameters */
    if (auth_pack->clientPublicValue.length > 0) {
        retval = server_check_dh(context, plgctx->cryptoctx,
                                 reqctx->cryptoctx, plgctx->idctx,
                                 &auth_pack->clientPublicValue,
                                 plgctx->opts->dh_min_bits);
        if (retval) {
            pkiDebug("bad dh parameters\n");
            goto cleanup;
        }
    } else if (!is_signed) {
        /*Anonymous pkinit requires DH*/
        retval = KRB5KDC_ERR_PREAUTH_FAILED;
        krb5_set_error_message(context, retval,
                               _("Anonymous pkinit without DH public "
                                 "value not supported."));
        goto cleanup;
    }
    der_req = cb->request_body(context, rock);
    retval = krb5_c_make_checksum(context, CKSUMTYPE_SHA1, NULL, 0, der_req,
                                  &cksum);
    if (retval) {
        pkiDebug("unable to calculate AS REQ checksum\n");
        goto cleanup;
    }
    if (cksum.length != auth_pack->pkAuthenticator.paChecksum.length ||
        k5_bcmp(cksum.contents, auth_pack->pkAuthenticator.paChecksum.contents,
                cksum.length) != 0) {
        pkiDebug("failed to match the checksum\n");
#ifdef DEBUG_CKSUM
        pkiDebug("calculating checksum on buf size (%d)\n", req_pkt->length);
        print_buffer(req_pkt->data, req_pkt->length);
        pkiDebug("received checksum type=%d size=%d ",
                 auth_pack->pkAuthenticator.paChecksum.checksum_type,
                 auth_pack->pkAuthenticator.paChecksum.length);
        print_buffer(auth_pack->pkAuthenticator.paChecksum.contents,
                     auth_pack->pkAuthenticator.paChecksum.length);
        pkiDebug("expected checksum type=%d size=%d ",
                 cksum.checksum_type, cksum.length);
        print_buffer(cksum.contents, cksum.length);
#endif

        retval = KRB5KDC_ERR_PA_CHECKSUM_MUST_BE_INCLUDED;
        goto cleanup;
    }

    ftoken = auth_pack->pkAuthenticator.freshnessToken;
    if (ftoken != NULL) {
        retval = cb->check_freshness_token(context, rock, ftoken);
        if (retval)
            goto cleanup;
        valid_freshness_token = TRUE;
    }

    /* check if kdcPkId present and match KDC's subjectIdentifier */
    if (reqp->kdcPkId.data != NULL) {
        int valid_kdcPkId = 0;
        retval = pkinit_check_kdc_pkid(context, plgctx->cryptoctx,
                                       reqctx->cryptoctx, plgctx->idctx,
                                       (unsigned char *)reqp->kdcPkId.data,
                                       reqp->kdcPkId.length, &valid_kdcPkId);
        if (retval)
            goto cleanup;
        if (!valid_kdcPkId) {
            pkiDebug("kdcPkId in AS_REQ does not match KDC's cert; "
                     "RFC says to ignore and proceed\n");
        }
    }
    /* remember the decoded auth_pack for verify_padata routine */
    reqctx->rcv_auth_pack = auth_pack;
    auth_pack = NULL;

    if (is_signed) {
        retval = check_log_freshness(context, plgctx, request,
                                     valid_freshness_token);
        if (retval)
            goto cleanup;
    }

    if (is_signed && plgctx->auth_indicators != NULL) {
        /* Assert configured authentication indicators. */
        for (sp = plgctx->auth_indicators; *sp != NULL; sp++) {
            retval = cb->add_auth_indicator(context, rock, *sp);
            if (retval)
                goto cleanup;
        }
    }

    /* remember to set the PREAUTH flag in the reply */
    enc_tkt_reply->flags |= TKT_FLG_PRE_AUTH;
    if (hwauth)
        enc_tkt_reply->flags |= TKT_FLG_HW_AUTH;
    modreq = (krb5_kdcpreauth_modreq)reqctx;
    reqctx = NULL;

cleanup:
    if (retval && data->pa_type == KRB5_PADATA_PK_AS_REQ) {
        pkiDebug("pkinit_verify_padata failed: creating e-data\n");
        if (pkinit_create_edata(context, plgctx->cryptoctx, reqctx->cryptoctx,
                                plgctx->idctx, plgctx->opts, retval, &e_data))
            pkiDebug("pkinit_create_edata failed\n");
    }

    free_krb5_pa_pk_as_req(&reqp);
    free(cksum.contents);
    free(authp_data.data);
    free(krb5_authz.data);
    if (reqctx != NULL)
        pkinit_fini_kdc_req_context(context, reqctx);
    free_krb5_auth_pack(&auth_pack);

    (*respond)(arg, retval, modreq, e_data, NULL);
}
static krb5_error_code
return_pkinit_kx(krb5_context context, krb5_kdc_req *request,
                 krb5_kdc_rep *reply, krb5_keyblock *encrypting_key,
                 krb5_pa_data **out_padata)
{
    krb5_error_code ret = 0;
    krb5_keyblock *session = reply->ticket->enc_part2->session;
    krb5_keyblock *new_session = NULL;
    krb5_pa_data *pa = NULL;
    krb5_enc_data enc;
    krb5_data *scratch = NULL;

    *out_padata = NULL;
    enc.ciphertext.data = NULL;
    if (!krb5_principal_compare(context, request->client,
                                krb5_anonymous_principal()))
        return 0;
    /*
     * The KDC contribution key needs to be a fresh key of an enctype supported
     * by the client and server. The existing session key meets these
     * requirements so we use it.
     */
    ret = krb5_c_fx_cf2_simple(context, session, "PKINIT",
                               encrypting_key, "KEYEXCHANGE",
                               &new_session);
    if (ret)
        goto cleanup;
    ret = encode_krb5_encryption_key( session, &scratch);
    if (ret)
        goto cleanup;
    ret = krb5_encrypt_helper(context, encrypting_key,
                              KRB5_KEYUSAGE_PA_PKINIT_KX, scratch, &enc);
    if (ret)
        goto cleanup;
    memset(scratch->data, 0, scratch->length);
    krb5_free_data(context, scratch);
    scratch = NULL;
    ret = encode_krb5_enc_data(&enc, &scratch);
    if (ret)
        goto cleanup;
    pa = malloc(sizeof(krb5_pa_data));
    if (pa == NULL) {
        ret = ENOMEM;
        goto cleanup;
    }
    pa->pa_type = KRB5_PADATA_PKINIT_KX;
    pa->length = scratch->length;
    pa->contents = (krb5_octet *) scratch->data;
    *out_padata = pa;
    scratch->data = NULL;
    memset(session->contents, 0, session->length);
    krb5_free_keyblock_contents(context, session);
    *session = *new_session;
    new_session->contents = NULL;
cleanup:
    krb5_free_data_contents(context, &enc.ciphertext);
    krb5_free_keyblock(context, new_session);
    krb5_free_data(context, scratch);
    return ret;
}

static krb5_error_code
pkinit_pick_kdf_alg(krb5_context context, krb5_data **kdf_list,
                    krb5_data **alg_oid)
{
    krb5_error_code retval = 0;
    krb5_data *req_oid = NULL;
    const krb5_data *supp_oid = NULL;
    krb5_data *tmp_oid = NULL;
    int i, j = 0;

    /* if we don't find a match, return NULL value */
    *alg_oid = NULL;

    /* for each of the OIDs that the server supports... */
    for (i = 0; NULL != (supp_oid = supported_kdf_alg_ids[i]); i++) {
        /* if the requested OID is in the client's list, use it. */
        for (j = 0; NULL != (req_oid = kdf_list[j]); j++) {
            if ((req_oid->length == supp_oid->length) &&
                (0 == memcmp(req_oid->data, supp_oid->data, req_oid->length))) {
                tmp_oid = k5alloc(sizeof(krb5_data), &retval);
                if (retval)
                    goto cleanup;
                tmp_oid->data = k5memdup(supp_oid->data, supp_oid->length,
                                         &retval);
                if (retval)
                    goto cleanup;
                tmp_oid->length = supp_oid->length;
                *alg_oid = tmp_oid;
                /* don't free the OID in clean-up if we are returning it */
                tmp_oid = NULL;
                goto cleanup;
            }
        }
    }
cleanup:
    if (tmp_oid)
        krb5_free_data(context, tmp_oid);
    return retval;
}

static krb5_error_code
pkinit_server_return_padata(krb5_context context,
                            krb5_pa_data * padata,
                            krb5_data *req_pkt,
                            krb5_kdc_req * request,
                            krb5_kdc_rep * reply,
                            krb5_keyblock * encrypting_key,
                            krb5_pa_data ** send_pa,
                            krb5_kdcpreauth_callbacks cb,
                            krb5_kdcpreauth_rock rock,
                            krb5_kdcpreauth_moddata moddata,
                            krb5_kdcpreauth_modreq modreq)
{
    krb5_error_code retval = 0;
    krb5_data scratch = {0, 0, NULL};
    krb5_pa_pk_as_req *reqp = NULL;
    int i = 0;

    unsigned char *dh_pubkey = NULL, *server_key = NULL;
    unsigned int server_key_len = 0, dh_pubkey_len = 0;
    krb5_keyblock reply_key = { 0 };

    krb5_kdc_dh_key_info dhkey_info;
    krb5_data *encoded_dhkey_info = NULL;
    krb5_pa_pk_as_rep *rep = NULL;
    krb5_data *out_data = NULL;
    krb5_data secret;

    krb5_enctype enctype = -1;

    krb5_reply_key_pack *key_pack = NULL;
    krb5_data *encoded_key_pack = NULL;

    pkinit_kdc_context plgctx;
    pkinit_kdc_req_context reqctx;

    *send_pa = NULL;
    if (padata->pa_type == KRB5_PADATA_PKINIT_KX) {
        return return_pkinit_kx(context, request, reply,
                                encrypting_key, send_pa);
    }
    if (padata->length <= 0 || padata->contents == NULL)
        return 0;

    if (modreq == NULL) {
        pkiDebug("missing request context \n");
        return EINVAL;
    }

    plgctx = pkinit_find_realm_context(context, moddata, request->server);
    if (plgctx == NULL) {
        pkiDebug("Unable to locate correct realm context\n");
        return ENOENT;
    }

    TRACE_PKINIT_SERVER_RETURN_PADATA(context);
    reqctx = (pkinit_kdc_req_context)modreq;

    for(i = 0; i < request->nktypes; i++) {
        enctype = request->ktype[i];
        if (!krb5_c_valid_enctype(enctype))
            continue;
        else {
            pkiDebug("KDC picked etype = %d\n", enctype);
            break;
        }
    }

    if (i == request->nktypes) {
        retval = KRB5KDC_ERR_ETYPE_NOSUPP;
        goto cleanup;
    }

    init_krb5_pa_pk_as_rep(&rep);
    if (rep == NULL) {
        retval = ENOMEM;
        goto cleanup;
    }
    /* let's assume it's RSA. we'll reset it to DH if needed */
    rep->choice = choice_pa_pk_as_rep_encKeyPack;

    if (reqctx->rcv_auth_pack != NULL &&
        reqctx->rcv_auth_pack->clientPublicValue.length > 0) {
        rep->choice = choice_pa_pk_as_rep_dhInfo;

        pkiDebug("received DH key delivery AS REQ\n");
        retval = server_process_dh(context, plgctx->cryptoctx,
                                   reqctx->cryptoctx, plgctx->idctx,
                                   &dh_pubkey, &dh_pubkey_len,
                                   &server_key, &server_key_len);
        if (retval) {
            pkiDebug("failed to process/create dh parameters\n");
            goto cleanup;
        }

        /*
         * This is DH, so don't generate the key until after we
         * encode the reply, because the encoded reply is needed
         * to generate the key in some cases.
         */

        dhkey_info.subjectPublicKey.length = dh_pubkey_len;
        dhkey_info.subjectPublicKey.data = (char *)dh_pubkey;
        dhkey_info.nonce = request->nonce;
        dhkey_info.dhKeyExpiration = 0;

        retval = k5int_encode_krb5_kdc_dh_key_info(&dhkey_info,
                                                   &encoded_dhkey_info);
        if (retval) {
            pkiDebug("encode_krb5_kdc_dh_key_info failed\n");
            goto cleanup;
        }
#ifdef DEBUG_ASN1
        print_buffer_bin((unsigned char *)encoded_dhkey_info->data,
                         encoded_dhkey_info->length,
                         "/tmp/kdc_dh_key_info");
#endif

        retval = cms_signeddata_create(context, plgctx->cryptoctx,
                                       reqctx->cryptoctx, plgctx->idctx,
                                       CMS_SIGN_SERVER,
                                       (unsigned char *)
                                       encoded_dhkey_info->data,
                                       encoded_dhkey_info->length,
                                       (unsigned char **)
                                       &rep->u.dh_Info.dhSignedData.data,
                                       &rep->u.dh_Info.dhSignedData.length);
        if (retval) {
            pkiDebug("failed to create pkcs7 signed data\n");
            goto cleanup;
        }

    } else {
        pkiDebug("received RSA key delivery AS REQ\n");

        init_krb5_reply_key_pack(&key_pack);
        if (key_pack == NULL) {
            retval = ENOMEM;
            goto cleanup;
        }

        retval = krb5_c_make_random_key(context, enctype, &key_pack->replyKey);
        if (retval) {
            pkiDebug("unable to make a session key\n");
            goto cleanup;
        }

        retval = krb5_c_make_checksum(context, 0, &key_pack->replyKey,
                                      KRB5_KEYUSAGE_TGS_REQ_AUTH_CKSUM,
                                      req_pkt, &key_pack->asChecksum);
        if (retval) {
            pkiDebug("unable to calculate AS REQ checksum\n");
            goto cleanup;
        }
#ifdef DEBUG_CKSUM
        pkiDebug("calculating checksum on buf size = %d\n", req_pkt->length);
        print_buffer(req_pkt->data, req_pkt->length);
        pkiDebug("checksum size = %d\n", key_pack->asChecksum.length);
        print_buffer(key_pack->asChecksum.contents,
                     key_pack->asChecksum.length);
        pkiDebug("encrypting key (%d)\n", key_pack->replyKey.length);
        print_buffer(key_pack->replyKey.contents, key_pack->replyKey.length);
#endif

        retval = k5int_encode_krb5_reply_key_pack(key_pack,
                                                  &encoded_key_pack);
        if (retval) {
            pkiDebug("failed to encode reply_key_pack\n");
            goto cleanup;
        }

        rep->choice = choice_pa_pk_as_rep_encKeyPack;
        retval = cms_envelopeddata_create(context, plgctx->cryptoctx,
                                          reqctx->cryptoctx, plgctx->idctx,
                                          padata->pa_type,
                                          (unsigned char *)
                                          encoded_key_pack->data,
                                          encoded_key_pack->length,
                                          (unsigned char **)
                                          &rep->u.encKeyPack.data,
                                          &rep->u.encKeyPack.length);
        if (retval) {
            pkiDebug("failed to create pkcs7 enveloped data: %s\n",
                     error_message(retval));
            goto cleanup;
        }
#ifdef DEBUG_ASN1
        print_buffer_bin((unsigned char *)encoded_key_pack->data,
                         encoded_key_pack->length,
                         "/tmp/kdc_key_pack");
        print_buffer_bin(rep->u.encKeyPack.data, rep->u.encKeyPack.length,
                         "/tmp/kdc_enc_key_pack");
#endif

        retval = cb->replace_reply_key(context, rock, &key_pack->replyKey,
                                       FALSE);
        if (retval)
            goto cleanup;
    }

    if (rep->choice == choice_pa_pk_as_rep_dhInfo &&
        ((reqctx->rcv_auth_pack != NULL &&
          reqctx->rcv_auth_pack->supportedKDFs != NULL))) {

        /* If using the alg-agility KDF, put the algorithm in the reply
         * before encoding it.
         */
        if (reqctx->rcv_auth_pack != NULL &&
            reqctx->rcv_auth_pack->supportedKDFs != NULL) {
            retval = pkinit_pick_kdf_alg(context, reqctx->rcv_auth_pack->supportedKDFs,
                                         &(rep->u.dh_Info.kdfID));
            if (retval) {
                pkiDebug("pkinit_pick_kdf_alg failed: %s\n",
                         error_message(retval));
                goto cleanup;
            }
        }
    }

    retval = k5int_encode_krb5_pa_pk_as_rep(rep, &out_data);
    if (retval) {
        pkiDebug("failed to encode AS_REP\n");
        goto cleanup;
    }
#ifdef DEBUG_ASN1
    if (out_data != NULL)
        print_buffer_bin((unsigned char *)out_data->data, out_data->length,
                         "/tmp/kdc_as_rep");
#endif

    /* If this is DH, we haven't computed the key yet, so do it now. */
    if (rep->choice == choice_pa_pk_as_rep_dhInfo) {

        /* If mutually supported KDFs were found, use the algorithm agility
         * KDF. */
        if (rep->u.dh_Info.kdfID) {
            secret.data = (char *)server_key;
            secret.length = server_key_len;

            retval = pkinit_alg_agility_kdf(context, &secret,
                                            rep->u.dh_Info.kdfID,
                                            request->client, request->server,
                                            enctype, req_pkt, out_data,
                                            &reply_key);
            if (retval) {
                pkiDebug("pkinit_alg_agility_kdf failed: %s\n",
                         error_message(retval));
                goto cleanup;
            }

            /* Otherwise, use the older octetstring2key() function */
        } else {
            retval = pkinit_octetstring2key(context, enctype, server_key,
                                            server_key_len, &reply_key);
            if (retval) {
                pkiDebug("pkinit_octetstring2key failed: %s\n",
                         error_message(retval));
                goto cleanup;
            }
        }
        retval = cb->replace_reply_key(context, rock, &reply_key, FALSE);
        if (retval)
            goto cleanup;
    }

    *send_pa = malloc(sizeof(krb5_pa_data));
    if (*send_pa == NULL) {
        retval = ENOMEM;
        free(out_data->data);
        free(out_data);
        out_data = NULL;
        goto cleanup;
    }
    (*send_pa)->magic = KV5M_PA_DATA;
    (*send_pa)->pa_type = KRB5_PADATA_PK_AS_REP;
    (*send_pa)->length = out_data->length;
    (*send_pa)->contents = (krb5_octet *) out_data->data;

cleanup:
    free(scratch.data);
    free(out_data);
    if (encoded_dhkey_info != NULL)
        krb5_free_data(context, encoded_dhkey_info);
    if (encoded_key_pack != NULL)
        krb5_free_data(context, encoded_key_pack);
    free(dh_pubkey);
    free(server_key);
    free_krb5_pa_pk_as_req(&reqp);
    free_krb5_pa_pk_as_rep(&rep);
    free_krb5_reply_key_pack(&key_pack);
    krb5_free_keyblock_contents(context, &reply_key);

    if (retval)
        pkiDebug("pkinit_verify_padata failure");

    return retval;
}

static int
pkinit_server_get_flags(krb5_context kcontext, krb5_preauthtype patype)
{
    if (patype == KRB5_PADATA_PKINIT_KX)
        return PA_INFO;
    /* PKINIT does not normally set the hw-authent ticket flag, but a
     * certauth module can cause it to do so. */
    return PA_SUFFICIENT | PA_REPLACES_KEY | PA_TYPED_E_DATA | PA_HARDWARE;
}

static krb5_preauthtype supported_server_pa_types[] = {
    KRB5_PADATA_PK_AS_REQ,
    KRB5_PADATA_PKINIT_KX,
    0
};

static void
pkinit_fini_kdc_profile(krb5_context context, pkinit_kdc_context plgctx)
{
    /*
     * There is nothing currently allocated by pkinit_init_kdc_profile()
     * which needs to be freed here.
     */
}

static krb5_error_code
pkinit_init_kdc_profile(krb5_context context, pkinit_kdc_context plgctx)
{
    krb5_error_code retval;
    char *eku_string = NULL, *ocsp_check = NULL;

    pkiDebug("%s: entered for realm %s\n", __FUNCTION__, plgctx->realmname);
    retval = pkinit_kdcdefault_string(context, plgctx->realmname,
                                      KRB5_CONF_PKINIT_IDENTITY,
                                      &plgctx->idopts->identity);
    if (retval != 0 || NULL == plgctx->idopts->identity) {
        retval = EINVAL;
        krb5_set_error_message(context, retval,
                               _("No pkinit_identity supplied for realm %s"),
                               plgctx->realmname);
        goto errout;
    }

    retval = pkinit_kdcdefault_strings(context, plgctx->realmname,
                                       KRB5_CONF_PKINIT_ANCHORS,
                                       &plgctx->idopts->anchors);
    if (retval != 0 || NULL == plgctx->idopts->anchors) {
        retval = EINVAL;
        krb5_set_error_message(context, retval,
                               _("No pkinit_anchors supplied for realm %s"),
                               plgctx->realmname);
        goto errout;
    }

    pkinit_kdcdefault_strings(context, plgctx->realmname,
                              KRB5_CONF_PKINIT_POOL,
                              &plgctx->idopts->intermediates);

    pkinit_kdcdefault_strings(context, plgctx->realmname,
                              KRB5_CONF_PKINIT_REVOKE,
                              &plgctx->idopts->crls);

    pkinit_kdcdefault_string(context, plgctx->realmname,
                             KRB5_CONF_PKINIT_KDC_OCSP,
                             &ocsp_check);
    if (ocsp_check != NULL) {
        free(ocsp_check);
        retval = ENOTSUP;
        krb5_set_error_message(context, retval,
                               _("OCSP is not supported: (realm: %s)"),
                               plgctx->realmname);
        goto errout;
    }

    pkinit_kdcdefault_integer(context, plgctx->realmname,
                              KRB5_CONF_PKINIT_DH_MIN_BITS,
                              PKINIT_DEFAULT_DH_MIN_BITS,
                              &plgctx->opts->dh_min_bits);
    if (plgctx->opts->dh_min_bits < PKINIT_DH_MIN_CONFIG_BITS) {
        pkiDebug("%s: invalid value (%d < %d) for pkinit_dh_min_bits, "
                 "using default value (%d) instead\n", __FUNCTION__,
                 plgctx->opts->dh_min_bits, PKINIT_DH_MIN_CONFIG_BITS,
                 PKINIT_DEFAULT_DH_MIN_BITS);
        plgctx->opts->dh_min_bits = PKINIT_DEFAULT_DH_MIN_BITS;
    }

    pkinit_kdcdefault_boolean(context, plgctx->realmname,
                              KRB5_CONF_PKINIT_ALLOW_UPN,
                              0, &plgctx->opts->allow_upn);

    pkinit_kdcdefault_boolean(context, plgctx->realmname,
                              KRB5_CONF_PKINIT_REQUIRE_CRL_CHECKING,
                              0, &plgctx->opts->require_crl_checking);

    pkinit_kdcdefault_boolean(context, plgctx->realmname,
                              KRB5_CONF_PKINIT_REQUIRE_FRESHNESS,
                              0, &plgctx->opts->require_freshness);

    pkinit_kdcdefault_string(context, plgctx->realmname,
                             KRB5_CONF_PKINIT_EKU_CHECKING,
                             &eku_string);
    if (eku_string != NULL) {
        if (strcasecmp(eku_string, "kpClientAuth") == 0) {
            plgctx->opts->require_eku = 1;
            plgctx->opts->accept_secondary_eku = 0;
        } else if (strcasecmp(eku_string, "scLogin") == 0) {
            plgctx->opts->require_eku = 1;
            plgctx->opts->accept_secondary_eku = 1;
        } else if (strcasecmp(eku_string, "none") == 0) {
            plgctx->opts->require_eku = 0;
            plgctx->opts->accept_secondary_eku = 0;
        } else {
            pkiDebug("%s: Invalid value for pkinit_eku_checking: '%s'\n",
                     __FUNCTION__, eku_string);
        }
        free(eku_string);
    }

    pkinit_kdcdefault_strings(context, plgctx->realmname,
                              KRB5_CONF_PKINIT_INDICATOR,
                              &plgctx->auth_indicators);

    return 0;
errout:
    pkinit_fini_kdc_profile(context, plgctx);
    return retval;
}

static pkinit_kdc_context
pkinit_find_realm_context(krb5_context context,
                          krb5_kdcpreauth_moddata moddata,
                          krb5_principal princ)
{
    int i;
    pkinit_kdc_context *realm_contexts;

    if (moddata == NULL)
        return NULL;

    realm_contexts = moddata->realm_contexts;
    if (realm_contexts == NULL)
        return NULL;

    for (i = 0; realm_contexts[i] != NULL; i++) {
        pkinit_kdc_context p = realm_contexts[i];

        if ((p->realmname_len == princ->realm.length) &&
            (strncmp(p->realmname, princ->realm.data, p->realmname_len) == 0)) {
            pkiDebug("%s: returning context at %p for realm '%s'\n",
                     __FUNCTION__, p, p->realmname);
            return p;
        }
    }
    pkiDebug("%s: unable to find realm context for realm '%.*s'\n",
             __FUNCTION__, princ->realm.length, princ->realm.data);
    return NULL;
}

static int
pkinit_server_plugin_init_realm(krb5_context context, const char *realmname,
                                pkinit_kdc_context *pplgctx)
{
    krb5_error_code retval = ENOMEM;
    pkinit_kdc_context plgctx = NULL;

    *pplgctx = NULL;

    plgctx = calloc(1, sizeof(*plgctx));
    if (plgctx == NULL)
        goto errout;

    pkiDebug("%s: initializing context at %p for realm '%s'\n",
             __FUNCTION__, plgctx, realmname);
    memset(plgctx, 0, sizeof(*plgctx));
    plgctx->magic = PKINIT_CTX_MAGIC;

    plgctx->realmname = strdup(realmname);
    if (plgctx->realmname == NULL)
        goto errout;
    plgctx->realmname_len = strlen(plgctx->realmname);

    retval = pkinit_init_plg_crypto(&plgctx->cryptoctx);
    if (retval)
        goto errout;

    retval = pkinit_init_plg_opts(&plgctx->opts);
    if (retval)
        goto errout;

    retval = pkinit_init_identity_crypto(&plgctx->idctx);
    if (retval)
        goto errout;

    retval = pkinit_init_identity_opts(&plgctx->idopts);
    if (retval)
        goto errout;

    retval = pkinit_init_kdc_profile(context, plgctx);
    if (retval)
        goto errout;

    retval = pkinit_identity_initialize(context, plgctx->cryptoctx, NULL,
                                        plgctx->idopts, plgctx->idctx,
                                        NULL, NULL, NULL);
    if (retval)
        goto errout;
    retval = pkinit_identity_prompt(context, plgctx->cryptoctx, NULL,
                                    plgctx->idopts, plgctx->idctx,
                                    NULL, NULL, 0, NULL);
    if (retval)
        goto errout;

    pkiDebug("%s: returning context at %p for realm '%s'\n",
             __FUNCTION__, plgctx, realmname);
    *pplgctx = plgctx;
    retval = 0;

errout:
    if (retval)
        pkinit_server_plugin_fini_realm(context, plgctx);

    return retval;
}

static krb5_error_code
pkinit_san_authorize(krb5_context context, krb5_certauth_moddata moddata,
                     const uint8_t *cert, size_t cert_len,
                     krb5_const_principal princ, const void *opts,
                     const struct _krb5_db_entry_new *db_entry,
                     char ***authinds_out)
{
    krb5_error_code ret;
    int valid_san;
    const struct certauth_req_opts *req_opts = opts;

    *authinds_out = NULL;

    ret = verify_client_san(context, req_opts->plgctx, req_opts->reqctx,
                            req_opts->cb, req_opts->rock, princ, &valid_san);
    if (ret == ENOENT)
        return KRB5_PLUGIN_NO_HANDLE;
    else if (ret)
        return ret;

    if (!valid_san) {
        TRACE_PKINIT_SERVER_SAN_REJECT(context);
        return KRB5KDC_ERR_CLIENT_NAME_MISMATCH;
    }

    return 0;
}

static krb5_error_code
pkinit_eku_authorize(krb5_context context, krb5_certauth_moddata moddata,
                     const uint8_t *cert, size_t cert_len,
                     krb5_const_principal princ, const void *opts,
                     const struct _krb5_db_entry_new *db_entry,
                     char ***authinds_out)
{
    krb5_error_code ret;
    int valid_eku;
    const struct certauth_req_opts *req_opts = opts;

    *authinds_out = NULL;

    /* Verify the client EKU. */
    ret = verify_client_eku(context, req_opts->plgctx, req_opts->reqctx,
                            &valid_eku);
    if (ret)
        return ret;

    if (!valid_eku) {
        TRACE_PKINIT_SERVER_EKU_REJECT(context);
        return KRB5KDC_ERR_INCONSISTENT_KEY_PURPOSE;
    }

    return KRB5_PLUGIN_NO_HANDLE;
}

static krb5_error_code
certauth_pkinit_san_initvt(krb5_context context, int maj_ver, int min_ver,
                           krb5_plugin_vtable vtable)
{
    krb5_certauth_vtable vt;

    if (maj_ver != 1)
        return KRB5_PLUGIN_VER_NOTSUPP;
    vt = (krb5_certauth_vtable)vtable;
    vt->name = "pkinit_san";
    vt->authorize = pkinit_san_authorize;
    return 0;
}

static krb5_error_code
certauth_pkinit_eku_initvt(krb5_context context, int maj_ver, int min_ver,
                           krb5_plugin_vtable vtable)
{
    krb5_certauth_vtable vt;

    if (maj_ver != 1)
        return KRB5_PLUGIN_VER_NOTSUPP;
    vt = (krb5_certauth_vtable)vtable;
    vt->name = "pkinit_eku";
    vt->authorize = pkinit_eku_authorize;
    return 0;
}

/*
 * Do certificate auth based on a match expression in the pkinit_cert_match
 * attribute string.  An expression should be in the same form as those used
 * for the pkinit_cert_match configuration option.
 */
static krb5_error_code
dbmatch_authorize(krb5_context context, krb5_certauth_moddata moddata,
                  const uint8_t *cert, size_t cert_len,
                  krb5_const_principal princ, const void *opts,
                  const struct _krb5_db_entry_new *db_entry,
                  char ***authinds_out)
{
    krb5_error_code ret;
    const struct certauth_req_opts *req_opts = opts;
    char *pattern;
    krb5_boolean matched;

    *authinds_out = NULL;

    /* Fetch the matching pattern.  Pass if it isn't specified. */
    ret = req_opts->cb->get_string(context, req_opts->rock,
                                   "pkinit_cert_match", &pattern);
    if (ret)
        return ret;
    if (pattern == NULL)
        return KRB5_PLUGIN_NO_HANDLE;

    /* Check the certificate against the match expression. */
    ret = pkinit_client_cert_match(context, req_opts->plgctx->cryptoctx,
                                   req_opts->reqctx->cryptoctx, pattern,
                                   &matched);
    req_opts->cb->free_string(context, req_opts->rock, pattern);
    if (ret)
        return ret;
    return matched ? 0 : KRB5KDC_ERR_CERTIFICATE_MISMATCH;
}

static krb5_error_code
certauth_dbmatch_initvt(krb5_context context, int maj_ver, int min_ver,
                        krb5_plugin_vtable vtable)
{
    krb5_certauth_vtable vt;

    if (maj_ver != 1)
        return KRB5_PLUGIN_VER_NOTSUPP;
    vt = (krb5_certauth_vtable)vtable;
    vt->name = "dbmatch";
    vt->authorize = dbmatch_authorize;
    return 0;
}

static krb5_error_code
load_certauth_plugins(krb5_context context, const char *const *realmnames,
                      certauth_handle **handle_out)
{
    krb5_error_code ret;
    krb5_plugin_initvt_fn *modules = NULL, *mod;
    certauth_handle *list = NULL, h;
    size_t count;

    /* Register the builtin modules. */
    ret = k5_plugin_register(context, PLUGIN_INTERFACE_CERTAUTH,
                             "pkinit_san", certauth_pkinit_san_initvt);
    if (ret)
        goto cleanup;

    ret = k5_plugin_register(context, PLUGIN_INTERFACE_CERTAUTH,
                             "pkinit_eku", certauth_pkinit_eku_initvt);
    if (ret)
        goto cleanup;

    ret = k5_plugin_register(context, PLUGIN_INTERFACE_CERTAUTH, "dbmatch",
                             certauth_dbmatch_initvt);
    if (ret)
        goto cleanup;

    ret = k5_plugin_load_all(context, PLUGIN_INTERFACE_CERTAUTH, &modules);
    if (ret)
        goto cleanup;

    /* Allocate handle list. */
    for (count = 0; modules[count]; count++);
    list = k5calloc(count + 1, sizeof(*list), &ret);
    if (list == NULL)
        goto cleanup;

    /* Initialize each module, ignoring ones that fail. */
    count = 0;
    for (mod = modules; *mod != NULL; mod++) {
        h = k5calloc(1, sizeof(*h), &ret);
        if (h == NULL)
            goto cleanup;

        ret = (*mod)(context, 1, 2, (krb5_plugin_vtable)&h->vt);
        if (ret) {
            TRACE_CERTAUTH_VTINIT_FAIL(context, ret);
            free(h);
            continue;
        }
        h->moddata = NULL;
        if (h->vt.init_ex != NULL)
            ret = h->vt.init_ex(context, realmnames, &h->moddata);
        else if (h->vt.init != NULL)
            ret = h->vt.init(context, &h->moddata);
        if (ret) {
            TRACE_CERTAUTH_INIT_FAIL(context, h->vt.name, ret);
            free(h);
            continue;
        }
        list[count++] = h;
        list[count] = NULL;
    }
    list[count] = NULL;

    ret = 0;
    *handle_out = list;
    list = NULL;

cleanup:
    k5_plugin_free_modules(context, modules);
    free_certauth_handles(context, list);
    return ret;
}

static int
pkinit_server_plugin_init(krb5_context context,
                          krb5_kdcpreauth_moddata *moddata_out,
                          const char **realmnames)
{
    krb5_error_code retval = ENOMEM;
    pkinit_kdc_context plgctx, *realm_contexts = NULL;
    certauth_handle *certauth_modules = NULL;
    krb5_kdcpreauth_moddata moddata;
    size_t  i, j;
    size_t numrealms;

    retval = pkinit_accessor_init();
    if (retval)
        return retval;

    /* Determine how many realms we may need to support */
    for (i = 0; realmnames[i] != NULL; i++) {};
    numrealms = i;

    realm_contexts = calloc(numrealms+1, sizeof(pkinit_kdc_context));
    if (realm_contexts == NULL)
        return ENOMEM;

    for (i = 0, j = 0; i < numrealms; i++) {
        TRACE_PKINIT_SERVER_INIT_REALM(context, realmnames[i]);
        krb5_clear_error_message(context);
        retval = pkinit_server_plugin_init_realm(context, realmnames[i],
                                                 &plgctx);
        if (retval)
            TRACE_PKINIT_SERVER_INIT_FAIL(context, realmnames[i], retval);
        else
            realm_contexts[j++] = plgctx;
    }

    if (j == 0) {
        if (numrealms == 1) {
            k5_prependmsg(context, retval, "PKINIT initialization failed");
        } else {
            retval = EINVAL;
            k5_setmsg(context, retval,
                      _("No realms configured correctly for pkinit support"));
        }
        goto errout;
    }

    retval = load_certauth_plugins(context, realmnames, &certauth_modules);
    if (retval)
        goto errout;

    moddata = k5calloc(1, sizeof(*moddata), &retval);
    if (moddata == NULL)
        goto errout;
    moddata->realm_contexts = realm_contexts;
    moddata->certauth_modules = certauth_modules;
    *moddata_out = moddata;
    pkiDebug("%s: returning context at %p\n", __FUNCTION__, moddata);
    return 0;

errout:
    free_realm_contexts(context, realm_contexts);
    free_certauth_handles(context, certauth_modules);
    return retval;
}

static void
pkinit_server_plugin_fini_realm(krb5_context context, pkinit_kdc_context plgctx)
{
    char **sp;

    if (plgctx == NULL)
        return;

    pkinit_fini_kdc_profile(context, plgctx);
    pkinit_fini_identity_opts(plgctx->idopts);
    pkinit_fini_identity_crypto(plgctx->idctx);
    pkinit_fini_plg_crypto(plgctx->cryptoctx);
    pkinit_fini_plg_opts(plgctx->opts);
    for (sp = plgctx->auth_indicators; sp != NULL && *sp != NULL; sp++)
        free(*sp);
    free(plgctx->auth_indicators);
    free(plgctx->realmname);
    free(plgctx);
}

static void
pkinit_server_plugin_fini(krb5_context context,
                          krb5_kdcpreauth_moddata moddata)
{
    if (moddata == NULL)
        return;
    free_realm_contexts(context, moddata->realm_contexts);
    free_certauth_handles(context, moddata->certauth_modules);
    free(moddata);
}

static krb5_error_code
pkinit_init_kdc_req_context(krb5_context context, pkinit_kdc_req_context *ctx)
{
    krb5_error_code retval = ENOMEM;
    pkinit_kdc_req_context reqctx = NULL;

    reqctx = malloc(sizeof(*reqctx));
    if (reqctx == NULL)
        return retval;
    memset(reqctx, 0, sizeof(*reqctx));
    reqctx->magic = PKINIT_CTX_MAGIC;

    retval = pkinit_init_req_crypto(&reqctx->cryptoctx);
    if (retval)
        goto cleanup;
    reqctx->rcv_auth_pack = NULL;

    pkiDebug("%s: returning reqctx at %p\n", __FUNCTION__, reqctx);
    *ctx = reqctx;
    retval = 0;
cleanup:
    if (retval)
        pkinit_fini_kdc_req_context(context, reqctx);

    return retval;
}

static void
pkinit_fini_kdc_req_context(krb5_context context, void *ctx)
{
    pkinit_kdc_req_context reqctx = (pkinit_kdc_req_context)ctx;

    if (reqctx == NULL || reqctx->magic != PKINIT_CTX_MAGIC) {
        pkiDebug("pkinit_fini_kdc_req_context: got bad reqctx (%p)!\n", reqctx);
        return;
    }
    pkiDebug("%s: freeing reqctx at %p\n", __FUNCTION__, reqctx);

    pkinit_fini_req_crypto(reqctx->cryptoctx);
    if (reqctx->rcv_auth_pack != NULL)
        free_krb5_auth_pack(&reqctx->rcv_auth_pack);

    free(reqctx);
}

static void
pkinit_free_modreq(krb5_context context, krb5_kdcpreauth_moddata moddata,
                   krb5_kdcpreauth_modreq modreq)
{
    pkinit_fini_kdc_req_context(context, modreq);
}

krb5_error_code
kdcpreauth_pkinit_initvt(krb5_context context, int maj_ver, int min_ver,
                         krb5_plugin_vtable vtable);

krb5_error_code
kdcpreauth_pkinit_initvt(krb5_context context, int maj_ver, int min_ver,
                         krb5_plugin_vtable vtable)
{
    krb5_kdcpreauth_vtable vt;

    if (maj_ver != 1)
        return KRB5_PLUGIN_VER_NOTSUPP;
    vt = (krb5_kdcpreauth_vtable)vtable;
    vt->name = "pkinit";
    vt->pa_type_list = supported_server_pa_types;
    vt->init = pkinit_server_plugin_init;
    vt->fini = pkinit_server_plugin_fini;
    vt->flags = pkinit_server_get_flags;
    vt->edata = pkinit_server_get_edata;
    vt->verify = pkinit_server_verify_padata;
    vt->return_padata = pkinit_server_return_padata;
    vt->free_modreq = pkinit_free_modreq;
    return 0;
}
