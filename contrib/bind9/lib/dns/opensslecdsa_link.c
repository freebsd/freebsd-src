/*
 * Copyright (C) 2012-2014  Internet Systems Consortium, Inc. ("ISC")
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND ISC DISCLAIMS ALL WARRANTIES WITH
 * REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS.  IN NO EVENT SHALL ISC BE LIABLE FOR ANY SPECIAL, DIRECT,
 * INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
 * LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE
 * OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

#include <config.h>

#ifdef HAVE_OPENSSL_ECDSA

#if !defined(HAVE_EVP_SHA256) || !defined(HAVE_EVP_SHA384)
#error "ECDSA without EVP for SHA2?"
#endif

#include <isc/entropy.h>
#include <isc/mem.h>
#include <isc/sha2.h>
#include <isc/string.h>
#include <isc/util.h>

#include <dns/keyvalues.h>
#include <dst/result.h>

#include "dst_internal.h"
#include "dst_openssl.h"
#include "dst_parse.h"

#include <openssl/err.h>
#include <openssl/objects.h>
#include <openssl/ecdsa.h>
#include <openssl/bn.h>

#ifndef NID_X9_62_prime256v1
#error "P-256 group is not known (NID_X9_62_prime256v1)"
#endif
#ifndef NID_secp384r1
#error "P-384 group is not known (NID_secp384r1)"
#endif

#define DST_RET(a) {ret = a; goto err;}

static isc_result_t opensslecdsa_todns(const dst_key_t *key,
				       isc_buffer_t *data);

static isc_result_t
opensslecdsa_createctx(dst_key_t *key, dst_context_t *dctx) {
	EVP_MD_CTX *evp_md_ctx;
	const EVP_MD *type = NULL;

	UNUSED(key);
	REQUIRE(dctx->key->key_alg == DST_ALG_ECDSA256 ||
		dctx->key->key_alg == DST_ALG_ECDSA384);

	evp_md_ctx = EVP_MD_CTX_create();
	if (evp_md_ctx == NULL)
		return (ISC_R_NOMEMORY);
	if (dctx->key->key_alg == DST_ALG_ECDSA256)
		type = EVP_sha256();
	else
		type = EVP_sha384();

	if (!EVP_DigestInit_ex(evp_md_ctx, type, NULL)) {
		EVP_MD_CTX_destroy(evp_md_ctx);
		return (dst__openssl_toresult3(dctx->category,
					       "EVP_DigestInit_ex",
					       ISC_R_FAILURE));
	}

	dctx->ctxdata.evp_md_ctx = evp_md_ctx;

	return (ISC_R_SUCCESS);
}

static void
opensslecdsa_destroyctx(dst_context_t *dctx) {
	EVP_MD_CTX *evp_md_ctx = dctx->ctxdata.evp_md_ctx;

	REQUIRE(dctx->key->key_alg == DST_ALG_ECDSA256 ||
		dctx->key->key_alg == DST_ALG_ECDSA384);

	if (evp_md_ctx != NULL) {
		EVP_MD_CTX_destroy(evp_md_ctx);
		dctx->ctxdata.evp_md_ctx = NULL;
	}
}

static isc_result_t
opensslecdsa_adddata(dst_context_t *dctx, const isc_region_t *data) {
	EVP_MD_CTX *evp_md_ctx = dctx->ctxdata.evp_md_ctx;

	REQUIRE(dctx->key->key_alg == DST_ALG_ECDSA256 ||
		dctx->key->key_alg == DST_ALG_ECDSA384);

	if (!EVP_DigestUpdate(evp_md_ctx, data->base, data->length))
		return (dst__openssl_toresult3(dctx->category,
					       "EVP_DigestUpdate",
					       ISC_R_FAILURE));

	return (ISC_R_SUCCESS);
}

static int
BN_bn2bin_fixed(BIGNUM *bn, unsigned char *buf, int size) {
	int bytes = size - BN_num_bytes(bn);

	while (bytes-- > 0)
		*buf++ = 0;
	BN_bn2bin(bn, buf);
	return (size);
}

static isc_result_t
opensslecdsa_sign(dst_context_t *dctx, isc_buffer_t *sig) {
	isc_result_t ret;
	dst_key_t *key = dctx->key;
	isc_region_t r;
	ECDSA_SIG *ecdsasig;
	EVP_MD_CTX *evp_md_ctx = dctx->ctxdata.evp_md_ctx;
	EVP_PKEY *pkey = key->keydata.pkey;
	EC_KEY *eckey = EVP_PKEY_get1_EC_KEY(pkey);
	unsigned int dgstlen, siglen;
	unsigned char digest[EVP_MAX_MD_SIZE];

	REQUIRE(key->key_alg == DST_ALG_ECDSA256 ||
		key->key_alg == DST_ALG_ECDSA384);

	if (eckey == NULL)
		return (ISC_R_FAILURE);

	if (key->key_alg == DST_ALG_ECDSA256)
		siglen = DNS_SIG_ECDSA256SIZE;
	else
		siglen = DNS_SIG_ECDSA384SIZE;

	isc_buffer_availableregion(sig, &r);
	if (r.length < siglen)
		DST_RET(ISC_R_NOSPACE);

	if (!EVP_DigestFinal(evp_md_ctx, digest, &dgstlen))
		DST_RET(dst__openssl_toresult3(dctx->category,
					       "EVP_DigestFinal",
					       ISC_R_FAILURE));

	ecdsasig = ECDSA_do_sign(digest, dgstlen, eckey);
	if (ecdsasig == NULL)
		DST_RET(dst__openssl_toresult3(dctx->category,
					       "ECDSA_do_sign",
					       DST_R_SIGNFAILURE));
	BN_bn2bin_fixed(ecdsasig->r, r.base, siglen / 2);
	isc_region_consume(&r, siglen / 2);
	BN_bn2bin_fixed(ecdsasig->s, r.base, siglen / 2);
	isc_region_consume(&r, siglen / 2);
	ECDSA_SIG_free(ecdsasig);
	isc_buffer_add(sig, siglen);
	ret = ISC_R_SUCCESS;

 err:
	if (eckey != NULL)
		EC_KEY_free(eckey);
	return (ret);
}

static isc_result_t
opensslecdsa_verify(dst_context_t *dctx, const isc_region_t *sig) {
	isc_result_t ret;
	dst_key_t *key = dctx->key;
	int status;
	unsigned char *cp = sig->base;
	ECDSA_SIG *ecdsasig = NULL;
	EVP_MD_CTX *evp_md_ctx = dctx->ctxdata.evp_md_ctx;
	EVP_PKEY *pkey = key->keydata.pkey;
	EC_KEY *eckey = EVP_PKEY_get1_EC_KEY(pkey);
	unsigned int dgstlen, siglen;
	unsigned char digest[EVP_MAX_MD_SIZE];

	REQUIRE(key->key_alg == DST_ALG_ECDSA256 ||
		key->key_alg == DST_ALG_ECDSA384);

	if (eckey == NULL)
		return (ISC_R_FAILURE);

	if (key->key_alg == DST_ALG_ECDSA256)
		siglen = DNS_SIG_ECDSA256SIZE;
	else
		siglen = DNS_SIG_ECDSA384SIZE;

	if (sig->length != siglen)
		return (DST_R_VERIFYFAILURE);

	if (!EVP_DigestFinal_ex(evp_md_ctx, digest, &dgstlen))
		DST_RET (dst__openssl_toresult3(dctx->category,
						"EVP_DigestFinal_ex",
						ISC_R_FAILURE));

	ecdsasig = ECDSA_SIG_new();
	if (ecdsasig == NULL)
		DST_RET (ISC_R_NOMEMORY);
	if (ecdsasig->r != NULL)
		BN_free(ecdsasig->r);
	ecdsasig->r = BN_bin2bn(cp, siglen / 2, NULL);
	cp += siglen / 2;
	if (ecdsasig->s != NULL)
		BN_free(ecdsasig->s);
	ecdsasig->s = BN_bin2bn(cp, siglen / 2, NULL);
	/* cp += siglen / 2; */

	status = ECDSA_do_verify(digest, dgstlen, ecdsasig, eckey);
	switch (status) {
	case 1:
		ret = ISC_R_SUCCESS;
		break;
	case 0:
		ret = dst__openssl_toresult(DST_R_VERIFYFAILURE);
		break;
	default:
		ret = dst__openssl_toresult3(dctx->category,
					     "ECDSA_do_verify",
					     DST_R_VERIFYFAILURE);
		break;
	}

 err:
	if (ecdsasig != NULL)
		ECDSA_SIG_free(ecdsasig);
	if (eckey != NULL)
		EC_KEY_free(eckey);
	return (ret);
}

static isc_boolean_t
opensslecdsa_compare(const dst_key_t *key1, const dst_key_t *key2) {
	isc_boolean_t ret;
	int status;
	EVP_PKEY *pkey1 = key1->keydata.pkey;
	EVP_PKEY *pkey2 = key2->keydata.pkey;
	EC_KEY *eckey1 = NULL;
	EC_KEY *eckey2 = NULL;
	const BIGNUM *priv1, *priv2;

	if (pkey1 == NULL && pkey2 == NULL)
		return (ISC_TRUE);
	else if (pkey1 == NULL || pkey2 == NULL)
		return (ISC_FALSE);

	eckey1 = EVP_PKEY_get1_EC_KEY(pkey1);
	eckey2 = EVP_PKEY_get1_EC_KEY(pkey2);
	if (eckey1 == NULL && eckey2 == NULL) {
		DST_RET (ISC_TRUE);
	} else if (eckey1 == NULL || eckey2 == NULL)
		DST_RET (ISC_FALSE);

	status = EVP_PKEY_cmp(pkey1, pkey2);
	if (status != 1)
		DST_RET (ISC_FALSE);

	priv1 = EC_KEY_get0_private_key(eckey1);
	priv2 = EC_KEY_get0_private_key(eckey2);
	if (priv1 != NULL || priv2 != NULL) {
		if (priv1 == NULL || priv2 == NULL)
			DST_RET (ISC_FALSE);
		if (BN_cmp(priv1, priv2) != 0)
			DST_RET (ISC_FALSE);
	}
	ret = ISC_TRUE;

 err:
	if (eckey1 != NULL)
		EC_KEY_free(eckey1);
	if (eckey2 != NULL)
		EC_KEY_free(eckey2);
	return (ret);
}

static isc_result_t
opensslecdsa_generate(dst_key_t *key, int unused, void (*callback)(int)) {
	isc_result_t ret;
	EVP_PKEY *pkey;
	EC_KEY *eckey = NULL;
	int group_nid;

	REQUIRE(key->key_alg == DST_ALG_ECDSA256 ||
		key->key_alg == DST_ALG_ECDSA384);
	UNUSED(unused);
	UNUSED(callback);

	if (key->key_alg == DST_ALG_ECDSA256)
		group_nid = NID_X9_62_prime256v1;
	else
		group_nid = NID_secp384r1;

	eckey = EC_KEY_new_by_curve_name(group_nid);
	if (eckey == NULL)
		return (dst__openssl_toresult2("EC_KEY_new_by_curve_name",
					       DST_R_OPENSSLFAILURE));

	if (EC_KEY_generate_key(eckey) != 1)
		DST_RET (dst__openssl_toresult2("EC_KEY_generate_key",
						DST_R_OPENSSLFAILURE));

	pkey = EVP_PKEY_new();
	if (pkey == NULL)
		DST_RET (ISC_R_NOMEMORY);
	if (!EVP_PKEY_set1_EC_KEY(pkey, eckey)) {
		EVP_PKEY_free(pkey);
		DST_RET (ISC_R_FAILURE);
	}
	key->keydata.pkey = pkey;
	ret = ISC_R_SUCCESS;

 err:
	if (eckey != NULL)
		EC_KEY_free(eckey);
	return (ret);
}

static isc_boolean_t
opensslecdsa_isprivate(const dst_key_t *key) {
	isc_boolean_t ret;
	EVP_PKEY *pkey = key->keydata.pkey;
	EC_KEY *eckey = EVP_PKEY_get1_EC_KEY(pkey);

	ret = ISC_TF(eckey != NULL && EC_KEY_get0_private_key(eckey) != NULL);
	if (eckey != NULL)
		EC_KEY_free(eckey);
	return (ret);
}

static void
opensslecdsa_destroy(dst_key_t *key) {
	EVP_PKEY *pkey = key->keydata.pkey;

	EVP_PKEY_free(pkey);
	key->keydata.pkey = NULL;
}

static isc_result_t
opensslecdsa_todns(const dst_key_t *key, isc_buffer_t *data) {
	isc_result_t ret;
	EVP_PKEY *pkey;
	EC_KEY *eckey = NULL;
	isc_region_t r;
	int len;
	unsigned char *cp;
	unsigned char buf[DNS_KEY_ECDSA384SIZE + 1];

	REQUIRE(key->keydata.pkey != NULL);

	pkey = key->keydata.pkey;
	eckey = EVP_PKEY_get1_EC_KEY(pkey);
	if (eckey == NULL)
		return (dst__openssl_toresult(ISC_R_FAILURE));
	len = i2o_ECPublicKey(eckey, NULL);
	/* skip form */
	len--;

	isc_buffer_availableregion(data, &r);
	if (r.length < (unsigned int) len)
		DST_RET (ISC_R_NOSPACE);
	cp = buf;
	if (!i2o_ECPublicKey(eckey, &cp))
		DST_RET (dst__openssl_toresult(ISC_R_FAILURE));
	memmove(r.base, buf + 1, len);
	isc_buffer_add(data, len);
	ret = ISC_R_SUCCESS;

 err:
	if (eckey != NULL)
		EC_KEY_free(eckey);
	return (ret);
}

static isc_result_t
opensslecdsa_fromdns(dst_key_t *key, isc_buffer_t *data) {
	isc_result_t ret;
	EVP_PKEY *pkey;
	EC_KEY *eckey = NULL;
	isc_region_t r;
	int group_nid;
	unsigned int len;
	const unsigned char *cp;
	unsigned char buf[DNS_KEY_ECDSA384SIZE + 1];

	REQUIRE(key->key_alg == DST_ALG_ECDSA256 ||
		key->key_alg == DST_ALG_ECDSA384);

	if (key->key_alg == DST_ALG_ECDSA256) {
		len = DNS_KEY_ECDSA256SIZE;
		group_nid = NID_X9_62_prime256v1;
	} else {
		len = DNS_KEY_ECDSA384SIZE;
		group_nid = NID_secp384r1;
	}

	isc_buffer_remainingregion(data, &r);
	if (r.length == 0)
		return (ISC_R_SUCCESS);
	if (r.length < len)
		return (DST_R_INVALIDPUBLICKEY);

	eckey = EC_KEY_new_by_curve_name(group_nid);
	if (eckey == NULL)
		return (dst__openssl_toresult(DST_R_OPENSSLFAILURE));

	buf[0] = POINT_CONVERSION_UNCOMPRESSED;
	memmove(buf + 1, r.base, len);
	cp = buf;
	if (o2i_ECPublicKey(&eckey,
			    (const unsigned char **) &cp,
			    (long) len + 1) == NULL)
		DST_RET (dst__openssl_toresult(DST_R_INVALIDPUBLICKEY));
	if (EC_KEY_check_key(eckey) != 1)
		DST_RET (dst__openssl_toresult(DST_R_INVALIDPUBLICKEY));

	pkey = EVP_PKEY_new();
	if (pkey == NULL)
		DST_RET (ISC_R_NOMEMORY);
	if (!EVP_PKEY_set1_EC_KEY(pkey, eckey)) {
		EVP_PKEY_free(pkey);
		DST_RET (dst__openssl_toresult(ISC_R_FAILURE));
	}

	isc_buffer_forward(data, len);
	key->keydata.pkey = pkey;
	ret = ISC_R_SUCCESS;

 err:
	if (eckey != NULL)
		EC_KEY_free(eckey);
	return (ret);
}

static isc_result_t
opensslecdsa_tofile(const dst_key_t *key, const char *directory) {
	isc_result_t ret;
	EVP_PKEY *pkey;
	EC_KEY *eckey = NULL;
	const BIGNUM *privkey;
	dst_private_t priv;
	unsigned char *buf = NULL;

	if (key->keydata.pkey == NULL)
		return (DST_R_NULLKEY);

	if (key->external) {
		priv.nelements = 0;
		return (dst__privstruct_writefile(key, &priv, directory));
	}

	pkey = key->keydata.pkey;
	eckey = EVP_PKEY_get1_EC_KEY(pkey);
	if (eckey == NULL)
		return (dst__openssl_toresult(DST_R_OPENSSLFAILURE));
	privkey = EC_KEY_get0_private_key(eckey);
	if (privkey == NULL)
		DST_RET (ISC_R_FAILURE);

	buf = isc_mem_get(key->mctx, BN_num_bytes(privkey));
	if (buf == NULL)
		DST_RET (ISC_R_NOMEMORY);

	priv.elements[0].tag = TAG_ECDSA_PRIVATEKEY;
	priv.elements[0].length = BN_num_bytes(privkey);
	BN_bn2bin(privkey, buf);
	priv.elements[0].data = buf;
	priv.nelements = ECDSA_NTAGS;
	ret = dst__privstruct_writefile(key, &priv, directory);

 err:
	if (eckey != NULL)
		EC_KEY_free(eckey);
	if (buf != NULL)
		isc_mem_put(key->mctx, buf, BN_num_bytes(privkey));
	return (ret);
}

static isc_result_t
ecdsa_check(EC_KEY *eckey, dst_key_t *pub)
{
	isc_result_t ret = ISC_R_FAILURE;
	EVP_PKEY *pkey;
	EC_KEY *pubeckey = NULL;
	const EC_POINT *pubkey;

	if (pub == NULL)
		return (ISC_R_SUCCESS);
	pkey = pub->keydata.pkey;
	if (pkey == NULL)
		return (ISC_R_SUCCESS);
	pubeckey = EVP_PKEY_get1_EC_KEY(pkey);
	if (pubeckey == NULL)
		return (ISC_R_SUCCESS);
	pubkey = EC_KEY_get0_public_key(pubeckey);
	if (pubkey == NULL)
		DST_RET (ISC_R_SUCCESS);
	if (EC_KEY_set_public_key(eckey, pubkey) != 1)
		DST_RET (ISC_R_SUCCESS);
	if (EC_KEY_check_key(eckey) == 1)
		DST_RET (ISC_R_SUCCESS);

 err:
	if (pubeckey != NULL)
		EC_KEY_free(pubeckey);
	return (ret);
}

static isc_result_t
opensslecdsa_parse(dst_key_t *key, isc_lex_t *lexer, dst_key_t *pub) {
	dst_private_t priv;
	isc_result_t ret;
	EVP_PKEY *pkey, *pubpkey;
	EC_KEY *eckey = NULL, *pubeckey = NULL;
	const EC_POINT *pubkey;
	BIGNUM *privkey;
	int group_nid;
	isc_mem_t *mctx = key->mctx;

	REQUIRE(key->key_alg == DST_ALG_ECDSA256 ||
		key->key_alg == DST_ALG_ECDSA384);

	if (key->key_alg == DST_ALG_ECDSA256)
		group_nid = NID_X9_62_prime256v1;
	else
		group_nid = NID_secp384r1;

	eckey = EC_KEY_new_by_curve_name(group_nid);
	if (eckey == NULL)
		return (dst__openssl_toresult(DST_R_OPENSSLFAILURE));

	/* read private key file */
	ret = dst__privstruct_parse(key, DST_ALG_ECDSA256, lexer, mctx, &priv);
	if (ret != ISC_R_SUCCESS)
		goto err;

	if (key->external) {
		/*
		 * Copy the public key to this new key.
		 */
		if (pub == NULL)
			DST_RET(DST_R_INVALIDPRIVATEKEY);
		pubpkey = pub->keydata.pkey;
		pubeckey = EVP_PKEY_get1_EC_KEY(pubpkey);
		if (pubeckey == NULL)
			DST_RET(DST_R_INVALIDPRIVATEKEY);
		pubkey = EC_KEY_get0_public_key(pubeckey);
		if (pubkey == NULL)
			DST_RET(DST_R_INVALIDPRIVATEKEY);
		if (EC_KEY_set_public_key(eckey, pubkey) != 1)
			DST_RET(DST_R_INVALIDPRIVATEKEY);
		if (EC_KEY_check_key(eckey) != 1)
			DST_RET(DST_R_INVALIDPRIVATEKEY);
	} else {
		privkey = BN_bin2bn(priv.elements[0].data,
				    priv.elements[0].length, NULL);
		if (privkey == NULL)
			DST_RET(ISC_R_NOMEMORY);
		if (!EC_KEY_set_private_key(eckey, privkey))
			DST_RET(ISC_R_NOMEMORY);
		if (ecdsa_check(eckey, pub) != ISC_R_SUCCESS)
			DST_RET(DST_R_INVALIDPRIVATEKEY);
		dst__privstruct_free(&priv, mctx);
		memset(&priv, 0, sizeof(priv));
	}

	pkey = EVP_PKEY_new();
	if (pkey == NULL)
		DST_RET (ISC_R_NOMEMORY);
	if (!EVP_PKEY_set1_EC_KEY(pkey, eckey)) {
		EVP_PKEY_free(pkey);
		DST_RET (dst__openssl_toresult(DST_R_OPENSSLFAILURE));
	}
	key->keydata.pkey = pkey;
	ret = ISC_R_SUCCESS;

 err:
	if (eckey != NULL)
		EC_KEY_free(eckey);
	if (pubeckey != NULL)
		EC_KEY_free(pubeckey);
	dst__privstruct_free(&priv, mctx);
	memset(&priv, 0, sizeof(priv));
	return (ret);
}

static dst_func_t opensslecdsa_functions = {
	opensslecdsa_createctx,
	opensslecdsa_destroyctx,
	opensslecdsa_adddata,
	opensslecdsa_sign,
	opensslecdsa_verify,
	NULL, /*%< verify2 */
	NULL, /*%< computesecret */
	opensslecdsa_compare,
	NULL, /*%< paramcompare */
	opensslecdsa_generate,
	opensslecdsa_isprivate,
	opensslecdsa_destroy,
	opensslecdsa_todns,
	opensslecdsa_fromdns,
	opensslecdsa_tofile,
	opensslecdsa_parse,
	NULL, /*%< cleanup */
	NULL, /*%< fromlabel */
	NULL, /*%< dump */
	NULL, /*%< restore */
};

isc_result_t
dst__opensslecdsa_init(dst_func_t **funcp) {
	REQUIRE(funcp != NULL);
	if (*funcp == NULL)
		*funcp = &opensslecdsa_functions;
	return (ISC_R_SUCCESS);
}

#else /* HAVE_OPENSSL_ECDSA */

#include <isc/util.h>

EMPTY_TRANSLATION_UNIT

#endif /* HAVE_OPENSSL_ECDSA */
/*! \file */
