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

#include <sodium/crypto_onetimeauth_poly1305.h>
#include <sodium/crypto_stream_chacha20.h>

struct chacha20_poly1305_cipher_ctx {
	const void *key;
	uint32_t ic;
	bool ietf;
	char nonce[CHACHA20_POLY1305_IV_LEN];
};

static int
chacha20_poly1305_setkey(void *vctx, const uint8_t *key, int len)
{
	struct chacha20_poly1305_cipher_ctx *ctx = vctx;

	if (len != CHACHA20_POLY1305_KEY)
		return (EINVAL);

	ctx->key = key;
	return (0);
}

static void
chacha20_poly1305_reinit(void *vctx, const uint8_t *iv, size_t ivlen)
{
	struct chacha20_poly1305_cipher_ctx *ctx = vctx;

	KASSERT(ivlen == 8 || ivlen == sizeof(ctx->nonce),
	    ("%s: invalid nonce length", __func__));

	/* Block 0 is used for the poly1305 key. */
	memcpy(ctx->nonce, iv, ivlen);
	ctx->ietf = (ivlen == CHACHA20_POLY1305_IV_LEN);
	ctx->ic = 1;
}

static void
chacha20_poly1305_crypt(void *vctx, const uint8_t *in, uint8_t *out)
{
	struct chacha20_poly1305_cipher_ctx *ctx = vctx;
	int error;

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
	struct chacha20_poly1305_cipher_ctx *ctx = vctx;

	int error;

	if (ctx->ietf)
		error = crypto_stream_chacha20_ietf_xor_ic(out, in, len,
		    ctx->nonce, ctx->ic, ctx->key);
	else
		error = crypto_stream_chacha20_xor_ic(out, in, len, ctx->nonce,
		    ctx->ic, ctx->key);
	KASSERT(error == 0, ("%s failed: %d", __func__, error));
}

struct enc_xform enc_xform_chacha20_poly1305 = {
	.type = CRYPTO_CHACHA20_POLY1305,
	.name = "ChaCha20-Poly1305",
	.ctxsize = sizeof(struct chacha20_poly1305_cipher_ctx),
	.blocksize = 1,
	.native_blocksize = CHACHA20_NATIVE_BLOCK_LEN,
	.ivsize = CHACHA20_POLY1305_IV_LEN,
	.minkey = CHACHA20_POLY1305_KEY,
	.maxkey = CHACHA20_POLY1305_KEY,
	.encrypt = chacha20_poly1305_crypt,
	.decrypt = chacha20_poly1305_crypt,
	.setkey = chacha20_poly1305_setkey,
	.reinit = chacha20_poly1305_reinit,
	.encrypt_last = chacha20_poly1305_crypt_last,
	.decrypt_last = chacha20_poly1305_crypt_last,
};

struct chacha20_poly1305_auth_ctx {
	struct crypto_onetimeauth_poly1305_state state;
	const void *key;
};
CTASSERT(sizeof(union authctx) >= sizeof(struct chacha20_poly1305_auth_ctx));

static void
chacha20_poly1305_Init(void *vctx)
{
}

static void
chacha20_poly1305_Setkey(void *vctx, const uint8_t *key, u_int klen)
{
	struct chacha20_poly1305_auth_ctx *ctx = vctx;

	ctx->key = key;
}

static void
chacha20_poly1305_Reinit(void *vctx, const uint8_t *nonce, u_int noncelen)
{
	struct chacha20_poly1305_auth_ctx *ctx = vctx;
	char block[CHACHA20_NATIVE_BLOCK_LEN];

	switch (noncelen) {
	case 8:
		crypto_stream_chacha20(block, sizeof(block), nonce, ctx->key);
		break;
	case CHACHA20_POLY1305_IV_LEN:
		crypto_stream_chacha20_ietf(block, sizeof(block), nonce, ctx->key);
		break;
	default:
		__assert_unreachable();
	}
	crypto_onetimeauth_poly1305_init(&ctx->state, block);
	explicit_bzero(block, sizeof(block));
}

static int
chacha20_poly1305_Update(void *vctx, const void *data, u_int len)
{
	struct chacha20_poly1305_auth_ctx *ctx = vctx;

	crypto_onetimeauth_poly1305_update(&ctx->state, data, len);
	return (0);
}

static void
chacha20_poly1305_Final(uint8_t *digest, void *vctx)
{
	struct chacha20_poly1305_auth_ctx *ctx = vctx;

	crypto_onetimeauth_poly1305_final(&ctx->state, digest);
}

struct auth_hash auth_hash_chacha20_poly1305 = {
	.type = CRYPTO_POLY1305,
	.name = "ChaCha20-Poly1305",
	.keysize = POLY1305_KEY_LEN,
	.hashsize = POLY1305_HASH_LEN,
	.ctxsize = sizeof(struct chacha20_poly1305_auth_ctx),
	.blocksize = crypto_onetimeauth_poly1305_BYTES,
	.Init = chacha20_poly1305_Init,
	.Setkey = chacha20_poly1305_Setkey,
	.Reinit = chacha20_poly1305_Reinit,
	.Update = chacha20_poly1305_Update,
	.Final = chacha20_poly1305_Final,
};
