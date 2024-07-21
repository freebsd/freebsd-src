/*
 * Wrapper functions for OpenSSL libcrypto
 * Copyright (c) 2004-2024, Jouni Malinen <j@w1.fi>
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 */

#include "includes.h"
#include <openssl/opensslv.h>
#include <openssl/err.h>
#include <openssl/des.h>
#include <openssl/aes.h>
#include <openssl/bn.h>
#include <openssl/evp.h>
#include <openssl/dh.h>
#include <openssl/hmac.h>
#include <openssl/rand.h>
#include <openssl/rsa.h>
#include <openssl/pem.h>
#ifdef CONFIG_ECC
#include <openssl/ec.h>
#include <openssl/x509.h>
#endif /* CONFIG_ECC */
#if OPENSSL_VERSION_NUMBER >= 0x30000000L
#include <openssl/provider.h>
#include <openssl/core_names.h>
#include <openssl/param_build.h>
#include <openssl/encoder.h>
#include <openssl/decoder.h>
#else /* OpenSSL version >= 3.0 */
#include <openssl/cmac.h>
#endif /* OpenSSL version >= 3.0 */
#ifdef CONFIG_DPP3
#if OPENSSL_VERSION_NUMBER >= 0x30200000L
#include <openssl/hpke.h>
#endif
#endif /* CONFIG_DPP3 */

#include "common.h"
#include "utils/const_time.h"
#include "wpabuf.h"
#include "dh_group5.h"
#include "sha1.h"
#include "sha256.h"
#include "sha384.h"
#include "sha512.h"
#include "md5.h"
#include "aes_wrap.h"
#include "crypto.h"

#if OPENSSL_VERSION_NUMBER < 0x10100000L
/* Compatibility wrappers for older versions. */

static HMAC_CTX * HMAC_CTX_new(void)
{
	HMAC_CTX *ctx;

	ctx = os_zalloc(sizeof(*ctx));
	if (ctx)
		HMAC_CTX_init(ctx);
	return ctx;
}


static void HMAC_CTX_free(HMAC_CTX *ctx)
{
	if (!ctx)
		return;
	HMAC_CTX_cleanup(ctx);
	bin_clear_free(ctx, sizeof(*ctx));
}


static EVP_MD_CTX * EVP_MD_CTX_new(void)
{
	EVP_MD_CTX *ctx;

	ctx = os_zalloc(sizeof(*ctx));
	if (ctx)
		EVP_MD_CTX_init(ctx);
	return ctx;
}


static void EVP_MD_CTX_free(EVP_MD_CTX *ctx)
{
	if (!ctx)
		return;
	EVP_MD_CTX_cleanup(ctx);
	bin_clear_free(ctx, sizeof(*ctx));
}


#ifdef CONFIG_ECC

static EC_KEY * EVP_PKEY_get0_EC_KEY(EVP_PKEY *pkey)
{
	if (pkey->type != EVP_PKEY_EC)
		return NULL;
	return pkey->pkey.ec;
}


static int ECDSA_SIG_set0(ECDSA_SIG *sig, BIGNUM *r, BIGNUM *s)
{
	sig->r = r;
	sig->s = s;
	return 1;
}


static void ECDSA_SIG_get0(const ECDSA_SIG *sig, const BIGNUM **pr,
			   const BIGNUM **ps)
{
	if (pr)
		*pr = sig->r;
	if (ps)
		*ps = sig->s;
}

#endif /* CONFIG_ECC */

static const unsigned char * ASN1_STRING_get0_data(const ASN1_STRING *x)
{
	return ASN1_STRING_data((ASN1_STRING *) x);
}


static const ASN1_TIME * X509_get0_notBefore(const X509 *x)
{
	return X509_get_notBefore(x);
}


static const ASN1_TIME * X509_get0_notAfter(const X509 *x)
{
	return X509_get_notAfter(x);
}

#endif /* OpenSSL version < 1.1.0 */


#if OPENSSL_VERSION_NUMBER < 0x10101000L || \
	(defined(LIBRESSL_VERSION_NUMBER) && \
	 LIBRESSL_VERSION_NUMBER < 0x30400000L)

static int EC_POINT_get_affine_coordinates(const EC_GROUP *group,
					   const EC_POINT *point, BIGNUM *x,
					   BIGNUM *y, BN_CTX *ctx)
{
	return EC_POINT_get_affine_coordinates_GFp(group, point, x, y, ctx);
}


static int EC_POINT_set_affine_coordinates(const EC_GROUP *group,
					   EC_POINT *point, const BIGNUM *x,
					   const BIGNUM *y, BN_CTX *ctx)
{
	return EC_POINT_set_affine_coordinates_GFp(group, point, x, y, ctx);
}

#endif /* OpenSSL version < 1.1.1 */


#if OPENSSL_VERSION_NUMBER < 0x10101000L || \
	defined(OPENSSL_IS_BORINGSSL) || \
	(defined(LIBRESSL_VERSION_NUMBER) && \
	 LIBRESSL_VERSION_NUMBER < 0x30400000L)

static int EC_POINT_set_compressed_coordinates(const EC_GROUP *group,
					       EC_POINT *point, const BIGNUM *x,
					       int y_bit, BN_CTX *ctx)
{
	return EC_POINT_set_compressed_coordinates_GFp(group, point, x, y_bit,
						       ctx);
}


static int EC_GROUP_get_curve(const EC_GROUP *group, BIGNUM *p, BIGNUM *a,
			      BIGNUM *b, BN_CTX *ctx)
{
	return EC_GROUP_get_curve_GFp(group, p, a, b, ctx);
}

#endif /* OpenSSL version < 1.1.1 */


#if OPENSSL_VERSION_NUMBER >= 0x30000000L
static OSSL_PROVIDER *openssl_legacy_provider = NULL;
#endif /* OpenSSL version >= 3.0 */

void openssl_load_legacy_provider(void)
{
#if OPENSSL_VERSION_NUMBER >= 0x30000000L
	if (openssl_legacy_provider)
		return;

	openssl_legacy_provider = OSSL_PROVIDER_try_load(NULL, "legacy", 1);
#endif /* OpenSSL version >= 3.0 */
}


static void openssl_unload_legacy_provider(void)
{
#if OPENSSL_VERSION_NUMBER >= 0x30000000L
	if (openssl_legacy_provider) {
		OSSL_PROVIDER_unload(openssl_legacy_provider);
		openssl_legacy_provider = NULL;
	}
#endif /* OpenSSL version >= 3.0 */
}


#if OPENSSL_VERSION_NUMBER < 0x30000000L

static BIGNUM * get_group5_prime(void)
{
#if OPENSSL_VERSION_NUMBER >= 0x10100000L
	return BN_get_rfc3526_prime_1536(NULL);
#elif !defined(OPENSSL_IS_BORINGSSL)
	return get_rfc3526_prime_1536(NULL);
#else
	static const unsigned char RFC3526_PRIME_1536[] = {
		0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xC9,0x0F,0xDA,0xA2,
		0x21,0x68,0xC2,0x34,0xC4,0xC6,0x62,0x8B,0x80,0xDC,0x1C,0xD1,
		0x29,0x02,0x4E,0x08,0x8A,0x67,0xCC,0x74,0x02,0x0B,0xBE,0xA6,
		0x3B,0x13,0x9B,0x22,0x51,0x4A,0x08,0x79,0x8E,0x34,0x04,0xDD,
		0xEF,0x95,0x19,0xB3,0xCD,0x3A,0x43,0x1B,0x30,0x2B,0x0A,0x6D,
		0xF2,0x5F,0x14,0x37,0x4F,0xE1,0x35,0x6D,0x6D,0x51,0xC2,0x45,
		0xE4,0x85,0xB5,0x76,0x62,0x5E,0x7E,0xC6,0xF4,0x4C,0x42,0xE9,
		0xA6,0x37,0xED,0x6B,0x0B,0xFF,0x5C,0xB6,0xF4,0x06,0xB7,0xED,
		0xEE,0x38,0x6B,0xFB,0x5A,0x89,0x9F,0xA5,0xAE,0x9F,0x24,0x11,
		0x7C,0x4B,0x1F,0xE6,0x49,0x28,0x66,0x51,0xEC,0xE4,0x5B,0x3D,
		0xC2,0x00,0x7C,0xB8,0xA1,0x63,0xBF,0x05,0x98,0xDA,0x48,0x36,
		0x1C,0x55,0xD3,0x9A,0x69,0x16,0x3F,0xA8,0xFD,0x24,0xCF,0x5F,
		0x83,0x65,0x5D,0x23,0xDC,0xA3,0xAD,0x96,0x1C,0x62,0xF3,0x56,
		0x20,0x85,0x52,0xBB,0x9E,0xD5,0x29,0x07,0x70,0x96,0x96,0x6D,
		0x67,0x0C,0x35,0x4E,0x4A,0xBC,0x98,0x04,0xF1,0x74,0x6C,0x08,
		0xCA,0x23,0x73,0x27,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
	};
        return BN_bin2bn(RFC3526_PRIME_1536, sizeof(RFC3526_PRIME_1536), NULL);
#endif
}


static BIGNUM * get_group5_order(void)
{
	static const unsigned char RFC3526_ORDER_1536[] = {
		0x7F,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xE4,0x87,0xED,0x51,
		0x10,0xB4,0x61,0x1A,0x62,0x63,0x31,0x45,0xC0,0x6E,0x0E,0x68,
		0x94,0x81,0x27,0x04,0x45,0x33,0xE6,0x3A,0x01,0x05,0xDF,0x53,
		0x1D,0x89,0xCD,0x91,0x28,0xA5,0x04,0x3C,0xC7,0x1A,0x02,0x6E,
		0xF7,0xCA,0x8C,0xD9,0xE6,0x9D,0x21,0x8D,0x98,0x15,0x85,0x36,
		0xF9,0x2F,0x8A,0x1B,0xA7,0xF0,0x9A,0xB6,0xB6,0xA8,0xE1,0x22,
		0xF2,0x42,0xDA,0xBB,0x31,0x2F,0x3F,0x63,0x7A,0x26,0x21,0x74,
		0xD3,0x1B,0xF6,0xB5,0x85,0xFF,0xAE,0x5B,0x7A,0x03,0x5B,0xF6,
		0xF7,0x1C,0x35,0xFD,0xAD,0x44,0xCF,0xD2,0xD7,0x4F,0x92,0x08,
		0xBE,0x25,0x8F,0xF3,0x24,0x94,0x33,0x28,0xF6,0x72,0x2D,0x9E,
		0xE1,0x00,0x3E,0x5C,0x50,0xB1,0xDF,0x82,0xCC,0x6D,0x24,0x1B,
		0x0E,0x2A,0xE9,0xCD,0x34,0x8B,0x1F,0xD4,0x7E,0x92,0x67,0xAF,
		0xC1,0xB2,0xAE,0x91,0xEE,0x51,0xD6,0xCB,0x0E,0x31,0x79,0xAB,
		0x10,0x42,0xA9,0x5D,0xCF,0x6A,0x94,0x83,0xB8,0x4B,0x4B,0x36,
		0xB3,0x86,0x1A,0xA7,0x25,0x5E,0x4C,0x02,0x78,0xBA,0x36,0x04,
		0x65,0x11,0xB9,0x93,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF
	};
	return BN_bin2bn(RFC3526_ORDER_1536, sizeof(RFC3526_ORDER_1536), NULL);
}

#endif /* OpenSSL version < 3.0 */


#ifdef OPENSSL_NO_SHA256
#define NO_SHA256_WRAPPER
#endif
#ifdef OPENSSL_NO_SHA512
#define NO_SHA384_WRAPPER
#endif

static int openssl_digest_vector(const EVP_MD *type, size_t num_elem,
				 const u8 *addr[], const size_t *len, u8 *mac)
{
	EVP_MD_CTX *ctx;
	size_t i;
	unsigned int mac_len;

	if (TEST_FAIL())
		return -1;

	ctx = EVP_MD_CTX_new();
	if (!ctx)
		return -1;
	if (!EVP_DigestInit_ex(ctx, type, NULL)) {
		wpa_printf(MSG_ERROR, "OpenSSL: EVP_DigestInit_ex failed: %s",
			   ERR_error_string(ERR_get_error(), NULL));
		EVP_MD_CTX_free(ctx);
		return -1;
	}
	for (i = 0; i < num_elem; i++) {
		if (!EVP_DigestUpdate(ctx, addr[i], len[i])) {
			wpa_printf(MSG_ERROR, "OpenSSL: EVP_DigestUpdate "
				   "failed: %s",
				   ERR_error_string(ERR_get_error(), NULL));
			EVP_MD_CTX_free(ctx);
			return -1;
		}
	}
	if (!EVP_DigestFinal(ctx, mac, &mac_len)) {
		wpa_printf(MSG_ERROR, "OpenSSL: EVP_DigestFinal failed: %s",
			   ERR_error_string(ERR_get_error(), NULL));
		EVP_MD_CTX_free(ctx);
		return -1;
	}
	EVP_MD_CTX_free(ctx);

	return 0;
}


#ifndef CONFIG_FIPS

int md4_vector(size_t num_elem, const u8 *addr[], const size_t *len, u8 *mac)
{
	openssl_load_legacy_provider();
	return openssl_digest_vector(EVP_md4(), num_elem, addr, len, mac);
}


int des_encrypt(const u8 *clear, const u8 *key, u8 *cypher)
{
	u8 pkey[8], next, tmp;
	int i, plen, ret = -1;
	EVP_CIPHER_CTX *ctx;

	openssl_load_legacy_provider();

	/* Add parity bits to the key */
	next = 0;
	for (i = 0; i < 7; i++) {
		tmp = key[i];
		pkey[i] = (tmp >> i) | next | 1;
		next = tmp << (7 - i);
	}
	pkey[i] = next | 1;

	ctx = EVP_CIPHER_CTX_new();
	if (ctx &&
	    EVP_EncryptInit_ex(ctx, EVP_des_ecb(), NULL, pkey, NULL) == 1 &&
	    EVP_CIPHER_CTX_set_padding(ctx, 0) == 1 &&
	    EVP_EncryptUpdate(ctx, cypher, &plen, clear, 8) == 1 &&
	    EVP_EncryptFinal_ex(ctx, &cypher[plen], &plen) == 1)
		ret = 0;
	else
		wpa_printf(MSG_ERROR, "OpenSSL: DES encrypt failed");

	if (ctx)
		EVP_CIPHER_CTX_free(ctx);
	return ret;
}


#ifndef CONFIG_NO_RC4
int rc4_skip(const u8 *key, size_t keylen, size_t skip,
	     u8 *data, size_t data_len)
{
#ifdef OPENSSL_NO_RC4
	return -1;
#else /* OPENSSL_NO_RC4 */
	EVP_CIPHER_CTX *ctx;
	int outl;
	int res = -1;
	unsigned char skip_buf[16];

	openssl_load_legacy_provider();

	ctx = EVP_CIPHER_CTX_new();
	if (!ctx ||
	    !EVP_CipherInit_ex(ctx, EVP_rc4(), NULL, NULL, NULL, 1) ||
	    !EVP_CIPHER_CTX_set_padding(ctx, 0) ||
	    !EVP_CIPHER_CTX_set_key_length(ctx, keylen) ||
	    !EVP_CipherInit_ex(ctx, NULL, NULL, key, NULL, 1))
		goto out;

	while (skip >= sizeof(skip_buf)) {
		size_t len = skip;
		if (len > sizeof(skip_buf))
			len = sizeof(skip_buf);
		if (!EVP_CipherUpdate(ctx, skip_buf, &outl, skip_buf, len))
			goto out;
		skip -= len;
	}

	if (EVP_CipherUpdate(ctx, data, &outl, data, data_len))
		res = 0;

out:
	if (ctx)
		EVP_CIPHER_CTX_free(ctx);
	return res;
#endif /* OPENSSL_NO_RC4 */
}
#endif /* CONFIG_NO_RC4 */


int md5_vector(size_t num_elem, const u8 *addr[], const size_t *len, u8 *mac)
{
	return openssl_digest_vector(EVP_md5(), num_elem, addr, len, mac);
}

#endif /* CONFIG_FIPS */


int sha1_vector(size_t num_elem, const u8 *addr[], const size_t *len, u8 *mac)
{
	return openssl_digest_vector(EVP_sha1(), num_elem, addr, len, mac);
}


#ifndef NO_SHA256_WRAPPER
int sha256_vector(size_t num_elem, const u8 *addr[], const size_t *len,
		  u8 *mac)
{
	return openssl_digest_vector(EVP_sha256(), num_elem, addr, len, mac);
}
#endif /* NO_SHA256_WRAPPER */


#ifndef NO_SHA384_WRAPPER
int sha384_vector(size_t num_elem, const u8 *addr[], const size_t *len,
		  u8 *mac)
{
	return openssl_digest_vector(EVP_sha384(), num_elem, addr, len, mac);
}
#endif /* NO_SHA384_WRAPPER */


#ifndef NO_SHA512_WRAPPER
int sha512_vector(size_t num_elem, const u8 *addr[], const size_t *len,
		  u8 *mac)
{
	return openssl_digest_vector(EVP_sha512(), num_elem, addr, len, mac);
}
#endif /* NO_SHA512_WRAPPER */


static const EVP_CIPHER * aes_get_evp_cipher(size_t keylen)
{
	switch (keylen) {
	case 16:
		return EVP_aes_128_ecb();
	case 24:
		return EVP_aes_192_ecb();
	case 32:
		return EVP_aes_256_ecb();
	default:
		return NULL;
	}
}


void * aes_encrypt_init(const u8 *key, size_t len)
{
	EVP_CIPHER_CTX *ctx;
	const EVP_CIPHER *type;

	if (TEST_FAIL())
		return NULL;

	type = aes_get_evp_cipher(len);
	if (!type) {
		wpa_printf(MSG_INFO, "%s: Unsupported len=%u",
			   __func__, (unsigned int) len);
		return NULL;
	}

	ctx = EVP_CIPHER_CTX_new();
	if (ctx == NULL)
		return NULL;
	if (EVP_EncryptInit_ex(ctx, type, NULL, key, NULL) != 1 ||
	    EVP_CIPHER_CTX_set_padding(ctx, 0) != 1) {
		EVP_CIPHER_CTX_free(ctx);
		return NULL;
	}
	return ctx;
}


int aes_encrypt(void *ctx, const u8 *plain, u8 *crypt)
{
	EVP_CIPHER_CTX *c = ctx;
	int clen = 16;
	if (EVP_EncryptUpdate(c, crypt, &clen, plain, 16) != 1) {
		wpa_printf(MSG_ERROR, "OpenSSL: EVP_EncryptUpdate failed: %s",
			   ERR_error_string(ERR_get_error(), NULL));
		return -1;
	}
	return 0;
}


void aes_encrypt_deinit(void *ctx)
{
	EVP_CIPHER_CTX *c = ctx;
	u8 buf[16];
	int len = sizeof(buf);
	if (EVP_EncryptFinal_ex(c, buf, &len) != 1) {
		wpa_printf(MSG_ERROR, "OpenSSL: EVP_EncryptFinal_ex failed: "
			   "%s", ERR_error_string(ERR_get_error(), NULL));
	}
	if (len != 0) {
		wpa_printf(MSG_ERROR, "OpenSSL: Unexpected padding length %d "
			   "in AES encrypt", len);
	}
	EVP_CIPHER_CTX_free(c);
}


void * aes_decrypt_init(const u8 *key, size_t len)
{
	EVP_CIPHER_CTX *ctx;
	const EVP_CIPHER *type;

	if (TEST_FAIL())
		return NULL;

	type = aes_get_evp_cipher(len);
	if (!type) {
		wpa_printf(MSG_INFO, "%s: Unsupported len=%u",
			   __func__, (unsigned int) len);
		return NULL;
	}

	ctx = EVP_CIPHER_CTX_new();
	if (ctx == NULL)
		return NULL;
	if (EVP_DecryptInit_ex(ctx, type, NULL, key, NULL) != 1 ||
	    EVP_CIPHER_CTX_set_padding(ctx, 0) != 1) {
		EVP_CIPHER_CTX_free(ctx);
		return NULL;
	}
	return ctx;
}


int aes_decrypt(void *ctx, const u8 *crypt, u8 *plain)
{
	EVP_CIPHER_CTX *c = ctx;
	int plen = 16;
	if (EVP_DecryptUpdate(c, plain, &plen, crypt, 16) != 1) {
		wpa_printf(MSG_ERROR, "OpenSSL: EVP_DecryptUpdate failed: %s",
			   ERR_error_string(ERR_get_error(), NULL));
		return -1;
	}
	return 0;
}


void aes_decrypt_deinit(void *ctx)
{
	EVP_CIPHER_CTX *c = ctx;
	u8 buf[16];
	int len = sizeof(buf);
	if (EVP_DecryptFinal_ex(c, buf, &len) != 1) {
		wpa_printf(MSG_ERROR, "OpenSSL: EVP_DecryptFinal_ex failed: "
			   "%s", ERR_error_string(ERR_get_error(), NULL));
	}
	if (len != 0) {
		wpa_printf(MSG_ERROR, "OpenSSL: Unexpected padding length %d "
			   "in AES decrypt", len);
	}
	EVP_CIPHER_CTX_free(c);
}


#ifndef CONFIG_FIPS
#ifndef CONFIG_OPENSSL_INTERNAL_AES_WRAP

#if OPENSSL_VERSION_NUMBER >= 0x30000000L
static const EVP_CIPHER * aes_get_evp_wrap_cipher(size_t keylen)
{
	switch (keylen) {
	case 16:
		return EVP_aes_128_wrap();
	case 24:
		return EVP_aes_192_wrap();
	case 32:
		return EVP_aes_256_wrap();
	default:
		return NULL;
	}
}
#endif /* OpenSSL version >= 3.0 */


int aes_wrap(const u8 *kek, size_t kek_len, int n, const u8 *plain, u8 *cipher)
{
#if OPENSSL_VERSION_NUMBER >= 0x30000000L
	EVP_CIPHER_CTX *ctx;
	const EVP_CIPHER *type;
	int ret = -1, len;
	u8 buf[16];

	if (TEST_FAIL())
		return -1;

	type = aes_get_evp_wrap_cipher(kek_len);
	if (!type)
		return -1;

	ctx = EVP_CIPHER_CTX_new();
	if (!ctx)
		return -1;

	if (EVP_EncryptInit_ex(ctx, type, NULL, kek, NULL) == 1 &&
	    EVP_CIPHER_CTX_set_padding(ctx, 0) == 1 &&
	    EVP_EncryptUpdate(ctx, cipher, &len, plain, n * 8) == 1 &&
	    len == (n + 1) * 8 &&
	    EVP_EncryptFinal_ex(ctx, buf, &len) == 1)
		ret = 0;

	EVP_CIPHER_CTX_free(ctx);
	return ret;
#else /* OpenSSL version >= 3.0 */
	AES_KEY actx;
	int res;

	if (TEST_FAIL())
		return -1;
	if (AES_set_encrypt_key(kek, kek_len << 3, &actx))
		return -1;
	res = AES_wrap_key(&actx, NULL, cipher, plain, n * 8);
	OPENSSL_cleanse(&actx, sizeof(actx));
	return res <= 0 ? -1 : 0;
#endif /* OpenSSL version >= 3.0 */
}


int aes_unwrap(const u8 *kek, size_t kek_len, int n, const u8 *cipher,
	       u8 *plain)
{
#if OPENSSL_VERSION_NUMBER >= 0x30000000L
	EVP_CIPHER_CTX *ctx;
	const EVP_CIPHER *type;
	int ret = -1, len;
	u8 buf[16];

	if (TEST_FAIL())
		return -1;

	type = aes_get_evp_wrap_cipher(kek_len);
	if (!type)
		return -1;

	ctx = EVP_CIPHER_CTX_new();
	if (!ctx)
		return -1;

	if (EVP_DecryptInit_ex(ctx, type, NULL, kek, NULL) == 1 &&
	    EVP_CIPHER_CTX_set_padding(ctx, 0) == 1 &&
	    EVP_DecryptUpdate(ctx, plain, &len, cipher, (n + 1) * 8) == 1 &&
	    len == n * 8 &&
	    EVP_DecryptFinal_ex(ctx, buf, &len) == 1)
		ret = 0;

	EVP_CIPHER_CTX_free(ctx);
	return ret;
#else /* OpenSSL version >= 3.0 */
	AES_KEY actx;
	int res;

	if (TEST_FAIL())
		return -1;
	if (AES_set_decrypt_key(kek, kek_len << 3, &actx))
		return -1;
	res = AES_unwrap_key(&actx, NULL, plain, cipher, (n + 1) * 8);
	OPENSSL_cleanse(&actx, sizeof(actx));
	return res <= 0 ? -1 : 0;
#endif /* OpenSSL version >= 3.0 */
}

#endif /* CONFIG_OPENSSL_INTERNAL_AES_WRAP */
#endif /* CONFIG_FIPS */


int aes_128_cbc_encrypt(const u8 *key, const u8 *iv, u8 *data, size_t data_len)
{
	EVP_CIPHER_CTX *ctx;
	int clen, len;
	u8 buf[16];
	int res = -1;

	if (TEST_FAIL())
		return -1;

	ctx = EVP_CIPHER_CTX_new();
	if (!ctx)
		return -1;
	clen = data_len;
	len = sizeof(buf);
	if (EVP_EncryptInit_ex(ctx, EVP_aes_128_cbc(), NULL, key, iv) == 1 &&
	    EVP_CIPHER_CTX_set_padding(ctx, 0) == 1 &&
	    EVP_EncryptUpdate(ctx, data, &clen, data, data_len) == 1 &&
	    clen == (int) data_len &&
	    EVP_EncryptFinal_ex(ctx, buf, &len) == 1 && len == 0)
		res = 0;
	EVP_CIPHER_CTX_free(ctx);

	return res;
}


int aes_128_cbc_decrypt(const u8 *key, const u8 *iv, u8 *data, size_t data_len)
{
	EVP_CIPHER_CTX *ctx;
	int plen, len;
	u8 buf[16];
	int res = -1;

	if (TEST_FAIL())
		return -1;

	ctx = EVP_CIPHER_CTX_new();
	if (!ctx)
		return -1;
	plen = data_len;
	len = sizeof(buf);
	if (EVP_DecryptInit_ex(ctx, EVP_aes_128_cbc(), NULL, key, iv) == 1 &&
	    EVP_CIPHER_CTX_set_padding(ctx, 0) == 1 &&
	    EVP_DecryptUpdate(ctx, data, &plen, data, data_len) == 1 &&
	    plen == (int) data_len &&
	    EVP_DecryptFinal_ex(ctx, buf, &len) == 1 && len == 0)
		res = 0;
	EVP_CIPHER_CTX_free(ctx);

	return res;

}


int crypto_dh_init(u8 generator, const u8 *prime, size_t prime_len, u8 *privkey,
		   u8 *pubkey)
{
	size_t pubkey_len, pad;

	if (os_get_random(privkey, prime_len) < 0)
		return -1;
	if (os_memcmp(privkey, prime, prime_len) > 0) {
		/* Make sure private value is smaller than prime */
		privkey[0] = 0;
	}

	pubkey_len = prime_len;
	if (crypto_mod_exp(&generator, 1, privkey, prime_len, prime, prime_len,
			   pubkey, &pubkey_len) < 0)
		return -1;
	if (pubkey_len < prime_len) {
		pad = prime_len - pubkey_len;
		os_memmove(pubkey + pad, pubkey, pubkey_len);
		os_memset(pubkey, 0, pad);
	}

	return 0;
}


int crypto_dh_derive_secret(u8 generator, const u8 *prime, size_t prime_len,
			    const u8 *order, size_t order_len,
			    const u8 *privkey, size_t privkey_len,
			    const u8 *pubkey, size_t pubkey_len,
			    u8 *secret, size_t *len)
{
	BIGNUM *pub, *p;
	int res = -1;

	pub = BN_bin2bn(pubkey, pubkey_len, NULL);
	p = BN_bin2bn(prime, prime_len, NULL);
	if (!pub || !p || BN_is_zero(pub) || BN_is_one(pub) ||
	    BN_cmp(pub, p) >= 0)
		goto fail;

	if (order) {
		BN_CTX *ctx;
		BIGNUM *q, *tmp;
		int failed;

		/* verify: pubkey^q == 1 mod p */
		q = BN_bin2bn(order, order_len, NULL);
		ctx = BN_CTX_new();
		tmp = BN_new();
		failed = !q || !ctx || !tmp ||
			!BN_mod_exp(tmp, pub, q, p, ctx) ||
			!BN_is_one(tmp);
		BN_clear_free(q);
		BN_clear_free(tmp);
		BN_CTX_free(ctx);
		if (failed)
			goto fail;
	}

	res = crypto_mod_exp(pubkey, pubkey_len, privkey, privkey_len,
			     prime, prime_len, secret, len);
fail:
	BN_clear_free(pub);
	BN_clear_free(p);
	return res;
}


int crypto_mod_exp(const u8 *base, size_t base_len,
		   const u8 *power, size_t power_len,
		   const u8 *modulus, size_t modulus_len,
		   u8 *result, size_t *result_len)
{
	BIGNUM *bn_base, *bn_exp, *bn_modulus, *bn_result;
	int ret = -1;
	BN_CTX *ctx;

	ctx = BN_CTX_new();
	if (ctx == NULL)
		return -1;

	bn_base = BN_bin2bn(base, base_len, NULL);
	bn_exp = BN_bin2bn(power, power_len, NULL);
	bn_modulus = BN_bin2bn(modulus, modulus_len, NULL);
	bn_result = BN_new();

	if (bn_base == NULL || bn_exp == NULL || bn_modulus == NULL ||
	    bn_result == NULL)
		goto error;

	if (BN_mod_exp_mont_consttime(bn_result, bn_base, bn_exp, bn_modulus,
				      ctx, NULL) != 1)
		goto error;

	*result_len = BN_bn2bin(bn_result, result);
	ret = 0;

error:
	BN_clear_free(bn_base);
	BN_clear_free(bn_exp);
	BN_clear_free(bn_modulus);
	BN_clear_free(bn_result);
	BN_CTX_free(ctx);
	return ret;
}


struct crypto_cipher {
	EVP_CIPHER_CTX *enc;
	EVP_CIPHER_CTX *dec;
};


struct crypto_cipher * crypto_cipher_init(enum crypto_cipher_alg alg,
					  const u8 *iv, const u8 *key,
					  size_t key_len)
{
	struct crypto_cipher *ctx;
	const EVP_CIPHER *cipher;

	ctx = os_zalloc(sizeof(*ctx));
	if (ctx == NULL)
		return NULL;

	switch (alg) {
#ifndef CONFIG_NO_RC4
#ifndef OPENSSL_NO_RC4
	case CRYPTO_CIPHER_ALG_RC4:
		cipher = EVP_rc4();
		break;
#endif /* OPENSSL_NO_RC4 */
#endif /* CONFIG_NO_RC4 */
#ifndef OPENSSL_NO_AES
	case CRYPTO_CIPHER_ALG_AES:
		switch (key_len) {
		case 16:
			cipher = EVP_aes_128_cbc();
			break;
#ifndef OPENSSL_IS_BORINGSSL
		case 24:
			cipher = EVP_aes_192_cbc();
			break;
#endif /* OPENSSL_IS_BORINGSSL */
		case 32:
			cipher = EVP_aes_256_cbc();
			break;
		default:
			os_free(ctx);
			return NULL;
		}
		break;
#endif /* OPENSSL_NO_AES */
#ifndef OPENSSL_NO_DES
	case CRYPTO_CIPHER_ALG_3DES:
		cipher = EVP_des_ede3_cbc();
		break;
	case CRYPTO_CIPHER_ALG_DES:
		cipher = EVP_des_cbc();
		break;
#endif /* OPENSSL_NO_DES */
#ifndef OPENSSL_NO_RC2
	case CRYPTO_CIPHER_ALG_RC2:
		cipher = EVP_rc2_ecb();
		break;
#endif /* OPENSSL_NO_RC2 */
	default:
		os_free(ctx);
		return NULL;
	}

	if (!(ctx->enc = EVP_CIPHER_CTX_new()) ||
	    !EVP_EncryptInit_ex(ctx->enc, cipher, NULL, NULL, NULL) ||
	    !EVP_CIPHER_CTX_set_padding(ctx->enc, 0) ||
	    !EVP_CIPHER_CTX_set_key_length(ctx->enc, key_len) ||
	    !EVP_EncryptInit_ex(ctx->enc, NULL, NULL, key, iv)) {
		if (ctx->enc)
			EVP_CIPHER_CTX_free(ctx->enc);
		os_free(ctx);
		return NULL;
	}

	if (!(ctx->dec = EVP_CIPHER_CTX_new()) ||
	    !EVP_DecryptInit_ex(ctx->dec, cipher, NULL, NULL, NULL) ||
	    !EVP_CIPHER_CTX_set_padding(ctx->dec, 0) ||
	    !EVP_CIPHER_CTX_set_key_length(ctx->dec, key_len) ||
	    !EVP_DecryptInit_ex(ctx->dec, NULL, NULL, key, iv)) {
		EVP_CIPHER_CTX_free(ctx->enc);
		if (ctx->dec)
			EVP_CIPHER_CTX_free(ctx->dec);
		os_free(ctx);
		return NULL;
	}

	return ctx;
}


int crypto_cipher_encrypt(struct crypto_cipher *ctx, const u8 *plain,
			  u8 *crypt, size_t len)
{
	int outl;
	if (!EVP_EncryptUpdate(ctx->enc, crypt, &outl, plain, len))
		return -1;
	return 0;
}


int crypto_cipher_decrypt(struct crypto_cipher *ctx, const u8 *crypt,
			  u8 *plain, size_t len)
{
	int outl;
	outl = len;
	if (!EVP_DecryptUpdate(ctx->dec, plain, &outl, crypt, len))
		return -1;
	return 0;
}


void crypto_cipher_deinit(struct crypto_cipher *ctx)
{
	EVP_CIPHER_CTX_free(ctx->enc);
	EVP_CIPHER_CTX_free(ctx->dec);
	os_free(ctx);
}


void * dh5_init(struct wpabuf **priv, struct wpabuf **publ)
{
#if OPENSSL_VERSION_NUMBER < 0x10100000L
	DH *dh;
	struct wpabuf *pubkey = NULL, *privkey = NULL;
	size_t publen, privlen;

	*priv = NULL;
	wpabuf_free(*publ);
	*publ = NULL;

	dh = DH_new();
	if (dh == NULL)
		return NULL;

	dh->g = BN_new();
	if (dh->g == NULL || BN_set_word(dh->g, 2) != 1)
		goto err;

	dh->p = get_group5_prime();
	if (dh->p == NULL)
		goto err;

	dh->q = get_group5_order();
	if (!dh->q)
		goto err;

	if (DH_generate_key(dh) != 1)
		goto err;

	publen = BN_num_bytes(dh->pub_key);
	pubkey = wpabuf_alloc(publen);
	if (pubkey == NULL)
		goto err;
	privlen = BN_num_bytes(dh->priv_key);
	privkey = wpabuf_alloc(privlen);
	if (privkey == NULL)
		goto err;

	BN_bn2bin(dh->pub_key, wpabuf_put(pubkey, publen));
	BN_bn2bin(dh->priv_key, wpabuf_put(privkey, privlen));

	*priv = privkey;
	*publ = pubkey;
	return dh;

err:
	wpabuf_clear_free(pubkey);
	wpabuf_clear_free(privkey);
	DH_free(dh);
	return NULL;
#elif OPENSSL_VERSION_NUMBER >= 0x30000000L
	EVP_PKEY *pkey = NULL;
	OSSL_PARAM params[2];
	size_t pub_len = OSSL_PARAM_UNMODIFIED;
	size_t priv_len;
	struct wpabuf *pubkey = NULL, *privkey = NULL;
	BIGNUM *priv_bn = NULL;
	EVP_PKEY_CTX *gctx;

	*priv = NULL;
	wpabuf_free(*publ);
	*publ = NULL;

	params[0] = OSSL_PARAM_construct_utf8_string(OSSL_PKEY_PARAM_GROUP_NAME,
						     "modp_1536", 0);
	params[1] = OSSL_PARAM_construct_end();

	gctx = EVP_PKEY_CTX_new_from_name(NULL, "DH", NULL);
	if (!gctx ||
	    EVP_PKEY_keygen_init(gctx) != 1 ||
	    EVP_PKEY_CTX_set_params(gctx, params) != 1 ||
	    EVP_PKEY_generate(gctx, &pkey) != 1 ||
	    EVP_PKEY_get_bn_param(pkey, OSSL_PKEY_PARAM_PRIV_KEY,
				  &priv_bn) != 1 ||
	    EVP_PKEY_get_octet_string_param(pkey,
					    OSSL_PKEY_PARAM_ENCODED_PUBLIC_KEY,
					    NULL, 0, &pub_len) < 0 ||
	    pub_len == OSSL_PARAM_UNMODIFIED ||
	    (priv_len = BN_num_bytes(priv_bn)) == 0 ||
	    !(pubkey = wpabuf_alloc(pub_len)) ||
	    !(privkey = wpabuf_alloc(priv_len)) ||
	    EVP_PKEY_get_octet_string_param(pkey,
					    OSSL_PKEY_PARAM_ENCODED_PUBLIC_KEY,
					    wpabuf_put(pubkey, pub_len),
					    pub_len, NULL) != 1) {
		wpa_printf(MSG_INFO, "OpenSSL: failed: %s",
			   ERR_error_string(ERR_get_error(), NULL));
		wpabuf_free(pubkey);
		wpabuf_clear_free(privkey);
		EVP_PKEY_free(pkey);
		pkey = NULL;
	} else {
		BN_bn2bin(priv_bn, wpabuf_put(privkey, priv_len));

		*priv = privkey;
		*publ = pubkey;
	}

	BN_clear_free(priv_bn);
	EVP_PKEY_CTX_free(gctx);
	return pkey;
#else
	DH *dh;
	struct wpabuf *pubkey = NULL, *privkey = NULL;
	size_t publen, privlen;
	BIGNUM *p, *g, *q;
	const BIGNUM *priv_key = NULL, *pub_key = NULL;

	*priv = NULL;
	wpabuf_free(*publ);
	*publ = NULL;

	dh = DH_new();
	if (dh == NULL)
		return NULL;

	g = BN_new();
	p = get_group5_prime();
	q = get_group5_order();
	if (!g || BN_set_word(g, 2) != 1 || !p || !q ||
	    DH_set0_pqg(dh, p, q, g) != 1)
		goto err;
	p = NULL;
	q = NULL;
	g = NULL;

	if (DH_generate_key(dh) != 1)
		goto err;

	DH_get0_key(dh, &pub_key, &priv_key);
	publen = BN_num_bytes(pub_key);
	pubkey = wpabuf_alloc(publen);
	if (!pubkey)
		goto err;
	privlen = BN_num_bytes(priv_key);
	privkey = wpabuf_alloc(privlen);
	if (!privkey)
		goto err;

	BN_bn2bin(pub_key, wpabuf_put(pubkey, publen));
	BN_bn2bin(priv_key, wpabuf_put(privkey, privlen));

	*priv = privkey;
	*publ = pubkey;
	return dh;

err:
	BN_free(p);
	BN_free(q);
	BN_free(g);
	wpabuf_clear_free(pubkey);
	wpabuf_clear_free(privkey);
	DH_free(dh);
	return NULL;
#endif
}


void * dh5_init_fixed(const struct wpabuf *priv, const struct wpabuf *publ)
{
#if OPENSSL_VERSION_NUMBER < 0x10100000L
	DH *dh;

	dh = DH_new();
	if (dh == NULL)
		return NULL;

	dh->g = BN_new();
	if (dh->g == NULL || BN_set_word(dh->g, 2) != 1)
		goto err;

	dh->p = get_group5_prime();
	if (dh->p == NULL)
		goto err;

	dh->priv_key = BN_bin2bn(wpabuf_head(priv), wpabuf_len(priv), NULL);
	if (dh->priv_key == NULL)
		goto err;

	dh->pub_key = BN_bin2bn(wpabuf_head(publ), wpabuf_len(publ), NULL);
	if (dh->pub_key == NULL)
		goto err;

	if (DH_generate_key(dh) != 1)
		goto err;

	return dh;

err:
	DH_free(dh);
	return NULL;
#elif OPENSSL_VERSION_NUMBER >= 0x30000000L
	EVP_PKEY *pkey = NULL;
	OSSL_PARAM_BLD *bld;
	OSSL_PARAM *params = NULL;
	BIGNUM *priv_key, *pub_key;
	EVP_PKEY_CTX *fctx;

	fctx = EVP_PKEY_CTX_new_from_name(NULL, "DH", NULL);
	priv_key = BN_bin2bn(wpabuf_head(priv), wpabuf_len(priv), NULL);
	pub_key = BN_bin2bn(wpabuf_head(publ), wpabuf_len(publ), NULL);
	bld = OSSL_PARAM_BLD_new();
	if (!fctx || !priv_key || !pub_key || !bld ||
	    OSSL_PARAM_BLD_push_utf8_string(bld, OSSL_PKEY_PARAM_GROUP_NAME,
					    "modp_1536", 0) != 1 ||
	    OSSL_PARAM_BLD_push_BN(bld, OSSL_PKEY_PARAM_PRIV_KEY,
				   priv_key) != 1 ||
	    OSSL_PARAM_BLD_push_BN(bld, OSSL_PKEY_PARAM_PUB_KEY,
				   pub_key) != 1 ||
	    !(params = OSSL_PARAM_BLD_to_param(bld)) ||
	    EVP_PKEY_fromdata_init(fctx) != 1 ||
	    EVP_PKEY_fromdata(fctx, &pkey, EVP_PKEY_KEYPAIR, params) != 1) {
		wpa_printf(MSG_INFO, "OpenSSL: EVP_PKEY_fromdata failed: %s",
			   ERR_error_string(ERR_get_error(), NULL));
		EVP_PKEY_free(pkey);
		pkey = NULL;
	}

	BN_clear_free(priv_key);
	BN_free(pub_key);
	EVP_PKEY_CTX_free(fctx);
	OSSL_PARAM_BLD_free(bld);
	OSSL_PARAM_free(params);
	return pkey;
#else
	DH *dh;
	BIGNUM *p = NULL, *g, *priv_key = NULL, *pub_key = NULL;

	dh = DH_new();
	if (dh == NULL)
		return NULL;

	g = BN_new();
	p = get_group5_prime();
	if (!g || BN_set_word(g, 2) != 1 || !p ||
	    DH_set0_pqg(dh, p, NULL, g) != 1)
		goto err;
	p = NULL;
	g = NULL;

	priv_key = BN_bin2bn(wpabuf_head(priv), wpabuf_len(priv), NULL);
	pub_key = BN_bin2bn(wpabuf_head(publ), wpabuf_len(publ), NULL);
	if (!priv_key || !pub_key || DH_set0_key(dh, pub_key, priv_key) != 1)
		goto err;
	pub_key = NULL;
	priv_key = NULL;

	if (DH_generate_key(dh) != 1)
		goto err;

	return dh;

err:
	BN_free(p);
	BN_free(g);
	BN_free(pub_key);
	BN_clear_free(priv_key);
	DH_free(dh);
	return NULL;
#endif
}


struct wpabuf * dh5_derive_shared(void *ctx, const struct wpabuf *peer_public,
				  const struct wpabuf *own_private)
{
#if OPENSSL_VERSION_NUMBER >= 0x30000000L
	EVP_PKEY *pkey = ctx;
	EVP_PKEY *peer_pub;
	size_t len;
	struct wpabuf *res = NULL;
	EVP_PKEY_CTX *dctx = NULL;

	peer_pub = EVP_PKEY_new();
	if (!pkey || !peer_pub ||
	    EVP_PKEY_copy_parameters(peer_pub, pkey) != 1 ||
	    EVP_PKEY_set1_encoded_public_key(peer_pub, wpabuf_head(peer_public),
					     wpabuf_len(peer_public)) != 1 ||
	    !(dctx = EVP_PKEY_CTX_new(pkey, NULL)) ||
	    EVP_PKEY_derive_init(dctx) != 1 ||
	    EVP_PKEY_derive_set_peer(dctx, peer_pub) != 1 ||
	    EVP_PKEY_derive(dctx, NULL, &len) != 1 ||
	    !(res = wpabuf_alloc(len)) ||
	    EVP_PKEY_derive(dctx, wpabuf_mhead(res), &len) != 1) {
		wpa_printf(MSG_INFO, "OpenSSL: EVP_PKEY_derive failed: %s",
			   ERR_error_string(ERR_get_error(), NULL));
		wpabuf_free(res);
		res = NULL;
	} else {
		wpabuf_put(res, len);
	}

	EVP_PKEY_free(peer_pub);
	EVP_PKEY_CTX_free(dctx);
	return res;
#else /* OpenSSL version >= 3.0 */
	BIGNUM *pub_key;
	struct wpabuf *res = NULL;
	size_t rlen;
	DH *dh = ctx;
	int keylen;

	if (ctx == NULL)
		return NULL;

	pub_key = BN_bin2bn(wpabuf_head(peer_public), wpabuf_len(peer_public),
			    NULL);
	if (pub_key == NULL)
		return NULL;

	rlen = DH_size(dh);
	res = wpabuf_alloc(rlen);
	if (res == NULL)
		goto err;

	keylen = DH_compute_key(wpabuf_mhead(res), pub_key, dh);
	if (keylen < 0)
		goto err;
	wpabuf_put(res, keylen);
	BN_clear_free(pub_key);

	return res;

err:
	BN_clear_free(pub_key);
	wpabuf_clear_free(res);
	return NULL;
#endif /* OpenSSL version >= 3.0 */
}


void dh5_free(void *ctx)
{
#if OPENSSL_VERSION_NUMBER >= 0x30000000L
	EVP_PKEY *pkey = ctx;

	EVP_PKEY_free(pkey);
#else /* OpenSSL version >= 3.0 */
	DH *dh;
	if (ctx == NULL)
		return;
	dh = ctx;
	DH_free(dh);
#endif /* OpenSSL version >= 3.0 */
}


struct crypto_hash {
#if OPENSSL_VERSION_NUMBER >= 0x30000000L
	EVP_MAC_CTX *ctx;
#else /* OpenSSL version >= 3.0 */
	HMAC_CTX *ctx;
#endif /* OpenSSL version >= 3.0 */
	bool failed;
};


struct crypto_hash * crypto_hash_init(enum crypto_hash_alg alg, const u8 *key,
				      size_t key_len)
{
#if OPENSSL_VERSION_NUMBER >= 0x30000000L
	struct crypto_hash *ctx;
	EVP_MAC *mac;
	OSSL_PARAM params[2];
	char *a = NULL;

	switch (alg) {
#ifndef OPENSSL_NO_MD5
	case CRYPTO_HASH_ALG_HMAC_MD5:
		a = "MD5";
		break;
#endif /* OPENSSL_NO_MD5 */
#ifndef OPENSSL_NO_SHA
	case CRYPTO_HASH_ALG_HMAC_SHA1:
		a = "SHA1";
		break;
#endif /* OPENSSL_NO_SHA */
#ifndef OPENSSL_NO_SHA256
#ifdef CONFIG_SHA256
	case CRYPTO_HASH_ALG_HMAC_SHA256:
		a = "SHA256";
		break;
#endif /* CONFIG_SHA256 */
#endif /* OPENSSL_NO_SHA256 */
	default:
		return NULL;
	}

	mac = EVP_MAC_fetch(NULL, "HMAC", NULL);
	if (!mac)
		return NULL;

	params[0] = OSSL_PARAM_construct_utf8_string("digest", a, 0);
	params[1] = OSSL_PARAM_construct_end();

	ctx = os_zalloc(sizeof(*ctx));
	if (!ctx)
		goto fail;
	ctx->ctx = EVP_MAC_CTX_new(mac);
	if (!ctx->ctx) {
		os_free(ctx);
		ctx = NULL;
		goto fail;
	}

	if (EVP_MAC_init(ctx->ctx, key, key_len, params) != 1) {
		EVP_MAC_CTX_free(ctx->ctx);
		bin_clear_free(ctx, sizeof(*ctx));
		ctx = NULL;
		goto fail;
	}

fail:
	EVP_MAC_free(mac);
	return ctx;
#else /* OpenSSL version >= 3.0 */
	struct crypto_hash *ctx;
	const EVP_MD *md;

	switch (alg) {
#ifndef OPENSSL_NO_MD5
	case CRYPTO_HASH_ALG_HMAC_MD5:
		md = EVP_md5();
		break;
#endif /* OPENSSL_NO_MD5 */
#ifndef OPENSSL_NO_SHA
	case CRYPTO_HASH_ALG_HMAC_SHA1:
		md = EVP_sha1();
		break;
#endif /* OPENSSL_NO_SHA */
#ifndef OPENSSL_NO_SHA256
#ifdef CONFIG_SHA256
	case CRYPTO_HASH_ALG_HMAC_SHA256:
		md = EVP_sha256();
		break;
#endif /* CONFIG_SHA256 */
#endif /* OPENSSL_NO_SHA256 */
	default:
		return NULL;
	}

	ctx = os_zalloc(sizeof(*ctx));
	if (ctx == NULL)
		return NULL;
	ctx->ctx = HMAC_CTX_new();
	if (!ctx->ctx) {
		os_free(ctx);
		return NULL;
	}

	if (HMAC_Init_ex(ctx->ctx, key, key_len, md, NULL) != 1) {
		HMAC_CTX_free(ctx->ctx);
		bin_clear_free(ctx, sizeof(*ctx));
		return NULL;
	}

	return ctx;
#endif /* OpenSSL version >= 3.0 */
}


void crypto_hash_update(struct crypto_hash *ctx, const u8 *data, size_t len)
{
	if (ctx == NULL)
		return;
#if OPENSSL_VERSION_NUMBER >= 0x30000000L
	if (!EVP_MAC_update(ctx->ctx, data, len))
		ctx->failed = true;
#else /* OpenSSL version >= 3.0 */
	if (!HMAC_Update(ctx->ctx, data, len))
		ctx->failed = true;
#endif /* OpenSSL version >= 3.0 */
}


int crypto_hash_finish(struct crypto_hash *ctx, u8 *mac, size_t *len)
{
#if OPENSSL_VERSION_NUMBER >= 0x30000000L
	size_t mdlen;
	int res;
	bool failed;

	if (!ctx)
		return -2;

	if (!mac || !len) {
		EVP_MAC_CTX_free(ctx->ctx);
		bin_clear_free(ctx, sizeof(*ctx));
		return 0;
	}

	res = EVP_MAC_final(ctx->ctx, NULL, &mdlen, 0);
	if (res != 1) {
		EVP_MAC_CTX_free(ctx->ctx);
		bin_clear_free(ctx, sizeof(*ctx));
		return -1;
	}
	res = EVP_MAC_final(ctx->ctx, mac, &mdlen, mdlen);
	EVP_MAC_CTX_free(ctx->ctx);
	failed = ctx->failed;
	bin_clear_free(ctx, sizeof(*ctx));

	if (TEST_FAIL())
		return -1;

	if (failed)
		return -2;

	if (res == 1) {
		*len = mdlen;
		return 0;
	}

	return -1;
#else /* OpenSSL version >= 3.0 */
	unsigned int mdlen;
	int res;
	bool failed;

	if (ctx == NULL)
		return -2;

	if (mac == NULL || len == NULL) {
		HMAC_CTX_free(ctx->ctx);
		bin_clear_free(ctx, sizeof(*ctx));
		return 0;
	}

	mdlen = *len;
	res = HMAC_Final(ctx->ctx, mac, &mdlen);
	HMAC_CTX_free(ctx->ctx);
	failed = ctx->failed;
	bin_clear_free(ctx, sizeof(*ctx));

	if (TEST_FAIL())
		return -1;

	if (failed)
		return -2;

	if (res == 1) {
		*len = mdlen;
		return 0;
	}

	return -1;
#endif /* OpenSSL version >= 3.0 */
}


#if OPENSSL_VERSION_NUMBER >= 0x30000000L

static int openssl_hmac_vector(char *digest, const u8 *key,
			       size_t key_len, size_t num_elem,
			       const u8 *addr[], const size_t *len, u8 *mac,
			       unsigned int mdlen)
{
	EVP_MAC *hmac;
	OSSL_PARAM params[2];
	EVP_MAC_CTX *ctx;
	size_t i, mlen;
	int res;

	if (TEST_FAIL())
		return -1;

	hmac = EVP_MAC_fetch(NULL, "HMAC", NULL);
	if (!hmac)
		return -1;

	params[0] = OSSL_PARAM_construct_utf8_string("digest", digest, 0);
	params[1] = OSSL_PARAM_construct_end();

	ctx = EVP_MAC_CTX_new(hmac);
	EVP_MAC_free(hmac);
	if (!ctx)
		return -1;

	if (EVP_MAC_init(ctx, key, key_len, params) != 1)
		goto fail;

	for (i = 0; i < num_elem; i++) {
		if (EVP_MAC_update(ctx, addr[i], len[i]) != 1)
			goto fail;
	}

	res = EVP_MAC_final(ctx, mac, &mlen, mdlen);
	EVP_MAC_CTX_free(ctx);

	return res == 1 ? 0 : -1;
fail:
	EVP_MAC_CTX_free(ctx);
	return -1;
}


#ifndef CONFIG_FIPS

int hmac_md5_vector(const u8 *key, size_t key_len, size_t num_elem,
		    const u8 *addr[], const size_t *len, u8 *mac)
{
	return openssl_hmac_vector("MD5", key ,key_len, num_elem, addr, len,
				   mac, 16);
}


int hmac_md5(const u8 *key, size_t key_len, const u8 *data, size_t data_len,
	     u8 *mac)
{
	return hmac_md5_vector(key, key_len, 1, &data, &data_len, mac);
}

#endif /* CONFIG_FIPS */


int hmac_sha1_vector(const u8 *key, size_t key_len, size_t num_elem,
		     const u8 *addr[], const size_t *len, u8 *mac)
{
	return openssl_hmac_vector("SHA1", key, key_len, num_elem, addr,
				   len, mac, 20);
}


int hmac_sha1(const u8 *key, size_t key_len, const u8 *data, size_t data_len,
	       u8 *mac)
{
	return hmac_sha1_vector(key, key_len, 1, &data, &data_len, mac);
}


#ifdef CONFIG_SHA256

int hmac_sha256_vector(const u8 *key, size_t key_len, size_t num_elem,
		       const u8 *addr[], const size_t *len, u8 *mac)
{
	return openssl_hmac_vector("SHA256", key, key_len, num_elem, addr,
				   len, mac, 32);
}


int hmac_sha256(const u8 *key, size_t key_len, const u8 *data,
		size_t data_len, u8 *mac)
{
	return hmac_sha256_vector(key, key_len, 1, &data, &data_len, mac);
}

#endif /* CONFIG_SHA256 */


#ifdef CONFIG_SHA384

int hmac_sha384_vector(const u8 *key, size_t key_len, size_t num_elem,
		       const u8 *addr[], const size_t *len, u8 *mac)
{
	return openssl_hmac_vector("SHA384", key, key_len, num_elem, addr,
				   len, mac, 48);
}


int hmac_sha384(const u8 *key, size_t key_len, const u8 *data,
		size_t data_len, u8 *mac)
{
	return hmac_sha384_vector(key, key_len, 1, &data, &data_len, mac);
}

#endif /* CONFIG_SHA384 */


#ifdef CONFIG_SHA512

int hmac_sha512_vector(const u8 *key, size_t key_len, size_t num_elem,
		       const u8 *addr[], const size_t *len, u8 *mac)
{
	return openssl_hmac_vector("SHA512", key, key_len, num_elem, addr,
				   len, mac, 64);
}


int hmac_sha512(const u8 *key, size_t key_len, const u8 *data,
		size_t data_len, u8 *mac)
{
	return hmac_sha512_vector(key, key_len, 1, &data, &data_len, mac);
}

#endif /* CONFIG_SHA512 */

#else /* OpenSSL version >= 3.0 */

static int openssl_hmac_vector(const EVP_MD *type, const u8 *key,
			       size_t key_len, size_t num_elem,
			       const u8 *addr[], const size_t *len, u8 *mac,
			       unsigned int mdlen)
{
	HMAC_CTX *ctx;
	size_t i;
	int res;

	if (TEST_FAIL())
		return -1;

	ctx = HMAC_CTX_new();
	if (!ctx)
		return -1;
	res = HMAC_Init_ex(ctx, key, key_len, type, NULL);
	if (res != 1)
		goto done;

	for (i = 0; i < num_elem; i++)
		HMAC_Update(ctx, addr[i], len[i]);

	res = HMAC_Final(ctx, mac, &mdlen);
done:
	HMAC_CTX_free(ctx);

	return res == 1 ? 0 : -1;
}


#ifndef CONFIG_FIPS

int hmac_md5_vector(const u8 *key, size_t key_len, size_t num_elem,
		    const u8 *addr[], const size_t *len, u8 *mac)
{
	return openssl_hmac_vector(EVP_md5(), key ,key_len, num_elem, addr, len,
				   mac, 16);
}


int hmac_md5(const u8 *key, size_t key_len, const u8 *data, size_t data_len,
	     u8 *mac)
{
	return hmac_md5_vector(key, key_len, 1, &data, &data_len, mac);
}

#endif /* CONFIG_FIPS */


int hmac_sha1_vector(const u8 *key, size_t key_len, size_t num_elem,
		     const u8 *addr[], const size_t *len, u8 *mac)
{
	return openssl_hmac_vector(EVP_sha1(), key, key_len, num_elem, addr,
				   len, mac, 20);
}


int hmac_sha1(const u8 *key, size_t key_len, const u8 *data, size_t data_len,
	       u8 *mac)
{
	return hmac_sha1_vector(key, key_len, 1, &data, &data_len, mac);
}


#ifdef CONFIG_SHA256

int hmac_sha256_vector(const u8 *key, size_t key_len, size_t num_elem,
		       const u8 *addr[], const size_t *len, u8 *mac)
{
	return openssl_hmac_vector(EVP_sha256(), key, key_len, num_elem, addr,
				   len, mac, 32);
}


int hmac_sha256(const u8 *key, size_t key_len, const u8 *data,
		size_t data_len, u8 *mac)
{
	return hmac_sha256_vector(key, key_len, 1, &data, &data_len, mac);
}

#endif /* CONFIG_SHA256 */


#ifdef CONFIG_SHA384

int hmac_sha384_vector(const u8 *key, size_t key_len, size_t num_elem,
		       const u8 *addr[], const size_t *len, u8 *mac)
{
	return openssl_hmac_vector(EVP_sha384(), key, key_len, num_elem, addr,
				   len, mac, 48);
}


int hmac_sha384(const u8 *key, size_t key_len, const u8 *data,
		size_t data_len, u8 *mac)
{
	return hmac_sha384_vector(key, key_len, 1, &data, &data_len, mac);
}

#endif /* CONFIG_SHA384 */


#ifdef CONFIG_SHA512

int hmac_sha512_vector(const u8 *key, size_t key_len, size_t num_elem,
		       const u8 *addr[], const size_t *len, u8 *mac)
{
	return openssl_hmac_vector(EVP_sha512(), key, key_len, num_elem, addr,
				   len, mac, 64);
}


int hmac_sha512(const u8 *key, size_t key_len, const u8 *data,
		size_t data_len, u8 *mac)
{
	return hmac_sha512_vector(key, key_len, 1, &data, &data_len, mac);
}

#endif /* CONFIG_SHA512 */

#endif /* OpenSSL version >= 3.0 */


int pbkdf2_sha1(const char *passphrase, const u8 *ssid, size_t ssid_len,
		int iterations, u8 *buf, size_t buflen)
{
	if (PKCS5_PBKDF2_HMAC_SHA1(passphrase, os_strlen(passphrase), ssid,
				   ssid_len, iterations, buflen, buf) != 1)
		return -1;
	return 0;
}


int crypto_get_random(void *buf, size_t len)
{
	if (RAND_bytes(buf, len) != 1)
		return -1;
	return 0;
}


int omac1_aes_vector(const u8 *key, size_t key_len, size_t num_elem,
		     const u8 *addr[], const size_t *len, u8 *mac)
{
#if OPENSSL_VERSION_NUMBER >= 0x30000000L
	EVP_MAC_CTX *ctx = NULL;
	EVP_MAC *emac;
	int ret = -1;
	size_t outlen, i;
	OSSL_PARAM params[2];
	char *cipher = NULL;

	if (TEST_FAIL())
		return -1;

	emac = EVP_MAC_fetch(NULL, "CMAC", NULL);

	if (key_len == 32)
		cipher = "aes-256-cbc";
	else if (key_len == 24)
		cipher = "aes-192-cbc";
	else if (key_len == 16)
		cipher = "aes-128-cbc";

	params[0] = OSSL_PARAM_construct_utf8_string("cipher", cipher, 0);
	params[1] = OSSL_PARAM_construct_end();

	if (!emac || !cipher ||
	    !(ctx = EVP_MAC_CTX_new(emac)) ||
	    EVP_MAC_init(ctx, key, key_len, params) != 1)
		goto fail;

	for (i = 0; i < num_elem; i++) {
		if (!EVP_MAC_update(ctx, addr[i], len[i]))
			goto fail;
	}
	if (EVP_MAC_final(ctx, mac, &outlen, 16) != 1 || outlen != 16)
		goto fail;

	ret = 0;
fail:
	EVP_MAC_CTX_free(ctx);
	EVP_MAC_free(emac);
	return ret;
#else /* OpenSSL version >= 3.0 */
	CMAC_CTX *ctx;
	int ret = -1;
	size_t outlen, i;

	if (TEST_FAIL())
		return -1;

	ctx = CMAC_CTX_new();
	if (ctx == NULL)
		return -1;

	if (key_len == 32) {
		if (!CMAC_Init(ctx, key, 32, EVP_aes_256_cbc(), NULL))
			goto fail;
	} else if (key_len == 24) {
		if (!CMAC_Init(ctx, key, 24, EVP_aes_192_cbc(), NULL))
			goto fail;
	} else if (key_len == 16) {
		if (!CMAC_Init(ctx, key, 16, EVP_aes_128_cbc(), NULL))
			goto fail;
	} else {
		goto fail;
	}
	for (i = 0; i < num_elem; i++) {
		if (!CMAC_Update(ctx, addr[i], len[i]))
			goto fail;
	}
	if (!CMAC_Final(ctx, mac, &outlen) || outlen != 16)
		goto fail;

	ret = 0;
fail:
	CMAC_CTX_free(ctx);
	return ret;
#endif /* OpenSSL version >= 3.0 */
}


int omac1_aes_128_vector(const u8 *key, size_t num_elem,
			 const u8 *addr[], const size_t *len, u8 *mac)
{
	return omac1_aes_vector(key, 16, num_elem, addr, len, mac);
}


int omac1_aes_128(const u8 *key, const u8 *data, size_t data_len, u8 *mac)
{
	return omac1_aes_128_vector(key, 1, &data, &data_len, mac);
}


int omac1_aes_256(const u8 *key, const u8 *data, size_t data_len, u8 *mac)
{
	return omac1_aes_vector(key, 32, 1, &data, &data_len, mac);
}


struct crypto_bignum * crypto_bignum_init(void)
{
	if (TEST_FAIL())
		return NULL;
	return (struct crypto_bignum *) BN_new();
}


struct crypto_bignum * crypto_bignum_init_set(const u8 *buf, size_t len)
{
	BIGNUM *bn;

	if (TEST_FAIL())
		return NULL;

	bn = BN_bin2bn(buf, len, NULL);
	return (struct crypto_bignum *) bn;
}


struct crypto_bignum * crypto_bignum_init_uint(unsigned int val)
{
	BIGNUM *bn;

	if (TEST_FAIL())
		return NULL;

	bn = BN_new();
	if (!bn)
		return NULL;
	if (BN_set_word(bn, val) != 1) {
		BN_free(bn);
		return NULL;
	}
	return (struct crypto_bignum *) bn;
}


void crypto_bignum_deinit(struct crypto_bignum *n, int clear)
{
	if (clear)
		BN_clear_free((BIGNUM *) n);
	else
		BN_free((BIGNUM *) n);
}


int crypto_bignum_to_bin(const struct crypto_bignum *a,
			 u8 *buf, size_t buflen, size_t padlen)
{
	int num_bytes, offset;

	if (TEST_FAIL())
		return -1;

	if (padlen > buflen)
		return -1;

	if (padlen) {
#ifdef OPENSSL_IS_BORINGSSL
		if (BN_bn2bin_padded(buf, padlen, (const BIGNUM *) a) == 0)
			return -1;
		return padlen;
#else /* OPENSSL_IS_BORINGSSL */
#if OPENSSL_VERSION_NUMBER >= 0x10100000L && !defined(LIBRESSL_VERSION_NUMBER)
		return BN_bn2binpad((const BIGNUM *) a, buf, padlen);
#endif
#endif
	}

	num_bytes = BN_num_bytes((const BIGNUM *) a);
	if ((size_t) num_bytes > buflen)
		return -1;
	if (padlen > (size_t) num_bytes)
		offset = padlen - num_bytes;
	else
		offset = 0;

	os_memset(buf, 0, offset);
	BN_bn2bin((const BIGNUM *) a, buf + offset);

	return num_bytes + offset;
}


int crypto_bignum_rand(struct crypto_bignum *r, const struct crypto_bignum *m)
{
	if (TEST_FAIL())
		return -1;
	return BN_rand_range((BIGNUM *) r, (const BIGNUM *) m) == 1 ? 0 : -1;
}


int crypto_bignum_add(const struct crypto_bignum *a,
		      const struct crypto_bignum *b,
		      struct crypto_bignum *c)
{
	return BN_add((BIGNUM *) c, (const BIGNUM *) a, (const BIGNUM *) b) ?
		0 : -1;
}


int crypto_bignum_mod(const struct crypto_bignum *a,
		      const struct crypto_bignum *b,
		      struct crypto_bignum *c)
{
	int res;
	BN_CTX *bnctx;

	bnctx = BN_CTX_new();
	if (bnctx == NULL)
		return -1;
	res = BN_mod((BIGNUM *) c, (const BIGNUM *) a, (const BIGNUM *) b,
		     bnctx);
	BN_CTX_free(bnctx);

	return res ? 0 : -1;
}


int crypto_bignum_exptmod(const struct crypto_bignum *a,
			  const struct crypto_bignum *b,
			  const struct crypto_bignum *c,
			  struct crypto_bignum *d)
{
	int res;
	BN_CTX *bnctx;

	if (TEST_FAIL())
		return -1;

	bnctx = BN_CTX_new();
	if (bnctx == NULL)
		return -1;
	res = BN_mod_exp_mont_consttime((BIGNUM *) d, (const BIGNUM *) a,
					(const BIGNUM *) b, (const BIGNUM *) c,
					bnctx, NULL);
	BN_CTX_free(bnctx);

	return res ? 0 : -1;
}


int crypto_bignum_inverse(const struct crypto_bignum *a,
			  const struct crypto_bignum *b,
			  struct crypto_bignum *c)
{
	BIGNUM *res;
	BN_CTX *bnctx;

	if (TEST_FAIL())
		return -1;
	bnctx = BN_CTX_new();
	if (bnctx == NULL)
		return -1;
#ifdef OPENSSL_IS_BORINGSSL
	/* TODO: use BN_mod_inverse_blinded() ? */
#else /* OPENSSL_IS_BORINGSSL */
	BN_set_flags((BIGNUM *) a, BN_FLG_CONSTTIME);
#endif /* OPENSSL_IS_BORINGSSL */
	res = BN_mod_inverse((BIGNUM *) c, (const BIGNUM *) a,
			     (const BIGNUM *) b, bnctx);
	BN_CTX_free(bnctx);

	return res ? 0 : -1;
}


int crypto_bignum_sub(const struct crypto_bignum *a,
		      const struct crypto_bignum *b,
		      struct crypto_bignum *c)
{
	if (TEST_FAIL())
		return -1;
	return BN_sub((BIGNUM *) c, (const BIGNUM *) a, (const BIGNUM *) b) ?
		0 : -1;
}


int crypto_bignum_div(const struct crypto_bignum *a,
		      const struct crypto_bignum *b,
		      struct crypto_bignum *c)
{
	int res;

	BN_CTX *bnctx;

	if (TEST_FAIL())
		return -1;

	bnctx = BN_CTX_new();
	if (bnctx == NULL)
		return -1;
#ifndef OPENSSL_IS_BORINGSSL
	BN_set_flags((BIGNUM *) a, BN_FLG_CONSTTIME);
#endif /* OPENSSL_IS_BORINGSSL */
	res = BN_div((BIGNUM *) c, NULL, (const BIGNUM *) a,
		     (const BIGNUM *) b, bnctx);
	BN_CTX_free(bnctx);

	return res ? 0 : -1;
}


int crypto_bignum_addmod(const struct crypto_bignum *a,
			 const struct crypto_bignum *b,
			 const struct crypto_bignum *c,
			 struct crypto_bignum *d)
{
	int res;
	BN_CTX *bnctx;

	if (TEST_FAIL())
		return -1;

	bnctx = BN_CTX_new();
	if (!bnctx)
		return -1;
	res = BN_mod_add((BIGNUM *) d, (const BIGNUM *) a, (const BIGNUM *) b,
			 (const BIGNUM *) c, bnctx);
	BN_CTX_free(bnctx);

	return res ? 0 : -1;
}


int crypto_bignum_mulmod(const struct crypto_bignum *a,
			 const struct crypto_bignum *b,
			 const struct crypto_bignum *c,
			 struct crypto_bignum *d)
{
	int res;

	BN_CTX *bnctx;

	if (TEST_FAIL())
		return -1;

	bnctx = BN_CTX_new();
	if (bnctx == NULL)
		return -1;
	res = BN_mod_mul((BIGNUM *) d, (const BIGNUM *) a, (const BIGNUM *) b,
			 (const BIGNUM *) c, bnctx);
	BN_CTX_free(bnctx);

	return res ? 0 : -1;
}


int crypto_bignum_sqrmod(const struct crypto_bignum *a,
			 const struct crypto_bignum *b,
			 struct crypto_bignum *c)
{
	int res;
	BN_CTX *bnctx;

	if (TEST_FAIL())
		return -1;

	bnctx = BN_CTX_new();
	if (!bnctx)
		return -1;
	res = BN_mod_sqr((BIGNUM *) c, (const BIGNUM *) a, (const BIGNUM *) b,
			 bnctx);
	BN_CTX_free(bnctx);

	return res ? 0 : -1;
}


int crypto_bignum_rshift(const struct crypto_bignum *a, int n,
			 struct crypto_bignum *r)
{
	return BN_rshift((BIGNUM *) r, (const BIGNUM *) a, n) == 1 ? 0 : -1;
}


int crypto_bignum_cmp(const struct crypto_bignum *a,
		      const struct crypto_bignum *b)
{
	return BN_cmp((const BIGNUM *) a, (const BIGNUM *) b);
}


int crypto_bignum_is_zero(const struct crypto_bignum *a)
{
	return BN_is_zero((const BIGNUM *) a);
}


int crypto_bignum_is_one(const struct crypto_bignum *a)
{
	return BN_is_one((const BIGNUM *) a);
}


int crypto_bignum_is_odd(const struct crypto_bignum *a)
{
	return BN_is_odd((const BIGNUM *) a);
}


int crypto_bignum_legendre(const struct crypto_bignum *a,
			   const struct crypto_bignum *p)
{
	BN_CTX *bnctx;
	BIGNUM *exp = NULL, *tmp = NULL;
	int res = -2;
	unsigned int mask;

	if (TEST_FAIL())
		return -2;

	bnctx = BN_CTX_new();
	if (bnctx == NULL)
		return -2;

	exp = BN_new();
	tmp = BN_new();
	if (!exp || !tmp ||
	    /* exp = (p-1) / 2 */
	    !BN_sub(exp, (const BIGNUM *) p, BN_value_one()) ||
	    !BN_rshift1(exp, exp) ||
	    !BN_mod_exp_mont_consttime(tmp, (const BIGNUM *) a, exp,
				       (const BIGNUM *) p, bnctx, NULL))
		goto fail;

	/* Return 1 if tmp == 1, 0 if tmp == 0, or -1 otherwise. Need to use
	 * constant time selection to avoid branches here. */
	res = -1;
	mask = const_time_eq(BN_is_word(tmp, 1), 1);
	res = const_time_select_int(mask, 1, res);
	mask = const_time_eq(BN_is_zero(tmp), 1);
	res = const_time_select_int(mask, 0, res);

fail:
	BN_clear_free(tmp);
	BN_clear_free(exp);
	BN_CTX_free(bnctx);
	return res;
}


#ifdef CONFIG_ECC

struct crypto_ec {
	EC_GROUP *group;
	int nid;
	int iana_group;
	BN_CTX *bnctx;
	BIGNUM *prime;
	BIGNUM *order;
	BIGNUM *a;
	BIGNUM *b;
};


static int crypto_ec_group_2_nid(int group)
{
	/* Map from IANA registry for IKE D-H groups to OpenSSL NID */
	switch (group) {
	case 19:
		return NID_X9_62_prime256v1;
	case 20:
		return NID_secp384r1;
	case 21:
		return NID_secp521r1;
	case 25:
		return NID_X9_62_prime192v1;
	case 26:
		return NID_secp224r1;
#ifdef NID_brainpoolP224r1
	case 27:
		return NID_brainpoolP224r1;
#endif /* NID_brainpoolP224r1 */
#ifdef NID_brainpoolP256r1
	case 28:
		return NID_brainpoolP256r1;
#endif /* NID_brainpoolP256r1 */
#ifdef NID_brainpoolP384r1
	case 29:
		return NID_brainpoolP384r1;
#endif /* NID_brainpoolP384r1 */
#ifdef NID_brainpoolP512r1
	case 30:
		return NID_brainpoolP512r1;
#endif /* NID_brainpoolP512r1 */
	default:
		return -1;
	}
}


#if OPENSSL_VERSION_NUMBER >= 0x30000000L
static const char * crypto_ec_group_2_name(int group)
{
	/* Map from IANA registry for IKE D-H groups to OpenSSL group name */
	switch (group) {
	case 19:
		return "prime256v1";
	case 20:
		return "secp384r1";
	case 21:
		return "secp521r1";
	case 25:
		return "prime192v1";
	case 26:
		return "secp224r1";
#ifdef NID_brainpoolP224r1
	case 27:
		return "brainpoolP224r1";
#endif /* NID_brainpoolP224r1 */
#ifdef NID_brainpoolP256r1
	case 28:
		return "brainpoolP256r1";
#endif /* NID_brainpoolP256r1 */
#ifdef NID_brainpoolP384r1
	case 29:
		return "brainpoolP384r1";
#endif /* NID_brainpoolP384r1 */
#ifdef NID_brainpoolP512r1
	case 30:
		return "brainpoolP512r1";
#endif /* NID_brainpoolP512r1 */
	default:
		return NULL;
	}
}
#endif /* OpenSSL version >= 3.0 */


struct crypto_ec * crypto_ec_init(int group)
{
	struct crypto_ec *e;
	int nid;

	nid = crypto_ec_group_2_nid(group);
	if (nid < 0)
		return NULL;

	e = os_zalloc(sizeof(*e));
	if (e == NULL)
		return NULL;

	e->nid = nid;
	e->iana_group = group;
	e->bnctx = BN_CTX_new();
	e->group = EC_GROUP_new_by_curve_name(nid);
	e->prime = BN_new();
	e->order = BN_new();
	e->a = BN_new();
	e->b = BN_new();
	if (e->group == NULL || e->bnctx == NULL || e->prime == NULL ||
	    e->order == NULL || e->a == NULL || e->b == NULL ||
	    !EC_GROUP_get_curve(e->group, e->prime, e->a, e->b, e->bnctx) ||
	    !EC_GROUP_get_order(e->group, e->order, e->bnctx)) {
		crypto_ec_deinit(e);
		e = NULL;
	}

	return e;
}


void crypto_ec_deinit(struct crypto_ec *e)
{
	if (e == NULL)
		return;
	BN_clear_free(e->b);
	BN_clear_free(e->a);
	BN_clear_free(e->order);
	BN_clear_free(e->prime);
	EC_GROUP_free(e->group);
	BN_CTX_free(e->bnctx);
	os_free(e);
}


struct crypto_ec_point * crypto_ec_point_init(struct crypto_ec *e)
{
	if (TEST_FAIL())
		return NULL;
	if (e == NULL)
		return NULL;
	return (struct crypto_ec_point *) EC_POINT_new(e->group);
}


size_t crypto_ec_prime_len(struct crypto_ec *e)
{
	return BN_num_bytes(e->prime);
}


size_t crypto_ec_prime_len_bits(struct crypto_ec *e)
{
	return BN_num_bits(e->prime);
}


size_t crypto_ec_order_len(struct crypto_ec *e)
{
	return BN_num_bytes(e->order);
}


const struct crypto_bignum * crypto_ec_get_prime(struct crypto_ec *e)
{
	return (const struct crypto_bignum *) e->prime;
}


const struct crypto_bignum * crypto_ec_get_order(struct crypto_ec *e)
{
	return (const struct crypto_bignum *) e->order;
}


const struct crypto_bignum * crypto_ec_get_a(struct crypto_ec *e)
{
	return (const struct crypto_bignum *) e->a;
}


const struct crypto_bignum * crypto_ec_get_b(struct crypto_ec *e)
{
	return (const struct crypto_bignum *) e->b;
}


const struct crypto_ec_point * crypto_ec_get_generator(struct crypto_ec *e)
{
	return (const struct crypto_ec_point *)
		EC_GROUP_get0_generator(e->group);
}


void crypto_ec_point_deinit(struct crypto_ec_point *p, int clear)
{
	if (clear)
		EC_POINT_clear_free((EC_POINT *) p);
	else
		EC_POINT_free((EC_POINT *) p);
}


int crypto_ec_point_x(struct crypto_ec *e, const struct crypto_ec_point *p,
		      struct crypto_bignum *x)
{
	return EC_POINT_get_affine_coordinates(e->group,
					       (const EC_POINT *) p,
					       (BIGNUM *) x, NULL,
					       e->bnctx) == 1 ? 0 : -1;
}


int crypto_ec_point_to_bin(struct crypto_ec *e,
			   const struct crypto_ec_point *point, u8 *x, u8 *y)
{
	BIGNUM *x_bn, *y_bn;
	int ret = -1;
	int len = BN_num_bytes(e->prime);

	if (TEST_FAIL())
		return -1;

	x_bn = BN_new();
	y_bn = BN_new();

	if (x_bn && y_bn &&
	    EC_POINT_get_affine_coordinates(e->group, (EC_POINT *) point,
					    x_bn, y_bn, e->bnctx)) {
		if (x) {
			ret = crypto_bignum_to_bin(
				(struct crypto_bignum *) x_bn, x, len, len);
		}
		if (ret >= 0 && y) {
			ret = crypto_bignum_to_bin(
				(struct crypto_bignum *) y_bn, y, len, len);
		}

		if (ret > 0)
			ret = 0;
	}

	BN_clear_free(x_bn);
	BN_clear_free(y_bn);
	return ret;
}


struct crypto_ec_point * crypto_ec_point_from_bin(struct crypto_ec *e,
						  const u8 *val)
{
	BIGNUM *x, *y;
	EC_POINT *elem;
	int len = BN_num_bytes(e->prime);

	if (TEST_FAIL())
		return NULL;

	x = BN_bin2bn(val, len, NULL);
	y = BN_bin2bn(val + len, len, NULL);
	elem = EC_POINT_new(e->group);
	if (x == NULL || y == NULL || elem == NULL) {
		BN_clear_free(x);
		BN_clear_free(y);
		EC_POINT_clear_free(elem);
		return NULL;
	}

	if (!EC_POINT_set_affine_coordinates(e->group, elem, x, y, e->bnctx)) {
		EC_POINT_clear_free(elem);
		elem = NULL;
	}

	BN_clear_free(x);
	BN_clear_free(y);

	return (struct crypto_ec_point *) elem;
}


int crypto_ec_point_add(struct crypto_ec *e, const struct crypto_ec_point *a,
			const struct crypto_ec_point *b,
			struct crypto_ec_point *c)
{
	if (TEST_FAIL())
		return -1;
	return EC_POINT_add(e->group, (EC_POINT *) c, (const EC_POINT *) a,
			    (const EC_POINT *) b, e->bnctx) ? 0 : -1;
}


int crypto_ec_point_mul(struct crypto_ec *e, const struct crypto_ec_point *p,
			const struct crypto_bignum *b,
			struct crypto_ec_point *res)
{
	if (TEST_FAIL())
		return -1;
	return EC_POINT_mul(e->group, (EC_POINT *) res, NULL,
			    (const EC_POINT *) p, (const BIGNUM *) b, e->bnctx)
		? 0 : -1;
}


int crypto_ec_point_invert(struct crypto_ec *e, struct crypto_ec_point *p)
{
	if (TEST_FAIL())
		return -1;
	return EC_POINT_invert(e->group, (EC_POINT *) p, e->bnctx) ? 0 : -1;
}


struct crypto_bignum *
crypto_ec_point_compute_y_sqr(struct crypto_ec *e,
			      const struct crypto_bignum *x)
{
	BIGNUM *tmp;

	if (TEST_FAIL())
		return NULL;

	tmp = BN_new();

	/* y^2 = x^3 + ax + b = (x^2 + a)x + b */
	if (tmp &&
	    BN_mod_sqr(tmp, (const BIGNUM *) x, e->prime, e->bnctx) &&
	    BN_mod_add_quick(tmp, e->a, tmp, e->prime) &&
	    BN_mod_mul(tmp, tmp, (const BIGNUM *) x, e->prime, e->bnctx) &&
	    BN_mod_add_quick(tmp, tmp, e->b, e->prime))
		return (struct crypto_bignum *) tmp;

	BN_clear_free(tmp);
	return NULL;
}


int crypto_ec_point_is_at_infinity(struct crypto_ec *e,
				   const struct crypto_ec_point *p)
{
	return EC_POINT_is_at_infinity(e->group, (const EC_POINT *) p);
}


int crypto_ec_point_is_on_curve(struct crypto_ec *e,
				const struct crypto_ec_point *p)
{
	return EC_POINT_is_on_curve(e->group, (const EC_POINT *) p,
				    e->bnctx) == 1;
}


int crypto_ec_point_cmp(const struct crypto_ec *e,
			const struct crypto_ec_point *a,
			const struct crypto_ec_point *b)
{
	return EC_POINT_cmp(e->group, (const EC_POINT *) a,
			    (const EC_POINT *) b, e->bnctx);
}


void crypto_ec_point_debug_print(const struct crypto_ec *e,
				 const struct crypto_ec_point *p,
				 const char *title)
{
	BIGNUM *x, *y;
	char *x_str = NULL, *y_str = NULL;

	x = BN_new();
	y = BN_new();
	if (!x || !y ||
	    EC_POINT_get_affine_coordinates(e->group, (const EC_POINT *) p,
					    x, y, e->bnctx) != 1)
		goto fail;

	x_str = BN_bn2hex(x);
	y_str = BN_bn2hex(y);
	if (!x_str || !y_str)
		goto fail;

	wpa_printf(MSG_DEBUG, "%s (%s,%s)", title, x_str, y_str);

fail:
	OPENSSL_free(x_str);
	OPENSSL_free(y_str);
	BN_free(x);
	BN_free(y);
}


struct crypto_ecdh {
	struct crypto_ec *ec;
	EVP_PKEY *pkey;
};

struct crypto_ecdh * crypto_ecdh_init(int group)
{
#if OPENSSL_VERSION_NUMBER >= 0x30000000L
	struct crypto_ecdh *ecdh;
	const char *name;

	ecdh = os_zalloc(sizeof(*ecdh));
	if (!ecdh)
		goto fail;

	ecdh->ec = crypto_ec_init(group);
	if (!ecdh->ec)
		goto fail;

	name = OSSL_EC_curve_nid2name(ecdh->ec->nid);
	if (!name)
		goto fail;

	ecdh->pkey = EVP_EC_gen(name);
	if (!ecdh->pkey)
		goto fail;

done:
	return ecdh;
fail:
	crypto_ecdh_deinit(ecdh);
	ecdh = NULL;
	goto done;
#else /* OpenSSL version >= 3.0 */
	struct crypto_ecdh *ecdh;
	EVP_PKEY *params = NULL;
	EC_KEY *ec_params = NULL;
	EVP_PKEY_CTX *kctx = NULL;

	ecdh = os_zalloc(sizeof(*ecdh));
	if (!ecdh)
		goto fail;

	ecdh->ec = crypto_ec_init(group);
	if (!ecdh->ec)
		goto fail;

	ec_params = EC_KEY_new_by_curve_name(ecdh->ec->nid);
	if (!ec_params) {
		wpa_printf(MSG_ERROR,
			   "OpenSSL: Failed to generate EC_KEY parameters");
		goto fail;
	}
	EC_KEY_set_asn1_flag(ec_params, OPENSSL_EC_NAMED_CURVE);
	params = EVP_PKEY_new();
	if (!params || EVP_PKEY_set1_EC_KEY(params, ec_params) != 1) {
		wpa_printf(MSG_ERROR,
			   "OpenSSL: Failed to generate EVP_PKEY parameters");
		goto fail;
	}

	kctx = EVP_PKEY_CTX_new(params, NULL);
	if (!kctx)
		goto fail;

	if (EVP_PKEY_keygen_init(kctx) != 1) {
		wpa_printf(MSG_ERROR,
			   "OpenSSL: EVP_PKEY_keygen_init failed: %s",
			   ERR_error_string(ERR_get_error(), NULL));
		goto fail;
	}

	if (EVP_PKEY_keygen(kctx, &ecdh->pkey) != 1) {
		wpa_printf(MSG_ERROR, "OpenSSL: EVP_PKEY_keygen failed: %s",
			   ERR_error_string(ERR_get_error(), NULL));
		goto fail;
	}

done:
	EC_KEY_free(ec_params);
	EVP_PKEY_free(params);
	EVP_PKEY_CTX_free(kctx);

	return ecdh;
fail:
	crypto_ecdh_deinit(ecdh);
	ecdh = NULL;
	goto done;
#endif /* OpenSSL version >= 3.0 */
}


struct crypto_ecdh * crypto_ecdh_init2(int group, struct crypto_ec_key *own_key)
{
#if OPENSSL_VERSION_NUMBER >= 0x30000000L
	struct crypto_ecdh *ecdh;

	ecdh = os_zalloc(sizeof(*ecdh));
	if (!ecdh)
		goto fail;

	ecdh->ec = crypto_ec_init(group);
	if (!ecdh->ec)
		goto fail;

	ecdh->pkey = EVP_PKEY_dup((EVP_PKEY *) own_key);
	if (!ecdh->pkey)
		goto fail;

	return ecdh;
fail:
	crypto_ecdh_deinit(ecdh);
	return NULL;
#else /* OpenSSL version >= 3.0 */
	struct crypto_ecdh *ecdh;

	ecdh = os_zalloc(sizeof(*ecdh));
	if (!ecdh)
		goto fail;

	ecdh->ec = crypto_ec_init(group);
	if (!ecdh->ec)
		goto fail;

	ecdh->pkey = EVP_PKEY_new();
	if (!ecdh->pkey ||
	    EVP_PKEY_assign_EC_KEY(ecdh->pkey,
				   EVP_PKEY_get1_EC_KEY((EVP_PKEY *) own_key))
	    != 1)
		goto fail;

	return ecdh;
fail:
	crypto_ecdh_deinit(ecdh);
	return NULL;
#endif /* OpenSSL version >= 3.0 */
}


struct wpabuf * crypto_ecdh_get_pubkey(struct crypto_ecdh *ecdh, int inc_y)
{
#if OPENSSL_VERSION_NUMBER >= 0x30000000L
	struct wpabuf *buf = NULL;
	unsigned char *pub;
	size_t len, exp_len;

	len = EVP_PKEY_get1_encoded_public_key(ecdh->pkey, &pub);
	if (len == 0)
		return NULL;

	/* Encoded using SECG SEC 1, Sec. 2.3.4 format */
	exp_len = 1 + 2 * crypto_ec_prime_len(ecdh->ec);
	if (len != exp_len) {
		wpa_printf(MSG_ERROR,
			   "OpenSSL:%s: Unexpected encoded public key length %zu (expected %zu)",
			   __func__, len, exp_len);
		goto fail;
	}
	buf = wpabuf_alloc_copy(pub + 1, inc_y ? len - 1 : len / 2);
fail:
	OPENSSL_free(pub);
	return buf;
#else /* OpenSSL version >= 3.0 */
	struct wpabuf *buf = NULL;
	EC_KEY *eckey;
	const EC_POINT *pubkey;
	BIGNUM *x, *y = NULL;
	int len = BN_num_bytes(ecdh->ec->prime);
	int res;

	eckey = EVP_PKEY_get1_EC_KEY(ecdh->pkey);
	if (!eckey)
		return NULL;

	pubkey = EC_KEY_get0_public_key(eckey);
	if (!pubkey)
		return NULL;

	x = BN_new();
	if (inc_y) {
		y = BN_new();
		if (!y)
			goto fail;
	}
	buf = wpabuf_alloc(inc_y ? 2 * len : len);
	if (!x || !buf)
		goto fail;

	if (EC_POINT_get_affine_coordinates(ecdh->ec->group, pubkey,
					    x, y, ecdh->ec->bnctx) != 1) {
		wpa_printf(MSG_ERROR,
			   "OpenSSL: EC_POINT_get_affine_coordinates failed: %s",
			   ERR_error_string(ERR_get_error(), NULL));
		goto fail;
	}

	res = crypto_bignum_to_bin((struct crypto_bignum *) x,
				   wpabuf_put(buf, len), len, len);
	if (res < 0)
		goto fail;

	if (inc_y) {
		res = crypto_bignum_to_bin((struct crypto_bignum *) y,
					   wpabuf_put(buf, len), len, len);
		if (res < 0)
			goto fail;
	}

done:
	BN_clear_free(x);
	BN_clear_free(y);
	EC_KEY_free(eckey);

	return buf;
fail:
	wpabuf_free(buf);
	buf = NULL;
	goto done;
#endif /* OpenSSL version >= 3.0 */
}


struct wpabuf * crypto_ecdh_set_peerkey(struct crypto_ecdh *ecdh, int inc_y,
					const u8 *key, size_t len)
{
#if OPENSSL_VERSION_NUMBER >= 0x30000000L
	EVP_PKEY *peerkey = EVP_PKEY_new();
	EVP_PKEY_CTX *ctx;
	size_t res_len;
	struct wpabuf *res = NULL;
	u8 *peer;

	/* Encode using SECG SEC 1, Sec. 2.3.4 format */
	peer = os_malloc(1 + len);
	if (!peer) {
		EVP_PKEY_free(peerkey);
		return NULL;
	}
	peer[0] = inc_y ? 0x04 : 0x02;
	os_memcpy(peer + 1, key, len);

	if (!peerkey ||
	    EVP_PKEY_copy_parameters(peerkey, ecdh->pkey) != 1 ||
	    EVP_PKEY_set1_encoded_public_key(peerkey, peer, 1 + len) != 1) {
		wpa_printf(MSG_INFO, "OpenSSL: EVP_PKEY_set1_encoded_public_key failed: %s",
			   ERR_error_string(ERR_get_error(), NULL));
		EVP_PKEY_free(peerkey);
		os_free(peer);
		return NULL;
	}
	os_free(peer);

	ctx = EVP_PKEY_CTX_new(ecdh->pkey, NULL);
	if (!ctx ||
	    EVP_PKEY_derive_init(ctx) != 1 ||
	    EVP_PKEY_derive_set_peer(ctx, peerkey) != 1 ||
	    EVP_PKEY_derive(ctx, NULL, &res_len) != 1 ||
	    !(res = wpabuf_alloc(res_len)) ||
	    EVP_PKEY_derive(ctx, wpabuf_mhead(res), &res_len) != 1) {
		wpa_printf(MSG_INFO, "OpenSSL: EVP_PKEY_derive failed: %s",
			   ERR_error_string(ERR_get_error(), NULL));
		wpabuf_free(res);
		res = NULL;
	} else {
		wpabuf_put(res, res_len);
	}

	EVP_PKEY_free(peerkey);
	EVP_PKEY_CTX_free(ctx);
	return res;
#else /* OpenSSL version >= 3.0 */
	BIGNUM *x, *y = NULL;
	EVP_PKEY_CTX *ctx = NULL;
	EVP_PKEY *peerkey = NULL;
	struct wpabuf *secret = NULL;
	size_t secret_len;
	EC_POINT *pub;
	EC_KEY *eckey = NULL;

	x = BN_bin2bn(key, inc_y ? len / 2 : len, NULL);
	pub = EC_POINT_new(ecdh->ec->group);
	if (!x || !pub)
		goto fail;

	if (inc_y) {
		y = BN_bin2bn(key + len / 2, len / 2, NULL);
		if (!y)
			goto fail;
		if (!EC_POINT_set_affine_coordinates(ecdh->ec->group, pub,
						     x, y, ecdh->ec->bnctx)) {
			wpa_printf(MSG_ERROR,
				   "OpenSSL: EC_POINT_set_affine_coordinates failed: %s",
				   ERR_error_string(ERR_get_error(), NULL));
			goto fail;
		}
	} else if (!EC_POINT_set_compressed_coordinates(ecdh->ec->group,
							pub, x, 0,
							ecdh->ec->bnctx)) {
		wpa_printf(MSG_ERROR,
			   "OpenSSL: EC_POINT_set_compressed_coordinates failed: %s",
			   ERR_error_string(ERR_get_error(), NULL));
		goto fail;
	}

	if (!EC_POINT_is_on_curve(ecdh->ec->group, pub, ecdh->ec->bnctx)) {
		wpa_printf(MSG_ERROR,
			   "OpenSSL: ECDH peer public key is not on curve");
		goto fail;
	}

	eckey = EC_KEY_new_by_curve_name(ecdh->ec->nid);
	if (!eckey || EC_KEY_set_public_key(eckey, pub) != 1) {
		wpa_printf(MSG_ERROR,
			   "OpenSSL: EC_KEY_set_public_key failed: %s",
			   ERR_error_string(ERR_get_error(), NULL));
		goto fail;
	}

	peerkey = EVP_PKEY_new();
	if (!peerkey || EVP_PKEY_set1_EC_KEY(peerkey, eckey) != 1)
		goto fail;

	ctx = EVP_PKEY_CTX_new(ecdh->pkey, NULL);
	if (!ctx || EVP_PKEY_derive_init(ctx) != 1 ||
	    EVP_PKEY_derive_set_peer(ctx, peerkey) != 1 ||
	    EVP_PKEY_derive(ctx, NULL, &secret_len) != 1) {
		wpa_printf(MSG_ERROR,
			   "OpenSSL: EVP_PKEY_derive(1) failed: %s",
			   ERR_error_string(ERR_get_error(), NULL));
		goto fail;
	}

	secret = wpabuf_alloc(secret_len);
	if (!secret)
		goto fail;
	if (EVP_PKEY_derive(ctx, wpabuf_put(secret, 0), &secret_len) != 1) {
		wpa_printf(MSG_ERROR,
			   "OpenSSL: EVP_PKEY_derive(2) failed: %s",
			   ERR_error_string(ERR_get_error(), NULL));
		goto fail;
	}
	if (secret->size != secret_len)
		wpa_printf(MSG_DEBUG,
			   "OpenSSL: EVP_PKEY_derive(2) changed secret_len %d -> %d",
			   (int) secret->size, (int) secret_len);
	wpabuf_put(secret, secret_len);

done:
	BN_free(x);
	BN_free(y);
	EC_KEY_free(eckey);
	EC_POINT_free(pub);
	EVP_PKEY_CTX_free(ctx);
	EVP_PKEY_free(peerkey);
	return secret;
fail:
	wpabuf_free(secret);
	secret = NULL;
	goto done;
#endif /* OpenSSL version >= 3.0 */
}


void crypto_ecdh_deinit(struct crypto_ecdh *ecdh)
{
	if (ecdh) {
		crypto_ec_deinit(ecdh->ec);
		EVP_PKEY_free(ecdh->pkey);
		os_free(ecdh);
	}
}


size_t crypto_ecdh_prime_len(struct crypto_ecdh *ecdh)
{
	return crypto_ec_prime_len(ecdh->ec);
}


struct crypto_ec_key * crypto_ec_key_parse_priv(const u8 *der, size_t der_len)
{
#if OPENSSL_VERSION_NUMBER >= 0x30000000L
	EVP_PKEY *pkey = NULL;
	OSSL_DECODER_CTX *ctx;

	ctx = OSSL_DECODER_CTX_new_for_pkey(
		&pkey, "DER", NULL, "EC",
		OSSL_KEYMGMT_SELECT_KEYPAIR |
		OSSL_KEYMGMT_SELECT_DOMAIN_PARAMETERS,
		NULL, NULL);
	if (!ctx ||
	    OSSL_DECODER_from_data(ctx, &der, &der_len) != 1) {
		wpa_printf(MSG_INFO,
			   "OpenSSL: Decoding EC private key (DER) failed: %s",
			   ERR_error_string(ERR_get_error(), NULL));
		if (ctx)
			OSSL_DECODER_CTX_free(ctx);
		goto fail;
	}

	OSSL_DECODER_CTX_free(ctx);
	return (struct crypto_ec_key *) pkey;
fail:
	crypto_ec_key_deinit((struct crypto_ec_key *) pkey);
	return NULL;
#else /* OpenSSL version >= 3.0 */
	EVP_PKEY *pkey = NULL;
	EC_KEY *eckey;

	eckey = d2i_ECPrivateKey(NULL, &der, der_len);
	if (!eckey) {
		wpa_printf(MSG_INFO, "OpenSSL: d2i_ECPrivateKey() failed: %s",
			   ERR_error_string(ERR_get_error(), NULL));
		goto fail;
	}
	EC_KEY_set_conv_form(eckey, POINT_CONVERSION_COMPRESSED);

	pkey = EVP_PKEY_new();
	if (!pkey || EVP_PKEY_assign_EC_KEY(pkey, eckey) != 1) {
		EC_KEY_free(eckey);
		goto fail;
	}

	return (struct crypto_ec_key *) pkey;
fail:
	crypto_ec_key_deinit((struct crypto_ec_key *) pkey);
	return NULL;
#endif /* OpenSSL version >= 3.0 */
}


struct crypto_ec_key * crypto_ec_key_set_priv(int group,
					      const u8 *raw, size_t raw_len)
{
#if OPENSSL_VERSION_NUMBER >= 0x30000000L
	const char *group_name;
	OSSL_PARAM params[4];
	EVP_PKEY_CTX *ctx = NULL;
	EVP_PKEY *pkey = NULL;
	BIGNUM *priv;
	EC_POINT *pub = NULL;
	EC_GROUP *ec_group = NULL;
	size_t len;
	u8 *pub_bin = NULL;
	u8 *priv_bin = NULL;
	int priv_bin_len;

	group_name = crypto_ec_group_2_name(group);
	if (!group_name)
		return NULL;

	priv = BN_bin2bn(raw, raw_len, NULL);
	if (!priv)
		return NULL;
	priv_bin = os_malloc(raw_len);
	if (!priv_bin)
		goto fail;
	priv_bin_len = BN_bn2lebinpad(priv, priv_bin, raw_len);
	if (priv_bin_len < 0)
		goto fail;

	ec_group = EC_GROUP_new_by_curve_name(crypto_ec_group_2_nid(group));
	if (!ec_group)
		goto fail;
	pub = EC_POINT_new(ec_group);
	if (!pub ||
	    EC_POINT_mul(ec_group, pub, priv, NULL, NULL, NULL) != 1)
		goto fail;
	len = EC_POINT_point2oct(ec_group, pub, POINT_CONVERSION_UNCOMPRESSED,
				 NULL, 0, NULL);
	if (len == 0)
		goto fail;
	pub_bin = os_malloc(len);
	if (!pub_bin)
		goto fail;
	len = EC_POINT_point2oct(ec_group, pub, POINT_CONVERSION_UNCOMPRESSED,
				 pub_bin, len, NULL);
	if (len == 0)
		goto fail;

	params[0] = OSSL_PARAM_construct_utf8_string(OSSL_PKEY_PARAM_GROUP_NAME,
						     (char *) group_name, 0);
	params[1] = OSSL_PARAM_construct_BN(OSSL_PKEY_PARAM_PRIV_KEY,
					    priv_bin, priv_bin_len);
	params[2] = OSSL_PARAM_construct_octet_string(OSSL_PKEY_PARAM_PUB_KEY,
						      pub_bin, len);
	params[3] = OSSL_PARAM_construct_end();

	ctx = EVP_PKEY_CTX_new_from_name(NULL, "EC", NULL);
	if (!ctx ||
	    EVP_PKEY_fromdata_init(ctx) <= 0 ||
	    EVP_PKEY_fromdata(ctx, &pkey, EVP_PKEY_KEYPAIR, params) <= 0)
		goto fail;

out:
	bin_clear_free(priv_bin, raw_len);
	os_free(pub_bin);
	BN_clear_free(priv);
	EVP_PKEY_CTX_free(ctx);
	EC_POINT_free(pub);
	EC_GROUP_free(ec_group);
	return (struct crypto_ec_key *) pkey;

fail:
	EVP_PKEY_free(pkey);
	pkey = NULL;
	goto out;
#else /* OpenSSL version >= 3.0 */
	EC_KEY *eckey = NULL;
	EVP_PKEY *pkey = NULL;
	BIGNUM *priv = NULL;
	int nid;
	const EC_GROUP *ec_group;
	EC_POINT *pub = NULL;

	nid = crypto_ec_group_2_nid(group);
	if (nid < 0) {
		wpa_printf(MSG_ERROR, "OpenSSL: Unsupported group %d", group);
		return NULL;
	}

	eckey = EC_KEY_new_by_curve_name(nid);
	priv = BN_bin2bn(raw, raw_len, NULL);
	if (!eckey || !priv ||
	    EC_KEY_set_private_key(eckey, priv) != 1) {
		wpa_printf(MSG_ERROR,
			   "OpenSSL: Failed to set EC_KEY: %s",
			   ERR_error_string(ERR_get_error(), NULL));
		goto fail;
	}

	ec_group = EC_KEY_get0_group(eckey);
	if (!ec_group)
		goto fail;
	pub = EC_POINT_new(ec_group);
	if (!pub ||
	    EC_POINT_mul(ec_group, pub, priv, NULL, NULL, NULL) != 1 ||
	    EC_KEY_set_public_key(eckey, pub) != 1) {
		wpa_printf(MSG_ERROR,
			   "OpenSSL: Failed to set EC_KEY(pub): %s",
			   ERR_error_string(ERR_get_error(), NULL));
		goto fail;
	}

	EC_KEY_set_asn1_flag(eckey, OPENSSL_EC_NAMED_CURVE);

	pkey = EVP_PKEY_new();
	if (!pkey || EVP_PKEY_assign_EC_KEY(pkey, eckey) != 1) {
		wpa_printf(MSG_ERROR, "OpenSSL: Could not create EVP_PKEY");
		goto fail;
	}

out:
	BN_clear_free(priv);
	EC_POINT_free(pub);
	return (struct crypto_ec_key *) pkey;

fail:
	EC_KEY_free(eckey);
	EVP_PKEY_free(pkey);
	pkey = NULL;
	goto out;
#endif /* OpenSSL version >= 3.0 */
}


struct crypto_ec_key * crypto_ec_key_parse_pub(const u8 *der, size_t der_len)
{
	EVP_PKEY *pkey;

	pkey = d2i_PUBKEY(NULL, &der, der_len);
	if (!pkey) {
		wpa_printf(MSG_INFO, "OpenSSL: d2i_PUBKEY() failed: %s",
			   ERR_error_string(ERR_get_error(), NULL));
		goto fail;
	}

	/* Ensure this is an EC key */
#if OPENSSL_VERSION_NUMBER >= 0x30000000L
	if (!EVP_PKEY_is_a(pkey, "EC"))
		goto fail;
#else /* OpenSSL version >= 3.0 */
	if (!EVP_PKEY_get0_EC_KEY(pkey))
		goto fail;
#endif /* OpenSSL version >= 3.0 */
	return (struct crypto_ec_key *) pkey;
fail:
	crypto_ec_key_deinit((struct crypto_ec_key *) pkey);
	return NULL;
}


struct crypto_ec_key * crypto_ec_key_set_pub(int group, const u8 *buf_x,
					     const u8 *buf_y, size_t len)
{
#if OPENSSL_VERSION_NUMBER >= 0x30000000L
	const char *group_name;
	OSSL_PARAM params[3];
	u8 *pub;
	EVP_PKEY_CTX *ctx;
	EVP_PKEY *pkey = NULL;

	group_name = crypto_ec_group_2_name(group);
	if (!group_name)
		return NULL;

	pub = os_malloc(1 + len * 2);
	if (!pub)
		return NULL;
	pub[0] = 0x04; /* uncompressed */
	os_memcpy(pub + 1, buf_x, len);
	os_memcpy(pub + 1 + len, buf_y, len);

	params[0] = OSSL_PARAM_construct_utf8_string(OSSL_PKEY_PARAM_GROUP_NAME,
						     (char *) group_name, 0);
	params[1] = OSSL_PARAM_construct_octet_string(OSSL_PKEY_PARAM_PUB_KEY,
						      pub, 1 + len * 2);
	params[2] = OSSL_PARAM_construct_end();

	ctx = EVP_PKEY_CTX_new_from_name(NULL, "EC", NULL);
	if (!ctx) {
		os_free(pub);
		return NULL;
	}
	if (EVP_PKEY_fromdata_init(ctx) <= 0 ||
	    EVP_PKEY_fromdata(ctx, &pkey, EVP_PKEY_PUBLIC_KEY, params) <= 0) {
		os_free(pub);
		EVP_PKEY_CTX_free(ctx);
		return NULL;
	}

	os_free(pub);
	EVP_PKEY_CTX_free(ctx);

	return (struct crypto_ec_key *) pkey;
#else /* OpenSSL version >= 3.0 */
	EC_KEY *eckey = NULL;
	EVP_PKEY *pkey = NULL;
	EC_GROUP *ec_group = NULL;
	BN_CTX *ctx;
	EC_POINT *point = NULL;
	BIGNUM *x = NULL, *y = NULL;
	int nid;

	if (!buf_x || !buf_y)
		return NULL;

	nid = crypto_ec_group_2_nid(group);
	if (nid < 0) {
		wpa_printf(MSG_ERROR, "OpenSSL: Unsupported group %d", group);
		return NULL;
	}

	ctx = BN_CTX_new();
	if (!ctx)
		goto fail;

	ec_group = EC_GROUP_new_by_curve_name(nid);
	if (!ec_group)
		goto fail;

	x = BN_bin2bn(buf_x, len, NULL);
	y = BN_bin2bn(buf_y, len, NULL);
	point = EC_POINT_new(ec_group);
	if (!x || !y || !point)
		goto fail;

	if (!EC_POINT_set_affine_coordinates(ec_group, point, x, y, ctx)) {
		wpa_printf(MSG_ERROR,
			   "OpenSSL: EC_POINT_set_affine_coordinates failed: %s",
			   ERR_error_string(ERR_get_error(), NULL));
		goto fail;
	}

	if (!EC_POINT_is_on_curve(ec_group, point, ctx) ||
	    EC_POINT_is_at_infinity(ec_group, point)) {
		wpa_printf(MSG_ERROR, "OpenSSL: Invalid point");
		goto fail;
	}

	eckey = EC_KEY_new();
	if (!eckey ||
	    EC_KEY_set_group(eckey, ec_group) != 1 ||
	    EC_KEY_set_public_key(eckey, point) != 1) {
		wpa_printf(MSG_ERROR,
			   "OpenSSL: Failed to set EC_KEY: %s",
			   ERR_error_string(ERR_get_error(), NULL));
		goto fail;
	}
	EC_KEY_set_asn1_flag(eckey, OPENSSL_EC_NAMED_CURVE);

	pkey = EVP_PKEY_new();
	if (!pkey || EVP_PKEY_assign_EC_KEY(pkey, eckey) != 1) {
		wpa_printf(MSG_ERROR, "OpenSSL: Could not create EVP_PKEY");
		goto fail;
	}

out:
	EC_GROUP_free(ec_group);
	BN_free(x);
	BN_free(y);
	EC_POINT_free(point);
	BN_CTX_free(ctx);
	return (struct crypto_ec_key *) pkey;

fail:
	EC_KEY_free(eckey);
	EVP_PKEY_free(pkey);
	pkey = NULL;
	goto out;
#endif /* OpenSSL version >= 3.0 */
}


struct crypto_ec_key *
crypto_ec_key_set_pub_point(struct crypto_ec *ec,
			    const struct crypto_ec_point *pub)
{
#if OPENSSL_VERSION_NUMBER >= 0x30000000L
	int len = BN_num_bytes(ec->prime);
	struct crypto_ec_key *key;
	u8 *buf;

	buf = os_malloc(2 * len);
	if (!buf)
		return NULL;
	if (crypto_ec_point_to_bin(ec, pub, buf, buf + len) < 0) {
		os_free(buf);
		return NULL;
	}

	key = crypto_ec_key_set_pub(ec->iana_group, buf, buf + len, len);
	os_free(buf);

	return key;
#else /* OpenSSL version >= 3.0 */
	EC_KEY *eckey;
	EVP_PKEY *pkey = NULL;

	eckey = EC_KEY_new();
	if (!eckey ||
	    EC_KEY_set_group(eckey, ec->group) != 1 ||
	    EC_KEY_set_public_key(eckey, (const EC_POINT *) pub) != 1) {
		wpa_printf(MSG_ERROR,
			   "OpenSSL: Failed to set EC_KEY: %s",
			   ERR_error_string(ERR_get_error(), NULL));
		goto fail;
	}
	EC_KEY_set_asn1_flag(eckey, OPENSSL_EC_NAMED_CURVE);

	pkey = EVP_PKEY_new();
	if (!pkey || EVP_PKEY_assign_EC_KEY(pkey, eckey) != 1) {
		wpa_printf(MSG_ERROR, "OpenSSL: Could not create EVP_PKEY");
		goto fail;
	}

out:
	return (struct crypto_ec_key *) pkey;

fail:
	EVP_PKEY_free(pkey);
	EC_KEY_free(eckey);
	pkey = NULL;
	goto out;
#endif /* OpenSSL version >= 3.0 */
}


struct crypto_ec_key * crypto_ec_key_gen(int group)
{
#if OPENSSL_VERSION_NUMBER >= 0x30000000L
	EVP_PKEY_CTX *ctx;
	OSSL_PARAM params[2];
	const char *group_name;
	EVP_PKEY *pkey = NULL;

	group_name = crypto_ec_group_2_name(group);
	if (!group_name)
		return NULL;

	params[0] = OSSL_PARAM_construct_utf8_string(OSSL_PKEY_PARAM_GROUP_NAME,
						     (char *) group_name, 0);
	params[1] = OSSL_PARAM_construct_end();

	ctx = EVP_PKEY_CTX_new_from_name(NULL, "EC", NULL);
	if (!ctx ||
	    EVP_PKEY_keygen_init(ctx) != 1 ||
	    EVP_PKEY_CTX_set_params(ctx, params) != 1 ||
	    EVP_PKEY_generate(ctx, &pkey) != 1) {
		wpa_printf(MSG_INFO,
			   "OpenSSL: failed to generate EC keypair: %s",
			   ERR_error_string(ERR_get_error(), NULL));
		pkey = NULL;
	}

	EVP_PKEY_CTX_free(ctx);

	return (struct crypto_ec_key *) pkey;
#else /* OpenSSL version >= 3.0 */
	EVP_PKEY_CTX *kctx = NULL;
	EC_KEY *ec_params = NULL, *eckey;
	EVP_PKEY *params = NULL, *key = NULL;
	int nid;

	nid = crypto_ec_group_2_nid(group);
	if (nid < 0) {
		wpa_printf(MSG_ERROR, "OpenSSL: Unsupported group %d", group);
		return NULL;
	}

	ec_params = EC_KEY_new_by_curve_name(nid);
	if (!ec_params) {
		wpa_printf(MSG_ERROR,
			   "OpenSSL: Failed to generate EC_KEY parameters");
		goto fail;
	}
	EC_KEY_set_asn1_flag(ec_params, OPENSSL_EC_NAMED_CURVE);
	params = EVP_PKEY_new();
	if (!params || EVP_PKEY_set1_EC_KEY(params, ec_params) != 1) {
		wpa_printf(MSG_ERROR,
			   "OpenSSL: Failed to generate EVP_PKEY parameters");
		goto fail;
	}

	kctx = EVP_PKEY_CTX_new(params, NULL);
	if (!kctx ||
	    EVP_PKEY_keygen_init(kctx) != 1 ||
	    EVP_PKEY_keygen(kctx, &key) != 1) {
		wpa_printf(MSG_ERROR, "OpenSSL: Failed to generate EC key");
		key = NULL;
		goto fail;
	}

	eckey = EVP_PKEY_get1_EC_KEY(key);
	if (!eckey) {
		key = NULL;
		goto fail;
	}
	EC_KEY_set_conv_form(eckey, POINT_CONVERSION_COMPRESSED);
	EC_KEY_free(eckey);

fail:
	EC_KEY_free(ec_params);
	EVP_PKEY_free(params);
	EVP_PKEY_CTX_free(kctx);
	return (struct crypto_ec_key *) key;
#endif /* OpenSSL version >= 3.0 */
}


void crypto_ec_key_deinit(struct crypto_ec_key *key)
{
	EVP_PKEY_free((EVP_PKEY *) key);
}


#ifdef OPENSSL_IS_BORINGSSL

/* BoringSSL version of i2d_PUBKEY() always outputs public EC key using
 * uncompressed form so define a custom function to export EC pubkey using
 * the compressed format that is explicitly required for some protocols. */

#include <openssl/asn1.h>
#include <openssl/asn1t.h>

typedef struct {
	/* AlgorithmIdentifier ecPublicKey with optional parameters present
	 * as an OID identifying the curve */
	X509_ALGOR *alg;
	/* Compressed format public key per ANSI X9.63 */
	ASN1_BIT_STRING *pub_key;
} EC_COMP_PUBKEY;

ASN1_SEQUENCE(EC_COMP_PUBKEY) = {
	ASN1_SIMPLE(EC_COMP_PUBKEY, alg, X509_ALGOR),
	ASN1_SIMPLE(EC_COMP_PUBKEY, pub_key, ASN1_BIT_STRING)
} ASN1_SEQUENCE_END(EC_COMP_PUBKEY);

IMPLEMENT_ASN1_FUNCTIONS(EC_COMP_PUBKEY);

#endif /* OPENSSL_IS_BORINGSSL */


struct wpabuf * crypto_ec_key_get_subject_public_key(struct crypto_ec_key *key)
{
	EVP_PKEY *pkey = (EVP_PKEY *) key;
#if OPENSSL_VERSION_NUMBER >= 0x30000000L
	OSSL_ENCODER_CTX *ctx;
	int selection;
	unsigned char *pdata = NULL;
	size_t pdata_len = 0;
	EVP_PKEY *copy = NULL;
	struct wpabuf *buf = NULL;

	if (EVP_PKEY_get_ec_point_conv_form(pkey) !=
	    POINT_CONVERSION_COMPRESSED) {
		copy = EVP_PKEY_dup(pkey);
		if (!copy)
			return NULL;
		if (EVP_PKEY_set_utf8_string_param(
			    copy, OSSL_PKEY_PARAM_EC_POINT_CONVERSION_FORMAT,
			    OSSL_PKEY_EC_POINT_CONVERSION_FORMAT_COMPRESSED) !=
		    1) {
			wpa_printf(MSG_INFO,
				   "OpenSSL: Failed to set compressed format");
			EVP_PKEY_free(copy);
			return NULL;
		}
		pkey = copy;
	}

	selection = OSSL_KEYMGMT_SELECT_ALL_PARAMETERS |
		OSSL_KEYMGMT_SELECT_PUBLIC_KEY;

	ctx = OSSL_ENCODER_CTX_new_for_pkey(pkey, selection, "DER",
					    "SubjectPublicKeyInfo",
					    NULL);
	if (!ctx || OSSL_ENCODER_to_data(ctx, &pdata, &pdata_len) != 1) {
		wpa_printf(MSG_INFO,
			   "OpenSSL: Failed to encode SubjectPublicKeyInfo: %s",
			   ERR_error_string(ERR_get_error(), NULL));
		pdata = NULL;
	}
	OSSL_ENCODER_CTX_free(ctx);
	if (pdata) {
		buf = wpabuf_alloc_copy(pdata, pdata_len);
		OPENSSL_free(pdata);
	}

	EVP_PKEY_free(copy);

	return buf;
#else /* OpenSSL version >= 3.0 */
#ifdef OPENSSL_IS_BORINGSSL
	unsigned char *der = NULL;
	int der_len;
	const EC_KEY *eckey;
	struct wpabuf *ret = NULL;
	size_t len;
	const EC_GROUP *group;
	const EC_POINT *point;
	BN_CTX *ctx;
	EC_COMP_PUBKEY *pubkey = NULL;
	int nid;

	ctx = BN_CTX_new();
	eckey = EVP_PKEY_get0_EC_KEY(pkey);
	if (!ctx || !eckey)
		goto fail;

	group = EC_KEY_get0_group(eckey);
	point = EC_KEY_get0_public_key(eckey);
	if (!group || !point)
		goto fail;
	nid = EC_GROUP_get_curve_name(group);

	pubkey = EC_COMP_PUBKEY_new();
	if (!pubkey ||
	    X509_ALGOR_set0(pubkey->alg, OBJ_nid2obj(EVP_PKEY_EC),
			    V_ASN1_OBJECT, (void *) OBJ_nid2obj(nid)) != 1)
		goto fail;

	len = EC_POINT_point2oct(group, point, POINT_CONVERSION_COMPRESSED,
				 NULL, 0, ctx);
	if (len == 0)
		goto fail;

	der = OPENSSL_malloc(len);
	if (!der)
		goto fail;
	len = EC_POINT_point2oct(group, point, POINT_CONVERSION_COMPRESSED,
				 der, len, ctx);

	OPENSSL_free(pubkey->pub_key->data);
	pubkey->pub_key->data = der;
	der = NULL;
	pubkey->pub_key->length = len;
	/* No unused bits */
	pubkey->pub_key->flags &= ~(ASN1_STRING_FLAG_BITS_LEFT | 0x07);
	pubkey->pub_key->flags |= ASN1_STRING_FLAG_BITS_LEFT;

	der_len = i2d_EC_COMP_PUBKEY(pubkey, &der);
	if (der_len <= 0) {
		wpa_printf(MSG_ERROR,
			   "BoringSSL: Failed to build DER encoded public key");
		goto fail;
	}

	ret = wpabuf_alloc_copy(der, der_len);
fail:
	EC_COMP_PUBKEY_free(pubkey);
	OPENSSL_free(der);
	BN_CTX_free(ctx);
	return ret;
#else /* OPENSSL_IS_BORINGSSL */
	unsigned char *der = NULL;
	int der_len;
	struct wpabuf *buf;
	EC_KEY *eckey;

	eckey = EVP_PKEY_get1_EC_KEY(pkey);
	if (!eckey)
		return NULL;

	/* For now, all users expect COMPRESSED form */
	EC_KEY_set_conv_form(eckey, POINT_CONVERSION_COMPRESSED);

	der_len = i2d_PUBKEY((EVP_PKEY *) key, &der);
	EC_KEY_free(eckey);
	if (der_len <= 0) {
		wpa_printf(MSG_INFO, "OpenSSL: i2d_PUBKEY() failed: %s",
			   ERR_error_string(ERR_get_error(), NULL));
		return NULL;
	}

	buf = wpabuf_alloc_copy(der, der_len);
	OPENSSL_free(der);
	return buf;
#endif /* OPENSSL_IS_BORINGSSL */
#endif /* OpenSSL version >= 3.0 */
}


struct wpabuf * crypto_ec_key_get_ecprivate_key(struct crypto_ec_key *key,
						bool include_pub)
{
	EVP_PKEY *pkey = (EVP_PKEY *) key;
#if OPENSSL_VERSION_NUMBER >= 0x30000000L
	OSSL_ENCODER_CTX *ctx;
	int selection;
	unsigned char *pdata = NULL;
	size_t pdata_len = 0;
	struct wpabuf *buf;
	EVP_PKEY *copy = NULL;

	selection = OSSL_KEYMGMT_SELECT_DOMAIN_PARAMETERS |
		OSSL_KEYMGMT_SELECT_PRIVATE_KEY;
	if (include_pub) {
		selection |= OSSL_KEYMGMT_SELECT_PUBLIC_KEY;
	} else {
		/* Not including OSSL_KEYMGMT_SELECT_PUBLIC_KEY does not seem
		 * to really be sufficient, so clone the key and explicitly
		 * mark it not to include the public key. */
		copy = EVP_PKEY_dup(pkey);
		if (!copy)
			return NULL;
		EVP_PKEY_set_int_param(copy, OSSL_PKEY_PARAM_EC_INCLUDE_PUBLIC,
				       0);
		pkey = copy;
	}

	ctx = OSSL_ENCODER_CTX_new_for_pkey(pkey, selection, "DER",
					    "type-specific", NULL);
	if (!ctx || OSSL_ENCODER_to_data(ctx, &pdata, &pdata_len) != 1) {
		OSSL_ENCODER_CTX_free(ctx);
		EVP_PKEY_free(copy);
		return NULL;
	}
	OSSL_ENCODER_CTX_free(ctx);
	buf = wpabuf_alloc_copy(pdata, pdata_len);
	OPENSSL_free(pdata);
	EVP_PKEY_free(copy);
	return buf;
#else /* OpenSSL version >= 3.0 */
	EC_KEY *eckey;
	unsigned char *der = NULL;
	int der_len;
	struct wpabuf *buf;
	unsigned int key_flags;

	eckey = EVP_PKEY_get1_EC_KEY(pkey);
	if (!eckey)
		return NULL;

	key_flags = EC_KEY_get_enc_flags(eckey);
	if (include_pub)
		key_flags &= ~EC_PKEY_NO_PUBKEY;
	else
		key_flags |= EC_PKEY_NO_PUBKEY;
	EC_KEY_set_enc_flags(eckey, key_flags);

	EC_KEY_set_conv_form(eckey, POINT_CONVERSION_UNCOMPRESSED);

	der_len = i2d_ECPrivateKey(eckey, &der);
	EC_KEY_free(eckey);
	if (der_len <= 0)
		return NULL;
	buf = wpabuf_alloc_copy(der, der_len);
	OPENSSL_free(der);

	return buf;
#endif /* OpenSSL version >= 3.0 */
}


struct wpabuf * crypto_ec_key_get_pubkey_point(struct crypto_ec_key *key,
					       int prefix)
{
	EVP_PKEY *pkey = (EVP_PKEY *) key;
#if OPENSSL_VERSION_NUMBER >= 0x30000000L
	struct wpabuf *buf;
	unsigned char *pos;
	size_t pub_len = OSSL_PARAM_UNMODIFIED;

	buf = NULL;
	if (!EVP_PKEY_is_a(pkey, "EC") ||
	    EVP_PKEY_get_octet_string_param(pkey,
					    OSSL_PKEY_PARAM_ENCODED_PUBLIC_KEY,
					    NULL, 0, &pub_len) < 0 ||
	    pub_len == OSSL_PARAM_UNMODIFIED ||
	    !(buf = wpabuf_alloc(pub_len)) ||
	    EVP_PKEY_get_octet_string_param(pkey,
					    OSSL_PKEY_PARAM_ENCODED_PUBLIC_KEY,
					    wpabuf_put(buf, pub_len),
					    pub_len, NULL) != 1 ||
	    wpabuf_head_u8(buf)[0] != 0x04) {
		wpa_printf(MSG_INFO,
			   "OpenSSL: Failed to get encoded public key: %s",
			   ERR_error_string(ERR_get_error(), NULL));
		wpabuf_free(buf);
		return NULL;
	}

	if (!prefix) {
		/* Remove 0x04 prefix if requested */
		pos = wpabuf_mhead(buf);
		os_memmove(pos, pos + 1, pub_len - 1);
		buf->used--;
	}

	return buf;
#else /* OpenSSL version >= 3.0 */
	int len, res;
	EC_KEY *eckey;
	struct wpabuf *buf;
	unsigned char *pos;

	eckey = EVP_PKEY_get1_EC_KEY(pkey);
	if (!eckey)
		return NULL;
	EC_KEY_set_conv_form(eckey, POINT_CONVERSION_UNCOMPRESSED);
	len = i2o_ECPublicKey(eckey, NULL);
	if (len <= 0) {
		wpa_printf(MSG_ERROR,
			   "OpenSSL: Failed to determine public key encoding length");
		EC_KEY_free(eckey);
		return NULL;
	}

	buf = wpabuf_alloc(len);
	if (!buf) {
		EC_KEY_free(eckey);
		return NULL;
	}

	pos = wpabuf_put(buf, len);
	res = i2o_ECPublicKey(eckey, &pos);
	EC_KEY_free(eckey);
	if (res != len) {
		wpa_printf(MSG_ERROR,
			   "OpenSSL: Failed to encode public key (res=%d/%d)",
			   res, len);
		wpabuf_free(buf);
		return NULL;
	}

	if (!prefix) {
		/* Remove 0x04 prefix if requested */
		pos = wpabuf_mhead(buf);
		os_memmove(pos, pos + 1, len - 1);
		buf->used--;
	}

	return buf;
#endif /* OpenSSL version >= 3.0 */
}


struct crypto_ec_point *
crypto_ec_key_get_public_key(struct crypto_ec_key *key)
{
	EVP_PKEY *pkey = (EVP_PKEY *) key;
#if OPENSSL_VERSION_NUMBER >= 0x30000000L
	char group[64];
	unsigned char pub[256];
	size_t len;
	EC_POINT *point = NULL;
	EC_GROUP *grp;
	int res = 0;
	OSSL_PARAM params[2];

	if (!EVP_PKEY_is_a(pkey, "EC") ||
	    EVP_PKEY_get_utf8_string_param(pkey, OSSL_PKEY_PARAM_GROUP_NAME,
					   group, sizeof(group), &len) != 1 ||
	    EVP_PKEY_get_octet_string_param(pkey, OSSL_PKEY_PARAM_PUB_KEY,
					    pub, sizeof(pub), &len) != 1)
		return NULL;

	params[0] = OSSL_PARAM_construct_utf8_string(OSSL_PKEY_PARAM_GROUP_NAME,
						     group, 0);
	params[1] = OSSL_PARAM_construct_end();
	grp = EC_GROUP_new_from_params(params, NULL, NULL);
	if (!grp)
		goto fail;
	point = EC_POINT_new(grp);
	if (!point)
		goto fail;
	res = EC_POINT_oct2point(grp, point, pub, len, NULL);

fail:
	if (res != 1) {
		EC_POINT_free(point);
		point = NULL;
	}

	EC_GROUP_free(grp);

	return (struct crypto_ec_point *) point;
#else /* OpenSSL version >= 3.0 */
	const EC_KEY *eckey;
	const EC_POINT *point;
	const EC_GROUP *group;

	eckey = EVP_PKEY_get0_EC_KEY(pkey);
	if (!eckey)
		return NULL;
	group = EC_KEY_get0_group(eckey);
	if (!group)
		return NULL;
	point = EC_KEY_get0_public_key(eckey);
	if (!point)
		return NULL;
	return (struct crypto_ec_point *) EC_POINT_dup(point, group);
#endif /* OpenSSL version >= 3.0 */
}


struct crypto_bignum *
crypto_ec_key_get_private_key(struct crypto_ec_key *key)
{
	EVP_PKEY *pkey = (EVP_PKEY *) key;
#if OPENSSL_VERSION_NUMBER >= 0x30000000L
	BIGNUM *bn = NULL;

	if (!EVP_PKEY_is_a(pkey, "EC") ||
	    EVP_PKEY_get_bn_param(pkey, OSSL_PKEY_PARAM_PRIV_KEY, &bn) != 1)
		return NULL;
	return (struct crypto_bignum *) bn;
#else /* OpenSSL version >= 3.0 */
	const EC_KEY *eckey;
	const BIGNUM *bn;

	eckey = EVP_PKEY_get0_EC_KEY(pkey);
	if (!eckey)
		return NULL;
	bn = EC_KEY_get0_private_key(eckey);
	if (!bn)
		return NULL;
	return (struct crypto_bignum *) BN_dup(bn);
#endif /* OpenSSL version >= 3.0 */
}


struct wpabuf * crypto_ec_key_sign(struct crypto_ec_key *key, const u8 *data,
				   size_t len)
{
	EVP_PKEY_CTX *pkctx;
	struct wpabuf *sig_der;
	size_t sig_len;

	sig_len = EVP_PKEY_size((EVP_PKEY *) key);
	sig_der = wpabuf_alloc(sig_len);
	if (!sig_der)
		return NULL;

	pkctx = EVP_PKEY_CTX_new((EVP_PKEY *) key, NULL);
	if (!pkctx ||
	    EVP_PKEY_sign_init(pkctx) <= 0 ||
	    EVP_PKEY_sign(pkctx, wpabuf_put(sig_der, 0), &sig_len,
			  data, len) <= 0) {
		wpabuf_free(sig_der);
		sig_der = NULL;
	} else {
		wpabuf_put(sig_der, sig_len);
	}

	EVP_PKEY_CTX_free(pkctx);
	return sig_der;
}


static int openssl_evp_pkey_ec_prime_len(struct crypto_ec_key *key)
{
#if OPENSSL_VERSION_NUMBER >= 0x30000000L
	char gname[50];
	int nid;
	EC_GROUP *group;
	BIGNUM *prime = NULL;
	int prime_len = -1;

	if (EVP_PKEY_get_group_name((EVP_PKEY *) key, gname, sizeof(gname),
				    NULL) != 1)
		return -1;
	nid = OBJ_txt2nid(gname);
	group = EC_GROUP_new_by_curve_name(nid);
	prime = BN_new();
	if (!group || !prime)
		goto fail;
	if (EC_GROUP_get_curve(group, prime, NULL, NULL, NULL) == 1)
		prime_len = BN_num_bytes(prime);
fail:
	EC_GROUP_free(group);
	BN_free(prime);
	return prime_len;
#else
	const EC_GROUP *group;
	const EC_KEY *eckey;
	BIGNUM *prime = NULL;
	int prime_len = -1;

	eckey = EVP_PKEY_get0_EC_KEY((EVP_PKEY *) key);
	if (!eckey)
		goto fail;
	group = EC_KEY_get0_group(eckey);
	prime = BN_new();
	if (!prime || !group ||
	    !EC_GROUP_get_curve(group, prime, NULL, NULL, NULL))
		goto fail;
	prime_len = BN_num_bytes(prime);
fail:
	BN_free(prime);
	return prime_len;
#endif
}


struct wpabuf * crypto_ec_key_sign_r_s(struct crypto_ec_key *key,
				       const u8 *data, size_t len)
{
	ECDSA_SIG *sig = NULL;
	const BIGNUM *r, *s;
	u8 *r_buf, *s_buf;
	struct wpabuf *buf;
	const unsigned char *p;
	int prime_len;

	prime_len = openssl_evp_pkey_ec_prime_len(key);
	if (prime_len < 0)
		return NULL;

	buf = crypto_ec_key_sign(key, data, len);
	if (!buf)
		return NULL;

	/* Extract (r,s) from Ecdsa-Sig-Value */

	p = wpabuf_head(buf);
	sig = d2i_ECDSA_SIG(NULL, &p, wpabuf_len(buf));
	if (!sig)
		goto fail;
	ECDSA_SIG_get0(sig, &r, &s);

	/* Re-use wpabuf returned by crypto_ec_key_sign() */
	buf->used = 0;
	r_buf = wpabuf_put(buf, prime_len);
	s_buf = wpabuf_put(buf, prime_len);
	if (crypto_bignum_to_bin((const struct crypto_bignum *) r, r_buf,
				 prime_len, prime_len) < 0 ||
	    crypto_bignum_to_bin((const struct crypto_bignum *) s, s_buf,
				 prime_len, prime_len) < 0)
		goto fail;

out:
	ECDSA_SIG_free(sig);
	return buf;
fail:
	wpabuf_clear_free(buf);
	buf = NULL;
	goto out;
}


int crypto_ec_key_verify_signature(struct crypto_ec_key *key, const u8 *data,
				   size_t len, const u8 *sig, size_t sig_len)
{
	EVP_PKEY_CTX *pkctx;
	int ret;

	pkctx = EVP_PKEY_CTX_new((EVP_PKEY *) key, NULL);
	if (!pkctx || EVP_PKEY_verify_init(pkctx) <= 0) {
		EVP_PKEY_CTX_free(pkctx);
		return -1;
	}

	ret = EVP_PKEY_verify(pkctx, sig, sig_len, data, len);
	EVP_PKEY_CTX_free(pkctx);
	if (ret == 1)
		return 1; /* signature ok */
	if (ret == 0)
		return 0; /* incorrect signature */
	return -1;
}


int crypto_ec_key_verify_signature_r_s(struct crypto_ec_key *key,
				       const u8 *data, size_t len,
				       const u8 *r, size_t r_len,
				       const u8 *s, size_t s_len)
{
	ECDSA_SIG *sig;
	BIGNUM *r_bn, *s_bn;
	unsigned char *der = NULL;
	int der_len;
	int ret = -1;

	r_bn = BN_bin2bn(r, r_len, NULL);
	s_bn = BN_bin2bn(s, s_len, NULL);
	sig = ECDSA_SIG_new();
	if (!r_bn || !s_bn || !sig || ECDSA_SIG_set0(sig, r_bn, s_bn) != 1)
		goto fail;
	r_bn = NULL;
	s_bn = NULL;

	der_len = i2d_ECDSA_SIG(sig, &der);
	if (der_len <= 0) {
		wpa_printf(MSG_DEBUG,
			   "OpenSSL: Could not DER encode signature");
		goto fail;
	}

	ret = crypto_ec_key_verify_signature(key, data, len, der, der_len);

fail:
	OPENSSL_free(der);
	BN_free(r_bn);
	BN_free(s_bn);
	ECDSA_SIG_free(sig);
	return ret;
}


int crypto_ec_key_group(struct crypto_ec_key *key)
{
#if OPENSSL_VERSION_NUMBER >= 0x30000000L
	char gname[50];
	int nid;

	if (EVP_PKEY_get_group_name((EVP_PKEY *) key, gname, sizeof(gname),
				    NULL) != 1)
		return -1;
	nid = OBJ_txt2nid(gname);
#else
	const EC_KEY *eckey;
	const EC_GROUP *group;
	int nid;

	eckey = EVP_PKEY_get0_EC_KEY((EVP_PKEY *) key);
	if (!eckey)
		return -1;
	group = EC_KEY_get0_group(eckey);
	if (!group)
		return -1;
	nid = EC_GROUP_get_curve_name(group);
#endif
	switch (nid) {
	case NID_X9_62_prime256v1:
		return 19;
	case NID_secp384r1:
		return 20;
	case NID_secp521r1:
		return 21;
#ifdef NID_brainpoolP256r1
	case NID_brainpoolP256r1:
		return 28;
#endif /* NID_brainpoolP256r1 */
#ifdef NID_brainpoolP384r1
	case NID_brainpoolP384r1:
		return 29;
#endif /* NID_brainpoolP384r1 */
#ifdef NID_brainpoolP512r1
	case NID_brainpoolP512r1:
		return 30;
#endif /* NID_brainpoolP512r1 */
	default:
		wpa_printf(MSG_ERROR,
			   "OpenSSL: Unsupported curve (nid=%d) in EC key",
			   nid);
		return -1;
	}
}


int crypto_ec_key_cmp(struct crypto_ec_key *key1, struct crypto_ec_key *key2)
{
#if OPENSSL_VERSION_NUMBER >= 0x30000000L
	if (EVP_PKEY_eq((EVP_PKEY *) key1, (EVP_PKEY *) key2) != 1)
		return -1;
#else
	if (EVP_PKEY_cmp((EVP_PKEY *) key1, (EVP_PKEY *) key2) != 1)
		return -1;
#endif
	return 0;
}


void crypto_ec_key_debug_print(const struct crypto_ec_key *key,
			       const char *title)
{
	BIO *out;
	size_t rlen;
	char *txt;
	int res;

	out = BIO_new(BIO_s_mem());
	if (!out)
		return;

	EVP_PKEY_print_private(out, (EVP_PKEY *) key, 0, NULL);
	rlen = BIO_ctrl_pending(out);
	txt = os_malloc(rlen + 1);
	if (txt) {
		res = BIO_read(out, txt, rlen);
		if (res > 0) {
			txt[res] = '\0';
			wpa_printf(MSG_DEBUG, "%s: %s", title, txt);
		}
		os_free(txt);
	}
	BIO_free(out);
}


struct wpabuf * crypto_pkcs7_get_certificates(const struct wpabuf *pkcs7)
{
#ifdef OPENSSL_IS_BORINGSSL
	CBS pkcs7_cbs;
#else /* OPENSSL_IS_BORINGSSL */
	PKCS7 *p7 = NULL;
	const unsigned char *p = wpabuf_head(pkcs7);
#endif /* OPENSSL_IS_BORINGSSL */
	STACK_OF(X509) *certs;
	int i, num;
	BIO *out = NULL;
	size_t rlen;
	struct wpabuf *pem = NULL;
	int res;

#ifdef OPENSSL_IS_BORINGSSL
	certs = sk_X509_new_null();
	if (!certs)
		goto fail;
	CBS_init(&pkcs7_cbs, wpabuf_head(pkcs7), wpabuf_len(pkcs7));
	if (!PKCS7_get_certificates(certs, &pkcs7_cbs)) {
		wpa_printf(MSG_INFO,
			   "OpenSSL: Could not parse PKCS#7 object: %s",
			   ERR_error_string(ERR_get_error(), NULL));
		goto fail;
	}
#else /* OPENSSL_IS_BORINGSSL */
	p7 = d2i_PKCS7(NULL, &p, wpabuf_len(pkcs7));
	if (!p7) {
		wpa_printf(MSG_INFO,
			   "OpenSSL: Could not parse PKCS#7 object: %s",
			   ERR_error_string(ERR_get_error(), NULL));
		goto fail;
	}

	switch (OBJ_obj2nid(p7->type)) {
	case NID_pkcs7_signed:
		certs = p7->d.sign->cert;
		break;
	case NID_pkcs7_signedAndEnveloped:
		certs = p7->d.signed_and_enveloped->cert;
		break;
	default:
		certs = NULL;
		break;
	}
#endif /* OPENSSL_IS_BORINGSSL */

	if (!certs || ((num = sk_X509_num(certs)) == 0)) {
		wpa_printf(MSG_INFO,
			   "OpenSSL: No certificates found in PKCS#7 object");
		goto fail;
	}

	out = BIO_new(BIO_s_mem());
	if (!out)
		goto fail;

	for (i = 0; i < num; i++) {
		X509 *cert = sk_X509_value(certs, i);

		PEM_write_bio_X509(out, cert);
	}

	rlen = BIO_ctrl_pending(out);
	pem = wpabuf_alloc(rlen);
	if (!pem)
		goto fail;
	res = BIO_read(out, wpabuf_put(pem, 0), rlen);
	if (res <= 0) {
		wpabuf_free(pem);
		pem = NULL;
		goto fail;
	}
	wpabuf_put(pem, res);

fail:
#ifdef OPENSSL_IS_BORINGSSL
	if (certs)
		sk_X509_pop_free(certs, X509_free);
#else /* OPENSSL_IS_BORINGSSL */
	PKCS7_free(p7);
#endif /* OPENSSL_IS_BORINGSSL */
	if (out)
		BIO_free_all(out);

	return pem;
}


struct crypto_csr * crypto_csr_init()
{
	return (struct crypto_csr *)X509_REQ_new();
}


struct crypto_csr * crypto_csr_verify(const struct wpabuf *req)
{
	X509_REQ *csr;
	EVP_PKEY *pkey = NULL;
	const u8 *der = wpabuf_head(req);

	csr = d2i_X509_REQ(NULL, &der, wpabuf_len(req));
	if (!csr)
		return NULL;

	pkey = X509_REQ_get_pubkey((X509_REQ *)csr);
	if (!pkey)
		goto fail;

	if (X509_REQ_verify((X509_REQ *)csr, pkey) != 1)
		goto fail;

	return (struct crypto_csr *)csr;
fail:
	X509_REQ_free(csr);
	return NULL;
}


void crypto_csr_deinit(struct crypto_csr *csr)
{
	X509_REQ_free((X509_REQ *)csr);
}


int crypto_csr_set_ec_public_key(struct crypto_csr *csr, struct crypto_ec_key *key)
{
	if (!X509_REQ_set_pubkey((X509_REQ *)csr, (EVP_PKEY *)key))
		return -1;

	return 0;
}


int crypto_csr_set_name(struct crypto_csr *csr, enum crypto_csr_name type,
			const char *name)
{
	X509_NAME *n;
	int nid;

	switch (type) {
	case CSR_NAME_CN:
		nid = NID_commonName;
		break;
	case CSR_NAME_SN:
		nid = NID_surname;
		break;
	case CSR_NAME_C:
		nid = NID_countryName;
		break;
	case CSR_NAME_O:
		nid = NID_organizationName;
		break;
	case CSR_NAME_OU:
		nid = NID_organizationalUnitName;
		break;
	default:
		return -1;
	}

	n = X509_REQ_get_subject_name((X509_REQ *) csr);
	if (!n)
		return -1;

#if OPENSSL_VERSION_NUMBER < 0x10100000L
	if (!X509_NAME_add_entry_by_NID(n, nid, MBSTRING_UTF8,
					(unsigned char *) name,
					os_strlen(name), -1, 0))
		return -1;
#else
	if (!X509_NAME_add_entry_by_NID(n, nid, MBSTRING_UTF8,
					(const unsigned char *) name,
					os_strlen(name), -1, 0))
		return -1;
#endif

	return 0;
}


int crypto_csr_set_attribute(struct crypto_csr *csr, enum crypto_csr_attr attr,
			     int attr_type, const u8 *value, size_t len)
{
	int nid;

	switch (attr) {
	case CSR_ATTR_CHALLENGE_PASSWORD:
		nid = NID_pkcs9_challengePassword;
		break;
	default:
		return -1;
	}

	if (!X509_REQ_add1_attr_by_NID((X509_REQ *) csr, nid, attr_type, value,
				       len))
		return -1;

	return 0;
}


const u8 * crypto_csr_get_attribute(struct crypto_csr *csr,
				    enum crypto_csr_attr attr,
				    size_t *len, int *type)
{
	X509_ATTRIBUTE *attrib;
	ASN1_TYPE *attrib_type;
	ASN1_STRING *data;
	int loc;
	int nid;

	switch (attr) {
	case CSR_ATTR_CHALLENGE_PASSWORD:
		nid = NID_pkcs9_challengePassword;
		break;
	default:
		return NULL;
	}

	loc = X509_REQ_get_attr_by_NID((X509_REQ *) csr, nid, -1);
	if (loc < 0)
		return NULL;

	attrib = X509_REQ_get_attr((X509_REQ *) csr, loc);
	if (!attrib)
		return NULL;

	attrib_type = X509_ATTRIBUTE_get0_type(attrib, 0);
	if (!attrib_type)
		return NULL;
	*type = ASN1_TYPE_get(attrib_type);
	data = X509_ATTRIBUTE_get0_data(attrib, 0, *type, NULL);
	if (!data)
		return NULL;
	*len = ASN1_STRING_length(data);
	return ASN1_STRING_get0_data(data);
}


struct wpabuf * crypto_csr_sign(struct crypto_csr *csr,
				struct crypto_ec_key *key,
				enum crypto_hash_alg algo)
{
	const EVP_MD *sign_md;
	struct wpabuf *buf;
	unsigned char *der = NULL;
	int der_len;

	switch (algo) {
	case CRYPTO_HASH_ALG_SHA256:
		sign_md = EVP_sha256();
		break;
	case CRYPTO_HASH_ALG_SHA384:
		sign_md = EVP_sha384();
		break;
	case CRYPTO_HASH_ALG_SHA512:
		sign_md = EVP_sha512();
		break;
	default:
		return NULL;
	}

	if (!X509_REQ_sign((X509_REQ *) csr, (EVP_PKEY *) key, sign_md))
		return NULL;

	der_len = i2d_X509_REQ((X509_REQ *) csr, &der);
	if (der_len < 0)
		return NULL;

	buf = wpabuf_alloc_copy(der, der_len);
	OPENSSL_free(der);

	return buf;
}

#endif /* CONFIG_ECC */


static EVP_PKEY * crypto_rsa_key_read_public(FILE *f)
{
	EVP_PKEY *pkey;
	X509 *x509;
	const ASN1_TIME *not_before, *not_after;
	int res_before, res_after;

	pkey = PEM_read_PUBKEY(f, NULL, NULL, NULL);
	if (pkey)
		return pkey;

	rewind(f);
	x509 = PEM_read_X509(f, NULL, NULL, NULL);
	if (!x509)
		return NULL;

	not_before = X509_get0_notBefore(x509);
	not_after = X509_get0_notAfter(x509);
	if (!not_before || !not_after)
		goto fail;
	res_before = X509_cmp_current_time(not_before);
	res_after = X509_cmp_current_time(not_after);
	if (!res_before || !res_after)
		goto fail;
	if (res_before > 0 || res_after < 0) {
		wpa_printf(MSG_INFO,
			   "OpenSSL: Certificate for RSA public key is not valid at this time (%d %d)",
			   res_before, res_after);
		goto fail;
	}

	pkey = X509_get_pubkey(x509);
	X509_free(x509);

	if (!pkey)
		return NULL;
	if (EVP_PKEY_base_id(pkey) != EVP_PKEY_RSA) {
		wpa_printf(MSG_INFO, "OpenSSL: No RSA public key found");
		EVP_PKEY_free(pkey);
		return NULL;
	}

	return pkey;
fail:
	X509_free(x509);
	return NULL;
}


struct crypto_rsa_key * crypto_rsa_key_read(const char *file, bool private_key)
{
	FILE *f;
	EVP_PKEY *pkey;

	f = fopen(file, "r");
	if (!f)
		return NULL;
	if (private_key)
		pkey = PEM_read_PrivateKey(f, NULL, NULL, NULL);
	else
		pkey = crypto_rsa_key_read_public(f);
	fclose(f);
	return (struct crypto_rsa_key *) pkey;
}


#ifndef OPENSSL_NO_SHA256

struct wpabuf * crypto_rsa_oaep_sha256_encrypt(struct crypto_rsa_key *key,
					       const struct wpabuf *in)
{
#if !defined(LIBRESSL_VERSION_NUMBER) || LIBRESSL_VERSION_NUMBER >= 0x30400000L
	EVP_PKEY *pkey = (EVP_PKEY *) key;
	EVP_PKEY_CTX *pkctx;
	struct wpabuf *res = NULL;
	size_t outlen;

	pkctx = EVP_PKEY_CTX_new(pkey, NULL);
	if (!pkctx)
		goto fail;

	if (EVP_PKEY_encrypt_init(pkctx) != 1 ||
	    EVP_PKEY_CTX_set_rsa_padding(pkctx, RSA_PKCS1_OAEP_PADDING) <= 0 ||
	    EVP_PKEY_CTX_set_rsa_oaep_md(pkctx, EVP_sha256()) <= 0 ||
	    EVP_PKEY_encrypt(pkctx, NULL, &outlen, wpabuf_head(in),
			     wpabuf_len(in)) != 1 ||
	    !(res = wpabuf_alloc(outlen)) ||
	    EVP_PKEY_encrypt(pkctx, wpabuf_put(res, 0), &outlen,
			     wpabuf_head(in), wpabuf_len(in)) != 1) {
		wpabuf_free(res);
		res = NULL;
		goto fail;
	}
	wpabuf_put(res, outlen);

fail:
	EVP_PKEY_CTX_free(pkctx);
	return res;
#else
	wpa_printf(MSG_ERROR, "%s() not supported", __func__);
	return NULL;
#endif
}


struct wpabuf * crypto_rsa_oaep_sha256_decrypt(struct crypto_rsa_key *key,
					       const struct wpabuf *in)
{
#if !defined(LIBRESSL_VERSION_NUMBER) || LIBRESSL_VERSION_NUMBER >= 0x30400000L
	EVP_PKEY *pkey = (EVP_PKEY *) key;
	EVP_PKEY_CTX *pkctx;
	struct wpabuf *res = NULL;
	size_t outlen;

	pkctx = EVP_PKEY_CTX_new(pkey, NULL);
	if (!pkctx)
		goto fail;

	if (EVP_PKEY_decrypt_init(pkctx) != 1 ||
	    EVP_PKEY_CTX_set_rsa_padding(pkctx, RSA_PKCS1_OAEP_PADDING) <= 0 ||
	    EVP_PKEY_CTX_set_rsa_oaep_md(pkctx, EVP_sha256()) <= 0 ||
	    EVP_PKEY_decrypt(pkctx, NULL, &outlen, wpabuf_head(in),
			     wpabuf_len(in)) != 1 ||
	    !(res = wpabuf_alloc(outlen)) ||
	    EVP_PKEY_decrypt(pkctx, wpabuf_put(res, 0), &outlen,
			     wpabuf_head(in), wpabuf_len(in)) != 1) {
		wpabuf_free(res);
		res = NULL;
		goto fail;
	}
	wpabuf_put(res, outlen);

fail:
	EVP_PKEY_CTX_free(pkctx);
	return res;
#else
	wpa_printf(MSG_ERROR, "%s() not supported", __func__);
	return NULL;
#endif
}

#endif /* OPENSSL_NO_SHA256 */


void crypto_rsa_key_free(struct crypto_rsa_key *key)
{
	EVP_PKEY_free((EVP_PKEY *) key);
}


#ifdef CONFIG_DPP3

#define HPKE_MAX_SHARED_SECRET_LEN 66
#define HPKE_MAX_HASH_LEN 64
#define HPKE_MAX_KEY_LEN 32
#define HPKE_MAX_NONCE_LEN 12
#define HPKE_MAX_PUB_LEN (1 + 2 * 66)

struct hpke_context {
	/* KEM */
	enum hpke_kem_id kem_id;
	int kem_nid;
	int iana_group;
	size_t n_pk;
	size_t n_secret;
	const EVP_MD *kem_h;
	size_t kem_n_h;

	/* KDF */
	enum hpke_kdf_id kdf_id;
	const EVP_MD *kdf_h;
	size_t n_h;

	/* AEAD */
	enum hpke_aead_id aead_id;
	const EVP_CIPHER *cipher;
	size_t n_k;
	size_t n_n;
	size_t n_t;
	u8 key[HPKE_MAX_KEY_LEN];
	u8 base_nonce[HPKE_MAX_NONCE_LEN];
};


static void hpke_free_context(struct hpke_context *ctx)
{
	bin_clear_free(ctx, sizeof(*ctx));
}


static struct hpke_context * hpke_get_context(enum hpke_kem_id kem_id,
					      enum hpke_kdf_id kdf_id,
					      enum hpke_aead_id aead_id,
					      struct crypto_ec_key *key)
{
	struct hpke_context *ctx;
	int group;

	ctx = os_zalloc(sizeof(*ctx));
	if (!ctx)
		return NULL;

	ctx->kem_id = kem_id;
	switch (kem_id) {
	case HPKE_DHKEM_P256_HKDF_SHA256:
		ctx->kem_nid = NID_X9_62_prime256v1;
		ctx->iana_group = 19;
		ctx->n_pk = 65;
		ctx->n_secret = 32;
		ctx->kem_h = EVP_sha256();
		ctx->kem_n_h = 32;
		break;
	case HPKE_DHKEM_P384_HKDF_SHA384:
		ctx->kem_nid = NID_secp384r1;
		ctx->iana_group = 20;
		ctx->n_pk = 97;
		ctx->n_secret = 48;
		ctx->kem_h = EVP_sha384();
		ctx->kem_n_h = 48;
		break;
	case HPKE_DHKEM_P521_HKDF_SHA512:
		ctx->kem_nid = NID_secp521r1;
		ctx->iana_group = 21;
		ctx->n_pk = 133;
		ctx->n_secret = 64;
		ctx->kem_h = EVP_sha512();
		ctx->kem_n_h = 64;
		break;
	default:
		goto fail;
	}

	ctx->kdf_id = kdf_id;
	switch (kdf_id) {
	case HPKE_KDF_HKDF_SHA256:
		ctx->kdf_h = EVP_sha256();
		ctx->n_h = 32;
		break;
	case HPKE_KDF_HKDF_SHA384:
		ctx->kdf_h = EVP_sha384();
		ctx->n_h = 48;
		break;
	case HPKE_KDF_HKDF_SHA512:
		ctx->kdf_h = EVP_sha512();
		ctx->n_h = 64;
		break;
	default:
		goto fail;
	}

	ctx->aead_id = aead_id;
	switch (aead_id) {
	case HPKE_AEAD_AES_128_GCM:
		ctx->cipher = EVP_aes_128_gcm();
		ctx->n_k = 16;
		ctx->n_n = 12;
		ctx->n_t = 16;
		break;
	case HPKE_AEAD_AES_256_GCM:
		ctx->cipher = EVP_aes_256_gcm();
		ctx->n_k = 32;
		ctx->n_n = 12;
		ctx->n_t = 16;
		break;
	default:
		goto fail;
	}

	/* Convert BP-256/384/512 to P-256/384/521 for DPP */
	group = crypto_ec_key_group(key);
	if (group == 28 && ctx->iana_group == 19) {
		ctx->iana_group = 28;
	} else if (group == 29 && ctx->iana_group == 20) {
		ctx->iana_group = 29;
	} else if (group == 30 && ctx->iana_group == 21) {
		ctx->iana_group = 30;
		ctx->n_pk = 129;
	}
	if (group != ctx->iana_group) {
		wpa_printf(MSG_INFO, "OpenSSL:%s:group mismatch (%d != %d)",
			   __func__, group, ctx->iana_group);
		goto fail;
	}

	return ctx;
fail:
	hpke_free_context(ctx);
	return NULL;
}


static size_t hpke_suite_id(struct hpke_context *ctx, bool kem, u8 *suite_id)
{
	size_t suite_id_len;

	if (kem) {
		os_memcpy(suite_id, "KEM", 3);
		WPA_PUT_BE16(&suite_id[3], ctx->kem_id);
		suite_id_len = 5;
	} else {
		os_memcpy(suite_id, "HPKE", 4);
		WPA_PUT_BE16(&suite_id[4], ctx->kem_id);
		WPA_PUT_BE16(&suite_id[6], ctx->kdf_id);
		WPA_PUT_BE16(&suite_id[8], ctx->aead_id);
		suite_id_len = 10;
	}
	return suite_id_len;
}


static int hpke_labeled_extract(struct hpke_context *ctx, bool kem,
				const u8 *salt, size_t salt_len,
				const char *label,
				const u8 *ikm, size_t ikm_len, u8 *prk)
{
	u8 zero[HPKE_MAX_HASH_LEN];
	u8 suite_id[10];
	size_t suite_id_len;
	unsigned int mdlen = kem ? ctx->kem_n_h : ctx->n_h;
#if OPENSSL_VERSION_NUMBER >= 0x30000000L
	EVP_MAC *hmac;
	OSSL_PARAM params[2];
	EVP_MAC_CTX *hctx;
	size_t mlen;
	int res;
#else /* OpenSSL version >= 3.0 */
	HMAC_CTX *hctx;
	int res;
#endif /* OpenSSL version >= 3.0 */

	if (!salt || !salt_len) {
		salt_len = mdlen;
		os_memset(zero, 0, salt_len);
		salt = zero;
	}

	suite_id_len = hpke_suite_id(ctx, kem, suite_id);

	/* labeled_ikm = concat("HPKE-v1", suite_id, label, ikm)
	 * return Extract(salt, labeled_ikm) */

#if OPENSSL_VERSION_NUMBER >= 0x30000000L
	hmac = EVP_MAC_fetch(NULL, "HMAC", NULL);
	if (!hmac)
		return -1;

	params[0] = OSSL_PARAM_construct_utf8_string(
		"digest",
		(char *) EVP_MD_get0_name(kem ? ctx->kem_h : ctx->kdf_h), 0);
	params[1] = OSSL_PARAM_construct_end();

	hctx = EVP_MAC_CTX_new(hmac);
	EVP_MAC_free(hmac);
	if (!hctx)
		return -1;

	if (EVP_MAC_init(hctx, salt, salt_len, params) != 1)
		goto fail;

	if (EVP_MAC_update(hctx, (const unsigned char *) "HPKE-v1", 7) != 1 ||
	    EVP_MAC_update(hctx, suite_id, suite_id_len) != 1 ||
	    EVP_MAC_update(hctx, (const unsigned char *) label,
			   os_strlen(label)) != 1 ||
	    EVP_MAC_update(hctx, ikm, ikm_len) != 1)
		goto fail;

	res = EVP_MAC_final(hctx, prk, &mlen, mdlen);
	EVP_MAC_CTX_free(hctx);

	return res == 1 ? 0 : -1;
fail:
	EVP_MAC_CTX_free(hctx);
	return -1;
#else /* OpenSSL version >= 3.0 */
	hctx = HMAC_CTX_new();
	if (!hctx)
		return -1;
	res = HMAC_Init_ex(hctx, salt, salt_len, kem ? ctx->kem_h : ctx->kdf_h,
			   NULL);
	if (res != 1)
		goto done;

	HMAC_Update(hctx, (const unsigned char *) "HPKE-v1", 7);
	HMAC_Update(hctx, suite_id, suite_id_len);
	HMAC_Update(hctx, (const unsigned char *) label, os_strlen(label));
	HMAC_Update(hctx, ikm, ikm_len);

	res = HMAC_Final(hctx, prk, &mdlen);
done:
	HMAC_CTX_free(hctx);

	return res == 1 ? 0 : -1;
#endif /* OpenSSL version >= 3.0 */
}


static int
hpke_labeled_expand(struct hpke_context *ctx, bool kem, const u8 *prk,
		    const char *label, const u8 *info, size_t info_len,
		    u8 *out, size_t out_len)
{
	u8 suite_id[10];
	size_t suite_id_len;
	u8 hash[HPKE_MAX_HASH_LEN];
	u8 iter = 0;
	size_t label_len = os_strlen(label);
	u8 *pos;
	size_t left = out_len, clen;
	int res = -1;
	u8 *labeled_info;
	size_t labeled_info_len;
#if OPENSSL_VERSION_NUMBER >= 0x30000000L
	EVP_MAC *hmac;
	OSSL_PARAM params[2];
	EVP_MAC_CTX *hctx = NULL;
	size_t mdlen;
#else /* OpenSSL version >= 3.0 */
	HMAC_CTX *hctx;
	unsigned int mdlen;
#endif /* OpenSSL version >= 3.0 */

	/* labeled_info = concat(I2OSP(L, 2), "HPKE-v1", suite_id,
	 *                       label, info)
	 * return Expand(prk, labeled_info, L) */
	suite_id_len = hpke_suite_id(ctx, kem, suite_id);
	labeled_info_len = 2 + 7 + suite_id_len + label_len + info_len;
	labeled_info = os_malloc(labeled_info_len);
	if (!labeled_info)
		return -1;
	pos = labeled_info;
	WPA_PUT_BE16(pos, out_len);
	pos += 2;
	os_memcpy(pos, "HPKE-v1", 7);
	pos += 7;
	os_memcpy(pos, suite_id, suite_id_len);
	pos += suite_id_len;
	os_memcpy(pos, label, label_len);
	pos += label_len;
	if (info && info_len)
		os_memcpy(pos, info, info_len);

	pos = out;
#if OPENSSL_VERSION_NUMBER >= 0x30000000L
	hmac = EVP_MAC_fetch(NULL, "HMAC", NULL);
	if (!hmac)
		goto fail;

	params[0] = OSSL_PARAM_construct_utf8_string(
		"digest",
		(char *) EVP_MD_get0_name(kem ? ctx->kem_h : ctx->kdf_h), 0);
	params[1] = OSSL_PARAM_construct_end();
#else /* OpenSSL version >= 3.0 */
	hctx = HMAC_CTX_new();
	if (!hctx)
		goto fail;
#endif /* OpenSSL version >= 3.0 */

	while (left > 0) {
		mdlen = kem ? ctx->kem_n_h : ctx->n_h;
#if OPENSSL_VERSION_NUMBER >= 0x30000000L
		EVP_MAC_CTX_free(hctx);
		hctx = EVP_MAC_CTX_new(hmac);
		if (!hctx)
			goto fail;

		if (EVP_MAC_init(hctx, prk, mdlen, params) != 1)
			goto fail;

		if (iter > 0 && EVP_MAC_update(hctx, hash, mdlen) != 1)
			goto fail;
		if (iter == 255)
			goto fail;
		iter++;

		if (EVP_MAC_update(hctx, labeled_info, labeled_info_len) != 1 ||
		    EVP_MAC_update(hctx, &iter, sizeof(iter)) != 1)
			goto fail;

		if (EVP_MAC_final(hctx, hash, &mdlen, mdlen) != 1)
			goto fail;
#else /* OpenSSL version >= 3.0 */
		if (HMAC_Init_ex(hctx, prk, mdlen,
				 kem ? ctx->kem_h : ctx->kdf_h,
				 NULL) != 1)
			goto fail;

		if (iter > 0)
			HMAC_Update(hctx, hash, mdlen);
		if (iter == 255)
			goto fail;
		iter++;
		HMAC_Update(hctx, labeled_info, labeled_info_len);
		HMAC_Update(hctx, &iter, sizeof(iter));

		if (HMAC_Final(hctx, hash, &mdlen) != 1)
			goto fail;
		HMAC_CTX_reset(hctx);
#endif /* OpenSSL version >= 3.0 */

		clen = left > mdlen ? mdlen : left;
		os_memcpy(pos, hash, clen);
		pos += clen;
		left -= clen;
	}
	res = 0;
fail:
#if OPENSSL_VERSION_NUMBER >= 0x30000000L
	EVP_MAC_free(hmac);
	EVP_MAC_CTX_free(hctx);
#else /* OpenSSL version >= 3.0 */
	HMAC_CTX_free(hctx);
#endif /* OpenSSL version >= 3.0 */
	os_free(labeled_info);

	return res;
}


static int hpke_extract_and_expand(struct hpke_context *ctx,
				   const u8 *dhss, size_t dhss_len,
				   const u8 *enc, size_t enc_len,
				   const u8 *pk_rm, size_t pk_rm_len,
				   u8 *shared_secret)
{
	u8 kem_context[2 * HPKE_MAX_PUB_LEN];
	u8 eae_prk[HPKE_MAX_HASH_LEN];

	/* eae_prk = LabeledExtract("", "eae_prk", dh) */
	if (hpke_labeled_extract(ctx, true, NULL, 0, "eae_prk", dhss, dhss_len,
				 eae_prk) < 0)
		return -1;

	if (enc_len > HPKE_MAX_PUB_LEN || pk_rm_len > HPKE_MAX_PUB_LEN)
		return -1;
	/* kem_context = concat(enc, pkRm) */
	os_memcpy(kem_context, enc, enc_len);
	os_memcpy(&kem_context[enc_len], pk_rm, pk_rm_len);

	/* shared_secret = LabeledExpand(eae_prk, "shared_secret",
	 *                               kem_context, Nsecret) */
	if (hpke_labeled_expand(ctx, true, eae_prk, "shared_secret",
				kem_context, enc_len + pk_rm_len,
				shared_secret, ctx->n_secret) < 0)
		return -1;

	forced_memzero(eae_prk, sizeof(eae_prk));
	return 0;
}


static int hpke_key_schedule(struct hpke_context *ctx, const u8 *shared_secret,
			     const u8 *info, size_t info_len)
{
	u8 key_schedule_context[1 + 2 * HPKE_MAX_HASH_LEN];
	u8 secret[HPKE_MAX_HASH_LEN];
	int res = -1;

	/* key_schedule_context = concat(mode, psk_id_hash, info_hash) */
	key_schedule_context[0] = HPKE_MODE_BASE;

	/* psk_id_hash = LabeledExtract("", "psk_id_hash", psk_id) */
	if (hpke_labeled_extract(ctx, false, NULL, 0, "psk_id_hash",
				 NULL, 0, &key_schedule_context[1]) < 0)
		goto fail;

	/* info_hash = LabeledExtract("", "info_hash", info) */
	if (hpke_labeled_extract(ctx, false, NULL, 0, "info_hash",
				 info, info_len,
				 &key_schedule_context[1 + ctx->n_h]) < 0)
		goto fail;

	/* secret = LabeledExtract(shared_secret, "secret", psk) */
	if (hpke_labeled_extract(ctx, false, shared_secret, ctx->n_secret,
				 "secret", NULL, 0, secret) < 0)
		goto fail;

	/* key = LabeledExpand(secret, "key", key_schedule_context, Nk) */
	if (hpke_labeled_expand(ctx, false, secret, "key",
				key_schedule_context, 1 + 2 * ctx->n_h,
				ctx->key, ctx->n_k) < 0)
		goto fail;

	/* base_nonce = LabeledExpand(secret, "base_nonce",
	 *                            key_schedule_context, Nn) */
	if (hpke_labeled_expand(ctx, false, secret, "base_nonce",
				key_schedule_context, 1 + 2 * ctx->n_h,
				ctx->base_nonce, ctx->n_n) < 0)
		goto fail;
	res = 0;
fail:
	forced_memzero(key_schedule_context, sizeof(key_schedule_context));
	forced_memzero(secret, sizeof(secret));
	return res;
}


static int hpke_encap(struct hpke_context *ctx, struct crypto_ec_key *pk_r,
		      u8 *shared_secret, u8 *enc)
{
	EVP_PKEY_CTX *pctx = NULL;
	struct crypto_ec_key *sk_e;
	int res = -1;
	u8 *dhss = NULL;
	size_t dhss_len = 0;
	struct wpabuf *enc_buf = NULL, *pk_rm = NULL;

	/* skE, pkE = GenerateKeyPair() */
	sk_e = crypto_ec_key_gen(ctx->iana_group);
	if (!sk_e) {
		wpa_printf(MSG_INFO, "OpenSSL:%s:Could not generate key pair",
			   __func__);
		goto fail;
	}

	/* dh = DH(skE, pkR) */
	dhss_len = sizeof(dhss);
	pctx = EVP_PKEY_CTX_new((EVP_PKEY *) sk_e, NULL);
	if (!pctx ||
	    EVP_PKEY_derive_init(pctx) != 1 ||
	    EVP_PKEY_derive_set_peer(pctx, (EVP_PKEY *) pk_r) != 1 ||
	    EVP_PKEY_derive(pctx, NULL, &dhss_len) != 1 ||
	    !(dhss = os_malloc(dhss_len)) ||
	    EVP_PKEY_derive(pctx, dhss, &dhss_len) != 1 ||
	    dhss_len > HPKE_MAX_SHARED_SECRET_LEN) {
		wpa_printf(MSG_INFO,
			   "OpenSSL: hpke_encap: EVP_PKEY_derive failed (dhss_len=%zu): %s",
			   dhss_len, ERR_error_string(ERR_get_error(), NULL));
		goto fail;
	}

	/* enc = SerializePublicKey(pkE) */
	enc_buf = crypto_ec_key_get_pubkey_point(sk_e, 1);
	if (!enc_buf)
		goto fail;
	os_memcpy(enc, wpabuf_head(enc_buf), wpabuf_len(enc_buf));

	/* pkRm = SerializePublicKey(pkR) */
	pk_rm = crypto_ec_key_get_pubkey_point(pk_r, 1);
	if (!pk_rm)
		goto fail;

	/* kem_context = concat(enc, pkRm) */
	/* shared_secret = ExtractAndExpand(dh, kem_context) */
	/* return shared_secret, enc */
	res = hpke_extract_and_expand(ctx, dhss, dhss_len, enc, ctx->n_pk,
				      wpabuf_head(pk_rm),
				      wpabuf_len(pk_rm), shared_secret);
fail:
	bin_clear_free(dhss, dhss_len);
	crypto_ec_key_deinit(sk_e);
	EVP_PKEY_CTX_free(pctx);
	wpabuf_free(enc_buf);
	wpabuf_free(pk_rm);
	return res;
}


static struct wpabuf *
hpke_aead_seal(struct hpke_context *ctx, const u8 *aad, size_t aad_len,
	       const u8 *pt, size_t pt_len)
{
	EVP_CIPHER_CTX *cctx;
	int len = 0;
	struct wpabuf *ct = NULL;

	/* No need to xor in sequence number since we support only the
	 * single-shot API, i.e., base_nonce can be used as-is. */

	cctx = EVP_CIPHER_CTX_new();
	if (!cctx ||
	    EVP_EncryptInit_ex(cctx, ctx->cipher, NULL, ctx->key,
			       ctx->base_nonce) != 1) {
		wpa_printf(MSG_INFO, "OpenSSL:%s:EVP_DecryptInit_ex failed",
			   __func__);
		goto fail;
	}
	if (aad && aad_len &&
	    EVP_EncryptUpdate(cctx, NULL, &len, aad, aad_len) != 1) {
		wpa_printf(MSG_INFO, "OpenSSL:%s:EVP_EncryptUpdate(AAD) failed",
			   __func__);
		goto fail;
	}
	ct = wpabuf_alloc(pt_len + AES_BLOCK_SIZE + ctx->n_t);
	if (!ct)
		goto fail;
	if (EVP_EncryptUpdate(cctx, wpabuf_put(ct, 0), &len, pt, pt_len) != 1) {
		wpa_printf(MSG_INFO, "OpenSSL:%s:EVP_EncryptUpdate failed",
			   __func__);
		goto fail;
	}
	wpabuf_put(ct, len);

	if (EVP_EncryptFinal(cctx, wpabuf_put(ct, 0), &len) != 1) {
		wpa_printf(MSG_INFO, "OpenSSL:%s:EVP_DecryptFinal failed",
			   __func__);
		wpabuf_free(ct);
		ct = NULL;
		goto fail;
	}

	if (EVP_CIPHER_CTX_ctrl(cctx, EVP_CTRL_AEAD_GET_TAG, ctx->n_t,
				wpabuf_put(ct, ctx->n_t)) != 1) {
		wpa_printf(MSG_INFO, "OpenSSL:%s:Could not get tag",
			   __func__);
		wpabuf_free(ct);
		ct = NULL;
		goto fail;
	}
fail:
	EVP_CIPHER_CTX_free(cctx);
	return ct;
}


static struct wpabuf * hpke_base_seal_int(enum hpke_kem_id kem_id,
					  enum hpke_kdf_id kdf_id,
					  enum hpke_aead_id aead_id,
					  struct crypto_ec_key *peer_pub,
					  const u8 *info, size_t info_len,
					  const u8 *aad, size_t aad_len,
					  const u8 *pt, size_t pt_len)
{
	struct hpke_context *ctx;
	u8 shared_secret[HPKE_MAX_SHARED_SECRET_LEN];
	u8 enc[1 + 2 * HPKE_MAX_PUB_LEN];
	struct wpabuf *ct = NULL, *enc_ct = NULL;

	ctx = hpke_get_context(kem_id, kdf_id, aead_id, peer_pub);
	if (!ctx)
		return NULL;

	/* shared_secret, enc = Encap(pkR) */
	if (hpke_encap(ctx, peer_pub, shared_secret, enc) < 0)
		goto fail;

	/* KeyScheduleS(mode_base, shared_secret, info,
	 *              default_psk, default_psk_id) */
	if (hpke_key_schedule(ctx, shared_secret, info, info_len) < 0)
		goto fail;

	/* ct = ctx.Seal(aad, pt) */
	ct = hpke_aead_seal(ctx, aad, aad_len, pt, pt_len);
	if (!ct)
		goto fail;

	/* return enc, ct */
	enc_ct = wpabuf_alloc(ctx->n_pk + wpabuf_len(ct));
	if (!enc_ct)
		goto fail;
	wpabuf_put_data(enc_ct, enc, ctx->n_pk);
	wpabuf_put_buf(enc_ct, ct);

fail:
	forced_memzero(shared_secret, sizeof(shared_secret));
	hpke_free_context(ctx);
	wpabuf_free(ct);
	return enc_ct;
}


static int hpke_decap(struct hpke_context *ctx, const u8 *enc,
		      size_t enc_ct_len, struct crypto_ec_key *sk_r,
		      u8 *shared_secret)
{
	EVP_PKEY_CTX *pctx = NULL;
	struct wpabuf *pk_rm = NULL;
	size_t len;
	int res = -1;
	struct crypto_ec_key *pk_e = NULL;
	u8 *dhss = NULL;
	size_t dhss_len = 0;

	/* pkE = DeserializePublicKey(enc) */
	if (enc_ct_len < ctx->n_pk)
		return -1; /* not enough room for enc */
	if (enc[0] != 0x04)
		return -1; /* not in uncompressed form */
	len = (ctx->n_pk - 1) / 2;
	pk_e = crypto_ec_key_set_pub(ctx->iana_group, &enc[1],
				     &enc[1 + len], len);
	if (!pk_e)
		return -1; /* invalid public key point */
	/* dh = DH(skR, pkE) */
	pctx = EVP_PKEY_CTX_new((EVP_PKEY *) sk_r, NULL);
	if (!pctx ||
	    EVP_PKEY_derive_init(pctx) != 1 ||
	    EVP_PKEY_derive_set_peer(pctx, (EVP_PKEY *) pk_e) != 1 ||
	    EVP_PKEY_derive(pctx, NULL, &dhss_len) != 1 ||
	    !(dhss = os_malloc(dhss_len)) ||
	    EVP_PKEY_derive(pctx, dhss, &dhss_len) != 1 ||
	    dhss_len > HPKE_MAX_SHARED_SECRET_LEN) {
		wpa_printf(MSG_INFO,
			   "OpenSSL: hpke_decap: EVP_PKEY_derive failed (dhss_len=%zu): %s",
			   dhss_len, ERR_error_string(ERR_get_error(), NULL));
		goto fail;
	}

	/* pkRm = SerializePublicKey(pk(skR)) */
	pk_rm = crypto_ec_key_get_pubkey_point(sk_r, 1);
	if (!pk_rm)
		goto fail;

	/* kem_context = concat(enc, pkRm) */
	/* shared_secret = ExtractAndExpand(dh, kem_context) */
	res = hpke_extract_and_expand(ctx, dhss, dhss_len, enc, ctx->n_pk,
				      wpabuf_head(pk_rm),
				      wpabuf_len(pk_rm), shared_secret);
fail:
	bin_clear_free(dhss, dhss_len);
	crypto_ec_key_deinit(pk_e);
	EVP_PKEY_CTX_free(pctx);
	wpabuf_free(pk_rm);
	return res;
}


static struct wpabuf *
hpke_aead_open(struct hpke_context *ctx, const u8 *aad, size_t aad_len,
	       const u8 *ct, size_t ct_len)
{
	EVP_CIPHER_CTX *cctx;
	int len = 0;
	const u8 *tag;
	struct wpabuf *pt = NULL;

	if (ct_len < ctx->n_t)
		return NULL;
	tag = ct + ct_len - ctx->n_t;
	ct_len -= ctx->n_t;

	/* No need to xor in sequence number since we support only the
	 * single-shot API, i.e., base_nonce can be used as-is. */

	cctx = EVP_CIPHER_CTX_new();
	if (!cctx ||
	    EVP_DecryptInit_ex(cctx, ctx->cipher, NULL, ctx->key,
			       ctx->base_nonce) != 1) {
		wpa_printf(MSG_INFO, "OpenSSL:%s:EVP_DecryptInit_ex failed",
			   __func__);
		goto fail;
	}
	if (aad && aad_len &&
	    EVP_DecryptUpdate(cctx, NULL, &len, aad, aad_len) != 1) {
		wpa_printf(MSG_INFO, "OpenSSL:%s:EVP_DecryptUpdate(AAD) failed",
			   __func__);
		goto fail;
	}
	pt = wpabuf_alloc(ct_len + AES_BLOCK_SIZE);
	if (!pt)
		goto fail;
	if (EVP_DecryptUpdate(cctx, wpabuf_put(pt, 0), &len, ct, ct_len) != 1) {
		wpa_printf(MSG_INFO, "OpenSSL:%s:EVP_DecryptUpdate failed",
			   __func__);
		goto fail;
	}
	wpabuf_put(pt, len);

	if (EVP_CIPHER_CTX_ctrl(cctx, EVP_CTRL_AEAD_SET_TAG, ctx->n_t,
				(void *) tag) != 1) {
		wpa_printf(MSG_INFO, "OpenSSL:%s:Could not set tag",
			   __func__);
		wpabuf_free(pt);
		pt = NULL;
		goto fail;
	}

	if (EVP_DecryptFinal(cctx, wpabuf_put(pt, 0), &len) != 1) {
		wpa_printf(MSG_INFO, "OpenSSL:%s:EVP_DecryptFinal failed",
			   __func__);
		wpabuf_free(pt);
		pt = NULL;
	}
fail:
	EVP_CIPHER_CTX_free(cctx);
	return pt;
}


static struct wpabuf * hpke_base_open_int(enum hpke_kem_id kem_id,
					  enum hpke_kdf_id kdf_id,
					  enum hpke_aead_id aead_id,
					  struct crypto_ec_key *own_priv,
					  const u8 *info, size_t info_len,
					  const u8 *aad, size_t aad_len,
					  const u8 *enc_ct, size_t enc_ct_len)
{
	struct hpke_context *ctx;
	u8 shared_secret[HPKE_MAX_SHARED_SECRET_LEN];
	struct wpabuf *pt = NULL;

	ctx = hpke_get_context(kem_id, kdf_id, aead_id, own_priv);
	if (!ctx)
		return NULL;

	/* shared_secret = Decap(enc, skR) */
	if (hpke_decap(ctx, enc_ct, enc_ct_len, own_priv, shared_secret) < 0)
		goto fail;

	/* KeyScheduleR(mode_base, shared_secret, info,
	 *              default_psk, default_psk_id) */
	if (hpke_key_schedule(ctx, shared_secret, info, info_len) < 0)
		goto fail;

	/* return ctx.Open(aad, ct) */
	pt = hpke_aead_open(ctx, aad, aad_len,
			    &enc_ct[ctx->n_pk], enc_ct_len - ctx->n_pk);

fail:
	forced_memzero(shared_secret, sizeof(shared_secret));
	hpke_free_context(ctx);
	return pt;
}


#if OPENSSL_VERSION_NUMBER >= 0x30200000L

static bool hpke_set_suite(OSSL_HPKE_SUITE *suite,
			   enum hpke_kem_id kem_id,
			   enum hpke_kdf_id kdf_id,
			   enum hpke_aead_id aead_id)
{
	os_memset(suite, 0, sizeof(*suite));

	switch (kem_id) {
	case HPKE_DHKEM_P256_HKDF_SHA256:
		suite->kem_id = OSSL_HPKE_KEM_ID_P256;
		break;
	case HPKE_DHKEM_P384_HKDF_SHA384:
		suite->kem_id = OSSL_HPKE_KEM_ID_P384;
		break;
	case HPKE_DHKEM_P521_HKDF_SHA512:
		suite->kem_id = OSSL_HPKE_KEM_ID_P521;
		break;
	default:
		return false;
	}

	switch (kdf_id) {
	case HPKE_KDF_HKDF_SHA256:
		suite->kdf_id = OSSL_HPKE_KDF_ID_HKDF_SHA256;
		break;
	case HPKE_KDF_HKDF_SHA384:
		suite->kdf_id = OSSL_HPKE_KDF_ID_HKDF_SHA384;
		break;
	case HPKE_KDF_HKDF_SHA512:
		suite->kdf_id = OSSL_HPKE_KDF_ID_HKDF_SHA512;
		break;
	default:
		return false;
	}

	switch (aead_id) {
	case HPKE_AEAD_AES_128_GCM:
		suite->aead_id = OSSL_HPKE_AEAD_ID_AES_GCM_128;
		break;
	case HPKE_AEAD_AES_256_GCM:
		suite->aead_id = OSSL_HPKE_AEAD_ID_AES_GCM_256;
		break;
	default:
		return false;
	}

	if (!OSSL_HPKE_suite_check(*suite)) {
		wpa_printf(MSG_INFO,
			   "OpenSSL: HPKE suite kem_id=%d kdf_id=%d aead_id=%d not supported",
			   kem_id, kdf_id, aead_id);
		return false;
	}

	return true;
}


struct wpabuf * hpke_base_seal(enum hpke_kem_id kem_id,
			       enum hpke_kdf_id kdf_id,
			       enum hpke_aead_id aead_id,
			       struct crypto_ec_key *peer_pub,
			       const u8 *info, size_t info_len,
			       const u8 *aad, size_t aad_len,
			       const u8 *pt, size_t pt_len)
{
	OSSL_HPKE_SUITE suite;
	OSSL_HPKE_CTX *ctx = NULL;
	struct wpabuf *res = NULL, *buf, *pub = NULL;
	size_t enc_len, ct_len;
	int group;

	group = crypto_ec_key_group(peer_pub);
	if (group == 28 || group == 29 || group == 30) {
		/* Use the internal routines for the special DPP use case with
		 * brainpool curves, */
		return hpke_base_seal_int(kem_id, kdf_id, aead_id, peer_pub,
					  info, info_len, aad, aad_len,
					  pt, pt_len);
	}


	if (!hpke_set_suite(&suite, kem_id, kdf_id, aead_id))
		return NULL;

	enc_len = OSSL_HPKE_get_public_encap_size(suite);
	ct_len = OSSL_HPKE_get_ciphertext_size(suite, pt_len);
	buf = wpabuf_alloc(enc_len + ct_len);
	if (!buf)
		goto out;

	pub = crypto_ec_key_get_pubkey_point(peer_pub, 1);
	if (!pub)
		goto out;

	ctx = OSSL_HPKE_CTX_new(OSSL_HPKE_MODE_BASE, suite,
				OSSL_HPKE_ROLE_SENDER, NULL, NULL);
	if (!ctx)
		goto out;

	if (OSSL_HPKE_encap(ctx, wpabuf_put(buf, 0), &enc_len,
			    wpabuf_head(pub), wpabuf_len(pub),
			    info, info_len) != 1) {
		wpa_printf(MSG_DEBUG, "OpenSSL: OSSL_HPKE_encap failed: %s",
			   ERR_error_string(ERR_get_error(), NULL));
		goto out;
	}
	wpabuf_put(buf, enc_len);

	if (OSSL_HPKE_seal(ctx, wpabuf_put(buf, 0), &ct_len, aad, aad_len,
			   pt, pt_len) != 1) {
		wpa_printf(MSG_DEBUG, "OpenSSL: OSSL_HPKE_seal failed: %s",
			   ERR_error_string(ERR_get_error(), NULL));
		goto out;
	}
	wpabuf_put(buf, ct_len);
	res = buf;
	buf = NULL;

out:
	OSSL_HPKE_CTX_free(ctx);
	wpabuf_free(buf);
	wpabuf_free(pub);
	return res;
}


struct wpabuf * hpke_base_open(enum hpke_kem_id kem_id,
			       enum hpke_kdf_id kdf_id,
			       enum hpke_aead_id aead_id,
			       struct crypto_ec_key *own_priv,
			       const u8 *info, size_t info_len,
			       const u8 *aad, size_t aad_len,
			       const u8 *enc_ct, size_t enc_ct_len)
{
	OSSL_HPKE_SUITE suite;
	OSSL_HPKE_CTX *ctx;
	struct wpabuf *buf = NULL, *res = NULL;
	size_t len, enc_len;
	int group;

	group = crypto_ec_key_group(own_priv);
	if (group == 28 || group == 29 || group == 30) {
		/* Use the internal routines for the special DPP use case with
		 * brainpool curves, */
		return hpke_base_open_int(kem_id, kdf_id, aead_id, own_priv,
					  info, info_len, aad, aad_len,
					  enc_ct, enc_ct_len);
	}

	if (!hpke_set_suite(&suite, kem_id, kdf_id, aead_id))
		return NULL;

	enc_len = OSSL_HPKE_get_public_encap_size(suite);
	if (enc_ct_len < enc_len) {
		wpa_printf(MSG_DEBUG, "OpenSSL: Too short HPKE enc_ct data");
		return NULL;
	}

	ctx = OSSL_HPKE_CTX_new(OSSL_HPKE_MODE_BASE, suite,
				OSSL_HPKE_ROLE_RECEIVER, NULL, NULL);
	if (!ctx)
		goto out;

	if (OSSL_HPKE_decap(ctx, enc_ct, enc_len, (EVP_PKEY *) own_priv,
			    info, info_len) != 1) {
		wpa_printf(MSG_DEBUG, "OpenSSL: OSSL_HPKE_decap failed: %s",
			   ERR_error_string(ERR_get_error(), NULL));
		goto out;
	}

	len = enc_ct_len;
	buf = wpabuf_alloc(len);
	if (!buf)
		goto out;

	if (OSSL_HPKE_open(ctx, wpabuf_put(buf, 0), &len, aad, aad_len,
			   enc_ct + enc_len, enc_ct_len - enc_len) != 1) {
		wpa_printf(MSG_DEBUG, "OpenSSL: OSSL_HPKE_open failed: %s",
			   ERR_error_string(ERR_get_error(), NULL));
		goto out;
	}

	wpabuf_put(buf, len);
	res = buf;
	buf = NULL;

out:
	OSSL_HPKE_CTX_free(ctx);
	wpabuf_free(buf);
	return res;
}

#else /* OpenSSL < 3.2 */

struct wpabuf * hpke_base_seal(enum hpke_kem_id kem_id,
			       enum hpke_kdf_id kdf_id,
			       enum hpke_aead_id aead_id,
			       struct crypto_ec_key *peer_pub,
			       const u8 *info, size_t info_len,
			       const u8 *aad, size_t aad_len,
			       const u8 *pt, size_t pt_len)
{
	return hpke_base_seal_int(kem_id, kdf_id, aead_id, peer_pub,
				  info, info_len, aad, aad_len, pt, pt_len);
}


struct wpabuf * hpke_base_open(enum hpke_kem_id kem_id,
			       enum hpke_kdf_id kdf_id,
			       enum hpke_aead_id aead_id,
			       struct crypto_ec_key *own_priv,
			       const u8 *info, size_t info_len,
			       const u8 *aad, size_t aad_len,
			       const u8 *enc_ct, size_t enc_ct_len)
{
	return hpke_base_open_int(kem_id, kdf_id, aead_id, own_priv,
				  info, info_len, aad, aad_len,
				  enc_ct, enc_ct_len);
}

#endif /* OpenSSL < 3.2 */

#endif /* CONFIG_DPP3 */


void crypto_unload(void)
{
	openssl_unload_legacy_provider();
}
