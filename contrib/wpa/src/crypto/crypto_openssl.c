/*
 * Wrapper functions for OpenSSL libcrypto
 * Copyright (c) 2004-2017, Jouni Malinen <j@w1.fi>
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
#ifdef CONFIG_OPENSSL_CMAC
#include <openssl/cmac.h>
#endif /* CONFIG_OPENSSL_CMAC */
#ifdef CONFIG_ECC
#include <openssl/ec.h>
#include <openssl/x509.h>
#endif /* CONFIG_ECC */

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

#if OPENSSL_VERSION_NUMBER < 0x10100000L || \
	(defined(LIBRESSL_VERSION_NUMBER) && \
	 LIBRESSL_VERSION_NUMBER < 0x20700000L)
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
#endif /* CONFIG_ECC */

#endif /* OpenSSL version < 1.1.0 */

static BIGNUM * get_group5_prime(void)
{
#if OPENSSL_VERSION_NUMBER >= 0x10100000L && \
	!(defined(LIBRESSL_VERSION_NUMBER) && \
	  LIBRESSL_VERSION_NUMBER < 0x20700000L)
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
	return openssl_digest_vector(EVP_md4(), num_elem, addr, len, mac);
}
#endif /* CONFIG_FIPS */


int des_encrypt(const u8 *clear, const u8 *key, u8 *cypher)
{
	u8 pkey[8], next, tmp;
	int i;
	DES_key_schedule ks;

	/* Add parity bits to the key */
	next = 0;
	for (i = 0; i < 7; i++) {
		tmp = key[i];
		pkey[i] = (tmp >> i) | next | 1;
		next = tmp << (7 - i);
	}
	pkey[i] = next | 1;

	DES_set_key((DES_cblock *) &pkey, &ks);
	DES_ecb_encrypt((DES_cblock *) clear, (DES_cblock *) cypher, &ks,
			DES_ENCRYPT);
	return 0;
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

	ctx = EVP_CIPHER_CTX_new();
	if (!ctx ||
	    !EVP_CIPHER_CTX_set_padding(ctx, 0) ||
	    !EVP_CipherInit_ex(ctx, EVP_rc4(), NULL, NULL, NULL, 1) ||
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


#ifndef CONFIG_FIPS
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
	}

	return NULL;
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
	if (EVP_EncryptInit_ex(ctx, type, NULL, key, NULL) != 1) {
		os_free(ctx);
		return NULL;
	}
	EVP_CIPHER_CTX_set_padding(ctx, 0);
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
	if (EVP_DecryptInit_ex(ctx, type, NULL, key, NULL) != 1) {
		EVP_CIPHER_CTX_free(ctx);
		return NULL;
	}
	EVP_CIPHER_CTX_set_padding(ctx, 0);
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

int aes_wrap(const u8 *kek, size_t kek_len, int n, const u8 *plain, u8 *cipher)
{
	AES_KEY actx;
	int res;

	if (TEST_FAIL())
		return -1;
	if (AES_set_encrypt_key(kek, kek_len << 3, &actx))
		return -1;
	res = AES_wrap_key(&actx, NULL, cipher, plain, n * 8);
	OPENSSL_cleanse(&actx, sizeof(actx));
	return res <= 0 ? -1 : 0;
}


int aes_unwrap(const u8 *kek, size_t kek_len, int n, const u8 *cipher,
	       u8 *plain)
{
	AES_KEY actx;
	int res;

	if (TEST_FAIL())
		return -1;
	if (AES_set_decrypt_key(kek, kek_len << 3, &actx))
		return -1;
	res = AES_unwrap_key(&actx, NULL, plain, cipher, (n + 1) * 8);
	OPENSSL_cleanse(&actx, sizeof(actx));
	return res <= 0 ? -1 : 0;
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
	    !EVP_CIPHER_CTX_set_padding(ctx->enc, 0) ||
	    !EVP_EncryptInit_ex(ctx->enc, cipher, NULL, NULL, NULL) ||
	    !EVP_CIPHER_CTX_set_key_length(ctx->enc, key_len) ||
	    !EVP_EncryptInit_ex(ctx->enc, NULL, NULL, key, iv)) {
		if (ctx->enc)
			EVP_CIPHER_CTX_free(ctx->enc);
		os_free(ctx);
		return NULL;
	}

	if (!(ctx->dec = EVP_CIPHER_CTX_new()) ||
	    !EVP_CIPHER_CTX_set_padding(ctx->dec, 0) ||
	    !EVP_DecryptInit_ex(ctx->dec, cipher, NULL, NULL, NULL) ||
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
#if OPENSSL_VERSION_NUMBER < 0x10100000L || \
	(defined(LIBRESSL_VERSION_NUMBER) && \
	 LIBRESSL_VERSION_NUMBER < 0x20700000L)
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
#if OPENSSL_VERSION_NUMBER < 0x10100000L || \
	(defined(LIBRESSL_VERSION_NUMBER) && \
	 LIBRESSL_VERSION_NUMBER < 0x20700000L)
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
}


void dh5_free(void *ctx)
{
	DH *dh;
	if (ctx == NULL)
		return;
	dh = ctx;
	DH_free(dh);
}


struct crypto_hash {
	HMAC_CTX *ctx;
};


struct crypto_hash * crypto_hash_init(enum crypto_hash_alg alg, const u8 *key,
				      size_t key_len)
{
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
}


void crypto_hash_update(struct crypto_hash *ctx, const u8 *data, size_t len)
{
	if (ctx == NULL)
		return;
	HMAC_Update(ctx->ctx, data, len);
}


int crypto_hash_finish(struct crypto_hash *ctx, u8 *mac, size_t *len)
{
	unsigned int mdlen;
	int res;

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
	bin_clear_free(ctx, sizeof(*ctx));

	if (TEST_FAIL())
		return -1;

	if (res == 1) {
		*len = mdlen;
		return 0;
	}

	return -1;
}


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


int pbkdf2_sha1(const char *passphrase, const u8 *ssid, size_t ssid_len,
		int iterations, u8 *buf, size_t buflen)
{
	if (PKCS5_PBKDF2_HMAC_SHA1(passphrase, os_strlen(passphrase), ssid,
				   ssid_len, iterations, buflen, buf) != 1)
		return -1;
	return 0;
}


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


int crypto_get_random(void *buf, size_t len)
{
	if (RAND_bytes(buf, len) != 1)
		return -1;
	return 0;
}


#ifdef CONFIG_OPENSSL_CMAC
int omac1_aes_vector(const u8 *key, size_t key_len, size_t num_elem,
		     const u8 *addr[], const size_t *len, u8 *mac)
{
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
#endif /* CONFIG_OPENSSL_CMAC */


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
	/* Note: BN_rshift() does not modify the first argument even though it
	 * has not been marked const. */
	return BN_rshift((BIGNUM *) a, (BIGNUM *) r, n) == 1 ? 0 : -1;
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
	BN_CTX *bnctx;
	BIGNUM *prime;
	BIGNUM *order;
	BIGNUM *a;
	BIGNUM *b;
};

struct crypto_ec * crypto_ec_init(int group)
{
	struct crypto_ec *e;
	int nid;

	/* Map from IANA registry for IKE D-H groups to OpenSSL NID */
	switch (group) {
	case 19:
		nid = NID_X9_62_prime256v1;
		break;
	case 20:
		nid = NID_secp384r1;
		break;
	case 21:
		nid = NID_secp521r1;
		break;
	case 25:
		nid = NID_X9_62_prime192v1;
		break;
	case 26:
		nid = NID_secp224r1;
		break;
#ifdef NID_brainpoolP224r1
	case 27:
		nid = NID_brainpoolP224r1;
		break;
#endif /* NID_brainpoolP224r1 */
#ifdef NID_brainpoolP256r1
	case 28:
		nid = NID_brainpoolP256r1;
		break;
#endif /* NID_brainpoolP256r1 */
#ifdef NID_brainpoolP384r1
	case 29:
		nid = NID_brainpoolP384r1;
		break;
#endif /* NID_brainpoolP384r1 */
#ifdef NID_brainpoolP512r1
	case 30:
		nid = NID_brainpoolP512r1;
		break;
#endif /* NID_brainpoolP512r1 */
	default:
		return NULL;
	}

	e = os_zalloc(sizeof(*e));
	if (e == NULL)
		return NULL;

	e->nid = nid;
	e->bnctx = BN_CTX_new();
	e->group = EC_GROUP_new_by_curve_name(nid);
	e->prime = BN_new();
	e->order = BN_new();
	e->a = BN_new();
	e->b = BN_new();
	if (e->group == NULL || e->bnctx == NULL || e->prime == NULL ||
	    e->order == NULL || e->a == NULL || e->b == NULL ||
	    !EC_GROUP_get_curve_GFp(e->group, e->prime, e->a, e->b, e->bnctx) ||
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
	return EC_POINT_get_affine_coordinates_GFp(e->group,
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
	    EC_POINT_get_affine_coordinates_GFp(e->group, (EC_POINT *) point,
						x_bn, y_bn, e->bnctx)) {
		if (x) {
			crypto_bignum_to_bin((struct crypto_bignum *) x_bn,
					     x, len, len);
		}
		if (y) {
			crypto_bignum_to_bin((struct crypto_bignum *) y_bn,
					     y, len, len);
		}
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

	if (!EC_POINT_set_affine_coordinates_GFp(e->group, elem, x, y,
						 e->bnctx)) {
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


int crypto_ec_point_solve_y_coord(struct crypto_ec *e,
				  struct crypto_ec_point *p,
				  const struct crypto_bignum *x, int y_bit)
{
	if (TEST_FAIL())
		return -1;
	if (!EC_POINT_set_compressed_coordinates_GFp(e->group, (EC_POINT *) p,
						     (const BIGNUM *) x, y_bit,
						     e->bnctx) ||
	    !EC_POINT_is_on_curve(e->group, (EC_POINT *) p, e->bnctx))
		return -1;
	return 0;
}


struct crypto_bignum *
crypto_ec_point_compute_y_sqr(struct crypto_ec *e,
			      const struct crypto_bignum *x)
{
	BIGNUM *tmp, *tmp2, *y_sqr = NULL;

	if (TEST_FAIL())
		return NULL;

	tmp = BN_new();
	tmp2 = BN_new();

	/* y^2 = x^3 + ax + b */
	if (tmp && tmp2 &&
	    BN_mod_sqr(tmp, (const BIGNUM *) x, e->prime, e->bnctx) &&
	    BN_mod_mul(tmp, tmp, (const BIGNUM *) x, e->prime, e->bnctx) &&
	    BN_mod_mul(tmp2, e->a, (const BIGNUM *) x, e->prime, e->bnctx) &&
	    BN_mod_add_quick(tmp2, tmp2, tmp, e->prime) &&
	    BN_mod_add_quick(tmp2, tmp2, e->b, e->prime)) {
		y_sqr = tmp2;
		tmp2 = NULL;
	}

	BN_clear_free(tmp);
	BN_clear_free(tmp2);

	return (struct crypto_bignum *) y_sqr;
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


struct crypto_ecdh {
	struct crypto_ec *ec;
	EVP_PKEY *pkey;
};

struct crypto_ecdh * crypto_ecdh_init(int group)
{
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
}


struct wpabuf * crypto_ecdh_get_pubkey(struct crypto_ecdh *ecdh, int inc_y)
{
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

	if (EC_POINT_get_affine_coordinates_GFp(ecdh->ec->group, pubkey,
						x, y, ecdh->ec->bnctx) != 1) {
		wpa_printf(MSG_ERROR,
			   "OpenSSL: EC_POINT_get_affine_coordinates_GFp failed: %s",
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
}


struct wpabuf * crypto_ecdh_set_peerkey(struct crypto_ecdh *ecdh, int inc_y,
					const u8 *key, size_t len)
{
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
		if (!EC_POINT_set_affine_coordinates_GFp(ecdh->ec->group, pub,
							 x, y,
							 ecdh->ec->bnctx)) {
			wpa_printf(MSG_ERROR,
				   "OpenSSL: EC_POINT_set_affine_coordinates_GFp failed: %s",
				   ERR_error_string(ERR_get_error(), NULL));
			goto fail;
		}
	} else if (!EC_POINT_set_compressed_coordinates_GFp(ecdh->ec->group,
							    pub, x, 0,
							    ecdh->ec->bnctx)) {
		wpa_printf(MSG_ERROR,
			   "OpenSSL: EC_POINT_set_compressed_coordinates_GFp failed: %s",
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


struct crypto_ec_key {
	EVP_PKEY *pkey;
	EC_KEY *eckey;
};


struct crypto_ec_key * crypto_ec_key_parse_priv(const u8 *der, size_t der_len)
{
	struct crypto_ec_key *key;

	key = os_zalloc(sizeof(*key));
	if (!key)
		return NULL;

	key->eckey = d2i_ECPrivateKey(NULL, &der, der_len);
	if (!key->eckey) {
		wpa_printf(MSG_INFO, "OpenSSL: d2i_ECPrivateKey() failed: %s",
			   ERR_error_string(ERR_get_error(), NULL));
		goto fail;
	}
	EC_KEY_set_conv_form(key->eckey, POINT_CONVERSION_COMPRESSED);

	key->pkey = EVP_PKEY_new();
	if (!key->pkey || EVP_PKEY_assign_EC_KEY(key->pkey, key->eckey) != 1) {
		EC_KEY_free(key->eckey);
		key->eckey = NULL;
		goto fail;
	}

	return key;
fail:
	crypto_ec_key_deinit(key);
	return NULL;
}


struct crypto_ec_key * crypto_ec_key_parse_pub(const u8 *der, size_t der_len)
{
	struct crypto_ec_key *key;

	key = os_zalloc(sizeof(*key));
	if (!key)
		return NULL;

	key->pkey = d2i_PUBKEY(NULL, &der, der_len);
	if (!key->pkey) {
		wpa_printf(MSG_INFO, "OpenSSL: d2i_PUBKEY() failed: %s",
			   ERR_error_string(ERR_get_error(), NULL));
		goto fail;
	}

	key->eckey = EVP_PKEY_get0_EC_KEY(key->pkey);
	if (!key->eckey)
		goto fail;
	return key;
fail:
	crypto_ec_key_deinit(key);
	return NULL;
}


void crypto_ec_key_deinit(struct crypto_ec_key *key)
{
	if (key) {
		EVP_PKEY_free(key->pkey);
		os_free(key);
	}
}


struct wpabuf * crypto_ec_key_get_subject_public_key(struct crypto_ec_key *key)
{
	unsigned char *der = NULL;
	int der_len;
	struct wpabuf *buf;

	der_len = i2d_PUBKEY(key->pkey, &der);
	if (der_len <= 0) {
		wpa_printf(MSG_INFO, "OpenSSL: i2d_PUBKEY() failed: %s",
			   ERR_error_string(ERR_get_error(), NULL));
		return NULL;
	}

	buf = wpabuf_alloc_copy(der, der_len);
	OPENSSL_free(der);
	return buf;
}


struct wpabuf * crypto_ec_key_sign(struct crypto_ec_key *key, const u8 *data,
				   size_t len)
{
	EVP_PKEY_CTX *pkctx;
	struct wpabuf *sig_der;
	size_t sig_len;

	sig_len = EVP_PKEY_size(key->pkey);
	sig_der = wpabuf_alloc(sig_len);
	if (!sig_der)
		return NULL;

	pkctx = EVP_PKEY_CTX_new(key->pkey, NULL);
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


int crypto_ec_key_verify_signature(struct crypto_ec_key *key, const u8 *data,
				   size_t len, const u8 *sig, size_t sig_len)
{
	EVP_PKEY_CTX *pkctx;
	int ret;

	pkctx = EVP_PKEY_CTX_new(key->pkey, NULL);
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


int crypto_ec_key_group(struct crypto_ec_key *key)
{
	const EC_GROUP *group;
	int nid;

	group = EC_KEY_get0_group(key->eckey);
	if (!group)
		return -1;
	nid = EC_GROUP_get_curve_name(group);
	switch (nid) {
	case NID_X9_62_prime256v1:
		return 19;
	case NID_secp384r1:
		return 20;
	case NID_secp521r1:
		return 21;
	}
	return -1;
}

#endif /* CONFIG_ECC */
