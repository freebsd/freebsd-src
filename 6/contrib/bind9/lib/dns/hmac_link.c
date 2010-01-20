/*
 * Portions Copyright (C) 2004  Internet Systems Consortium, Inc. ("ISC")
 * Portions Copyright (C) 1999-2002  Internet Software Consortium.
 * Portions Copyright (C) 1995-2000 by Network Associates, Inc.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND ISC AND NETWORK ASSOCIATES DISCLAIMS
 * ALL WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS.  IN NO EVENT SHALL ISC BE LIABLE
 * FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR
 * IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/*
 * Principal Author: Brian Wellington
 * $Id: hmac_link.c,v 1.1.4.1 2004/12/09 04:07:17 marka Exp $
 */

#include <config.h>

#include <isc/buffer.h>
#include <isc/hmacmd5.h>
#include <isc/md5.h>
#include <isc/mem.h>
#include <isc/string.h>
#include <isc/util.h>

#include <dst/result.h>

#include "dst_internal.h"
#include "dst_parse.h"

#define HMAC_LEN	64
#define HMAC_IPAD	0x36
#define HMAC_OPAD	0x5c

static isc_result_t hmacmd5_fromdns(dst_key_t *key, isc_buffer_t *data);

typedef struct hmackey {
	unsigned char key[HMAC_LEN];
} HMAC_Key;

static isc_result_t
hmacmd5_createctx(dst_key_t *key, dst_context_t *dctx) {
	isc_hmacmd5_t *hmacmd5ctx;
	HMAC_Key *hkey = key->opaque;

	hmacmd5ctx = isc_mem_get(dctx->mctx, sizeof(isc_hmacmd5_t));
	if (hmacmd5ctx == NULL)
		return (ISC_R_NOMEMORY);
	isc_hmacmd5_init(hmacmd5ctx, hkey->key, HMAC_LEN);
	dctx->opaque = hmacmd5ctx;
	return (ISC_R_SUCCESS);
}

static void
hmacmd5_destroyctx(dst_context_t *dctx) {
	isc_hmacmd5_t *hmacmd5ctx = dctx->opaque;

	if (hmacmd5ctx != NULL) {
		isc_hmacmd5_invalidate(hmacmd5ctx);
		isc_mem_put(dctx->mctx, hmacmd5ctx, sizeof(isc_hmacmd5_t));
		dctx->opaque = NULL;
	}
}

static isc_result_t
hmacmd5_adddata(dst_context_t *dctx, const isc_region_t *data) {
	isc_hmacmd5_t *hmacmd5ctx = dctx->opaque;

	isc_hmacmd5_update(hmacmd5ctx, data->base, data->length);
	return (ISC_R_SUCCESS);
}

static isc_result_t
hmacmd5_sign(dst_context_t *dctx, isc_buffer_t *sig) {
	isc_hmacmd5_t *hmacmd5ctx = dctx->opaque;
	unsigned char *digest;

	if (isc_buffer_availablelength(sig) < ISC_MD5_DIGESTLENGTH)
		return (ISC_R_NOSPACE);
	digest = isc_buffer_used(sig);
	isc_hmacmd5_sign(hmacmd5ctx, digest);
	isc_buffer_add(sig, ISC_MD5_DIGESTLENGTH);

	return (ISC_R_SUCCESS);
}

static isc_result_t
hmacmd5_verify(dst_context_t *dctx, const isc_region_t *sig) {
	isc_hmacmd5_t *hmacmd5ctx = dctx->opaque;

	if (sig->length < ISC_MD5_DIGESTLENGTH)
		return (DST_R_VERIFYFAILURE);

	if (isc_hmacmd5_verify(hmacmd5ctx, sig->base))
		return (ISC_R_SUCCESS);
	else
		return (DST_R_VERIFYFAILURE);
}

static isc_boolean_t
hmacmd5_compare(const dst_key_t *key1, const dst_key_t *key2) {
	HMAC_Key *hkey1, *hkey2;

	hkey1 = (HMAC_Key *)key1->opaque;
	hkey2 = (HMAC_Key *)key2->opaque;

	if (hkey1 == NULL && hkey2 == NULL)
		return (ISC_TRUE);
	else if (hkey1 == NULL || hkey2 == NULL)
		return (ISC_FALSE);

	if (memcmp(hkey1->key, hkey2->key, HMAC_LEN) == 0)
		return (ISC_TRUE);
	else
		return (ISC_FALSE);
}

static isc_result_t
hmacmd5_generate(dst_key_t *key, int pseudorandom_ok) {
	isc_buffer_t b;
	isc_result_t ret;
	int bytes;
	unsigned char data[HMAC_LEN];

	bytes = (key->key_size + 7) / 8;
	if (bytes > 64) {
		bytes = 64;
		key->key_size = 512;
	}

	memset(data, 0, HMAC_LEN);
	ret = dst__entropy_getdata(data, bytes, ISC_TF(pseudorandom_ok != 0));

	if (ret != ISC_R_SUCCESS)
		return (ret);

	isc_buffer_init(&b, data, bytes);
	isc_buffer_add(&b, bytes);
	ret = hmacmd5_fromdns(key, &b);
	memset(data, 0, HMAC_LEN);

	return (ret);
}

static isc_boolean_t
hmacmd5_isprivate(const dst_key_t *key) {
	UNUSED(key);
	return (ISC_TRUE);
}

static void
hmacmd5_destroy(dst_key_t *key) {
	HMAC_Key *hkey = key->opaque;
	memset(hkey, 0, sizeof(HMAC_Key));
	isc_mem_put(key->mctx, hkey, sizeof(HMAC_Key));
	key->opaque = NULL;
}

static isc_result_t
hmacmd5_todns(const dst_key_t *key, isc_buffer_t *data) {
	HMAC_Key *hkey;
	unsigned int bytes;

	REQUIRE(key->opaque != NULL);

	hkey = (HMAC_Key *) key->opaque;

	bytes = (key->key_size + 7) / 8;
	if (isc_buffer_availablelength(data) < bytes)
		return (ISC_R_NOSPACE);
	isc_buffer_putmem(data, hkey->key, bytes);

	return (ISC_R_SUCCESS);
}

static isc_result_t
hmacmd5_fromdns(dst_key_t *key, isc_buffer_t *data) {
	HMAC_Key *hkey;
	int keylen;
	isc_region_t r;
	isc_md5_t md5ctx;

	isc_buffer_remainingregion(data, &r);
	if (r.length == 0)
		return (ISC_R_SUCCESS);

	hkey = (HMAC_Key *) isc_mem_get(key->mctx, sizeof(HMAC_Key));
	if (hkey == NULL)
		return (ISC_R_NOMEMORY);

	memset(hkey->key, 0, sizeof(hkey->key));

	if (r.length > HMAC_LEN) {
		isc_md5_init(&md5ctx);
		isc_md5_update(&md5ctx, r.base, r.length);
		isc_md5_final(&md5ctx, hkey->key);
		keylen = ISC_MD5_DIGESTLENGTH;
	}
	else {
		memcpy(hkey->key, r.base, r.length);
		keylen = r.length;
	}

	key->key_size = keylen * 8;
	key->opaque = hkey;

	return (ISC_R_SUCCESS);
}

static isc_result_t
hmacmd5_tofile(const dst_key_t *key, const char *directory) {
	int cnt = 0;
	HMAC_Key *hkey;
	dst_private_t priv;
	int bytes = (key->key_size + 7) / 8;

	if (key->opaque == NULL)
		return (DST_R_NULLKEY);

	hkey = (HMAC_Key *) key->opaque;

	priv.elements[cnt].tag = TAG_HMACMD5_KEY;
	priv.elements[cnt].length = bytes;
	priv.elements[cnt++].data = hkey->key;

	priv.nelements = cnt;
	return (dst__privstruct_writefile(key, &priv, directory));
}

static isc_result_t
hmacmd5_parse(dst_key_t *key, isc_lex_t *lexer) {
	dst_private_t priv;
	isc_result_t ret;
	isc_buffer_t b;
	isc_mem_t *mctx = key->mctx;

	/* read private key file */
	ret = dst__privstruct_parse(key, DST_ALG_HMACMD5, lexer, mctx, &priv);
	if (ret != ISC_R_SUCCESS)
		return (ret);

	isc_buffer_init(&b, priv.elements[0].data, priv.elements[0].length);
	isc_buffer_add(&b, priv.elements[0].length);
	ret = hmacmd5_fromdns(key, &b);
	dst__privstruct_free(&priv, mctx);
	memset(&priv, 0, sizeof(priv));
	return (ret);
}

static dst_func_t hmacmd5_functions = {
	hmacmd5_createctx,
	hmacmd5_destroyctx,
	hmacmd5_adddata,
	hmacmd5_sign,
	hmacmd5_verify,
	NULL, /* computesecret */
	hmacmd5_compare,
	NULL, /* paramcompare */
	hmacmd5_generate,
	hmacmd5_isprivate,
	hmacmd5_destroy,
	hmacmd5_todns,
	hmacmd5_fromdns,
	hmacmd5_tofile,
	hmacmd5_parse,
	NULL, /* cleanup */
};

isc_result_t
dst__hmacmd5_init(dst_func_t **funcp) {
	REQUIRE(funcp != NULL);
	if (*funcp == NULL)
		*funcp = &hmacmd5_functions;
	return (ISC_R_SUCCESS);
}
