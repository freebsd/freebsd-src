/* $OpenBSD: kexmlkem768x25519.c,v 1.1 2024/09/02 12:13:56 djm Exp $ */
/*
 * Copyright (c) 2023 Markus Friedl.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "includes.h"

#include <sys/types.h>

#include <stdio.h>
#ifdef HAVE_STDINT_H
#include <stdint.h>
#endif
#include <stdbool.h>
#include <string.h>
#include <signal.h>

#include "sshkey.h"
#include "kex.h"
#include "sshbuf.h"
#include "digest.h"
#include "ssherr.h"
#include "log.h"

#ifdef USE_MLKEM768X25519

#include "libcrux_mlkem768_sha3.h"

int
kex_kem_mlkem768x25519_keypair(struct kex *kex)
{
	struct sshbuf *buf = NULL;
	u_char rnd[LIBCRUX_ML_KEM_KEY_PAIR_PRNG_LEN], *cp = NULL;
	size_t need;
	int r = SSH_ERR_INTERNAL_ERROR;
	struct libcrux_mlkem768_keypair keypair;

	if ((buf = sshbuf_new()) == NULL)
		return SSH_ERR_ALLOC_FAIL;
	need = crypto_kem_mlkem768_PUBLICKEYBYTES + CURVE25519_SIZE;
	if ((r = sshbuf_reserve(buf, need, &cp)) != 0)
		goto out;
	arc4random_buf(rnd, sizeof(rnd));
	keypair = libcrux_ml_kem_mlkem768_portable_generate_key_pair(rnd);
	memcpy(cp, keypair.pk.value, crypto_kem_mlkem768_PUBLICKEYBYTES);
	memcpy(kex->mlkem768_client_key, keypair.sk.value,
	    sizeof(kex->mlkem768_client_key));
#ifdef DEBUG_KEXECDH
	dump_digest("client public key mlkem768:", cp,
	    crypto_kem_mlkem768_PUBLICKEYBYTES);
#endif
	cp += crypto_kem_mlkem768_PUBLICKEYBYTES;
	kexc25519_keygen(kex->c25519_client_key, cp);
#ifdef DEBUG_KEXECDH
	dump_digest("client public key c25519:", cp, CURVE25519_SIZE);
#endif
	/* success */
	r = 0;
	kex->client_pub = buf;
	buf = NULL;
 out:
	explicit_bzero(&keypair, sizeof(keypair));
	explicit_bzero(rnd, sizeof(rnd));
	sshbuf_free(buf);
	return r;
}

int
kex_kem_mlkem768x25519_enc(struct kex *kex,
   const struct sshbuf *client_blob, struct sshbuf **server_blobp,
   struct sshbuf **shared_secretp)
{
	struct sshbuf *server_blob = NULL;
	struct sshbuf *buf = NULL;
	const u_char *client_pub;
	u_char rnd[LIBCRUX_ML_KEM_ENC_PRNG_LEN];
	u_char server_pub[CURVE25519_SIZE], server_key[CURVE25519_SIZE];
	u_char hash[SSH_DIGEST_MAX_LENGTH];
	size_t need;
	int r = SSH_ERR_INTERNAL_ERROR;
	struct libcrux_mlkem768_enc_result enc;
	struct libcrux_mlkem768_pk mlkem_pub;

	*server_blobp = NULL;
	*shared_secretp = NULL;
	memset(&mlkem_pub, 0, sizeof(mlkem_pub));

	/* client_blob contains both KEM and ECDH client pubkeys */
	need = crypto_kem_mlkem768_PUBLICKEYBYTES + CURVE25519_SIZE;
	if (sshbuf_len(client_blob) != need) {
		r = SSH_ERR_SIGNATURE_INVALID;
		goto out;
	}
	client_pub = sshbuf_ptr(client_blob);
#ifdef DEBUG_KEXECDH
	dump_digest("client public key mlkem768:", client_pub,
	    crypto_kem_mlkem768_PUBLICKEYBYTES);
	dump_digest("client public key 25519:",
	    client_pub + crypto_kem_mlkem768_PUBLICKEYBYTES,
	    CURVE25519_SIZE);
#endif
	/* check public key validity */
	memcpy(mlkem_pub.value, client_pub, crypto_kem_mlkem768_PUBLICKEYBYTES);
	if (!libcrux_ml_kem_mlkem768_portable_validate_public_key(&mlkem_pub)) {
		r = SSH_ERR_SIGNATURE_INVALID;
		goto out;
	}

	/* allocate buffer for concatenation of KEM key and ECDH shared key */
	/* the buffer will be hashed and the result is the shared secret */
	if ((buf = sshbuf_new()) == NULL) {
		r = SSH_ERR_ALLOC_FAIL;
		goto out;
	}
	/* allocate space for encrypted KEM key and ECDH pub key */
	if ((server_blob = sshbuf_new()) == NULL) {
		r = SSH_ERR_ALLOC_FAIL;
		goto out;
	}
	/* generate and encrypt KEM key with client key */
	arc4random_buf(rnd, sizeof(rnd));
	enc = libcrux_ml_kem_mlkem768_portable_encapsulate(&mlkem_pub, rnd);
	/* generate ECDH key pair, store server pubkey after ciphertext */
	kexc25519_keygen(server_key, server_pub);
	if ((r = sshbuf_put(buf, enc.snd, sizeof(enc.snd))) != 0 ||
	    (r = sshbuf_put(server_blob, enc.fst.value, sizeof(enc.fst.value))) != 0 ||
	    (r = sshbuf_put(server_blob, server_pub, sizeof(server_pub))) != 0)
		goto out;
	/* append ECDH shared key */
	client_pub += crypto_kem_mlkem768_PUBLICKEYBYTES;
	if ((r = kexc25519_shared_key_ext(server_key, client_pub, buf, 1)) < 0)
		goto out;
	if ((r = ssh_digest_buffer(kex->hash_alg, buf, hash, sizeof(hash))) != 0)
		goto out;
#ifdef DEBUG_KEXECDH
	dump_digest("server public key 25519:", server_pub, CURVE25519_SIZE);
	dump_digest("server cipher text:",
	    enc.fst.value, sizeof(enc.fst.value));
	dump_digest("server kem key:", enc.snd, sizeof(enc.snd));
	dump_digest("concatenation of KEM key and ECDH shared key:",
	    sshbuf_ptr(buf), sshbuf_len(buf));
#endif
	/* string-encoded hash is resulting shared secret */
	sshbuf_reset(buf);
	if ((r = sshbuf_put_string(buf, hash,
	    ssh_digest_bytes(kex->hash_alg))) != 0)
		goto out;
#ifdef DEBUG_KEXECDH
	dump_digest("encoded shared secret:", sshbuf_ptr(buf), sshbuf_len(buf));
#endif
	/* success */
	r = 0;
	*server_blobp = server_blob;
	*shared_secretp = buf;
	server_blob = NULL;
	buf = NULL;
 out:
	explicit_bzero(hash, sizeof(hash));
	explicit_bzero(server_key, sizeof(server_key));
	explicit_bzero(rnd, sizeof(rnd));
	explicit_bzero(&enc, sizeof(enc));
	sshbuf_free(server_blob);
	sshbuf_free(buf);
	return r;
}

int
kex_kem_mlkem768x25519_dec(struct kex *kex,
    const struct sshbuf *server_blob, struct sshbuf **shared_secretp)
{
	struct sshbuf *buf = NULL;
	u_char mlkem_key[crypto_kem_mlkem768_BYTES];
	const u_char *ciphertext, *server_pub;
	u_char hash[SSH_DIGEST_MAX_LENGTH];
	size_t need;
	int r;
	struct libcrux_mlkem768_sk mlkem_priv;
	struct libcrux_mlkem768_ciphertext mlkem_ciphertext;

	*shared_secretp = NULL;
	memset(&mlkem_priv, 0, sizeof(mlkem_priv));
	memset(&mlkem_ciphertext, 0, sizeof(mlkem_ciphertext));

	need = crypto_kem_mlkem768_CIPHERTEXTBYTES + CURVE25519_SIZE;
	if (sshbuf_len(server_blob) != need) {
		r = SSH_ERR_SIGNATURE_INVALID;
		goto out;
	}
	ciphertext = sshbuf_ptr(server_blob);
	server_pub = ciphertext + crypto_kem_mlkem768_CIPHERTEXTBYTES;
	/* hash concatenation of KEM key and ECDH shared key */
	if ((buf = sshbuf_new()) == NULL) {
		r = SSH_ERR_ALLOC_FAIL;
		goto out;
	}
	memcpy(mlkem_priv.value, kex->mlkem768_client_key,
	    sizeof(kex->mlkem768_client_key));
	memcpy(mlkem_ciphertext.value, ciphertext,
	    sizeof(mlkem_ciphertext.value));
#ifdef DEBUG_KEXECDH
	dump_digest("server cipher text:", mlkem_ciphertext.value,
	    sizeof(mlkem_ciphertext.value));
	dump_digest("server public key c25519:", server_pub, CURVE25519_SIZE);
#endif
	libcrux_ml_kem_mlkem768_portable_decapsulate(&mlkem_priv,
	    &mlkem_ciphertext, mlkem_key);
	if ((r = sshbuf_put(buf, mlkem_key, sizeof(mlkem_key))) != 0)
		goto out;
	if ((r = kexc25519_shared_key_ext(kex->c25519_client_key, server_pub,
	    buf, 1)) < 0)
		goto out;
	if ((r = ssh_digest_buffer(kex->hash_alg, buf,
	    hash, sizeof(hash))) != 0)
		goto out;
#ifdef DEBUG_KEXECDH
	dump_digest("client kem key:", mlkem_key, sizeof(mlkem_key));
	dump_digest("concatenation of KEM key and ECDH shared key:",
	    sshbuf_ptr(buf), sshbuf_len(buf));
#endif
	sshbuf_reset(buf);
	if ((r = sshbuf_put_string(buf, hash,
	    ssh_digest_bytes(kex->hash_alg))) != 0)
		goto out;
#ifdef DEBUG_KEXECDH
	dump_digest("encoded shared secret:", sshbuf_ptr(buf), sshbuf_len(buf));
#endif
	/* success */
	r = 0;
	*shared_secretp = buf;
	buf = NULL;
 out:
	explicit_bzero(hash, sizeof(hash));
	explicit_bzero(&mlkem_priv, sizeof(mlkem_priv));
	explicit_bzero(&mlkem_ciphertext, sizeof(mlkem_ciphertext));
	explicit_bzero(mlkem_key, sizeof(mlkem_key));
	sshbuf_free(buf);
	return r;
}
#else /* USE_MLKEM768X25519 */
int
kex_kem_mlkem768x25519_keypair(struct kex *kex)
{
	return SSH_ERR_SIGN_ALG_UNSUPPORTED;
}

int
kex_kem_mlkem768x25519_enc(struct kex *kex,
   const struct sshbuf *client_blob, struct sshbuf **server_blobp,
   struct sshbuf **shared_secretp)
{
	return SSH_ERR_SIGN_ALG_UNSUPPORTED;
}

int
kex_kem_mlkem768x25519_dec(struct kex *kex,
    const struct sshbuf *server_blob, struct sshbuf **shared_secretp)
{
	return SSH_ERR_SIGN_ALG_UNSUPPORTED;
}
#endif /* USE_MLKEM768X25519 */
