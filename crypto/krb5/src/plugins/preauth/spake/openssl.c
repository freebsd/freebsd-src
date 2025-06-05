/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* plugins/preauth/spake/openssl.c - SPAKE implementations using OpenSSL */
/*
 * Copyright (C) 2015 by the Massachusetts Institute of Technology.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * * Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 *
 * * Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer in
 *   the documentation and/or other materials provided with the
 *   distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "k5-int.h"

#include "groups.h"
#include "iana.h"

#ifdef SPAKE_OPENSSL
#include <openssl/bn.h>
#include <openssl/ec.h>
#include <openssl/obj_mac.h>
#include <openssl/evp.h>

/* OpenSSL 1.1 standardizes constructor and destructor names, renaming
 * EVP_MD_CTX_create and EVP_MD_CTX_destroy. */
#if OPENSSL_VERSION_NUMBER < 0x10100000L
#define EVP_MD_CTX_new EVP_MD_CTX_create
#define EVP_MD_CTX_free EVP_MD_CTX_destroy
#endif

struct groupdata_st {
    const groupdef *gdef;
    EC_GROUP *group;
    BIGNUM *order;
    BN_CTX *ctx;
    EC_POINT *M;
    EC_POINT *N;
    const EVP_MD *md;
};

static void
ossl_fini(groupdata *gd)
{
    if (gd == NULL)
        return;

    EC_GROUP_free(gd->group);
    EC_POINT_free(gd->M);
    EC_POINT_free(gd->N);
    BN_CTX_free(gd->ctx);
    BN_free(gd->order);
    free(gd);
}

static krb5_error_code
ossl_init(krb5_context context, const groupdef *gdef, groupdata **gdata_out)
{
    const spake_iana *reg = gdef->reg;
    const EVP_MD *md;
    groupdata *gd;
    int nid;

    switch (reg->id) {
    case SPAKE_GROUP_P256:
        nid = NID_X9_62_prime256v1;
        md = EVP_sha256();
        break;
    case SPAKE_GROUP_P384:
        nid = NID_secp384r1;
        md = EVP_sha384();
        break;
    case SPAKE_GROUP_P521:
        nid = NID_secp521r1;
        md = EVP_sha512();
        break;
    default:
        return EINVAL;
    };

    gd = calloc(1, sizeof(*gd));
    if (gd == NULL)
        return ENOMEM;
    gd->gdef = gdef;

    gd->group = EC_GROUP_new_by_curve_name(nid);
    if (gd->group == NULL)
        goto error;

    gd->ctx = BN_CTX_new();
    if (gd->ctx == NULL)
        goto error;

    gd->order = BN_new();
    if (gd->order == NULL)
        goto error;
    if (!EC_GROUP_get_order(gd->group, gd->order, gd->ctx))
        goto error;

    gd->M = EC_POINT_new(gd->group);
    if (gd->M == NULL)
        goto error;
    if (!EC_POINT_oct2point(gd->group, gd->M, reg->m, reg->elem_len, gd->ctx))
        goto error;

    gd->N = EC_POINT_new(gd->group);
    if (gd->N == NULL)
        goto error;
    if (!EC_POINT_oct2point(gd->group, gd->N, reg->n, reg->elem_len, gd->ctx))
        goto error;

    gd->md = md;

    *gdata_out = gd;
    return 0;

error:
    ossl_fini(gd);
    return ENOMEM;
}

/* Convert pseudo-random bytes into a scalar value in constant time.
 * Return NULL on failure. */
static BIGNUM *
unmarshal_w(const groupdata *gdata, const uint8_t *wbytes)
{
    const spake_iana *reg = gdata->gdef->reg;
    BIGNUM *w = NULL;

    w = BN_new();
    if (w == NULL)
        return NULL;

    BN_set_flags(w, BN_FLG_CONSTTIME);

    if (BN_bin2bn(wbytes, reg->mult_len, w) &&
        BN_div(NULL, w, w, gdata->order, gdata->ctx))
        return w;

    BN_free(w);
    return NULL;
}

static krb5_error_code
ossl_keygen(krb5_context context, groupdata *gdata, const uint8_t *wbytes,
            krb5_boolean use_m, uint8_t *priv_out, uint8_t *pub_out)
{
    const spake_iana *reg = gdata->gdef->reg;
    const EC_POINT *constant = use_m ? gdata->M : gdata->N;
    krb5_boolean success = FALSE;
    EC_POINT *pub = NULL;
    BIGNUM *priv = NULL, *w = NULL;
    size_t len;

    w = unmarshal_w(gdata, wbytes);
    if (w == NULL)
        goto cleanup;

    pub = EC_POINT_new(gdata->group);
    if (pub == NULL)
        goto cleanup;

    priv = BN_new();
    if (priv == NULL)
        goto cleanup;

    if (!BN_rand_range(priv, gdata->order))
        goto cleanup;

    /* Compute priv*G + w*constant; EC_POINT_mul() does this in one call. */
    if (!EC_POINT_mul(gdata->group, pub, priv, constant, w, gdata->ctx))
        goto cleanup;

    /* Marshal priv into priv_out. */
    memset(priv_out, 0, reg->mult_len);
    BN_bn2bin(priv, &priv_out[reg->mult_len - BN_num_bytes(priv)]);

    /* Marshal pub into pub_out. */
    len = EC_POINT_point2oct(gdata->group, pub, POINT_CONVERSION_COMPRESSED,
                             pub_out, reg->elem_len, gdata->ctx);
    if (len != reg->elem_len)
        goto cleanup;

    success = TRUE;

cleanup:
    EC_POINT_free(pub);
    BN_clear_free(priv);
    BN_clear_free(w);
    return success ? 0 : ENOMEM;
}

static krb5_error_code
ossl_result(krb5_context context, groupdata *gdata, const uint8_t *wbytes,
            const uint8_t *ourpriv, const uint8_t *theirpub,
            krb5_boolean use_m, uint8_t *elem_out)
{
    const spake_iana *reg = gdata->gdef->reg;
    const EC_POINT *constant = use_m ? gdata->M : gdata->N;
    krb5_boolean success = FALSE, invalid = FALSE;
    EC_POINT *result = NULL, *pub = NULL;
    BIGNUM *priv = NULL, *w = NULL;
    size_t len;

    w = unmarshal_w(gdata, wbytes);
    if (w == NULL)
        goto cleanup;

    priv = BN_bin2bn(ourpriv, reg->mult_len, NULL);
    if (priv == NULL)
        goto cleanup;

    pub = EC_POINT_new(gdata->group);
    if (pub == NULL)
        goto cleanup;
    if (!EC_POINT_oct2point(gdata->group, pub, theirpub, reg->elem_len,
                            gdata->ctx)) {
        invalid = TRUE;
        goto cleanup;
    }

    /* Compute result = priv*(pub - w*constant), using result to hold the
     * intermediate steps. */
    result = EC_POINT_new(gdata->group);
    if (result == NULL)
        goto cleanup;
    if (!EC_POINT_mul(gdata->group, result, NULL, constant, w, gdata->ctx))
        goto cleanup;
    if (!EC_POINT_invert(gdata->group, result, gdata->ctx))
        goto cleanup;
    if (!EC_POINT_add(gdata->group, result, pub, result, gdata->ctx))
        goto cleanup;
    if (!EC_POINT_mul(gdata->group, result, NULL, result, priv, gdata->ctx))
        goto cleanup;

    /* Marshal result into elem_out. */
    len = EC_POINT_point2oct(gdata->group, result, POINT_CONVERSION_COMPRESSED,
                             elem_out, reg->elem_len, gdata->ctx);
    if (len != reg->elem_len)
        goto cleanup;

    success = TRUE;

cleanup:
    BN_clear_free(priv);
    BN_clear_free(w);
    EC_POINT_free(pub);
    EC_POINT_clear_free(result);
    return invalid ? EINVAL : (success ? 0 : ENOMEM);
}

static krb5_error_code
ossl_hash(krb5_context context, groupdata *gdata, const krb5_data *dlist,
          size_t ndata, uint8_t *result_out)
{
    EVP_MD_CTX *ctx;
    size_t i;
    int ok;

    ctx = EVP_MD_CTX_new();
    if (ctx == NULL)
        return ENOMEM;
    ok = EVP_DigestInit_ex(ctx, gdata->md, NULL);
    for (i = 0; i < ndata; i++)
        ok = ok && EVP_DigestUpdate(ctx, dlist[i].data, dlist[i].length);
    ok = ok && EVP_DigestFinal_ex(ctx, result_out, NULL);
    EVP_MD_CTX_free(ctx);
    return ok ? 0 : ENOMEM;
}

groupdef ossl_P256 = {
    .reg = &spake_iana_p256,
    .init = ossl_init,
    .fini = ossl_fini,
    .keygen = ossl_keygen,
    .result = ossl_result,
    .hash = ossl_hash,
};

groupdef ossl_P384 = {
    .reg = &spake_iana_p384,
    .init = ossl_init,
    .fini = ossl_fini,
    .keygen = ossl_keygen,
    .result = ossl_result,
    .hash = ossl_hash,
};

groupdef ossl_P521 = {
    .reg = &spake_iana_p521,
    .init = ossl_init,
    .fini = ossl_fini,
    .keygen = ossl_keygen,
    .result = ossl_result,
    .hash = ossl_hash,
};
#endif /* SPAKE_OPENSSL */
