/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2005-2010 Pawel Jakub Dawidek <pjd@FreeBSD.org>
 * All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHORS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHORS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/param.h>
#ifdef _KERNEL
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#else
#include <stdint.h>
#include <string.h>
#include <strings.h>
#include <errno.h>
#include <assert.h>
#include <openssl/evp.h>
#define	_OpenSSL_
#endif
#include <geom/eli/g_eli.h>

#ifdef _KERNEL
MALLOC_DECLARE(M_ELI);

static int
g_eli_crypto_done(struct cryptop *crp)
{

	crp->crp_opaque = (void *)crp;
	wakeup(crp);
	return (0);
}

static int
g_eli_crypto_cipher(u_int algo, int enc, u_char *data, size_t datasize,
    const u_char *key, size_t keysize)
{
	struct crypto_session_params csp;
	struct cryptop *crp;
	crypto_session_t sid;
	int error;

	KASSERT(algo != CRYPTO_AES_XTS,
	    ("%s: CRYPTO_AES_XTS unexpected here", __func__));

	memset(&csp, 0, sizeof(csp));
	csp.csp_mode = CSP_MODE_CIPHER;
	csp.csp_cipher_alg = algo;
	csp.csp_ivlen = g_eli_ivlen(algo);
	csp.csp_cipher_key = key;
	csp.csp_cipher_klen = keysize / 8;
	error = crypto_newsession(&sid, &csp, CRYPTOCAP_F_SOFTWARE);
	if (error != 0)
		return (error);
	crp = crypto_getreq(sid, M_NOWAIT);
	if (crp == NULL) {
		crypto_freesession(sid);
		return (ENOMEM);
	}

	crp->crp_payload_start = 0;
	crp->crp_payload_length = datasize;
	crp->crp_flags = CRYPTO_F_CBIFSYNC | CRYPTO_F_IV_SEPARATE;
	crp->crp_op = enc ? CRYPTO_OP_ENCRYPT : CRYPTO_OP_DECRYPT;
	memset(crp->crp_iv, 0, sizeof(crp->crp_iv));

	crp->crp_opaque = NULL;
	crp->crp_callback = g_eli_crypto_done;
	crypto_use_buf(crp, data, datasize);

	error = crypto_dispatch(crp);
	if (error == 0) {
		while (crp->crp_opaque == NULL)
			tsleep(crp, PRIBIO, "geli", hz / 5);
		error = crp->crp_etype;
	}

	crypto_freereq(crp);
	crypto_freesession(sid);
	return (error);
}
#else	/* !_KERNEL */
static int
g_eli_crypto_cipher(u_int algo, int enc, u_char *data, size_t datasize,
    const u_char *key, size_t keysize)
{
	EVP_CIPHER_CTX *ctx;
	const EVP_CIPHER *type;
	u_char iv[G_ELI_IVKEYLEN];
	int outsize;

	assert(algo != CRYPTO_AES_XTS);

	switch (algo) {
	case CRYPTO_NULL_CBC:
		type = EVP_enc_null();
		break;
	case CRYPTO_AES_CBC:
		switch (keysize) {
		case 128:
			type = EVP_aes_128_cbc();
			break;
		case 192:
			type = EVP_aes_192_cbc();
			break;
		case 256:
			type = EVP_aes_256_cbc();
			break;
		default:
			return (EINVAL);
		}
		break;
#ifndef OPENSSL_NO_CAMELLIA
	case CRYPTO_CAMELLIA_CBC:
		switch (keysize) {
		case 128:
			type = EVP_camellia_128_cbc();
			break;
		case 192:
			type = EVP_camellia_192_cbc();
			break;
		case 256:
			type = EVP_camellia_256_cbc();
			break;
		default:
			return (EINVAL);
		}
		break;
#endif
	default:
		return (EINVAL);
	}

	ctx = EVP_CIPHER_CTX_new();
	if (ctx == NULL)
		return (ENOMEM);

	EVP_CipherInit_ex(ctx, type, NULL, NULL, NULL, enc);
	EVP_CIPHER_CTX_set_key_length(ctx, keysize / 8);
	EVP_CIPHER_CTX_set_padding(ctx, 0);
	bzero(iv, sizeof(iv));
	EVP_CipherInit_ex(ctx, NULL, NULL, key, iv, enc);

	if (EVP_CipherUpdate(ctx, data, &outsize, data, datasize) == 0) {
		EVP_CIPHER_CTX_free(ctx);
		return (EINVAL);
	}
	assert(outsize == (int)datasize);

	if (EVP_CipherFinal_ex(ctx, data + outsize, &outsize) == 0) {
		EVP_CIPHER_CTX_free(ctx);
		return (EINVAL);
	}
	assert(outsize == 0);

	EVP_CIPHER_CTX_free(ctx);
	return (0);
}
#endif	/* !_KERNEL */

int
g_eli_crypto_encrypt(u_int algo, u_char *data, size_t datasize,
    const u_char *key, size_t keysize)
{

	/* We prefer AES-CBC for metadata protection. */
	if (algo == CRYPTO_AES_XTS)
		algo = CRYPTO_AES_CBC;

	return (g_eli_crypto_cipher(algo, 1, data, datasize, key, keysize));
}

int
g_eli_crypto_decrypt(u_int algo, u_char *data, size_t datasize,
    const u_char *key, size_t keysize)
{

	/* We prefer AES-CBC for metadata protection. */
	if (algo == CRYPTO_AES_XTS)
		algo = CRYPTO_AES_CBC;

	return (g_eli_crypto_cipher(algo, 0, data, datasize, key, keysize));
}
