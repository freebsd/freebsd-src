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

#include "k5-int.h"
#include "pkinit.h"
#include "k5-json.h"

#include <unistd.h>
#include <dlfcn.h>
#include <sys/stat.h>

/**
 * Return true if we should use ContentInfo rather than SignedData. This
 * happens if we are talking to what might be an old (pre-6112) MIT KDC and
 * we're using anonymous.
 */
static int
use_content_info(krb5_context context, pkinit_req_context req,
                 krb5_principal client)
{
    if (req->rfc6112_kdc)
        return 0;
    if (krb5_principal_compare_any_realm(context, client,
                                         krb5_anonymous_principal()))
        return 1;
    return 0;
}

static krb5_error_code
pkinit_as_req_create(krb5_context context, pkinit_context plgctx,
                     pkinit_req_context reqctx, krb5_timestamp ctsec,
                     krb5_int32 cusec, krb5_ui_4 nonce,
                     const krb5_checksum *cksum,
                     krb5_principal client, krb5_principal server,
                     krb5_data **as_req);

static krb5_error_code
pkinit_as_rep_parse(krb5_context context, pkinit_context plgctx,
                    pkinit_req_context reqctx, krb5_preauthtype pa_type,
                    krb5_kdc_req *request, const krb5_data *as_rep,
                    krb5_keyblock *key_block, krb5_enctype etype, krb5_data *);

static void pkinit_client_plugin_fini(krb5_context context,
                                      krb5_clpreauth_moddata moddata);

static krb5_error_code
pa_pkinit_gen_req(krb5_context context,
                  pkinit_context plgctx,
                  pkinit_req_context reqctx,
                  krb5_clpreauth_callbacks cb,
                  krb5_clpreauth_rock rock,
                  krb5_kdc_req * request,
                  krb5_preauthtype pa_type,
                  krb5_pa_data *** out_padata,
                  krb5_prompter_fct prompter,
                  void *prompter_data,
                  krb5_get_init_creds_opt *gic_opt)
{

    krb5_error_code retval = KRB5KDC_ERR_PREAUTH_FAILED;
    krb5_data *out_data = NULL;
    krb5_timestamp ctsec = 0;
    krb5_int32 cusec = 0;
    krb5_ui_4 nonce = 0;
    krb5_checksum cksum;
    krb5_data *der_req = NULL;
    krb5_pa_data **return_pa_data = NULL;

    memset(&cksum, 0, sizeof(cksum));
    reqctx->pa_type = pa_type;

    pkiDebug("kdc_options = 0x%x  till = %d\n",
             request->kdc_options, request->till);
    /* If we don't have a client, we're done */
    if (request->client == NULL) {
        pkiDebug("No request->client; aborting PKINIT\n");
        return KRB5KDC_ERR_PREAUTH_FAILED;
    }

    retval = pkinit_get_kdc_cert(context, plgctx->cryptoctx, reqctx->cryptoctx,
                                 reqctx->idctx, request->server);
    if (retval) {
        pkiDebug("pkinit_get_kdc_cert returned %d\n", retval);
        goto cleanup;
    }

    /* checksum of the encoded KDC-REQ-BODY */
    retval = k5int_encode_krb5_kdc_req_body(request, &der_req);
    if (retval) {
        pkiDebug("encode_krb5_kdc_req_body returned %d\n", (int) retval);
        goto cleanup;
    }

    retval = krb5_c_make_checksum(context, CKSUMTYPE_NIST_SHA, NULL, 0,
                                  der_req, &cksum);
    if (retval)
        goto cleanup;
    TRACE_PKINIT_CLIENT_REQ_CHECKSUM(context, &cksum);
#ifdef DEBUG_CKSUM
    pkiDebug("calculating checksum on buf size (%d)\n", der_req->length);
    print_buffer(der_req->data, der_req->length);
#endif

    retval = cb->get_preauth_time(context, rock, TRUE, &ctsec, &cusec);
    if (retval)
        goto cleanup;

    /* XXX PKINIT RFC says that nonce in PKAuthenticator doesn't have be the
     * same as in the AS_REQ. However, if we pick a different nonce, then we
     * need to remember that info when AS_REP is returned. I'm choosing to
     * reuse the AS_REQ nonce.
     */
    nonce = request->nonce;

    retval = pkinit_as_req_create(context, plgctx, reqctx, ctsec, cusec,
                                  nonce, &cksum, request->client, request->server, &out_data);
    if (retval) {
        pkiDebug("error %d on pkinit_as_req_create; aborting PKINIT\n",
                 (int) retval);
        goto cleanup;
    }

    /*
     * The most we'll return is two pa_data, normally just one.
     * We need to make room for the NULL terminator.
     */
    return_pa_data = k5calloc(3, sizeof(*return_pa_data), &retval);
    if (return_pa_data == NULL)
        goto cleanup;

    return_pa_data[0] = k5alloc(sizeof(*return_pa_data[0]), &retval);
    if (return_pa_data[0] == NULL)
        goto cleanup;

    return_pa_data[0]->magic = KV5M_PA_DATA;

    if (pa_type == KRB5_PADATA_PK_AS_REQ_OLD)
        return_pa_data[0]->pa_type = KRB5_PADATA_PK_AS_REP_OLD;
    else
        return_pa_data[0]->pa_type = pa_type;
    return_pa_data[0]->length = out_data->length;
    return_pa_data[0]->contents = (krb5_octet *) out_data->data;
    *out_data = empty_data();

    if (return_pa_data[0]->pa_type == KRB5_PADATA_PK_AS_REP_OLD) {
        return_pa_data[1] = k5alloc(sizeof(*return_pa_data[1]), &retval);
        if (return_pa_data[1] == NULL)
            goto cleanup;
        return_pa_data[1]->pa_type = KRB5_PADATA_AS_CHECKSUM;
    }

    *out_padata = return_pa_data;
    return_pa_data = NULL;

cleanup:
    krb5_free_data(context, der_req);
    krb5_free_checksum_contents(context, &cksum);
    krb5_free_data(context, out_data);
    krb5_free_pa_data(context, return_pa_data);
    return retval;
}

static krb5_error_code
pkinit_as_req_create(krb5_context context,
                     pkinit_context plgctx,
                     pkinit_req_context reqctx,
                     krb5_timestamp ctsec,
                     krb5_int32 cusec,
                     krb5_ui_4 nonce,
                     const krb5_checksum * cksum,
                     krb5_principal client,
                     krb5_principal server,
                     krb5_data ** as_req)
{
    krb5_error_code retval = ENOMEM;
    krb5_subject_pk_info info;
    krb5_data *coded_auth_pack = NULL;
    krb5_auth_pack auth_pack;
    krb5_pa_pk_as_req *req = NULL;
    krb5_auth_pack_draft9 auth_pack9;
    krb5_pa_pk_as_req_draft9 *req9 = NULL;
    krb5_algorithm_identifier **cmstypes = NULL;
    int protocol = reqctx->opts->dh_or_rsa;
    unsigned char *dh_params = NULL, *dh_pubkey = NULL;
    unsigned int dh_params_len, dh_pubkey_len;

    pkiDebug("pkinit_as_req_create pa_type = %d\n", reqctx->pa_type);

    /* Create the authpack */
    switch((int)reqctx->pa_type) {
    case KRB5_PADATA_PK_AS_REQ_OLD:
        protocol = RSA_PROTOCOL;
        memset(&auth_pack9, 0, sizeof(auth_pack9));
        auth_pack9.pkAuthenticator.ctime = ctsec;
        auth_pack9.pkAuthenticator.cusec = cusec;
        auth_pack9.pkAuthenticator.nonce = nonce;
        auth_pack9.pkAuthenticator.kdcName = server;
        break;
    case KRB5_PADATA_PK_AS_REQ:
        memset(&info, 0, sizeof(info));
        memset(&auth_pack, 0, sizeof(auth_pack));
        auth_pack.pkAuthenticator.ctime = ctsec;
        auth_pack.pkAuthenticator.cusec = cusec;
        auth_pack.pkAuthenticator.nonce = nonce;
        auth_pack.pkAuthenticator.paChecksum = *cksum;
        auth_pack.clientDHNonce.length = 0;
        auth_pack.clientPublicValue = &info;
        auth_pack.supportedKDFs = (krb5_data **)supported_kdf_alg_ids;

        /* add List of CMS algorithms */
        retval = create_krb5_supportedCMSTypes(context, plgctx->cryptoctx,
                                               reqctx->cryptoctx,
                                               reqctx->idctx, &cmstypes);
        auth_pack.supportedCMSTypes = cmstypes;
        if (retval)
            goto cleanup;
        break;
    default:
        pkiDebug("as_req: unrecognized pa_type = %d\n",
                 (int)reqctx->pa_type);
        retval = -1;
        goto cleanup;
    }

    switch(protocol) {
    case DH_PROTOCOL:
        TRACE_PKINIT_CLIENT_REQ_DH(context);
        pkiDebug("as_req: DH key transport algorithm\n");
        info.algorithm.algorithm = dh_oid;

        /* create client-side DH keys */
        retval = client_create_dh(context, plgctx->cryptoctx,
                                  reqctx->cryptoctx, reqctx->idctx,
                                  reqctx->opts->dh_size, &dh_params,
                                  &dh_params_len, &dh_pubkey, &dh_pubkey_len);
        if (retval != 0) {
            pkiDebug("failed to create dh parameters\n");
            goto cleanup;
        }
        info.algorithm.parameters = make_data(dh_params, dh_params_len);
        info.subjectPublicKey = make_data(dh_pubkey, dh_pubkey_len);
        break;
    case RSA_PROTOCOL:
        TRACE_PKINIT_CLIENT_REQ_RSA(context);
        pkiDebug("as_req: RSA key transport algorithm\n");
        switch((int)reqctx->pa_type) {
        case KRB5_PADATA_PK_AS_REQ_OLD:
            auth_pack9.clientPublicValue = NULL;
            break;
        case KRB5_PADATA_PK_AS_REQ:
            auth_pack.clientPublicValue = NULL;
            break;
        }
        break;
    default:
        pkiDebug("as_req: unknown key transport protocol %d\n",
                 protocol);
        retval = -1;
        goto cleanup;
    }

    /* Encode the authpack */
    switch((int)reqctx->pa_type) {
    case KRB5_PADATA_PK_AS_REQ:
        retval = k5int_encode_krb5_auth_pack(&auth_pack, &coded_auth_pack);
        break;
    case KRB5_PADATA_PK_AS_REQ_OLD:
        retval = k5int_encode_krb5_auth_pack_draft9(&auth_pack9,
                                                    &coded_auth_pack);
        break;
    }
    if (retval) {
        pkiDebug("failed to encode the AuthPack %d\n", retval);
        goto cleanup;
    }
#ifdef DEBUG_ASN1
    print_buffer_bin((unsigned char *)coded_auth_pack->data,
                     coded_auth_pack->length,
                     "/tmp/client_auth_pack");
#endif

    /* create PKCS7 object from authpack */
    switch((int)reqctx->pa_type) {
    case KRB5_PADATA_PK_AS_REQ:
        init_krb5_pa_pk_as_req(&req);
        if (req == NULL) {
            retval = ENOMEM;
            goto cleanup;
        }
        if (use_content_info(context, reqctx, client)) {
            retval = cms_contentinfo_create(context, plgctx->cryptoctx,
                                            reqctx->cryptoctx, reqctx->idctx,
                                            CMS_SIGN_CLIENT,
                                            (unsigned char *)
                                            coded_auth_pack->data,
                                            coded_auth_pack->length,
                                            (unsigned char **)
                                            &req->signedAuthPack.data,
                                            &req->signedAuthPack.length);
        } else {
            retval = cms_signeddata_create(context, plgctx->cryptoctx,
                                           reqctx->cryptoctx, reqctx->idctx,
                                           CMS_SIGN_CLIENT, 1,
                                           (unsigned char *)
                                           coded_auth_pack->data,
                                           coded_auth_pack->length,
                                           (unsigned char **)
                                           &req->signedAuthPack.data,
                                           &req->signedAuthPack.length);
        }
#ifdef DEBUG_ASN1
        print_buffer_bin((unsigned char *)req->signedAuthPack.data,
                         req->signedAuthPack.length,
                         "/tmp/client_signed_data");
#endif
        break;
    case KRB5_PADATA_PK_AS_REQ_OLD:
        init_krb5_pa_pk_as_req_draft9(&req9);
        if (req9 == NULL) {
            retval = ENOMEM;
            goto cleanup;
        }
        retval = cms_signeddata_create(context, plgctx->cryptoctx,
                                       reqctx->cryptoctx, reqctx->idctx, CMS_SIGN_DRAFT9, 1,
                                       (unsigned char *)coded_auth_pack->data,
                                       coded_auth_pack->length,
                                       (unsigned char **)
                                       &req9->signedAuthPack.data,
                                       &req9->signedAuthPack.length);
        break;
#ifdef DEBUG_ASN1
        print_buffer_bin((unsigned char *)req9->signedAuthPack.data,
                         req9->signedAuthPack.length,
                         "/tmp/client_signed_data_draft9");
#endif
    }
    krb5_free_data(context, coded_auth_pack);
    if (retval) {
        pkiDebug("failed to create pkcs7 signed data\n");
        goto cleanup;
    }

    /* create a list of trusted CAs */
    switch((int)reqctx->pa_type) {
    case KRB5_PADATA_PK_AS_REQ:
        retval = create_krb5_trustedCertifiers(context, plgctx->cryptoctx,
                                               reqctx->cryptoctx, reqctx->idctx, &req->trustedCertifiers);
        if (retval)
            goto cleanup;
        retval = create_issuerAndSerial(context, plgctx->cryptoctx,
                                        reqctx->cryptoctx, reqctx->idctx,
                                        (unsigned char **)&req->kdcPkId.data,
                                        &req->kdcPkId.length);
        if (retval)
            goto cleanup;

        /* Encode the as-req */
        retval = k5int_encode_krb5_pa_pk_as_req(req, as_req);
        break;
    case KRB5_PADATA_PK_AS_REQ_OLD:
        retval = create_issuerAndSerial(context, plgctx->cryptoctx,
                                        reqctx->cryptoctx, reqctx->idctx,
                                        (unsigned char **)&req9->kdcCert.data,
                                        &req9->kdcCert.length);
        if (retval)
            goto cleanup;
        /* Encode the as-req */
        retval = k5int_encode_krb5_pa_pk_as_req_draft9(req9, as_req);
        break;
    }
#ifdef DEBUG_ASN1
    if (!retval)
        print_buffer_bin((unsigned char *)(*as_req)->data, (*as_req)->length,
                         "/tmp/client_as_req");
#endif

cleanup:
    free_krb5_algorithm_identifiers(&cmstypes);
    free(dh_params);
    free(dh_pubkey);
    free_krb5_pa_pk_as_req(&req);
    free_krb5_pa_pk_as_req_draft9(&req9);

    pkiDebug("pkinit_as_req_create retval=%d\n", (int) retval);

    return retval;
}

static krb5_error_code
pa_pkinit_parse_rep(krb5_context context,
                    pkinit_context plgctx,
                    pkinit_req_context reqctx,
                    krb5_kdc_req * request,
                    krb5_pa_data * in_padata,
                    krb5_enctype etype,
                    krb5_keyblock * as_key,
                    krb5_data *encoded_request)
{
    krb5_error_code retval = KRB5KDC_ERR_PREAUTH_FAILED;
    krb5_data asRep = { 0, 0, NULL};

    /*
     * One way or the other - success or failure - no other PA systems can
     * work if the server sent us a PKINIT reply, since only we know how to
     * decrypt the key.
     */
    if ((in_padata == NULL) || (in_padata->length == 0)) {
        pkiDebug("pa_pkinit_parse_rep: no in_padata\n");
        return KRB5KDC_ERR_PREAUTH_FAILED;
    }

    asRep.data = (char *) in_padata->contents;
    asRep.length = in_padata->length;

    retval =
        pkinit_as_rep_parse(context, plgctx, reqctx, in_padata->pa_type,
                            request, &asRep, as_key, etype, encoded_request);
    if (retval) {
        pkiDebug("pkinit_as_rep_parse returned %d (%s)\n",
                 retval, error_message(retval));
        goto cleanup;
    }

    retval = 0;

cleanup:

    return retval;
}

static krb5_error_code
verify_kdc_san(krb5_context context,
               pkinit_context plgctx,
               pkinit_req_context reqctx,
               krb5_principal kdcprinc,
               int *valid_san,
               int *need_eku_checking)
{
    krb5_error_code retval;
    char **certhosts = NULL, **cfghosts = NULL, **hostptr;
    krb5_principal *princs = NULL;
    unsigned char ***get_dns;
    int i, j;

    *valid_san = 0;
    *need_eku_checking = 1;

    retval = pkinit_libdefault_strings(context,
                                       krb5_princ_realm(context, kdcprinc),
                                       KRB5_CONF_PKINIT_KDC_HOSTNAME,
                                       &cfghosts);
    if (retval || cfghosts == NULL) {
        pkiDebug("%s: No pkinit_kdc_hostname values found in config file\n",
                 __FUNCTION__);
        get_dns = NULL;
    } else {
        pkiDebug("%s: pkinit_kdc_hostname values found in config file\n",
                 __FUNCTION__);
        for (hostptr = cfghosts; *hostptr != NULL; hostptr++)
            TRACE_PKINIT_CLIENT_SAN_CONFIG_DNSNAME(context, *hostptr);
        get_dns = (unsigned char ***)&certhosts;
    }

    retval = crypto_retrieve_cert_sans(context, plgctx->cryptoctx,
                                       reqctx->cryptoctx, reqctx->idctx,
                                       &princs, NULL, get_dns);
    if (retval) {
        pkiDebug("%s: error from retrieve_certificate_sans()\n", __FUNCTION__);
        TRACE_PKINIT_CLIENT_SAN_ERR(context);
        retval = KRB5KDC_ERR_KDC_NAME_MISMATCH;
        goto out;
    }
    for (i = 0; princs != NULL && princs[i] != NULL; i++)
        TRACE_PKINIT_CLIENT_SAN_KDCCERT_PRINC(context, princs[i]);
    if (certhosts != NULL) {
        for (hostptr = certhosts; *hostptr != NULL; hostptr++)
            TRACE_PKINIT_CLIENT_SAN_KDCCERT_DNSNAME(context, *hostptr);
    }
#if 0
    retval = call_san_checking_plugins(context, plgctx, reqctx, idctx,
                                       princs, hosts, &plugin_decision,
                                       need_eku_checking);
    pkiDebug("%s: call_san_checking_plugins() returned retval %d\n",
             __FUNCTION__);
    if (retval) {
        retval = KRB5KDC_ERR_KDC_NAME_MISMATCH;
        goto out;
    }
    pkiDebug("%s: call_san_checking_plugins() returned decision %d and "
             "need_eku_checking %d\n",
             __FUNCTION__, plugin_decision, *need_eku_checking);
    if (plugin_decision != NO_DECISION) {
        retval = plugin_decision;
        goto out;
    }
#endif

    pkiDebug("%s: Checking pkinit sans\n", __FUNCTION__);
    for (i = 0; princs != NULL && princs[i] != NULL; i++) {
        if (krb5_principal_compare(context, princs[i], kdcprinc)) {
            TRACE_PKINIT_CLIENT_SAN_MATCH_PRINC(context, kdcprinc);
            pkiDebug("%s: pkinit san match found\n", __FUNCTION__);
            *valid_san = 1;
            *need_eku_checking = 0;
            retval = 0;
            goto out;
        }
    }
    pkiDebug("%s: no pkinit san match found\n", __FUNCTION__);

    if (certhosts == NULL) {
        pkiDebug("%s: no certhosts (or we wouldn't accept them anyway)\n",
                 __FUNCTION__);
        retval = KRB5KDC_ERR_KDC_NAME_MISMATCH;
        goto out;
    }

    for (i = 0; certhosts[i] != NULL; i++) {
        for (j = 0; cfghosts != NULL && cfghosts[j] != NULL; j++) {
            pkiDebug("%s: comparing cert name '%s' with config name '%s'\n",
                     __FUNCTION__, certhosts[i], cfghosts[j]);
            if (strcasecmp(certhosts[i], cfghosts[j]) == 0) {
                TRACE_PKINIT_CLIENT_SAN_MATCH_DNSNAME(context, certhosts[i]);
                pkiDebug("%s: we have a dnsName match\n", __FUNCTION__);
                *valid_san = 1;
                retval = 0;
                goto out;
            }
        }
    }
    TRACE_PKINIT_CLIENT_SAN_MATCH_NONE(context);
    pkiDebug("%s: no dnsName san match found\n", __FUNCTION__);

    /* We found no match */
    retval = 0;

out:
    if (princs != NULL) {
        for (i = 0; princs[i] != NULL; i++)
            krb5_free_principal(context, princs[i]);
        free(princs);
    }
    if (certhosts != NULL) {
        for (i = 0; certhosts[i] != NULL; i++)
            free(certhosts[i]);
        free(certhosts);
    }
    if (cfghosts != NULL)
        profile_free_list(cfghosts);

    pkiDebug("%s: returning retval %d, valid_san %d, need_eku_checking %d\n",
             __FUNCTION__, retval, *valid_san, *need_eku_checking);
    return retval;
}

static krb5_error_code
verify_kdc_eku(krb5_context context,
               pkinit_context plgctx,
               pkinit_req_context reqctx,
               int *eku_accepted)
{
    krb5_error_code retval;

    *eku_accepted = 0;

    if (reqctx->opts->require_eku == 0) {
        TRACE_PKINIT_CLIENT_EKU_SKIP(context);
        pkiDebug("%s: configuration requests no EKU checking\n", __FUNCTION__);
        *eku_accepted = 1;
        retval = 0;
        goto out;
    }
    retval = crypto_check_cert_eku(context, plgctx->cryptoctx,
                                   reqctx->cryptoctx, reqctx->idctx,
                                   1, /* kdc cert */
                                   reqctx->opts->accept_secondary_eku,
                                   eku_accepted);
    if (retval) {
        pkiDebug("%s: Error from crypto_check_cert_eku %d (%s)\n",
                 __FUNCTION__, retval, error_message(retval));
        goto out;
    }

out:
    if (*eku_accepted)
        TRACE_PKINIT_CLIENT_EKU_ACCEPT(context);
    else
        TRACE_PKINIT_CLIENT_EKU_REJECT(context);
    pkiDebug("%s: returning retval %d, eku_accepted %d\n",
             __FUNCTION__, retval, *eku_accepted);
    return retval;
}

/*
 * Parse PA-PK-AS-REP message. Optionally evaluates the message's
 * certificate chain.
 * Optionally returns various components.
 */
static krb5_error_code
pkinit_as_rep_parse(krb5_context context,
                    pkinit_context plgctx,
                    pkinit_req_context reqctx,
                    krb5_preauthtype pa_type,
                    krb5_kdc_req *request,
                    const krb5_data *as_rep,
                    krb5_keyblock *key_block,
                    krb5_enctype etype,
                    krb5_data *encoded_request)
{
    krb5_error_code retval = KRB5KDC_ERR_PREAUTH_FAILED;
    krb5_principal kdc_princ = NULL;
    krb5_pa_pk_as_rep *kdc_reply = NULL;
    krb5_kdc_dh_key_info *kdc_dh = NULL;
    krb5_reply_key_pack *key_pack = NULL;
    krb5_data dh_data = { 0, 0, NULL };
    unsigned char *client_key = NULL, *kdc_hostname = NULL;
    unsigned int client_key_len = 0;
    krb5_checksum cksum = {0, 0, 0, NULL};
    krb5_data k5data;
    krb5_data secret;
    int valid_san = 0;
    int valid_eku = 0;
    int need_eku_checking = 1;

    assert((as_rep != NULL) && (key_block != NULL));

#ifdef DEBUG_ASN1
    print_buffer_bin((unsigned char *)as_rep->data, as_rep->length,
                     "/tmp/client_as_rep");
#endif

    if ((retval = k5int_decode_krb5_pa_pk_as_rep(as_rep, &kdc_reply))) {
        pkiDebug("decode_krb5_as_rep failed %d\n", retval);
        return retval;
    }

    switch(kdc_reply->choice) {
    case choice_pa_pk_as_rep_dhInfo:
        pkiDebug("as_rep: DH key transport algorithm\n");
#ifdef DEBUG_ASN1
        print_buffer_bin(kdc_reply->u.dh_Info.dhSignedData.data,
                         kdc_reply->u.dh_Info.dhSignedData.length, "/tmp/client_kdc_signeddata");
#endif
        if ((retval = cms_signeddata_verify(context, plgctx->cryptoctx,
                                            reqctx->cryptoctx, reqctx->idctx, CMS_SIGN_SERVER,
                                            reqctx->opts->require_crl_checking,
                                            (unsigned char *)
                                            kdc_reply->u.dh_Info.dhSignedData.data,
                                            kdc_reply->u.dh_Info.dhSignedData.length,
                                            (unsigned char **)&dh_data.data,
                                            &dh_data.length,
                                            NULL, NULL, NULL)) != 0) {
            pkiDebug("failed to verify pkcs7 signed data\n");
            TRACE_PKINIT_CLIENT_REP_DH_FAIL(context);
            goto cleanup;
        }
        TRACE_PKINIT_CLIENT_REP_DH(context);
        break;
    case choice_pa_pk_as_rep_encKeyPack:
        pkiDebug("as_rep: RSA key transport algorithm\n");
        if ((retval = cms_envelopeddata_verify(context, plgctx->cryptoctx,
                                               reqctx->cryptoctx, reqctx->idctx, pa_type,
                                               reqctx->opts->require_crl_checking,
                                               (unsigned char *)
                                               kdc_reply->u.encKeyPack.data,
                                               kdc_reply->u.encKeyPack.length,
                                               (unsigned char **)&dh_data.data,
                                               &dh_data.length)) != 0) {
            pkiDebug("failed to verify pkcs7 enveloped data\n");
            TRACE_PKINIT_CLIENT_REP_RSA_FAIL(context);
            goto cleanup;
        }
        TRACE_PKINIT_CLIENT_REP_RSA(context);
        break;
    default:
        pkiDebug("unknown as_rep type %d\n", kdc_reply->choice);
        retval = -1;
        goto cleanup;
    }
    retval = krb5_build_principal_ext(context, &kdc_princ,
                                      request->server->realm.length,
                                      request->server->realm.data,
                                      strlen(KRB5_TGS_NAME), KRB5_TGS_NAME,
                                      request->server->realm.length,
                                      request->server->realm.data,
                                      0);
    if (retval)
        goto cleanup;
    retval = verify_kdc_san(context, plgctx, reqctx, kdc_princ,
                            &valid_san, &need_eku_checking);
    if (retval)
        goto cleanup;
    if (!valid_san) {
        pkiDebug("%s: did not find an acceptable SAN in KDC certificate\n",
                 __FUNCTION__);
        retval = KRB5KDC_ERR_KDC_NAME_MISMATCH;
        goto cleanup;
    }

    if (need_eku_checking) {
        retval = verify_kdc_eku(context, plgctx, reqctx,
                                &valid_eku);
        if (retval)
            goto cleanup;
        if (!valid_eku) {
            pkiDebug("%s: did not find an acceptable EKU in KDC certificate\n",
                     __FUNCTION__);
            retval = KRB5KDC_ERR_INCONSISTENT_KEY_PURPOSE;
            goto cleanup;
        }
    } else
        pkiDebug("%s: skipping EKU check\n", __FUNCTION__);

    OCTETDATA_TO_KRB5DATA(&dh_data, &k5data);

    switch(kdc_reply->choice) {
    case choice_pa_pk_as_rep_dhInfo:
#ifdef DEBUG_ASN1
        print_buffer_bin(dh_data.data, dh_data.length,
                         "/tmp/client_dh_key");
#endif
        if ((retval = k5int_decode_krb5_kdc_dh_key_info(&k5data,
                                                        &kdc_dh)) != 0) {
            pkiDebug("failed to decode kdc_dh_key_info\n");
            goto cleanup;
        }

        /* client after KDC reply */
        if ((retval = client_process_dh(context, plgctx->cryptoctx,
                                        reqctx->cryptoctx, reqctx->idctx,
                                        (unsigned char *)
                                        kdc_dh->subjectPublicKey.data,
                                        kdc_dh->subjectPublicKey.length,
                                        &client_key, &client_key_len)) != 0) {
            pkiDebug("failed to process dh params\n");
            goto cleanup;
        }

        /* If we have a KDF algorithm ID, call the algorithm agility KDF... */
        if (kdc_reply->u.dh_Info.kdfID) {
            secret.length = client_key_len;
            secret.data = (char *)client_key;

            retval = pkinit_alg_agility_kdf(context, &secret,
                                            kdc_reply->u.dh_Info.kdfID,
                                            request->client, request->server,
                                            etype, encoded_request,
                                            (krb5_data *)as_rep, key_block);

            if (retval) {
                pkiDebug("failed to create key pkinit_alg_agility_kdf %s\n",
                         error_message(retval));
                goto cleanup;
            }
            TRACE_PKINIT_CLIENT_KDF_ALG(context, kdc_reply->u.dh_Info.kdfID,
                                        key_block);

            /* ...otherwise, use the older octetstring2key function. */
        } else {

            retval = pkinit_octetstring2key(context, etype, client_key,
                                            client_key_len, key_block);
            if (retval) {
                pkiDebug("failed to create key pkinit_octetstring2key %s\n",
                         error_message(retval));
                goto cleanup;
            }
            TRACE_PKINIT_CLIENT_KDF_OS2K(context, key_block);
        }

        break;
    case choice_pa_pk_as_rep_encKeyPack:
#ifdef DEBUG_ASN1
        print_buffer_bin(dh_data.data, dh_data.length,
                         "/tmp/client_key_pack");
#endif
        retval = k5int_decode_krb5_reply_key_pack(&k5data, &key_pack);
        if (retval) {
            pkiDebug("failed to decode reply_key_pack\n");
            goto cleanup;
        }
        /*
         * This is hack but Windows sends back SHA1 checksum
         * with checksum type of 14. There is currently no
         * checksum type of 14 defined.
         */
        if (key_pack->asChecksum.checksum_type == 14)
            key_pack->asChecksum.checksum_type = CKSUMTYPE_NIST_SHA;
        retval = krb5_c_make_checksum(context,
                                      key_pack->asChecksum.checksum_type,
                                      &key_pack->replyKey,
                                      KRB5_KEYUSAGE_TGS_REQ_AUTH_CKSUM,
                                      encoded_request, &cksum);
        if (retval) {
            pkiDebug("failed to make a checksum\n");
            goto cleanup;
        }

        if ((cksum.length != key_pack->asChecksum.length) ||
            k5_bcmp(cksum.contents, key_pack->asChecksum.contents,
                    cksum.length) != 0) {
            TRACE_PKINIT_CLIENT_REP_CHECKSUM_FAIL(context, &cksum,
                                                  &key_pack->asChecksum);
            pkiDebug("failed to match the checksums\n");
#ifdef DEBUG_CKSUM
            pkiDebug("calculating checksum on buf size (%d)\n",
                     encoded_request->length);
            print_buffer(encoded_request->data, encoded_request->length);
            pkiDebug("encrypting key (%d)\n", key_pack->replyKey.length);
            print_buffer(key_pack->replyKey.contents,
                         key_pack->replyKey.length);
            pkiDebug("received checksum type=%d size=%d ",
                     key_pack->asChecksum.checksum_type,
                     key_pack->asChecksum.length);
            print_buffer(key_pack->asChecksum.contents,
                         key_pack->asChecksum.length);
            pkiDebug("expected checksum type=%d size=%d ",
                     cksum.checksum_type, cksum.length);
            print_buffer(cksum.contents, cksum.length);
#endif
            goto cleanup;
        } else
            pkiDebug("checksums match\n");

        krb5_copy_keyblock_contents(context, &key_pack->replyKey,
                                    key_block);
        TRACE_PKINIT_CLIENT_REP_RSA_KEY(context, key_block, &cksum);

        break;
    default:
        pkiDebug("unknow as_rep type %d\n", kdc_reply->choice);
        goto cleanup;
    }

    retval = 0;

cleanup:
    free(dh_data.data);
    krb5_free_principal(context, kdc_princ);
    free(client_key);
    free_krb5_kdc_dh_key_info(&kdc_dh);
    free_krb5_pa_pk_as_rep(&kdc_reply);

    if (key_pack != NULL) {
        free_krb5_reply_key_pack(&key_pack);
        free(cksum.contents);
    }

    free(kdc_hostname);

    pkiDebug("pkinit_as_rep_parse returning %d (%s)\n",
             retval, error_message(retval));
    return retval;
}

static void
pkinit_client_profile(krb5_context context,
                      pkinit_context plgctx,
                      pkinit_req_context reqctx,
                      krb5_clpreauth_callbacks cb,
                      krb5_clpreauth_rock rock,
                      const krb5_data *realm)
{
    const char *configured_identity;
    char *eku_string = NULL;

    pkiDebug("pkinit_client_profile %p %p %p %p\n",
             context, plgctx, reqctx, realm);

    pkinit_libdefault_boolean(context, realm,
                              KRB5_CONF_PKINIT_REQUIRE_CRL_CHECKING,
                              reqctx->opts->require_crl_checking,
                              &reqctx->opts->require_crl_checking);
    pkinit_libdefault_integer(context, realm,
                              KRB5_CONF_PKINIT_DH_MIN_BITS,
                              reqctx->opts->dh_size,
                              &reqctx->opts->dh_size);
    if (reqctx->opts->dh_size != 1024 && reqctx->opts->dh_size != 2048
        && reqctx->opts->dh_size != 4096) {
        pkiDebug("%s: invalid value (%d) for pkinit_dh_min_bits, "
                 "using default value (%d) instead\n", __FUNCTION__,
                 reqctx->opts->dh_size, PKINIT_DEFAULT_DH_MIN_BITS);
        reqctx->opts->dh_size = PKINIT_DEFAULT_DH_MIN_BITS;
    }
    pkinit_libdefault_string(context, realm,
                             KRB5_CONF_PKINIT_EKU_CHECKING,
                             &eku_string);
    if (eku_string != NULL) {
        if (strcasecmp(eku_string, "kpKDC") == 0) {
            reqctx->opts->require_eku = 1;
            reqctx->opts->accept_secondary_eku = 0;
        } else if (strcasecmp(eku_string, "kpServerAuth") == 0) {
            reqctx->opts->require_eku = 1;
            reqctx->opts->accept_secondary_eku = 1;
        } else if (strcasecmp(eku_string, "none") == 0) {
            reqctx->opts->require_eku = 0;
            reqctx->opts->accept_secondary_eku = 0;
        } else {
            pkiDebug("%s: Invalid value for pkinit_eku_checking: '%s'\n",
                     __FUNCTION__, eku_string);
        }
        free(eku_string);
    }

    /* Only process anchors here if they were not specified on command line */
    if (reqctx->idopts->anchors == NULL)
        pkinit_libdefault_strings(context, realm,
                                  KRB5_CONF_PKINIT_ANCHORS,
                                  &reqctx->idopts->anchors);
    pkinit_libdefault_strings(context, realm,
                              KRB5_CONF_PKINIT_POOL,
                              &reqctx->idopts->intermediates);
    pkinit_libdefault_strings(context, realm,
                              KRB5_CONF_PKINIT_REVOKE,
                              &reqctx->idopts->crls);
    pkinit_libdefault_strings(context, realm,
                              KRB5_CONF_PKINIT_IDENTITIES,
                              &reqctx->idopts->identity_alt);
    reqctx->do_identity_matching = TRUE;

    /* If we had a primary identity in the stored configuration, pick it up. */
    configured_identity = cb->get_cc_config(context, rock,
                                            "X509_user_identity");
    if (configured_identity != NULL) {
        free(reqctx->idopts->identity);
        reqctx->idopts->identity = strdup(configured_identity);
        reqctx->do_identity_matching = FALSE;
    }
}

/*
 * Convert a PKCS11 token flags value to the subset that we're interested in
 * passing along to our API callers.
 */
static long long
pkinit_client_get_token_flags(unsigned long pkcs11_token_flags)
{
    long long flags = 0;

    if (pkcs11_token_flags & CKF_USER_PIN_COUNT_LOW)
        flags |= KRB5_RESPONDER_PKINIT_FLAGS_TOKEN_USER_PIN_COUNT_LOW;
    if (pkcs11_token_flags & CKF_USER_PIN_FINAL_TRY)
        flags |= KRB5_RESPONDER_PKINIT_FLAGS_TOKEN_USER_PIN_FINAL_TRY;
    if (pkcs11_token_flags & CKF_USER_PIN_LOCKED)
        flags |= KRB5_RESPONDER_PKINIT_FLAGS_TOKEN_USER_PIN_LOCKED;
    return flags;
}

/*
 * Phase one of loading client identity information - call
 * identity_initialize() to load any identities which we can without requiring
 * help from the calling user, and use their names of those which we can't load
 * to construct the challenge for the responder callback.
 */
static krb5_error_code
pkinit_client_prep_questions(krb5_context context,
                             krb5_clpreauth_moddata moddata,
                             krb5_clpreauth_modreq modreq,
                             krb5_get_init_creds_opt *opt,
                             krb5_clpreauth_callbacks cb,
                             krb5_clpreauth_rock rock,
                             krb5_kdc_req *request,
                             krb5_data *encoded_request_body,
                             krb5_data *encoded_previous_request,
                             krb5_pa_data *pa_data)
{
    krb5_error_code retval;
    pkinit_context plgctx = (pkinit_context)moddata;
    pkinit_req_context reqctx = (pkinit_req_context)modreq;
    int i, n;
    const pkinit_deferred_id *deferred_ids;
    const char *identity;
    unsigned long ck_flags;
    char *encoded;
    k5_json_object jval = NULL;
    k5_json_number jflag = NULL;

    if (!reqctx->identity_initialized) {
        pkinit_client_profile(context, plgctx, reqctx, cb, rock,
                              &request->server->realm);
        retval = pkinit_identity_initialize(context, plgctx->cryptoctx,
                                            reqctx->cryptoctx, reqctx->idopts,
                                            reqctx->idctx, cb, rock,
                                            request->client);
        if (retval != 0) {
            TRACE_PKINIT_CLIENT_NO_IDENTITY(context);
            pkiDebug("pkinit_identity_initialize returned %d (%s)\n",
                     retval, error_message(retval));
        }

        reqctx->identity_initialized = TRUE;
        crypto_free_cert_info(context, plgctx->cryptoctx,
                              reqctx->cryptoctx, reqctx->idctx);
        if (retval != 0) {
            pkiDebug("%s: not asking responder question\n", __FUNCTION__);
            retval = 0;
            goto cleanup;
        }
    }

    deferred_ids = crypto_get_deferred_ids(context, reqctx->idctx);
    for (i = 0; deferred_ids != NULL && deferred_ids[i] != NULL; i++)
        continue;
    n = i;

    /* Make sure we don't just return an empty challenge. */
    if (n == 0) {
        pkiDebug("%s: no questions to ask\n", __FUNCTION__);
        retval = 0;
        goto cleanup;
    }

    /* Create the top-level object. */
    retval = k5_json_object_create(&jval);
    if (retval != 0)
        goto cleanup;

    for (i = 0; i < n; i++) {
        /* Add an entry to the top-level object for the identity. */
        identity = deferred_ids[i]->identity;
        ck_flags = deferred_ids[i]->ck_flags;
        /* Calculate the flags value for the bits that that we care about. */
        retval = k5_json_number_create(pkinit_client_get_token_flags(ck_flags),
                                       &jflag);
        if (retval != 0)
            goto cleanup;
        retval = k5_json_object_set(jval, identity, jflag);
        if (retval != 0)
            goto cleanup;
        k5_json_release(jflag);
        jflag = NULL;
    }

    /* Encode and done. */
    retval = k5_json_encode(jval, &encoded);
    if (retval == 0) {
        cb->ask_responder_question(context, rock,
                                   KRB5_RESPONDER_QUESTION_PKINIT,
                                   encoded);
        pkiDebug("%s: asking question '%s'\n", __FUNCTION__, encoded);
        free(encoded);
    }

cleanup:
    k5_json_release(jval);
    k5_json_release(jflag);

    pkiDebug("%s returning %d\n", __FUNCTION__, retval);

    return retval;
}

/*
 * Parse data supplied by the application's responder callback, saving off any
 * PINs and passwords for identities which we noted needed them.
 */
struct save_one_password_data {
    krb5_context context;
    krb5_clpreauth_modreq modreq;
    const char *caller;
};

static void
save_one_password(void *arg, const char *key, k5_json_value val)
{
    struct save_one_password_data *data = arg;
    pkinit_req_context reqctx = (pkinit_req_context)data->modreq;
    const char *password;

    if (k5_json_get_tid(val) == K5_JSON_TID_STRING) {
        password = k5_json_string_utf8(val);
        pkiDebug("%s: \"%s\": %p\n", data->caller, key, password);
        crypto_set_deferred_id(data->context, reqctx->idctx, key, password);
    }
}

static krb5_error_code
pkinit_client_parse_answers(krb5_context context,
                            krb5_clpreauth_moddata moddata,
                            krb5_clpreauth_modreq modreq,
                            krb5_clpreauth_callbacks cb,
                            krb5_clpreauth_rock rock)
{
    krb5_error_code retval;
    const char *encoded;
    k5_json_value jval;
    struct save_one_password_data data;

    data.context = context;
    data.modreq = modreq;
    data.caller = __FUNCTION__;

    encoded = cb->get_responder_answer(context, rock,
                                       KRB5_RESPONDER_QUESTION_PKINIT);
    if (encoded == NULL)
        return 0;

    pkiDebug("pkinit_client_parse_answers: %s\n", encoded);

    retval = k5_json_decode(encoded, &jval);
    if (retval != 0)
        goto cleanup;

    /* Expect that the top-level answer is an object. */
    if (k5_json_get_tid(jval) != K5_JSON_TID_OBJECT) {
        retval = EINVAL;
        goto cleanup;
    }

    /* Store the passed-in per-identity passwords. */
    k5_json_object_iterate(jval, &save_one_password, &data);
    retval = 0;

cleanup:
    if (jval != NULL)
        k5_json_release(jval);
    return retval;
}

static krb5_error_code
pkinit_client_process(krb5_context context, krb5_clpreauth_moddata moddata,
                      krb5_clpreauth_modreq modreq,
                      krb5_get_init_creds_opt *gic_opt,
                      krb5_clpreauth_callbacks cb, krb5_clpreauth_rock rock,
                      krb5_kdc_req *request, krb5_data *encoded_request_body,
                      krb5_data *encoded_previous_request,
                      krb5_pa_data *in_padata,
                      krb5_prompter_fct prompter, void *prompter_data,
                      krb5_pa_data ***out_padata)
{
    krb5_error_code retval = KRB5KDC_ERR_PREAUTH_FAILED;
    krb5_enctype enctype = -1;
    int processing_request = 0;
    pkinit_context plgctx = (pkinit_context)moddata;
    pkinit_req_context reqctx = (pkinit_req_context)modreq;
    krb5_keyblock as_key;

    pkiDebug("pkinit_client_process %p %p %p %p\n",
             context, plgctx, reqctx, request);


    if (plgctx == NULL || reqctx == NULL)
        return EINVAL;

    switch ((int) in_padata->pa_type) {
    case KRB5_PADATA_PKINIT_KX:
        reqctx->rfc6112_kdc = 1;
        return 0;
    case KRB5_PADATA_PK_AS_REQ:
        pkiDebug("processing KRB5_PADATA_PK_AS_REQ\n");
        processing_request = 1;
        break;

    case KRB5_PADATA_PK_AS_REP:
        pkiDebug("processing KRB5_PADATA_PK_AS_REP\n");
        break;
    case KRB5_PADATA_PK_AS_REP_OLD:
    case KRB5_PADATA_PK_AS_REQ_OLD:
        if (in_padata->length == 0) {
            pkiDebug("processing KRB5_PADATA_PK_AS_REQ_OLD\n");
            in_padata->pa_type = KRB5_PADATA_PK_AS_REQ_OLD;
            processing_request = 1;
        } else {
            pkiDebug("processing KRB5_PADATA_PK_AS_REP_OLD\n");
            in_padata->pa_type = KRB5_PADATA_PK_AS_REP_OLD;
        }
        break;
    default:
        pkiDebug("unrecognized patype = %d for PKINIT\n",
                 in_padata->pa_type);
        return EINVAL;
    }

    if (processing_request) {
        pkinit_client_profile(context, plgctx, reqctx, cb, rock,
                              &request->server->realm);
        /* Pull in PINs and passwords for identities which we deferred
         * loading earlier. */
        retval = pkinit_client_parse_answers(context, moddata, modreq,
                                             cb, rock);
        if (retval) {
            if (retval == KRB5KRB_ERR_GENERIC)
                pkiDebug("pkinit responder answers were invalid\n");
            return retval;
        }
        if (!reqctx->identity_prompted) {
            reqctx->identity_prompted = TRUE;
            /*
             * Load identities (again, potentially), prompting, if we can, for
             * anything for which we didn't get an answer from the responder
             * callback.
             */
            pkinit_identity_set_prompter(reqctx->idctx, prompter,
                                         prompter_data);
            retval = pkinit_identity_prompt(context, plgctx->cryptoctx,
                                            reqctx->cryptoctx, reqctx->idopts,
                                            reqctx->idctx, cb, rock,
                                            reqctx->do_identity_matching,
                                            request->client);
            pkinit_identity_set_prompter(reqctx->idctx, NULL, NULL);
            reqctx->identity_prompt_retval = retval;
            if (retval) {
                TRACE_PKINIT_CLIENT_NO_IDENTITY(context);
                pkiDebug("pkinit_identity_prompt returned %d (%s)\n",
                         retval, error_message(retval));
                return retval;
            }
        } else if (reqctx->identity_prompt_retval) {
            retval = reqctx->identity_prompt_retval;
            TRACE_PKINIT_CLIENT_NO_IDENTITY(context);
            pkiDebug("pkinit_identity_prompt previously returned %d (%s)\n",
                     retval, error_message(retval));
            return retval;
        }
        retval = pa_pkinit_gen_req(context, plgctx, reqctx, cb, rock, request,
                                   in_padata->pa_type, out_padata, prompter,
                                   prompter_data, gic_opt);
    } else {
        /*
         * Get the enctype of the reply.
         */
        enctype = cb->get_etype(context, rock);
        retval = pa_pkinit_parse_rep(context, plgctx, reqctx, request,
                                     in_padata, enctype, &as_key,
                                     encoded_previous_request);
        if (retval == 0) {
            retval = cb->set_as_key(context, rock, &as_key);
            krb5_free_keyblock_contents(context, &as_key);
        }
    }

    pkiDebug("pkinit_client_process: returning %d (%s)\n",
             retval, error_message(retval));
    return retval;
}

static krb5_error_code
pkinit_client_tryagain(krb5_context context, krb5_clpreauth_moddata moddata,
                       krb5_clpreauth_modreq modreq,
                       krb5_get_init_creds_opt *gic_opt,
                       krb5_clpreauth_callbacks cb, krb5_clpreauth_rock rock,
                       krb5_kdc_req *request, krb5_data *encoded_request_body,
                       krb5_data *encoded_previous_request,
                       krb5_preauthtype pa_type, krb5_error *err_reply,
                       krb5_pa_data **err_padata, krb5_prompter_fct prompter,
                       void *prompter_data, krb5_pa_data ***out_padata)
{
    krb5_error_code retval = KRB5KDC_ERR_PREAUTH_FAILED;
    pkinit_context plgctx = (pkinit_context)moddata;
    pkinit_req_context reqctx = (pkinit_req_context)modreq;
    krb5_pa_data *pa;
    krb5_data scratch;
    krb5_external_principal_identifier **certifiers = NULL;
    krb5_algorithm_identifier **algId = NULL;
    int do_again = 0;

    pkiDebug("pkinit_client_tryagain %p %p %p %p\n",
             context, plgctx, reqctx, request);

    if (reqctx->pa_type != pa_type || err_padata == NULL)
        return retval;

    for (; *err_padata != NULL && !do_again; err_padata++) {
        pa = *err_padata;
        PADATA_TO_KRB5DATA(pa, &scratch);
        switch (pa->pa_type) {
        case TD_TRUSTED_CERTIFIERS:
        case TD_INVALID_CERTIFICATES:
            retval = k5int_decode_krb5_td_trusted_certifiers(&scratch,
                                                             &certifiers);
            if (retval) {
                pkiDebug("failed to decode sequence of trusted certifiers\n");
                goto cleanup;
            }
            retval = pkinit_process_td_trusted_certifiers(context,
                                                          plgctx->cryptoctx,
                                                          reqctx->cryptoctx,
                                                          reqctx->idctx,
                                                          certifiers,
                                                          pa->pa_type);
            if (!retval)
                do_again = 1;
            break;
        case TD_DH_PARAMETERS:
            retval = k5int_decode_krb5_td_dh_parameters(&scratch, &algId);
            if (retval) {
                pkiDebug("failed to decode td_dh_parameters\n");
                goto cleanup;
            }
            retval = pkinit_process_td_dh_params(context, plgctx->cryptoctx,
                                                 reqctx->cryptoctx,
                                                 reqctx->idctx, algId,
                                                 &reqctx->opts->dh_size);
            if (!retval)
                do_again = 1;
            break;
        default:
            break;
        }
    }

    if (do_again) {
        TRACE_PKINIT_CLIENT_TRYAGAIN(context);
        retval = pa_pkinit_gen_req(context, plgctx, reqctx, cb, rock, request,
                                   pa_type, out_padata, prompter,
                                   prompter_data, gic_opt);
        if (retval)
            goto cleanup;
    }

    retval = 0;
cleanup:
    if (certifiers != NULL)
        free_krb5_external_principal_identifier(&certifiers);

    if (algId != NULL)
        free_krb5_algorithm_identifiers(&algId);

    pkiDebug("pkinit_client_tryagain: returning %d (%s)\n",
             retval, error_message(retval));
    return retval;
}

static int
pkinit_client_get_flags(krb5_context kcontext, krb5_preauthtype patype)
{
    if (patype == KRB5_PADATA_PKINIT_KX)
        return PA_INFO;
    return PA_REAL;
}

/*
 * We want to be notified about KRB5_PADATA_PKINIT_KX in addition to the actual
 * pkinit patypes because RFC 6112 requires anonymous KDCs to send it. We use
 * that to determine whether to use the broken MIT 1.9 behavior of sending
 * ContentInfo rather than SignedData or the RFC 6112 behavior
 */
static krb5_preauthtype supported_client_pa_types[] = {
    KRB5_PADATA_PK_AS_REP,
    KRB5_PADATA_PK_AS_REQ,
    KRB5_PADATA_PK_AS_REP_OLD,
    KRB5_PADATA_PK_AS_REQ_OLD,
    KRB5_PADATA_PKINIT_KX,
    0
};

static void
pkinit_client_req_init(krb5_context context,
                       krb5_clpreauth_moddata moddata,
                       krb5_clpreauth_modreq *modreq_out)
{
    krb5_error_code retval = ENOMEM;
    pkinit_req_context reqctx = NULL;
    pkinit_context plgctx = (pkinit_context)moddata;

    *modreq_out = NULL;

    reqctx = malloc(sizeof(*reqctx));
    if (reqctx == NULL)
        return;
    memset(reqctx, 0, sizeof(*reqctx));

    reqctx->magic = PKINIT_REQ_CTX_MAGIC;
    reqctx->cryptoctx = NULL;
    reqctx->opts = NULL;
    reqctx->idctx = NULL;
    reqctx->idopts = NULL;

    retval = pkinit_init_req_opts(&reqctx->opts);
    if (retval)
        goto cleanup;

    reqctx->opts->require_eku = plgctx->opts->require_eku;
    reqctx->opts->accept_secondary_eku = plgctx->opts->accept_secondary_eku;
    reqctx->opts->dh_or_rsa = plgctx->opts->dh_or_rsa;
    reqctx->opts->allow_upn = plgctx->opts->allow_upn;
    reqctx->opts->require_crl_checking = plgctx->opts->require_crl_checking;

    retval = pkinit_init_req_crypto(&reqctx->cryptoctx);
    if (retval)
        goto cleanup;

    retval = pkinit_init_identity_crypto(&reqctx->idctx);
    if (retval)
        goto cleanup;

    retval = pkinit_dup_identity_opts(plgctx->idopts, &reqctx->idopts);
    if (retval)
        goto cleanup;

    *modreq_out = (krb5_clpreauth_modreq)reqctx;
    pkiDebug("%s: returning reqctx at %p\n", __FUNCTION__, reqctx);

cleanup:
    if (retval) {
        if (reqctx->idctx != NULL)
            pkinit_fini_identity_crypto(reqctx->idctx);
        if (reqctx->cryptoctx != NULL)
            pkinit_fini_req_crypto(reqctx->cryptoctx);
        if (reqctx->opts != NULL)
            pkinit_fini_req_opts(reqctx->opts);
        if (reqctx->idopts != NULL)
            pkinit_fini_identity_opts(reqctx->idopts);
        free(reqctx);
    }

    return;
}

static void
pkinit_client_req_fini(krb5_context context, krb5_clpreauth_moddata moddata,
                       krb5_clpreauth_modreq modreq)
{
    pkinit_req_context reqctx = (pkinit_req_context)modreq;

    pkiDebug("%s: received reqctx at %p\n", __FUNCTION__, reqctx);
    if (reqctx == NULL)
        return;
    if (reqctx->magic != PKINIT_REQ_CTX_MAGIC) {
        pkiDebug("%s: Bad magic value (%x) in req ctx\n",
                 __FUNCTION__, reqctx->magic);
        return;
    }
    if (reqctx->opts != NULL)
        pkinit_fini_req_opts(reqctx->opts);

    if (reqctx->cryptoctx != NULL)
        pkinit_fini_req_crypto(reqctx->cryptoctx);

    if (reqctx->idctx != NULL)
        pkinit_fini_identity_crypto(reqctx->idctx);

    if (reqctx->idopts != NULL)
        pkinit_fini_identity_opts(reqctx->idopts);

    free(reqctx);
    return;
}

static int
pkinit_client_plugin_init(krb5_context context,
                          krb5_clpreauth_moddata *moddata_out)
{
    krb5_error_code retval = ENOMEM;
    pkinit_context ctx = NULL;

    ctx = calloc(1, sizeof(*ctx));
    if (ctx == NULL)
        return ENOMEM;
    memset(ctx, 0, sizeof(*ctx));
    ctx->magic = PKINIT_CTX_MAGIC;
    ctx->opts = NULL;
    ctx->cryptoctx = NULL;
    ctx->idopts = NULL;

    retval = pkinit_accessor_init();
    if (retval)
        goto errout;

    retval = pkinit_init_plg_opts(&ctx->opts);
    if (retval)
        goto errout;

    retval = pkinit_init_plg_crypto(&ctx->cryptoctx);
    if (retval)
        goto errout;

    retval = pkinit_init_identity_opts(&ctx->idopts);
    if (retval)
        goto errout;

    *moddata_out = (krb5_clpreauth_moddata)ctx;

    pkiDebug("%s: returning plgctx at %p\n", __FUNCTION__, ctx);

errout:
    if (retval)
        pkinit_client_plugin_fini(context, (krb5_clpreauth_moddata)ctx);

    return retval;
}

static void
pkinit_client_plugin_fini(krb5_context context, krb5_clpreauth_moddata moddata)
{
    pkinit_context ctx = (pkinit_context)moddata;

    if (ctx == NULL || ctx->magic != PKINIT_CTX_MAGIC) {
        pkiDebug("pkinit_lib_fini: got bad plgctx (%p)!\n", ctx);
        return;
    }
    pkiDebug("%s: got plgctx at %p\n", __FUNCTION__, ctx);

    pkinit_fini_identity_opts(ctx->idopts);
    pkinit_fini_plg_crypto(ctx->cryptoctx);
    pkinit_fini_plg_opts(ctx->opts);
    free(ctx);

}

static krb5_error_code
add_string_to_array(krb5_context context, char ***array, const char *addition)
{
    char **a = *array;
    size_t len;

    for (len = 0; a != NULL && a[len] != NULL; len++);
    a = realloc(a, (len + 2) * sizeof(char *));
    if (a == NULL)
        return ENOMEM;
    *array = a;
    a[len] = strdup(addition);
    if (a[len] == NULL)
        return ENOMEM;
    a[len + 1] = NULL;
    return 0;
}

static krb5_error_code
handle_gic_opt(krb5_context context,
               pkinit_context plgctx,
               const char *attr,
               const char *value)
{
    krb5_error_code retval;

    if (strcmp(attr, "X509_user_identity") == 0) {
        if (plgctx->idopts->identity != NULL) {
            krb5_set_error_message(context, KRB5_PREAUTH_FAILED,
                                   "X509_user_identity can not be given twice\n");
            return KRB5_PREAUTH_FAILED;
        }
        plgctx->idopts->identity = strdup(value);
        if (plgctx->idopts->identity == NULL) {
            krb5_set_error_message(context, ENOMEM,
                                   "Could not duplicate X509_user_identity value\n");
            return ENOMEM;
        }
    } else if (strcmp(attr, "X509_anchors") == 0) {
        retval = add_string_to_array(context, &plgctx->idopts->anchors, value);
        if (retval)
            return retval;
    } else if (strcmp(attr, "flag_RSA_PROTOCOL") == 0) {
        if (strcmp(value, "yes") == 0) {
            pkiDebug("Setting flag to use RSA_PROTOCOL\n");
            plgctx->opts->dh_or_rsa = RSA_PROTOCOL;
        }
    }
    return 0;
}

static krb5_error_code
pkinit_client_gic_opt(krb5_context context, krb5_clpreauth_moddata moddata,
                      krb5_get_init_creds_opt *gic_opt,
                      const char *attr,
                      const char *value)
{
    krb5_error_code retval;
    pkinit_context plgctx = (pkinit_context)moddata;

    pkiDebug("(pkinit) received '%s' = '%s'\n", attr, value);
    retval = handle_gic_opt(context, plgctx, attr, value);
    if (retval)
        return retval;

    return 0;
}

krb5_error_code
clpreauth_pkinit_initvt(krb5_context context, int maj_ver, int min_ver,
                        krb5_plugin_vtable vtable);

krb5_error_code
clpreauth_pkinit_initvt(krb5_context context, int maj_ver, int min_ver,
                        krb5_plugin_vtable vtable)
{
    krb5_clpreauth_vtable vt;

    if (maj_ver != 1)
        return KRB5_PLUGIN_VER_NOTSUPP;
    vt = (krb5_clpreauth_vtable)vtable;
    vt->name = "pkinit";
    vt->pa_type_list = supported_client_pa_types;
    vt->init = pkinit_client_plugin_init;
    vt->fini = pkinit_client_plugin_fini;
    vt->flags = pkinit_client_get_flags;
    vt->request_init = pkinit_client_req_init;
    vt->prep_questions = pkinit_client_prep_questions;
    vt->request_fini = pkinit_client_req_fini;
    vt->process = pkinit_client_process;
    vt->tryagain = pkinit_client_tryagain;
    vt->gic_opts = pkinit_client_gic_opt;
    return 0;
}
