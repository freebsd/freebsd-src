/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
 * Copyright (c) 2020 Netflix Inc.
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
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <opencrypto/xform_auth.h>
#include <opencrypto/xform_enc.h>

#include <sodium/crypto_core_hchacha20.h>
#include <sodium/crypto_onetimeauth_poly1305.h>
#include <sodium/crypto_stream_chacha20.h>

struct chacha20_poly1305_ctx {
	struct crypto_onetimeauth_poly1305_state auth;
	const void *key;
	uint32_t ic;
	bool ietf;
	char nonce[CHACHA20_POLY1305_IV_LEN];
};

struct xchacha20_poly1305_ctx {
	struct chacha20_poly1305_ctx base_ctx;	/* must be first */
	const void *key;
	char derived_key[CHACHA20_POLY1305_KEY];
};

static int
chacha20_poly1305_setkey(void *vctx, const uint8_t *key, int len)
{
	struct chacha20_poly1305_ctx *ctx = vctx;

	if (len != CHACHA20_POLY1305_KEY)
		return (EINVAL);

	ctx->key = key;
	return (0);
}

static void
chacha20_poly1305_reinit(void *vctx, const uint8_t *iv, size_t ivlen)
{
	struct chacha20_poly1305_ctx *ctx = vctx;
	char block[CHACHA20_NATIVE_BLOCK_LEN];

	KASSERT(ivlen == 8 || ivlen == sizeof(ctx->nonce),
	    ("%s: invalid nonce length", __func__));

	memcpy(ctx->nonce, iv, ivlen);
	ctx->ietf = (ivlen == CHACHA20_POLY1305_IV_LEN);

	/* Block 0 is used for the poly1305 key. */
	if (ctx->ietf)
		crypto_stream_chacha20_ietf(block, sizeof(block), iv, ctx->key);
	else
		crypto_stream_chacha20(block, sizeof(block), iv, ctx->key);
	crypto_onetimeauth_poly1305_init(&ctx->auth, block);
	explicit_bzero(block, sizeof(block));

	/* Start with block 1 for ciphertext. */
	ctx->ic = 1;
}

static void
chacha20_poly1305_crypt(void *vctx, const uint8_t *in, uint8_t *out)
{
	struct chacha20_poly1305_ctx *ctx = vctx;
	int error __diagused;

	if (ctx->ietf)
		error = crypto_stream_chacha20_ietf_xor_ic(out, in,
		    CHACHA20_NATIVE_BLOCK_LEN, ctx->nonce, ctx->ic, ctx->key);
	else
		error = crypto_stream_chacha20_xor_ic(out, in,
		    CHACHA20_NATIVE_BLOCK_LEN, ctx->nonce, ctx->ic, ctx->key);
	KASSERT(error == 0, ("%s failed: %d", __func__, error));
	ctx->ic++;
}

static void
chacha20_poly1305_crypt_last(void *vctx, const uint8_t *in, uint8_t *out,
    size_t len)
{
	struct chacha20_poly1305_ctx *ctx = vctx;

	int error __diagused;

	if (ctx->ietf)
		error = crypto_stream_chacha20_ietf_xor_ic(out, in, len,
		    ctx->nonce, ctx->ic, ctx->key);
	else
		error = crypto_stream_chacha20_xor_ic(out, in, len, ctx->nonce,
		    ctx->ic, ctx->key);
	KASSERT(error == 0, ("%s failed: %d", __func__, error));
}

static int
chacha20_poly1305_update(void *vctx, const void *data, u_int len)
{
	struct chacha20_poly1305_ctx *ctx = vctx;

	crypto_onetimeauth_poly1305_update(&ctx->auth, data, len);
	return (0);
}

static void
chacha20_poly1305_final(uint8_t *digest, void *vctx)
{
	struct chacha20_poly1305_ctx *ctx = vctx;

	crypto_onetimeauth_poly1305_final(&ctx->auth, digest);
}

const struct enc_xform enc_xform_chacha20_poly1305 = {
	.type = CRYPTO_CHACHA20_POLY1305,
	.name = "ChaCha20-Poly1305",
	.ctxsize = sizeof(struct chacha20_poly1305_ctx),
	.blocksize = 1,
	.native_blocksize = CHACHA20_NATIVE_BLOCK_LEN,
	.ivsize = CHACHA20_POLY1305_IV_LEN,
	.minkey = CHACHA20_POLY1305_KEY,
	.maxkey = CHACHA20_POLY1305_KEY,
	.macsize = POLY1305_HASH_LEN,
	.encrypt = chacha20_poly1305_crypt,
	.decrypt = chacha20_poly1305_crypt,
	.setkey = chacha20_poly1305_setkey,
	.reinit = chacha20_poly1305_reinit,
	.encrypt_last = chacha20_poly1305_crypt_last,
	.decrypt_last = chacha20_poly1305_crypt_last,
	.update = chacha20_poly1305_update,
	.final = chacha20_poly1305_final,
};

static int
xchacha20_poly1305_setkey(void *vctx, const uint8_t *key, int len)
{
	struct xchacha20_poly1305_ctx *ctx = vctx;

	if (len != XCHACHA20_POLY1305_KEY)
		return (EINVAL);

	ctx->key = key;
	ctx->base_ctx.key = ctx->derived_key;
	return (0);
}

static void
xchacha20_poly1305_reinit(void *vctx, const uint8_t *iv, size_t ivlen)
{
	struct xchacha20_poly1305_ctx *ctx = vctx;
	char nonce[CHACHA20_POLY1305_IV_LEN];

	KASSERT(ivlen == XCHACHA20_POLY1305_IV_LEN,
	    ("%s: invalid nonce length", __func__));

	/*
	 * Use HChaCha20 to derive the internal key used for
	 * ChaCha20-Poly1305.
	 */
	crypto_core_hchacha20(ctx->derived_key, iv, ctx->key, NULL);

	memset(nonce, 0, 4);
	memcpy(nonce + 4, iv + crypto_core_hchacha20_INPUTBYTES,
	    sizeof(nonce) - 4);
	chacha20_poly1305_reinit(&ctx->base_ctx, nonce, sizeof(nonce));
	explicit_bzero(nonce, sizeof(nonce));
}

const struct enc_xform enc_xform_xchacha20_poly1305 = {
	.type = CRYPTO_XCHACHA20_POLY1305,
	.name = "XChaCha20-Poly1305",
	.ctxsize = sizeof(struct xchacha20_poly1305_ctx),
	.blocksize = 1,
	.native_blocksize = CHACHA20_NATIVE_BLOCK_LEN,
	.ivsize = XCHACHA20_POLY1305_IV_LEN,
	.minkey = XCHACHA20_POLY1305_KEY,
	.maxkey = XCHACHA20_POLY1305_KEY,
	.macsize = POLY1305_HASH_LEN,
	.encrypt = chacha20_poly1305_crypt,
	.decrypt = chacha20_poly1305_crypt,
	.setkey = xchacha20_poly1305_setkey,
	.reinit = xchacha20_poly1305_reinit,
	.encrypt_last = chacha20_poly1305_crypt_last,
	.decrypt_last = chacha20_poly1305_crypt_last,
	.update = chacha20_poly1305_update,
	.final = chacha20_poly1305_final,
};
