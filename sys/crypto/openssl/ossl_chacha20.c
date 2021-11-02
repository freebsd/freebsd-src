/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
 * Copyright (c) 2020 Netflix, Inc
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer,
 *    without modification.
 * 2. Redistributions in binary form must reproduce at minimum a disclaimer
 *    similar to the "NO WARRANTY" disclaimer below ("Disclaimer") and any
 *    redistribution must be conditioned upon including a substantially
 *    similar Disclaimer requirement for further binary redistribution.
 *
 * NO WARRANTY
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF NONINFRINGEMENT, MERCHANTIBILITY
 * AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
 * THE COPYRIGHT HOLDERS OR CONTRIBUTORS BE LIABLE FOR SPECIAL, EXEMPLARY,
 * OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER
 * IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGES.
 */

#include <sys/types.h>
#include <sys/endian.h>
#include <sys/malloc.h>
#include <sys/time.h>

#include <opencrypto/cryptodev.h>

#include <crypto/openssl/ossl.h>
#include <crypto/openssl/ossl_chacha.h>
#include <crypto/openssl/ossl_cipher.h>
#include <crypto/openssl/ossl_poly1305.h>

static ossl_cipher_process_t ossl_chacha20;

struct ossl_cipher ossl_cipher_chacha20 = {
	.type = CRYPTO_CHACHA20,
	.blocksize = CHACHA_BLK_SIZE,
	.ivsize = CHACHA_CTR_SIZE,

	.set_encrypt_key = NULL,
	.set_decrypt_key = NULL,
	.process = ossl_chacha20
};

static int
ossl_chacha20(struct ossl_session_cipher *s, struct cryptop *crp,
    const struct crypto_session_params *csp)
{
	_Alignas(8) unsigned int key[CHACHA_KEY_SIZE / 4];
	unsigned int counter[CHACHA_CTR_SIZE / 4];
	unsigned char block[CHACHA_BLK_SIZE];
	struct crypto_buffer_cursor cc_in, cc_out;
	const unsigned char *in, *inseg, *cipher_key;
	unsigned char *out, *outseg;
	size_t resid, todo, inlen, outlen;
	uint32_t next_counter;
	u_int i;

	if (crp->crp_cipher_key != NULL)
		cipher_key = crp->crp_cipher_key;
	else
		cipher_key = csp->csp_cipher_key;
	for (i = 0; i < nitems(key); i++)
		key[i] = CHACHA_U8TOU32(cipher_key + i * 4);
	crypto_read_iv(crp, counter);
	for (i = 0; i < nitems(counter); i++)
		counter[i] = le32toh(counter[i]);

	resid = crp->crp_payload_length;
	crypto_cursor_init(&cc_in, &crp->crp_buf);
	crypto_cursor_advance(&cc_in, crp->crp_payload_start);
	inseg = crypto_cursor_segment(&cc_in, &inlen);
	if (CRYPTO_HAS_OUTPUT_BUFFER(crp)) {
		crypto_cursor_init(&cc_out, &crp->crp_obuf);
		crypto_cursor_advance(&cc_out, crp->crp_payload_output_start);
	} else
		cc_out = cc_in;
	outseg = crypto_cursor_segment(&cc_out, &outlen);
	while (resid >= CHACHA_BLK_SIZE) {
		if (inlen < CHACHA_BLK_SIZE) {
			crypto_cursor_copydata(&cc_in, CHACHA_BLK_SIZE, block);
			in = block;
			inlen = CHACHA_BLK_SIZE;
		} else
			in = inseg;
		if (outlen < CHACHA_BLK_SIZE) {
			out = block;
			outlen = CHACHA_BLK_SIZE;
		} else
			out = outseg;

		/* Figure out how many blocks we can encrypt/decrypt at once. */
		todo = rounddown(MIN(resid, MIN(inlen, outlen)),
		    CHACHA_BLK_SIZE);

#ifdef __LP64__
		/* ChaCha20_ctr32() assumes length is <= 4GB. */
		todo = (uint32_t)todo;
#endif

		/* Truncate if the 32-bit counter would roll over. */
		next_counter = counter[0] + todo / CHACHA_BLK_SIZE;
		if (next_counter < counter[0]) {
			todo -= next_counter * CHACHA_BLK_SIZE;
			next_counter = 0;
		}

		ChaCha20_ctr32(out, in, todo, key, counter);

		counter[0] = next_counter;
		if (counter[0] == 0)
			counter[1]++;

		if (out == block) {
			crypto_cursor_copyback(&cc_out, CHACHA_BLK_SIZE, block);
			outseg = crypto_cursor_segment(&cc_out, &outlen);
		} else {
			crypto_cursor_advance(&cc_out, todo);
			outseg += todo;
			outlen -= todo;
		}
		if (in == block) {
			inseg = crypto_cursor_segment(&cc_in, &inlen);
		} else {
			crypto_cursor_advance(&cc_in, todo);
			inseg += todo;
			inlen -= todo;
		}
		resid -= todo;
	}

	if (resid > 0) {
		memset(block, 0, sizeof(block));
		crypto_cursor_copydata(&cc_in, resid, block);
		ChaCha20_ctr32(block, block, CHACHA_BLK_SIZE, key, counter);
		crypto_cursor_copyback(&cc_out, resid, block);
	}

	explicit_bzero(block, sizeof(block));
	explicit_bzero(counter, sizeof(counter));
	explicit_bzero(key, sizeof(key));
	return (0);
}

int
ossl_chacha20_poly1305_encrypt(struct cryptop *crp,
    const struct crypto_session_params *csp)
{
	_Alignas(8) unsigned int key[CHACHA_KEY_SIZE / 4];
	unsigned int counter[CHACHA_CTR_SIZE / 4];
	_Alignas(8) unsigned char block[CHACHA_BLK_SIZE];
	unsigned char tag[POLY1305_HASH_LEN];
	POLY1305 auth_ctx;
	struct crypto_buffer_cursor cc_in, cc_out;
	const unsigned char *in, *inseg, *cipher_key;
	unsigned char *out, *outseg;
	size_t resid, todo, inlen, outlen;
	uint32_t next_counter;
	u_int i;

	if (crp->crp_cipher_key != NULL)
		cipher_key = crp->crp_cipher_key;
	else
		cipher_key = csp->csp_cipher_key;
	for (i = 0; i < nitems(key); i++)
		key[i] = CHACHA_U8TOU32(cipher_key + i * 4);

	memset(counter, 0, sizeof(counter));
	crypto_read_iv(crp, counter + (CHACHA_CTR_SIZE - csp->csp_ivlen) / 4);
	for (i = 1; i < nitems(counter); i++)
		counter[i] = le32toh(counter[i]);

	/* Block 0 is used to generate the poly1305 key. */
	counter[0] = 0;

	memset(block, 0, sizeof(block));
	ChaCha20_ctr32(block, block, sizeof(block), key, counter);
	Poly1305_Init(&auth_ctx, block);

	/* MAC the AAD. */
	if (crp->crp_aad != NULL)
		Poly1305_Update(&auth_ctx, crp->crp_aad, crp->crp_aad_length);
	else
		crypto_apply(crp, crp->crp_aad_start, crp->crp_aad_length,
		    ossl_poly1305_update, &auth_ctx);
	if (crp->crp_aad_length % 16 != 0) {
		/* padding1 */
		memset(block, 0, 16);
		Poly1305_Update(&auth_ctx, block,
		    16 - crp->crp_aad_length % 16);
	}

	/* Encryption starts with block 1. */
	counter[0] = 1;

	/* Do encryption with MAC */
	resid = crp->crp_payload_length;
	crypto_cursor_init(&cc_in, &crp->crp_buf);
	crypto_cursor_advance(&cc_in, crp->crp_payload_start);
	inseg = crypto_cursor_segment(&cc_in, &inlen);
	if (CRYPTO_HAS_OUTPUT_BUFFER(crp)) {
		crypto_cursor_init(&cc_out, &crp->crp_obuf);
		crypto_cursor_advance(&cc_out, crp->crp_payload_output_start);
	} else
		cc_out = cc_in;
	outseg = crypto_cursor_segment(&cc_out, &outlen);
	while (resid >= CHACHA_BLK_SIZE) {
		if (inlen < CHACHA_BLK_SIZE) {
			crypto_cursor_copydata(&cc_in, CHACHA_BLK_SIZE, block);
			in = block;
			inlen = CHACHA_BLK_SIZE;
		} else
			in = inseg;
		if (outlen < CHACHA_BLK_SIZE) {
			out = block;
			outlen = CHACHA_BLK_SIZE;
		} else
			out = outseg;

		/* Figure out how many blocks we can encrypt/decrypt at once. */
		todo = rounddown(MIN(resid, MIN(inlen, outlen)),
		    CHACHA_BLK_SIZE);

#ifdef __LP64__
		/* ChaCha20_ctr32() assumes length is <= 4GB. */
		todo = (uint32_t)todo;
#endif

		/* Truncate if the 32-bit counter would roll over. */
		next_counter = counter[0] + todo / CHACHA_BLK_SIZE;
		if (csp->csp_ivlen == 8 && next_counter < counter[0]) {
			todo -= next_counter * CHACHA_BLK_SIZE;
			next_counter = 0;
		}

		ChaCha20_ctr32(out, in, todo, key, counter);
		Poly1305_Update(&auth_ctx, out, todo);

		counter[0] = next_counter;
		if (csp->csp_ivlen == 8 && counter[0] == 0)
			counter[1]++;

		if (out == block) {
			crypto_cursor_copyback(&cc_out, CHACHA_BLK_SIZE, block);
			outseg = crypto_cursor_segment(&cc_out, &outlen);
		} else {
			crypto_cursor_advance(&cc_out, todo);
			outseg += todo;
			outlen -= todo;
		}
		if (in == block) {
			inseg = crypto_cursor_segment(&cc_in, &inlen);
		} else {
			crypto_cursor_advance(&cc_in, todo);
			inseg += todo;
			inlen -= todo;
		}
		resid -= todo;
	}

	if (resid > 0) {
		memset(block, 0, sizeof(block));
		crypto_cursor_copydata(&cc_in, resid, block);
		ChaCha20_ctr32(block, block, CHACHA_BLK_SIZE, key, counter);
		crypto_cursor_copyback(&cc_out, resid, block);

		/* padding2 */
		todo = roundup2(resid, 16);
		memset(block + resid, 0, todo - resid);
		Poly1305_Update(&auth_ctx, block, todo);
	}

	/* lengths */
	le64enc(block, crp->crp_aad_length);
	le64enc(block + 8, crp->crp_payload_length);
	Poly1305_Update(&auth_ctx, block, sizeof(uint64_t) * 2);

	Poly1305_Final(&auth_ctx, tag);
	crypto_copyback(crp, crp->crp_digest_start, csp->csp_auth_mlen == 0 ?
	    POLY1305_HASH_LEN : csp->csp_auth_mlen, tag);

	explicit_bzero(&auth_ctx, sizeof(auth_ctx));
	explicit_bzero(tag, sizeof(tag));
	explicit_bzero(block, sizeof(block));
	explicit_bzero(counter, sizeof(counter));
	explicit_bzero(key, sizeof(key));
	return (0);
}


int
ossl_chacha20_poly1305_decrypt(struct cryptop *crp,
    const struct crypto_session_params *csp)
{
	_Alignas(8) unsigned int key[CHACHA_KEY_SIZE / 4];
	unsigned int counter[CHACHA_CTR_SIZE / 4];
	_Alignas(8) unsigned char block[CHACHA_BLK_SIZE];
	unsigned char tag[POLY1305_HASH_LEN], tag2[POLY1305_HASH_LEN];
	struct poly1305_context auth_ctx;
	struct crypto_buffer_cursor cc_in, cc_out;
	const unsigned char *in, *inseg, *cipher_key;
	unsigned char *out, *outseg;
	size_t resid, todo, inlen, outlen;
	uint32_t next_counter;
	int error;
	u_int i, mlen;

	if (crp->crp_cipher_key != NULL)
		cipher_key = crp->crp_cipher_key;
	else
		cipher_key = csp->csp_cipher_key;
	for (i = 0; i < nitems(key); i++)
		key[i] = CHACHA_U8TOU32(cipher_key + i * 4);

	memset(counter, 0, sizeof(counter));
	crypto_read_iv(crp, counter + (CHACHA_CTR_SIZE - csp->csp_ivlen) / 4);
	for (i = 1; i < nitems(counter); i++)
		counter[i] = le32toh(counter[i]);

	/* Block 0 is used to generate the poly1305 key. */
	counter[0] = 0;

	memset(block, 0, sizeof(block));
	ChaCha20_ctr32(block, block, sizeof(block), key, counter);
	Poly1305_Init(&auth_ctx, block);

	/* MAC the AAD. */
	if (crp->crp_aad != NULL)
		Poly1305_Update(&auth_ctx, crp->crp_aad, crp->crp_aad_length);
	else
		crypto_apply(crp, crp->crp_aad_start, crp->crp_aad_length,
		    ossl_poly1305_update, &auth_ctx);
	if (crp->crp_aad_length % 16 != 0) {
		/* padding1 */
		memset(block, 0, 16);
		Poly1305_Update(&auth_ctx, block,
		    16 - crp->crp_aad_length % 16);
	}

	/* Mac the ciphertext. */
	crypto_apply(crp, crp->crp_payload_start, crp->crp_payload_length,
	    ossl_poly1305_update, &auth_ctx);
	if (crp->crp_payload_length % 16 != 0) {
		/* padding2 */
		memset(block, 0, 16);
		Poly1305_Update(&auth_ctx, block,
		    16 - crp->crp_payload_length % 16);
	}

	/* lengths */
	le64enc(block, crp->crp_aad_length);
	le64enc(block + 8, crp->crp_payload_length);
	Poly1305_Update(&auth_ctx, block, sizeof(uint64_t) * 2);

	Poly1305_Final(&auth_ctx, tag);
	mlen = csp->csp_auth_mlen == 0 ? POLY1305_HASH_LEN : csp->csp_auth_mlen;
	crypto_copydata(crp, crp->crp_digest_start, mlen, tag2);
	if (timingsafe_bcmp(tag, tag2, mlen) != 0) {
		error = EBADMSG;
		goto out;
	}

	/* Decryption starts with block 1. */
	counter[0] = 1;

	resid = crp->crp_payload_length;
	crypto_cursor_init(&cc_in, &crp->crp_buf);
	crypto_cursor_advance(&cc_in, crp->crp_payload_start);
	inseg = crypto_cursor_segment(&cc_in, &inlen);
	if (CRYPTO_HAS_OUTPUT_BUFFER(crp)) {
		crypto_cursor_init(&cc_out, &crp->crp_obuf);
		crypto_cursor_advance(&cc_out, crp->crp_payload_output_start);
	} else
		cc_out = cc_in;
	outseg = crypto_cursor_segment(&cc_out, &outlen);
	while (resid >= CHACHA_BLK_SIZE) {
		if (inlen < CHACHA_BLK_SIZE) {
			crypto_cursor_copydata(&cc_in, CHACHA_BLK_SIZE, block);
			in = block;
			inlen = CHACHA_BLK_SIZE;
		} else
			in = inseg;
		if (outlen < CHACHA_BLK_SIZE) {
			out = block;
			outlen = CHACHA_BLK_SIZE;
		} else
			out = outseg;

		/* Figure out how many blocks we can encrypt/decrypt at once. */
		todo = rounddown(MIN(resid, MIN(inlen, outlen)),
		    CHACHA_BLK_SIZE);

#ifdef __LP64__
		/* ChaCha20_ctr32() assumes length is <= 4GB. */
		todo = (uint32_t)todo;
#endif

		/* Truncate if the 32-bit counter would roll over. */
		next_counter = counter[0] + todo / CHACHA_BLK_SIZE;
		if (csp->csp_ivlen == 8 && next_counter < counter[0]) {
			todo -= next_counter * CHACHA_BLK_SIZE;
			next_counter = 0;
		}

		ChaCha20_ctr32(out, in, todo, key, counter);

		counter[0] = next_counter;
		if (csp->csp_ivlen == 8 && counter[0] == 0)
			counter[1]++;

		if (out == block) {
			crypto_cursor_copyback(&cc_out, CHACHA_BLK_SIZE, block);
			outseg = crypto_cursor_segment(&cc_out, &outlen);
		} else {
			crypto_cursor_advance(&cc_out, todo);
			outseg += todo;
			outlen -= todo;
		}
		if (in == block) {
			inseg = crypto_cursor_segment(&cc_in, &inlen);
		} else {
			crypto_cursor_advance(&cc_in, todo);
			inseg += todo;
			inlen -= todo;
		}
		resid -= todo;
	}

	if (resid > 0) {
		memset(block, 0, sizeof(block));
		crypto_cursor_copydata(&cc_in, resid, block);
		ChaCha20_ctr32(block, block, CHACHA_BLK_SIZE, key, counter);
		crypto_cursor_copyback(&cc_out, resid, block);
	}

	error = 0;
out:
	explicit_bzero(&auth_ctx, sizeof(auth_ctx));
	explicit_bzero(tag, sizeof(tag));
	explicit_bzero(block, sizeof(block));
	explicit_bzero(counter, sizeof(counter));
	explicit_bzero(key, sizeof(key));
	return (error);
}
