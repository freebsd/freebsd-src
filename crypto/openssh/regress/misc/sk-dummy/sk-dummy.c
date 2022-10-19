/*
 * Copyright (c) 2019 Markus Friedl
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include "includes.h"

#ifdef HAVE_STDINT_H
#include <stdint.h>
#endif
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stddef.h>
#include <stdarg.h>
#ifdef HAVE_SHA2_H
#include <sha2.h>
#endif

#include "crypto_api.h"
#include "sk-api.h"

#if defined(WITH_OPENSSL) && !defined(OPENSSL_HAS_ECC)
# undef WITH_OPENSSL
#endif

#ifdef WITH_OPENSSL
/* We don't use sha2 from OpenSSL and they can conflict with system sha2.h */
#define OPENSSL_NO_SHA
#define USE_LIBC_SHA2	/* NetBSD 9 */
#include <openssl/opensslv.h>
#include <openssl/crypto.h>
#include <openssl/evp.h>
#include <openssl/bn.h>
#include <openssl/ec.h>
#include <openssl/ecdsa.h>
#include <openssl/pem.h>

/* Compatibility with OpenSSH 1.0.x */
#if (OPENSSL_VERSION_NUMBER < 0x10100000L)
#define ECDSA_SIG_get0(sig, pr, ps) \
	do { \
		(*pr) = sig->r; \
		(*ps) = sig->s; \
	} while (0)
#endif
#endif /* WITH_OPENSSL */

/* #define SK_DEBUG 1 */

#if SSH_SK_VERSION_MAJOR != 0x000a0000
# error SK API has changed, sk-dummy.c needs an update
#endif

#ifdef SK_DUMMY_INTEGRATE
# define sk_api_version		ssh_sk_api_version
# define sk_enroll		ssh_sk_enroll
# define sk_sign		ssh_sk_sign
# define sk_load_resident_keys	ssh_sk_load_resident_keys
#endif /* !SK_STANDALONE */

static void skdebug(const char *func, const char *fmt, ...)
    __attribute__((__format__ (printf, 2, 3)));

static void
skdebug(const char *func, const char *fmt, ...)
{
#if defined(SK_DEBUG)
	va_list ap;

	va_start(ap, fmt);
	fprintf(stderr, "sk-dummy %s: ", func);
	vfprintf(stderr, fmt, ap);
	fputc('\n', stderr);
	va_end(ap);
#else
	(void)func; /* XXX */
	(void)fmt; /* XXX */
#endif
}

uint32_t
sk_api_version(void)
{
	return SSH_SK_VERSION_MAJOR;
}

static int
pack_key_ecdsa(struct sk_enroll_response *response)
{
#ifdef OPENSSL_HAS_ECC
	EC_KEY *key = NULL;
	const EC_GROUP *g;
	const EC_POINT *q;
	int ret = -1;
	long privlen;
	BIO *bio = NULL;
	char *privptr;

	response->public_key = NULL;
	response->public_key_len = 0;
	response->key_handle = NULL;
	response->key_handle_len = 0;

	if ((key = EC_KEY_new_by_curve_name(NID_X9_62_prime256v1)) == NULL) {
		skdebug(__func__, "EC_KEY_new_by_curve_name");
		goto out;
	}
	if (EC_KEY_generate_key(key) != 1) {
		skdebug(__func__, "EC_KEY_generate_key");
		goto out;
	}
	EC_KEY_set_asn1_flag(key, OPENSSL_EC_NAMED_CURVE);
	if ((bio = BIO_new(BIO_s_mem())) == NULL ||
	    (g = EC_KEY_get0_group(key)) == NULL ||
	    (q = EC_KEY_get0_public_key(key)) == NULL) {
		skdebug(__func__, "couldn't get key parameters");
		goto out;
	}
	response->public_key_len = EC_POINT_point2oct(g, q,
	    POINT_CONVERSION_UNCOMPRESSED, NULL, 0, NULL);
	if (response->public_key_len == 0 || response->public_key_len > 2048) {
		skdebug(__func__, "bad pubkey length %zu",
		    response->public_key_len);
		goto out;
	}
	if ((response->public_key = malloc(response->public_key_len)) == NULL) {
		skdebug(__func__, "malloc pubkey failed");
		goto out;
	}
	if (EC_POINT_point2oct(g, q, POINT_CONVERSION_UNCOMPRESSED,
	    response->public_key, response->public_key_len, NULL) == 0) {
		skdebug(__func__, "EC_POINT_point2oct failed");
		goto out;
	}
	/* Key handle contains PEM encoded private key */
	if (!PEM_write_bio_ECPrivateKey(bio, key, NULL, NULL, 0, NULL, NULL)) {
		skdebug(__func__, "PEM_write_bio_ECPrivateKey failed");
		goto out;
	}
	if ((privlen = BIO_get_mem_data(bio, &privptr)) <= 0) {
		skdebug(__func__, "BIO_get_mem_data failed");
		goto out;
	}
	if ((response->key_handle = malloc(privlen)) == NULL) {
		skdebug(__func__, "malloc key_handle failed");
		goto out;
	}
	response->key_handle_len = (size_t)privlen;
	memcpy(response->key_handle, privptr, response->key_handle_len);
	/* success */
	ret = 0;
 out:
	if (ret != 0) {
		if (response->public_key != NULL) {
			memset(response->public_key, 0,
			    response->public_key_len);
			free(response->public_key);
			response->public_key = NULL;
		}
		if (response->key_handle != NULL) {
			memset(response->key_handle, 0,
			    response->key_handle_len);
			free(response->key_handle);
			response->key_handle = NULL;
		}
	}
	BIO_free(bio);
	EC_KEY_free(key);
	return ret;
#else
	return -1;
#endif
}

static int
pack_key_ed25519(struct sk_enroll_response *response)
{
	int ret = -1;
	u_char pk[crypto_sign_ed25519_PUBLICKEYBYTES];
	u_char sk[crypto_sign_ed25519_SECRETKEYBYTES];

	response->public_key = NULL;
	response->public_key_len = 0;
	response->key_handle = NULL;
	response->key_handle_len = 0;

	memset(pk, 0, sizeof(pk));
	memset(sk, 0, sizeof(sk));
	crypto_sign_ed25519_keypair(pk, sk);

	response->public_key_len = sizeof(pk);
	if ((response->public_key = malloc(response->public_key_len)) == NULL) {
		skdebug(__func__, "malloc pubkey failed");
		goto out;
	}
	memcpy(response->public_key, pk, sizeof(pk));
	/* Key handle contains sk */
	response->key_handle_len = sizeof(sk);
	if ((response->key_handle = malloc(response->key_handle_len)) == NULL) {
		skdebug(__func__, "malloc key_handle failed");
		goto out;
	}
	memcpy(response->key_handle, sk, sizeof(sk));
	/* success */
	ret = 0;
 out:
	if (ret != 0)
		free(response->public_key);
	return ret;
}

static int
check_options(struct sk_option **options)
{
	size_t i;

	if (options == NULL)
		return 0;
	for (i = 0; options[i] != NULL; i++) {
		skdebug(__func__, "requested unsupported option %s",
		    options[i]->name);
		if (options[i]->required) {
			skdebug(__func__, "unknown required option");
			return -1;
		}
	}
	return 0;
}

int
sk_enroll(uint32_t alg, const uint8_t *challenge, size_t challenge_len,
    const char *application, uint8_t flags, const char *pin,
    struct sk_option **options, struct sk_enroll_response **enroll_response)
{
	struct sk_enroll_response *response = NULL;
	int ret = SSH_SK_ERR_GENERAL;

	(void)flags; /* XXX; unused */

	if (enroll_response == NULL) {
		skdebug(__func__, "enroll_response == NULL");
		goto out;
	}
	*enroll_response = NULL;
	if (check_options(options) != 0)
		goto out; /* error already logged */
	if ((response = calloc(1, sizeof(*response))) == NULL) {
		skdebug(__func__, "calloc response failed");
		goto out;
	}
	response->flags = flags;
	switch(alg) {
	case SSH_SK_ECDSA:
		if (pack_key_ecdsa(response) != 0)
			goto out;
		break;
	case SSH_SK_ED25519:
		if (pack_key_ed25519(response) != 0)
			goto out;
		break;
	default:
		skdebug(__func__, "unsupported key type %d", alg);
		return -1;
	}
	/* Have to return something here */
	if ((response->signature = calloc(1, 1)) == NULL) {
		skdebug(__func__, "calloc signature failed");
		goto out;
	}
	response->signature_len = 0;

	*enroll_response = response;
	response = NULL;
	ret = 0;
 out:
	if (response != NULL) {
		free(response->public_key);
		free(response->key_handle);
		free(response->signature);
		free(response->attestation_cert);
		free(response);
	}
	return ret;
}

static void
dump(const char *preamble, const void *sv, size_t l)
{
#ifdef SK_DEBUG
	const u_char *s = (const u_char *)sv;
	size_t i;

	fprintf(stderr, "%s (len %zu):\n", preamble, l);
	for (i = 0; i < l; i++) {
		if (i % 16 == 0)
			fprintf(stderr, "%04zu: ", i);
		fprintf(stderr, "%02x", s[i]);
		if (i % 16 == 15 || i == l - 1)
			fprintf(stderr, "\n");
	}
#endif
}

static int
sig_ecdsa(const uint8_t *message, size_t message_len,
    const char *application, uint32_t counter, uint8_t flags,
    const uint8_t *key_handle, size_t key_handle_len,
    struct sk_sign_response *response)
{
#ifdef OPENSSL_HAS_ECC
	ECDSA_SIG *sig = NULL;
	const BIGNUM *sig_r, *sig_s;
	int ret = -1;
	BIO *bio = NULL;
	EVP_PKEY *pk = NULL;
	EC_KEY *ec = NULL;
	SHA2_CTX ctx;
	uint8_t	apphash[SHA256_DIGEST_LENGTH];
	uint8_t	sighash[SHA256_DIGEST_LENGTH];
	uint8_t countbuf[4];

	/* Decode EC_KEY from key handle */
	if ((bio = BIO_new(BIO_s_mem())) == NULL ||
	    BIO_write(bio, key_handle, key_handle_len) != (int)key_handle_len) {
		skdebug(__func__, "BIO setup failed");
		goto out;
	}
	if ((pk = PEM_read_bio_PrivateKey(bio, NULL, NULL, "")) == NULL) {
		skdebug(__func__, "PEM_read_bio_PrivateKey failed");
		goto out;
	}
	if (EVP_PKEY_base_id(pk) != EVP_PKEY_EC) {
		skdebug(__func__, "Not an EC key: %d", EVP_PKEY_base_id(pk));
		goto out;
	}
	if ((ec = EVP_PKEY_get1_EC_KEY(pk)) == NULL) {
		skdebug(__func__, "EVP_PKEY_get1_EC_KEY failed");
		goto out;
	}
	/* Expect message to be pre-hashed */
	if (message_len != SHA256_DIGEST_LENGTH) {
		skdebug(__func__, "bad message len %zu", message_len);
		goto out;
	}
	/* Prepare data to be signed */
	dump("message", message, message_len);
	SHA256Init(&ctx);
	SHA256Update(&ctx, (const u_char *)application, strlen(application));
	SHA256Final(apphash, &ctx);
	dump("apphash", apphash, sizeof(apphash));
	countbuf[0] = (counter >> 24) & 0xff;
	countbuf[1] = (counter >> 16) & 0xff;
	countbuf[2] = (counter >> 8) & 0xff;
	countbuf[3] = counter & 0xff;
	dump("countbuf", countbuf, sizeof(countbuf));
	dump("flags", &flags, sizeof(flags));
	SHA256Init(&ctx);
	SHA256Update(&ctx, apphash, sizeof(apphash));
	SHA256Update(&ctx, &flags, sizeof(flags));
	SHA256Update(&ctx, countbuf, sizeof(countbuf));
	SHA256Update(&ctx, message, message_len);
	SHA256Final(sighash, &ctx);
	dump("sighash", sighash, sizeof(sighash));
	/* create and encode signature */
	if ((sig = ECDSA_do_sign(sighash, sizeof(sighash), ec)) == NULL) {
		skdebug(__func__, "ECDSA_do_sign failed");
		goto out;
	}
	ECDSA_SIG_get0(sig, &sig_r, &sig_s);
	response->sig_r_len = BN_num_bytes(sig_r);
	response->sig_s_len = BN_num_bytes(sig_s);
	if ((response->sig_r = calloc(1, response->sig_r_len)) == NULL ||
	    (response->sig_s = calloc(1, response->sig_s_len)) == NULL) {
		skdebug(__func__, "calloc signature failed");
		goto out;
	}
	BN_bn2bin(sig_r, response->sig_r);
	BN_bn2bin(sig_s, response->sig_s);
	ret = 0;
 out:
	explicit_bzero(&ctx, sizeof(ctx));
	explicit_bzero(&apphash, sizeof(apphash));
	explicit_bzero(&sighash, sizeof(sighash));
	ECDSA_SIG_free(sig);
	if (ret != 0) {
		free(response->sig_r);
		free(response->sig_s);
		response->sig_r = NULL;
		response->sig_s = NULL;
	}
	BIO_free(bio);
	EC_KEY_free(ec);
	EVP_PKEY_free(pk);
	return ret;
#else
	return -1;
#endif
}

static int
sig_ed25519(const uint8_t *message, size_t message_len,
    const char *application, uint32_t counter, uint8_t flags,
    const uint8_t *key_handle, size_t key_handle_len,
    struct sk_sign_response *response)
{
	size_t o;
	int ret = -1;
	SHA2_CTX ctx;
	uint8_t	apphash[SHA256_DIGEST_LENGTH];
	uint8_t signbuf[sizeof(apphash) + sizeof(flags) +
	    sizeof(counter) + SHA256_DIGEST_LENGTH];
	uint8_t sig[crypto_sign_ed25519_BYTES + sizeof(signbuf)];
	unsigned long long smlen;

	if (key_handle_len != crypto_sign_ed25519_SECRETKEYBYTES) {
		skdebug(__func__, "bad key handle length %zu", key_handle_len);
		goto out;
	}
	/* Expect message to be pre-hashed */
	if (message_len != SHA256_DIGEST_LENGTH) {
		skdebug(__func__, "bad message len %zu", message_len);
		goto out;
	}
	/* Prepare data to be signed */
	dump("message", message, message_len);
	SHA256Init(&ctx);
	SHA256Update(&ctx, (const u_char *)application, strlen(application));
	SHA256Final(apphash, &ctx);
	dump("apphash", apphash, sizeof(apphash));

	memcpy(signbuf, apphash, sizeof(apphash));
	o = sizeof(apphash);
	signbuf[o++] = flags;
	signbuf[o++] = (counter >> 24) & 0xff;
	signbuf[o++] = (counter >> 16) & 0xff;
	signbuf[o++] = (counter >> 8) & 0xff;
	signbuf[o++] = counter & 0xff;
	memcpy(signbuf + o, message, message_len);
	o += message_len;
	if (o != sizeof(signbuf)) {
		skdebug(__func__, "bad sign buf len %zu, expected %zu",
		    o, sizeof(signbuf));
		goto out;
	}
	dump("signbuf", signbuf, sizeof(signbuf));
	/* create and encode signature */
	smlen = sizeof(signbuf);
	if (crypto_sign_ed25519(sig, &smlen, signbuf, sizeof(signbuf),
	    key_handle) != 0) {
		skdebug(__func__, "crypto_sign_ed25519 failed");
		goto out;
	}
	if (smlen <= sizeof(signbuf)) {
		skdebug(__func__, "bad sign smlen %llu, expected min %zu",
		    smlen, sizeof(signbuf) + 1);
		goto out;
	}
	response->sig_r_len = (size_t)(smlen - sizeof(signbuf));
	if ((response->sig_r = calloc(1, response->sig_r_len)) == NULL) {
		skdebug(__func__, "calloc signature failed");
		goto out;
	}
	memcpy(response->sig_r, sig, response->sig_r_len);
	dump("sig_r", response->sig_r, response->sig_r_len);
	ret = 0;
 out:
	explicit_bzero(&ctx, sizeof(ctx));
	explicit_bzero(&apphash, sizeof(apphash));
	explicit_bzero(&signbuf, sizeof(signbuf));
	explicit_bzero(&sig, sizeof(sig));
	if (ret != 0) {
		free(response->sig_r);
		response->sig_r = NULL;
	}
	return ret;
}

int
sk_sign(uint32_t alg, const uint8_t *data, size_t datalen,
    const char *application, const uint8_t *key_handle, size_t key_handle_len,
    uint8_t flags, const char *pin, struct sk_option **options,
    struct sk_sign_response **sign_response)
{
	struct sk_sign_response *response = NULL;
	int ret = SSH_SK_ERR_GENERAL;
	SHA2_CTX ctx;
	uint8_t message[32];

	if (sign_response == NULL) {
		skdebug(__func__, "sign_response == NULL");
		goto out;
	}
	*sign_response = NULL;
	if (check_options(options) != 0)
		goto out; /* error already logged */
	if ((response = calloc(1, sizeof(*response))) == NULL) {
		skdebug(__func__, "calloc response failed");
		goto out;
	}
	SHA256Init(&ctx);
	SHA256Update(&ctx, data, datalen);
	SHA256Final(message, &ctx);
	response->flags = flags;
	response->counter = 0x12345678;
	switch(alg) {
	case SSH_SK_ECDSA:
		if (sig_ecdsa(message, sizeof(message), application,
		    response->counter, flags, key_handle, key_handle_len,
		    response) != 0)
			goto out;
		break;
	case SSH_SK_ED25519:
		if (sig_ed25519(message, sizeof(message), application,
		    response->counter, flags, key_handle, key_handle_len,
		    response) != 0)
			goto out;
		break;
	default:
		skdebug(__func__, "unsupported key type %d", alg);
		return -1;
	}
	*sign_response = response;
	response = NULL;
	ret = 0;
 out:
	explicit_bzero(message, sizeof(message));
	if (response != NULL) {
		free(response->sig_r);
		free(response->sig_s);
		free(response);
	}
	return ret;
}

int
sk_load_resident_keys(const char *pin, struct sk_option **options,
    struct sk_resident_key ***rks, size_t *nrks)
{
	return SSH_SK_ERR_UNSUPPORTED;
}
