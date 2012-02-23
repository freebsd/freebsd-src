/*
 * Copyright (C) 2004-2009, 2011  Internet Systems Consortium, Inc. ("ISC")
 * Copyright (C) 2000-2003  Internet Software Consortium.
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

/*
 * Principal Author: Brian Wellington
 * $Id: opensslrsa_link.c,v 1.39.10.2 2011-03-11 02:57:35 marka Exp $
 */
#ifdef OPENSSL
#include <config.h>

#ifndef USE_EVP
#if !defined(HAVE_EVP_SHA256) || !defined(HAVE_EVP_SHA512)
#define USE_EVP 0
#else
#define USE_EVP 1
#endif
#endif


#include <isc/entropy.h>
#include <isc/md5.h>
#include <isc/sha1.h>
#include <isc/sha2.h>
#include <isc/mem.h>
#include <isc/string.h>
#include <isc/util.h>

#include <dst/result.h>

#include "dst_internal.h"
#include "dst_openssl.h"
#include "dst_parse.h"

#include <openssl/err.h>
#include <openssl/objects.h>
#include <openssl/rsa.h>
#if OPENSSL_VERSION_NUMBER > 0x00908000L
#include <openssl/bn.h>
#endif
#ifdef USE_ENGINE
#include <openssl/engine.h>
#endif

/*
 * We don't use configure for windows so enforce the OpenSSL version
 * here.  Unlike with configure we don't support overriding this test.
 */
#ifdef WIN32
#if !((OPENSSL_VERSION_NUMBER >= 0x009070cfL && \
       OPENSSL_VERSION_NUMBER < 0x00908000L) || \
      OPENSSL_VERSION_NUMBER >= 0x0090804fL)
#error Please upgrade OpenSSL to 0.9.8d/0.9.7l or greater.
#endif
#endif


	/*
	 * XXXMPA  Temporarily disable RSA_BLINDING as it requires
	 * good quality random data that cannot currently be guaranteed.
	 * XXXMPA  Find which versions of openssl use pseudo random data
	 * and set RSA_FLAG_BLINDING for those.
	 */

#if 0
#if OPENSSL_VERSION_NUMBER < 0x0090601fL
#define SET_FLAGS(rsa) \
	do { \
	(rsa)->flags &= ~(RSA_FLAG_CACHE_PUBLIC | RSA_FLAG_CACHE_PRIVATE); \
	(rsa)->flags |= RSA_FLAG_BLINDING; \
	} while (0)
#else
#define SET_FLAGS(rsa) \
	do { \
		(rsa)->flags |= RSA_FLAG_BLINDING; \
	} while (0)
#endif
#endif

#if OPENSSL_VERSION_NUMBER < 0x0090601fL
#define SET_FLAGS(rsa) \
	do { \
	(rsa)->flags &= ~(RSA_FLAG_CACHE_PUBLIC | RSA_FLAG_CACHE_PRIVATE); \
	(rsa)->flags &= ~RSA_FLAG_BLINDING; \
	} while (0)
#elif defined(RSA_FLAG_NO_BLINDING)
#define SET_FLAGS(rsa) \
	do { \
		(rsa)->flags &= ~RSA_FLAG_BLINDING; \
		(rsa)->flags |= RSA_FLAG_NO_BLINDING; \
	} while (0)
#else
#define SET_FLAGS(rsa) \
	do { \
		(rsa)->flags &= ~RSA_FLAG_BLINDING; \
	} while (0)
#endif

#define DST_RET(a) {ret = a; goto err;}

static isc_result_t opensslrsa_todns(const dst_key_t *key, isc_buffer_t *data);

static isc_result_t
opensslrsa_createctx(dst_key_t *key, dst_context_t *dctx) {
#if USE_EVP
	EVP_MD_CTX *evp_md_ctx;
	const EVP_MD *type = NULL;
#endif

	UNUSED(key);
	REQUIRE(dctx->key->key_alg == DST_ALG_RSAMD5 ||
		dctx->key->key_alg == DST_ALG_RSASHA1 ||
		dctx->key->key_alg == DST_ALG_NSEC3RSASHA1 ||
		dctx->key->key_alg == DST_ALG_RSASHA256 ||
		dctx->key->key_alg == DST_ALG_RSASHA512);

#if USE_EVP
	evp_md_ctx = EVP_MD_CTX_create();
	if (evp_md_ctx == NULL)
		return (ISC_R_NOMEMORY);

	switch (dctx->key->key_alg) {
	case DST_ALG_RSAMD5:
		type = EVP_md5();	/* MD5 + RSA */
		break;
	case DST_ALG_RSASHA1:
	case DST_ALG_NSEC3RSASHA1:
		type = EVP_sha1();	/* SHA1 + RSA */
		break;
#ifdef HAVE_EVP_SHA256
	case DST_ALG_RSASHA256:
		type = EVP_sha256();	/* SHA256 + RSA */
		break;
#endif
#ifdef HAVE_EVP_SHA512
	case DST_ALG_RSASHA512:
		type = EVP_sha512();
		break;
#endif
	default:
		INSIST(0);
	}

	if (!EVP_DigestInit_ex(evp_md_ctx, type, NULL)) {
		EVP_MD_CTX_destroy(evp_md_ctx);
		return (ISC_R_FAILURE);
	}
	dctx->ctxdata.evp_md_ctx = evp_md_ctx;
#else
	switch (dctx->key->key_alg) {
	case DST_ALG_RSAMD5:
		{
			isc_md5_t *md5ctx;

			md5ctx = isc_mem_get(dctx->mctx, sizeof(isc_md5_t));
			if (md5ctx == NULL)
				return (ISC_R_NOMEMORY);
			isc_md5_init(md5ctx);
			dctx->ctxdata.md5ctx = md5ctx;
		}
		break;
	case DST_ALG_RSASHA1:
	case DST_ALG_NSEC3RSASHA1:
		{
			isc_sha1_t *sha1ctx;

			sha1ctx = isc_mem_get(dctx->mctx, sizeof(isc_sha1_t));
			if (sha1ctx == NULL)
				return (ISC_R_NOMEMORY);
			isc_sha1_init(sha1ctx);
			dctx->ctxdata.sha1ctx = sha1ctx;
		}
		break;
	case DST_ALG_RSASHA256:
		{
			isc_sha256_t *sha256ctx;

			sha256ctx = isc_mem_get(dctx->mctx,
						sizeof(isc_sha256_t));
			if (sha256ctx == NULL)
				return (ISC_R_NOMEMORY);
			isc_sha256_init(sha256ctx);
			dctx->ctxdata.sha256ctx = sha256ctx;
		}
		break;
	case DST_ALG_RSASHA512:
		{
			isc_sha512_t *sha512ctx;

			sha512ctx = isc_mem_get(dctx->mctx,
						sizeof(isc_sha512_t));
			if (sha512ctx == NULL)
				return (ISC_R_NOMEMORY);
			isc_sha512_init(sha512ctx);
			dctx->ctxdata.sha512ctx = sha512ctx;
		}
		break;
	default:
		INSIST(0);
	}
#endif

	return (ISC_R_SUCCESS);
}

static void
opensslrsa_destroyctx(dst_context_t *dctx) {
#if USE_EVP
	EVP_MD_CTX *evp_md_ctx = dctx->ctxdata.evp_md_ctx;
#endif

	REQUIRE(dctx->key->key_alg == DST_ALG_RSAMD5 ||
		dctx->key->key_alg == DST_ALG_RSASHA1 ||
		dctx->key->key_alg == DST_ALG_NSEC3RSASHA1 ||
		dctx->key->key_alg == DST_ALG_RSASHA256 ||
		dctx->key->key_alg == DST_ALG_RSASHA512);

#if USE_EVP
	if (evp_md_ctx != NULL) {
		EVP_MD_CTX_destroy(evp_md_ctx);
		dctx->ctxdata.evp_md_ctx = NULL;
	}
#else
	switch (dctx->key->key_alg) {
	case DST_ALG_RSAMD5:
		{
			isc_md5_t *md5ctx = dctx->ctxdata.md5ctx;

			if (md5ctx != NULL) {
				isc_md5_invalidate(md5ctx);
				isc_mem_put(dctx->mctx, md5ctx,
					    sizeof(isc_md5_t));
				dctx->ctxdata.md5ctx = NULL;
			}
		}
		break;
	case DST_ALG_RSASHA1:
	case DST_ALG_NSEC3RSASHA1:
		{
			isc_sha1_t *sha1ctx = dctx->ctxdata.sha1ctx;

			if (sha1ctx != NULL) {
				isc_sha1_invalidate(sha1ctx);
				isc_mem_put(dctx->mctx, sha1ctx,
					    sizeof(isc_sha1_t));
				dctx->ctxdata.sha1ctx = NULL;
			}
		}
		break;
	case DST_ALG_RSASHA256:
		{
			isc_sha256_t *sha256ctx = dctx->ctxdata.sha256ctx;

			if (sha256ctx != NULL) {
				isc_sha256_invalidate(sha256ctx);
				isc_mem_put(dctx->mctx, sha256ctx,
					    sizeof(isc_sha256_t));
				dctx->ctxdata.sha256ctx = NULL;
			}
		}
		break;
	case DST_ALG_RSASHA512:
		{
			isc_sha512_t *sha512ctx = dctx->ctxdata.sha512ctx;

			if (sha512ctx != NULL) {
				isc_sha512_invalidate(sha512ctx);
				isc_mem_put(dctx->mctx, sha512ctx,
					    sizeof(isc_sha512_t));
				dctx->ctxdata.sha512ctx = NULL;
			}
		}
		break;
	default:
		INSIST(0);
	}
#endif
}

static isc_result_t
opensslrsa_adddata(dst_context_t *dctx, const isc_region_t *data) {
#if USE_EVP
	EVP_MD_CTX *evp_md_ctx = dctx->ctxdata.evp_md_ctx;
#endif

	REQUIRE(dctx->key->key_alg == DST_ALG_RSAMD5 ||
		dctx->key->key_alg == DST_ALG_RSASHA1 ||
		dctx->key->key_alg == DST_ALG_NSEC3RSASHA1 ||
		dctx->key->key_alg == DST_ALG_RSASHA256 ||
		dctx->key->key_alg == DST_ALG_RSASHA512);

#if USE_EVP
	if (!EVP_DigestUpdate(evp_md_ctx, data->base, data->length)) {
		return (ISC_R_FAILURE);
	}
#else
	switch (dctx->key->key_alg) {
	case DST_ALG_RSAMD5:
		{
			isc_md5_t *md5ctx = dctx->ctxdata.md5ctx;

			isc_md5_update(md5ctx, data->base, data->length);
		}
		break;
	case DST_ALG_RSASHA1:
	case DST_ALG_NSEC3RSASHA1:
		{
			isc_sha1_t *sha1ctx = dctx->ctxdata.sha1ctx;

			isc_sha1_update(sha1ctx, data->base, data->length);
		}
		break;
	case DST_ALG_RSASHA256:
		{
			isc_sha256_t *sha256ctx = dctx->ctxdata.sha256ctx;

			isc_sha256_update(sha256ctx, data->base, data->length);
		}
		break;
	case DST_ALG_RSASHA512:
		{
			isc_sha512_t *sha512ctx = dctx->ctxdata.sha512ctx;

			isc_sha512_update(sha512ctx, data->base, data->length);
		}
		break;
	default:
		INSIST(0);
	}
#endif
	return (ISC_R_SUCCESS);
}

#if ! USE_EVP && OPENSSL_VERSION_NUMBER < 0x00908000L
/*
 * Digest prefixes from RFC 5702.
 */
static unsigned char sha256_prefix[] =
	 { 0x30, 0x31, 0x30, 0x0d, 0x06, 0x09, 0x60, 0x86, 0x48,
	   0x01, 0x65, 0x03, 0x04, 0x02, 0x01, 0x05, 0x00, 0x04, 0x20};
static unsigned char sha512_prefix[] =
	 { 0x30, 0x51, 0x30, 0x0d, 0x06, 0x09, 0x60, 0x86, 0x48,
	   0x01, 0x65, 0x03, 0x04, 0x02, 0x03, 0x05, 0x00, 0x04, 0x40};
#define PREFIXLEN sizeof(sha512_prefix)
#else
#define PREFIXLEN 0
#endif

static isc_result_t
opensslrsa_sign(dst_context_t *dctx, isc_buffer_t *sig) {
	dst_key_t *key = dctx->key;
	isc_region_t r;
	unsigned int siglen = 0;
#if USE_EVP
	EVP_MD_CTX *evp_md_ctx = dctx->ctxdata.evp_md_ctx;
	EVP_PKEY *pkey = key->keydata.pkey;
#else
	RSA *rsa = key->keydata.rsa;
	/* note: ISC_SHA512_DIGESTLENGTH >= ISC_*_DIGESTLENGTH */
	unsigned char digest[PREFIXLEN + ISC_SHA512_DIGESTLENGTH];
	int status;
	int type = 0;
	unsigned int digestlen = 0;
	char *message;
	unsigned long err;
	const char* file;
	int line;
#if OPENSSL_VERSION_NUMBER < 0x00908000L
	unsigned int prefixlen = 0;
	const unsigned char *prefix = NULL;
#endif
#endif

	REQUIRE(dctx->key->key_alg == DST_ALG_RSAMD5 ||
		dctx->key->key_alg == DST_ALG_RSASHA1 ||
		dctx->key->key_alg == DST_ALG_NSEC3RSASHA1 ||
		dctx->key->key_alg == DST_ALG_RSASHA256 ||
		dctx->key->key_alg == DST_ALG_RSASHA512);

	isc_buffer_availableregion(sig, &r);

#if USE_EVP
	if (r.length < (unsigned int) EVP_PKEY_size(pkey))
		return (ISC_R_NOSPACE);

	if (!EVP_SignFinal(evp_md_ctx, r.base, &siglen, pkey)) {
		return (ISC_R_FAILURE);
	}
#else
	if (r.length < (unsigned int) RSA_size(rsa))
		return (ISC_R_NOSPACE);

	switch (dctx->key->key_alg) {
	case DST_ALG_RSAMD5:
		{
			isc_md5_t *md5ctx = dctx->ctxdata.md5ctx;

			isc_md5_final(md5ctx, digest);
			type = NID_md5;
			digestlen = ISC_MD5_DIGESTLENGTH;
		}
		break;
	case DST_ALG_RSASHA1:
	case DST_ALG_NSEC3RSASHA1:
		{
			isc_sha1_t *sha1ctx = dctx->ctxdata.sha1ctx;

			isc_sha1_final(sha1ctx, digest);
			type = NID_sha1;
			digestlen = ISC_SHA1_DIGESTLENGTH;
		}
		break;
	case DST_ALG_RSASHA256:
		{
			isc_sha256_t *sha256ctx = dctx->ctxdata.sha256ctx;

			isc_sha256_final(digest, sha256ctx);
			digestlen = ISC_SHA256_DIGESTLENGTH;
#if OPENSSL_VERSION_NUMBER < 0x00908000L
			prefix = sha256_prefix;
			prefixlen = sizeof(sha256_prefix);
#else
			type = NID_sha256;
#endif
		}
		break;
	case DST_ALG_RSASHA512:
		{
			isc_sha512_t *sha512ctx = dctx->ctxdata.sha512ctx;

			isc_sha512_final(digest, sha512ctx);
			digestlen = ISC_SHA512_DIGESTLENGTH;
#if OPENSSL_VERSION_NUMBER < 0x00908000L
			prefix = sha512_prefix;
			prefixlen = sizeof(sha512_prefix);
#else
			type = NID_sha512;
#endif
		}
		break;
	default:
		INSIST(0);
	}

#if OPENSSL_VERSION_NUMBER < 0x00908000L
	switch (dctx->key->key_alg) {
	case DST_ALG_RSAMD5:
	case DST_ALG_RSASHA1:
	case DST_ALG_NSEC3RSASHA1:
		INSIST(type != 0);
		status = RSA_sign(type, digest, digestlen, r.base,
				  &siglen, rsa);
		break;

	case DST_ALG_RSASHA256:
	case DST_ALG_RSASHA512:
		INSIST(prefix != NULL);
		INSIST(prefixlen != 0);
		INSIST(prefixlen + digestlen <= sizeof(digest));

		memmove(digest + prefixlen, digest, digestlen);
		memcpy(digest, prefix, prefixlen);
		status = RSA_private_encrypt(digestlen + prefixlen,
					     digest, r.base, rsa,
					     RSA_PKCS1_PADDING);
		if (status < 0)
			status = 0;
		else
			siglen = status;
		break;

	default:
		INSIST(0);
	}
#else
	INSIST(type != 0);
	status = RSA_sign(type, digest, digestlen, r.base, &siglen, rsa);
#endif
	if (status == 0) {
		err = ERR_peek_error_line(&file, &line);
		if (err != 0U) {
			message = ERR_error_string(err, NULL);
		}
		return (dst__openssl_toresult(DST_R_OPENSSLFAILURE));
	}
#endif

	isc_buffer_add(sig, siglen);

	return (ISC_R_SUCCESS);
}

static isc_result_t
opensslrsa_verify(dst_context_t *dctx, const isc_region_t *sig) {
	dst_key_t *key = dctx->key;
	int status = 0;
#if USE_EVP
	EVP_MD_CTX *evp_md_ctx = dctx->ctxdata.evp_md_ctx;
	EVP_PKEY *pkey = key->keydata.pkey;
#else
	/* note: ISC_SHA512_DIGESTLENGTH >= ISC_*_DIGESTLENGTH */
	unsigned char digest[ISC_SHA512_DIGESTLENGTH];
	int type = 0;
	unsigned int digestlen = 0;
	RSA *rsa = key->keydata.rsa;
#if OPENSSL_VERSION_NUMBER < 0x00908000L
	unsigned int prefixlen = 0;
	const unsigned char *prefix = NULL;
#endif
#endif

	REQUIRE(dctx->key->key_alg == DST_ALG_RSAMD5 ||
		dctx->key->key_alg == DST_ALG_RSASHA1 ||
		dctx->key->key_alg == DST_ALG_NSEC3RSASHA1 ||
		dctx->key->key_alg == DST_ALG_RSASHA256 ||
		dctx->key->key_alg == DST_ALG_RSASHA512);

#if USE_EVP
	status = EVP_VerifyFinal(evp_md_ctx, sig->base, sig->length, pkey);
#else
	switch (dctx->key->key_alg) {
	case DST_ALG_RSAMD5:
		{
			isc_md5_t *md5ctx = dctx->ctxdata.md5ctx;

			isc_md5_final(md5ctx, digest);
			type = NID_md5;
			digestlen = ISC_MD5_DIGESTLENGTH;
		}
		break;
	case DST_ALG_RSASHA1:
	case DST_ALG_NSEC3RSASHA1:
		{
			isc_sha1_t *sha1ctx = dctx->ctxdata.sha1ctx;

			isc_sha1_final(sha1ctx, digest);
			type = NID_sha1;
			digestlen = ISC_SHA1_DIGESTLENGTH;
		}
		break;
	case DST_ALG_RSASHA256:
		{
			isc_sha256_t *sha256ctx = dctx->ctxdata.sha256ctx;

			isc_sha256_final(digest, sha256ctx);
			digestlen = ISC_SHA256_DIGESTLENGTH;
#if OPENSSL_VERSION_NUMBER < 0x00908000L
			prefix = sha256_prefix;
			prefixlen = sizeof(sha256_prefix);
#else
			type = NID_sha256;
#endif
		}
		break;
	case DST_ALG_RSASHA512:
		{
			isc_sha512_t *sha512ctx = dctx->ctxdata.sha512ctx;

			isc_sha512_final(digest, sha512ctx);
			digestlen = ISC_SHA512_DIGESTLENGTH;
#if OPENSSL_VERSION_NUMBER < 0x00908000L
			prefix = sha512_prefix;
			prefixlen = sizeof(sha512_prefix);
#else
			type = NID_sha512;
#endif
		}
		break;
	default:
		INSIST(0);
	}

	if (sig->length != (unsigned int) RSA_size(rsa))
		return (DST_R_VERIFYFAILURE);

#if OPENSSL_VERSION_NUMBER < 0x00908000L
	switch (dctx->key->key_alg) {
	case DST_ALG_RSAMD5:
	case DST_ALG_RSASHA1:
	case DST_ALG_NSEC3RSASHA1:
		INSIST(type != 0);
		status = RSA_verify(type, digest, digestlen, sig->base,
				    RSA_size(rsa), rsa);
		break;

	case DST_ALG_RSASHA256:
	case DST_ALG_RSASHA512:
		{
			/*
			 * 1024 is big enough for all valid RSA bit sizes
			 * for use with DNSSEC.
			 */
			unsigned char original[PREFIXLEN + 1024];

			INSIST(prefix != NULL);
			INSIST(prefixlen != 0U);

			if (RSA_size(rsa) > (int)sizeof(original))
				return (DST_R_VERIFYFAILURE);

			status = RSA_public_decrypt(sig->length, sig->base,
						    original, rsa,
						    RSA_PKCS1_PADDING);
			if (status <= 0)
				return (DST_R_VERIFYFAILURE);
			if (status != (int)(prefixlen + digestlen))
				return (DST_R_VERIFYFAILURE);
			if (memcmp(original, prefix, prefixlen))
				return (DST_R_VERIFYFAILURE);
			if (memcmp(original + prefixlen, digest, digestlen))
				return (DST_R_VERIFYFAILURE);
			status = 1;
		}
		break;

	default:
		INSIST(0);
	}
#else
	INSIST(type != 0);
	status = RSA_verify(type, digest, digestlen, sig->base,
			     RSA_size(rsa), rsa);
#endif
#endif
	if (status != 1)
		return (dst__openssl_toresult(DST_R_VERIFYFAILURE));

	return (ISC_R_SUCCESS);
}

static isc_boolean_t
opensslrsa_compare(const dst_key_t *key1, const dst_key_t *key2) {
	int status;
	RSA *rsa1 = NULL, *rsa2 = NULL;
#if USE_EVP
	EVP_PKEY *pkey1, *pkey2;
#endif

#if USE_EVP
	pkey1 = key1->keydata.pkey;
	pkey2 = key2->keydata.pkey;
	/*
	 * The pkey reference will keep these around after
	 * the RSA_free() call.
	 */
	if (pkey1 != NULL) {
		rsa1 = EVP_PKEY_get1_RSA(pkey1);
		RSA_free(rsa1);
	}
	if (pkey2 != NULL) {
		rsa2 = EVP_PKEY_get1_RSA(pkey2);
		RSA_free(rsa2);
	}
#else
	rsa1 = key1->keydata.rsa;
	rsa2 = key2->keydata.rsa;
#endif

	if (rsa1 == NULL && rsa2 == NULL)
		return (ISC_TRUE);
	else if (rsa1 == NULL || rsa2 == NULL)
		return (ISC_FALSE);

	status = BN_cmp(rsa1->n, rsa2->n) ||
		 BN_cmp(rsa1->e, rsa2->e);

	if (status != 0)
		return (ISC_FALSE);

#if USE_EVP
	if ((rsa1->flags & RSA_FLAG_EXT_PKEY) != 0 ||
	    (rsa2->flags & RSA_FLAG_EXT_PKEY) != 0) {
		if ((rsa1->flags & RSA_FLAG_EXT_PKEY) == 0 ||
		    (rsa2->flags & RSA_FLAG_EXT_PKEY) == 0)
			return (ISC_FALSE);
		/*
		 * Can't compare private parameters, BTW does it make sense?
		 */
		return (ISC_TRUE);
	}
#endif

	if (rsa1->d != NULL || rsa2->d != NULL) {
		if (rsa1->d == NULL || rsa2->d == NULL)
			return (ISC_FALSE);
		status = BN_cmp(rsa1->d, rsa2->d) ||
			 BN_cmp(rsa1->p, rsa2->p) ||
			 BN_cmp(rsa1->q, rsa2->q);

		if (status != 0)
			return (ISC_FALSE);
	}
	return (ISC_TRUE);
}

#if OPENSSL_VERSION_NUMBER > 0x00908000L
static int
progress_cb(int p, int n, BN_GENCB *cb)
{
	union {
		void *dptr;
		void (*fptr)(int);
	} u;

	UNUSED(n);

	u.dptr = cb->arg;
	if (u.fptr != NULL)
		u.fptr(p);
	return (1);
}
#endif

static isc_result_t
opensslrsa_generate(dst_key_t *key, int exp, void (*callback)(int)) {
#if OPENSSL_VERSION_NUMBER > 0x00908000L
	BN_GENCB cb;
	union {
		void *dptr;
		void (*fptr)(int);
	} u;
	RSA *rsa = RSA_new();
	BIGNUM *e = BN_new();
#if USE_EVP
	EVP_PKEY *pkey = EVP_PKEY_new();
#endif

	if (rsa == NULL || e == NULL)
		goto err;
#if USE_EVP
	if (pkey == NULL)
		goto err;
	if (!EVP_PKEY_set1_RSA(pkey, rsa))
		goto err;
#endif

	if (exp == 0) {
		/* RSA_F4 0x10001 */
		BN_set_bit(e, 0);
		BN_set_bit(e, 16);
	} else {
		/* F5 0x100000001 */
		BN_set_bit(e, 0);
		BN_set_bit(e, 32);
	}

	if (callback == NULL) {
		BN_GENCB_set_old(&cb, NULL, NULL);
	} else {
		u.fptr = callback;
		BN_GENCB_set(&cb, &progress_cb, u.dptr);
	}

	if (RSA_generate_key_ex(rsa, key->key_size, e, &cb)) {
		BN_free(e);
		SET_FLAGS(rsa);
#if USE_EVP
		key->keydata.pkey = pkey;

		RSA_free(rsa);
#else
		key->keydata.rsa = rsa;
#endif
		return (ISC_R_SUCCESS);
	}

err:
#if USE_EVP
	if (pkey != NULL)
		EVP_PKEY_free(pkey);
#endif
	if (e != NULL)
		BN_free(e);
	if (rsa != NULL)
		RSA_free(rsa);
	return (dst__openssl_toresult(DST_R_OPENSSLFAILURE));
#else
	RSA *rsa;
	unsigned long e;
#if USE_EVP
	EVP_PKEY *pkey = EVP_PKEY_new();

	UNUSED(callback);

	if (pkey == NULL)
		return (ISC_R_NOMEMORY);
#else
	UNUSED(callback);
#endif

	if (exp == 0)
	       e = RSA_F4;
	else
	       e = 0x40000003;
	rsa = RSA_generate_key(key->key_size, e, NULL, NULL);
	if (rsa == NULL) {
#if USE_EVP
		EVP_PKEY_free(pkey);
#endif
		return (dst__openssl_toresult(DST_R_OPENSSLFAILURE));
	}
	SET_FLAGS(rsa);
#if USE_EVP
	if (!EVP_PKEY_set1_RSA(pkey, rsa)) {
		EVP_PKEY_free(pkey);
		RSA_free(rsa);
		return (dst__openssl_toresult(DST_R_OPENSSLFAILURE));
	}
	key->keydata.pkey = pkey;
	RSA_free(rsa);
#else
	key->keydata.rsa = rsa;
#endif

	return (ISC_R_SUCCESS);
#endif
}

static isc_boolean_t
opensslrsa_isprivate(const dst_key_t *key) {
#if USE_EVP
	RSA *rsa = EVP_PKEY_get1_RSA(key->keydata.pkey);
	INSIST(rsa != NULL);
	RSA_free(rsa);
	/* key->keydata.pkey still has a reference so rsa is still valid. */
#else
	RSA *rsa = key->keydata.rsa;
#endif
	if (rsa != NULL && (rsa->flags & RSA_FLAG_EXT_PKEY) != 0)
		return (ISC_TRUE);
	return (ISC_TF(rsa != NULL && rsa->d != NULL));
}

static void
opensslrsa_destroy(dst_key_t *key) {
#if USE_EVP
	EVP_PKEY *pkey = key->keydata.pkey;
	EVP_PKEY_free(pkey);
	key->keydata.pkey = NULL;
#else
	RSA *rsa = key->keydata.rsa;
	RSA_free(rsa);
	key->keydata.rsa = NULL;
#endif
}


static isc_result_t
opensslrsa_todns(const dst_key_t *key, isc_buffer_t *data) {
	isc_region_t r;
	unsigned int e_bytes;
	unsigned int mod_bytes;
	isc_result_t ret;
	RSA *rsa;
#if USE_EVP
	EVP_PKEY *pkey;
#endif

#if USE_EVP
	REQUIRE(key->keydata.pkey != NULL);
#else
	REQUIRE(key->keydata.rsa != NULL);
#endif

#if USE_EVP
	pkey = key->keydata.pkey;
	rsa = EVP_PKEY_get1_RSA(pkey);
	if (rsa == NULL)
		return (dst__openssl_toresult(DST_R_OPENSSLFAILURE));
#else
	rsa = key->keydata.rsa;
#endif

	isc_buffer_availableregion(data, &r);

	e_bytes = BN_num_bytes(rsa->e);
	mod_bytes = BN_num_bytes(rsa->n);

	if (e_bytes < 256) {	/*%< key exponent is <= 2040 bits */
		if (r.length < 1)
			DST_RET(ISC_R_NOSPACE);
		isc_buffer_putuint8(data, (isc_uint8_t) e_bytes);
		isc_region_consume(&r, 1);
	} else {
		if (r.length < 3)
			DST_RET(ISC_R_NOSPACE);
		isc_buffer_putuint8(data, 0);
		isc_buffer_putuint16(data, (isc_uint16_t) e_bytes);
		isc_region_consume(&r, 3);
	}

	if (r.length < e_bytes + mod_bytes)
		DST_RET(ISC_R_NOSPACE);

	BN_bn2bin(rsa->e, r.base);
	isc_region_consume(&r, e_bytes);
	BN_bn2bin(rsa->n, r.base);

	isc_buffer_add(data, e_bytes + mod_bytes);

	ret = ISC_R_SUCCESS;
 err:
#if USE_EVP
	if (rsa != NULL)
		RSA_free(rsa);
#endif
	return (ret);
}

static isc_result_t
opensslrsa_fromdns(dst_key_t *key, isc_buffer_t *data) {
	RSA *rsa;
	isc_region_t r;
	unsigned int e_bytes;
#if USE_EVP
	EVP_PKEY *pkey;
#endif

	isc_buffer_remainingregion(data, &r);
	if (r.length == 0)
		return (ISC_R_SUCCESS);

	rsa = RSA_new();
	if (rsa == NULL)
		return (dst__openssl_toresult(ISC_R_NOMEMORY));
	SET_FLAGS(rsa);

	if (r.length < 1) {
		RSA_free(rsa);
		return (DST_R_INVALIDPUBLICKEY);
	}
	e_bytes = *r.base++;
	r.length--;

	if (e_bytes == 0) {
		if (r.length < 2) {
			RSA_free(rsa);
			return (DST_R_INVALIDPUBLICKEY);
		}
		e_bytes = ((*r.base++) << 8);
		e_bytes += *r.base++;
		r.length -= 2;
	}

	if (r.length < e_bytes) {
		RSA_free(rsa);
		return (DST_R_INVALIDPUBLICKEY);
	}
	rsa->e = BN_bin2bn(r.base, e_bytes, NULL);
	r.base += e_bytes;
	r.length -= e_bytes;

	rsa->n = BN_bin2bn(r.base, r.length, NULL);

	key->key_size = BN_num_bits(rsa->n);

	isc_buffer_forward(data, r.length);

#if USE_EVP
	pkey = EVP_PKEY_new();
	if (pkey == NULL) {
		RSA_free(rsa);
		return (ISC_R_NOMEMORY);
	}
	if (!EVP_PKEY_set1_RSA(pkey, rsa)) {
		EVP_PKEY_free(pkey);
		RSA_free(rsa);
		return (dst__openssl_toresult(DST_R_OPENSSLFAILURE));
	}
	key->keydata.pkey = pkey;
	RSA_free(rsa);
#else
	key->keydata.rsa = rsa;
#endif

	return (ISC_R_SUCCESS);
}

static isc_result_t
opensslrsa_tofile(const dst_key_t *key, const char *directory) {
	int i;
	RSA *rsa;
	dst_private_t priv;
	unsigned char *bufs[8];
	isc_result_t result;

#if USE_EVP
	if (key->keydata.pkey == NULL)
		return (DST_R_NULLKEY);
	rsa = EVP_PKEY_get1_RSA(key->keydata.pkey);
	if (rsa == NULL)
		return (dst__openssl_toresult(DST_R_OPENSSLFAILURE));
#else
	if (key->keydata.rsa == NULL)
		return (DST_R_NULLKEY);
	rsa = key->keydata.rsa;
#endif

	for (i = 0; i < 8; i++) {
		bufs[i] = isc_mem_get(key->mctx, BN_num_bytes(rsa->n));
		if (bufs[i] == NULL) {
			result = ISC_R_NOMEMORY;
			goto fail;
		}
	}

	i = 0;

	priv.elements[i].tag = TAG_RSA_MODULUS;
	priv.elements[i].length = BN_num_bytes(rsa->n);
	BN_bn2bin(rsa->n, bufs[i]);
	priv.elements[i].data = bufs[i];
	i++;

	priv.elements[i].tag = TAG_RSA_PUBLICEXPONENT;
	priv.elements[i].length = BN_num_bytes(rsa->e);
	BN_bn2bin(rsa->e, bufs[i]);
	priv.elements[i].data = bufs[i];
	i++;

	if (rsa->d != NULL) {
		priv.elements[i].tag = TAG_RSA_PRIVATEEXPONENT;
		priv.elements[i].length = BN_num_bytes(rsa->d);
		BN_bn2bin(rsa->d, bufs[i]);
		priv.elements[i].data = bufs[i];
		i++;
	}

	if (rsa->p != NULL) {
		priv.elements[i].tag = TAG_RSA_PRIME1;
		priv.elements[i].length = BN_num_bytes(rsa->p);
		BN_bn2bin(rsa->p, bufs[i]);
		priv.elements[i].data = bufs[i];
		i++;
	}

	if (rsa->q != NULL) {
		priv.elements[i].tag = TAG_RSA_PRIME2;
		priv.elements[i].length = BN_num_bytes(rsa->q);
		BN_bn2bin(rsa->q, bufs[i]);
		priv.elements[i].data = bufs[i];
		i++;
	}

	if (rsa->dmp1 != NULL) {
		priv.elements[i].tag = TAG_RSA_EXPONENT1;
		priv.elements[i].length = BN_num_bytes(rsa->dmp1);
		BN_bn2bin(rsa->dmp1, bufs[i]);
		priv.elements[i].data = bufs[i];
		i++;
	}

	if (rsa->dmq1 != NULL) {
		priv.elements[i].tag = TAG_RSA_EXPONENT2;
		priv.elements[i].length = BN_num_bytes(rsa->dmq1);
		BN_bn2bin(rsa->dmq1, bufs[i]);
		priv.elements[i].data = bufs[i];
		i++;
	}

	if (rsa->iqmp != NULL) {
		priv.elements[i].tag = TAG_RSA_COEFFICIENT;
		priv.elements[i].length = BN_num_bytes(rsa->iqmp);
		BN_bn2bin(rsa->iqmp, bufs[i]);
		priv.elements[i].data = bufs[i];
		i++;
	}

	if (key->engine != NULL) {
		priv.elements[i].tag = TAG_RSA_ENGINE;
		priv.elements[i].length = strlen(key->engine) + 1;
		priv.elements[i].data = (unsigned char *)key->engine;
		i++;
	}

	if (key->label != NULL) {
		priv.elements[i].tag = TAG_RSA_LABEL;
		priv.elements[i].length = strlen(key->label) + 1;
		priv.elements[i].data = (unsigned char *)key->label;
		i++;
	}


	priv.nelements = i;
	result = dst__privstruct_writefile(key, &priv, directory);
 fail:
#if USE_EVP
	RSA_free(rsa);
#endif
	for (i = 0; i < 8; i++) {
		if (bufs[i] == NULL)
			break;
		isc_mem_put(key->mctx, bufs[i], BN_num_bytes(rsa->n));
	}
	return (result);
}

static isc_result_t
rsa_check(RSA *rsa, RSA *pub)
{
	/* Public parameters should be the same but if they are not set
	 * copy them from the public key. */
	if (pub != NULL) {
		if (rsa->n != NULL) {
			if (BN_cmp(rsa->n, pub->n) != 0)
				return (DST_R_INVALIDPRIVATEKEY);
		} else {
			rsa->n = pub->n;
			pub->n = NULL;
		}
		if (rsa->e != NULL) {
			if (BN_cmp(rsa->e, pub->e) != 0)
				return (DST_R_INVALIDPRIVATEKEY);
		} else {
			rsa->e = pub->e;
			pub->e = NULL;
		}
	}
	if (rsa->n == NULL || rsa->e == NULL)
		return (DST_R_INVALIDPRIVATEKEY);
	return (ISC_R_SUCCESS);
}

static isc_result_t
opensslrsa_parse(dst_key_t *key, isc_lex_t *lexer, dst_key_t *pub) {
	dst_private_t priv;
	isc_result_t ret;
	int i;
	RSA *rsa = NULL, *pubrsa = NULL;
#ifdef USE_ENGINE
	ENGINE *e = NULL;
#endif
	isc_mem_t *mctx = key->mctx;
	const char *engine = NULL, *label = NULL;
#if defined(USE_ENGINE) || USE_EVP
	EVP_PKEY *pkey = NULL;
#endif

#if USE_EVP
	if (pub != NULL && pub->keydata.pkey != NULL)
		pubrsa = EVP_PKEY_get1_RSA(pub->keydata.pkey);
#else
	if (pub != NULL && pub->keydata.rsa != NULL) {
		pubrsa = pub->keydata.rsa;
		pub->keydata.rsa = NULL;
	}
#endif

	/* read private key file */
	ret = dst__privstruct_parse(key, DST_ALG_RSA, lexer, mctx, &priv);
	if (ret != ISC_R_SUCCESS)
		return (ret);

	for (i = 0; i < priv.nelements; i++) {
		switch (priv.elements[i].tag) {
		case TAG_RSA_ENGINE:
			engine = (char *)priv.elements[i].data;
			break;
		case TAG_RSA_LABEL:
			label = (char *)priv.elements[i].data;
			break;
		default:
			break;
		}
	}
	/*
	 * Is this key is stored in a HSM?
	 * See if we can fetch it.
	 */
	if (label != NULL) {
#ifdef USE_ENGINE
		if (engine == NULL)
			DST_RET(DST_R_NOENGINE);
		e = dst__openssl_getengine(engine);
		if (e == NULL)
			DST_RET(DST_R_NOENGINE);
		pkey = ENGINE_load_private_key(e, label, NULL, NULL);
		if (pkey == NULL) {
			/* ERR_print_errors_fp(stderr); */
			DST_RET(ISC_R_NOTFOUND);
		}
		key->engine = isc_mem_strdup(key->mctx, engine);
		if (key->engine == NULL)
			DST_RET(ISC_R_NOMEMORY);
		key->label = isc_mem_strdup(key->mctx, label);
		if (key->label == NULL)
			DST_RET(ISC_R_NOMEMORY);
		rsa = EVP_PKEY_get1_RSA(pkey);
		if (rsa == NULL)
			DST_RET(dst__openssl_toresult(DST_R_OPENSSLFAILURE));
		if (rsa_check(rsa, pubrsa) != ISC_R_SUCCESS)
			DST_RET(DST_R_INVALIDPRIVATEKEY);
		if (pubrsa != NULL)
			RSA_free(pubrsa);
		key->key_size = EVP_PKEY_bits(pkey);
#if USE_EVP
		key->keydata.pkey = pkey;
		RSA_free(rsa);
#else
		key->keydata.rsa = rsa;
		EVP_PKEY_free(pkey);
#endif
		dst__privstruct_free(&priv, mctx);
		memset(&priv, 0, sizeof(priv));
		return (ISC_R_SUCCESS);
#else
		DST_RET(DST_R_NOENGINE);
#endif
	}

	rsa = RSA_new();
	if (rsa == NULL)
		DST_RET(ISC_R_NOMEMORY);
	SET_FLAGS(rsa);

#if USE_EVP
	pkey = EVP_PKEY_new();
	if (pkey == NULL)
		DST_RET(ISC_R_NOMEMORY);
	if (!EVP_PKEY_set1_RSA(pkey, rsa))
		DST_RET(ISC_R_FAILURE);
	key->keydata.pkey = pkey;
#else
	key->keydata.rsa = rsa;
#endif

	for (i = 0; i < priv.nelements; i++) {
		BIGNUM *bn;
		switch (priv.elements[i].tag) {
		case TAG_RSA_ENGINE:
			continue;
		case TAG_RSA_LABEL:
			continue;
		case TAG_RSA_PIN:
			continue;
		default:
			bn = BN_bin2bn(priv.elements[i].data,
				       priv.elements[i].length, NULL);
			if (bn == NULL)
				DST_RET(ISC_R_NOMEMORY);
		}

		switch (priv.elements[i].tag) {
			case TAG_RSA_MODULUS:
				rsa->n = bn;
				break;
			case TAG_RSA_PUBLICEXPONENT:
				rsa->e = bn;
				break;
			case TAG_RSA_PRIVATEEXPONENT:
				rsa->d = bn;
				break;
			case TAG_RSA_PRIME1:
				rsa->p = bn;
				break;
			case TAG_RSA_PRIME2:
				rsa->q = bn;
				break;
			case TAG_RSA_EXPONENT1:
				rsa->dmp1 = bn;
				break;
			case TAG_RSA_EXPONENT2:
				rsa->dmq1 = bn;
				break;
			case TAG_RSA_COEFFICIENT:
				rsa->iqmp = bn;
				break;
		}
	}
	dst__privstruct_free(&priv, mctx);
	memset(&priv, 0, sizeof(priv));

	if (rsa_check(rsa, pubrsa) != ISC_R_SUCCESS)
		DST_RET(DST_R_INVALIDPRIVATEKEY);
	key->key_size = BN_num_bits(rsa->n);
	if (pubrsa != NULL)
		RSA_free(pubrsa);
#if USE_EVP
	RSA_free(rsa);
#endif

	return (ISC_R_SUCCESS);

 err:
#if USE_EVP
	if (pkey != NULL)
		EVP_PKEY_free(pkey);
#endif
	if (rsa != NULL)
		RSA_free(rsa);
	if (pubrsa != NULL)
		RSA_free(pubrsa);
	opensslrsa_destroy(key);
	dst__privstruct_free(&priv, mctx);
	memset(&priv, 0, sizeof(priv));
	return (ret);
}

static isc_result_t
opensslrsa_fromlabel(dst_key_t *key, const char *engine, const char *label,
		     const char *pin)
{
#ifdef USE_ENGINE
	ENGINE *e = NULL;
	isc_result_t ret;
	EVP_PKEY *pkey = NULL;
	RSA *rsa = NULL, *pubrsa = NULL;
	char *colon;

	UNUSED(pin);

	if (engine == NULL)
		DST_RET(DST_R_NOENGINE);
	e = dst__openssl_getengine(engine);
	if (e == NULL)
		DST_RET(DST_R_NOENGINE);
	pkey = ENGINE_load_public_key(e, label, NULL, NULL);
	if (pkey != NULL) {
		pubrsa = EVP_PKEY_get1_RSA(pkey);
		EVP_PKEY_free(pkey);
		if (pubrsa == NULL)
			DST_RET(dst__openssl_toresult(DST_R_OPENSSLFAILURE));
	}
	pkey = ENGINE_load_private_key(e, label, NULL, NULL);
	if (pkey == NULL)
		DST_RET(ISC_R_NOTFOUND);
	if (engine != NULL) {
		key->engine = isc_mem_strdup(key->mctx, engine);
		if (key->engine == NULL)
			DST_RET(ISC_R_NOMEMORY);
	} else {
		key->engine = isc_mem_strdup(key->mctx, label);
		if (key->engine == NULL)
			DST_RET(ISC_R_NOMEMORY);
		colon = strchr(key->engine, ':');
		if (colon != NULL)
			*colon = '\0';
	}
	key->label = isc_mem_strdup(key->mctx, label);
	if (key->label == NULL)
		DST_RET(ISC_R_NOMEMORY);
	rsa = EVP_PKEY_get1_RSA(pkey);
	if (rsa == NULL)
		DST_RET(dst__openssl_toresult(DST_R_OPENSSLFAILURE));
	if (rsa_check(rsa, pubrsa) != ISC_R_SUCCESS)
		DST_RET(DST_R_INVALIDPRIVATEKEY);
	if (pubrsa != NULL)
		RSA_free(pubrsa);
	key->key_size = EVP_PKEY_bits(pkey);
#if USE_EVP
	key->keydata.pkey = pkey;
	RSA_free(rsa);
#else
	key->keydata.rsa = rsa;
	EVP_PKEY_free(pkey);
#endif
	return (ISC_R_SUCCESS);

 err:
	if (rsa != NULL)
		RSA_free(rsa);
	if (pubrsa != NULL)
		RSA_free(pubrsa);
	if (pkey != NULL)
		EVP_PKEY_free(pkey);
	return (ret);
#else
	UNUSED(key);
	UNUSED(engine);
	UNUSED(label);
	UNUSED(pin);
	return(DST_R_NOENGINE);
#endif
}

static dst_func_t opensslrsa_functions = {
	opensslrsa_createctx,
	opensslrsa_destroyctx,
	opensslrsa_adddata,
	opensslrsa_sign,
	opensslrsa_verify,
	NULL, /*%< computesecret */
	opensslrsa_compare,
	NULL, /*%< paramcompare */
	opensslrsa_generate,
	opensslrsa_isprivate,
	opensslrsa_destroy,
	opensslrsa_todns,
	opensslrsa_fromdns,
	opensslrsa_tofile,
	opensslrsa_parse,
	NULL, /*%< cleanup */
	opensslrsa_fromlabel,
	NULL, /*%< dump */
	NULL, /*%< restore */
};

isc_result_t
dst__opensslrsa_init(dst_func_t **funcp, unsigned char algorithm) {
	REQUIRE(funcp != NULL);

	if (*funcp == NULL) {
		switch (algorithm) {
		case DST_ALG_RSASHA256:
#if defined(HAVE_EVP_SHA256) || !USE_EVP
			*funcp = &opensslrsa_functions;
#endif
			break;
		case DST_ALG_RSASHA512:
#if defined(HAVE_EVP_SHA512) || !USE_EVP
			*funcp = &opensslrsa_functions;
#endif
			break;
		default:
			*funcp = &opensslrsa_functions;
			break;
		}
	}
	return (ISC_R_SUCCESS);
}

#else /* OPENSSL */

#include <isc/util.h>

EMPTY_TRANSLATION_UNIT

#endif /* OPENSSL */
/*! \file */
