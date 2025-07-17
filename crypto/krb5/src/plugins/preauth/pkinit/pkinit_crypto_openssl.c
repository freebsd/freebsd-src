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
#include "pkinit_crypto_openssl.h"
#include "k5-buf.h"
#include "k5-err.h"
#include "k5-hex.h"
#include <unistd.h>
#include <dirent.h>
#include <arpa/inet.h>

#if OPENSSL_VERSION_NUMBER >= 0x30000000L
#include <openssl/core_names.h>
#include <openssl/kdf.h>
#include <openssl/params.h>
#endif

static krb5_error_code pkinit_init_pkinit_oids(pkinit_plg_crypto_context );
static void pkinit_fini_pkinit_oids(pkinit_plg_crypto_context );

static krb5_error_code pkinit_init_dh_params(pkinit_plg_crypto_context );
static void pkinit_fini_dh_params(pkinit_plg_crypto_context );

static krb5_error_code pkinit_init_certs(pkinit_identity_crypto_context ctx);
static void pkinit_fini_certs(pkinit_identity_crypto_context ctx);

static krb5_error_code pkinit_init_pkcs11(pkinit_identity_crypto_context ctx);
static void pkinit_fini_pkcs11(pkinit_identity_crypto_context ctx);

static krb5_error_code pkinit_sign_data
(krb5_context context, pkinit_identity_crypto_context cryptoctx,
 unsigned char *data, unsigned int data_len,
 unsigned char **sig, unsigned int *sig_len);

static krb5_error_code create_signature
(unsigned char **, unsigned int *, unsigned char *, unsigned int,
 EVP_PKEY *pkey);

static krb5_error_code pkinit_decode_data
(krb5_context context, pkinit_identity_crypto_context cryptoctx,
 const uint8_t *data, unsigned int data_len, uint8_t **decoded,
 unsigned int *decoded_len);

#ifdef DEBUG_DH
static void print_dh(DH *, char *);
static void print_pubkey(BIGNUM *, char *);
#endif

static int prepare_enc_data
(const uint8_t *indata, int indata_len, uint8_t **outdata, int *outdata_len);

static int openssl_callback (int, X509_STORE_CTX *);
static int openssl_callback_ignore_crls (int, X509_STORE_CTX *);

static int pkcs7_decrypt
(krb5_context context, pkinit_identity_crypto_context id_cryptoctx, PKCS7 *p7,
 unsigned char **data_out, unsigned int *len_out);

static ASN1_OBJECT * pkinit_pkcs7type2oid
(pkinit_plg_crypto_context plg_cryptoctx, int pkcs7_type);

static krb5_error_code pkinit_create_sequence_of_principal_identifiers
(krb5_context context, pkinit_plg_crypto_context plg_cryptoctx,
 pkinit_req_crypto_context req_cryptoctx,
 pkinit_identity_crypto_context id_cryptoctx,
 int type, krb5_pa_data ***e_data_out);

#ifndef WITHOUT_PKCS11
static krb5_error_code pkinit_find_private_key
(pkinit_identity_crypto_context, CK_ATTRIBUTE_TYPE usage,
 CK_OBJECT_HANDLE *objp);
static krb5_error_code pkinit_login
(krb5_context context, pkinit_identity_crypto_context id_cryptoctx,
 CK_TOKEN_INFO *tip, const char *password);
static krb5_error_code pkinit_open_session
(krb5_context context, pkinit_identity_crypto_context id_cryptoctx);
#ifdef SILLYDECRYPT
CK_RV pkinit_C_Decrypt
(pkinit_identity_crypto_context id_cryptoctx,
 CK_BYTE_PTR pEncryptedData, CK_ULONG  ulEncryptedDataLen,
 CK_BYTE_PTR pData, CK_ULONG_PTR pulDataLen);
#endif

static krb5_error_code pkinit_sign_data_pkcs11
(krb5_context context, pkinit_identity_crypto_context id_cryptoctx,
 unsigned char *data, unsigned int data_len,
 unsigned char **sig, unsigned int *sig_len);
static krb5_error_code pkinit_decode_data_pkcs11
(krb5_context context, pkinit_identity_crypto_context id_cryptoctx,
 const uint8_t *data, unsigned int data_len, uint8_t **decoded_data,
 unsigned int *decoded_data_len);
#endif  /* WITHOUT_PKCS11 */

static krb5_error_code pkinit_sign_data_fs
(krb5_context context, pkinit_identity_crypto_context id_cryptoctx,
 unsigned char *data, unsigned int data_len,
 unsigned char **sig, unsigned int *sig_len);
static krb5_error_code pkinit_decode_data_fs
(krb5_context context, pkinit_identity_crypto_context id_cryptoctx,
 const uint8_t *data, unsigned int data_len, uint8_t **decoded_data,
 unsigned int *decoded_data_len);

static krb5_error_code
create_krb5_invalidCertificates(krb5_context context,
                                pkinit_plg_crypto_context plg_cryptoctx,
                                pkinit_req_crypto_context req_cryptoctx,
                                pkinit_identity_crypto_context id_cryptoctx,
                                krb5_external_principal_identifier *** ids);

static krb5_error_code
create_identifiers_from_stack(STACK_OF(X509) *sk,
                              krb5_external_principal_identifier *** ids);
static int
wrap_signeddata(unsigned char *data, unsigned int data_len,
                unsigned char **out, unsigned int *out_len);

static const char *
pkcs11err(int err);


#if OPENSSL_VERSION_NUMBER < 0x10100000L

/* 1.1 standardizes constructor and destructor names, renaming
 * EVP_MD_CTX_{create,destroy} and deprecating ASN1_STRING_data. */

#define EVP_MD_CTX_new EVP_MD_CTX_create
#define EVP_MD_CTX_free EVP_MD_CTX_destroy
#define ASN1_STRING_get0_data ASN1_STRING_data

/*
 * 1.1 adds DHX support, which uses the RFC 3279 DomainParameters encoding we
 * need for PKINIT.  For 1.0 we must use the original DH type when creating
 * EVP_PKEY objects.
 */
#define EVP_PKEY_DHX EVP_PKEY_DH

/* 1.1 makes many handle types opaque and adds accessors.  Add compatibility
 * versions of the new accessors we use for pre-1.1. */

#define OBJ_get0_data(o) ((o)->data)
#define OBJ_length(o) ((o)->length)

#define DH_set0_key compat_dh_set0_key
static int
compat_dh_set0_key(DH *dh, BIGNUM *pub, BIGNUM *priv)
{
    if (pub != NULL) {
        BN_clear_free(dh->pub_key);
        dh->pub_key = pub;
    }
    if (priv != NULL) {
        BN_clear_free(dh->priv_key);
        dh->priv_key = priv;
    }
    return 1;
}

#define DH_get0_key compat_dh_get0_key
static void compat_dh_get0_key(const DH *dh, const BIGNUM **pub,
                               const BIGNUM **priv)
{
    if (pub != NULL)
        *pub = dh->pub_key;
    if (priv != NULL)
        *priv = dh->priv_key;
}

#define EVP_PKEY_get0_DH compat_get0_DH
static DH *
compat_get0_DH(const EVP_PKEY *pkey)
{
    if (pkey->type != EVP_PKEY_DH)
        return NULL;
    return pkey->pkey.dh;

}

/* Return true if the cert c includes a key usage which doesn't include u.
 * Define using direct member access for pre-1.1. */
#define ku_reject(c, u)                                                 \
    (((c)->ex_flags & EXFLAG_KUSAGE) && !((c)->ex_kusage & (u)))

#else /* OPENSSL_VERSION_NUMBER >= 0x10100000L */

/* Return true if the cert x includes a key usage which doesn't include u. */
#define ku_reject(c, u) (!(X509_get_key_usage(c) & (u)))

#endif

#if OPENSSL_VERSION_NUMBER < 0x30000000L
/* OpenSSL 3.0 changes several preferred function names. */
#define EVP_PKEY_parameters_eq EVP_PKEY_cmp_parameters
#define EVP_MD_CTX_get0_md EVP_MD_CTX_md
#define EVP_PKEY_get_size EVP_PKEY_size
#define EVP_PKEY_get_bits EVP_PKEY_bits

/*
 * Convert *dh to an EVP_PKEY object, taking ownership of *dh and setting it to
 * NULL.  On error, return NULL and do not take ownership of or change *dh.
 * OpenSSL 3.0 deprecates the low-level DH interfaces, so this helper will only
 * be used with prior versions.
 */
static EVP_PKEY *
dh_to_pkey(DH **dh)
{
    EVP_PKEY *pkey;

    pkey = EVP_PKEY_new();
    if (pkey == NULL)
        return NULL;
    if (!EVP_PKEY_assign(pkey, EVP_PKEY_DHX, *dh)) {
        EVP_PKEY_free(pkey);
        return NULL;
    }
    *dh = NULL;
    return pkey;
}
#endif /* OPENSSL_VERSION_NUMBER < 0x30000000L */

/* Encode a bignum as an ASN.1 integer in DER. */
static int
encode_bn_der(const BIGNUM *bn, uint8_t **der_out, int *len_out)
{
    ASN1_INTEGER *intval;
    int len;
    uint8_t *der = NULL, *outptr;

    intval = BN_to_ASN1_INTEGER(bn, NULL);
    if (intval == NULL)
        return 0;
    len = i2d_ASN1_INTEGER(intval, NULL);
    if (len > 0 && (outptr = der = malloc(len)) != NULL)
        (void)i2d_ASN1_INTEGER(intval, &outptr);
    ASN1_INTEGER_free(intval);
    if (der == NULL)
        return 0;
    *der_out = der;
    *len_out = len;
    return 1;
}

/* Decode an ASN.1 integer, returning a bignum. */
static BIGNUM *
decode_bn_der(const uint8_t *der, size_t len)
{
    ASN1_INTEGER *intval;
    BIGNUM *bn;

    intval = d2i_ASN1_INTEGER(NULL, &der, len);
    if (intval == NULL)
        return NULL;
    bn = ASN1_INTEGER_to_BN(intval, NULL);
    ASN1_INTEGER_free(intval);
    return bn;
}

#if OPENSSL_VERSION_NUMBER >= 0x10100000L
static int
params_valid(EVP_PKEY *params)
{
    EVP_PKEY_CTX *ctx;
    int result;

    ctx = EVP_PKEY_CTX_new(params, NULL);
    if (ctx == NULL)
        return 0;
    result = EVP_PKEY_param_check(ctx);
    EVP_PKEY_CTX_free(ctx);
    return result == 1;
}
#else
static int
params_valid(EVP_PKEY *params)
{
    DH *dh;
    int codes;

    dh = EVP_PKEY_get0_DH(params);
    return (dh == NULL) ? 0 : (DH_check(dh, &codes) && codes == 0);
}
#endif

#if OPENSSL_VERSION_NUMBER >= 0x10100000L

#if OPENSSL_VERSION_NUMBER >= 0x30000000L
static EVP_PKEY *
decode_dh_params(const krb5_data *params_der)
{
    EVP_PKEY *pkey = NULL;
    const uint8_t *inptr = (uint8_t *)params_der->data;
    size_t len = params_der->length;
    OSSL_DECODER_CTX *dctx;
    int ok;

    dctx = OSSL_DECODER_CTX_new_for_pkey(&pkey, "DER", "type-specific", "DHX",
                                         EVP_PKEY_KEY_PARAMETERS, NULL, NULL);
    if (dctx == NULL)
        return NULL;

    ok = OSSL_DECODER_from_data(dctx, &inptr, &len);
    OSSL_DECODER_CTX_free(dctx);
    return ok ? pkey : NULL;
}
#else
static EVP_PKEY *
decode_dh_params(const krb5_data *params_der)
{
    const uint8_t *p = (uint8_t *)params_der->data;
    DH *dh;
    EVP_PKEY *pkey;

    dh = d2i_DHxparams(NULL, &p, params_der->length);
    pkey = dh_to_pkey(&dh);
    DH_free(dh);
    return pkey;
}
#endif

static krb5_error_code
encode_spki(EVP_PKEY *pkey, krb5_data *spki_out)
{
    krb5_error_code ret = ENOMEM;
    int len;
    uint8_t *outptr;

    len = i2d_PUBKEY(pkey, NULL);
    ret = alloc_data(spki_out, len);
    if (ret)
        goto cleanup;
    outptr = (uint8_t *)spki_out->data;
    (void)i2d_PUBKEY(pkey, &outptr);

cleanup:
    return ret;
}

static EVP_PKEY *
decode_spki(const krb5_data *spki)
{
    const uint8_t *inptr = (uint8_t *)spki->data;

    return d2i_PUBKEY(NULL, &inptr, spki->length);
}

#else /* OPENSSL_VERSION_NUMBER < 0x10100000L */

/*
 * OpenSSL 1.0 has no DHX support, so we need a custom decoder for RFC 3279
 * DomainParameters, and we need to use X509_PUBKEY values to marshal
 * SubjectPublicKeyInfo.
 */

typedef struct {
    ASN1_BIT_STRING *seed;
    BIGNUM *counter;
} int_dhvparams;

typedef struct {
    BIGNUM *p;
    BIGNUM *q;
    BIGNUM *g;
    BIGNUM *j;
    int_dhvparams *vparams;
} int_dhxparams;

ASN1_SEQUENCE(int_dhvparams) = {
    ASN1_SIMPLE(int_dhvparams, seed, ASN1_BIT_STRING),
    ASN1_SIMPLE(int_dhvparams, counter, BIGNUM)
} ASN1_SEQUENCE_END(int_dhvparams);

ASN1_SEQUENCE(int_dhxparams) = {
    ASN1_SIMPLE(int_dhxparams, p, BIGNUM),
    ASN1_SIMPLE(int_dhxparams, g, BIGNUM),
    ASN1_SIMPLE(int_dhxparams, q, BIGNUM),
    ASN1_OPT(int_dhxparams, j, BIGNUM),
    ASN1_OPT(int_dhxparams, vparams, int_dhvparams)
} ASN1_SEQUENCE_END(int_dhxparams);

static EVP_PKEY *
decode_dh_params(const krb5_data *params_der)
{
    int_dhxparams *params;
    DH *dh;
    EVP_PKEY *pkey;
    const uint8_t *p;

    dh = DH_new();
    if (dh == NULL)
        return NULL;

    p = (uint8_t *)params_der->data;
    params = (int_dhxparams *)ASN1_item_d2i(NULL, &p, params_der->length,
                                            ASN1_ITEM_rptr(int_dhxparams));
    if (params == NULL) {
        DH_free(dh);
        return NULL;
    }

    /* Steal p, q, and g from dhparams for dh.  Ignore j and vparams. */
    dh->p = params->p;
    dh->q = params->q;
    dh->g = params->g;
    params->p = params->q = params->g = NULL;
    ASN1_item_free((ASN1_VALUE *)params, ASN1_ITEM_rptr(int_dhxparams));
    pkey = dh_to_pkey(&dh);
    DH_free(dh);
    return pkey;
}

static krb5_error_code
encode_spki(EVP_PKEY *pkey, krb5_data *spki_out)
{
    krb5_error_code ret = ENOMEM;
    const DH *dh;
    uint8_t *param_der = NULL, *pubkey_der = NULL, *outptr;
    int param_der_len, pubkey_der_len, len;
    X509_PUBKEY pubkey;
    int_dhxparams dhxparams;
    X509_ALGOR algor;
    ASN1_OBJECT algorithm;
    ASN1_TYPE parameter;
    ASN1_STRING param_str, pubkey_str;

    dh = EVP_PKEY_get0_DH(pkey);
    if (dh == NULL)
        goto cleanup;

    dhxparams.p = dh->p;
    dhxparams.q = dh->q;
    dhxparams.g = dh->g;
    dhxparams.j = NULL;
    dhxparams.vparams = NULL;
    param_der_len = ASN1_item_i2d((ASN1_VALUE *)&dhxparams, &param_der,
                                  ASN1_ITEM_rptr(int_dhxparams));
    if (param_der_len < 0)
        goto cleanup;
    param_str.length = param_der_len;
    param_str.type = V_ASN1_SEQUENCE;
    param_str.data = param_der;
    param_str.flags = 0;
    parameter.type = V_ASN1_SEQUENCE;
    parameter.value.sequence = &param_str;

    memset(&algorithm, 0, sizeof(algorithm));
    algorithm.data = (uint8_t *)dh_oid.data;
    algorithm.length = dh_oid.length;

    algor.algorithm = &algorithm;
    algor.parameter = &parameter;

    if (!encode_bn_der(dh->pub_key, &pubkey_der, &pubkey_der_len))
        goto cleanup;
    pubkey_str.length = pubkey_der_len;
    pubkey_str.type = V_ASN1_BIT_STRING;
    pubkey_str.data = pubkey_der;
    pubkey_str.flags = ASN1_STRING_FLAG_BITS_LEFT;

    pubkey.algor = &algor;
    pubkey.public_key = &pubkey_str;
    len = i2d_X509_PUBKEY(&pubkey, NULL);
    if (len < 0)
        goto cleanup;
    ret = alloc_data(spki_out, len);
    if (ret)
        goto cleanup;
    outptr = (uint8_t *)spki_out->data;
    i2d_X509_PUBKEY(&pubkey, &outptr);

cleanup:
    OPENSSL_free(param_der);
    free(pubkey_der);
    return ret;
}

static EVP_PKEY *
decode_spki(const krb5_data *spki)
{
    X509_PUBKEY *pubkey = NULL;
    const uint8_t *inptr;
    DH *dh;
    EVP_PKEY *pkey = NULL, *pkey_ret = NULL;
    const ASN1_STRING *params;
    const ASN1_BIT_STRING *public_key;
    krb5_data d;

    inptr = (uint8_t *)spki->data;
    pubkey = d2i_X509_PUBKEY(NULL, &inptr, spki->length);
    if (pubkey == NULL)
        goto cleanup;

    if (pubkey->algor->parameter->type != V_ASN1_SEQUENCE)
        goto cleanup;
    params = pubkey->algor->parameter->value.sequence;
    d = make_data(params->data, params->length);
    pkey = decode_dh_params(&d);
    if (pkey == NULL)
        goto cleanup;
    dh = EVP_PKEY_get0_DH(pkey);
    if (dh == NULL)
        goto cleanup;
    public_key = pubkey->public_key;
    dh->pub_key = decode_bn_der(public_key->data, public_key->length);
    if (dh->pub_key == NULL)
        goto cleanup;

    pkey_ret = pkey;
    pkey = NULL;

cleanup:
    X509_PUBKEY_free(pubkey);
    EVP_PKEY_free(pkey);
    return pkey_ret;
}

#endif /* OPENSSL_VERSION_NUMBER < 0x10100000L */

/* Attempt to specify padded Diffie-Hellman result derivation.  Don't error out
 * if this fails since we also detect short results and adjust them. */
#if OPENSSL_VERSION_NUMBER >= 0x30000000L
static void
set_padded_derivation(EVP_PKEY_CTX *ctx)
{
    EVP_PKEY_CTX_set_dh_pad(ctx, 1);
}
#elif OPENSSL_VERSION_NUMBER >= 0x10100000L
static void
set_padded_derivation(EVP_PKEY_CTX *ctx)
{
    /* We would use EVP_PKEY_CTX_set_dh_pad() but it doesn't work with DHX. */
    EVP_PKEY_CTX_ctrl(ctx, EVP_PKEY_DHX, EVP_PKEY_OP_DERIVE,
                      EVP_PKEY_CTRL_DH_PAD, 1, NULL);
}
#else
static void
set_padded_derivation(EVP_PKEY_CTX *ctx)
{
    /* There's no support for padded derivation in 1.0. */
}
#endif

static int
dh_result(EVP_PKEY *pkey, EVP_PKEY *peer,
          uint8_t **result_out, unsigned int *len_out)
{
    EVP_PKEY_CTX *derive_ctx = NULL;
    int ok = 0;
    uint8_t *buf = NULL;
    size_t len, dh_size = EVP_PKEY_get_size(pkey);

    *result_out = NULL;
    *len_out = 0;

    derive_ctx = EVP_PKEY_CTX_new(pkey, NULL);
    if (derive_ctx == NULL)
        goto cleanup;
    if (EVP_PKEY_derive_init(derive_ctx) <= 0)
        goto cleanup;
    set_padded_derivation(derive_ctx);
    if (EVP_PKEY_derive_set_peer(derive_ctx, peer) <= 0)
        goto cleanup;

    buf = malloc(dh_size);
    if (buf == NULL)
        goto cleanup;
    len = dh_size;
    if (EVP_PKEY_derive(derive_ctx, buf, &len) <= 0)
        goto cleanup;
    if (len < dh_size) {        /* only possible without padded derivation */
        memmove(buf + (dh_size - len), buf, len);
        memset(buf, 0, dh_size - len);
    }

    ok = 1;
    *result_out = buf;
    *len_out = dh_size;
    buf = NULL;

cleanup:
    EVP_PKEY_CTX_free(derive_ctx);
    free(buf);
    return ok;
}

#if OPENSSL_VERSION_NUMBER >= 0x30000000L
static int
dh_pubkey_der(EVP_PKEY *pkey, uint8_t **pubkey_out, unsigned int *len_out)
{
    BIGNUM *pubkey_bn = NULL;
    int len, ok;
    uint8_t *buf;

    if (!EVP_PKEY_get_bn_param(pkey, OSSL_PKEY_PARAM_PUB_KEY, &pubkey_bn))
        return 0;
    ok = encode_bn_der(pubkey_bn, &buf, &len);
    BN_free(pubkey_bn);
    if (ok) {
        *pubkey_out = buf;
        *len_out = len;
    }
    return ok;
}
#else
static int
dh_pubkey_der(EVP_PKEY *pkey, uint8_t **pubkey_out, unsigned int *len_out)
{
    const DH *dh;
    const BIGNUM *pubkey_bn;
    uint8_t *buf;
    int len;

    dh = EVP_PKEY_get0_DH(pkey);
    if (dh == NULL)
        return 0;
    DH_get0_key(dh, &pubkey_bn, NULL);
    if (!encode_bn_der(pubkey_bn, &buf, &len))
        return 0;
    *pubkey_out = buf;
    *len_out = len;
    return 1;
}
#endif

#if OPENSSL_VERSION_NUMBER >= 0x10100000L
/* OpenSSL 1.1 and later will copy the q parameter when generating keys. */
static int
copy_q_openssl10(EVP_PKEY *src, EVP_PKEY *dest)
{
    return 1;
}
#else
/* OpenSSL 1.0 won't copy the q parameter, so we have to do it. */
static int
copy_q_openssl10(EVP_PKEY *src, EVP_PKEY *dest)
{
    DH *dhsrc = EVP_PKEY_get0_DH(src), *dhdest = EVP_PKEY_get0_DH(dest);

    if (dhsrc == NULL || dhsrc->q == NULL || dhdest == NULL)
        return 0;
    if (dhdest->q != NULL)
        return 1;
    dhdest->q = BN_dup(dhsrc->q);
    return dhdest->q != NULL;
}
#endif

static EVP_PKEY *
generate_dh_pkey(EVP_PKEY *params)
{
    EVP_PKEY_CTX *ctx = NULL;
    EVP_PKEY *pkey = NULL;

    ctx = EVP_PKEY_CTX_new(params, NULL);
    if (ctx == NULL)
        goto cleanup;
    if (EVP_PKEY_keygen_init(ctx) <= 0)
        goto cleanup;
    if (EVP_PKEY_keygen(ctx, &pkey) <= 0)
        goto cleanup;
    if (!copy_q_openssl10(params, pkey)) {
        EVP_PKEY_free(pkey);
        pkey = NULL;
    }

cleanup:
    EVP_PKEY_CTX_free(ctx);
    return pkey;
}

#if OPENSSL_VERSION_NUMBER >= 0x30000000L

static EVP_PKEY *
compose_dh_pkey(EVP_PKEY *params, const uint8_t *pubkey_der, size_t der_len)
{
    EVP_PKEY *pkey = NULL, *pkey_ret = NULL;
    BIGNUM *pubkey_bn = NULL;
    uint8_t *pubkey_bin = NULL;
    int binlen;

    pkey = EVP_PKEY_dup(params);
    if (pkey == NULL)
        goto cleanup;

    pubkey_bn = decode_bn_der(pubkey_der, der_len);
    if (pubkey_bn == NULL)
        goto cleanup;
    binlen = EVP_PKEY_get_size(pkey);
    pubkey_bin = malloc(binlen);
    if (pubkey_bin == NULL)
        goto cleanup;
    if (BN_bn2binpad(pubkey_bn, pubkey_bin, binlen) != binlen)
        goto cleanup;
    if (EVP_PKEY_set1_encoded_public_key(pkey, pubkey_bin, binlen) != 1)
        goto cleanup;

    pkey_ret = pkey;
    pkey = NULL;

cleanup:
    EVP_PKEY_free(pkey);
    BN_free(pubkey_bn);
    free(pubkey_bin);
    return pkey_ret;
}

#else /* OPENSSL_VERSION_NUMBER < 0x30000000L */

#if OPENSSL_VERSION_NUMBER >= 0x10100000L
static DH *
dup_dh_params(DH *src)
{
    return DHparams_dup(src);
}
#else
/* DHparams_dup() won't copy q in OpenSSL 1.0. */
static DH *
dup_dh_params(DH *src)
{
    DH *dh;

    dh = DH_new();
    if (dh == NULL)
        return NULL;
    dh->p = BN_dup(src->p);
    dh->q = BN_dup(src->q);
    dh->g = BN_dup(src->g);
    if (dh->p == NULL || dh->q == NULL || dh->g == NULL) {
        DH_free(dh);
        return NULL;
    }
    return dh;
}
#endif

static EVP_PKEY *
compose_dh_pkey(EVP_PKEY *params, const uint8_t *pubkey_der, size_t der_len)
{
    DH *dhparams, *dh = NULL;
    EVP_PKEY *pkey = NULL;
    BIGNUM *pubkey_bn = NULL;

    pubkey_bn = decode_bn_der(pubkey_der, der_len);
    if (pubkey_bn == NULL)
        goto cleanup;

    dhparams = EVP_PKEY_get0_DH(params);
    if (dhparams == NULL)
        goto cleanup;
    dh = dup_dh_params(dhparams);
    if (dh == NULL)
        goto cleanup;
    if (!DH_set0_key(dh, pubkey_bn, NULL))
        goto cleanup;
    pubkey_bn = NULL;

    pkey = dh_to_pkey(&dh);

cleanup:
    BN_free(pubkey_bn);
    DH_free(dh);
    return pkey;
}

#endif /* OPENSSL_VERSION_NUMBER < 0x30000000L */

static struct pkcs11_errstrings {
    short code;
    char *text;
} pkcs11_errstrings[] = {
    { 0x0,      "ok" },
    { 0x1,      "cancel" },
    { 0x2,      "host memory" },
    { 0x3,      "slot id invalid" },
    { 0x5,      "general error" },
    { 0x6,      "function failed" },
    { 0x7,      "arguments bad" },
    { 0x8,      "no event" },
    { 0x9,      "need to create threads" },
    { 0xa,      "cant lock" },
    { 0x10,     "attribute read only" },
    { 0x11,     "attribute sensitive" },
    { 0x12,     "attribute type invalid" },
    { 0x13,     "attribute value invalid" },
    { 0x20,     "data invalid" },
    { 0x21,     "data len range" },
    { 0x30,     "device error" },
    { 0x31,     "device memory" },
    { 0x32,     "device removed" },
    { 0x40,     "encrypted data invalid" },
    { 0x41,     "encrypted data len range" },
    { 0x50,     "function canceled" },
    { 0x51,     "function not parallel" },
    { 0x54,     "function not supported" },
    { 0x60,     "key handle invalid" },
    { 0x62,     "key size range" },
    { 0x63,     "key type inconsistent" },
    { 0x64,     "key not needed" },
    { 0x65,     "key changed" },
    { 0x66,     "key needed" },
    { 0x67,     "key indigestible" },
    { 0x68,     "key function not permitted" },
    { 0x69,     "key not wrappable" },
    { 0x6a,     "key unextractable" },
    { 0x70,     "mechanism invalid" },
    { 0x71,     "mechanism param invalid" },
    { 0x82,     "object handle invalid" },
    { 0x90,     "operation active" },
    { 0x91,     "operation not initialized" },
    { 0xa0,     "pin incorrect" },
    { 0xa1,     "pin invalid" },
    { 0xa2,     "pin len range" },
    { 0xa3,     "pin expired" },
    { 0xa4,     "pin locked" },
    { 0xb0,     "session closed" },
    { 0xb1,     "session count" },
    { 0xb3,     "session handle invalid" },
    { 0xb4,     "session parallel not supported" },
    { 0xb5,     "session read only" },
    { 0xb6,     "session exists" },
    { 0xb7,     "session read only exists" },
    { 0xb8,     "session read write so exists" },
    { 0xc0,     "signature invalid" },
    { 0xc1,     "signature len range" },
    { 0xd0,     "template incomplete" },
    { 0xd1,     "template inconsistent" },
    { 0xe0,     "token not present" },
    { 0xe1,     "token not recognized" },
    { 0xe2,     "token write protected" },
    { 0xf0,     "unwrapping key handle invalid" },
    { 0xf1,     "unwrapping key size range" },
    { 0xf2,     "unwrapping key type inconsistent" },
    { 0x100,    "user already logged in" },
    { 0x101,    "user not logged in" },
    { 0x102,    "user pin not initialized" },
    { 0x103,    "user type invalid" },
    { 0x104,    "user another already logged in" },
    { 0x105,    "user too many types" },
    { 0x110,    "wrapped key invalid" },
    { 0x112,    "wrapped key len range" },
    { 0x113,    "wrapping key handle invalid" },
    { 0x114,    "wrapping key size range" },
    { 0x115,    "wrapping key type inconsistent" },
    { 0x120,    "random seed not supported" },
    { 0x121,    "random no rng" },
    { 0x130,    "domain params invalid" },
    { 0x150,    "buffer too small" },
    { 0x160,    "saved state invalid" },
    { 0x170,    "information sensitive" },
    { 0x180,    "state unsaveable" },
    { 0x190,    "cryptoki not initialized" },
    { 0x191,    "cryptoki already initialized" },
    { 0x1a0,    "mutex bad" },
    { 0x1a1,    "mutex not locked" },
    { 0x200,    "function rejected" },
    { -1,       NULL }
};

MAKE_INIT_FUNCTION(pkinit_openssl_init);

static krb5_error_code oerr(krb5_context context, krb5_error_code code,
                            const char *fmt, ...)
#if !defined(__cplusplus) && (__GNUC__ > 2)
    __attribute__((__format__(__printf__, 3, 4)))
#endif
    ;

/*
 * Set an error string containing the formatted arguments and the first pending
 * OpenSSL error.  Write the formatted arguments and all pending OpenSSL error
 * messages to the trace log.  Return code, or KRB5KDC_ERR_PREAUTH_FAILED if
 * code is 0.
 */
static krb5_error_code
oerr(krb5_context context, krb5_error_code code, const char *fmt, ...)
{
    unsigned long err;
    va_list ap;
    char *str, buf[128];
    int r;

    if (!code)
        code = KRB5KDC_ERR_PREAUTH_FAILED;

    va_start(ap, fmt);
    r = vasprintf(&str, fmt, ap);
    va_end(ap);
    if (r < 0)
        return code;

    err = ERR_peek_error();
    if (err) {
        krb5_set_error_message(context, code, _("%s: %s"), str,
                               ERR_reason_error_string(err));
    } else {
        krb5_set_error_message(context, code, "%s", str);
    }

    TRACE_PKINIT_OPENSSL_ERROR(context, str);
    while ((err = ERR_get_error()) != 0) {
        ERR_error_string_n(err, buf, sizeof(buf));
        TRACE_PKINIT_OPENSSL_ERROR(context, buf);
    }

    free(str);
    return code;
}

/*
 * Set an appropriate error string containing msg for a certificate
 * verification failure from certctx.  Write the message and all pending
 * OpenSSL error messages to the trace log.  Return code, or
 * KRB5KDC_ERR_PREAUTH_FAILED if code is 0.
 */
static krb5_error_code
oerr_cert(krb5_context context, krb5_error_code code, X509_STORE_CTX *certctx,
          const char *msg)
{
    int depth = X509_STORE_CTX_get_error_depth(certctx);
    int err = X509_STORE_CTX_get_error(certctx);
    const char *errstr = X509_verify_cert_error_string(err);

    return oerr(context, code, _("%s (depth %d): %s"), msg, depth, errstr);
}

krb5_error_code
pkinit_init_plg_crypto(pkinit_plg_crypto_context *cryptoctx)
{
    krb5_error_code retval = ENOMEM;
    pkinit_plg_crypto_context ctx = NULL;

    (void)CALL_INIT_FUNCTION(pkinit_openssl_init);

    ctx = malloc(sizeof(*ctx));
    if (ctx == NULL)
        goto out;
    memset(ctx, 0, sizeof(*ctx));

    pkiDebug("%s: initializing openssl crypto context at %p\n",
             __FUNCTION__, ctx);
    retval = pkinit_init_pkinit_oids(ctx);
    if (retval)
        goto out;

    retval = pkinit_init_dh_params(ctx);
    if (retval)
        goto out;

    *cryptoctx = ctx;

out:
    if (retval && ctx != NULL)
        pkinit_fini_plg_crypto(ctx);

    return retval;
}

void
pkinit_fini_plg_crypto(pkinit_plg_crypto_context cryptoctx)
{
    pkiDebug("%s: freeing context at %p\n", __FUNCTION__, cryptoctx);

    if (cryptoctx == NULL)
        return;
    pkinit_fini_pkinit_oids(cryptoctx);
    pkinit_fini_dh_params(cryptoctx);
    free(cryptoctx);
}

krb5_error_code
pkinit_init_identity_crypto(pkinit_identity_crypto_context *idctx)
{
    krb5_error_code retval = ENOMEM;
    pkinit_identity_crypto_context ctx = NULL;

    ctx = malloc(sizeof(*ctx));
    if (ctx == NULL)
        goto out;
    memset(ctx, 0, sizeof(*ctx));

    ctx->identity = NULL;

    retval = pkinit_init_certs(ctx);
    if (retval)
        goto out;

    retval = pkinit_init_pkcs11(ctx);
    if (retval)
        goto out;

    pkiDebug("%s: returning ctx at %p\n", __FUNCTION__, ctx);
    *idctx = ctx;

out:
    if (retval) {
        if (ctx)
            pkinit_fini_identity_crypto(ctx);
    }

    return retval;
}

void
pkinit_fini_identity_crypto(pkinit_identity_crypto_context idctx)
{
    if (idctx == NULL)
        return;

    pkiDebug("%s: freeing ctx at %p\n", __FUNCTION__, idctx);
    if (idctx->deferred_ids != NULL)
        pkinit_free_deferred_ids(idctx->deferred_ids);
    free(idctx->identity);
    pkinit_fini_certs(idctx);
    pkinit_fini_pkcs11(idctx);
    free(idctx);
}

krb5_error_code
pkinit_init_req_crypto(pkinit_req_crypto_context *cryptoctx)
{
    krb5_error_code retval = ENOMEM;
    pkinit_req_crypto_context ctx = NULL;

    ctx = malloc(sizeof(*ctx));
    if (ctx == NULL)
        goto out;
    memset(ctx, 0, sizeof(*ctx));

    ctx->client_pkey = NULL;
    ctx->received_params = NULL;
    ctx->received_cert = NULL;

    *cryptoctx = ctx;

    pkiDebug("%s: returning ctx at %p\n", __FUNCTION__, ctx);
    retval = 0;
out:
    if (retval)
        free(ctx);

    return retval;
}

void
pkinit_fini_req_crypto(pkinit_req_crypto_context req_cryptoctx)
{
    if (req_cryptoctx == NULL)
        return;

    pkiDebug("%s: freeing ctx at %p\n", __FUNCTION__, req_cryptoctx);
    EVP_PKEY_free(req_cryptoctx->client_pkey);
    EVP_PKEY_free(req_cryptoctx->received_params);
    X509_free(req_cryptoctx->received_cert);

    free(req_cryptoctx);
}

static krb5_error_code
pkinit_init_pkinit_oids(pkinit_plg_crypto_context ctx)
{
    ctx->id_pkinit_san = OBJ_txt2obj("1.3.6.1.5.2.2", 1);
    if (ctx->id_pkinit_san == NULL)
        return ENOMEM;

    ctx->id_pkinit_authData = OBJ_txt2obj("1.3.6.1.5.2.3.1", 1);
    if (ctx->id_pkinit_authData == NULL)
        return ENOMEM;

    ctx->id_pkinit_DHKeyData = OBJ_txt2obj("1.3.6.1.5.2.3.2", 1);
    if (ctx->id_pkinit_DHKeyData == NULL)
        return ENOMEM;

    ctx->id_pkinit_rkeyData = OBJ_txt2obj("1.3.6.1.5.2.3.3", 1);
    if (ctx->id_pkinit_rkeyData == NULL)
        return ENOMEM;

    ctx->id_pkinit_KPClientAuth = OBJ_txt2obj("1.3.6.1.5.2.3.4", 1);
    if (ctx->id_pkinit_KPClientAuth == NULL)
        return ENOMEM;

    ctx->id_pkinit_KPKdc = OBJ_txt2obj("1.3.6.1.5.2.3.5", 1);
    if (ctx->id_pkinit_KPKdc == NULL)
        return ENOMEM;

    ctx->id_ms_kp_sc_logon = OBJ_txt2obj("1.3.6.1.4.1.311.20.2.2", 1);
    if (ctx->id_ms_kp_sc_logon == NULL)
        return ENOMEM;

    ctx->id_ms_san_upn = OBJ_txt2obj("1.3.6.1.4.1.311.20.2.3", 1);
    if (ctx->id_ms_san_upn == NULL)
        return ENOMEM;

    ctx->id_kp_serverAuth = OBJ_txt2obj("1.3.6.1.5.5.7.3.1", 1);
    if (ctx->id_kp_serverAuth == NULL)
        return ENOMEM;

    return 0;
}

static krb5_error_code
get_cert(char *filename, X509 **retcert)
{
    X509 *cert = NULL;
    BIO *tmp = NULL;
    int code;
    krb5_error_code retval;

    if (filename == NULL || retcert == NULL)
        return EINVAL;

    *retcert = NULL;

    tmp = BIO_new(BIO_s_file());
    if (tmp == NULL)
        return ENOMEM;

    code = BIO_read_filename(tmp, filename);
    if (code == 0) {
        retval = errno;
        goto cleanup;
    }

    cert = (X509 *) PEM_read_bio_X509(tmp, NULL, NULL, NULL);
    if (cert == NULL) {
        retval = EIO;
        pkiDebug("failed to read certificate from %s\n", filename);
        goto cleanup;
    }
    *retcert = cert;
    retval = 0;
cleanup:
    if (tmp != NULL)
        BIO_free(tmp);
    return retval;
}

struct get_key_cb_data {
    krb5_context context;
    pkinit_identity_crypto_context id_cryptoctx;
    const char *fsname;
    char *filename;
    const char *password;
};

static int
get_key_cb(char *buf, int size, int rwflag, void *userdata)
{
    struct get_key_cb_data *data = userdata;
    pkinit_identity_crypto_context id_cryptoctx;
    krb5_data rdat;
    krb5_prompt kprompt;
    krb5_prompt_type prompt_type;
    krb5_error_code retval;
    char *prompt;

    if (data->id_cryptoctx->defer_id_prompt) {
        /* Supply the identity name to be passed to a responder callback. */
        pkinit_set_deferred_id(&data->id_cryptoctx->deferred_ids,
                               data->fsname, 0, NULL);
        return -1;
    }
    if (data->password == NULL) {
        /* We don't already have a password to use, so prompt for one. */
        if (data->id_cryptoctx->prompter == NULL)
            return -1;
        if (asprintf(&prompt, "%s %s", _("Pass phrase for"),
                     data->filename) < 0)
            return -1;
        rdat.data = buf;
        rdat.length = size;
        kprompt.prompt = prompt;
        kprompt.hidden = 1;
        kprompt.reply = &rdat;
        prompt_type = KRB5_PROMPT_TYPE_PREAUTH;

        /* PROMPTER_INVOCATION */
        k5int_set_prompt_types(data->context, &prompt_type);
        id_cryptoctx = data->id_cryptoctx;
        retval = (data->id_cryptoctx->prompter)(data->context,
                                                id_cryptoctx->prompter_data,
                                                NULL, NULL, 1, &kprompt);
        k5int_set_prompt_types(data->context, 0);
        free(prompt);
        if (retval != 0)
            return -1;
    } else {
        /* Just use the already-supplied password. */
        rdat.length = strlen(data->password);
        if ((int)rdat.length >= size)
            return -1;
        snprintf(buf, size, "%s", data->password);
    }
    return (int)rdat.length;
}

static krb5_error_code
get_key(krb5_context context, pkinit_identity_crypto_context id_cryptoctx,
        char *filename, const char *fsname, EVP_PKEY **retkey,
        const char *password)
{
    EVP_PKEY *pkey = NULL;
    BIO *tmp = NULL;
    struct get_key_cb_data cb_data;
    int code;
    krb5_error_code retval;

    if (filename == NULL || retkey == NULL)
        return EINVAL;

    tmp = BIO_new(BIO_s_file());
    if (tmp == NULL)
        return ENOMEM;

    code = BIO_read_filename(tmp, filename);
    if (code == 0) {
        retval = errno;
        goto cleanup;
    }
    cb_data.context = context;
    cb_data.id_cryptoctx = id_cryptoctx;
    cb_data.filename = filename;
    cb_data.fsname = fsname;
    cb_data.password = password;
    pkey = PEM_read_bio_PrivateKey(tmp, NULL, get_key_cb, &cb_data);
    if (pkey == NULL && !id_cryptoctx->defer_id_prompt) {
        retval = EIO;
        pkiDebug("failed to read private key from %s\n", filename);
        goto cleanup;
    }
    *retkey = pkey;
    retval = 0;
cleanup:
    if (tmp != NULL)
        BIO_free(tmp);
    return retval;
}

static void
pkinit_fini_pkinit_oids(pkinit_plg_crypto_context ctx)
{
    if (ctx == NULL)
        return;
    ASN1_OBJECT_free(ctx->id_pkinit_san);
    ASN1_OBJECT_free(ctx->id_pkinit_authData);
    ASN1_OBJECT_free(ctx->id_pkinit_DHKeyData);
    ASN1_OBJECT_free(ctx->id_pkinit_rkeyData);
    ASN1_OBJECT_free(ctx->id_pkinit_KPClientAuth);
    ASN1_OBJECT_free(ctx->id_pkinit_KPKdc);
    ASN1_OBJECT_free(ctx->id_ms_kp_sc_logon);
    ASN1_OBJECT_free(ctx->id_ms_san_upn);
    ASN1_OBJECT_free(ctx->id_kp_serverAuth);
}

static krb5_error_code
pkinit_init_dh_params(pkinit_plg_crypto_context plgctx)
{
    krb5_error_code retval = ENOMEM;

    plgctx->dh_1024 = decode_dh_params(&oakley_1024);
    if (plgctx->dh_1024 == NULL)
        goto cleanup;

    plgctx->dh_2048 = decode_dh_params(&oakley_2048);
    if (plgctx->dh_2048 == NULL)
        goto cleanup;

    plgctx->dh_4096 = decode_dh_params(&oakley_4096);
    if (plgctx->dh_4096 == NULL)
        goto cleanup;

    retval = 0;

cleanup:
    if (retval)
        pkinit_fini_dh_params(plgctx);

    return retval;
}

static void
pkinit_fini_dh_params(pkinit_plg_crypto_context plgctx)
{
    EVP_PKEY_free(plgctx->dh_1024);
    EVP_PKEY_free(plgctx->dh_2048);
    EVP_PKEY_free(plgctx->dh_4096);
    plgctx->dh_1024 = plgctx->dh_2048 = plgctx->dh_4096 = NULL;
}

static krb5_error_code
pkinit_init_certs(pkinit_identity_crypto_context ctx)
{
    krb5_error_code retval = ENOMEM;
    int i;

    for (i = 0; i < MAX_CREDS_ALLOWED; i++)
        ctx->creds[i] = NULL;
    ctx->my_certs = NULL;
    ctx->cert_index = 0;
    ctx->my_key = NULL;
    ctx->trustedCAs = NULL;
    ctx->intermediateCAs = NULL;
    ctx->revoked = NULL;

    retval = 0;
    return retval;
}

static void
pkinit_fini_certs(pkinit_identity_crypto_context ctx)
{
    if (ctx == NULL)
        return;

    if (ctx->my_certs != NULL)
        sk_X509_pop_free(ctx->my_certs, X509_free);

    if (ctx->my_key != NULL)
        EVP_PKEY_free(ctx->my_key);

    if (ctx->trustedCAs != NULL)
        sk_X509_pop_free(ctx->trustedCAs, X509_free);

    if (ctx->intermediateCAs != NULL)
        sk_X509_pop_free(ctx->intermediateCAs, X509_free);

    if (ctx->revoked != NULL)
        sk_X509_CRL_pop_free(ctx->revoked, X509_CRL_free);
}

static krb5_error_code
pkinit_init_pkcs11(pkinit_identity_crypto_context ctx)
{
    krb5_error_code retval = ENOMEM;

#ifndef WITHOUT_PKCS11
    ctx->p11_module_name = strdup(PKCS11_MODNAME);
    if (ctx->p11_module_name == NULL)
        return retval;
    ctx->p11_module = NULL;
    ctx->slotid = PK_NOSLOT;
    ctx->token_label = NULL;
    ctx->cert_label = NULL;
    ctx->session = CK_INVALID_HANDLE;
    ctx->p11 = NULL;
#endif
    ctx->pkcs11_method = 0;

    retval = 0;
    return retval;
}

static void
pkinit_fini_pkcs11(pkinit_identity_crypto_context ctx)
{
#ifndef WITHOUT_PKCS11
    if (ctx == NULL)
        return;

    if (ctx->p11 != NULL) {
        if (ctx->session != CK_INVALID_HANDLE) {
            ctx->p11->C_CloseSession(ctx->session);
            ctx->session = CK_INVALID_HANDLE;
        }
        ctx->p11->C_Finalize(NULL_PTR);
        ctx->p11 = NULL;
    }
    if (ctx->p11_module != NULL) {
        krb5int_close_plugin(ctx->p11_module);
        ctx->p11_module = NULL;
    }
    free(ctx->p11_module_name);
    free(ctx->token_label);
    free(ctx->cert_id);
    free(ctx->cert_label);
#endif
}

krb5_error_code
pkinit_identity_set_prompter(pkinit_identity_crypto_context id_cryptoctx,
                             krb5_prompter_fct prompter,
                             void *prompter_data)
{
    id_cryptoctx->prompter = prompter;
    id_cryptoctx->prompter_data = prompter_data;

    return 0;
}

/* Create a CMS ContentInfo of type oid containing the octet string in data. */
static krb5_error_code
create_contentinfo(krb5_context context, ASN1_OBJECT *oid,
                   unsigned char *data, size_t data_len, PKCS7 **p7_out)
{
    PKCS7 *p7 = NULL;
    ASN1_OCTET_STRING *ostr = NULL;

    *p7_out = NULL;

    ostr = ASN1_OCTET_STRING_new();
    if (ostr == NULL)
        goto oom;
    if (!ASN1_OCTET_STRING_set(ostr, (unsigned char *)data, data_len))
        goto oom;

    p7 = PKCS7_new();
    if (p7 == NULL)
        goto oom;
    p7->type = OBJ_dup(oid);
    if (p7->type == NULL)
        goto oom;

    p7->d.other = ASN1_TYPE_new();
    if (p7->d.other == NULL)
        goto oom;
    p7->d.other->type = V_ASN1_OCTET_STRING;
    p7->d.other->value.octet_string = ostr;

    *p7_out = p7;
    return 0;

oom:
    if (ostr != NULL)
        ASN1_OCTET_STRING_free(ostr);
    if (p7 != NULL)
        PKCS7_free(p7);
    return ENOMEM;
}

krb5_error_code
cms_contentinfo_create(krb5_context context,                          /* IN */
                       pkinit_plg_crypto_context plg_cryptoctx,       /* IN */
                       pkinit_req_crypto_context req_cryptoctx,       /* IN */
                       pkinit_identity_crypto_context id_cryptoctx,   /* IN */
                       int cms_msg_type,
                       unsigned char *data, unsigned int data_len,
                       unsigned char **out_data, unsigned int *out_data_len)
{
    krb5_error_code retval = ENOMEM;
    ASN1_OBJECT *oid;
    PKCS7 *p7 = NULL;
    unsigned char *p;

    /* Pick the correct oid for the eContentInfo. */
    oid = pkinit_pkcs7type2oid(plg_cryptoctx, cms_msg_type);
    if (oid == NULL)
        goto cleanup;
    retval = create_contentinfo(context, oid, data, data_len, &p7);
    if (retval != 0)
        goto cleanup;
    *out_data_len = i2d_PKCS7(p7, NULL);
    if (!(*out_data_len)) {
        retval = oerr(context, 0, _("Failed to DER encode PKCS7"));
        goto cleanup;
    }
    retval = ENOMEM;
    if ((p = *out_data = malloc(*out_data_len)) == NULL)
        goto cleanup;

    /* DER encode PKCS7 data */
    retval = i2d_PKCS7(p7, &p);
    if (!retval) {
        retval = oerr(context, 0, _("Failed to DER encode PKCS7"));
        goto cleanup;
    }
    retval = 0;
cleanup:
    if (p7)
        PKCS7_free(p7);
    return retval;
}



krb5_error_code
cms_signeddata_create(krb5_context context,
                      pkinit_plg_crypto_context plg_cryptoctx,
                      pkinit_req_crypto_context req_cryptoctx,
                      pkinit_identity_crypto_context id_cryptoctx,
                      int cms_msg_type,
                      unsigned char *data,
                      unsigned int data_len,
                      unsigned char **signed_data,
                      unsigned int *signed_data_len)
{
    krb5_error_code retval = ENOMEM;
    PKCS7  *p7 = NULL, *inner_p7 = NULL;
    PKCS7_SIGNED *p7s = NULL;
    PKCS7_SIGNER_INFO *p7si = NULL;
    unsigned char *p;
    STACK_OF(X509) * cert_stack = NULL;
    ASN1_OCTET_STRING *digest_attr = NULL;
    EVP_MD_CTX *ctx;
    const EVP_MD *md_tmp = NULL;
    unsigned char md_data[EVP_MAX_MD_SIZE], md_data2[EVP_MAX_MD_SIZE];
    unsigned char *digestInfo_buf = NULL, *abuf = NULL;
    unsigned int md_len, md_len2, alen, digestInfo_len;
    STACK_OF(X509_ATTRIBUTE) * sk;
    unsigned char *sig = NULL;
    unsigned int sig_len = 0;
    X509_ALGOR *alg = NULL;
    ASN1_OCTET_STRING *digest = NULL;
    unsigned int alg_len = 0, digest_len = 0;
    unsigned char *y = NULL;
    X509 *cert = NULL;
    ASN1_OBJECT *oid = NULL, *oid_copy;

    /* Start creating PKCS7 data. */
    if ((p7 = PKCS7_new()) == NULL)
        goto cleanup;
    p7->type = OBJ_nid2obj(NID_pkcs7_signed);

    if ((p7s = PKCS7_SIGNED_new()) == NULL)
        goto cleanup;
    p7->d.sign = p7s;
    if (!ASN1_INTEGER_set(p7s->version, 3))
        goto cleanup;

    /* pick the correct oid for the eContentInfo */
    oid = pkinit_pkcs7type2oid(plg_cryptoctx, cms_msg_type);
    if (oid == NULL)
        goto cleanup;

    if (id_cryptoctx->my_certs != NULL) {
        X509_STORE *certstore = NULL;
        X509_STORE_CTX *certctx;
        STACK_OF(X509) *certstack = NULL;
        char buf[DN_BUF_LEN];
        unsigned int i = 0, size = 0;

        /* create a cert chain */
        if ((cert_stack = sk_X509_new_null()) == NULL)
            goto cleanup;

        cert = sk_X509_value(id_cryptoctx->my_certs, id_cryptoctx->cert_index);

        certstore = X509_STORE_new();
        if (certstore == NULL)
            goto cleanup;
        pkiDebug("building certificate chain\n");
        X509_STORE_set_verify_cb(certstore, openssl_callback);
        certctx = X509_STORE_CTX_new();
        if (certctx == NULL)
            goto cleanup;
        X509_STORE_CTX_init(certctx, certstore, cert,
                            id_cryptoctx->intermediateCAs);
        X509_STORE_CTX_trusted_stack(certctx, id_cryptoctx->trustedCAs);
        if (!X509_verify_cert(certctx)) {
            retval = oerr_cert(context, 0, certctx,
                               _("Failed to verify own certificate"));
            goto cleanup;
        }
        certstack = X509_STORE_CTX_get1_chain(certctx);
        size = sk_X509_num(certstack);
        for (i = 0; i < size - 1; i++) {
            X509 *x = sk_X509_value(certstack, i);
            X509_NAME_oneline(X509_get_subject_name(x), buf, sizeof(buf));
            TRACE_PKINIT_CERT_CHAIN_NAME(context, (int)i, buf);
            sk_X509_push(cert_stack, X509_dup(x));
        }
        X509_STORE_CTX_free(certctx);
        X509_STORE_free(certstore);
        sk_X509_pop_free(certstack, X509_free);

        p7s->cert = cert_stack;

        /* fill-in PKCS7_SIGNER_INFO */
        if ((p7si = PKCS7_SIGNER_INFO_new()) == NULL)
            goto cleanup;
        if (!ASN1_INTEGER_set(p7si->version, 1))
            goto cleanup;
        if (!X509_NAME_set(&p7si->issuer_and_serial->issuer,
                           X509_get_issuer_name(cert)))
            goto cleanup;
        /* because ASN1_INTEGER_set is used to set a 'long' we will do
         * things the ugly way. */
        ASN1_INTEGER_free(p7si->issuer_and_serial->serial);
        if (!(p7si->issuer_and_serial->serial =
              ASN1_INTEGER_dup(X509_get_serialNumber(cert))))
            goto cleanup;

        /* will not fill-out EVP_PKEY because it's on the smartcard */

        /* Set digest algs */
        p7si->digest_alg->algorithm = OBJ_nid2obj(NID_sha256);

        if (p7si->digest_alg->parameter != NULL)
            ASN1_TYPE_free(p7si->digest_alg->parameter);
        if ((p7si->digest_alg->parameter = ASN1_TYPE_new()) == NULL)
            goto cleanup;
        p7si->digest_alg->parameter->type = V_ASN1_NULL;

        /* Set sig algs */
        if (p7si->digest_enc_alg->parameter != NULL)
            ASN1_TYPE_free(p7si->digest_enc_alg->parameter);
        p7si->digest_enc_alg->algorithm =
            OBJ_nid2obj(NID_sha256WithRSAEncryption);
        if (!(p7si->digest_enc_alg->parameter = ASN1_TYPE_new()))
            goto cleanup;
        p7si->digest_enc_alg->parameter->type = V_ASN1_NULL;

        /* add signed attributes */
        /* compute sha256 digest over the EncapsulatedContentInfo */
        ctx = EVP_MD_CTX_new();
        if (ctx == NULL)
            goto cleanup;
        EVP_DigestInit_ex(ctx, EVP_sha256(), NULL);
        EVP_DigestUpdate(ctx, data, data_len);
        md_tmp = EVP_MD_CTX_get0_md(ctx);
        EVP_DigestFinal_ex(ctx, md_data, &md_len);
        EVP_MD_CTX_free(ctx);

        /* create a message digest attr */
        digest_attr = ASN1_OCTET_STRING_new();
        ASN1_OCTET_STRING_set(digest_attr, md_data, (int)md_len);
        PKCS7_add_signed_attribute(p7si, NID_pkcs9_messageDigest,
                                   V_ASN1_OCTET_STRING, (char *)digest_attr);

        /* create a content-type attr */
        oid_copy = OBJ_dup(oid);
        if (oid_copy == NULL)
            goto cleanup2;
        PKCS7_add_signed_attribute(p7si, NID_pkcs9_contentType,
                                   V_ASN1_OBJECT, oid_copy);

        /* create the signature over signed attributes. get DER encoded value */
        /* This is the place where smartcard signature needs to be calculated */
        sk = p7si->auth_attr;
        alen = ASN1_item_i2d((ASN1_VALUE *)sk, &abuf,
                             ASN1_ITEM_rptr(PKCS7_ATTR_SIGN));
        if (abuf == NULL)
            goto cleanup2;

#ifndef WITHOUT_PKCS11
        /*
         * Some tokens can only do RSAEncryption without a hash.  To compute
         * sha256WithRSAEncryption, encode the algorithm ID for the hash
         * function and the hash value into an ASN.1 value of type DigestInfo:
         * DigestInfo ::= SEQUENCE {
         *   digestAlgorithm  AlgorithmIdentifier,
         *   digest  OCTET STRING
         * }
         */
        if (id_cryptoctx->pkcs11_method == 1 &&
            id_cryptoctx->mech == CKM_RSA_PKCS) {
            pkiDebug("mech = CKM_RSA_PKCS\n");
            ctx = EVP_MD_CTX_new();
            if (ctx == NULL)
                goto cleanup;
            EVP_DigestInit_ex(ctx, md_tmp, NULL);
            EVP_DigestUpdate(ctx, abuf, alen);
            EVP_DigestFinal_ex(ctx, md_data2, &md_len2);
            EVP_MD_CTX_free(ctx);

            alg = X509_ALGOR_new();
            if (alg == NULL)
                goto cleanup2;
            X509_ALGOR_set0(alg, OBJ_nid2obj(NID_sha256), V_ASN1_NULL, NULL);
            alg_len = i2d_X509_ALGOR(alg, NULL);

            digest = ASN1_OCTET_STRING_new();
            if (digest == NULL)
                goto cleanup2;
            ASN1_OCTET_STRING_set(digest, md_data2, (int)md_len2);
            digest_len = i2d_ASN1_OCTET_STRING(digest, NULL);

            digestInfo_len = ASN1_object_size(1, (int)(alg_len + digest_len),
                                              V_ASN1_SEQUENCE);
            y = digestInfo_buf = malloc(digestInfo_len);
            if (digestInfo_buf == NULL)
                goto cleanup2;
            ASN1_put_object(&y, 1, (int)(alg_len + digest_len), V_ASN1_SEQUENCE,
                            V_ASN1_UNIVERSAL);
            i2d_X509_ALGOR(alg, &y);
            i2d_ASN1_OCTET_STRING(digest, &y);
#ifdef DEBUG_SIG
            pkiDebug("signing buffer\n");
            print_buffer(digestInfo_buf, digestInfo_len);
            print_buffer_bin(digestInfo_buf, digestInfo_len, "/tmp/pkcs7_tosign");
#endif
            retval = pkinit_sign_data(context, id_cryptoctx, digestInfo_buf,
                                      digestInfo_len, &sig, &sig_len);
        } else
#endif
        {
            pkiDebug("mech = %s\n",
                     id_cryptoctx->pkcs11_method == 1 ? "CKM_SHA256_RSA_PKCS" : "FS");
            retval = pkinit_sign_data(context, id_cryptoctx, abuf, alen,
                                      &sig, &sig_len);
        }
#ifdef DEBUG_SIG
        print_buffer(sig, sig_len);
#endif
        free(abuf);
        if (retval)
            goto cleanup2;

        /* Add signature */
        if (!ASN1_STRING_set(p7si->enc_digest, (unsigned char *) sig,
                             (int)sig_len)) {
            retval = oerr(context, 0, _("Failed to add digest attribute"));
            goto cleanup2;
        }
        /* adder signer_info to pkcs7 signed */
        if (!PKCS7_add_signer(p7, p7si))
            goto cleanup2;
    } /* we have a certificate */

    /* start on adding data to the pkcs7 signed */
    retval = create_contentinfo(context, oid, data, data_len, &inner_p7);
    if (p7s->contents != NULL)
        PKCS7_free(p7s->contents);
    p7s->contents = inner_p7;

    *signed_data_len = i2d_PKCS7(p7, NULL);
    if (!(*signed_data_len)) {
        retval = oerr(context, 0, _("Failed to DER encode PKCS7"));
        goto cleanup2;
    }
    retval = ENOMEM;
    if ((p = *signed_data = malloc(*signed_data_len)) == NULL)
        goto cleanup2;

    /* DER encode PKCS7 data */
    retval = i2d_PKCS7(p7, &p);
    if (!retval) {
        retval = oerr(context, 0, _("Failed to DER encode PKCS7"));
        goto cleanup2;
    }
    retval = 0;

#ifdef DEBUG_ASN1
    if (cms_msg_type == CMS_SIGN_CLIENT) {
        print_buffer_bin(*signed_data, *signed_data_len,
                         "/tmp/client_pkcs7_signeddata");
    } else {
        print_buffer_bin(*signed_data, *signed_data_len,
                         "/tmp/kdc_pkcs7_signeddata");
    }
#endif

cleanup2:
    if (p7si) {
#ifndef WITHOUT_PKCS11
        if (id_cryptoctx->pkcs11_method == 1 &&
            id_cryptoctx->mech == CKM_RSA_PKCS) {
            free(digestInfo_buf);
            if (digest != NULL)
                ASN1_OCTET_STRING_free(digest);
        }
#endif
        if (alg != NULL)
            X509_ALGOR_free(alg);
    }
cleanup:
    if (p7 != NULL)
        PKCS7_free(p7);
    free(sig);

    return retval;
}

krb5_error_code
cms_signeddata_verify(krb5_context context,
                      pkinit_plg_crypto_context plgctx,
                      pkinit_req_crypto_context reqctx,
                      pkinit_identity_crypto_context idctx,
                      int cms_msg_type,
                      int require_crl_checking,
                      unsigned char *signed_data,
                      unsigned int signed_data_len,
                      unsigned char **data,
                      unsigned int *data_len,
                      unsigned char **authz_data,
                      unsigned int *authz_data_len,
                      int *is_signed)
{
    /*
     * Warning: Since most openssl functions do not set retval, large chunks of
     * this function assume that retval is always a failure and may go to
     * cleanup without setting retval explicitly. Make sure retval is not set
     * to 0 or errors such as signature verification failure may be converted
     * to success with significant security consequences.
     */
    krb5_error_code retval = KRB5KDC_ERR_PREAUTH_FAILED;
    CMS_ContentInfo *cms = NULL;
    BIO *out = NULL;
    int flags = CMS_NO_SIGNER_CERT_VERIFY;
    int valid_oid = 0;
    unsigned int i = 0;
    unsigned int vflags = 0, size = 0;
    const unsigned char *p = signed_data;
    STACK_OF(CMS_SignerInfo) *si_sk = NULL;
    CMS_SignerInfo *si = NULL;
    X509 *x = NULL;
    X509_STORE *store = NULL;
    X509_STORE_CTX *cert_ctx;
    STACK_OF(X509) *signerCerts = NULL;
    STACK_OF(X509) *intermediateCAs = NULL;
    STACK_OF(X509_CRL) *signerRevoked = NULL;
    STACK_OF(X509_CRL) *revoked = NULL;
    STACK_OF(X509) *verified_chain = NULL;
    ASN1_OBJECT *oid = NULL;
    const ASN1_OBJECT *type = NULL, *etype = NULL;
    ASN1_OCTET_STRING **octets;
    krb5_external_principal_identifier **krb5_verified_chain = NULL;
    krb5_data *authz = NULL;
    char buf[DN_BUF_LEN];

#ifdef DEBUG_ASN1
    print_buffer_bin(signed_data, signed_data_len,
                     "/tmp/client_received_pkcs7_signeddata");
#endif
    if (is_signed)
        *is_signed = 1;

    oid = pkinit_pkcs7type2oid(plgctx, cms_msg_type);
    if (oid == NULL)
        goto cleanup;

    /* decode received CMS message */
    if ((cms = d2i_CMS_ContentInfo(NULL, &p, (int)signed_data_len)) == NULL) {
        retval = oerr(context, 0, _("Failed to decode CMS message"));
        goto cleanup;
    }
    etype = CMS_get0_eContentType(cms);

    /*
     * Prior to 1.10 the MIT client incorrectly emitted the pkinit structure
     * directly in a CMS ContentInfo rather than using SignedData with no
     * signers. Handle that case.
     */
    type = CMS_get0_type(cms);
    if (is_signed && !OBJ_cmp(type, oid)) {
        unsigned char *d;
        *is_signed = 0;
        octets = CMS_get0_content(cms);
        if (!octets || ((*octets)->type != V_ASN1_OCTET_STRING)) {
            retval = KRB5KDC_ERR_PREAUTH_FAILED;
            krb5_set_error_message(context, retval,
                                   _("Invalid pkinit packet: octet string "
                                     "expected"));
            goto cleanup;
        }
        *data_len = ASN1_STRING_length(*octets);
        d = malloc(*data_len);
        if (d == NULL) {
            retval = ENOMEM;
            goto cleanup;
        }
        memcpy(d, ASN1_STRING_get0_data(*octets), *data_len);
        *data = d;
        goto out;
    } else {
        /* Verify that the received message is CMS SignedData message. */
        if (OBJ_obj2nid(type) != NID_pkcs7_signed) {
            pkiDebug("Expected id-signedData CMS msg (received type = %d)\n",
                     OBJ_obj2nid(type));
            krb5_set_error_message(context, retval, _("wrong oid\n"));
            goto cleanup;
        }
    }

    /* setup to verify X509 certificate used to sign CMS message */
    if (!(store = X509_STORE_new()))
        goto cleanup;

    /* check if we are inforcing CRL checking */
    vflags = X509_V_FLAG_CRL_CHECK|X509_V_FLAG_CRL_CHECK_ALL;
    if (require_crl_checking)
        X509_STORE_set_verify_cb(store, openssl_callback);
    else
        X509_STORE_set_verify_cb(store, openssl_callback_ignore_crls);
    X509_STORE_set_flags(store, vflags);

    /*
     * Get the signer's information from the CMS message.  Match signer ID
     * against anchors and intermediate CAs in case no certs are present in the
     * SignedData.  If we start sending kdcPkId values in requests, we'll need
     * to match against the source of that information too.
     */
    CMS_set1_signers_certs(cms, NULL, 0);
    CMS_set1_signers_certs(cms, idctx->trustedCAs, CMS_NOINTERN);
    CMS_set1_signers_certs(cms, idctx->intermediateCAs, CMS_NOINTERN);
    if (((si_sk = CMS_get0_SignerInfos(cms)) == NULL) ||
        ((si = sk_CMS_SignerInfo_value(si_sk, 0)) == NULL)) {
        /* Not actually signed; anonymous case */
        if (!is_signed)
            goto cleanup;
        *is_signed = 0;
        /* We cannot use CMS_dataInit because there may be no digest */
        octets = CMS_get0_content(cms);
        if (octets)
            out = BIO_new_mem_buf((*octets)->data, (*octets)->length);
        if (out == NULL)
            goto cleanup;
    } else {
        CMS_SignerInfo_get0_algs(si, NULL, &x, NULL, NULL);
        if (x == NULL)
            goto cleanup;

        /* create available CRL information (get local CRLs and include CRLs
         * received in the CMS message
         */
        signerRevoked = CMS_get1_crls(cms);
        if (idctx->revoked == NULL)
            revoked = signerRevoked;
        else if (signerRevoked == NULL)
            revoked = idctx->revoked;
        else {
            size = sk_X509_CRL_num(idctx->revoked);
            revoked = sk_X509_CRL_new_null();
            for (i = 0; i < size; i++)
                sk_X509_CRL_push(revoked, sk_X509_CRL_value(idctx->revoked, i));
            size = sk_X509_CRL_num(signerRevoked);
            for (i = 0; i < size; i++)
                sk_X509_CRL_push(revoked, sk_X509_CRL_value(signerRevoked, i));
        }

        /* create available intermediate CAs chains (get local intermediateCAs and
         * include the CA chain received in the CMS message
         */
        signerCerts = CMS_get1_certs(cms);
        if (idctx->intermediateCAs == NULL)
            intermediateCAs = signerCerts;
        else if (signerCerts == NULL)
            intermediateCAs = idctx->intermediateCAs;
        else {
            size = sk_X509_num(idctx->intermediateCAs);
            intermediateCAs = sk_X509_new_null();
            for (i = 0; i < size; i++) {
                sk_X509_push(intermediateCAs,
                             sk_X509_value(idctx->intermediateCAs, i));
            }
            size = sk_X509_num(signerCerts);
            for (i = 0; i < size; i++) {
                sk_X509_push(intermediateCAs, sk_X509_value(signerCerts, i));
            }
        }

        /* initialize x509 context with the received certificate and
         * trusted and intermediate CA chains and CRLs
         */
        cert_ctx = X509_STORE_CTX_new();
        if (cert_ctx == NULL)
            goto cleanup;
        if (!X509_STORE_CTX_init(cert_ctx, store, x, intermediateCAs))
            goto cleanup;

        X509_STORE_CTX_set0_crls(cert_ctx, revoked);

        /* add trusted CAs certificates for cert verification */
        if (idctx->trustedCAs != NULL)
            X509_STORE_CTX_trusted_stack(cert_ctx, idctx->trustedCAs);
        else {
            pkiDebug("unable to find any trusted CAs\n");
            goto cleanup;
        }
#ifdef DEBUG_CERTCHAIN
        if (intermediateCAs != NULL) {
            size = sk_X509_num(intermediateCAs);
            pkiDebug("untrusted cert chain of size %d\n", size);
            for (i = 0; i < size; i++) {
                X509_NAME_oneline(X509_get_subject_name(
                                      sk_X509_value(intermediateCAs, i)), buf, sizeof(buf));
                pkiDebug("cert #%d: %s\n", i, buf);
            }
        }
        if (idctx->trustedCAs != NULL) {
            size = sk_X509_num(idctx->trustedCAs);
            pkiDebug("trusted cert chain of size %d\n", size);
            for (i = 0; i < size; i++) {
                X509_NAME_oneline(X509_get_subject_name(
                                      sk_X509_value(idctx->trustedCAs, i)), buf, sizeof(buf));
                pkiDebug("cert #%d: %s\n", i, buf);
            }
        }
        if (revoked != NULL) {
            size = sk_X509_CRL_num(revoked);
            pkiDebug("CRL chain of size %d\n", size);
            for (i = 0; i < size; i++) {
                X509_CRL *crl = sk_X509_CRL_value(revoked, i);
                X509_NAME_oneline(X509_CRL_get_issuer(crl), buf, sizeof(buf));
                pkiDebug("crls by CA #%d: %s\n", i , buf);
            }
        }
#endif

        i = X509_verify_cert(cert_ctx);
        if (i <= 0) {
            int j = X509_STORE_CTX_get_error(cert_ctx);
            X509 *cert;

            cert = X509_STORE_CTX_get_current_cert(cert_ctx);
            reqctx->received_cert = X509_dup(cert);
            switch(j) {
            case X509_V_ERR_CERT_REVOKED:
                retval = KRB5KDC_ERR_REVOKED_CERTIFICATE;
                break;
            case X509_V_ERR_UNABLE_TO_GET_CRL:
                retval = KRB5KDC_ERR_REVOCATION_STATUS_UNKNOWN;
                break;
            case X509_V_ERR_UNABLE_TO_GET_ISSUER_CERT:
            case X509_V_ERR_UNABLE_TO_GET_ISSUER_CERT_LOCALLY:
                retval = KRB5KDC_ERR_CANT_VERIFY_CERTIFICATE;
                break;
            default:
                retval = KRB5KDC_ERR_INVALID_CERTIFICATE;
            }
            (void)oerr_cert(context, retval, cert_ctx,
                            _("Failed to verify received certificate"));
            if (reqctx->received_cert == NULL)
                strlcpy(buf, "(none)", sizeof(buf));
            else
                X509_NAME_oneline(X509_get_subject_name(reqctx->received_cert),
                                  buf, sizeof(buf));
            pkiDebug("problem with cert DN = %s (error=%d) %s\n", buf, j,
                     X509_verify_cert_error_string(j));
#ifdef DEBUG_CERTCHAIN
            size = sk_X509_num(signerCerts);
            pkiDebug("received cert chain of size %d\n", size);
            for (j = 0; j < size; j++) {
                X509 *tmp_cert = sk_X509_value(signerCerts, j);
                X509_NAME_oneline(X509_get_subject_name(tmp_cert), buf, sizeof(buf));
                pkiDebug("cert #%d: %s\n", j, buf);
            }
#endif
        } else {
            /* retrieve verified certificate chain */
            if (cms_msg_type == CMS_SIGN_CLIENT)
                verified_chain = X509_STORE_CTX_get1_chain(cert_ctx);
        }
        X509_STORE_CTX_free(cert_ctx);
        if (i <= 0)
            goto cleanup;
        out = BIO_new(BIO_s_mem());
        if (CMS_verify(cms, NULL, store, NULL, out, flags) == 0) {
            if (ERR_peek_last_error() == CMS_R_VERIFICATION_FAILURE)
                retval = KRB5KDC_ERR_INVALID_SIG;
            else
                retval = KRB5KDC_ERR_DIGEST_IN_SIGNED_DATA_NOT_ACCEPTED;
            (void)oerr(context, retval, _("Failed to verify CMS message"));
            goto cleanup;
        }
    } /* message was signed */
    if (!OBJ_cmp(etype, oid))
        valid_oid = 1;

    if (valid_oid)
        pkiDebug("CMS Verification successful\n");
    else {
        pkiDebug("wrong oid in eContentType\n");
        print_buffer(OBJ_get0_data(etype), OBJ_length(etype));
        retval = KRB5KDC_ERR_PREAUTH_FAILED;
        krb5_set_error_message(context, retval, "wrong oid\n");
        goto cleanup;
    }

    /* transfer the data from CMS message into return buffer */
    for (size = 0;;) {
        int remain;
        retval = ENOMEM;
        if ((*data = realloc(*data, size + 1024 * 10)) == NULL)
            goto cleanup;
        remain = BIO_read(out, &((*data)[size]), 1024 * 10);
        if (remain <= 0)
            break;
        else
            size += remain;
    }
    *data_len = size;

    if (x) {
        reqctx->received_cert = X509_dup(x);

        /* generate authorization data */
        if (cms_msg_type == CMS_SIGN_CLIENT) {

            if (authz_data == NULL || authz_data_len == NULL)
                goto out;

            *authz_data = NULL;
            retval = create_identifiers_from_stack(verified_chain,
                                                   &krb5_verified_chain);
            if (retval) {
                pkiDebug("create_identifiers_from_stack failed\n");
                goto cleanup;
            }

            retval = k5int_encode_krb5_td_trusted_certifiers((krb5_external_principal_identifier *const *)krb5_verified_chain, &authz);
            if (retval) {
                pkiDebug("encode_krb5_td_trusted_certifiers failed\n");
                goto cleanup;
            }
#ifdef DEBUG_ASN1
            print_buffer_bin((unsigned char *)authz->data, authz->length,
                             "/tmp/kdc_ad_initial_verified_cas");
#endif
            *authz_data = malloc(authz->length);
            if (*authz_data == NULL) {
                retval = ENOMEM;
                goto cleanup;
            }
            memcpy(*authz_data, authz->data, authz->length);
            *authz_data_len = authz->length;
        }
    }
out:
    retval = 0;

cleanup:
    if (out != NULL)
        BIO_free(out);
    if (store != NULL)
        X509_STORE_free(store);
    if (cms != NULL) {
        if (signerCerts != NULL)
            sk_X509_pop_free(signerCerts, X509_free);
        if (idctx->intermediateCAs != NULL && signerCerts)
            sk_X509_free(intermediateCAs);
        if (signerRevoked != NULL)
            sk_X509_CRL_pop_free(signerRevoked, X509_CRL_free);
        if (idctx->revoked != NULL && signerRevoked)
            sk_X509_CRL_free(revoked);
        CMS_ContentInfo_free(cms);
    }
    if (verified_chain != NULL)
        sk_X509_pop_free(verified_chain, X509_free);
    if (krb5_verified_chain != NULL)
        free_krb5_external_principal_identifier(&krb5_verified_chain);
    if (authz != NULL)
        krb5_free_data(context, authz);

    return retval;
}

krb5_error_code
cms_envelopeddata_create(krb5_context context,
                         pkinit_plg_crypto_context plgctx,
                         pkinit_req_crypto_context reqctx,
                         pkinit_identity_crypto_context idctx,
                         krb5_preauthtype pa_type,
                         unsigned char *key_pack,
                         unsigned int key_pack_len,
                         unsigned char **out,
                         unsigned int *out_len)
{

    krb5_error_code retval = ENOMEM;
    PKCS7 *p7 = NULL;
    BIO *in = NULL;
    unsigned char *p = NULL, *signed_data = NULL, *enc_data = NULL;
    int signed_data_len = 0, enc_data_len = 0, flags = PKCS7_BINARY;
    STACK_OF(X509) *encerts = NULL;
    const EVP_CIPHER *cipher = NULL;

    retval = cms_signeddata_create(context, plgctx, reqctx, idctx,
                                   CMS_ENVEL_SERVER, key_pack, key_pack_len,
                                   &signed_data,
                                   (unsigned int *)&signed_data_len);
    if (retval) {
        pkiDebug("failed to create pkcs7 signed data\n");
        goto cleanup;
    }

    /* check we have client's certificate */
    if (reqctx->received_cert == NULL) {
        retval = KRB5KDC_ERR_PREAUTH_FAILED;
        goto cleanup;
    }
    encerts = sk_X509_new_null();
    sk_X509_push(encerts, reqctx->received_cert);

    cipher = EVP_des_ede3_cbc();
    in = BIO_new(BIO_s_mem());
    prepare_enc_data(signed_data, signed_data_len, &enc_data,
                     &enc_data_len);
    retval = BIO_write(in, enc_data, enc_data_len);
    if (retval != enc_data_len) {
        pkiDebug("BIO_write only wrote %d\n", retval);
        goto cleanup;
    }

    p7 = PKCS7_encrypt(encerts, in, cipher, flags);
    if (p7 == NULL) {
        retval = oerr(context, 0, _("Failed to encrypt PKCS7 object"));
        goto cleanup;
    }
    p7->d.enveloped->enc_data->content_type = OBJ_nid2obj(NID_pkcs7_signed);

    *out_len = i2d_PKCS7(p7, NULL);
    if (!*out_len || (p = *out = malloc(*out_len)) == NULL) {
        retval = ENOMEM;
        goto cleanup;
    }
    retval = i2d_PKCS7(p7, &p);
    if (!retval) {
        retval = oerr(context, 0, _("Failed to DER encode PKCS7"));
        goto cleanup;
    }
    retval = 0;

#ifdef DEBUG_ASN1
    print_buffer_bin(*out, *out_len, "/tmp/kdc_enveloped_data");
#endif

cleanup:
    if (p7 != NULL)
        PKCS7_free(p7);
    if (in != NULL)
        BIO_free(in);
    free(signed_data);
    free(enc_data);
    if (encerts != NULL)
        sk_X509_free(encerts);

    return retval;
}

krb5_error_code
cms_envelopeddata_verify(krb5_context context,
                         pkinit_plg_crypto_context plg_cryptoctx,
                         pkinit_req_crypto_context req_cryptoctx,
                         pkinit_identity_crypto_context id_cryptoctx,
                         krb5_preauthtype pa_type,
                         int require_crl_checking,
                         unsigned char *enveloped_data,
                         unsigned int enveloped_data_len,
                         unsigned char **data,
                         unsigned int *data_len)
{
    krb5_error_code retval = KRB5KDC_ERR_PREAUTH_FAILED;
    PKCS7 *p7 = NULL;
    const unsigned char *p = enveloped_data;
    unsigned int tmp_buf_len = 0, tmp_buf2_len = 0, vfy_buf_len = 0;
    unsigned char *tmp_buf = NULL, *tmp_buf2 = NULL, *vfy_buf = NULL;

#ifdef DEBUG_ASN1
    print_buffer_bin(enveloped_data, enveloped_data_len,
                     "/tmp/client_envelopeddata");
#endif
    /* decode received PKCS7 message */
    if ((p7 = d2i_PKCS7(NULL, &p, (int)enveloped_data_len)) == NULL) {
        retval = oerr(context, 0, _("Failed to decode PKCS7"));
        goto cleanup;
    }

    /* verify that the received message is PKCS7 EnvelopedData message */
    if (OBJ_obj2nid(p7->type) != NID_pkcs7_enveloped ||
        p7->d.enveloped == NULL ||
        p7->d.enveloped->enc_data->enc_data == NULL) {
        pkiDebug("Expected id-enveloped PKCS7 msg (received type = %d)\n",
                 OBJ_obj2nid(p7->type));
        krb5_set_error_message(context, retval, "wrong oid\n");
        goto cleanup;
    }

    /* decrypt received PKCS7 message */
    if (pkcs7_decrypt(context, id_cryptoctx, p7, &tmp_buf, &tmp_buf_len)) {
        pkiDebug("PKCS7 decryption successful\n");
    } else {
        retval = oerr(context, 0, _("Failed to decrypt PKCS7 message"));
        goto cleanup;
    }

#ifdef DEBUG_ASN1
    print_buffer_bin(tmp_buf, tmp_buf_len, "/tmp/client_enc_keypack");
#endif
    /* verify PKCS7 SignedData message */
    /* Wrap the signed data to make decoding easier in the verify routine. */
    retval = wrap_signeddata(tmp_buf, tmp_buf_len, &tmp_buf2, &tmp_buf2_len);
    if (retval) {
        pkiDebug("failed to encode signeddata\n");
        goto cleanup;
    }
    vfy_buf = tmp_buf2;
    vfy_buf_len = tmp_buf2_len;

#ifdef DEBUG_ASN1
    print_buffer_bin(vfy_buf, vfy_buf_len, "/tmp/client_enc_keypack2");
#endif

    retval = cms_signeddata_verify(context, plg_cryptoctx, req_cryptoctx,
                                   id_cryptoctx, CMS_ENVEL_SERVER,
                                   require_crl_checking,
                                   vfy_buf, vfy_buf_len,
                                   data, data_len, NULL, NULL, NULL);

    if (!retval)
        pkiDebug("PKCS7 Verification Success\n");
    else {
        pkiDebug("PKCS7 Verification Failure\n");
        goto cleanup;
    }

    retval = 0;

cleanup:

    if (p7 != NULL)
        PKCS7_free(p7);
    free(tmp_buf);
    free(tmp_buf2);

    return retval;
}

static krb5_error_code
crypto_retrieve_X509_sans(krb5_context context,
                          pkinit_plg_crypto_context plgctx,
                          pkinit_req_crypto_context reqctx,
                          X509 *cert,
                          krb5_principal **princs_ret, char ***upn_ret,
                          unsigned char ***dns_ret)
{
    krb5_error_code retval = EINVAL;
    char buf[DN_BUF_LEN];
    int p = 0, u = 0, d = 0, ret = 0, l;
    krb5_principal *princs = NULL;
    char **upns = NULL;
    unsigned char **dnss = NULL;
    unsigned int i, num_sans = 0;
    X509_EXTENSION *ext = NULL;
    GENERAL_NAMES *ialt = NULL;
    GENERAL_NAME *gen = NULL;

    if (princs_ret != NULL)
        *princs_ret = NULL;
    if (upn_ret != NULL)
        *upn_ret = NULL;
    if (dns_ret != NULL)
        *dns_ret = NULL;

    if (princs_ret == NULL && upn_ret == NULL && dns_ret == NULL) {
        pkiDebug("%s: nowhere to return any values!\n", __FUNCTION__);
        return retval;
    }

    if (cert == NULL) {
        pkiDebug("%s: no certificate!\n", __FUNCTION__);
        return retval;
    }

    X509_NAME_oneline(X509_get_subject_name(cert),
                      buf, sizeof(buf));

    l = X509_get_ext_by_NID(cert, NID_subject_alt_name, -1);
    if (l < 0)
        return 0;

    if (!(ext = X509_get_ext(cert, l)) || !(ialt = X509V3_EXT_d2i(ext))) {
        TRACE_PKINIT_SAN_CERT_NONE(context, buf);
        goto cleanup;
    }
    num_sans = sk_GENERAL_NAME_num(ialt);

    /* OK, we're likely returning something. Allocate return values */
    if (princs_ret != NULL) {
        princs = calloc(num_sans + 1, sizeof(krb5_principal));
        if (princs == NULL) {
            retval = ENOMEM;
            goto cleanup;
        }
    }
    if (upn_ret != NULL) {
        upns = calloc(num_sans + 1, sizeof(*upns));
        if (upns == NULL) {
            retval = ENOMEM;
            goto cleanup;
        }
    }
    if (dns_ret != NULL) {
        dnss = calloc(num_sans + 1, sizeof(*dnss));
        if (dnss == NULL) {
            retval = ENOMEM;
            goto cleanup;
        }
    }

    for (i = 0; i < num_sans; i++) {
        krb5_data name = { 0, 0, NULL };

        gen = sk_GENERAL_NAME_value(ialt, i);
        switch (gen->type) {
        case GEN_OTHERNAME:
            name.length = gen->d.otherName->value->value.sequence->length;
            name.data = (char *)gen->d.otherName->value->value.sequence->data;
            if (princs != NULL &&
                OBJ_cmp(plgctx->id_pkinit_san,
                        gen->d.otherName->type_id) == 0) {
#ifdef DEBUG_ASN1
                print_buffer_bin((unsigned char *)name.data, name.length,
                                 "/tmp/pkinit_san");
#endif
                ret = k5int_decode_krb5_principal_name(&name, &princs[p]);
                if (ret) {
                    pkiDebug("%s: failed decoding pkinit san value\n",
                             __FUNCTION__);
                } else {
                    p++;
                }
            } else if (upns != NULL &&
                       OBJ_cmp(plgctx->id_ms_san_upn,
                               gen->d.otherName->type_id) == 0) {
                /* Prevent abuse of embedded null characters. */
                if (memchr(name.data, '\0', name.length))
                    break;
                upns[u] = k5memdup0(name.data, name.length, &ret);
                if (upns[u] == NULL)
                    goto cleanup;
                u++;
            } else {
                pkiDebug("%s: unrecognized othername oid in SAN\n",
                         __FUNCTION__);
                continue;
            }

            break;
        case GEN_DNS:
            if (dnss != NULL) {
                /* Prevent abuse of embedded null characters. */
                if (memchr(gen->d.dNSName->data, '\0', gen->d.dNSName->length))
                    break;
                pkiDebug("%s: found dns name = %s\n", __FUNCTION__,
                         gen->d.dNSName->data);
                dnss[d] = (unsigned char *)
                    strdup((char *)gen->d.dNSName->data);
                if (dnss[d] == NULL) {
                    pkiDebug("%s: failed to duplicate dns name\n",
                             __FUNCTION__);
                } else {
                    d++;
                }
            }
            break;
        default:
            pkiDebug("%s: SAN type = %d expecting %d\n", __FUNCTION__,
                     gen->type, GEN_OTHERNAME);
        }
    }
    sk_GENERAL_NAME_pop_free(ialt, GENERAL_NAME_free);

    TRACE_PKINIT_SAN_CERT_COUNT(context, (int)num_sans, p, u, d, buf);

    retval = 0;
    if (princs != NULL && *princs != NULL) {
        *princs_ret = princs;
        princs = NULL;
    }
    if (upns != NULL && *upns != NULL) {
        *upn_ret = upns;
        upns = NULL;
    }
    if (dnss != NULL && *dnss != NULL) {
        *dns_ret = dnss;
        dnss = NULL;
    }

cleanup:
    for (i = 0; princs != NULL && princs[i] != NULL; i++)
        krb5_free_principal(context, princs[i]);
    free(princs);
    for (i = 0; upns != NULL && upns[i] != NULL; i++)
        free(upns[i]);
    free(upns);
    for (i = 0; dnss != NULL && dnss[i] != NULL; i++)
        free(dnss[i]);
    free(dnss);
    return retval;
}

krb5_error_code
crypto_retrieve_signer_identity(krb5_context context,
                                pkinit_identity_crypto_context id_cryptoctx,
                                const char **identity)
{
    *identity = id_cryptoctx->identity;
    if (*identity == NULL)
        return ENOENT;
    return 0;
}

krb5_error_code
crypto_retrieve_cert_sans(krb5_context context,
                          pkinit_plg_crypto_context plgctx,
                          pkinit_req_crypto_context reqctx,
                          pkinit_identity_crypto_context idctx,
                          krb5_principal **princs_ret, char ***upn_ret,
                          unsigned char ***dns_ret)
{
    krb5_error_code retval = EINVAL;

    if (reqctx->received_cert == NULL) {
        pkiDebug("%s: No certificate!\n", __FUNCTION__);
        return retval;
    }

    return crypto_retrieve_X509_sans(context, plgctx, reqctx,
                                     reqctx->received_cert, princs_ret,
                                     upn_ret, dns_ret);
}

krb5_error_code
crypto_check_cert_eku(krb5_context context,
                      pkinit_plg_crypto_context plgctx,
                      pkinit_req_crypto_context reqctx,
                      pkinit_identity_crypto_context idctx,
                      int checking_kdc_cert,
                      int allow_secondary_usage,
                      int *valid_eku)
{
    char buf[DN_BUF_LEN];
    int found_eku = 0;
    krb5_error_code retval = EINVAL;
    int i;

    *valid_eku = 0;
    if (reqctx->received_cert == NULL)
        goto cleanup;

    X509_NAME_oneline(X509_get_subject_name(reqctx->received_cert),
                      buf, sizeof(buf));

    if ((i = X509_get_ext_by_NID(reqctx->received_cert,
                                 NID_ext_key_usage, -1)) >= 0) {
        EXTENDED_KEY_USAGE *extusage;

        extusage = X509_get_ext_d2i(reqctx->received_cert, NID_ext_key_usage,
                                    NULL, NULL);
        if (extusage) {
            pkiDebug("%s: found eku info in the cert\n", __FUNCTION__);
            for (i = 0; found_eku == 0 && i < sk_ASN1_OBJECT_num(extusage); i++) {
                ASN1_OBJECT *tmp_oid;

                tmp_oid = sk_ASN1_OBJECT_value(extusage, i);
                pkiDebug("%s: checking eku %d of %d, allow_secondary = %d\n",
                         __FUNCTION__, i+1, sk_ASN1_OBJECT_num(extusage),
                         allow_secondary_usage);
                if (checking_kdc_cert) {
                    if ((OBJ_cmp(tmp_oid, plgctx->id_pkinit_KPKdc) == 0)
                        || (allow_secondary_usage
                            && OBJ_cmp(tmp_oid, plgctx->id_kp_serverAuth) == 0))
                        found_eku = 1;
                } else {
                    if ((OBJ_cmp(tmp_oid, plgctx->id_pkinit_KPClientAuth) == 0)
                        || (allow_secondary_usage
                            && OBJ_cmp(tmp_oid, plgctx->id_ms_kp_sc_logon) == 0))
                        found_eku = 1;
                }
            }
        }
        EXTENDED_KEY_USAGE_free(extusage);

        if (found_eku) {
            ASN1_BIT_STRING *usage = NULL;

            /* check that digitalSignature KeyUsage is present */
            X509_check_ca(reqctx->received_cert);
            if ((usage = X509_get_ext_d2i(reqctx->received_cert,
                                          NID_key_usage, NULL, NULL))) {

                if (!ku_reject(reqctx->received_cert,
                               X509v3_KU_DIGITAL_SIGNATURE)) {
                    TRACE_PKINIT_EKU(context);
                    *valid_eku = 1;
                } else
                    TRACE_PKINIT_EKU_NO_KU(context);
            }
            ASN1_BIT_STRING_free(usage);
        }
    }
    retval = 0;
cleanup:
    pkiDebug("%s: returning retval %d, valid_eku %d\n",
             __FUNCTION__, retval, *valid_eku);
    return retval;
}

krb5_error_code
pkinit_octetstring2key(krb5_context context,
                       krb5_enctype etype,
                       unsigned char *key,
                       unsigned int dh_key_len,
                       krb5_keyblock *key_block)
{
    krb5_error_code retval;
    unsigned char *buf = NULL;
    unsigned char md[SHA_DIGEST_LENGTH];
    unsigned char counter;
    size_t keybytes, keylength, offset;
    krb5_data random_data;
    EVP_MD_CTX *sha1_ctx = NULL;

    buf = k5alloc(dh_key_len, &retval);
    if (buf == NULL)
        goto cleanup;

    sha1_ctx = EVP_MD_CTX_new();
    if (sha1_ctx == NULL) {
        retval = KRB5_CRYPTO_INTERNAL;
        goto cleanup;
    }

    counter = 0;
    offset = 0;
    do {
        if (!EVP_DigestInit(sha1_ctx, EVP_sha1()) ||
            !EVP_DigestUpdate(sha1_ctx, &counter, 1) ||
            !EVP_DigestUpdate(sha1_ctx, key, dh_key_len) ||
            !EVP_DigestFinal(sha1_ctx, md, NULL)) {
            retval = KRB5_CRYPTO_INTERNAL;
            goto cleanup;
        }

        if (dh_key_len - offset < sizeof(md))
            memcpy(buf + offset, md, dh_key_len - offset);
        else
            memcpy(buf + offset, md, sizeof(md));

        offset += sizeof(md);
        counter++;
    } while (offset < dh_key_len);

    key_block->magic = 0;
    key_block->enctype = etype;

    retval = krb5_c_keylengths(context, etype, &keybytes, &keylength);
    if (retval)
        goto cleanup;

    key_block->length = keylength;
    key_block->contents = k5alloc(keylength, &retval);
    if (key_block->contents == NULL)
        goto cleanup;

    random_data.length = keybytes;
    random_data.data = (char *)buf;

    retval = krb5_c_random_to_key(context, etype, &random_data, key_block);

cleanup:
    EVP_MD_CTX_free(sha1_ctx);
    free(buf);
    /* If this is an error return, free the allocated keyblock, if any */
    if (retval) {
        krb5_free_keyblock_contents(context, key_block);
    }

    return retval;
}


/* Return the OpenSSL descriptor for the given RFC 5652 OID specified in RFC
 * 8636.  RFC 8636 defines a SHA384 variant, but we don't use it. */
static const EVP_MD *
algid_to_md(const krb5_data *alg_id)
{
    if (data_eq(*alg_id, sha1_id))
        return EVP_sha1();
    if (data_eq(*alg_id, sha256_id))
        return EVP_sha256();
    if (data_eq(*alg_id, sha512_id))
        return EVP_sha512();
    return NULL;
}

#if OPENSSL_VERSION_NUMBER >= 0x30000000L

#define sskdf openssl_sskdf
static krb5_error_code
openssl_sskdf(krb5_context context, const EVP_MD *md, krb5_data *key,
              krb5_data *info, size_t len, krb5_data *out)
{
    krb5_error_code ret;
    EVP_KDF *kdf = NULL;
    EVP_KDF_CTX *kctx = NULL;
    OSSL_PARAM params[4], *p = params;

    ret = alloc_data(out, len);
    if (ret)
        goto cleanup;

    kdf = EVP_KDF_fetch(NULL, "SSKDF", NULL);
    if (kdf == NULL) {
        ret = oerr(context, KRB5_CRYPTO_INTERNAL, _("Failed to fetch SSKDF"));
        goto cleanup;
    }

    kctx = EVP_KDF_CTX_new(kdf);
    if (!kctx) {
        ret = oerr(context, KRB5_CRYPTO_INTERNAL,
                   _("Failed to instantiate SSKDF"));
        goto cleanup;
    }

    *p++ = OSSL_PARAM_construct_utf8_string(OSSL_KDF_PARAM_DIGEST,
                                            (char *)EVP_MD_get0_name(md), 0);
    *p++ = OSSL_PARAM_construct_octet_string(OSSL_KDF_PARAM_KEY,
                                             key->data, key->length);
    *p++ = OSSL_PARAM_construct_octet_string(OSSL_KDF_PARAM_INFO,
                                             info->data, info->length);
    *p = OSSL_PARAM_construct_end();
    if (EVP_KDF_derive(kctx, (uint8_t *)out->data, len, params) <= 0) {
        ret = oerr(context, KRB5_CRYPTO_INTERNAL,
                   _("Failed to derive key using SSKDF"));
        goto cleanup;
    }

    ret = 0;

cleanup:
    EVP_KDF_free(kdf);
    EVP_KDF_CTX_free(kctx);
    return ret;
}

#else /* OPENSSL_VERSION_NUMBER < 0x30000000L */

#define sskdf builtin_sskdf
static krb5_error_code
builtin_sskdf(krb5_context context, const EVP_MD *md, krb5_data *key,
              krb5_data *info, size_t len, krb5_data *out)
{
    krb5_error_code ret;
    uint32_t counter = 1, reps;
    uint8_t be_counter[4], *outptr;
    EVP_MD_CTX *ctx = NULL;
    unsigned int s, hash_len;

    hash_len = EVP_MD_size(md);

    /* 1.  reps = keydatalen (K) / hash length (H) rounded up. */
    reps = (len + hash_len - 1) / hash_len;

    /* Allocate enough space in the random data buffer to hash directly into
     * it, even if the last hash will make it bigger than the key length. */
    ret = alloc_data(out, reps * hash_len);
    if (ret)
        goto cleanup;
    out->length = len;

    /*
     * 2.  Initialize a 32-bit, big-endian bit string counter as 1.
     * 3.  For i = 1 to reps by 1, do the following:
     *     -   Compute Hashi = H(counter || Z || OtherInfo).
     *     -   Increment counter (modulo 2^32)
     * 4.  Set key = Hash1 || Hash2 || ... so that length of key is K
     *     bytes.
     */
    outptr = (uint8_t *)out->data;
    for (counter = 1; counter <= reps; counter++) {
        store_32_be(counter, be_counter);

        ctx = EVP_MD_CTX_new();
        if (ctx == NULL) {
            ret = KRB5_CRYPTO_INTERNAL;
            goto cleanup;
        }

        /* -   Compute Hashi = H(counter || Z || OtherInfo). */
        if (!EVP_DigestInit(ctx, md) ||
            !EVP_DigestUpdate(ctx, be_counter, 4) ||
            !EVP_DigestUpdate(ctx, key->data, key->length) ||
            !EVP_DigestUpdate(ctx, info->data, info->length) ||
            !EVP_DigestFinal(ctx, outptr, &s)) {
            ret = oerr(context, KRB5_CRYPTO_INTERNAL,
                       _("Failed to compute digest"));
            goto cleanup;
        }

        assert(s == hash_len);
        outptr += s;

        EVP_MD_CTX_free(ctx);
        ctx = NULL;
    }

cleanup:
    EVP_MD_CTX_free(ctx);
    return ret;
}

#endif /* OPENSSL_VERSION_NUMBER < 0x30000000L */

/* id-pkinit-kdf family, as specified by RFC 8636. */
krb5_error_code
pkinit_alg_agility_kdf(krb5_context context, krb5_data *secret,
                       krb5_data *alg_oid, krb5_const_principal party_u_info,
                       krb5_const_principal party_v_info,
                       krb5_enctype enctype, krb5_data *as_req,
                       krb5_data *pk_as_rep, krb5_keyblock *key_block)
{
    krb5_error_code ret;
    size_t rand_len = 0, key_len = 0;
    const EVP_MD *md;
    krb5_sp80056a_other_info other_info_fields;
    krb5_pkinit_supp_pub_info supp_pub_info_fields;
    krb5_data *other_info = NULL, *supp_pub_info = NULL;
    krb5_data random_data = empty_data();
    krb5_algorithm_identifier alg_id;
    char *hash_name = NULL;

    ret = krb5_c_keylengths(context, enctype, &rand_len, &key_len);
    if (ret)
        goto cleanup;

    /* Allocate and initialize the key block. */
    key_block->magic = 0;
    key_block->enctype = enctype;
    key_block->length = key_len;
    key_block->contents = k5calloc(key_block->length, 1, &ret);
    if (key_block->contents == NULL)
        goto cleanup;

    /* If this is anonymous pkinit, use the anonymous principle for
     * party_u_info. */
    if (party_u_info &&
        krb5_principal_compare_any_realm(context, party_u_info,
                                         krb5_anonymous_principal())) {
        party_u_info = (krb5_principal)krb5_anonymous_principal();
    }

    md = algid_to_md(alg_oid);
    if (md == NULL) {
        krb5_set_error_message(context, KRB5_ERR_BAD_S2K_PARAMS,
                               "Bad algorithm ID passed to PK-INIT KDF.");
        return KRB5_ERR_BAD_S2K_PARAMS;
    }

    /* Encode the ASN.1 octet string for "SuppPubInfo". */
    supp_pub_info_fields.enctype = enctype;
    supp_pub_info_fields.as_req = *as_req;
    supp_pub_info_fields.pk_as_rep = *pk_as_rep;
    ret = encode_krb5_pkinit_supp_pub_info(&supp_pub_info_fields,
                                           &supp_pub_info);
    if (ret)
        goto cleanup;

    /* Now encode the ASN.1 octet string for "OtherInfo". */
    memset(&alg_id, 0, sizeof(alg_id));
    alg_id.algorithm = *alg_oid;
    other_info_fields.algorithm_identifier = alg_id;
    other_info_fields.party_u_info = (krb5_principal)party_u_info;
    other_info_fields.party_v_info = (krb5_principal)party_v_info;
    other_info_fields.supp_pub_info = *supp_pub_info;
    ret = encode_krb5_sp80056a_other_info(&other_info_fields, &other_info);
    if (ret)
        goto cleanup;

    ret = sskdf(context, md, secret, other_info, rand_len, &random_data);
    if (ret)
        goto cleanup;

    ret = krb5_c_random_to_key(context, enctype, &random_data, key_block);

cleanup:
    if (ret)
        krb5_free_keyblock_contents(context, key_block);
    free(hash_name);
    zapfree(random_data.data, random_data.length);
    krb5_free_data(context, other_info);
    krb5_free_data(context, supp_pub_info);
    return ret;
}

krb5_error_code
client_create_dh(krb5_context context,
                 pkinit_plg_crypto_context plg_cryptoctx,
                 pkinit_req_crypto_context cryptoctx,
                 pkinit_identity_crypto_context id_cryptoctx,
                 int dh_size, krb5_data *spki_out)
{
    krb5_error_code retval = KRB5KDC_ERR_PREAUTH_FAILED;
    EVP_PKEY *params = NULL, *pkey = NULL;

    *spki_out = empty_data();

    if (cryptoctx->received_params != NULL)
        params = cryptoctx->received_params;
    else if (dh_size == 1024)
        params = plg_cryptoctx->dh_1024;
    else if (dh_size == 2048)
        params = plg_cryptoctx->dh_2048;
    else if (dh_size == 4096)
        params = plg_cryptoctx->dh_4096;
    else
        goto cleanup;

    pkey = generate_dh_pkey(params);
    if (pkey == NULL)
        goto cleanup;

    retval = encode_spki(pkey, spki_out);
    if (retval)
        goto cleanup;

    EVP_PKEY_free(cryptoctx->client_pkey);
    cryptoctx->client_pkey = pkey;
    pkey = NULL;

cleanup:
    EVP_PKEY_free(pkey);
    return retval;
}

krb5_error_code
client_process_dh(krb5_context context,
                  pkinit_plg_crypto_context plg_cryptoctx,
                  pkinit_req_crypto_context cryptoctx,
                  pkinit_identity_crypto_context id_cryptoctx,
                  unsigned char *subjectPublicKey_data,
                  unsigned int subjectPublicKey_length,
                  unsigned char **client_key_out,
                  unsigned int *client_key_len_out)
{
    krb5_error_code retval = KRB5KDC_ERR_PREAUTH_FAILED;
    EVP_PKEY *server_pkey = NULL;
    uint8_t *client_key = NULL;
    unsigned int client_key_len;

    *client_key_out = NULL;
    *client_key_len_out = 0;

    server_pkey = compose_dh_pkey(cryptoctx->client_pkey,
                                  subjectPublicKey_data,
                                  subjectPublicKey_length);
    if (server_pkey == NULL)
        goto cleanup;

    if (!dh_result(cryptoctx->client_pkey, server_pkey,
                   &client_key, &client_key_len))
        goto cleanup;

#ifdef DEBUG_DH
    print_pubkey(server_pub_key, "server's pub_key=");
    pkiDebug("client computed key (%d)= ", client_key_len);
    print_buffer(client_key, client_key_len);
#endif

    *client_key_out = client_key;
    *client_key_len_out = client_key_len;
    client_key = NULL;

    retval = 0;

cleanup:
    EVP_PKEY_free(server_pkey);
    free(client_key);
    return retval;
}

/* Return 1 if dh is a permitted well-known group, otherwise return 0. */
static int
check_dh_wellknown(pkinit_plg_crypto_context cryptoctx, EVP_PKEY *pkey,
                   int nbits)
{
    if (nbits == 1024)
        return EVP_PKEY_parameters_eq(cryptoctx->dh_1024, pkey) == 1;
    else if (nbits == 2048)
        return EVP_PKEY_parameters_eq(cryptoctx->dh_2048, pkey) == 1;
    else if (nbits == 4096)
        return EVP_PKEY_parameters_eq(cryptoctx->dh_4096, pkey) == 1;
    return 0;
}

krb5_error_code
server_check_dh(krb5_context context,
                pkinit_plg_crypto_context cryptoctx,
                pkinit_req_crypto_context req_cryptoctx,
                pkinit_identity_crypto_context id_cryptoctx,
                const krb5_data *client_spki,
                int minbits)
{
    EVP_PKEY *client_pkey = NULL;
    int dh_prime_bits;
    krb5_error_code retval = KRB5KDC_ERR_DH_KEY_PARAMETERS_NOT_ACCEPTED;

    client_pkey = decode_spki(client_spki);
    if (client_pkey == NULL) {
        pkiDebug("failed to decode dhparams\n");
        goto cleanup;
    }

    /* KDC SHOULD check to see if the key parameters satisfy its policy */
    dh_prime_bits = EVP_PKEY_get_bits(client_pkey);
    if (minbits && dh_prime_bits < minbits) {
        pkiDebug("client sent dh params with %d bits, we require %d\n",
                 dh_prime_bits, minbits);
        goto cleanup;
    }

    if (check_dh_wellknown(cryptoctx, client_pkey, dh_prime_bits))
        retval = 0;

cleanup:
    if (retval == 0)
        req_cryptoctx->client_pkey = client_pkey;
    else
        EVP_PKEY_free(client_pkey);

    return retval;
}

/* kdc's dh function */
krb5_error_code
server_process_dh(krb5_context context,
                  pkinit_plg_crypto_context plg_cryptoctx,
                  pkinit_req_crypto_context cryptoctx,
                  pkinit_identity_crypto_context id_cryptoctx,
                  unsigned char **dh_pubkey_out,
                  unsigned int *dh_pubkey_len_out,
                  unsigned char **server_key_out,
                  unsigned int *server_key_len_out)
{
    krb5_error_code retval = ENOMEM;
    EVP_PKEY *server_pkey = NULL;
    unsigned char *dh_pubkey = NULL, *server_key = NULL;
    unsigned int dh_pubkey_len = 0, server_key_len = 0;

    *dh_pubkey_out = *server_key_out = NULL;
    *dh_pubkey_len_out = *server_key_len_out = 0;

    /* Generate a server DH key with the same parameters as the client key. */
    server_pkey = generate_dh_pkey(cryptoctx->client_pkey);
    if (server_pkey == NULL)
        goto cleanup;

    if (!dh_result(server_pkey, cryptoctx->client_pkey, &server_key,
                   &server_key_len))
        goto cleanup;

    if (!dh_pubkey_der(server_pkey, &dh_pubkey, &dh_pubkey_len))
        goto cleanup;

    *dh_pubkey_out = dh_pubkey;
    *dh_pubkey_len_out = dh_pubkey_len;
    *server_key_out = server_key;
    *server_key_len_out = server_key_len;
    dh_pubkey = server_key = NULL;

    retval = 0;

cleanup:
    EVP_PKEY_free(server_pkey);
    free(dh_pubkey);
    free(server_key);

    return retval;
}

int
pkinit_openssl_init()
{
    /* Initialize OpenSSL. */
    ERR_load_crypto_strings();
    OpenSSL_add_all_algorithms();
    return 0;
}

static krb5_error_code
pkinit_create_sequence_of_principal_identifiers(
    krb5_context context,
    pkinit_plg_crypto_context plg_cryptoctx,
    pkinit_req_crypto_context req_cryptoctx,
    pkinit_identity_crypto_context id_cryptoctx,
    int type,
    krb5_pa_data ***e_data_out)
{
    krb5_error_code retval = KRB5KRB_ERR_GENERIC;
    krb5_external_principal_identifier **krb5_trusted_certifiers = NULL;
    krb5_data *td_certifiers = NULL;
    krb5_pa_data **pa_data = NULL;

    switch(type) {
    case TD_TRUSTED_CERTIFIERS:
        retval = create_krb5_trustedCertifiers(context, plg_cryptoctx,
                                               req_cryptoctx, id_cryptoctx, &krb5_trusted_certifiers);
        if (retval) {
            pkiDebug("create_krb5_trustedCertifiers failed\n");
            goto cleanup;
        }
        break;
    case TD_INVALID_CERTIFICATES:
        retval = create_krb5_invalidCertificates(context, plg_cryptoctx,
                                                 req_cryptoctx, id_cryptoctx, &krb5_trusted_certifiers);
        if (retval) {
            pkiDebug("create_krb5_invalidCertificates failed\n");
            goto cleanup;
        }
        break;
    default:
        retval = -1;
        goto cleanup;
    }

    retval = k5int_encode_krb5_td_trusted_certifiers((krb5_external_principal_identifier *const *)krb5_trusted_certifiers, &td_certifiers);
    if (retval) {
        pkiDebug("encode_krb5_td_trusted_certifiers failed\n");
        goto cleanup;
    }
#ifdef DEBUG_ASN1
    print_buffer_bin((unsigned char *)td_certifiers->data,
                     td_certifiers->length, "/tmp/kdc_td_certifiers");
#endif
    pa_data = malloc(2 * sizeof(krb5_pa_data *));
    if (pa_data == NULL) {
        retval = ENOMEM;
        goto cleanup;
    }
    pa_data[1] = NULL;
    pa_data[0] = malloc(sizeof(krb5_pa_data));
    if (pa_data[0] == NULL) {
        free(pa_data);
        retval = ENOMEM;
        goto cleanup;
    }
    pa_data[0]->pa_type = type;
    pa_data[0]->length = td_certifiers->length;
    pa_data[0]->contents = (krb5_octet *)td_certifiers->data;
    *e_data_out = pa_data;
    retval = 0;

cleanup:
    if (krb5_trusted_certifiers != NULL)
        free_krb5_external_principal_identifier(&krb5_trusted_certifiers);
    free(td_certifiers);
    return retval;
}

krb5_error_code
pkinit_create_td_trusted_certifiers(krb5_context context,
                                    pkinit_plg_crypto_context plg_cryptoctx,
                                    pkinit_req_crypto_context req_cryptoctx,
                                    pkinit_identity_crypto_context id_cryptoctx,
                                    krb5_pa_data ***e_data_out)
{
    krb5_error_code retval = KRB5KRB_ERR_GENERIC;

    retval = pkinit_create_sequence_of_principal_identifiers(context,
                                                             plg_cryptoctx, req_cryptoctx, id_cryptoctx,
                                                             TD_TRUSTED_CERTIFIERS, e_data_out);

    return retval;
}

krb5_error_code
pkinit_create_td_invalid_certificate(
    krb5_context context,
    pkinit_plg_crypto_context plg_cryptoctx,
    pkinit_req_crypto_context req_cryptoctx,
    pkinit_identity_crypto_context id_cryptoctx,
    krb5_pa_data ***e_data_out)
{
    krb5_error_code retval = KRB5KRB_ERR_GENERIC;

    retval = pkinit_create_sequence_of_principal_identifiers(context,
                                                             plg_cryptoctx, req_cryptoctx, id_cryptoctx,
                                                             TD_INVALID_CERTIFICATES, e_data_out);

    return retval;
}

krb5_error_code
pkinit_create_td_dh_parameters(krb5_context context,
                               pkinit_plg_crypto_context plg_cryptoctx,
                               pkinit_req_crypto_context req_cryptoctx,
                               pkinit_identity_crypto_context id_cryptoctx,
                               pkinit_plg_opts *opts,
                               krb5_pa_data ***e_data_out)
{
    krb5_error_code ret;
    int i;
    krb5_pa_data **pa_data = NULL;
    krb5_data *der_alglist = NULL;
    krb5_algorithm_identifier alg_1024 = { dh_oid, oakley_1024 };
    krb5_algorithm_identifier alg_2048 = { dh_oid, oakley_2048 };
    krb5_algorithm_identifier alg_4096 = { dh_oid, oakley_4096 };
    krb5_algorithm_identifier *alglist[4];

    if (opts->dh_min_bits > 4096) {
        ret = KRB5KRB_ERR_GENERIC;
        goto cleanup;
    }

    i = 0;
    if (opts->dh_min_bits <= 2048)
        alglist[i++] = &alg_2048;
    alglist[i++] = &alg_4096;
    if (opts->dh_min_bits <= 1024)
        alglist[i++] = &alg_1024;
    alglist[i] = NULL;

    ret = k5int_encode_krb5_td_dh_parameters(alglist, &der_alglist);
    if (ret)
        goto cleanup;

    pa_data = k5calloc(2, sizeof(*pa_data), &ret);
    if (pa_data == NULL)
        goto cleanup;
    pa_data[1] = NULL;
    pa_data[0] = k5alloc(sizeof(*pa_data[0]), &ret);
    if (pa_data[0] == NULL) {
        free(pa_data);
        goto cleanup;
    }
    pa_data[0]->pa_type = TD_DH_PARAMETERS;
    pa_data[0]->length = der_alglist->length;
    pa_data[0]->contents = (krb5_octet *)der_alglist->data;
    der_alglist->data = NULL;
    *e_data_out = pa_data;

cleanup:
    krb5_free_data(context, der_alglist);
    return ret;
}

krb5_error_code
pkinit_check_kdc_pkid(krb5_context context,
                      pkinit_plg_crypto_context plg_cryptoctx,
                      pkinit_req_crypto_context req_cryptoctx,
                      pkinit_identity_crypto_context id_cryptoctx,
                      unsigned char *pdid_buf,
                      unsigned int pkid_len,
                      int *valid_kdcPkId)
{
    PKCS7_ISSUER_AND_SERIAL *is = NULL;
    const unsigned char *p = pdid_buf;
    int status = 1;
    X509 *kdc_cert = sk_X509_value(id_cryptoctx->my_certs, id_cryptoctx->cert_index);

    *valid_kdcPkId = 0;
    pkiDebug("found kdcPkId in AS REQ\n");
    is = d2i_PKCS7_ISSUER_AND_SERIAL(NULL, &p, (int)pkid_len);
    if (is == NULL)
        return KRB5KDC_ERR_PREAUTH_FAILED;

    status = X509_NAME_cmp(X509_get_issuer_name(kdc_cert), is->issuer);
    if (!status) {
        status = ASN1_INTEGER_cmp(X509_get_serialNumber(kdc_cert), is->serial);
        if (!status)
            *valid_kdcPkId = 1;
    }

    X509_NAME_free(is->issuer);
    ASN1_INTEGER_free(is->serial);
    free(is);

    return 0;
}

krb5_error_code
pkinit_process_td_dh_params(krb5_context context,
                            pkinit_plg_crypto_context cryptoctx,
                            pkinit_req_crypto_context req_cryptoctx,
                            pkinit_identity_crypto_context id_cryptoctx,
                            krb5_algorithm_identifier **algId,
                            int *new_dh_size)
{
    krb5_error_code retval = KRB5KDC_ERR_DH_KEY_PARAMETERS_NOT_ACCEPTED;
    EVP_PKEY *params = NULL;
    int i, dh_prime_bits, old_dh_size;

    pkiDebug("dh parameters\n");

    EVP_PKEY_free(req_cryptoctx->received_params);
    req_cryptoctx->received_params = NULL;

    old_dh_size = *new_dh_size;

    for (i = 0; algId[i] != NULL; i++) {
        /* Free any parameters from the previous iteration. */
        EVP_PKEY_free(params);
        params = NULL;

        /* Skip any parameters for algorithms other than DH. */
        if (algId[i]->algorithm.length != dh_oid.length ||
            memcmp(algId[i]->algorithm.data, dh_oid.data, dh_oid.length))
            continue;

        params = decode_dh_params(&algId[i]->parameters);
        if (params == NULL)
            continue;
        dh_prime_bits = EVP_PKEY_get_bits(params);
        /* Skip any parameters shorter than the previous size. */
        if (dh_prime_bits < old_dh_size)
            continue;
        pkiDebug("client sent %d DH bits server prefers %d DH bits\n",
                 *new_dh_size, dh_prime_bits);

        /* If this is one of our well-known groups, just save the new size; we
         * will use our own copy of the parameters. */
        if (check_dh_wellknown(cryptoctx, params, dh_prime_bits)) {
            *new_dh_size = dh_prime_bits;
            retval = 0;
            goto cleanup;
        }

        /* If the parameters aren't well-known but check out, save them. */
        if (params_valid(params)) {
            req_cryptoctx->received_params = params;
            params = NULL;
            retval = 0;
            goto cleanup;
        }
    }

cleanup:
    EVP_PKEY_free(params);
    return retval;
}

static int
openssl_callback(int ok, X509_STORE_CTX * ctx)
{
#ifdef DEBUG
    if (!ok) {
        X509 *cert = X509_STORE_CTX_get_current_cert(ctx);
        int err = X509_STORE_CTX_get_error(ctx);
        const char *errmsg = X509_verify_cert_error_string(err);
        char buf[DN_BUF_LEN];

        X509_NAME_oneline(X509_get_subject_name(cert), buf, sizeof(buf));
        pkiDebug("cert = %s\n", buf);
        pkiDebug("callback function: %d (%s)\n", err, errmsg);
    }
#endif
    return ok;
}

static int
openssl_callback_ignore_crls(int ok, X509_STORE_CTX * ctx)
{
    if (ok)
        return ok;
    return X509_STORE_CTX_get_error(ctx) == X509_V_ERR_UNABLE_TO_GET_CRL;
}

static ASN1_OBJECT *
pkinit_pkcs7type2oid(pkinit_plg_crypto_context cryptoctx, int pkcs7_type)
{
    switch (pkcs7_type) {
    case CMS_SIGN_CLIENT:
        return cryptoctx->id_pkinit_authData;
    case CMS_SIGN_SERVER:
        return cryptoctx->id_pkinit_DHKeyData;
    case CMS_ENVEL_SERVER:
        return cryptoctx->id_pkinit_rkeyData;
    default:
        return NULL;
    }

}

static int
wrap_signeddata(unsigned char *data, unsigned int data_len,
                unsigned char **out, unsigned int *out_len)
{

    unsigned int orig_len = 0, oid_len = 0, tot_len = 0;
    ASN1_OBJECT *oid = NULL;
    unsigned char *p = NULL;

    /* Get length to wrap the original data with SEQUENCE tag */
    tot_len = orig_len = ASN1_object_size(1, (int)data_len, V_ASN1_SEQUENCE);

    /* Add the signedData OID and adjust lengths */
    oid = OBJ_nid2obj(NID_pkcs7_signed);
    oid_len = i2d_ASN1_OBJECT(oid, NULL);

    tot_len = ASN1_object_size(1, (int)(orig_len+oid_len), V_ASN1_SEQUENCE);

    p = *out = malloc(tot_len);
    if (p == NULL) return -1;

    ASN1_put_object(&p, 1, (int)(orig_len+oid_len),
                    V_ASN1_SEQUENCE, V_ASN1_UNIVERSAL);

    i2d_ASN1_OBJECT(oid, &p);

    ASN1_put_object(&p, 1, (int)data_len, 0, V_ASN1_CONTEXT_SPECIFIC);
    memcpy(p, data, data_len);

    *out_len = tot_len;

    return 0;
}

static int
prepare_enc_data(const uint8_t *indata, int indata_len, uint8_t **outdata,
                 int *outdata_len)
{
    int tag, class;
    long tlen, slen;
    const uint8_t *p = indata, *oldp;

    if (ASN1_get_object(&p, &slen, &tag, &class, indata_len) & 0x80)
        return EINVAL;
    if (tag != V_ASN1_SEQUENCE)
        return EINVAL;

    oldp = p;
    if (ASN1_get_object(&p, &tlen, &tag, &class, slen) & 0x80)
        return EINVAL;
    p += tlen;
    slen -= (p - oldp);

    if (ASN1_get_object(&p, &tlen, &tag, &class, slen) & 0x80)
        return EINVAL;

    *outdata = malloc(tlen);
    if (*outdata == NULL)
        return ENOMEM;
    memcpy(*outdata, p, tlen);
    *outdata_len = tlen;
    return 0;
}

#ifndef WITHOUT_PKCS11
static struct plugin_file_handle *
load_pkcs11_module(krb5_context context, const char *modname,
                   CK_FUNCTION_LIST_PTR_PTR p11p)
{
    struct plugin_file_handle *handle = NULL;
    CK_RV (*getflist)(CK_FUNCTION_LIST_PTR_PTR);
    struct errinfo einfo = EMPTY_ERRINFO;
    const char *errmsg = NULL;
    void (*sym)();
    long err;
    CK_RV rv;

    TRACE_PKINIT_PKCS11_OPEN(context, modname);
    err = krb5int_open_plugin(modname, &handle, &einfo);
    if (err) {
        errmsg = k5_get_error(&einfo, err);
        TRACE_PKINIT_PKCS11_OPEN_FAILED(context, errmsg);
        goto error;
    }

    err = krb5int_get_plugin_func(handle, "C_GetFunctionList", &sym, &einfo);
    if (err) {
        errmsg = k5_get_error(&einfo, err);
        TRACE_PKINIT_PKCS11_GETSYM_FAILED(context, errmsg);
        goto error;
    }

    getflist = (CK_RV (*)())sym;
    rv = (*getflist)(p11p);
    if (rv != CKR_OK) {
        TRACE_PKINIT_PKCS11_GETFLIST_FAILED(context, pkcs11err(rv));
        goto error;
    }

    return handle;

error:
    k5_free_error(&einfo, errmsg);
    k5_clear_error(&einfo);
    if (handle != NULL)
        krb5int_close_plugin(handle);
    return NULL;
}

static krb5_error_code
pkinit_login(krb5_context context,
             pkinit_identity_crypto_context id_cryptoctx,
             CK_TOKEN_INFO *tip, const char *password)
{
    krb5_data rdat;
    char *prompt;
    const char *warning;
    krb5_prompt kprompt;
    krb5_prompt_type prompt_type;
    int r = 0;

    if (tip->flags & CKF_PROTECTED_AUTHENTICATION_PATH) {
        rdat.data = NULL;
        rdat.length = 0;
    } else if (password != NULL) {
        rdat.data = strdup(password);
        rdat.length = strlen(password);
    } else if (id_cryptoctx->prompter == NULL) {
        r = KRB5_LIBOS_CANTREADPWD;
        rdat.data = NULL;
    } else {
        if (tip->flags & CKF_USER_PIN_LOCKED)
            warning = " (Warning: PIN locked)";
        else if (tip->flags & CKF_USER_PIN_FINAL_TRY)
            warning = " (Warning: PIN final try)";
        else if (tip->flags & CKF_USER_PIN_COUNT_LOW)
            warning = " (Warning: PIN count low)";
        else
            warning = "";
        if (asprintf(&prompt, "%.*s PIN%s", (int) sizeof (tip->label),
                     tip->label, warning) < 0)
            return ENOMEM;
        rdat.data = malloc(tip->ulMaxPinLen + 2);
        rdat.length = tip->ulMaxPinLen + 1;

        kprompt.prompt = prompt;
        kprompt.hidden = 1;
        kprompt.reply = &rdat;
        prompt_type = KRB5_PROMPT_TYPE_PREAUTH;

        /* PROMPTER_INVOCATION */
        k5int_set_prompt_types(context, &prompt_type);
        r = (*id_cryptoctx->prompter)(context, id_cryptoctx->prompter_data,
                                      NULL, NULL, 1, &kprompt);
        k5int_set_prompt_types(context, 0);
        free(prompt);
    }

    if (r == 0) {
        r = id_cryptoctx->p11->C_Login(id_cryptoctx->session, CKU_USER,
                                       (u_char *) rdat.data, rdat.length);

        if (r != CKR_OK) {
            TRACE_PKINIT_PKCS11_LOGIN_FAILED(context, pkcs11err(r));
            r = KRB5KDC_ERR_PREAUTH_FAILED;
        }
    }
    free(rdat.data);

    return r;
}

static krb5_error_code
pkinit_open_session(krb5_context context,
                    pkinit_identity_crypto_context cctx)
{
    CK_ULONG i, pret;
    unsigned char *cp;
    size_t label_len;
    CK_ULONG count = 0;
    CK_SLOT_ID_PTR slotlist = NULL;
    CK_TOKEN_INFO tinfo;
    char *p11name = NULL;
    const char *password;
    krb5_error_code ret;

    if (cctx->p11_module != NULL)
        return 0; /* session already open */

    /* Load module */
    cctx->p11_module = load_pkcs11_module(context, cctx->p11_module_name,
                                          &cctx->p11);
    if (cctx->p11_module == NULL)
        return KRB5KDC_ERR_PREAUTH_FAILED;

    /* Init */
    pret = cctx->p11->C_Initialize(NULL);
    if (pret != CKR_OK) {
        pkiDebug("C_Initialize: %s\n", pkcs11err(pret));
        return KRB5KDC_ERR_PREAUTH_FAILED;
    }

    /* Get the list of available slots */
    if (cctx->p11->C_GetSlotList(TRUE, NULL, &count) != CKR_OK)
        return KRB5KDC_ERR_PREAUTH_FAILED;
    if (count == 0) {
        TRACE_PKINIT_PKCS11_NO_TOKEN(context);
        return KRB5KDC_ERR_PREAUTH_FAILED;
    }
    slotlist = calloc(count, sizeof(CK_SLOT_ID));
    if (slotlist == NULL)
        return ENOMEM;
    if (cctx->p11->C_GetSlotList(TRUE, slotlist, &count) != CKR_OK) {
        ret = KRB5KDC_ERR_PREAUTH_FAILED;
        goto cleanup;
    }

    /* Look for the given token label, or if none given take the first one */
    for (i = 0; i < count; i++) {
        /* Skip slots that don't match the specified slotid, if given. */
        if (cctx->slotid != PK_NOSLOT && cctx->slotid != slotlist[i])
            continue;

        /* Open session */
        pret = cctx->p11->C_OpenSession(slotlist[i], CKF_SERIAL_SESSION,
                                        NULL, NULL, &cctx->session);
        if (pret != CKR_OK) {
            pkiDebug("C_OpenSession: %s\n", pkcs11err(pret));
            ret = KRB5KDC_ERR_PREAUTH_FAILED;
            goto cleanup;
        }

        /* Get token info */
        pret = cctx->p11->C_GetTokenInfo(slotlist[i], &tinfo);
        if (pret != CKR_OK) {
            pkiDebug("C_GetTokenInfo: %s\n", pkcs11err(pret));
            ret = KRB5KDC_ERR_PREAUTH_FAILED;
            goto cleanup;
        }

        /* tinfo.label is zero-filled but not necessarily zero-terminated.
         * Find the length, ignoring any trailing spaces. */
        for (cp = tinfo.label + sizeof(tinfo.label); cp > tinfo.label; cp--) {
            if (cp[-1] != '\0' && cp[-1] != ' ')
                break;
        }
        label_len = cp - tinfo.label;

        TRACE_PKINIT_PKCS11_SLOT(context, (int)slotlist[i], (int)label_len,
                                 tinfo.label);
        if (cctx->token_label == NULL ||
            (strlen(cctx->token_label) == label_len &&
             memcmp(cctx->token_label, tinfo.label, label_len) == 0))
            break;
        cctx->p11->C_CloseSession(cctx->session);
    }
    if (i >= count) {
        TRACE_PKINIT_PKCS11_NO_MATCH_TOKEN(context);
        ret = KRB5KDC_ERR_PREAUTH_FAILED;
        goto cleanup;
    }
    cctx->slotid = slotlist[i];
    pkiDebug("open_session: slotid %d (%lu of %d)\n", (int)cctx->slotid,
             i + 1, (int) count);

    /* Login if needed */
    if (tinfo.flags & CKF_LOGIN_REQUIRED) {
        if (cctx->p11_module_name != NULL) {
            if (cctx->slotid != PK_NOSLOT) {
                if (asprintf(&p11name,
                             "PKCS11:module_name=%s:slotid=%ld:token=%.*s",
                             cctx->p11_module_name, (long)cctx->slotid,
                             (int)label_len, tinfo.label) < 0)
                    p11name = NULL;
            } else {
                if (asprintf(&p11name,
                             "PKCS11:module_name=%s,token=%.*s",
                             cctx->p11_module_name,
                             (int)label_len, tinfo.label) < 0)
                    p11name = NULL;
            }
        }
        if (cctx->defer_id_prompt) {
            /* Supply the identity name to be passed to the responder. */
            pkinit_set_deferred_id(&cctx->deferred_ids,
                                   p11name, tinfo.flags, NULL);
            ret = 0;
            goto cleanup;
        }
        /* Look up a responder-supplied password for the token. */
        password = pkinit_find_deferred_id(cctx->deferred_ids, p11name);
        ret = pkinit_login(context, cctx, &tinfo, password);
        if (ret)
            goto cleanup;
    }

    ret = 0;
cleanup:
    free(slotlist);
    free(p11name);
    return ret;
}

/*
 * Look for a key that's:
 * 1. private
 * 2. capable of the specified operation (usually signing or decrypting)
 * 3. RSA (this may be wrong but it's all we can do for now)
 * 4. matches the id of the cert we chose
 *
 * You must call pkinit_get_certs before calling pkinit_find_private_key
 * (that's because we need the ID of the private key)
 *
 * pkcs11 says the id of the key doesn't have to match that of the cert, but
 * I can't figure out any other way to decide which key to use.
 *
 * We should only find one key that fits all the requirements.
 * If there are more than one, we just take the first one.
 */

krb5_error_code
pkinit_find_private_key(pkinit_identity_crypto_context id_cryptoctx,
                        CK_ATTRIBUTE_TYPE usage,
                        CK_OBJECT_HANDLE *objp)
{
    CK_OBJECT_CLASS cls;
    CK_ATTRIBUTE attrs[4];
    CK_ULONG count;
    CK_KEY_TYPE keytype;
    unsigned int nattrs = 0;
    int r;
#ifdef PKINIT_USE_KEY_USAGE
    CK_BBOOL true_false;
#endif

    cls = CKO_PRIVATE_KEY;
    attrs[nattrs].type = CKA_CLASS;
    attrs[nattrs].pValue = &cls;
    attrs[nattrs].ulValueLen = sizeof cls;
    nattrs++;

#ifdef PKINIT_USE_KEY_USAGE
    /*
     * Some cards get confused if you try to specify a key usage,
     * so don't, and hope for the best. This will fail if you have
     * several keys with the same id and different usages but I have
     * not seen this on real cards.
     */
    true_false = TRUE;
    attrs[nattrs].type = usage;
    attrs[nattrs].pValue = &true_false;
    attrs[nattrs].ulValueLen = sizeof true_false;
    nattrs++;
#endif

    keytype = CKK_RSA;
    attrs[nattrs].type = CKA_KEY_TYPE;
    attrs[nattrs].pValue = &keytype;
    attrs[nattrs].ulValueLen = sizeof keytype;
    nattrs++;

    attrs[nattrs].type = CKA_ID;
    attrs[nattrs].pValue = id_cryptoctx->cert_id;
    attrs[nattrs].ulValueLen = id_cryptoctx->cert_id_len;
    nattrs++;

    r = id_cryptoctx->p11->C_FindObjectsInit(id_cryptoctx->session, attrs, nattrs);
    if (r != CKR_OK) {
        pkiDebug("krb5_pkinit_sign_data: C_FindObjectsInit: %s\n",
                 pkcs11err(r));
        return KRB5KDC_ERR_PREAUTH_FAILED;
    }

    r = id_cryptoctx->p11->C_FindObjects(id_cryptoctx->session, objp, 1, &count);
    id_cryptoctx->p11->C_FindObjectsFinal(id_cryptoctx->session);
    pkiDebug("found %d private keys (%s)\n", (int)count, pkcs11err(r));
    if (r != CKR_OK || count < 1)
        return KRB5KDC_ERR_PREAUTH_FAILED;
    return 0;
}
#endif

static krb5_error_code
pkinit_decode_data_fs(krb5_context context,
                      pkinit_identity_crypto_context id_cryptoctx,
                      const uint8_t *data, unsigned int data_len,
                      uint8_t **decoded_data, unsigned int *decoded_data_len)
{
    X509 *cert = sk_X509_value(id_cryptoctx->my_certs,
                               id_cryptoctx->cert_index);
    EVP_PKEY *pkey = id_cryptoctx->my_key;
    EVP_PKEY_CTX *ctx = NULL;
    uint8_t *buf = NULL;
    size_t buf_len = 0;
    int ok;

    *decoded_data = NULL;
    *decoded_data_len = 0;

    if (cert != NULL && !X509_check_private_key(cert, pkey)) {
        pkiDebug("private key does not match certificate\n");
        return KRB5KDC_ERR_PREAUTH_FAILED;
    }

    ctx = EVP_PKEY_CTX_new(pkey, NULL);
    if (ctx == NULL)
        return KRB5KDC_ERR_PREAUTH_FAILED;

    ok = EVP_PKEY_decrypt_init(ctx);
    if (!ok)
        goto cleanup;

    /* Get the length of the eventual output. */
    ok = EVP_PKEY_decrypt(ctx, NULL, &buf_len, data, data_len);
    if (!ok) {
        pkiDebug("unable to decrypt received data\n");
        goto cleanup;
    }

    buf = malloc(buf_len);
    if (buf == NULL) {
        ok = 0;
        goto cleanup;
    }

    ok = EVP_PKEY_decrypt(ctx, buf, &buf_len, data, data_len);
    if (!ok) {
        pkiDebug("unable to decrypt received data\n");
        goto cleanup;
    }

    *decoded_data = buf;
    *decoded_data_len = buf_len;
    buf = NULL;
cleanup:
    zapfree(buf, buf_len);
    EVP_PKEY_CTX_free(ctx);
    return ok ? 0 : KRB5KDC_ERR_PREAUTH_FAILED;
}

#ifndef WITHOUT_PKCS11
/*
 * When using the ActivCard Linux pkcs11 library (v2.0.1), the decrypt function
 * fails.  By inserting an extra function call, which serves nothing but to
 * change the stack, we were able to work around the issue.  If the ActivCard
 * library is fixed in the future, this function can be inlined back into the
 * caller.
 */
static CK_RV
pkinit_C_Decrypt(pkinit_identity_crypto_context id_cryptoctx,
                 CK_BYTE_PTR pEncryptedData,
                 CK_ULONG  ulEncryptedDataLen,
                 CK_BYTE_PTR pData,
                 CK_ULONG_PTR pulDataLen)
{
    CK_RV rv = CKR_OK;

    rv = id_cryptoctx->p11->C_Decrypt(id_cryptoctx->session, pEncryptedData,
                                      ulEncryptedDataLen, pData, pulDataLen);
    if (rv == CKR_OK) {
        pkiDebug("pData %p *pulDataLen %d\n", (void *) pData,
                 (int) *pulDataLen);
    }
    return rv;
}

static krb5_error_code
pkinit_decode_data_pkcs11(krb5_context context,
                          pkinit_identity_crypto_context id_cryptoctx,
                          const uint8_t *data, unsigned int data_len,
                          uint8_t **decoded_data,
                          unsigned int *decoded_data_len)
{
    CK_OBJECT_HANDLE obj;
    CK_ULONG len;
    CK_MECHANISM mech;
    uint8_t *cp;
    int r;

    *decoded_data = NULL;
    *decoded_data_len = 0;

    if (pkinit_open_session(context, id_cryptoctx)) {
        pkiDebug("can't open pkcs11 session\n");
        return KRB5KDC_ERR_PREAUTH_FAILED;
    }

    pkinit_find_private_key(id_cryptoctx, CKA_DECRYPT, &obj);

    mech.mechanism = CKM_RSA_PKCS;
    mech.pParameter = NULL;
    mech.ulParameterLen = 0;

    if ((r = id_cryptoctx->p11->C_DecryptInit(id_cryptoctx->session, &mech,
                                              obj)) != CKR_OK) {
        pkiDebug("C_DecryptInit: 0x%x\n", (int) r);
        return KRB5KDC_ERR_PREAUTH_FAILED;
    }
    pkiDebug("data_len = %d\n", data_len);
    cp = malloc((size_t) data_len);
    if (cp == NULL)
        return ENOMEM;
    len = data_len;
    pkiDebug("session %p edata %p edata_len %d data %p datalen @%p %d\n",
             (void *) id_cryptoctx->session, (void *) data, (int) data_len,
             (void *) cp, (void *) &len, (int) len);
    r = pkinit_C_Decrypt(id_cryptoctx, (CK_BYTE_PTR) data, (CK_ULONG) data_len,
                         cp, &len);
    if (r != CKR_OK) {
        pkiDebug("C_Decrypt: %s\n", pkcs11err(r));
        if (r == CKR_BUFFER_TOO_SMALL)
            pkiDebug("decrypt %d needs %d\n", (int) data_len, (int) len);
        return KRB5KDC_ERR_PREAUTH_FAILED;
    }
    pkiDebug("decrypt %d -> %d\n", (int) data_len, (int) len);
    *decoded_data_len = len;
    *decoded_data = cp;

    return 0;
}
#endif

krb5_error_code
pkinit_decode_data(krb5_context context,
                   pkinit_identity_crypto_context id_cryptoctx,
                   const uint8_t *data, unsigned int data_len,
                   uint8_t **decoded_data, unsigned int *decoded_data_len)
{
    krb5_error_code retval = KRB5KDC_ERR_PREAUTH_FAILED;

    *decoded_data = NULL;
    *decoded_data_len = 0;

    if (id_cryptoctx->pkcs11_method != 1)
        retval = pkinit_decode_data_fs(context, id_cryptoctx, data, data_len,
                                       decoded_data, decoded_data_len);
#ifndef WITHOUT_PKCS11
    else
        retval = pkinit_decode_data_pkcs11(context, id_cryptoctx, data,
                                           data_len, decoded_data, decoded_data_len);
#endif

    return retval;
}

static krb5_error_code
pkinit_sign_data_fs(krb5_context context,
                    pkinit_identity_crypto_context id_cryptoctx,
                    unsigned char *data,
                    unsigned int data_len,
                    unsigned char **sig,
                    unsigned int *sig_len)
{
    if (create_signature(sig, sig_len, data, data_len,
                         id_cryptoctx->my_key) != 0) {
        pkiDebug("failed to create the signature\n");
        return KRB5KDC_ERR_PREAUTH_FAILED;
    }
    return 0;
}

#ifndef WITHOUT_PKCS11
static krb5_error_code
pkinit_sign_data_pkcs11(krb5_context context,
                        pkinit_identity_crypto_context id_cryptoctx,
                        unsigned char *data,
                        unsigned int data_len,
                        unsigned char **sig,
                        unsigned int *sig_len)
{
    CK_OBJECT_HANDLE obj;
    CK_ULONG len;
    CK_MECHANISM mech;
    unsigned char *cp;
    int r;

    if (pkinit_open_session(context, id_cryptoctx)) {
        pkiDebug("can't open pkcs11 session\n");
        return KRB5KDC_ERR_PREAUTH_FAILED;
    }

    pkinit_find_private_key(id_cryptoctx, CKA_SIGN, &obj);

    mech.mechanism = id_cryptoctx->mech;
    mech.pParameter = NULL;
    mech.ulParameterLen = 0;

    if ((r = id_cryptoctx->p11->C_SignInit(id_cryptoctx->session, &mech,
                                           obj)) != CKR_OK) {
        pkiDebug("C_SignInit: %s\n", pkcs11err(r));
        return KRB5KDC_ERR_PREAUTH_FAILED;
    }

    /*
     * Key len would give an upper bound on sig size, but there's no way to
     * get that. So guess, and if it's too small, re-malloc.
     */
    len = PK_SIGLEN_GUESS;
    cp = malloc((size_t) len);
    if (cp == NULL)
        return ENOMEM;

    r = id_cryptoctx->p11->C_Sign(id_cryptoctx->session, data,
                                  (CK_ULONG) data_len, cp, &len);
    if (r == CKR_BUFFER_TOO_SMALL || (r == CKR_OK && len >= PK_SIGLEN_GUESS)) {
        free(cp);
        pkiDebug("C_Sign realloc %d\n", (int) len);
        cp = malloc((size_t) len);
        r = id_cryptoctx->p11->C_Sign(id_cryptoctx->session, data,
                                      (CK_ULONG) data_len, cp, &len);
    }
    if (r != CKR_OK) {
        pkiDebug("C_Sign: %s\n", pkcs11err(r));
        return KRB5KDC_ERR_PREAUTH_FAILED;
    }
    pkiDebug("sign %d -> %d\n", (int) data_len, (int) len);
    *sig_len = len;
    *sig = cp;

    return 0;
}
#endif

krb5_error_code
pkinit_sign_data(krb5_context context,
                 pkinit_identity_crypto_context id_cryptoctx,
                 unsigned char *data,
                 unsigned int data_len,
                 unsigned char **sig,
                 unsigned int *sig_len)
{
    krb5_error_code retval = KRB5KDC_ERR_PREAUTH_FAILED;

    if (id_cryptoctx == NULL || id_cryptoctx->pkcs11_method != 1)
        retval = pkinit_sign_data_fs(context, id_cryptoctx, data, data_len,
                                     sig, sig_len);
#ifndef WITHOUT_PKCS11
    else
        retval = pkinit_sign_data_pkcs11(context, id_cryptoctx, data, data_len,
                                         sig, sig_len);
#endif

    return retval;
}


static krb5_error_code
create_signature(unsigned char **sig, unsigned int *sig_len,
                 unsigned char *data, unsigned int data_len, EVP_PKEY *pkey)
{
    krb5_error_code retval = ENOMEM;
    EVP_MD_CTX *ctx;

    if (pkey == NULL)
        return retval;

    ctx = EVP_MD_CTX_new();
    if (ctx == NULL)
        return ENOMEM;
    EVP_SignInit(ctx, EVP_sha256());
    EVP_SignUpdate(ctx, data, data_len);
    *sig_len = EVP_PKEY_size(pkey);
    if ((*sig = malloc(*sig_len)) == NULL)
        goto cleanup;
    EVP_SignFinal(ctx, *sig, sig_len, pkey);

    retval = 0;

cleanup:
    EVP_MD_CTX_free(ctx);

    return retval;
}

/*
 * Note:
 * This is not the routine the KDC uses to get its certificate.
 * This routine is intended to be called by the client
 * to obtain the KDC's certificate from some local storage
 * to be sent as a hint in its request to the KDC.
 */
krb5_error_code
pkinit_get_kdc_cert(krb5_context context,
                    pkinit_plg_crypto_context plg_cryptoctx,
                    pkinit_req_crypto_context req_cryptoctx,
                    pkinit_identity_crypto_context id_cryptoctx,
                    krb5_principal princ)
{
    krb5_error_code retval = KRB5KDC_ERR_PREAUTH_FAILED;

    req_cryptoctx->received_cert = NULL;
    retval = 0;
    return retval;
}

static char *
reassemble_pkcs12_name(const char *filename)
{
    char *ret;

    if (asprintf(&ret, "PKCS12:%s", filename) < 0)
        return NULL;
    return ret;
}

static krb5_error_code
pkinit_get_certs_pkcs12(krb5_context context,
                        pkinit_plg_crypto_context plg_cryptoctx,
                        pkinit_req_crypto_context req_cryptoctx,
                        pkinit_identity_opts *idopts,
                        pkinit_identity_crypto_context id_cryptoctx,
                        krb5_principal princ)
{
    krb5_error_code retval = KRB5KDC_ERR_PREAUTH_FAILED;
    char *prompt_string = NULL;
    X509 *x = NULL;
    PKCS12 *p12 = NULL;
    int ret;
    FILE *fp;
    EVP_PKEY *y = NULL;

    if (idopts->cert_filename == NULL) {
        pkiDebug("%s: failed to get user's cert location\n", __FUNCTION__);
        goto cleanup;
    }

    if (idopts->key_filename == NULL) {
        pkiDebug("%s: failed to get user's private key location\n", __FUNCTION__);
        goto cleanup;
    }

    fp = fopen(idopts->cert_filename, "rb");
    if (fp == NULL) {
        TRACE_PKINIT_PKCS_OPEN_FAIL(context, idopts->cert_filename, errno);
        goto cleanup;
    }
    set_cloexec_file(fp);

    p12 = d2i_PKCS12_fp(fp, NULL);
    fclose(fp);
    if (p12 == NULL) {
        TRACE_PKINIT_PKCS_DECODE_FAIL(context, idopts->cert_filename);
        goto cleanup;
    }
    /*
     * Try parsing with no pass phrase first.  If that fails,
     * prompt for the pass phrase and try again.
     */
    ret = PKCS12_parse(p12, NULL, &y, &x, NULL);
    if (ret == 0) {
        krb5_data rdat;
        krb5_prompt kprompt;
        krb5_prompt_type prompt_type;
        krb5_error_code r;
        char prompt_reply[128];
        char *prompt_prefix = _("Pass phrase for");
        char *p12name = reassemble_pkcs12_name(idopts->cert_filename);
        const char *tmp;

        TRACE_PKINIT_PKCS_PARSE_FAIL_FIRST(context);

        if (id_cryptoctx->defer_id_prompt) {
            /* Supply the identity name to be passed to the responder. */
            pkinit_set_deferred_id(&id_cryptoctx->deferred_ids, p12name, 0,
                                   NULL);
            free(p12name);
            retval = 0;
            goto cleanup;
        }
        /* Try to read a responder-supplied password. */
        tmp = pkinit_find_deferred_id(id_cryptoctx->deferred_ids, p12name);
        free(p12name);
        if (tmp != NULL) {
            /* Try using the responder-supplied password. */
            rdat.data = (char *)tmp;
            rdat.length = strlen(tmp);
        } else if (id_cryptoctx->prompter == NULL) {
            /* We can't use a prompter. */
            goto cleanup;
        } else {
            /* Ask using a prompter. */
            memset(prompt_reply, '\0', sizeof(prompt_reply));
            rdat.data = prompt_reply;
            rdat.length = sizeof(prompt_reply);

            if (asprintf(&prompt_string, "%s %s", prompt_prefix,
                         idopts->cert_filename) < 0) {
                prompt_string = NULL;
                goto cleanup;
            }
            kprompt.prompt = prompt_string;
            kprompt.hidden = 1;
            kprompt.reply = &rdat;
            prompt_type = KRB5_PROMPT_TYPE_PREAUTH;
            /* PROMPTER_INVOCATION */
            k5int_set_prompt_types(context, &prompt_type);
            r = (*id_cryptoctx->prompter)(context, id_cryptoctx->prompter_data,
                                          NULL, NULL, 1, &kprompt);
            k5int_set_prompt_types(context, 0);
            if (r) {
                TRACE_PKINIT_PKCS_PROMPT_FAIL(context);
                goto cleanup;
            }
        }

        ret = PKCS12_parse(p12, rdat.data, &y, &x, NULL);
        if (ret == 0) {
            TRACE_PKINIT_PKCS_PARSE_FAIL_SECOND(context);
            goto cleanup;
        }
    }
    id_cryptoctx->creds[0] = malloc(sizeof(struct _pkinit_cred_info));
    if (id_cryptoctx->creds[0] == NULL)
        goto cleanup;
    id_cryptoctx->creds[0]->name =
        reassemble_pkcs12_name(idopts->cert_filename);
    id_cryptoctx->creds[0]->cert = x;
#ifndef WITHOUT_PKCS11
    id_cryptoctx->creds[0]->cert_id = NULL;
    id_cryptoctx->creds[0]->cert_id_len = 0;
#endif
    id_cryptoctx->creds[0]->key = y;
    id_cryptoctx->creds[1] = NULL;

    retval = 0;

cleanup:
    free(prompt_string);
    if (p12)
        PKCS12_free(p12);
    if (retval) {
        if (x != NULL)
            X509_free(x);
        if (y != NULL)
            EVP_PKEY_free(y);
    }
    return retval;
}

static char *
reassemble_files_name(const char *certfile, const char *keyfile)
{
    char *ret;

    if (keyfile != NULL) {
        if (asprintf(&ret, "FILE:%s,%s", certfile, keyfile) < 0)
            return NULL;
    } else {
        if (asprintf(&ret, "FILE:%s", certfile) < 0)
            return NULL;
    }
    return ret;
}

static krb5_error_code
pkinit_load_fs_cert_and_key(krb5_context context,
                            pkinit_identity_crypto_context id_cryptoctx,
                            char *certname,
                            char *keyname,
                            int cindex)
{
    krb5_error_code retval;
    X509 *x = NULL;
    EVP_PKEY *y = NULL;
    char *fsname = NULL;
    const char *password;

    fsname = reassemble_files_name(certname, keyname);

    /* Try to read a responder-supplied password. */
    password = pkinit_find_deferred_id(id_cryptoctx->deferred_ids, fsname);

    /* Load the certificate. */
    retval = get_cert(certname, &x);
    if (retval) {
        retval = oerr(context, retval, _("Cannot read certificate file '%s'"),
                      certname);
    }
    if (retval || x == NULL)
        goto cleanup;
    /* Load the key. */
    retval = get_key(context, id_cryptoctx, keyname, fsname, &y, password);
    if (retval)
        retval = oerr(context, retval, _("Cannot read key file '%s'"), fsname);
    if (retval || y == NULL)
        goto cleanup;

    id_cryptoctx->creds[cindex] = malloc(sizeof(struct _pkinit_cred_info));
    if (id_cryptoctx->creds[cindex] == NULL) {
        retval = ENOMEM;
        goto cleanup;
    }
    id_cryptoctx->creds[cindex]->name = reassemble_files_name(certname,
                                                              keyname);
    id_cryptoctx->creds[cindex]->cert = x;
#ifndef WITHOUT_PKCS11
    id_cryptoctx->creds[cindex]->cert_id = NULL;
    id_cryptoctx->creds[cindex]->cert_id_len = 0;
#endif
    id_cryptoctx->creds[cindex]->key = y;
    id_cryptoctx->creds[cindex+1] = NULL;

    retval = 0;

cleanup:
    free(fsname);
    if (retval != 0 || y == NULL) {
        if (x != NULL)
            X509_free(x);
        if (y != NULL)
            EVP_PKEY_free(y);
    }
    return retval;
}

static krb5_error_code
pkinit_get_certs_fs(krb5_context context,
                    pkinit_plg_crypto_context plg_cryptoctx,
                    pkinit_req_crypto_context req_cryptoctx,
                    pkinit_identity_opts *idopts,
                    pkinit_identity_crypto_context id_cryptoctx,
                    krb5_principal princ)
{
    krb5_error_code retval = KRB5KDC_ERR_PREAUTH_FAILED;

    if (idopts->cert_filename == NULL) {
        pkiDebug("%s: failed to get user's cert location\n", __FUNCTION__);
        goto cleanup;
    }

    if (idopts->key_filename == NULL) {
        TRACE_PKINIT_NO_PRIVKEY(context);
        goto cleanup;
    }

    retval = pkinit_load_fs_cert_and_key(context, id_cryptoctx,
                                         idopts->cert_filename,
                                         idopts->key_filename, 0);
cleanup:
    return retval;
}

static krb5_error_code
pkinit_get_certs_dir(krb5_context context,
                     pkinit_plg_crypto_context plg_cryptoctx,
                     pkinit_req_crypto_context req_cryptoctx,
                     pkinit_identity_opts *idopts,
                     pkinit_identity_crypto_context id_cryptoctx,
                     krb5_principal princ)
{
    krb5_error_code retval = ENOMEM;
    DIR *d = NULL;
    struct dirent *dentry = NULL;
    char certname[1024];
    char keyname[1024];
    int i = 0, len;
    char *dirname, *suf;

    if (idopts->cert_filename == NULL) {
        TRACE_PKINIT_NO_CERT(context);
        return ENOENT;
    }

    dirname = idopts->cert_filename;
    d = opendir(dirname);
    if (d == NULL)
        return errno;

    /*
     * We'll assume that certs are named XXX.crt and the corresponding
     * key is named XXX.key
     */
    while ((i < MAX_CREDS_ALLOWED) &&  (dentry = readdir(d)) != NULL) {
        /* Ignore subdirectories and anything starting with a dot */
#ifdef DT_DIR
        if (dentry->d_type == DT_DIR)
            continue;
#endif
        if (dentry->d_name[0] == '.')
            continue;
        len = strlen(dentry->d_name);
        if (len < 5)
            continue;
        suf = dentry->d_name + (len - 4);
        if (strncmp(suf, ".crt", 4) != 0)
            continue;

        /* Checked length */
        if (strlen(dirname) + strlen(dentry->d_name) + 2 > sizeof(certname)) {
            pkiDebug("%s: Path too long -- directory '%s' and file '%s'\n",
                     __FUNCTION__, dirname, dentry->d_name);
            continue;
        }
        snprintf(certname, sizeof(certname), "%s/%s", dirname, dentry->d_name);
        snprintf(keyname, sizeof(keyname), "%s/%s", dirname, dentry->d_name);
        len = strlen(keyname);
        keyname[len - 3] = 'k';
        keyname[len - 2] = 'e';
        keyname[len - 1] = 'y';

        retval = pkinit_load_fs_cert_and_key(context, id_cryptoctx,
                                             certname, keyname, i);
        if (retval == 0) {
            TRACE_PKINIT_LOADED_CERT(context, dentry->d_name);
            i++;
        }
        else
            continue;
    }

    if (!id_cryptoctx->defer_id_prompt && i == 0) {
        TRACE_PKINIT_NO_CERT_AND_KEY(context, idopts->cert_filename);
        retval = ENOENT;
        goto cleanup;
    }

    retval = 0;

cleanup:
    if (d)
        closedir(d);

    return retval;
}

#ifndef WITHOUT_PKCS11
static char *
reassemble_pkcs11_name(pkinit_identity_opts *idopts)
{
    struct k5buf buf;
    int n = 0;

    k5_buf_init_dynamic(&buf);
    k5_buf_add(&buf, "PKCS11:");
    n = 0;
    if (idopts->p11_module_name != NULL) {
        k5_buf_add_fmt(&buf, "%smodule_name=%s", n++ ? ":" : "",
                       idopts->p11_module_name);
    }
    if (idopts->token_label != NULL) {
        k5_buf_add_fmt(&buf, "%stoken=%s", n++ ? ":" : "",
                       idopts->token_label);
    }
    if (idopts->cert_label != NULL) {
        k5_buf_add_fmt(&buf, "%scertlabel=%s", n++ ? ":" : "",
                       idopts->cert_label);
    }
    if (idopts->cert_id_string != NULL) {
        k5_buf_add_fmt(&buf, "%scertid=%s", n++ ? ":" : "",
                       idopts->cert_id_string);
    }
    if (idopts->slotid != PK_NOSLOT) {
        k5_buf_add_fmt(&buf, "%sslotid=%ld", n++ ? ":" : "",
                       (long)idopts->slotid);
    }
    return k5_buf_cstring(&buf);
}

static krb5_error_code
load_one_cert(CK_FUNCTION_LIST_PTR p11, CK_SESSION_HANDLE session,
              pkinit_identity_opts *idopts, pkinit_cred_info *cred_out)
{
    krb5_error_code ret;
    CK_ATTRIBUTE attrs[2];
    CK_BYTE_PTR cert = NULL, cert_id = NULL;
    CK_RV pret;
    const unsigned char *cp;
    CK_OBJECT_HANDLE obj;
    CK_ULONG count;
    X509 *x = NULL;
    pkinit_cred_info cred;

    *cred_out = NULL;

    /* Look for X.509 cert. */
    pret = p11->C_FindObjects(session, &obj, 1, &count);
    if (pret != CKR_OK || count <= 0)
        return 0;

    /* Get cert and id len. */
    attrs[0].type = CKA_VALUE;
    attrs[0].pValue = NULL;
    attrs[0].ulValueLen = 0;
    attrs[1].type = CKA_ID;
    attrs[1].pValue = NULL;
    attrs[1].ulValueLen = 0;
    pret = p11->C_GetAttributeValue(session, obj, attrs, 2);
    if (pret != CKR_OK && pret != CKR_BUFFER_TOO_SMALL) {
        pkiDebug("C_GetAttributeValue: %s\n", pkcs11err(pret));
        ret = KRB5KDC_ERR_PREAUTH_FAILED;
        goto cleanup;
    }

    /* Allocate buffers and read the cert and id. */
    cert = k5alloc(attrs[0].ulValueLen + 1, &ret);
    if (cert == NULL)
        goto cleanup;
    cert_id = k5alloc(attrs[1].ulValueLen + 1, &ret);
    if (cert_id == NULL)
        goto cleanup;
    attrs[0].type = CKA_VALUE;
    attrs[0].pValue = cert;
    attrs[1].type = CKA_ID;
    attrs[1].pValue = cert_id;
    pret = p11->C_GetAttributeValue(session, obj, attrs, 2);
    if (pret != CKR_OK) {
        pkiDebug("C_GetAttributeValue: %s\n", pkcs11err(pret));
        ret = KRB5KDC_ERR_PREAUTH_FAILED;
        goto cleanup;
    }

    pkiDebug("cert: size %d, id %d, idlen %d\n", (int)attrs[0].ulValueLen,
             (int)cert_id[0], (int)attrs[1].ulValueLen);

    cp = (unsigned char *)cert;
    x = d2i_X509(NULL, &cp, (int)attrs[0].ulValueLen);
    if (x == NULL) {
        ret = KRB5KDC_ERR_PREAUTH_FAILED;
        goto cleanup;
    }

    cred = k5alloc(sizeof(struct _pkinit_cred_info), &ret);
    if (cred == NULL)
        goto cleanup;

    cred->name = reassemble_pkcs11_name(idopts);
    cred->cert = x;
    cred->key = NULL;
    cred->cert_id = cert_id;
    cred->cert_id_len = attrs[1].ulValueLen;

    *cred_out = cred;
    cert_id = NULL;
    ret = 0;

cleanup:
    free(cert);
    free(cert_id);
    return ret;
}

static krb5_error_code
pkinit_get_certs_pkcs11(krb5_context context,
                        pkinit_plg_crypto_context plg_cryptoctx,
                        pkinit_req_crypto_context req_cryptoctx,
                        pkinit_identity_opts *idopts,
                        pkinit_identity_crypto_context id_cryptoctx,
                        krb5_principal princ)
{
    CK_OBJECT_CLASS cls;
    CK_ATTRIBUTE attrs[4];
    CK_CERTIFICATE_TYPE certtype;
    int i;
    unsigned int nattrs;
    krb5_error_code ret;
    CK_RV pret;

    /* Copy stuff from idopts -> id_cryptoctx */
    if (idopts->p11_module_name != NULL) {
        free(id_cryptoctx->p11_module_name);
        id_cryptoctx->p11_module_name = strdup(idopts->p11_module_name);
        if (id_cryptoctx->p11_module_name == NULL)
            return ENOMEM;
    }
    if (idopts->token_label != NULL) {
        id_cryptoctx->token_label = strdup(idopts->token_label);
        if (id_cryptoctx->token_label == NULL)
            return ENOMEM;
    }
    if (idopts->cert_label != NULL) {
        id_cryptoctx->cert_label = strdup(idopts->cert_label);
        if (id_cryptoctx->cert_label == NULL)
            return ENOMEM;
    }
    /* Convert the ascii cert_id string into a binary blob */
    if (idopts->cert_id_string != NULL) {
        ret = k5_hex_decode(idopts->cert_id_string, &id_cryptoctx->cert_id,
                            &id_cryptoctx->cert_id_len);
        if (ret) {
            pkiDebug("Failed to convert certid string [%s]\n",
                     idopts->cert_id_string);
            return ret;
        }
    }
    id_cryptoctx->slotid = idopts->slotid;
    id_cryptoctx->pkcs11_method = 1;

    ret = pkinit_open_session(context, id_cryptoctx);
    if (ret)
        return ret;
    if (id_cryptoctx->defer_id_prompt) {
        /*
         * We need to reset all of the PKCS#11 state, so that the next time we
         * poke at it, it'll be in as close to the state it was in after we
         * loaded it the first time as we can make it.
         */
        pkinit_fini_pkcs11(id_cryptoctx);
        pkinit_init_pkcs11(id_cryptoctx);
        return 0;
    }

    /*
     * We'd like to use CKM_SHA256_RSA_PKCS for signing if it's available, but
     * historically many cards seem to be confused about whether they are
     * capable of mechanisms or not. The safe thing seems to be to ignore the
     * mechanism list, always use CKM_RSA_PKCS and calculate the sha256 digest
     * ourselves.
     */
    id_cryptoctx->mech = CKM_RSA_PKCS;

    cls = CKO_CERTIFICATE;
    attrs[0].type = CKA_CLASS;
    attrs[0].pValue = &cls;
    attrs[0].ulValueLen = sizeof(cls);

    certtype = CKC_X_509;
    attrs[1].type = CKA_CERTIFICATE_TYPE;
    attrs[1].pValue = &certtype;
    attrs[1].ulValueLen = sizeof(certtype);

    nattrs = 2;

    /* If a cert id and/or label were given, use them too */
    if (id_cryptoctx->cert_id_len > 0) {
        attrs[nattrs].type = CKA_ID;
        attrs[nattrs].pValue = id_cryptoctx->cert_id;
        attrs[nattrs].ulValueLen = id_cryptoctx->cert_id_len;
        nattrs++;
    }
    if (id_cryptoctx->cert_label != NULL) {
        attrs[nattrs].type = CKA_LABEL;
        attrs[nattrs].pValue = id_cryptoctx->cert_label;
        attrs[nattrs].ulValueLen = strlen(id_cryptoctx->cert_label);
        nattrs++;
    }

    pret = id_cryptoctx->p11->C_FindObjectsInit(id_cryptoctx->session, attrs,
                                                nattrs);
    if (pret != CKR_OK) {
        pkiDebug("C_FindObjectsInit: %s\n", pkcs11err(pret));
        return KRB5KDC_ERR_PREAUTH_FAILED;
    }

    for (i = 0; i < MAX_CREDS_ALLOWED; i++) {
        ret = load_one_cert(id_cryptoctx->p11, id_cryptoctx->session, idopts,
                            &id_cryptoctx->creds[i]);
        if (ret)
            return ret;
        if (id_cryptoctx->creds[i] == NULL)
            break;
    }
    if (i == MAX_CREDS_ALLOWED)
        return KRB5KDC_ERR_PREAUTH_FAILED;

    id_cryptoctx->p11->C_FindObjectsFinal(id_cryptoctx->session);

    /* Check if we found no certs. */
    if (id_cryptoctx->creds[0] == NULL)
        return KRB5KDC_ERR_PREAUTH_FAILED;
    return 0;
}

#endif /* !WITHOUT_PKCS11 */


static void
free_cred_info(krb5_context context,
               pkinit_identity_crypto_context id_cryptoctx,
               struct _pkinit_cred_info *cred)
{
    if (cred != NULL) {
        if (cred->cert != NULL)
            X509_free(cred->cert);
        if (cred->key != NULL)
            EVP_PKEY_free(cred->key);
#ifndef WITHOUT_PKCS11
        free(cred->cert_id);
#endif
        free(cred->name);
        free(cred);
    }
}

krb5_error_code
crypto_free_cert_info(krb5_context context,
                      pkinit_plg_crypto_context plg_cryptoctx,
                      pkinit_req_crypto_context req_cryptoctx,
                      pkinit_identity_crypto_context id_cryptoctx)
{
    int i;

    if (id_cryptoctx == NULL)
        return EINVAL;

    for (i = 0; i < MAX_CREDS_ALLOWED; i++) {
        if (id_cryptoctx->creds[i] != NULL) {
            free_cred_info(context, id_cryptoctx, id_cryptoctx->creds[i]);
            id_cryptoctx->creds[i] = NULL;
        }
    }
    return 0;
}

krb5_error_code
crypto_load_certs(krb5_context context,
                  pkinit_plg_crypto_context plg_cryptoctx,
                  pkinit_req_crypto_context req_cryptoctx,
                  pkinit_identity_opts *idopts,
                  pkinit_identity_crypto_context id_cryptoctx,
                  krb5_principal princ,
                  krb5_boolean defer_id_prompts)
{
    krb5_error_code retval;

    id_cryptoctx->defer_id_prompt = defer_id_prompts;

    switch(idopts->idtype) {
    case IDTYPE_FILE:
        retval = pkinit_get_certs_fs(context, plg_cryptoctx,
                                     req_cryptoctx, idopts,
                                     id_cryptoctx, princ);
        break;
    case IDTYPE_DIR:
        retval = pkinit_get_certs_dir(context, plg_cryptoctx,
                                      req_cryptoctx, idopts,
                                      id_cryptoctx, princ);
        break;
#ifndef WITHOUT_PKCS11
    case IDTYPE_PKCS11:
        retval = pkinit_get_certs_pkcs11(context, plg_cryptoctx,
                                         req_cryptoctx, idopts,
                                         id_cryptoctx, princ);
        break;
#endif
    case IDTYPE_PKCS12:
        retval = pkinit_get_certs_pkcs12(context, plg_cryptoctx,
                                         req_cryptoctx, idopts,
                                         id_cryptoctx, princ);
        break;
    default:
        retval = EINVAL;
    }
    if (retval)
        goto cleanup;

cleanup:
    return retval;
}

/*
 * Get certificate Key Usage and Extended Key Usage
 */
static krb5_error_code
crypto_retrieve_X509_key_usage(krb5_context context,
                               pkinit_plg_crypto_context plgcctx,
                               pkinit_req_crypto_context reqcctx,
                               X509 *x,
                               unsigned int *ret_ku_bits,
                               unsigned int *ret_eku_bits)
{
    krb5_error_code retval = 0;
    int i;
    unsigned int eku_bits = 0, ku_bits = 0;
    ASN1_BIT_STRING *usage = NULL;

    if (ret_ku_bits == NULL && ret_eku_bits == NULL)
        return EINVAL;

    if (ret_eku_bits)
        *ret_eku_bits = 0;
    else {
        pkiDebug("%s: EKUs not requested, not checking\n", __FUNCTION__);
        goto check_kus;
    }

    /* Start with Extended Key usage */
    i = X509_get_ext_by_NID(x, NID_ext_key_usage, -1);
    if (i >= 0) {
        EXTENDED_KEY_USAGE *eku;

        eku = X509_get_ext_d2i(x, NID_ext_key_usage, NULL, NULL);
        if (eku) {
            for (i = 0; i < sk_ASN1_OBJECT_num(eku); i++) {
                ASN1_OBJECT *certoid;
                certoid = sk_ASN1_OBJECT_value(eku, i);
                if ((OBJ_cmp(certoid, plgcctx->id_pkinit_KPClientAuth)) == 0)
                    eku_bits |= PKINIT_EKU_PKINIT;
                else if ((OBJ_cmp(certoid, OBJ_nid2obj(NID_ms_smartcard_login))) == 0)
                    eku_bits |= PKINIT_EKU_MSSCLOGIN;
                else if ((OBJ_cmp(certoid, OBJ_nid2obj(NID_client_auth))) == 0)
                    eku_bits |= PKINIT_EKU_CLIENTAUTH;
                else if ((OBJ_cmp(certoid, OBJ_nid2obj(NID_email_protect))) == 0)
                    eku_bits |= PKINIT_EKU_EMAILPROTECTION;
            }
            EXTENDED_KEY_USAGE_free(eku);
        }
    }
    pkiDebug("%s: returning eku 0x%08x\n", __FUNCTION__, eku_bits);
    *ret_eku_bits = eku_bits;

check_kus:
    /* Now the Key Usage bits */
    if (ret_ku_bits)
        *ret_ku_bits = 0;
    else {
        pkiDebug("%s: KUs not requested, not checking\n", __FUNCTION__);
        goto out;
    }

    /* Make sure usage exists before checking bits */
    X509_check_ca(x);
    usage = X509_get_ext_d2i(x, NID_key_usage, NULL, NULL);
    if (usage) {
        if (!ku_reject(x, X509v3_KU_DIGITAL_SIGNATURE))
            ku_bits |= PKINIT_KU_DIGITALSIGNATURE;
        if (!ku_reject(x, X509v3_KU_KEY_ENCIPHERMENT))
            ku_bits |= PKINIT_KU_KEYENCIPHERMENT;
        ASN1_BIT_STRING_free(usage);
    }

    pkiDebug("%s: returning ku 0x%08x\n", __FUNCTION__, ku_bits);
    *ret_ku_bits = ku_bits;
    retval = 0;
out:
    return retval;
}

static krb5_error_code
rfc2253_name(X509_NAME *name, char **str_out)
{
    BIO *b = NULL;
    char *str;

    *str_out = NULL;
    b = BIO_new(BIO_s_mem());
    if (b == NULL)
        return ENOMEM;
    if (X509_NAME_print_ex(b, name, 0, XN_FLAG_SEP_COMMA_PLUS) < 0)
        goto error;
    str = calloc(BIO_number_written(b) + 1, 1);
    if (str == NULL)
        goto error;
    BIO_read(b, str, BIO_number_written(b));
    BIO_free(b);
    *str_out = str;
    return 0;

error:
    BIO_free(b);
    return ENOMEM;
}

/*
 * Get number of certificates available after crypto_load_certs()
 */
static krb5_error_code
crypto_cert_get_count(pkinit_identity_crypto_context id_cryptoctx,
                      int *cert_count)
{
    int count;

    *cert_count = 0;
    if (id_cryptoctx == NULL || id_cryptoctx->creds[0] == NULL)
        return EINVAL;

    for (count = 0;
         count <= MAX_CREDS_ALLOWED && id_cryptoctx->creds[count] != NULL;
         count++);
    *cert_count = count;
    return 0;
}

void
crypto_cert_free_matching_data(krb5_context context,
                               pkinit_cert_matching_data *md)
{
    int i;

    if (md == NULL)
        return;
    free(md->subject_dn);
    free(md->issuer_dn);
    for (i = 0; md->sans != NULL && md->sans[i] != NULL; i++)
        krb5_free_principal(context, md->sans[i]);
    free(md->sans);
    for (i = 0; md->upns != NULL && md->upns[i] != NULL; i++)
        free(md->upns[i]);
    free(md->upns);
    free(md);
}

/*
 * Free certificate matching data.
 */
void
crypto_cert_free_matching_data_list(krb5_context context,
                                    pkinit_cert_matching_data **list)
{
    int i;

    for (i = 0; list != NULL && list[i] != NULL; i++)
        crypto_cert_free_matching_data(context, list[i]);
    free(list);
}

/*
 * Get certificate matching data for cert.
 */
static krb5_error_code
get_matching_data(krb5_context context,
                  pkinit_plg_crypto_context plg_cryptoctx,
                  pkinit_req_crypto_context req_cryptoctx, X509 *cert,
                  pkinit_cert_matching_data **md_out)
{
    krb5_error_code ret = ENOMEM;
    pkinit_cert_matching_data *md = NULL;

    *md_out = NULL;

    md = calloc(1, sizeof(*md));
    if (md == NULL)
        goto cleanup;

    ret = rfc2253_name(X509_get_subject_name(cert), &md->subject_dn);
    if (ret)
        goto cleanup;
    ret = rfc2253_name(X509_get_issuer_name(cert), &md->issuer_dn);
    if (ret)
        goto cleanup;

    /* Get the SAN data. */
    ret = crypto_retrieve_X509_sans(context, plg_cryptoctx, req_cryptoctx,
                                    cert, &md->sans, &md->upns, NULL);
    if (ret)
        goto cleanup;

    /* Get the KU and EKU data. */
    ret = crypto_retrieve_X509_key_usage(context, plg_cryptoctx,
                                         req_cryptoctx, cert, &md->ku_bits,
                                         &md->eku_bits);
    if (ret)
        goto cleanup;

    *md_out = md;
    md = NULL;

cleanup:
    crypto_cert_free_matching_data(context, md);
    return ret;
}

krb5_error_code
crypto_cert_get_matching_data(krb5_context context,
                              pkinit_plg_crypto_context plg_cryptoctx,
                              pkinit_req_crypto_context req_cryptoctx,
                              pkinit_identity_crypto_context id_cryptoctx,
                              pkinit_cert_matching_data ***md_out)
{
    krb5_error_code ret;
    pkinit_cert_matching_data **md_list = NULL;
    int count, i;

    ret = crypto_cert_get_count(id_cryptoctx, &count);
    if (ret)
        goto cleanup;

    md_list = calloc(count + 1, sizeof(*md_list));
    if (md_list == NULL) {
        ret = ENOMEM;
        goto cleanup;
    }

    for (i = 0; i < count; i++) {
        ret = get_matching_data(context, plg_cryptoctx, req_cryptoctx,
                                id_cryptoctx->creds[i]->cert, &md_list[i]);
        if (ret) {
            pkiDebug("%s: crypto_cert_get_matching_data error %d, %s\n",
                     __FUNCTION__, ret, error_message(ret));
            goto cleanup;
        }
    }

    *md_out = md_list;
    md_list = NULL;

cleanup:
    crypto_cert_free_matching_data_list(context, md_list);
    return ret;
}

/*
 * Set the certificate in idctx->creds[cred_index] as the selected certificate.
 */
krb5_error_code
crypto_cert_select(krb5_context context, pkinit_identity_crypto_context idctx,
                   size_t cred_index)
{
    pkinit_cred_info ci = NULL;

    if (cred_index >= MAX_CREDS_ALLOWED || idctx->creds[cred_index] == NULL)
        return ENOENT;

    ci = idctx->creds[cred_index];
    /* copy the selected cert into our id_cryptoctx */
    if (idctx->my_certs != NULL)
        sk_X509_pop_free(idctx->my_certs, X509_free);
    idctx->my_certs = sk_X509_new_null();
    sk_X509_push(idctx->my_certs, ci->cert);
    free(idctx->identity);
    /* hang on to the selected credential name */
    if (ci->name != NULL)
        idctx->identity = strdup(ci->name);
    else
        idctx->identity = NULL;

    ci->cert = NULL;       /* Don't free it twice */
    idctx->cert_index = 0;
    if (idctx->pkcs11_method != 1) {
        idctx->my_key = ci->key;
        ci->key = NULL;    /* Don't free it twice */
    }
#ifndef WITHOUT_PKCS11
    else {
        idctx->cert_id = ci->cert_id;
        ci->cert_id = NULL; /* Don't free it twice */
        idctx->cert_id_len = ci->cert_id_len;
    }
#endif
    return 0;
}

/*
 * Choose the default certificate as "the chosen one"
 */
krb5_error_code
crypto_cert_select_default(krb5_context context,
                           pkinit_plg_crypto_context plg_cryptoctx,
                           pkinit_req_crypto_context req_cryptoctx,
                           pkinit_identity_crypto_context id_cryptoctx)
{
    krb5_error_code retval;
    int cert_count;

    retval = crypto_cert_get_count(id_cryptoctx, &cert_count);
    if (retval)
        goto errout;

    if (cert_count != 1) {
        TRACE_PKINIT_NO_DEFAULT_CERT(context, cert_count);
        retval = EINVAL;
        goto errout;
    }
    /* copy the selected cert into our id_cryptoctx */
    if (id_cryptoctx->my_certs != NULL) {
        sk_X509_pop_free(id_cryptoctx->my_certs, X509_free);
    }
    id_cryptoctx->my_certs = sk_X509_new_null();
    sk_X509_push(id_cryptoctx->my_certs, id_cryptoctx->creds[0]->cert);
    id_cryptoctx->creds[0]->cert = NULL;        /* Don't free it twice */
    id_cryptoctx->cert_index = 0;
    /* hang on to the selected credential name */
    if (id_cryptoctx->creds[0]->name != NULL)
        id_cryptoctx->identity = strdup(id_cryptoctx->creds[0]->name);
    else
        id_cryptoctx->identity = NULL;

    if (id_cryptoctx->pkcs11_method != 1) {
        id_cryptoctx->my_key = id_cryptoctx->creds[0]->key;
        id_cryptoctx->creds[0]->key = NULL;     /* Don't free it twice */
    }
#ifndef WITHOUT_PKCS11
    else {
        id_cryptoctx->cert_id = id_cryptoctx->creds[0]->cert_id;
        id_cryptoctx->creds[0]->cert_id = NULL; /* Don't free it twice */
        id_cryptoctx->cert_id_len = id_cryptoctx->creds[0]->cert_id_len;
    }
#endif
    retval = 0;
errout:
    return retval;
}



static krb5_error_code
load_cas_and_crls(krb5_context context,
                  pkinit_plg_crypto_context plg_cryptoctx,
                  pkinit_req_crypto_context req_cryptoctx,
                  pkinit_identity_crypto_context id_cryptoctx,
                  int catype,
                  char *filename)
{
    STACK_OF(X509_INFO) *sk = NULL;
    STACK_OF(X509) *ca_certs = NULL;
    STACK_OF(X509_CRL) *ca_crls = NULL;
    BIO *in = NULL;
    krb5_error_code retval = ENOMEM;
    int i = 0;

    /* If there isn't already a stack in the context,
     * create a temporary one now */
    switch(catype) {
    case CATYPE_ANCHORS:
        if (id_cryptoctx->trustedCAs != NULL)
            ca_certs = id_cryptoctx->trustedCAs;
        else {
            ca_certs = sk_X509_new_null();
            if (ca_certs == NULL)
                return ENOMEM;
        }
        break;
    case CATYPE_INTERMEDIATES:
        if (id_cryptoctx->intermediateCAs != NULL)
            ca_certs = id_cryptoctx->intermediateCAs;
        else {
            ca_certs = sk_X509_new_null();
            if (ca_certs == NULL)
                return ENOMEM;
        }
        break;
    case CATYPE_CRLS:
        if (id_cryptoctx->revoked != NULL)
            ca_crls = id_cryptoctx->revoked;
        else {
            ca_crls = sk_X509_CRL_new_null();
            if (ca_crls == NULL)
                return ENOMEM;
        }
        break;
    default:
        return ENOTSUP;
    }

    if (!(in = BIO_new_file(filename, "r"))) {
        retval = oerr(context, 0, _("Cannot open file '%s'"), filename);
        goto cleanup;
    }

    /* This loads from a file, a stack of x509/crl/pkey sets */
    if ((sk = PEM_X509_INFO_read_bio(in, NULL, NULL, NULL)) == NULL) {
        pkiDebug("%s: error reading file '%s'\n", __FUNCTION__, filename);
        retval = oerr(context, 0, _("Cannot read file '%s'"), filename);
        goto cleanup;
    }

    /* scan over the stack created from loading the file contents,
     * weed out duplicates, and push new ones onto the return stack
     */
    for (i = 0; i < sk_X509_INFO_num(sk); i++) {
        X509_INFO *xi = sk_X509_INFO_value(sk, i);
        if (xi != NULL && xi->x509 != NULL && catype != CATYPE_CRLS) {
            int j = 0, size = sk_X509_num(ca_certs), flag = 0;

            if (!size) {
                sk_X509_push(ca_certs, xi->x509);
                xi->x509 = NULL;
                continue;
            }
            for (j = 0; j < size; j++) {
                X509 *x = sk_X509_value(ca_certs, j);
                flag = X509_cmp(x, xi->x509);
                if (flag == 0)
                    break;
                else
                    continue;
            }
            if (flag != 0) {
                sk_X509_push(ca_certs, X509_dup(xi->x509));
            }
        } else if (xi != NULL && xi->crl != NULL && catype == CATYPE_CRLS) {
            int j = 0, size = sk_X509_CRL_num(ca_crls), flag = 0;
            if (!size) {
                sk_X509_CRL_push(ca_crls, xi->crl);
                xi->crl = NULL;
                continue;
            }
            for (j = 0; j < size; j++) {
                X509_CRL *x = sk_X509_CRL_value(ca_crls, j);
                flag = X509_CRL_cmp(x, xi->crl);
                if (flag == 0)
                    break;
                else
                    continue;
            }
            if (flag != 0) {
                sk_X509_CRL_push(ca_crls, X509_CRL_dup(xi->crl));
            }
        }
    }

    /* If we added something and there wasn't a stack in the
     * context before, add the temporary stack to the context.
     */
    switch(catype) {
    case CATYPE_ANCHORS:
        if (sk_X509_num(ca_certs) == 0) {
            TRACE_PKINIT_NO_CA_ANCHOR(context, filename);
            if (id_cryptoctx->trustedCAs == NULL)
                sk_X509_free(ca_certs);
        } else {
            if (id_cryptoctx->trustedCAs == NULL)
                id_cryptoctx->trustedCAs = ca_certs;
        }
        break;
    case CATYPE_INTERMEDIATES:
        if (sk_X509_num(ca_certs) == 0) {
            TRACE_PKINIT_NO_CA_INTERMEDIATE(context, filename);
            if (id_cryptoctx->intermediateCAs == NULL)
                sk_X509_free(ca_certs);
        } else {
            if (id_cryptoctx->intermediateCAs == NULL)
                id_cryptoctx->intermediateCAs = ca_certs;
        }
        break;
    case CATYPE_CRLS:
        if (sk_X509_CRL_num(ca_crls) == 0) {
            TRACE_PKINIT_NO_CRL(context, filename);
            if (id_cryptoctx->revoked == NULL)
                sk_X509_CRL_free(ca_crls);
        } else {
            if (id_cryptoctx->revoked == NULL)
                id_cryptoctx->revoked = ca_crls;
        }
        break;
    default:
        /* Should have been caught above! */
        retval = EINVAL;
        goto cleanup;
        break;
    }

    retval = 0;

cleanup:
    if (in != NULL)
        BIO_free(in);
    if (sk != NULL)
        sk_X509_INFO_pop_free(sk, X509_INFO_free);

    return retval;
}

static krb5_error_code
load_cas_and_crls_dir(krb5_context context,
                      pkinit_plg_crypto_context plg_cryptoctx,
                      pkinit_req_crypto_context req_cryptoctx,
                      pkinit_identity_crypto_context id_cryptoctx,
                      int catype,
                      char *dirname)
{
    krb5_error_code retval = EINVAL;
    DIR *d = NULL;
    struct dirent *dentry = NULL;
    char filename[1024];

    if (dirname == NULL)
        return EINVAL;

    d = opendir(dirname);
    if (d == NULL)
        return ENOENT;

    while ((dentry = readdir(d))) {
        if (strlen(dirname) + strlen(dentry->d_name) + 2 > sizeof(filename)) {
            pkiDebug("%s: Path too long -- directory '%s' and file '%s'\n",
                     __FUNCTION__, dirname, dentry->d_name);
            goto cleanup;
        }
        /* Ignore subdirectories and anything starting with a dot */
#ifdef DT_DIR
        if (dentry->d_type == DT_DIR)
            continue;
#endif
        if (dentry->d_name[0] == '.')
            continue;
        snprintf(filename, sizeof(filename), "%s/%s", dirname, dentry->d_name);

        retval = load_cas_and_crls(context, plg_cryptoctx, req_cryptoctx,
                                   id_cryptoctx, catype, filename);
        if (retval)
            goto cleanup;
    }

    retval = 0;

cleanup:
    if (d != NULL)
        closedir(d);

    return retval;
}

krb5_error_code
crypto_load_cas_and_crls(krb5_context context,
                         pkinit_plg_crypto_context plg_cryptoctx,
                         pkinit_req_crypto_context req_cryptoctx,
                         pkinit_identity_opts *idopts,
                         pkinit_identity_crypto_context id_cryptoctx,
                         int idtype,
                         int catype,
                         char *id)
{
    switch (idtype) {
    case IDTYPE_FILE:
        TRACE_PKINIT_LOAD_FROM_FILE(context, id);
        return load_cas_and_crls(context, plg_cryptoctx, req_cryptoctx,
                                 id_cryptoctx, catype, id);
        break;
    case IDTYPE_DIR:
        TRACE_PKINIT_LOAD_FROM_DIR(context, id);
        return load_cas_and_crls_dir(context, plg_cryptoctx, req_cryptoctx,
                                     id_cryptoctx, catype, id);
        break;
    default:
        return ENOTSUP;
        break;
    }
}

static krb5_error_code
create_identifiers_from_stack(STACK_OF(X509) *sk,
                              krb5_external_principal_identifier *** ids)
{
    int i = 0, sk_size = sk_X509_num(sk);
    krb5_external_principal_identifier **krb5_cas = NULL;
    X509 *x = NULL;
    X509_NAME *xn = NULL;
    unsigned char *p = NULL;
    int len = 0;
    PKCS7_ISSUER_AND_SERIAL *is = NULL;
    char buf[DN_BUF_LEN];

    *ids = NULL;

    krb5_cas = calloc(sk_size + 1, sizeof(*krb5_cas));
    if (krb5_cas == NULL)
        return ENOMEM;

    for (i = 0; i < sk_size; i++) {
        krb5_cas[i] = malloc(sizeof(krb5_external_principal_identifier));

        x = sk_X509_value(sk, i);

        X509_NAME_oneline(X509_get_subject_name(x), buf, sizeof(buf));
        pkiDebug("#%d cert= %s\n", i, buf);

        /* fill-in subjectName */
        krb5_cas[i]->subjectName.magic = 0;
        krb5_cas[i]->subjectName.length = 0;
        krb5_cas[i]->subjectName.data = NULL;

        xn = X509_get_subject_name(x);
        len = i2d_X509_NAME(xn, NULL);
        if ((p = malloc((size_t) len)) == NULL)
            goto oom;
        krb5_cas[i]->subjectName.data = (char *)p;
        i2d_X509_NAME(xn, &p);
        krb5_cas[i]->subjectName.length = len;

        /* fill-in issuerAndSerialNumber */
        krb5_cas[i]->issuerAndSerialNumber.length = 0;
        krb5_cas[i]->issuerAndSerialNumber.magic = 0;
        krb5_cas[i]->issuerAndSerialNumber.data = NULL;

        is = PKCS7_ISSUER_AND_SERIAL_new();
        if (is == NULL)
            goto oom;
        X509_NAME_set(&is->issuer, X509_get_issuer_name(x));
        ASN1_INTEGER_free(is->serial);
        is->serial = ASN1_INTEGER_dup(X509_get_serialNumber(x));
        if (is->serial == NULL)
            goto oom;
        len = i2d_PKCS7_ISSUER_AND_SERIAL(is, NULL);
        p = malloc(len);
        if (p == NULL)
            goto oom;
        krb5_cas[i]->issuerAndSerialNumber.data = (char *)p;
        i2d_PKCS7_ISSUER_AND_SERIAL(is, &p);
        krb5_cas[i]->issuerAndSerialNumber.length = len;

        /* fill-in subjectKeyIdentifier */
        krb5_cas[i]->subjectKeyIdentifier.length = 0;
        krb5_cas[i]->subjectKeyIdentifier.magic = 0;
        krb5_cas[i]->subjectKeyIdentifier.data = NULL;

        if (X509_get_ext_by_NID(x, NID_subject_key_identifier, -1) >= 0) {
            ASN1_OCTET_STRING *ikeyid;

            ikeyid = X509_get_ext_d2i(x, NID_subject_key_identifier, NULL,
                                      NULL);
            if (ikeyid != NULL) {
                len = i2d_ASN1_OCTET_STRING(ikeyid, NULL);
                p = malloc(len);
                if (p == NULL)
                    goto oom;
                krb5_cas[i]->subjectKeyIdentifier.data = (char *)p;
                i2d_ASN1_OCTET_STRING(ikeyid, &p);
                krb5_cas[i]->subjectKeyIdentifier.length = len;
                ASN1_OCTET_STRING_free(ikeyid);
            }
        }
        PKCS7_ISSUER_AND_SERIAL_free(is);
        is = NULL;
    }

    *ids = krb5_cas;
    return 0;

oom:
    free_krb5_external_principal_identifier(&krb5_cas);
    PKCS7_ISSUER_AND_SERIAL_free(is);
    return ENOMEM;
}

static krb5_error_code
create_krb5_invalidCertificates(krb5_context context,
                                pkinit_plg_crypto_context plg_cryptoctx,
                                pkinit_req_crypto_context req_cryptoctx,
                                pkinit_identity_crypto_context id_cryptoctx,
                                krb5_external_principal_identifier *** ids)
{

    krb5_error_code retval = ENOMEM;
    STACK_OF(X509) *sk = NULL;

    *ids = NULL;
    if (req_cryptoctx->received_cert == NULL)
        return KRB5KDC_ERR_PREAUTH_FAILED;

    sk = sk_X509_new_null();
    if (sk == NULL)
        goto cleanup;
    sk_X509_push(sk, req_cryptoctx->received_cert);

    retval = create_identifiers_from_stack(sk, ids);

    sk_X509_free(sk);
cleanup:

    return retval;
}

krb5_error_code
create_krb5_supportedCMSTypes(krb5_context context,
                              pkinit_plg_crypto_context plg_cryptoctx,
                              pkinit_req_crypto_context req_cryptoctx,
                              pkinit_identity_crypto_context id_cryptoctx,
                              krb5_algorithm_identifier ***algs_out)
{
    krb5_error_code ret;
    krb5_algorithm_identifier **algs = NULL;
    size_t i, count;

    *algs_out = NULL;

    /* Count supported OIDs and allocate list (including null terminator). */
    for (count = 0; supported_cms_algs[count] != NULL; count++);
    algs = k5calloc(count + 1, sizeof(*algs), &ret);
    if (algs == NULL)
        goto cleanup;

    /* Add an algorithm identifier for each OID, with no parameters. */
    for (i = 0; i < count; i++) {
        algs[i] = k5alloc(sizeof(*algs[i]), &ret);
        if (algs[i] == NULL)
            goto cleanup;
        ret = krb5int_copy_data_contents(context, supported_cms_algs[i],
                                         &algs[i]->algorithm);
        if (ret)
            goto cleanup;
        algs[i]->parameters = empty_data();
    }

    *algs_out = algs;
    algs = NULL;

cleanup:
    free_krb5_algorithm_identifiers(&algs);
    return ret;
}

krb5_error_code
create_krb5_trustedCertifiers(krb5_context context,
                              pkinit_plg_crypto_context plg_cryptoctx,
                              pkinit_req_crypto_context req_cryptoctx,
                              pkinit_identity_crypto_context id_cryptoctx,
                              krb5_external_principal_identifier *** ids)
{

    krb5_error_code retval = ENOMEM;
    STACK_OF(X509) *sk = id_cryptoctx->trustedCAs;

    *ids = NULL;
    if (id_cryptoctx->trustedCAs == NULL)
        return KRB5KDC_ERR_PREAUTH_FAILED;

    retval = create_identifiers_from_stack(sk, ids);

    return retval;
}

krb5_error_code
create_issuerAndSerial(krb5_context context,
                       pkinit_plg_crypto_context plg_cryptoctx,
                       pkinit_req_crypto_context req_cryptoctx,
                       pkinit_identity_crypto_context id_cryptoctx,
                       unsigned char **out,
                       unsigned int *out_len)
{
    unsigned char *p = NULL;
    PKCS7_ISSUER_AND_SERIAL *is = NULL;
    int len = 0;
    krb5_error_code retval = ENOMEM;
    X509 *cert = req_cryptoctx->received_cert;

    *out = NULL;
    *out_len = 0;
    if (req_cryptoctx->received_cert == NULL)
        return 0;

    is = PKCS7_ISSUER_AND_SERIAL_new();
    X509_NAME_set(&is->issuer, X509_get_issuer_name(cert));
    ASN1_INTEGER_free(is->serial);
    is->serial = ASN1_INTEGER_dup(X509_get_serialNumber(cert));
    len = i2d_PKCS7_ISSUER_AND_SERIAL(is, NULL);
    if ((p = *out = malloc((size_t) len)) == NULL)
        goto cleanup;
    i2d_PKCS7_ISSUER_AND_SERIAL(is, &p);
    *out_len = len;
    retval = 0;

cleanup:
    X509_NAME_free(is->issuer);
    ASN1_INTEGER_free(is->serial);
    free(is);

    return retval;
}

krb5_error_code
pkinit_process_td_trusted_certifiers(
    krb5_context context,
    pkinit_plg_crypto_context plg_cryptoctx,
    pkinit_req_crypto_context req_cryptoctx,
    pkinit_identity_crypto_context id_cryptoctx,
    krb5_external_principal_identifier **krb5_trusted_certifiers,
    int td_type)
{
    krb5_error_code retval = ENOMEM;
    STACK_OF(X509_NAME) *sk_xn = NULL;
    X509_NAME *xn = NULL;
    PKCS7_ISSUER_AND_SERIAL *is = NULL;
    ASN1_OCTET_STRING *id = NULL;
    const unsigned char *p = NULL;
    char buf[DN_BUF_LEN];
    int i = 0;

    if (td_type == TD_TRUSTED_CERTIFIERS)
        pkiDebug("received trusted certifiers\n");
    else
        pkiDebug("received invalid certificate\n");

    sk_xn = sk_X509_NAME_new_null();
    while(krb5_trusted_certifiers[i] != NULL) {
        if (krb5_trusted_certifiers[i]->subjectName.data != NULL) {
            p = (unsigned char *)krb5_trusted_certifiers[i]->subjectName.data;
            xn = d2i_X509_NAME(NULL, &p,
                               (int)krb5_trusted_certifiers[i]->subjectName.length);
            if (xn == NULL)
                goto cleanup;
            X509_NAME_oneline(xn, buf, sizeof(buf));
            if (td_type == TD_TRUSTED_CERTIFIERS)
                pkiDebug("#%d cert = %s is trusted by kdc\n", i, buf);
            else
                pkiDebug("#%d cert = %s is invalid\n", i, buf);
            sk_X509_NAME_push(sk_xn, xn);
        }

        if (krb5_trusted_certifiers[i]->issuerAndSerialNumber.data != NULL) {
            p = (unsigned char *)
                krb5_trusted_certifiers[i]->issuerAndSerialNumber.data;
            is = d2i_PKCS7_ISSUER_AND_SERIAL(NULL, &p,
                                             (int)krb5_trusted_certifiers[i]->issuerAndSerialNumber.length);
            if (is == NULL)
                goto cleanup;
            X509_NAME_oneline(is->issuer, buf, sizeof(buf));
            if (td_type == TD_TRUSTED_CERTIFIERS)
                pkiDebug("#%d issuer = %s serial = %ld is trusted bu kdc\n", i,
                         buf, ASN1_INTEGER_get(is->serial));
            else
                pkiDebug("#%d issuer = %s serial = %ld is invalid\n", i, buf,
                         ASN1_INTEGER_get(is->serial));
            PKCS7_ISSUER_AND_SERIAL_free(is);
        }

        if (krb5_trusted_certifiers[i]->subjectKeyIdentifier.data != NULL) {
            p = (unsigned char *)
                krb5_trusted_certifiers[i]->subjectKeyIdentifier.data;
            id = d2i_ASN1_OCTET_STRING(NULL, &p,
                                       (int)krb5_trusted_certifiers[i]->subjectKeyIdentifier.length);
            if (id == NULL)
                goto cleanup;
            /* XXX */
            ASN1_OCTET_STRING_free(id);
        }
        i++;
    }
    /* XXX Since we not doing anything with received trusted certifiers
     * return an error. this is the place where we can pick a different
     * client certificate based on the information in td_trusted_certifiers
     */
    retval = KRB5KDC_ERR_PREAUTH_FAILED;
cleanup:
    if (sk_xn != NULL)
        sk_X509_NAME_pop_free(sk_xn, X509_NAME_free);

    return retval;
}

/* Originally based on OpenSSL's PKCS7_dataDecode(), now modified to remove the
 * use of BIO objects and to fit the PKINIT internal interfaces. */
static int
pkcs7_decrypt(krb5_context context,
              pkinit_identity_crypto_context id_cryptoctx, PKCS7 *p7,
              unsigned char **data_out, unsigned int *len_out)
{
    krb5_error_code ret;
    int ok = 0, plaintext_len = 0, final_len;
    unsigned int keylen = 0, eklen = 0, blocksize;
    unsigned char *ek = NULL, *tkey = NULL, *plaintext = NULL, *use_key;
    ASN1_OCTET_STRING *data_body = p7->d.enveloped->enc_data->enc_data;
    const EVP_CIPHER *evp_cipher;
    EVP_CIPHER_CTX *evp_ctx = NULL;
    X509_ALGOR *enc_alg = p7->d.enveloped->enc_data->algorithm;
    STACK_OF(PKCS7_RECIP_INFO) *rsk = p7->d.enveloped->recipientinfo;
    PKCS7_RECIP_INFO *ri = NULL;

    *data_out = NULL;
    *len_out = 0;

    p7->state = PKCS7_S_HEADER;

    /* RFC 4556 section 3.2.3.2 requires that there be exactly one
     * recipientInfo. */
    if (sk_PKCS7_RECIP_INFO_num(rsk) != 1) {
        pkiDebug("invalid number of EnvelopedData RecipientInfos\n");
        return 0;
    }
    ri = sk_PKCS7_RECIP_INFO_value(rsk, 0);

    evp_cipher = EVP_get_cipherbyobj(enc_alg->algorithm);
    if (evp_cipher == NULL)
        goto cleanup;
    keylen = EVP_CIPHER_key_length(evp_cipher);
    blocksize = EVP_CIPHER_block_size(evp_cipher);

    evp_ctx = EVP_CIPHER_CTX_new();
    if (evp_ctx == NULL)
        goto cleanup;
    if (!EVP_DecryptInit(evp_ctx, evp_cipher, NULL, NULL) ||
        EVP_CIPHER_asn1_to_param(evp_ctx, enc_alg->parameter) <= 0)
        goto cleanup;

    /* Generate a random symmetric key to avoid exposing timing data if RSA
     * decryption fails the padding check. */
    tkey = malloc(keylen);
    if (tkey == NULL || !EVP_CIPHER_CTX_rand_key(evp_ctx, tkey))
        goto cleanup;

    /* Decrypt the secret key with the private key. */
    ret = pkinit_decode_data(context, id_cryptoctx,
                             ASN1_STRING_get0_data(ri->enc_key),
                             ASN1_STRING_length(ri->enc_key), &ek, &eklen);
    use_key = (ret || eklen != keylen) ? tkey : ek;

    /* Allocate a plaintext buffer and decrypt data_body into it. */
    plaintext = malloc(data_body->length + blocksize);
    if (plaintext == NULL)
        goto cleanup;
    if (!EVP_DecryptInit(evp_ctx, NULL, use_key, NULL))
        goto cleanup;
    if (!EVP_DecryptUpdate(evp_ctx, plaintext, &plaintext_len,
                           data_body->data, data_body->length))
        goto cleanup;
    if (!EVP_DecryptFinal(evp_ctx, plaintext + plaintext_len, &final_len))
        goto cleanup;
    plaintext_len += final_len;

    *len_out = plaintext_len;
    *data_out = plaintext;
    plaintext = NULL;
    ok = 1;

cleanup:
    EVP_CIPHER_CTX_free(evp_ctx);
    zapfree(plaintext, plaintext_len);
    zapfree(ek, eklen);
    zapfree(tkey, keylen);
    return ok;
}

#ifdef DEBUG_DH
static void
print_dh(DH * dh, char *msg)
{
    BIO *bio_err = NULL;

    bio_err = BIO_new(BIO_s_file());
    BIO_set_fp(bio_err, stderr, BIO_NOCLOSE | BIO_FP_TEXT);

    if (msg)
        BIO_puts(bio_err, (const char *)msg);
    if (dh)
        DHparams_print(bio_err, dh);

    BIO_puts(bio_err, "private key: ");
    BN_print(bio_err, dh->priv_key);
    BIO_puts(bio_err, (const char *)"\n");
    BIO_free(bio_err);

}

static void
print_pubkey(BIGNUM * key, char *msg)
{
    BIO *bio_err = NULL;

    bio_err = BIO_new(BIO_s_file());
    BIO_set_fp(bio_err, stderr, BIO_NOCLOSE | BIO_FP_TEXT);

    if (msg)
        BIO_puts(bio_err, (const char *)msg);
    if (key)
        BN_print(bio_err, key);
    BIO_puts(bio_err, "\n");

    BIO_free(bio_err);

}
#endif

static const char *
pkcs11err(int err)
{
    int i;

    for (i = 0; pkcs11_errstrings[i].text != NULL; i++)
        if (pkcs11_errstrings[i].code == err)
            break;
    if (pkcs11_errstrings[i].text != NULL)
        return (pkcs11_errstrings[i].text);

    return "unknown PKCS11 error";
}

/*
 * Add an item to the pkinit_identity_crypto_context's list of deferred
 * identities.
 */
krb5_error_code
crypto_set_deferred_id(krb5_context context,
                       pkinit_identity_crypto_context id_cryptoctx,
                       const char *identity, const char *password)
{
    unsigned long ck_flags;

    ck_flags = pkinit_get_deferred_id_flags(id_cryptoctx->deferred_ids,
                                            identity);
    return pkinit_set_deferred_id(&id_cryptoctx->deferred_ids,
                                  identity, ck_flags, password);
}

/*
 * Retrieve a read-only copy of the pkinit_identity_crypto_context's list of
 * deferred identities, sure to be valid only until the next time someone calls
 * either pkinit_set_deferred_id() or crypto_set_deferred_id().
 */
const pkinit_deferred_id *
crypto_get_deferred_ids(krb5_context context,
                        pkinit_identity_crypto_context id_cryptoctx)
{
    pkinit_deferred_id *deferred;
    const pkinit_deferred_id *ret;

    deferred = id_cryptoctx->deferred_ids;
    ret = (const pkinit_deferred_id *)deferred;
    return ret;
}

/* Return the received certificate as DER-encoded data. */
krb5_error_code
crypto_encode_der_cert(krb5_context context, pkinit_req_crypto_context reqctx,
                       uint8_t **der_out, size_t *der_len)
{
    int len;
    unsigned char *der, *p;

    *der_out = NULL;
    *der_len = 0;

    if (reqctx->received_cert == NULL)
        return EINVAL;
    p = NULL;
    len = i2d_X509(reqctx->received_cert, NULL);
    if (len <= 0)
        return EINVAL;
    p = der = malloc(len);
    if (der == NULL)
        return ENOMEM;
    if (i2d_X509(reqctx->received_cert, &p) <= 0) {
        free(der);
        return EINVAL;
    }
    *der_out = der;
    *der_len = len;
    return 0;
}

/*
 * Get the certificate matching data from the request certificate.
 */
krb5_error_code
crypto_req_cert_matching_data(krb5_context context,
                              pkinit_plg_crypto_context plgctx,
                              pkinit_req_crypto_context reqctx,
                              pkinit_cert_matching_data **md_out)
{
    *md_out = NULL;

    if (reqctx == NULL || reqctx->received_cert == NULL)
        return ENOENT;

    return get_matching_data(context, plgctx, reqctx, reqctx->received_cert,
                             md_out);
}
