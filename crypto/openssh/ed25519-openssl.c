/* $OpenBSD: ed25519-openssl.c,v 1.1 2025/10/30 20:49:10 djm Exp $ */
/*
 * Copyright (c) 2025 OpenSSH
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

/*
 * OpenSSL-based implementation of Ed25519 crypto_sign API
 * Alternative to the internal SUPERCOP-based implementation in ed25519.c
 */

#include "includes.h"

#ifdef OPENSSL_HAS_ED25519

#include <sys/types.h>

#include <string.h>
#include <stdint.h>
#include <limits.h>

#include <openssl/evp.h>

#include "crypto_api.h"
#include "log.h"

#if crypto_sign_ed25519_SECRETKEYBYTES <= crypto_sign_ed25519_PUBLICKEYBYTES
#error "crypto_sign_ed25519_SECRETKEYBYTES < crypto_sign_ed25519_PUBLICKEYBYTES"
#endif

#define SSH_ED25519_RAW_SECRET_KEY_LEN \
    (crypto_sign_ed25519_SECRETKEYBYTES - crypto_sign_ed25519_PUBLICKEYBYTES)

int
crypto_sign_ed25519_keypair(unsigned char *pk, unsigned char *sk)
{
	EVP_PKEY_CTX *ctx = NULL;
	EVP_PKEY *pkey = NULL;
	size_t pklen, sklen;
	int ret = -1;

	if ((ctx = EVP_PKEY_CTX_new_id(EVP_PKEY_ED25519, NULL)) == NULL) {
		debug3_f("EVP_PKEY_CTX_new_id failed");
		goto out;
	}
	if (EVP_PKEY_keygen_init(ctx) <= 0) {
		debug3_f("EVP_PKEY_keygen_init failed");
		goto out;
	}
	if (EVP_PKEY_keygen(ctx, &pkey) <= 0) {
		debug3_f("EVP_PKEY_keygen failed");
		goto out;
	}

	/* Extract public key */
	pklen = crypto_sign_ed25519_PUBLICKEYBYTES;
	if (!EVP_PKEY_get_raw_public_key(pkey, pk, &pklen)) {
		debug3_f("EVP_PKEY_get_raw_public_key failed");
		goto out;
	}
	if (pklen != crypto_sign_ed25519_PUBLICKEYBYTES) {
		debug3_f("public key length mismatch: %zu", pklen);
		goto out;
	}

	sklen = SSH_ED25519_RAW_SECRET_KEY_LEN;
	/* Extract private key (32 bytes seed) */
	if (!EVP_PKEY_get_raw_private_key(pkey, sk, &sklen)) {
		debug3_f("EVP_PKEY_get_raw_private_key failed");
		goto out;
	}
	if (sklen != SSH_ED25519_RAW_SECRET_KEY_LEN) {
		debug3_f("private key length mismatch: %zu", sklen);
		goto out;
	}

	/* Append public key to secret key (SUPERCOP format compatibility) */
	memcpy(sk + sklen, pk, crypto_sign_ed25519_PUBLICKEYBYTES);

	ret = 0;
out:
	EVP_PKEY_free(pkey);
	EVP_PKEY_CTX_free(ctx);
	return ret;
}

int
crypto_sign_ed25519(unsigned char *sm, unsigned long long *smlen,
    const unsigned char *m, unsigned long long mlen,
    const unsigned char *sk)
{
	EVP_PKEY *pkey = NULL;
	EVP_MD_CTX *mdctx = NULL;
	size_t siglen;
	int ret = -1;

	/* Create EVP_PKEY from secret key (first 32 bytes are the seed) */
	if ((pkey = EVP_PKEY_new_raw_private_key(EVP_PKEY_ED25519, NULL,
	    sk, SSH_ED25519_RAW_SECRET_KEY_LEN)) == NULL) {
		debug3_f("EVP_PKEY_new_raw_private_key failed");
		goto out;
	}

	/* Sign the message */
	if ((mdctx = EVP_MD_CTX_new()) == NULL) {
		debug3_f("EVP_MD_CTX_new failed");
		goto out;
	}
	if (EVP_DigestSignInit(mdctx, NULL, NULL, NULL, pkey) != 1) {
		debug3_f("EVP_DigestSignInit failed");
		goto out;
	}
	siglen = crypto_sign_ed25519_BYTES;
	if (EVP_DigestSign(mdctx, sm, &siglen, m, mlen) != 1) {
		debug3_f("EVP_DigestSign failed");
		goto out;
	}
	if (siglen != crypto_sign_ed25519_BYTES) {
		debug3_f("signature length mismatch: %zu", siglen);
		goto out;
	}

	/* Append message after signature (SUPERCOP format) */
	if (mlen > ULLONG_MAX - siglen) {
		debug3_f("message length overflow: siglen=%zu mlen=%llu",
		    siglen, mlen);
		goto out;
	}
	memmove(sm + siglen, m, mlen);
	*smlen = siglen + mlen;

	ret = 0;
out:
	EVP_MD_CTX_free(mdctx);
	EVP_PKEY_free(pkey);
	return ret;
}

int
crypto_sign_ed25519_open(unsigned char *m, unsigned long long *mlen,
    const unsigned char *sm, unsigned long long smlen,
    const unsigned char *pk)
{
	EVP_PKEY *pkey = NULL;
	EVP_MD_CTX *mdctx = NULL;
	int ret = -1;
	const unsigned char *msg;
	size_t msglen;

	if (smlen < crypto_sign_ed25519_BYTES) {
		debug3_f("signed message bad length: %llu", smlen);
		return -1;
	}
	/* Signature is first crypto_sign_ed25519_BYTES, message follows */
	msg = sm + crypto_sign_ed25519_BYTES;
	msglen = smlen - crypto_sign_ed25519_BYTES;

	/* Make sure the message buffer is big enough. */
	if (*mlen < msglen) {
		debug_f("message bad length: %llu", *mlen);
		return -1;
	}

	/* Create EVP_PKEY from public key */
	if ((pkey = EVP_PKEY_new_raw_public_key(EVP_PKEY_ED25519, NULL,
	    pk, crypto_sign_ed25519_PUBLICKEYBYTES)) == NULL) {
		debug3_f("EVP_PKEY_new_raw_public_key failed");
		goto out;
	}

	if ((mdctx = EVP_MD_CTX_new()) == NULL) {
		debug3_f("EVP_MD_CTX_new failed");
		goto out;
	}
	if (EVP_DigestVerifyInit(mdctx, NULL, NULL, NULL, pkey) <= 0) {
		debug3_f("EVP_DigestVerifyInit failed");
		goto out;
	}
	if (EVP_DigestVerify(mdctx, sm, crypto_sign_ed25519_BYTES,
	    msg, msglen) != 1) {
		debug3_f("EVP_DigestVerify failed");
		goto out;
	}

	/* Copy message out */
	*mlen = msglen;
	memmove(m, msg, msglen);

	ret = 0;
out:
	EVP_MD_CTX_free(mdctx);
	EVP_PKEY_free(pkey);
	return ret;
}

#endif /* OPENSSL_HAS_ED25519 */
