/*
 * Wrapper functions for libwolfssl
 * Copyright (c) 2004-2017, Jouni Malinen <j@w1.fi>
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 */

#include "includes.h"

#include "common.h"
#include "crypto.h"
#include "tls/asn1.h"

/* wolfSSL headers */
#include <wolfssl/options.h> /* options.h needs to be included first */
#include <wolfssl/version.h>
#include <wolfssl/openssl/bn.h>
#include <wolfssl/wolfcrypt/aes.h>
#include <wolfssl/wolfcrypt/arc4.h>
#include <wolfssl/wolfcrypt/asn_public.h>
#include <wolfssl/wolfcrypt/cmac.h>
#include <wolfssl/wolfcrypt/des3.h>
#include <wolfssl/wolfcrypt/dh.h>
#include <wolfssl/wolfcrypt/ecc.h>
#include <wolfssl/wolfcrypt/error-crypt.h>
#include <wolfssl/wolfcrypt/hmac.h>
#include <wolfssl/wolfcrypt/md4.h>
#include <wolfssl/wolfcrypt/md5.h>
#include <wolfssl/wolfcrypt/pkcs7.h>
#include <wolfssl/wolfcrypt/pwdbased.h>
#include <wolfssl/wolfcrypt/sha.h>
#include <wolfssl/wolfcrypt/sha256.h>
#include <wolfssl/wolfcrypt/sha512.h>

#ifdef CONFIG_FIPS
#ifndef HAVE_FIPS
#warning "You are compiling wpa_supplicant/hostapd in FIPS mode but wolfSSL is not configured for FIPS mode."
#endif /* HAVE_FIPS */
#endif /* CONFIG_FIPS */


#ifdef CONFIG_FIPS
#if !defined(HAVE_FIPS_VERSION) || HAVE_FIPS_VERSION <= 2
#define WOLFSSL_OLD_FIPS
#endif
#endif

#if LIBWOLFSSL_VERSION_HEX < 0x05004000
static int wc_EccPublicKeyToDer_ex(ecc_key *key, byte *output,
				   word32 inLen, int with_AlgCurve,
				   int comp)
{
	return wc_EccPublicKeyToDer(key, output, inLen, with_AlgCurve);
}
#endif /* version < 5.4.0 */

#define LOG_WOLF_ERROR_VA(msg, ...) \
	wpa_printf(MSG_ERROR, "wolfSSL: %s:%d " msg, \
		   __func__, __LINE__, __VA_ARGS__)

#define LOG_WOLF_ERROR(msg) \
	LOG_WOLF_ERROR_VA("%s", (msg))

#define LOG_WOLF_ERROR_FUNC(func, err) \
	LOG_WOLF_ERROR_VA(#func " failed with err: %d %s", \
			  (err), wc_GetErrorString(err))

#define LOG_WOLF_ERROR_FUNC_NULL(func) \
	LOG_WOLF_ERROR(#func " failed with NULL return")

#define LOG_INVALID_PARAMETERS() \
	LOG_WOLF_ERROR("invalid input parameters")


/* Helper functions to make type allocation uniform */

static WC_RNG * wc_rng_init(void)
{
	WC_RNG *ret;

#ifdef CONFIG_FIPS
	ret = os_zalloc(sizeof(WC_RNG));
	if (!ret) {
		LOG_WOLF_ERROR_FUNC_NULL(os_zalloc);
	} else {
		int err;

		err = wc_InitRng(ret);
		if (err != 0) {
			LOG_WOLF_ERROR_FUNC(wc_InitRng, err);
			os_free(ret);
			ret = NULL;
		}
	}
#else /* CONFIG_FIPS */
	ret = wc_rng_new(NULL, 0, NULL);
	if (!ret)
		LOG_WOLF_ERROR_FUNC_NULL(wc_rng_new);
#endif /* CONFIG_FIPS */

	return ret;
}


static void wc_rng_deinit(WC_RNG *rng)
{
#ifdef CONFIG_FIPS
	wc_FreeRng(rng);
	os_free(rng);
#else /* CONFIG_FIPS */
	wc_rng_free(rng);
#endif /* CONFIG_FIPS */
}


static ecc_key * ecc_key_init(void)
{
	ecc_key *ret;
#ifdef CONFIG_FIPS
	int err;

	ret = os_zalloc(sizeof(ecc_key));
	if (!ret) {
		LOG_WOLF_ERROR_FUNC_NULL(os_zalloc);
	} else {
		err = wc_ecc_init_ex(ret, NULL, INVALID_DEVID);
		if (err != 0) {
			LOG_WOLF_ERROR("wc_ecc_init_ex failed");
			os_free(ret);
			ret = NULL;
		}
	}
#else /* CONFIG_FIPS */
	ret = wc_ecc_key_new(NULL);
	if (!ret)
		LOG_WOLF_ERROR_FUNC_NULL(wc_ecc_key_new);
#endif /* CONFIG_FIPS */

	return ret;
}


static void ecc_key_deinit(ecc_key *key)
{
#ifdef CONFIG_FIPS
	wc_ecc_free(key);
	os_free(key);
#else /* CONFIG_FIPS */
	wc_ecc_key_free(key);
#endif /* CONFIG_FIPS */
}

/* end of helper functions */


#ifndef CONFIG_FIPS

int md4_vector(size_t num_elem, const u8 *addr[], const size_t *len, u8 *mac)
{
	Md4 md4;
	size_t i;

	if (TEST_FAIL())
		return -1;

	wc_InitMd4(&md4);

	for (i = 0; i < num_elem; i++)
		wc_Md4Update(&md4, addr[i], len[i]);

	wc_Md4Final(&md4, mac);

	return 0;
}


int md5_vector(size_t num_elem, const u8 *addr[], const size_t *len, u8 *mac)
{
	wc_Md5 md5;
	size_t i;
	int err;
	int ret = -1;

	if (TEST_FAIL())
		return -1;

	err = wc_InitMd5(&md5);
	if (err != 0) {
		LOG_WOLF_ERROR_FUNC(wc_InitMd5, err);
		return -1;
	}

	for (i = 0; i < num_elem; i++) {
		err = wc_Md5Update(&md5, addr[i], len[i]);
		if (err != 0) {
			LOG_WOLF_ERROR_FUNC(wc_Md5Update, err);
			goto fail;
		}
	}

	err = wc_Md5Final(&md5, mac);
	if (err != 0) {
		LOG_WOLF_ERROR_FUNC(wc_Md5Final, err);
		goto fail;
	}

	ret = 0;
fail:
	wc_Md5Free(&md5);
	return ret;
}

#endif /* CONFIG_FIPS */


int sha1_vector(size_t num_elem, const u8 *addr[], const size_t *len, u8 *mac)
{
	wc_Sha sha;
	size_t i;
	int err;
	int ret = -1;

	if (TEST_FAIL())
		return -1;

	err = wc_InitSha(&sha);
	if (err != 0) {
		LOG_WOLF_ERROR_FUNC(wc_InitSha, err);
		return -1;
	}

	for (i = 0; i < num_elem; i++) {
		err = wc_ShaUpdate(&sha, addr[i], len[i]);
		if (err != 0) {
			LOG_WOLF_ERROR_FUNC(wc_ShaUpdate, err);
			goto fail;
		}
	}

	err = wc_ShaFinal(&sha, mac);
	if (err != 0) {
		LOG_WOLF_ERROR_FUNC(wc_ShaFinal, err);
		goto fail;
	}

	ret = 0;
fail:
	wc_ShaFree(&sha);
	return ret;
}


#ifndef NO_SHA256_WRAPPER
int sha256_vector(size_t num_elem, const u8 *addr[], const size_t *len,
		  u8 *mac)
{
	wc_Sha256 sha256;
	size_t i;
	int err;
	int ret = -1;

	if (TEST_FAIL())
		return -1;

	err = wc_InitSha256(&sha256);
	if (err != 0) {
		LOG_WOLF_ERROR_FUNC(wc_InitSha256, err);
		return -1;
	}

	for (i = 0; i < num_elem; i++) {
		err = wc_Sha256Update(&sha256, addr[i], len[i]);
		if (err != 0) {
			LOG_WOLF_ERROR_FUNC(wc_Sha256Update, err);
			goto fail;
		}
	}

	err = wc_Sha256Final(&sha256, mac);
	if (err != 0) {
		LOG_WOLF_ERROR_FUNC(wc_Sha256Final, err);
		goto fail;
	}

	ret = 0;
fail:
	wc_Sha256Free(&sha256);
	return ret;
}
#endif /* NO_SHA256_WRAPPER */


#ifdef CONFIG_SHA384
int sha384_vector(size_t num_elem, const u8 *addr[], const size_t *len,
		  u8 *mac)
{
	wc_Sha384 sha384;
	size_t i;
	int err;
	int ret = -1;

	if (TEST_FAIL())
		return -1;

	err = wc_InitSha384(&sha384);
	if (err != 0) {
		LOG_WOLF_ERROR_FUNC(wc_InitSha384, err);
		return -1;
	}

	for (i = 0; i < num_elem; i++) {
		err = wc_Sha384Update(&sha384, addr[i], len[i]);
		if (err != 0) {
			LOG_WOLF_ERROR_FUNC(wc_Sha384Update, err);
			goto fail;
		}
	}

	err = wc_Sha384Final(&sha384, mac);
	if (err != 0) {
		LOG_WOLF_ERROR_FUNC(wc_Sha384Final, err);
		goto fail;
	}

	ret = 0;
fail:
	wc_Sha384Free(&sha384);
	return ret;
}
#endif /* CONFIG_SHA384 */


#ifdef CONFIG_SHA512
int sha512_vector(size_t num_elem, const u8 *addr[], const size_t *len,
		  u8 *mac)
{
	wc_Sha512 sha512;
	size_t i;
	int err;
	int ret = -1;

	if (TEST_FAIL())
		return -1;

	err = wc_InitSha512(&sha512);
	if (err != 0) {
		LOG_WOLF_ERROR_FUNC(wc_InitSha512, err);
		return -1;
	}

	for (i = 0; i < num_elem; i++) {
		err = wc_Sha512Update(&sha512, addr[i], len[i]);
		if (err != 0) {
			LOG_WOLF_ERROR_FUNC(wc_Sha512Update, err);
			goto fail;
		}
	}

	err = wc_Sha512Final(&sha512, mac);
	if (err != 0) {
		LOG_WOLF_ERROR_FUNC(wc_Sha512Final, err);
		goto fail;
	}

	ret = 0;
fail:
	wc_Sha512Free(&sha512);
	return ret;
}
#endif /* CONFIG_SHA512 */


static int wolfssl_hmac_vector(int type, const u8 *key,
			       size_t key_len, size_t num_elem,
			       const u8 *addr[], const size_t *len, u8 *mac,
			       unsigned int mdlen)
{
	Hmac hmac;
	size_t i;
	int err;
	int ret = -1;

	(void) mdlen;

	if (TEST_FAIL())
		return -1;

	err = wc_HmacInit(&hmac, NULL, INVALID_DEVID);
	if (err != 0) {
		LOG_WOLF_ERROR_FUNC(wc_HmacInit, err);
		return -1;
	}

	err = wc_HmacSetKey(&hmac, type, key, (word32) key_len);
	if (err != 0) {
		LOG_WOLF_ERROR_FUNC(wc_HmacSetKey, err);
		goto fail;
	}

	for (i = 0; i < num_elem; i++) {
		err = wc_HmacUpdate(&hmac, addr[i], len[i]);
		if (err != 0) {
			LOG_WOLF_ERROR_FUNC(wc_HmacUpdate, err);
			goto fail;
		}
	}
	err = wc_HmacFinal(&hmac, mac);
	if (err != 0) {
		LOG_WOLF_ERROR_FUNC(wc_HmacFinal, err);
		goto fail;
	}

	ret = 0;
fail:
	wc_HmacFree(&hmac);
	return ret;
}


#ifndef CONFIG_FIPS

int hmac_md5_vector(const u8 *key, size_t key_len, size_t num_elem,
		    const u8 *addr[], const size_t *len, u8 *mac)
{
	return wolfssl_hmac_vector(WC_MD5, key, key_len, num_elem, addr, len,
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
	return wolfssl_hmac_vector(WC_SHA, key, key_len, num_elem, addr, len,
				   mac, 20);
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
	return wolfssl_hmac_vector(WC_SHA256, key, key_len, num_elem, addr, len,
				   mac, 32);
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
	return wolfssl_hmac_vector(WC_SHA384, key, key_len, num_elem, addr, len,
				   mac, 48);
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
	return wolfssl_hmac_vector(WC_SHA512, key, key_len, num_elem, addr, len,
				   mac, 64);
}


int hmac_sha512(const u8 *key, size_t key_len, const u8 *data,
		size_t data_len, u8 *mac)
{
	return hmac_sha512_vector(key, key_len, 1, &data, &data_len, mac);
}

#endif /* CONFIG_SHA512 */


int pbkdf2_sha1(const char *passphrase, const u8 *ssid, size_t ssid_len,
		int iterations, u8 *buf, size_t buflen)
{
	int ret;

	ret = wc_PBKDF2(buf, (const byte *) passphrase, os_strlen(passphrase),
			ssid, ssid_len, iterations, buflen, WC_SHA);
	if (ret != 0) {
		if (ret == HMAC_MIN_KEYLEN_E) {
			LOG_WOLF_ERROR_VA("wolfSSL: Password is too short. Make sure your password is at least %d characters long. This is a requirement for FIPS builds.",
					  HMAC_FIPS_MIN_KEY);
		}
		return -1;
	}
	return 0;
}


#ifdef CONFIG_DES
int des_encrypt(const u8 *clear, const u8 *key, u8 *cypher)
{
	Des des;
	u8  pkey[8], next, tmp;
	int i;

	/* Add parity bits to the key */
	next = 0;
	for (i = 0; i < 7; i++) {
		tmp = key[i];
		pkey[i] = (tmp >> i) | next | 1;
		next = tmp << (7 - i);
	}
	pkey[i] = next | 1;

	wc_Des_SetKey(&des, pkey, NULL, DES_ENCRYPTION);
	wc_Des_EcbEncrypt(&des, cypher, clear, DES_BLOCK_SIZE);

	return 0;
}
#endif /* CONFIG_DES */


void * aes_encrypt_init(const u8 *key, size_t len)
{
	Aes *aes;
	int err;

	if (TEST_FAIL())
		return NULL;

	aes = os_malloc(sizeof(Aes));
	if (!aes) {
		LOG_WOLF_ERROR_FUNC_NULL(os_malloc);
		return NULL;
	}

	err = wc_AesSetKey(aes, key, len, NULL, AES_ENCRYPTION);
	if (err < 0) {
		LOG_WOLF_ERROR_FUNC(wc_AesSetKey, err);
		os_free(aes);
		return NULL;
	}

	return aes;
}


int aes_encrypt(void *ctx, const u8 *plain, u8 *crypt)
{
#if defined(HAVE_FIPS) && \
    (!defined(HAVE_FIPS_VERSION) || (HAVE_FIPS_VERSION <= 2))
	/* Old FIPS has void return on this API */
	wc_AesEncryptDirect(ctx, crypt, plain);
#else
	int err = wc_AesEncryptDirect(ctx, crypt, plain);

	if (err != 0) {
		LOG_WOLF_ERROR_FUNC(wc_AesEncryptDirect, err);
		return -1;
	}
#endif
	return 0;
}


void aes_encrypt_deinit(void *ctx)
{
	os_free(ctx);
}


void * aes_decrypt_init(const u8 *key, size_t len)
{
	Aes *aes;
	int err;

	if (TEST_FAIL())
		return NULL;

	aes = os_malloc(sizeof(Aes));
	if (!aes) {
		LOG_WOLF_ERROR_FUNC_NULL(os_malloc);
		return NULL;
	}

	err = wc_AesSetKey(aes, key, len, NULL, AES_DECRYPTION);
	if (err < 0) {
		LOG_WOLF_ERROR_FUNC(wc_AesSetKey, err);
		os_free(aes);
		return NULL;
	}

	return aes;
}


int aes_decrypt(void *ctx, const u8 *crypt, u8 *plain)
{
#if defined(HAVE_FIPS) && \
    (!defined(HAVE_FIPS_VERSION) || (HAVE_FIPS_VERSION <= 2))
	/* Old FIPS has void return on this API */
	wc_AesDecryptDirect(ctx, plain, crypt);
#else
	int err = wc_AesDecryptDirect(ctx, plain, crypt);

	if (err != 0) {
		LOG_WOLF_ERROR_FUNC(wc_AesDecryptDirect, err);
		return -1;
	}
#endif
	return 0;
}


void aes_decrypt_deinit(void *ctx)
{
	os_free(ctx);
}


int aes_128_cbc_encrypt(const u8 *key, const u8 *iv, u8 *data, size_t data_len)
{
	Aes aes;
	int ret;

	if (TEST_FAIL())
		return -1;

	ret = wc_AesSetKey(&aes, key, 16, iv, AES_ENCRYPTION);
	if (ret != 0)
		return -1;

	ret = wc_AesCbcEncrypt(&aes, data, data, data_len);
	if (ret != 0)
		return -1;
	return 0;
}


int aes_128_cbc_decrypt(const u8 *key, const u8 *iv, u8 *data, size_t data_len)
{
	Aes aes;
	int ret;

	if (TEST_FAIL())
		return -1;

	ret = wc_AesSetKey(&aes, key, 16, iv, AES_DECRYPTION);
	if (ret != 0)
		return -1;

	ret = wc_AesCbcDecrypt(&aes, data, data, data_len);
	if (ret != 0)
		return -1;
	return 0;
}


#ifndef CONFIG_FIPS
#ifndef CONFIG_OPENSSL_INTERNAL_AES_WRAP
int aes_wrap(const u8 *kek, size_t kek_len, int n, const u8 *plain, u8 *cipher)
{
#ifdef HAVE_AES_KEYWRAP
	int ret;

	if (TEST_FAIL())
		return -1;

	ret = wc_AesKeyWrap(kek, kek_len, plain, n * 8, cipher, (n + 1) * 8,
			    NULL);
	return ret != (n + 1) * 8 ? -1 : 0;
#else /* HAVE_AES_KEYWRAP */
	return -1;
#endif /* HAVE_AES_KEYWRAP */
}


int aes_unwrap(const u8 *kek, size_t kek_len, int n, const u8 *cipher,
	       u8 *plain)
{
#ifdef HAVE_AES_KEYWRAP
	int ret;

	if (TEST_FAIL())
		return -1;

	ret = wc_AesKeyUnWrap(kek, kek_len, cipher, (n + 1) * 8, plain, n * 8,
			      NULL);
	return ret != n * 8 ? -1 : 0;
#else /* HAVE_AES_KEYWRAP */
	return -1;
#endif /* HAVE_AES_KEYWRAP */
}
#endif /* CONFIG_OPENSSL_INTERNAL_AES_WRAP */
#endif /* CONFIG_FIPS */


#ifndef CONFIG_NO_RC4
int rc4_skip(const u8 *key, size_t keylen, size_t skip, u8 *data,
	     size_t data_len)
{
#ifndef NO_RC4
	Arc4 arc4;
	unsigned char skip_buf[16];

	wc_Arc4SetKey(&arc4, key, keylen);

	while (skip >= sizeof(skip_buf)) {
		size_t len = skip;

		if (len > sizeof(skip_buf))
			len = sizeof(skip_buf);
		wc_Arc4Process(&arc4, skip_buf, skip_buf, len);
		skip -= len;
	}

	wc_Arc4Process(&arc4, data, data, data_len);

	return 0;
#else /* NO_RC4 */
	return -1;
#endif /* NO_RC4 */
}
#endif /* CONFIG_NO_RC4 */


#if defined(EAP_IKEV2) || defined(EAP_IKEV2_DYNAMIC) \
		       || defined(EAP_SERVER_IKEV2)
union wolfssl_cipher {
	Aes aes;
	Des3 des3;
	Arc4 arc4;
};

struct crypto_cipher {
	enum crypto_cipher_alg alg;
	union wolfssl_cipher enc;
	union wolfssl_cipher dec;
};

struct crypto_cipher * crypto_cipher_init(enum crypto_cipher_alg alg,
					  const u8 *iv, const u8 *key,
					  size_t key_len)
{
	struct crypto_cipher *ctx;

	ctx = os_zalloc(sizeof(*ctx));
	if (!ctx)
		return NULL;

	switch (alg) {
#ifndef CONFIG_NO_RC4
#ifndef NO_RC4
	case CRYPTO_CIPHER_ALG_RC4:
		wc_Arc4SetKey(&ctx->enc.arc4, key, key_len);
		wc_Arc4SetKey(&ctx->dec.arc4, key, key_len);
		break;
#endif /* NO_RC4 */
#endif /* CONFIG_NO_RC4 */
#ifndef NO_AES
	case CRYPTO_CIPHER_ALG_AES:
		switch (key_len) {
		case 16:
		case 24:
		case 32:
			break;
		default:
			os_free(ctx);
			return NULL;
		}
		if (wc_AesSetKey(&ctx->enc.aes, key, key_len, iv,
				 AES_ENCRYPTION) ||
		    wc_AesSetKey(&ctx->dec.aes, key, key_len, iv,
				 AES_DECRYPTION)) {
			os_free(ctx);
			return NULL;
		}
		break;
#endif /* NO_AES */
#ifndef NO_DES3
	case CRYPTO_CIPHER_ALG_3DES:
		if (key_len != DES3_KEYLEN ||
		    wc_Des3_SetKey(&ctx->enc.des3, key, iv, DES_ENCRYPTION) ||
		    wc_Des3_SetKey(&ctx->dec.des3, key, iv, DES_DECRYPTION)) {
			os_free(ctx);
			return NULL;
		}
		break;
#endif /* NO_DES3 */
	case CRYPTO_CIPHER_ALG_RC2:
	case CRYPTO_CIPHER_ALG_DES:
	default:
		os_free(ctx);
		return NULL;
	}

	ctx->alg = alg;

	return ctx;
}


int crypto_cipher_encrypt(struct crypto_cipher *ctx, const u8 *plain,
			  u8 *crypt, size_t len)
{
	switch (ctx->alg) {
#ifndef CONFIG_NO_RC4
#ifndef NO_RC4
	case CRYPTO_CIPHER_ALG_RC4:
		wc_Arc4Process(&ctx->enc.arc4, crypt, plain, len);
		return 0;
#endif /* NO_RC4 */
#endif /* CONFIG_NO_RC4 */
#ifndef NO_AES
	case CRYPTO_CIPHER_ALG_AES:
		if (wc_AesCbcEncrypt(&ctx->enc.aes, crypt, plain, len) != 0)
			return -1;
		return 0;
#endif /* NO_AES */
#ifndef NO_DES3
	case CRYPTO_CIPHER_ALG_3DES:
		if (wc_Des3_CbcEncrypt(&ctx->enc.des3, crypt, plain, len) != 0)
			return -1;
		return 0;
#endif /* NO_DES3 */
	default:
		return -1;
	}
	return -1;
}


int crypto_cipher_decrypt(struct crypto_cipher *ctx, const u8 *crypt,
			  u8 *plain, size_t len)
{
	switch (ctx->alg) {
#ifndef CONFIG_NO_RC4
#ifndef NO_RC4
	case CRYPTO_CIPHER_ALG_RC4:
		wc_Arc4Process(&ctx->dec.arc4, plain, crypt, len);
		return 0;
#endif /* NO_RC4 */
#endif /* CONFIG_NO_RC4 */
#ifndef NO_AES
	case CRYPTO_CIPHER_ALG_AES:
		if (wc_AesCbcDecrypt(&ctx->dec.aes, plain, crypt, len) != 0)
			return -1;
		return 0;
#endif /* NO_AES */
#ifndef NO_DES3
	case CRYPTO_CIPHER_ALG_3DES:
		if (wc_Des3_CbcDecrypt(&ctx->dec.des3, plain, crypt, len) != 0)
			return -1;
		return 0;
#endif /* NO_DES3 */
	default:
		return -1;
	}
	return -1;
}


void crypto_cipher_deinit(struct crypto_cipher *ctx)
{
	os_free(ctx);
}

#endif


#ifdef CONFIG_WPS

static const unsigned char RFC3526_PRIME_1536[] = {
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xC9, 0x0F, 0xDA, 0xA2,
	0x21, 0x68, 0xC2, 0x34, 0xC4, 0xC6, 0x62, 0x8B, 0x80, 0xDC, 0x1C, 0xD1,
	0x29, 0x02, 0x4E, 0x08, 0x8A, 0x67, 0xCC, 0x74, 0x02, 0x0B, 0xBE, 0xA6,
	0x3B, 0x13, 0x9B, 0x22, 0x51, 0x4A, 0x08, 0x79, 0x8E, 0x34, 0x04, 0xDD,
	0xEF, 0x95, 0x19, 0xB3, 0xCD, 0x3A, 0x43, 0x1B, 0x30, 0x2B, 0x0A, 0x6D,
	0xF2, 0x5F, 0x14, 0x37, 0x4F, 0xE1, 0x35, 0x6D, 0x6D, 0x51, 0xC2, 0x45,
	0xE4, 0x85, 0xB5, 0x76, 0x62, 0x5E, 0x7E, 0xC6, 0xF4, 0x4C, 0x42, 0xE9,
	0xA6, 0x37, 0xED, 0x6B, 0x0B, 0xFF, 0x5C, 0xB6, 0xF4, 0x06, 0xB7, 0xED,
	0xEE, 0x38, 0x6B, 0xFB, 0x5A, 0x89, 0x9F, 0xA5, 0xAE, 0x9F, 0x24, 0x11,
	0x7C, 0x4B, 0x1F, 0xE6, 0x49, 0x28, 0x66, 0x51, 0xEC, 0xE4, 0x5B, 0x3D,
	0xC2, 0x00, 0x7C, 0xB8, 0xA1, 0x63, 0xBF, 0x05, 0x98, 0xDA, 0x48, 0x36,
	0x1C, 0x55, 0xD3, 0x9A, 0x69, 0x16, 0x3F, 0xA8, 0xFD, 0x24, 0xCF, 0x5F,
	0x83, 0x65, 0x5D, 0x23, 0xDC, 0xA3, 0xAD, 0x96, 0x1C, 0x62, 0xF3, 0x56,
	0x20, 0x85, 0x52, 0xBB, 0x9E, 0xD5, 0x29, 0x07, 0x70, 0x96, 0x96, 0x6D,
	0x67, 0x0C, 0x35, 0x4E, 0x4A, 0xBC, 0x98, 0x04, 0xF1, 0x74, 0x6C, 0x08,
	0xCA, 0x23, 0x73, 0x27, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF
};

static const unsigned char RFC3526_GENERATOR_1536[] = {
	0x02
};

#define RFC3526_LEN sizeof(RFC3526_PRIME_1536)


void * dh5_init(struct wpabuf **priv, struct wpabuf **publ)
{
	WC_RNG rng;
	DhKey *ret = NULL;
	DhKey *dh = NULL;
	struct wpabuf *privkey = NULL;
	struct wpabuf *pubkey = NULL;
	word32 priv_sz, pub_sz;

	*priv = NULL;
	wpabuf_free(*publ);
	*publ = NULL;

	dh = XMALLOC(sizeof(DhKey), NULL, DYNAMIC_TYPE_TMP_BUFFER);
	if (!dh)
		return NULL;
	wc_InitDhKey(dh);

	if (wc_InitRng(&rng) != 0) {
		XFREE(dh, NULL, DYNAMIC_TYPE_TMP_BUFFER);
		return NULL;
	}

	privkey = wpabuf_alloc(RFC3526_LEN);
	pubkey = wpabuf_alloc(RFC3526_LEN);
	if (!privkey || !pubkey)
		goto done;

	if (wc_DhSetKey(dh, RFC3526_PRIME_1536, sizeof(RFC3526_PRIME_1536),
			RFC3526_GENERATOR_1536, sizeof(RFC3526_GENERATOR_1536))
	    != 0)
		goto done;

	priv_sz = pub_sz = RFC3526_LEN;
	if (wc_DhGenerateKeyPair(dh, &rng, wpabuf_mhead(privkey), &priv_sz,
				 wpabuf_mhead(pubkey), &pub_sz) != 0)
		goto done;

	wpabuf_put(privkey, priv_sz);
	wpabuf_put(pubkey, pub_sz);

	ret = dh;
	*priv = privkey;
	*publ = pubkey;
	dh = NULL;
	privkey = NULL;
	pubkey = NULL;
done:
	wpabuf_clear_free(pubkey);
	wpabuf_clear_free(privkey);
	if (dh) {
		wc_FreeDhKey(dh);
		XFREE(dh, NULL, DYNAMIC_TYPE_TMP_BUFFER);
	}
	wc_FreeRng(&rng);
	return ret;
}


#ifdef CONFIG_WPS_NFC

void * dh5_init_fixed(const struct wpabuf *priv, const struct wpabuf *publ)
{
	DhKey *ret = NULL;
	DhKey *dh;
	byte *secret;
	word32 secret_sz;

	dh = XMALLOC(sizeof(DhKey), NULL, DYNAMIC_TYPE_TMP_BUFFER);
	if (!dh)
		return NULL;
	wc_InitDhKey(dh);

	secret = XMALLOC(RFC3526_LEN, NULL, DYNAMIC_TYPE_TMP_BUFFER);
	if (!secret)
		goto done;

	if (wc_DhSetKey(dh, RFC3526_PRIME_1536, sizeof(RFC3526_PRIME_1536),
			RFC3526_GENERATOR_1536, sizeof(RFC3526_GENERATOR_1536))
	    != 0)
		goto done;

	if (wc_DhAgree(dh, secret, &secret_sz, wpabuf_head(priv),
		       wpabuf_len(priv), RFC3526_GENERATOR_1536,
		       sizeof(RFC3526_GENERATOR_1536)) != 0)
		goto done;

	if (secret_sz != wpabuf_len(publ) ||
	    os_memcmp(secret, wpabuf_head(publ), secret_sz) != 0)
		goto done;

	ret = dh;
	dh = NULL;
done:
	if (dh) {
		wc_FreeDhKey(dh);
		XFREE(dh, NULL, DYNAMIC_TYPE_TMP_BUFFER);
	}
	XFREE(secret, NULL, DYNAMIC_TYPE_TMP_BUFFER);
	return ret;
}

#endif /* CONFIG_WPS_NFC */


struct wpabuf * dh5_derive_shared(void *ctx, const struct wpabuf *peer_public,
				  const struct wpabuf *own_private)
{
	struct wpabuf *ret = NULL;
	struct wpabuf *secret;
	word32 secret_sz;

	secret = wpabuf_alloc(RFC3526_LEN);
	if (!secret)
		goto done;

	if (wc_DhAgree(ctx, wpabuf_mhead(secret), &secret_sz,
		       wpabuf_head(own_private), wpabuf_len(own_private),
		       wpabuf_head(peer_public), wpabuf_len(peer_public)) != 0)
		goto done;

	wpabuf_put(secret, secret_sz);

	ret = secret;
	secret = NULL;
done:
	wpabuf_clear_free(secret);
	return ret;
}


void dh5_free(void *ctx)
{
	if (!ctx)
		return;

	wc_FreeDhKey(ctx);
	XFREE(ctx, NULL, DYNAMIC_TYPE_TMP_BUFFER);
}

#endif /* CONFIG_WPS */


int crypto_dh_init(u8 generator, const u8 *prime, size_t prime_len, u8 *privkey,
		   u8 *pubkey)
{
	int ret = -1;
	WC_RNG rng;
	DhKey *dh = NULL;
	word32 priv_sz, pub_sz;

	if (TEST_FAIL())
		return -1;

	dh = os_malloc(sizeof(DhKey));
	if (!dh)
		return -1;
	wc_InitDhKey(dh);

	if (wc_InitRng(&rng) != 0) {
		os_free(dh);
		return -1;
	}

	if (wc_DhSetKey(dh, prime, prime_len, &generator, 1) != 0)
		goto done;

	priv_sz = pub_sz = prime_len;
	if (wc_DhGenerateKeyPair(dh, &rng, privkey, &priv_sz, pubkey, &pub_sz)
	    != 0)
		goto done;

	if (priv_sz < prime_len) {
		size_t pad_sz = prime_len - priv_sz;

		os_memmove(privkey + pad_sz, privkey, priv_sz);
		os_memset(privkey, 0, pad_sz);
	}

	if (pub_sz < prime_len) {
		size_t pad_sz = prime_len - pub_sz;

		os_memmove(pubkey + pad_sz, pubkey, pub_sz);
		os_memset(pubkey, 0, pad_sz);
	}
	ret = 0;
done:
	wc_FreeDhKey(dh);
	os_free(dh);
	wc_FreeRng(&rng);
	return ret;
}


int crypto_dh_derive_secret(u8 generator, const u8 *prime, size_t prime_len,
			    const u8 *order, size_t order_len,
			    const u8 *privkey, size_t privkey_len,
			    const u8 *pubkey, size_t pubkey_len,
			    u8 *secret, size_t *len)
{
	int ret = -1;
	DhKey *dh;
	word32 secret_sz;

	dh = os_malloc(sizeof(DhKey));
	if (!dh)
		return -1;
	wc_InitDhKey(dh);

	if (wc_DhSetKey(dh, prime, prime_len, &generator, 1) != 0)
		goto done;

	if (wc_DhAgree(dh, secret, &secret_sz, privkey, privkey_len, pubkey,
		       pubkey_len) != 0)
		goto done;

	*len = secret_sz;
	ret = 0;
done:
	wc_FreeDhKey(dh);
	os_free(dh);
	return ret;
}


#ifdef CONFIG_FIPS
int crypto_get_random(void *buf, size_t len)
{
	int ret = 0;
	WC_RNG rng;

	if (wc_InitRng(&rng) != 0)
		return -1;
	if (wc_RNG_GenerateBlock(&rng, buf, len) != 0)
		ret = -1;
	wc_FreeRng(&rng);
	return ret;
}
#endif /* CONFIG_FIPS */


#if defined(EAP_PWD) || defined(EAP_SERVER_PWD)
struct crypto_hash {
	Hmac hmac;
	int size;
};


struct crypto_hash * crypto_hash_init(enum crypto_hash_alg alg, const u8 *key,
				      size_t key_len)
{
	struct crypto_hash *ret = NULL;
	struct crypto_hash *hash;
	int type;

	hash = os_zalloc(sizeof(*hash));
	if (!hash)
		goto done;

	switch (alg) {
#ifndef NO_MD5
	case CRYPTO_HASH_ALG_HMAC_MD5:
		hash->size = 16;
		type = WC_MD5;
		break;
#endif /* NO_MD5 */
#ifndef NO_SHA
	case CRYPTO_HASH_ALG_HMAC_SHA1:
		type = WC_SHA;
		hash->size = 20;
		break;
#endif /* NO_SHA */
#ifdef CONFIG_SHA256
#ifndef NO_SHA256
	case CRYPTO_HASH_ALG_HMAC_SHA256:
		type = WC_SHA256;
		hash->size = 32;
		break;
#endif /* NO_SHA256 */
#endif /* CONFIG_SHA256 */
	default:
		goto done;
	}

	if (wc_HmacInit(&hash->hmac, NULL, INVALID_DEVID) != 0 ||
	    wc_HmacSetKey(&hash->hmac, type, key, key_len) != 0)
		goto done;

	ret = hash;
	hash = NULL;
done:
	os_free(hash);
	return ret;
}


void crypto_hash_update(struct crypto_hash *ctx, const u8 *data, size_t len)
{
	if (!ctx)
		return;
	wc_HmacUpdate(&ctx->hmac, data, len);
}


int crypto_hash_finish(struct crypto_hash *ctx, u8 *mac, size_t *len)
{
	int ret = 0;

	if (!ctx)
		return -2;

	if (!mac || !len)
		goto done;

	if (wc_HmacFinal(&ctx->hmac, mac) != 0) {
		ret = -1;
		goto done;
	}

	*len = ctx->size;
	ret = 0;
done:
	bin_clear_free(ctx, sizeof(*ctx));
	if (TEST_FAIL())
		return -1;
	return ret;
}

#endif


int omac1_aes_vector(const u8 *key, size_t key_len, size_t num_elem,
		     const u8 *addr[], const size_t *len, u8 *mac)
{
	Cmac cmac;
	size_t i;
	word32 sz;

	if (TEST_FAIL())
		return -1;

	if (wc_InitCmac(&cmac, key, key_len, WC_CMAC_AES, NULL) != 0)
		return -1;

	for (i = 0; i < num_elem; i++)
		if (wc_CmacUpdate(&cmac, addr[i], len[i]) != 0)
			return -1;

	sz = AES_BLOCK_SIZE;
	if (wc_CmacFinal(&cmac, mac, &sz) != 0 || sz != AES_BLOCK_SIZE)
		return -1;

	return 0;
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
	mp_int *a;

	if (TEST_FAIL())
		return NULL;

	a = os_malloc(sizeof(*a));
	if (!a || mp_init(a) != MP_OKAY) {
		os_free(a);
		a = NULL;
	}

	return (struct crypto_bignum *) a;
}


struct crypto_bignum * crypto_bignum_init_set(const u8 *buf, size_t len)
{
	mp_int *a;

	if (TEST_FAIL())
		return NULL;

	a = (mp_int *) crypto_bignum_init();
	if (!a)
		return NULL;

	if (mp_read_unsigned_bin(a, buf, len) != MP_OKAY) {
		os_free(a);
		a = NULL;
	}

	return (struct crypto_bignum *) a;
}


struct crypto_bignum * crypto_bignum_init_uint(unsigned int val)
{
	mp_int *a;

	if (TEST_FAIL())
		return NULL;

	a = (mp_int *) crypto_bignum_init();
	if (!a)
		return NULL;

	if (mp_set_int(a, val) != MP_OKAY) {
		os_free(a);
		a = NULL;
	}

	return (struct crypto_bignum *) a;
}


void crypto_bignum_deinit(struct crypto_bignum *n, int clear)
{
	if (!n)
		return;

	if (clear)
		mp_forcezero((mp_int *) n);
	mp_clear((mp_int *) n);
	os_free((mp_int *) n);
}


int crypto_bignum_to_bin(const struct crypto_bignum *a,
			 u8 *buf, size_t buflen, size_t padlen)
{
	int num_bytes, offset;

	if (TEST_FAIL())
		return -1;

	if (padlen > buflen)
		return -1;

	num_bytes = (mp_count_bits((mp_int *) a) + 7) / 8;
	if ((size_t) num_bytes > buflen)
		return -1;
	if (padlen > (size_t) num_bytes)
		offset = padlen - num_bytes;
	else
		offset = 0;

	os_memset(buf, 0, offset);
	mp_to_unsigned_bin((mp_int *) a, buf + offset);

	return num_bytes + offset;
}


int crypto_bignum_rand(struct crypto_bignum *r, const struct crypto_bignum *m)
{
	int ret = 0;
	WC_RNG rng;
	size_t len;
	u8 *buf;

	if (TEST_FAIL())
		return -1;
	if (wc_InitRng(&rng) != 0)
		return -1;
	len = (mp_count_bits((mp_int *) m) + 7) / 8;
	buf = os_malloc(len);
	if (!buf || wc_RNG_GenerateBlock(&rng, buf, len) != 0 ||
	    mp_read_unsigned_bin((mp_int *) r, buf, len) != MP_OKAY ||
	    mp_mod((mp_int *) r, (mp_int *) m, (mp_int *) r) != 0)
		ret = -1;
	wc_FreeRng(&rng);
	bin_clear_free(buf, len);
	return ret;
}


int crypto_bignum_add(const struct crypto_bignum *a,
		      const struct crypto_bignum *b,
		      struct crypto_bignum *r)
{
	return mp_add((mp_int *) a, (mp_int *) b,
		      (mp_int *) r) == MP_OKAY ? 0 : -1;
}


int crypto_bignum_mod(const struct crypto_bignum *a,
		      const struct crypto_bignum *m,
		      struct crypto_bignum *r)
{
	return mp_mod((mp_int *) a, (mp_int *) m,
		      (mp_int *) r) == MP_OKAY ? 0 : -1;
}


int crypto_bignum_exptmod(const struct crypto_bignum *b,
			  const struct crypto_bignum *e,
			  const struct crypto_bignum *m,
			  struct crypto_bignum *r)
{
	if (TEST_FAIL())
		return -1;

	return mp_exptmod((mp_int *) b, (mp_int *) e, (mp_int *) m,
			  (mp_int *) r) == MP_OKAY ?  0 : -1;
}


int crypto_bignum_inverse(const struct crypto_bignum *a,
			  const struct crypto_bignum *m,
			  struct crypto_bignum *r)
{
	if (TEST_FAIL())
		return -1;

	return mp_invmod((mp_int *) a, (mp_int *) m,
			 (mp_int *) r) == MP_OKAY ? 0 : -1;
}


int crypto_bignum_sub(const struct crypto_bignum *a,
		      const struct crypto_bignum *b,
		      struct crypto_bignum *r)
{
	if (TEST_FAIL())
		return -1;

	return mp_sub((mp_int *) a, (mp_int *) b,
		      (mp_int *) r) == MP_OKAY ? 0 : -1;
}


int crypto_bignum_div(const struct crypto_bignum *a,
		      const struct crypto_bignum *b,
		      struct crypto_bignum *d)
{
	if (TEST_FAIL())
		return -1;

	return mp_div((mp_int *) a, (mp_int *) b, (mp_int *) d,
		      NULL) == MP_OKAY ? 0 : -1;
}


int crypto_bignum_addmod(const struct crypto_bignum *a,
			 const struct crypto_bignum *b,
			 const struct crypto_bignum *c,
			 struct crypto_bignum *d)
{
	if (TEST_FAIL())
		return -1;

	return mp_addmod((mp_int *) a, (mp_int *) b, (mp_int *) c,
			 (mp_int *) d) == MP_OKAY ?  0 : -1;
}


int crypto_bignum_mulmod(const struct crypto_bignum *a,
			 const struct crypto_bignum *b,
			 const struct crypto_bignum *m,
			 struct crypto_bignum *d)
{
	if (TEST_FAIL())
		return -1;

	return mp_mulmod((mp_int *) a, (mp_int *) b, (mp_int *) m,
			 (mp_int *) d) == MP_OKAY ?  0 : -1;
}


int crypto_bignum_sqrmod(const struct crypto_bignum *a,
			 const struct crypto_bignum *b,
			 struct crypto_bignum *c)
{
	if (TEST_FAIL())
		return -1;

	return mp_sqrmod((mp_int *) a, (mp_int *) b,
			 (mp_int *) c) == MP_OKAY ?  0 : -1;
}


int crypto_bignum_rshift(const struct crypto_bignum *a, int n,
			 struct crypto_bignum *r)
{
	if (mp_copy((mp_int *) a, (mp_int *) r) != MP_OKAY)
		return -1;
	mp_rshb((mp_int *) r, n);
	return 0;
}


int crypto_bignum_cmp(const struct crypto_bignum *a,
		      const struct crypto_bignum *b)
{
	return mp_cmp((mp_int *) a, (mp_int *) b);
}


int crypto_bignum_is_zero(const struct crypto_bignum *a)
{
	return mp_iszero((mp_int *) a);
}


int crypto_bignum_is_one(const struct crypto_bignum *a)
{
	return mp_isone((const mp_int *) a);
}

int crypto_bignum_is_odd(const struct crypto_bignum *a)
{
	return mp_isodd((mp_int *) a);
}


int crypto_bignum_legendre(const struct crypto_bignum *a,
			   const struct crypto_bignum *p)
{
	mp_int t;
	int ret;
	int res = -2;

	if (TEST_FAIL())
		return -2;

	if (mp_init(&t) != MP_OKAY)
		return -2;

	/* t = (p-1) / 2 */
	ret = mp_sub_d((mp_int *) p, 1, &t);
	if (ret == MP_OKAY)
		mp_rshb(&t, 1);
	if (ret == MP_OKAY)
		ret = mp_exptmod((mp_int *) a, &t, (mp_int *) p, &t);
	if (ret == MP_OKAY) {
		if (mp_isone(&t))
			res = 1;
		else if (mp_iszero(&t))
			res = 0;
		else
			res = -1;
	}

	mp_clear(&t);
	return res;
}


#ifdef CONFIG_ECC

static int crypto_ec_group_2_id(int group)
{
	switch (group) {
	case 19:
		return ECC_SECP256R1;
	case 20:
		return ECC_SECP384R1;
	case 21:
		return ECC_SECP521R1;
	case 25:
		return ECC_SECP192R1;
	case 26:
		return ECC_SECP224R1;
#ifdef HAVE_ECC_BRAINPOOL
	case 27:
		return ECC_BRAINPOOLP224R1;
	case 28:
		return ECC_BRAINPOOLP256R1;
	case 29:
		return ECC_BRAINPOOLP384R1;
	case 30:
		return ECC_BRAINPOOLP512R1;
#endif /* HAVE_ECC_BRAINPOOL */
	default:
		LOG_WOLF_ERROR_VA("Unsupported curve (id=%d) in EC key", group);
		return ECC_CURVE_INVALID;
	}
}


int ecc_map(ecc_point *, mp_int *, mp_digit);
int ecc_projective_add_point(ecc_point *P, ecc_point *Q, ecc_point *R,
			     mp_int *a, mp_int *modulus, mp_digit mp);

struct crypto_ec {
	ecc_key *key;
#ifdef CONFIG_DPP
	ecc_point *g; /* Only used in DPP for now */
#endif /* CONFIG_DPP */
	mp_int a;
	mp_int prime;
	mp_int order;
	mp_digit mont_b;
	mp_int b;
	int curve_id;
	bool own_key; /* Should we free the `key` */
};


struct crypto_ec * crypto_ec_init(int group)
{
	int built = 0;
	struct crypto_ec *e;
	int curve_id = crypto_ec_group_2_id(group);
	int err;

	if (curve_id == ECC_CURVE_INVALID) {
		LOG_INVALID_PARAMETERS();
		return NULL;
	}

	e = os_zalloc(sizeof(*e));
	if (!e) {
		LOG_WOLF_ERROR_FUNC_NULL(os_zalloc);
		return NULL;
	}

	e->curve_id = curve_id;
	e->own_key = true;
	e->key = ecc_key_init();
	if (!e->key) {
		LOG_WOLF_ERROR_FUNC_NULL(ecc_key_init);
		goto done;
	}

	err = wc_ecc_set_curve(e->key, 0, curve_id);
	if (err != 0) {
		LOG_WOLF_ERROR_FUNC(wc_ecc_set_curve, err);
		goto done;
	}
#ifdef CONFIG_DPP
	e->g = wc_ecc_new_point();
	if (!e->g) {
		LOG_WOLF_ERROR_FUNC_NULL(wc_ecc_new_point);
		goto done;
	}
#ifdef CONFIG_FIPS
	/* Setup generator manually in FIPS mode */
	if (!e->key->dp) {
		LOG_WOLF_ERROR_FUNC_NULL(e->key->dp);
		goto done;
	}
	err = mp_read_radix(e->g->x, e->key->dp->Gx, MP_RADIX_HEX);
	if (err != MP_OKAY) {
		LOG_WOLF_ERROR_FUNC(mp_read_radix, err);
		goto done;
	}
	err = mp_read_radix(e->g->y, e->key->dp->Gy, MP_RADIX_HEX);
	if (err != MP_OKAY) {
		LOG_WOLF_ERROR_FUNC(mp_read_radix, err);
		goto done;
	}
	err = mp_set(e->g->z, 1);
	if (err != MP_OKAY) {
		LOG_WOLF_ERROR_FUNC(mp_set, err);
		goto done;
	}
#else /* CONFIG_FIPS */
	err = wc_ecc_get_generator(e->g, wc_ecc_get_curve_idx(curve_id));
	if (err != MP_OKAY) {
		LOG_WOLF_ERROR_FUNC(wc_ecc_get_generator, err);
		goto done;
	}
#endif /* CONFIG_FIPS */
#endif /* CONFIG_DPP */
	err = mp_init_multi(&e->a, &e->prime, &e->order, &e->b, NULL, NULL);
	if (err != MP_OKAY) {
		LOG_WOLF_ERROR_FUNC(mp_init_multi, err);
		goto done;
	}
	err = mp_read_radix(&e->a, e->key->dp->Af, 16);
	if (err != MP_OKAY) {
		LOG_WOLF_ERROR_FUNC(mp_read_radix, err);
		goto done;
	}
	err = mp_read_radix(&e->b, e->key->dp->Bf, 16);
	if (err != MP_OKAY) {
		LOG_WOLF_ERROR_FUNC(mp_read_radix, err);
		goto done;
	}
	err = mp_read_radix(&e->prime, e->key->dp->prime, 16);
	if (err != MP_OKAY) {
		LOG_WOLF_ERROR_FUNC(mp_read_radix, err);
		goto done;
	}
	err = mp_read_radix(&e->order, e->key->dp->order, 16);
	if (err != MP_OKAY) {
		LOG_WOLF_ERROR_FUNC(mp_read_radix, err);
		goto done;
	}
	err = mp_montgomery_setup(&e->prime, &e->mont_b);
	if (err != MP_OKAY) {
		LOG_WOLF_ERROR_FUNC(mp_montgomery_setup, err);
		goto done;
	}

	built = 1;
done:
	if (!built) {
		crypto_ec_deinit(e);
		e = NULL;
	}
	return e;
}


void crypto_ec_deinit(struct crypto_ec* e)
{
	if (!e)
		return;

	mp_clear(&e->b);
	mp_clear(&e->order);
	mp_clear(&e->prime);
	mp_clear(&e->a);
#ifdef CONFIG_DPP
	wc_ecc_del_point(e->g);
#endif /* CONFIG_DPP */
	if (e->own_key)
		ecc_key_deinit(e->key);
	os_free(e);
}


struct crypto_ec_point * crypto_ec_point_init(struct crypto_ec *e)
{
	if (TEST_FAIL())
		return NULL;
	if (!e)
		return NULL;
	return (struct crypto_ec_point *) wc_ecc_new_point();
}


size_t crypto_ec_prime_len(struct crypto_ec *e)
{
	return (mp_count_bits(&e->prime) + 7) / 8;
}


size_t crypto_ec_prime_len_bits(struct crypto_ec *e)
{
	return mp_count_bits(&e->prime);
}


size_t crypto_ec_order_len(struct crypto_ec *e)
{
	return (mp_count_bits(&e->order) + 7) / 8;
}


const struct crypto_bignum * crypto_ec_get_prime(struct crypto_ec *e)
{
	return (const struct crypto_bignum *) &e->prime;
}


const struct crypto_bignum * crypto_ec_get_order(struct crypto_ec *e)
{
	return (const struct crypto_bignum *) &e->order;
}


const struct crypto_bignum * crypto_ec_get_a(struct crypto_ec *e)
{
	return (const struct crypto_bignum *) &e->a;
}


const struct crypto_bignum * crypto_ec_get_b(struct crypto_ec *e)
{
	return (const struct crypto_bignum *) &e->b;
}


void crypto_ec_point_deinit(struct crypto_ec_point *p, int clear)
{
	ecc_point *point = (ecc_point *) p;

	if (!p)
		return;

	if (clear) {
#ifdef CONFIG_FIPS
		mp_forcezero(point->x);
		mp_forcezero(point->y);
		mp_forcezero(point->z);
#else /* CONFIG_FIPS */
		wc_ecc_forcezero_point(point);
#endif /* CONFIG_FIPS */
	}
	wc_ecc_del_point(point);
}


#ifdef CONFIG_DPP
const struct crypto_ec_point * crypto_ec_get_generator(struct crypto_ec *e)
{
	return (const struct crypto_ec_point *) e->g;
}
#endif /* CONFIG_DPP */


int crypto_ec_point_x(struct crypto_ec *e, const struct crypto_ec_point *p,
		      struct crypto_bignum *x)
{
	return mp_copy(((ecc_point *) p)->x, (mp_int *) x) == MP_OKAY ? 0 : -1;
}


int crypto_ec_point_to_bin(struct crypto_ec *e,
			   const struct crypto_ec_point *point, u8 *x, u8 *y)
{
	ecc_point *p = (ecc_point *) point;
	int len;
	int err;

	if (TEST_FAIL())
		return -1;

	if (!mp_isone(p->z)) {
		err = ecc_map(p, &e->prime, e->mont_b);
		if (err != MP_OKAY) {
			LOG_WOLF_ERROR_FUNC(ecc_map, err);
			return -1;
		}
	}

	len = wc_ecc_get_curve_size_from_id(e->curve_id);
	if (len <= 0) {
		LOG_WOLF_ERROR_FUNC(wc_ecc_get_curve_size_from_id, len);
		LOG_WOLF_ERROR_VA("wc_ecc_get_curve_size_from_id error for curve_id %d", e->curve_id);
		return -1;
	}

	if (x) {
		if (crypto_bignum_to_bin((struct crypto_bignum *)p->x, x,
					 (size_t) len, (size_t) len) <= 0) {
			LOG_WOLF_ERROR_FUNC(crypto_bignum_to_bin, -1);
			return -1;
		}
	}

	if (y) {
		if (crypto_bignum_to_bin((struct crypto_bignum *) p->y, y,
					 (size_t) len, (size_t) len) <= 0) {
			LOG_WOLF_ERROR_FUNC(crypto_bignum_to_bin, -1);
			return -1;
		}
	}

	return 0;
}


struct crypto_ec_point * crypto_ec_point_from_bin(struct crypto_ec *e,
						  const u8 *val)
{
	ecc_point *point = NULL;
	int loaded = 0;

	if (TEST_FAIL())
		return NULL;

	point = wc_ecc_new_point();
	if (!point)
		goto done;

	if (mp_read_unsigned_bin(point->x, val, e->key->dp->size) != MP_OKAY)
		goto done;
	val += e->key->dp->size;
	if (mp_read_unsigned_bin(point->y, val, e->key->dp->size) != MP_OKAY)
		goto done;
	mp_set(point->z, 1);

	loaded = 1;
done:
	if (!loaded) {
		wc_ecc_del_point(point);
		point = NULL;
	}
	return (struct crypto_ec_point *) point;
}


int crypto_ec_point_add(struct crypto_ec *e, const struct crypto_ec_point *a,
			const struct crypto_ec_point *b,
			struct crypto_ec_point *c)
{
	mp_int mu;
	ecc_point *ta = NULL, *tb = NULL;
	ecc_point *pa = (ecc_point *) a, *pb = (ecc_point *) b;
	mp_int *modulus = &e->prime;
	int ret;

	if (TEST_FAIL())
		return -1;

	ret = mp_init(&mu);
	if (ret != MP_OKAY)
		return -1;

	ret = mp_montgomery_calc_normalization(&mu, modulus);
	if (ret != MP_OKAY) {
		mp_clear(&mu);
		return -1;
	}

	if (!mp_isone(&mu)) {
		ta = wc_ecc_new_point();
		if (!ta) {
			mp_clear(&mu);
			return -1;
		}
		tb = wc_ecc_new_point();
		if (!tb) {
			wc_ecc_del_point(ta);
			mp_clear(&mu);
			return -1;
		}

		if (mp_mulmod(pa->x, &mu, modulus, ta->x) != MP_OKAY ||
		    mp_mulmod(pa->y, &mu, modulus, ta->y) != MP_OKAY ||
		    mp_mulmod(pa->z, &mu, modulus, ta->z) != MP_OKAY ||
		    mp_mulmod(pb->x, &mu, modulus, tb->x) != MP_OKAY ||
		    mp_mulmod(pb->y, &mu, modulus, tb->y) != MP_OKAY ||
		    mp_mulmod(pb->z, &mu, modulus, tb->z) != MP_OKAY) {
			ret = -1;
			goto end;
		}
		pa = ta;
		pb = tb;
	}

	ret = ecc_projective_add_point(pa, pb, (ecc_point *) c, &e->a,
				       &e->prime, e->mont_b);
	if (ret != 0) {
		ret = -1;
		goto end;
	}

	if (ecc_map((ecc_point *) c, &e->prime, e->mont_b) != MP_OKAY)
		ret = -1;
	else
		ret = 0;
end:
	wc_ecc_del_point(tb);
	wc_ecc_del_point(ta);
	mp_clear(&mu);
	return ret;
}


int crypto_ec_point_mul(struct crypto_ec *e, const struct crypto_ec_point *p,
			const struct crypto_bignum *b,
			struct crypto_ec_point *res)
{
	int ret;

	if (TEST_FAIL())
		return -1;

	ret = wc_ecc_mulmod((mp_int *) b, (ecc_point *) p, (ecc_point *) res,
			    &e->a, &e->prime, 1);
	return ret == 0 ? 0 : -1;
}


int crypto_ec_point_invert(struct crypto_ec *e, struct crypto_ec_point *p)
{
	ecc_point *point = (ecc_point *) p;

	if (TEST_FAIL())
		return -1;

	if (mp_sub(&e->prime, point->y, point->y) != MP_OKAY)
		return -1;

	return 0;
}


struct crypto_bignum *
crypto_ec_point_compute_y_sqr(struct crypto_ec *e,
			      const struct crypto_bignum *x)
{
	mp_int *y2;

	if (TEST_FAIL())
		return NULL;

	/* y^2 = x^3 + ax + b = (x^2 + a)x + b */
	y2 = (mp_int *) crypto_bignum_init();
	if (!y2 ||
	    mp_sqrmod((mp_int *) x, &e->prime, y2) != 0 ||
	    mp_addmod(y2, &e->a, &e->prime, y2) != 0 ||
	    mp_mulmod((mp_int *) x, y2, &e->prime, y2) != 0 ||
	    mp_addmod(y2, &e->b, &e->prime, y2) != 0) {
		mp_clear(y2);
		os_free(y2);
		y2 = NULL;
	}

	return (struct crypto_bignum *) y2;
}


int crypto_ec_point_is_at_infinity(struct crypto_ec *e,
				   const struct crypto_ec_point *p)
{
	return wc_ecc_point_is_at_infinity((ecc_point *) p);
}


int crypto_ec_point_is_on_curve(struct crypto_ec *e,
				const struct crypto_ec_point *p)
{
	return wc_ecc_is_point((ecc_point *) p, &e->a, &e->b, &e->prime) ==
		MP_OKAY;
}


int crypto_ec_point_cmp(const struct crypto_ec *e,
			const struct crypto_ec_point *a,
			const struct crypto_ec_point *b)
{
	return wc_ecc_cmp_point((ecc_point *) a, (ecc_point *) b);
}

struct crypto_ec_key {
	ecc_key *eckey;
	WC_RNG *rng; /* Needs to be initialized before use.
		      * *NOT* initialized in crypto_ec_key_init */
};


struct crypto_ecdh {
	struct crypto_ec *ec;
	WC_RNG *rng;
};

static struct crypto_ecdh * _crypto_ecdh_init(int group)
{
	struct crypto_ecdh *ecdh = NULL;
#if defined(ECC_TIMING_RESISTANT) && !defined(WOLFSSL_OLD_FIPS)
	int ret;
#endif /* ECC_TIMING_RESISTANT && !WOLFSSL_OLD_FIPS */

	ecdh = os_zalloc(sizeof(*ecdh));
	if (!ecdh) {
		LOG_WOLF_ERROR_FUNC_NULL(os_zalloc);
		return NULL;
	}

	ecdh->rng = wc_rng_init();
	if (!ecdh->rng) {
		LOG_WOLF_ERROR_FUNC_NULL(wc_rng_init);
		goto fail;
	}

	ecdh->ec = crypto_ec_init(group);
	if (!ecdh->ec) {
		LOG_WOLF_ERROR_FUNC_NULL(crypto_ec_init);
		goto fail;
	}

#if defined(ECC_TIMING_RESISTANT) && !defined(WOLFSSL_OLD_FIPS)
	ret = wc_ecc_set_rng(ecdh->ec->key, ecdh->rng);
	if (ret != 0) {
		LOG_WOLF_ERROR_FUNC(wc_ecc_set_rng, ret);
		goto fail;
	}
#endif /* ECC_TIMING_RESISTANT && !WOLFSSL_OLD_FIPS */

	return ecdh;
fail:
	crypto_ecdh_deinit(ecdh);
	return NULL;
}


struct crypto_ecdh * crypto_ecdh_init(int group)
{
	struct crypto_ecdh *ret = NULL;
	int err;

	ret = _crypto_ecdh_init(group);

	if (!ret) {
		LOG_WOLF_ERROR_FUNC_NULL(_crypto_ecdh_init);
		return NULL;
	}

	err = wc_ecc_make_key_ex(ret->rng, 0, ret->ec->key,
				 crypto_ec_group_2_id(group));
	if (err != MP_OKAY) {
		LOG_WOLF_ERROR_FUNC(wc_ecc_make_key_ex, err);
		crypto_ecdh_deinit(ret);
		ret = NULL;
	}

	return ret;
}


struct crypto_ecdh * crypto_ecdh_init2(int group, struct crypto_ec_key *own_key)
{
	struct crypto_ecdh *ret = NULL;

	if (!own_key || crypto_ec_key_group(own_key) != group) {
		LOG_INVALID_PARAMETERS();
		return NULL;
	}

	ret = _crypto_ecdh_init(group);
	if (ret) {
		/* Already init'ed to the right group. Enough to substitute the
		 * key. */
		ecc_key_deinit(ret->ec->key);
		ret->ec->key = own_key->eckey;
		ret->ec->own_key = false;
#if defined(ECC_TIMING_RESISTANT) && !defined(WOLFSSL_OLD_FIPS)
		if (!ret->ec->key->rng) {
			int err = wc_ecc_set_rng(ret->ec->key, ret->rng);

			if (err != 0)
				LOG_WOLF_ERROR_FUNC(wc_ecc_set_rng, err);
		}
#endif /* ECC_TIMING_RESISTANT && !CONFIG_FIPS */
	}

	return ret;
}


void crypto_ecdh_deinit(struct crypto_ecdh *ecdh)
{
	if (ecdh) {
#if defined(ECC_TIMING_RESISTANT) && !defined(WOLFSSL_OLD_FIPS)
		/* Disassociate the rng */
		if (ecdh->ec && ecdh->ec->key &&
		    ecdh->ec->key->rng == ecdh->rng)
			(void) wc_ecc_set_rng(ecdh->ec->key, NULL);
#endif /* ECC_TIMING_RESISTANT && !WOLFSSL_OLD_FIPS */
		crypto_ec_deinit(ecdh->ec);
		wc_rng_deinit(ecdh->rng);
		os_free(ecdh);
	}
}


struct wpabuf * crypto_ecdh_get_pubkey(struct crypto_ecdh *ecdh, int inc_y)
{
	struct wpabuf *buf = NULL;
	int ret;
	int len = ecdh->ec->key->dp->size;

	buf = wpabuf_alloc(inc_y ? 2 * len : len);
	if (!buf)
		goto fail;

	ret = crypto_bignum_to_bin((struct crypto_bignum *)
				   ecdh->ec->key->pubkey.x, wpabuf_put(buf, len),
				   len, len);
	if (ret < 0)
		goto fail;
	if (inc_y) {
		ret = crypto_bignum_to_bin((struct crypto_bignum *)
					   ecdh->ec->key->pubkey.y,
					   wpabuf_put(buf, len), len, len);
		if (ret < 0)
			goto fail;
	}

done:
	return buf;
fail:
	wpabuf_free(buf);
	buf = NULL;
	goto done;
}


struct wpabuf * crypto_ecdh_set_peerkey(struct crypto_ecdh *ecdh, int inc_y,
					const u8 *key, size_t len)
{
	int ret;
	struct wpabuf *pubkey = NULL;
	struct wpabuf *secret = NULL;
	word32 key_len = ecdh->ec->key->dp->size;
	ecc_point *point = NULL;
	size_t need_key_len = inc_y ? 2 * key_len : key_len;

	if (len < need_key_len) {
		LOG_WOLF_ERROR("key len too small");
		goto fail;
	}
	pubkey = wpabuf_alloc(1 + 2 * key_len);
	if (!pubkey) {
		LOG_WOLF_ERROR_FUNC_NULL(wpabuf_alloc);
		goto fail;
	}
	wpabuf_put_u8(pubkey, inc_y ? ECC_POINT_UNCOMP : ECC_POINT_COMP_EVEN);
	wpabuf_put_data(pubkey, key, need_key_len);

	point = wc_ecc_new_point();
	if (!point) {
		LOG_WOLF_ERROR_FUNC_NULL(wc_ecc_new_point);
		goto fail;
	}

	ret = wc_ecc_import_point_der(wpabuf_mhead(pubkey), 1 + 2 * key_len,
				      ecdh->ec->key->idx, point);
	if (ret != MP_OKAY) {
		LOG_WOLF_ERROR_FUNC(wc_ecc_import_point_der, ret);
		goto fail;
	}

	secret = wpabuf_alloc(key_len);
	if (!secret) {
		LOG_WOLF_ERROR_FUNC_NULL(wpabuf_alloc);
		goto fail;
	}

	ret = wc_ecc_shared_secret_ex(ecdh->ec->key, point,
				      wpabuf_put(secret, key_len), &key_len);
	if (ret != MP_OKAY) {
		LOG_WOLF_ERROR_FUNC(wc_ecc_shared_secret_ex, ret);
		goto fail;
	}

done:
	wc_ecc_del_point(point);
	wpabuf_free(pubkey);
	return secret;
fail:
	wpabuf_free(secret);
	secret = NULL;
	goto done;
}


size_t crypto_ecdh_prime_len(struct crypto_ecdh *ecdh)
{
	return crypto_ec_prime_len(ecdh->ec);
}

static struct crypto_ec_key * crypto_ec_key_init(void)
{
	struct crypto_ec_key *key;

	key = os_zalloc(sizeof(struct crypto_ec_key));
	if (key) {
		key->eckey = ecc_key_init();
		/* Omit key->rng initialization because it seeds itself and thus
		 * consumes entropy that may never be used. Lazy initialize when
		 * necessary. */
		if (!key->eckey) {
			LOG_WOLF_ERROR_FUNC_NULL(ecc_key_init);
			crypto_ec_key_deinit(key);
			key = NULL;
		}
	}
	return key;
}


void crypto_ec_key_deinit(struct crypto_ec_key *key)
{
	if (key) {
		ecc_key_deinit(key->eckey);
		wc_rng_deinit(key->rng);
		os_free(key);
	}
}


static WC_RNG * crypto_ec_key_init_rng(struct crypto_ec_key *key)
{
	if (!key->rng) {
		/* Lazy init key->rng */
		key->rng = wc_rng_init();
		if (!key->rng)
			LOG_WOLF_ERROR_FUNC_NULL(wc_rng_init);
	}
	return key->rng;
}


struct crypto_ec_key * crypto_ec_key_parse_priv(const u8 *der, size_t der_len)
{
	struct crypto_ec_key *ret;
	word32 idx = 0;
	int err;

	ret = crypto_ec_key_init();
	if (!ret) {
		LOG_WOLF_ERROR_FUNC_NULL(crypto_ec_key_init);
		goto fail;
	}

	err = wc_EccPrivateKeyDecode(der, &idx, ret->eckey, (word32) der_len);
	if (err != 0) {
		LOG_WOLF_ERROR_FUNC(wc_EccPrivateKeyDecode, err);
		goto fail;
	}

	return ret;
fail:
	if (ret)
		crypto_ec_key_deinit(ret);
	return NULL;
}


int crypto_ec_key_group(struct crypto_ec_key *key)
{

	if (!key || !key->eckey || !key->eckey->dp) {
		LOG_INVALID_PARAMETERS();
		return -1;
	}

	switch (key->eckey->dp->id) {
	case ECC_SECP256R1:
		return 19;
	case ECC_SECP384R1:
		return 20;
	case ECC_SECP521R1:
		return 21;
	case ECC_SECP192R1:
		return 25;
	case ECC_SECP224R1:
		return 26;
#ifdef HAVE_ECC_BRAINPOOL
	case ECC_BRAINPOOLP224R1:
		return 27;
	case ECC_BRAINPOOLP256R1:
		return 28;
	case ECC_BRAINPOOLP384R1:
		return 29;
	case ECC_BRAINPOOLP512R1:
		return 30;
#endif /* HAVE_ECC_BRAINPOOL */
	}

	LOG_WOLF_ERROR_VA("Unsupported curve (id=%d) in EC key",
			  key->eckey->dp->id);
	return -1;
}


static int crypto_ec_key_gen_public_key(struct crypto_ec_key *key)
{
	int err;

#ifdef WOLFSSL_OLD_FIPS
	err = wc_ecc_make_pub(key->eckey, NULL);
#else /* WOLFSSL_OLD_FIPS */
	/* Have wolfSSL generate the public key to make it available for output
	 */
	if (!crypto_ec_key_init_rng(key)) {
		LOG_WOLF_ERROR_FUNC_NULL(crypto_ec_key_init_rng);
		return -1;
	}

	err = wc_ecc_make_pub_ex(key->eckey, NULL, key->rng);
#endif /* WOLFSSL_OLD_FIPS */

	if (err != MP_OKAY) {
		LOG_WOLF_ERROR_FUNC(wc_ecc_make_pub_ex, err);
		return -1;
	}

	return 0;
}


struct wpabuf * crypto_ec_key_get_subject_public_key(struct crypto_ec_key *key)
{
	int der_len;
	struct wpabuf *ret = NULL;
	int err;

	if (!key || !key->eckey) {
		LOG_INVALID_PARAMETERS();
		goto fail;
	}

#ifdef WOLFSSL_OLD_FIPS
	if (key->eckey->type == ECC_PRIVATEKEY_ONLY &&
	    crypto_ec_key_gen_public_key(key) != 0) {
		LOG_WOLF_ERROR_FUNC(crypto_ec_key_gen_public_key, -1);
		goto fail;
	}
#endif /* WOLFSSL_OLD_FIPS */

	der_len = err = wc_EccPublicKeyToDer_ex(key->eckey, NULL, 0, 1, 1);
	if (err == ECC_PRIVATEONLY_E) {
		if (crypto_ec_key_gen_public_key(key) != 0) {
			LOG_WOLF_ERROR_FUNC(crypto_ec_key_gen_public_key, -1);
			goto fail;
		}
		der_len = err = wc_EccPublicKeyToDer_ex(key->eckey, NULL, 0, 1,
							1);
	}
	if (err <= 0) {
		LOG_WOLF_ERROR_FUNC(wc_EccPublicKeyDerSize, err);
		goto fail;
	}

	ret = wpabuf_alloc(der_len);
	if (!ret) {
		LOG_WOLF_ERROR_FUNC_NULL(wpabuf_alloc);
		goto fail;
	}

	err = wc_EccPublicKeyToDer_ex(key->eckey, wpabuf_mhead(ret), der_len, 1,
				      1);
	if (err == ECC_PRIVATEONLY_E) {
		if (crypto_ec_key_gen_public_key(key) != 0) {
			LOG_WOLF_ERROR_FUNC(crypto_ec_key_gen_public_key, -1);
			goto fail;
		}
		err = wc_EccPublicKeyToDer_ex(key->eckey, wpabuf_mhead(ret),
					      der_len, 1, 1);
	}
	if (err <= 0) {
		LOG_WOLF_ERROR_FUNC(wc_EccPublicKeyToDer, err);
		goto fail;
	}
	der_len = err;
	wpabuf_put(ret, der_len);

	return ret;

fail:
	wpabuf_free(ret);
	return NULL;
}


struct crypto_ec_key * crypto_ec_key_parse_pub(const u8 *der, size_t der_len)
{
	word32 idx = 0;
	struct crypto_ec_key *ret = NULL;
	int err;

	ret = crypto_ec_key_init();
	if (!ret) {
		LOG_WOLF_ERROR_FUNC_NULL(crypto_ec_key_init);
		goto fail;
	}

	err = wc_EccPublicKeyDecode(der, &idx, ret->eckey, (word32) der_len);
	if (err != 0) {
		LOG_WOLF_ERROR_FUNC(wc_EccPublicKeyDecode, err);
		goto fail;
	}

	return ret;
fail:
	crypto_ec_key_deinit(ret);
	return NULL;
}


struct wpabuf * crypto_ec_key_sign(struct crypto_ec_key *key, const u8 *data,
				   size_t len)
{
	int der_len;
	int err;
	word32 w32_der_len;
	struct wpabuf *ret = NULL;

	if (!key || !key->eckey || !data || len == 0) {
		LOG_INVALID_PARAMETERS();
		goto fail;
	}

	if (!crypto_ec_key_init_rng(key)) {
		LOG_WOLF_ERROR_FUNC_NULL(crypto_ec_key_init_rng);
		goto fail;
	}

	der_len = wc_ecc_sig_size(key->eckey);
	if (der_len <= 0) {
		LOG_WOLF_ERROR_FUNC(wc_ecc_sig_size, der_len);
		goto fail;
	}

	ret = wpabuf_alloc(der_len);
	if (!ret) {
		LOG_WOLF_ERROR_FUNC_NULL(wpabuf_alloc);
		goto fail;
	}

	w32_der_len = (word32) der_len;
	err = wc_ecc_sign_hash(data, len, wpabuf_mhead(ret), &w32_der_len,
			       key->rng, key->eckey);
	if (err != 0) {
		LOG_WOLF_ERROR_FUNC(wc_ecc_sign_hash, err);
		goto fail;
	}
	wpabuf_put(ret, w32_der_len);

	return ret;
fail:
	wpabuf_free(ret);
	return NULL;
}


int crypto_ec_key_verify_signature(struct crypto_ec_key *key, const u8 *data,
				   size_t len, const u8 *sig, size_t sig_len)
{
	int res = 0;

	if (!key || !key->eckey || !data || len == 0 || !sig || sig_len == 0) {
		LOG_INVALID_PARAMETERS();
		return -1;
	}

	if (wc_ecc_verify_hash(sig, sig_len, data, len, &res, key->eckey) != 0)
	{
		LOG_WOLF_ERROR("wc_ecc_verify_hash failed");
		return -1;
	}

	if (res != 1)
		LOG_WOLF_ERROR("crypto_ec_key_verify_signature failed");

	return res;
}

#endif /* CONFIG_ECC */

#ifdef CONFIG_DPP

struct wpabuf * crypto_ec_key_get_ecprivate_key(struct crypto_ec_key *key,
						bool include_pub)
{
	int len;
	int err;
	struct wpabuf *ret = NULL;

	if (!key || !key->eckey) {
		LOG_INVALID_PARAMETERS();
		return NULL;
	}

#ifdef WOLFSSL_OLD_FIPS
	if (key->eckey->type != ECC_PRIVATEKEY &&
	    key->eckey->type != ECC_PRIVATEKEY_ONLY) {
		LOG_INVALID_PARAMETERS();
		return NULL;
	}
#endif /* WOLFSSL_OLD_FIPS */

	len = err = wc_EccKeyDerSize(key->eckey, include_pub);
	if (err == ECC_PRIVATEONLY_E && include_pub) {
		if (crypto_ec_key_gen_public_key(key) != 0) {
			LOG_WOLF_ERROR_FUNC(crypto_ec_key_gen_public_key, -1);
			return NULL;
		}
		len = err = wc_EccKeyDerSize(key->eckey, include_pub);
	}
	if (err <= 0) {
		/* Exception for BAD_FUNC_ARG because higher levels blindly call
		 * this function to determine if this is a private key or not.
		 * BAD_FUNC_ARG most probably means that key->eckey is a public
		 * key not private. */
		if (err != BAD_FUNC_ARG)
			LOG_WOLF_ERROR_FUNC(wc_EccKeyDerSize, err);
		return NULL;
	}

	ret = wpabuf_alloc(len);
	if (!ret) {
		LOG_WOLF_ERROR_FUNC_NULL(wpabuf_alloc);
		return NULL;
	}

	if (include_pub)
		err = wc_EccKeyToDer(key->eckey, wpabuf_put(ret, len), len);
	else
		err = wc_EccPrivateKeyToDer(key->eckey, wpabuf_put(ret, len),
					    len);

	if (err != len) {
		LOG_WOLF_ERROR_VA("%s failed with err: %d", include_pub ?
				  "wc_EccKeyToDer" : "wc_EccPrivateKeyToDer",
				  err);
		wpabuf_free(ret);
		ret = NULL;
	}

	return ret;
}


struct wpabuf * crypto_ec_key_get_pubkey_point(struct crypto_ec_key *key,
					       int prefix)
{
	int err;
	word32 len = 0;
	struct wpabuf *ret = NULL;

	if (!key || !key->eckey) {
		LOG_INVALID_PARAMETERS();
		return NULL;
	}

	err = wc_ecc_export_x963(key->eckey, NULL, &len);
	if (err != LENGTH_ONLY_E) {
		LOG_WOLF_ERROR_FUNC(wc_ecc_export_x963, err);
		goto fail;
	}

	ret = wpabuf_alloc(len);
	if (!ret) {
		LOG_WOLF_ERROR_FUNC_NULL(wpabuf_alloc);
		goto fail;
	}

	err = wc_ecc_export_x963(key->eckey, wpabuf_mhead(ret), &len);
	if (err == ECC_PRIVATEONLY_E) {
		if (crypto_ec_key_gen_public_key(key) != 0) {
			LOG_WOLF_ERROR_FUNC(crypto_ec_key_gen_public_key, -1);
			goto fail;
		}
		err = wc_ecc_export_x963(key->eckey, wpabuf_mhead(ret), &len);
	}
	if (err != MP_OKAY) {
		LOG_WOLF_ERROR_FUNC(wc_ecc_export_x963, err);
		goto fail;
	}

	if (!prefix)
		os_memmove(wpabuf_mhead(ret), wpabuf_mhead_u8(ret) + 1,
			   (size_t)--len);
	wpabuf_put(ret, len);

	return ret;

fail:
	wpabuf_free(ret);
	return NULL;
}


struct crypto_ec_key * crypto_ec_key_set_pub(int group, const u8 *x,
					     const u8 *y, size_t len)
{
	struct crypto_ec_key *ret = NULL;
	int curve_id = crypto_ec_group_2_id(group);
	int err;

	if (!x || !y || len == 0 || curve_id == ECC_CURVE_INVALID ||
	    wc_ecc_get_curve_size_from_id(curve_id) != (int) len) {
		LOG_INVALID_PARAMETERS();
		return NULL;
	}

	ret = crypto_ec_key_init();
	if (!ret) {
		LOG_WOLF_ERROR_FUNC_NULL(crypto_ec_key_init);
		return NULL;
	}

	/* Cast necessary for FIPS API */
	err = wc_ecc_import_unsigned(ret->eckey, (u8 *) x, (u8 *) y, NULL,
				     curve_id);
	if (err != MP_OKAY) {
		LOG_WOLF_ERROR_FUNC(wc_ecc_import_unsigned, err);
		crypto_ec_key_deinit(ret);
		return NULL;
	}

	return ret;
}


int crypto_ec_key_cmp(struct crypto_ec_key *key1, struct crypto_ec_key *key2)
{
	int ret;
	struct wpabuf *key1_buf = crypto_ec_key_get_subject_public_key(key1);
	struct wpabuf *key2_buf = crypto_ec_key_get_subject_public_key(key2);

	if ((key1 && !key1_buf) || (key2 && !key2_buf)) {
		LOG_WOLF_ERROR("crypto_ec_key_get_subject_public_key failed");
		return -1;
	}

	ret = wpabuf_cmp(key1_buf, key2_buf);
	if (ret != 0)
		ret = -1; /* Default to -1 for different keys */

	wpabuf_clear_free(key1_buf);
	wpabuf_clear_free(key2_buf);
	return ret;
}


/* wolfSSL doesn't have a pretty print function for keys so just print out the
 * PEM of the private key. */
void crypto_ec_key_debug_print(const struct crypto_ec_key *key,
			       const char *title)
{
	struct wpabuf * key_buf;
	struct wpabuf * out = NULL;
	int err;
	int pem_len;

	if (!key || !key->eckey) {
		LOG_INVALID_PARAMETERS();
		return;
	}

	if (key->eckey->type == ECC_PUBLICKEY)
		key_buf = crypto_ec_key_get_subject_public_key(
			(struct crypto_ec_key *) key);
	else
		key_buf = crypto_ec_key_get_ecprivate_key(
			(struct crypto_ec_key *) key, 1);

	if (!key_buf) {
		LOG_WOLF_ERROR_VA("%s has returned NULL",
				  key->eckey->type == ECC_PUBLICKEY ?
				  "crypto_ec_key_get_subject_public_key" :
				  "crypto_ec_key_get_ecprivate_key");
		goto fail;
	}

	if (!title)
		title = "";

	err = wc_DerToPem(wpabuf_head(key_buf), wpabuf_len(key_buf), NULL, 0,
			  ECC_TYPE);
	if (err <= 0) {
		LOG_WOLF_ERROR_FUNC(wc_DerToPem, err);
		goto fail;
	}
	pem_len = err;

	out = wpabuf_alloc(pem_len + 1);
	if (!out) {
		LOG_WOLF_ERROR_FUNC_NULL(wc_DerToPem);
		goto fail;
	}

	err = wc_DerToPem(wpabuf_head(key_buf), wpabuf_len(key_buf),
			  wpabuf_mhead(out), pem_len, ECC_TYPE);
	if (err <= 0) {
		LOG_WOLF_ERROR_FUNC(wc_DerToPem, err);
		goto fail;
	}

	wpabuf_mhead_u8(out)[err] = '\0';
	wpabuf_put(out, err + 1);
	wpa_printf(MSG_DEBUG, "%s:\n%s", title,
		   (const char *) wpabuf_head(out));

fail:
	wpabuf_clear_free(key_buf);
	wpabuf_clear_free(out);
}


void crypto_ec_point_debug_print(const struct crypto_ec *e,
				 const struct crypto_ec_point *p,
				 const char *title)
{
	u8 x[ECC_MAXSIZE];
	u8 y[ECC_MAXSIZE];
	int coord_size;
	int err;

	if (!p || !e) {
		LOG_INVALID_PARAMETERS();
		return;
	}

	coord_size = e->key->dp->size;

	if (!title)
		title = "";

	err = crypto_ec_point_to_bin((struct crypto_ec *)e, p, x, y);
	if (err != 0) {
		LOG_WOLF_ERROR_FUNC(crypto_ec_point_to_bin, err);
		return;
	}

	wpa_hexdump(MSG_DEBUG, title, x, coord_size);
	wpa_hexdump(MSG_DEBUG, title, y, coord_size);
}


struct crypto_ec_key * crypto_ec_key_gen(int group)
{
	int curve_id = crypto_ec_group_2_id(group);
	int err;
	struct crypto_ec_key * ret = NULL;

	if (curve_id == ECC_CURVE_INVALID) {
		LOG_INVALID_PARAMETERS();
		return NULL;
	}

	ret = crypto_ec_key_init();
	if (!ret) {
		LOG_WOLF_ERROR_FUNC_NULL(crypto_ec_key_init);
		return NULL;
	}

	if (!crypto_ec_key_init_rng(ret)) {
		LOG_WOLF_ERROR_FUNC_NULL(crypto_ec_key_init_rng);
		goto fail;
	}

	err = wc_ecc_make_key_ex(ret->rng, 0, ret->eckey, curve_id);
	if (err != MP_OKAY) {
		LOG_WOLF_ERROR_FUNC(wc_ecc_make_key_ex, err);
		goto fail;
	}

	return ret;
fail:
	crypto_ec_key_deinit(ret);
	return NULL;
}


int crypto_ec_key_verify_signature_r_s(struct crypto_ec_key *key,
				       const u8 *data, size_t len,
				       const u8 *r, size_t r_len,
				       const u8 *s, size_t s_len)
{
	int err;
	u8 sig[ECC_MAX_SIG_SIZE];
	word32 sig_len = ECC_MAX_SIG_SIZE;

	if (!key || !key->eckey || !data || !len || !r || !r_len ||
	    !s || !s_len) {
		LOG_INVALID_PARAMETERS();
		return -1;
	}

	err = wc_ecc_rs_raw_to_sig(r, r_len, s, s_len, sig, &sig_len);
	if (err != MP_OKAY) {
		LOG_WOLF_ERROR_FUNC(wc_ecc_rs_raw_to_sig, err);
		return -1;
	}

	return crypto_ec_key_verify_signature(key, data, len, sig, sig_len);
}


struct crypto_ec_point * crypto_ec_key_get_public_key(struct crypto_ec_key *key)
{
	ecc_point *point = NULL;
	int err;
	u8 *der = NULL;
	word32 der_len = 0;

	if (!key || !key->eckey || !key->eckey->dp) {
		LOG_INVALID_PARAMETERS();
		goto fail;
	}

	err = wc_ecc_export_x963(key->eckey, NULL, &der_len);
	if (err != LENGTH_ONLY_E) {
		LOG_WOLF_ERROR_FUNC(wc_ecc_export_x963, err);
		goto fail;
	}

	der = os_malloc(der_len);
	if (!der) {
		LOG_WOLF_ERROR_FUNC_NULL(os_malloc);
		goto fail;
	}

	err = wc_ecc_export_x963(key->eckey, der, &der_len);
	if (err == ECC_PRIVATEONLY_E) {
		if (crypto_ec_key_gen_public_key(key) != 0) {
			LOG_WOLF_ERROR_FUNC(crypto_ec_key_gen_public_key, -1);
			goto fail;
		}
		err = wc_ecc_export_x963(key->eckey, der, &der_len);
	}
	if (err != MP_OKAY) {
		LOG_WOLF_ERROR_FUNC(wc_ecc_export_x963, err);
		goto fail;
	}

	point = wc_ecc_new_point();
	if (!point) {
		LOG_WOLF_ERROR_FUNC_NULL(wc_ecc_new_point);
		goto fail;
	}

	err = wc_ecc_import_point_der(der, der_len, key->eckey->idx, point);
	if (err != MP_OKAY) {
		LOG_WOLF_ERROR_FUNC(wc_ecc_import_point_der, err);
		goto fail;
	}

	os_free(der);
	return (struct crypto_ec_point *) point;

fail:
	os_free(der);
	if (point)
		wc_ecc_del_point(point);
	return NULL;
}


struct crypto_bignum * crypto_ec_key_get_private_key(struct crypto_ec_key *key)
{
	u8 priv[ECC_MAXSIZE];
	word32 priv_len = ECC_MAXSIZE;
#ifdef WOLFSSL_OLD_FIPS
	/* Needed to be compliant with the old API */
	u8 qx[ECC_MAXSIZE];
	word32 qx_len = ECC_MAXSIZE;
	u8 qy[ECC_MAXSIZE];
	word32 qy_len = ECC_MAXSIZE;
#endif /* WOLFSSL_OLD_FIPS */
	struct crypto_bignum *ret = NULL;
	int err;

	if (!key || !key->eckey) {
		LOG_INVALID_PARAMETERS();
		return NULL;
	}

#ifndef WOLFSSL_OLD_FIPS
	err = wc_ecc_export_private_raw(key->eckey, NULL, NULL, NULL, NULL,
					priv, &priv_len);
#else /* WOLFSSL_OLD_FIPS */
	err = wc_ecc_export_private_raw(key->eckey, qx, &qx_len, qy, &qy_len,
					priv, &priv_len);
#endif /* WOLFSSL_OLD_FIPS */
	if (err != MP_OKAY) {
		LOG_WOLF_ERROR_FUNC(wc_ecc_export_private_raw, err);
		return NULL;
	}

	ret = crypto_bignum_init_set(priv, priv_len);
	forced_memzero(priv, priv_len);
	return ret;
}


struct wpabuf * crypto_ec_key_sign_r_s(struct crypto_ec_key *key,
				       const u8 *data, size_t len)
{
	int err;
	u8 success = 0;
	mp_int r;
	mp_int s;
	u8 rs_init = 0;
	int sz;
	struct wpabuf * ret = NULL;

	if (!key || !key->eckey || !key->eckey->dp || !data || !len) {
		LOG_INVALID_PARAMETERS();
		return NULL;
	}

	sz = key->eckey->dp->size;

	if (!crypto_ec_key_init_rng(key)) {
		LOG_WOLF_ERROR_FUNC_NULL(crypto_ec_key_init_rng);
		goto fail;
	}

	err = mp_init_multi(&r, &s, NULL, NULL, NULL, NULL);
	if (err != MP_OKAY) {
		LOG_WOLF_ERROR_FUNC(mp_init_multi, err);
		goto fail;
	}
	rs_init = 1;

	err = wc_ecc_sign_hash_ex(data, len, key->rng, key->eckey, &r, &s);
	if (err != MP_OKAY) {
		LOG_WOLF_ERROR_FUNC(wc_ecc_sign_hash_ex, err);
		goto fail;
	}

	if (mp_unsigned_bin_size(&r) > sz || mp_unsigned_bin_size(&s) > sz) {
		LOG_WOLF_ERROR_VA("Unexpected size of r or s (%d %d %d)", sz,
				  mp_unsigned_bin_size(&r),
				  mp_unsigned_bin_size(&s));
		goto fail;
	}

	ret = wpabuf_alloc(2 * sz);
	if (!ret) {
		LOG_WOLF_ERROR_FUNC_NULL(wpabuf_alloc);
		goto fail;
	}

	err = mp_to_unsigned_bin_len(&r, wpabuf_put(ret, sz), sz);
	if (err == MP_OKAY)
		err = mp_to_unsigned_bin_len(&s, wpabuf_put(ret, sz), sz);
	if (err != MP_OKAY) {
		LOG_WOLF_ERROR_FUNC(wc_ecc_sign_hash_ex, err);
		goto fail;
	}

	success = 1;
fail:
	if (rs_init) {
		mp_free(&r);
		mp_free(&s);
	}
	if (!success) {
		wpabuf_free(ret);
		ret = NULL;
	}

	return ret;
}


struct crypto_ec_key *
crypto_ec_key_set_pub_point(struct crypto_ec *e,
			    const struct crypto_ec_point *pub)
{
	struct crypto_ec_key  *ret = NULL;
	int err;
	byte *buf = NULL;
	word32 buf_len = 0;

	if (!e || !pub) {
		LOG_INVALID_PARAMETERS();
		return NULL;
	}

	/* Export to DER to not mess with wolfSSL internals */
	err = wc_ecc_export_point_der(wc_ecc_get_curve_idx(e->curve_id),
				      (ecc_point *) pub, NULL, &buf_len);
	if (err != LENGTH_ONLY_E || !buf_len) {
		LOG_WOLF_ERROR_FUNC(wc_ecc_export_point_der, err);
		goto fail;
	}

	buf = os_malloc(buf_len);
	if (!buf) {
		LOG_WOLF_ERROR_FUNC_NULL(os_malloc);
		goto fail;
	}

	err = wc_ecc_export_point_der(wc_ecc_get_curve_idx(e->curve_id),
			(ecc_point *) pub, buf, &buf_len);
	if (err != MP_OKAY) {
		LOG_WOLF_ERROR_FUNC(wc_ecc_export_point_der, err);
		goto fail;
	}

	ret = crypto_ec_key_init();
	if (!ret) {
		LOG_WOLF_ERROR_FUNC_NULL(crypto_ec_key_init);
		goto fail;
	}

	err = wc_ecc_import_x963_ex(buf, buf_len, ret->eckey, e->curve_id);
	if (err != MP_OKAY) {
		LOG_WOLF_ERROR_FUNC(wc_ecc_import_x963_ex, err);
		goto fail;
	}

	os_free(buf);
	return ret;

fail:
	os_free(buf);
	crypto_ec_key_deinit(ret);
	return NULL;
}


struct wpabuf * crypto_pkcs7_get_certificates(const struct wpabuf *pkcs7)
{
	PKCS7 *p7 = NULL;
	struct wpabuf *ret = NULL;
	int err = 0;
	int total_sz = 0;
	int i;

	if (!pkcs7) {
		LOG_INVALID_PARAMETERS();
		return NULL;
	}

	p7 = wc_PKCS7_New(NULL, INVALID_DEVID);
	if (!p7) {
		LOG_WOLF_ERROR_FUNC_NULL(wc_PKCS7_New);
		return NULL;
	}

	err = wc_PKCS7_VerifySignedData(p7, (byte *) wpabuf_head(pkcs7),
					wpabuf_len(pkcs7));
	if (err != 0) {
		LOG_WOLF_ERROR_FUNC(wc_PKCS7_VerifySignedData, err);
		wc_PKCS7_Free(p7);
		goto fail;
	}

	/* Need to access p7 members directly */
	for (i = 0; i < MAX_PKCS7_CERTS; i++) {
		if (p7->certSz[i] == 0)
			continue;
		err = wc_DerToPem(p7->cert[i], p7->certSz[i], NULL, 0,
				  CERT_TYPE);
		if (err > 0) {
			total_sz += err;
		} else {
			LOG_WOLF_ERROR_FUNC(wc_DerToPem, err);
			goto fail;
		}
	}

	if (total_sz == 0) {
		LOG_WOLF_ERROR("No certificates found in PKCS7 input");
		goto fail;
	}

	ret = wpabuf_alloc(total_sz);
	if (!ret) {
		LOG_WOLF_ERROR_FUNC_NULL(wpabuf_alloc);
		goto fail;
	}

	/* Need to access p7 members directly */
	for (i = 0; i < MAX_PKCS7_CERTS; i++) {
		if (p7->certSz[i] == 0)
			continue;
		/* Not using wpabuf_put() here so that wpabuf_overflow() isn't
		 * called in case of a size mismatch. wc_DerToPem() checks if
		 * the output is large enough internally. */
		err = wc_DerToPem(p7->cert[i], p7->certSz[i],
				  wpabuf_mhead_u8(ret) + wpabuf_len(ret),
				  wpabuf_tailroom(ret),
				  CERT_TYPE);
		if (err > 0) {
			wpabuf_put(ret, err);
		} else {
			LOG_WOLF_ERROR_FUNC(wc_DerToPem, err);
			wpabuf_free(ret);
			ret = NULL;
			goto fail;
		}
	}

fail:
	if (p7)
		wc_PKCS7_Free(p7);
	return ret;
}


/* BEGIN Certificate Signing Request (CSR) APIs */

enum cert_type {
	cert_type_none = 0,
	cert_type_decoded_cert,
	cert_type_cert,
};

struct crypto_csr {
	union {
		/* For parsed csr should be read-only for higher levels */
		DecodedCert dc;
		Cert c; /* For generating a csr */
	} req;
	enum cert_type type;
	struct crypto_ec_key *pubkey;
};


/* Helper function to make sure that the correct type is initialized */
static void crypto_csr_init_type(struct crypto_csr *csr, enum cert_type type,
				 const byte *source, word32 in_sz)
{
	int err;

	if (csr->type == type)
		return; /* Already correct type */

	switch (csr->type) {
	case cert_type_decoded_cert:
		wc_FreeDecodedCert(&csr->req.dc);
		break;
	case cert_type_cert:
#ifdef WOLFSSL_CERT_GEN_CACHE
		wc_SetCert_Free(&csr->req.c);
#endif /* WOLFSSL_CERT_GEN_CACHE */
		break;
	case cert_type_none:
		break;
	}

	switch (type) {
	case cert_type_decoded_cert:
		wc_InitDecodedCert(&csr->req.dc, source, in_sz, NULL);
		break;
	case cert_type_cert:
		err = wc_InitCert(&csr->req.c);
		if (err != 0)
			LOG_WOLF_ERROR_FUNC(wc_InitCert, err);
		break;
	case cert_type_none:
		break;
	}

	csr->type = type;
}


struct crypto_csr * crypto_csr_init(void)
{
	struct crypto_csr *ret = os_malloc(sizeof(struct crypto_csr));

	if (!ret) {
		LOG_WOLF_ERROR_FUNC_NULL(os_malloc);
		return NULL;
	}

	ret->type = cert_type_none;
	crypto_csr_init_type(ret, cert_type_cert, NULL, 0);
	ret->pubkey = NULL;

	return ret;
}


void crypto_csr_deinit(struct crypto_csr *csr)
{
	if (csr) {
		crypto_csr_init_type(csr, cert_type_none, NULL, 0);
		crypto_ec_key_deinit(csr->pubkey);
		os_free(csr);
	}
}


int crypto_csr_set_ec_public_key(struct crypto_csr *csr,
				 struct crypto_ec_key *key)
{
	struct wpabuf *der = NULL;

	if (!csr || !key || !key->eckey) {
		LOG_INVALID_PARAMETERS();
		return -1;
	}

	if (csr->pubkey) {
		crypto_ec_key_deinit(csr->pubkey);
		csr->pubkey = NULL;
	}

	/* Create copy of key to mitigate use-after-free errors */
	der = crypto_ec_key_get_subject_public_key(key);
	if (!der) {
		LOG_WOLF_ERROR_FUNC_NULL(crypto_ec_key_get_subject_public_key);
		return -1;
	}

	csr->pubkey = crypto_ec_key_parse_pub(wpabuf_head(der),
					      wpabuf_len(der));
	wpabuf_free(der);
	if (!csr->pubkey) {
		LOG_WOLF_ERROR_FUNC_NULL(crypto_ec_key_parse_pub);
		return -1;
	}

	return 0;
}


int crypto_csr_set_name(struct crypto_csr *csr, enum crypto_csr_name type,
			const char *name)
{
	int name_len;
	char *dest;

	if (!csr || !name) {
		LOG_INVALID_PARAMETERS();
		return -1;
	}

	if (csr->type != cert_type_cert) {
		LOG_WOLF_ERROR_VA("csr is incorrect type (%d)", csr->type);
		return -1;
	}

	name_len = os_strlen(name);
	if (name_len >= CTC_NAME_SIZE) {
		LOG_WOLF_ERROR("name input too long");
		return -1;
	}

	switch (type) {
	case CSR_NAME_CN:
		dest = csr->req.c.subject.commonName;
		break;
	case CSR_NAME_SN:
		dest = csr->req.c.subject.sur;
		break;
	case CSR_NAME_C:
		dest = csr->req.c.subject.country;
		break;
	case CSR_NAME_O:
		dest = csr->req.c.subject.org;
		break;
	case CSR_NAME_OU:
		dest = csr->req.c.subject.unit;
		break;
	default:
		LOG_INVALID_PARAMETERS();
		return -1;
	}

	os_memcpy(dest, name, name_len);
	dest[name_len] = '\0';

	return 0;
}


int crypto_csr_set_attribute(struct crypto_csr *csr, enum crypto_csr_attr attr,
			     int attr_type, const u8 *value, size_t len)
{
	if (!csr || attr_type != ASN1_TAG_UTF8STRING || !value ||
	    len >= CTC_NAME_SIZE) {
		LOG_INVALID_PARAMETERS();
		return -1;
	}

	if (csr->type != cert_type_cert) {
		LOG_WOLF_ERROR_VA("csr is incorrect type (%d)", csr->type);
		return -1;
	}

	switch (attr) {
	case CSR_ATTR_CHALLENGE_PASSWORD:
		os_memcpy(csr->req.c.challengePw, value, len);
		csr->req.c.challengePw[len] = '\0';
		break;
	default:
		return -1;
	}

	return 0;
}


const u8 * crypto_csr_get_attribute(struct crypto_csr *csr,
				    enum crypto_csr_attr attr,
				    size_t *len, int *type)
{
	if (!csr || !len || !type) {
		LOG_INVALID_PARAMETERS();
		return NULL;;
	}

	switch (attr) {
	case CSR_ATTR_CHALLENGE_PASSWORD:
		switch (csr->type) {
		case cert_type_decoded_cert:
			*type = ASN1_TAG_UTF8STRING;
			*len = csr->req.dc.cPwdLen;
			return (const u8 *) csr->req.dc.cPwd;
		case cert_type_cert:
			*type = ASN1_TAG_UTF8STRING;
			*len = os_strlen(csr->req.c.challengePw);
			return (const u8 *) csr->req.c.challengePw;
		case cert_type_none:
			return NULL;
		}
		break;
	}
	return NULL;
}


struct wpabuf * crypto_csr_sign(struct crypto_csr *csr,
				struct crypto_ec_key *key,
				enum crypto_hash_alg algo)
{
	int err;
	int len;
	u8 *buf = NULL;
	int buf_len;
	struct wpabuf *ret = NULL;

	if (!csr || !key || !key->eckey) {
		LOG_INVALID_PARAMETERS();
		return NULL;
	}

	if (csr->type != cert_type_cert) {
		LOG_WOLF_ERROR_VA("csr is incorrect type (%d)", csr->type);
		return NULL;
	}

	if (!crypto_ec_key_init_rng(key)) {
		LOG_WOLF_ERROR_FUNC_NULL(crypto_ec_key_init_rng);
		return NULL;
	}

	switch (algo) {
	case CRYPTO_HASH_ALG_SHA256:
		csr->req.c.sigType = CTC_SHA256wECDSA;
		break;
	case CRYPTO_HASH_ALG_SHA384:
		csr->req.c.sigType = CTC_SHA384wECDSA;
		break;
	case CRYPTO_HASH_ALG_SHA512:
		csr->req.c.sigType = CTC_SHA512wECDSA;
		break;
	default:
		LOG_INVALID_PARAMETERS();
		return NULL;
	}

	/* Pass in large value that is guaranteed to be larger than the
	 * necessary buffer */
	err = wc_MakeCertReq(&csr->req.c, NULL, 100000, NULL,
			     csr->pubkey->eckey);
	if (err <= 0) {
		LOG_WOLF_ERROR_FUNC(wc_MakeCertReq, err);
		goto fail;
	}
	len = err;

	buf_len = len + MAX_SEQ_SZ * 2 + MAX_ENCODED_SIG_SZ;
	buf = os_malloc(buf_len);
	if (!buf) {
		LOG_WOLF_ERROR_FUNC_NULL(os_malloc);
		goto fail;
	}

	err = wc_MakeCertReq(&csr->req.c, buf, buf_len, NULL,
			     csr->pubkey->eckey);
	if (err <= 0) {
		LOG_WOLF_ERROR_FUNC(wc_MakeCertReq, err);
		goto fail;
	}
	len = err;

	err = wc_SignCert(len, csr->req.c.sigType, buf, buf_len, NULL,
			  key->eckey, key->rng);
	if (err <= 0) {
		LOG_WOLF_ERROR_FUNC(wc_SignCert, err);
		goto fail;
	}
	len = err;

	ret = wpabuf_alloc_copy(buf, len);
	if (!ret) {
		LOG_WOLF_ERROR_FUNC_NULL(wpabuf_alloc_copy);
		goto fail;
	}

fail:
	os_free(buf);
	return ret;
}


struct crypto_csr * crypto_csr_verify(const struct wpabuf *req)
{
	struct crypto_csr *csr = NULL;
	int err;

	if (!req) {
		LOG_INVALID_PARAMETERS();
		return NULL;
	}

	csr = crypto_csr_init();
	if (!csr) {
		LOG_WOLF_ERROR_FUNC_NULL(crypto_csr_init);
		goto fail;
	}

	crypto_csr_init_type(csr, cert_type_decoded_cert,
			     wpabuf_head(req), wpabuf_len(req));
	err = wc_ParseCert(&csr->req.dc, CERTREQ_TYPE, VERIFY, NULL);
	if (err != 0) {
		LOG_WOLF_ERROR_FUNC(wc_ParseCert, err);
		goto fail;
	}

	return csr;
fail:
	crypto_csr_deinit(csr);
	return NULL;
}

/* END Certificate Signing Request (CSR) APIs */

#endif /* CONFIG_DPP */


void crypto_unload(void)
{
}
