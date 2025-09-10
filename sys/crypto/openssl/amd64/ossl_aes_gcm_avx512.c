/*
 * Copyright 2010-2022 The OpenSSL Project Authors. All Rights Reserved.
 * Copyright (c) 2021, Intel Corporation. All Rights Reserved.
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

/*
 * This file contains an AES-GCM wrapper implementation from OpenSSL, using
 * VAES extensions. It was ported from cipher_aes_gcm_hw_vaes_avx512.inc.
 */

#include <sys/endian.h>
#include <sys/systm.h>

#include <crypto/openssl/ossl.h>
#include <crypto/openssl/ossl_aes_gcm.h>
#include <crypto/openssl/ossl_cipher.h>

#include <opencrypto/cryptodev.h>

_Static_assert(
    sizeof(struct ossl_gcm_context) <= sizeof(struct ossl_cipher_context),
    "ossl_gcm_context too large");

void aesni_set_encrypt_key(const void *key, int bits, void *ctx);

static void
gcm_init(struct ossl_gcm_context *ctx, const void *key, size_t keylen)
{
	KASSERT(keylen == 128 || keylen == 192 || keylen == 256,
	    ("%s: invalid key length %zu", __func__, keylen));

	memset(&ctx->gcm, 0, sizeof(ctx->gcm));
	memset(&ctx->aes_ks, 0, sizeof(ctx->aes_ks));
	aesni_set_encrypt_key(key, keylen, &ctx->aes_ks);
	ctx->ops->init(ctx, key, keylen);
}

static void
gcm_tag(struct ossl_gcm_context *ctx, unsigned char *tag, size_t len)
{
	(void)ctx->ops->finish(ctx, NULL, 0);
	memcpy(tag, ctx->gcm.Xi.c, len);
}

void ossl_gcm_gmult_avx512(uint64_t Xi[2], void *gcm128ctx);
void ossl_aes_gcm_init_avx512(const void *ks, void *gcm128ctx);
void ossl_aes_gcm_setiv_avx512(const void *ks, void *gcm128ctx,
    const unsigned char *iv, size_t ivlen);
void ossl_aes_gcm_update_aad_avx512(void *gcm128ctx, const unsigned char *aad,
    size_t len);
void ossl_aes_gcm_encrypt_avx512(const void *ks, void *gcm128ctx,
    unsigned int *pblocklen, const unsigned char *in, size_t len,
    unsigned char *out);
void ossl_aes_gcm_decrypt_avx512(const void *ks, void *gcm128ctx,
    unsigned int *pblocklen, const unsigned char *in, size_t len,
    unsigned char *out);
void ossl_aes_gcm_finalize_avx512(void *gcm128ctx, unsigned int pblocklen);

static void
gcm_init_avx512(struct ossl_gcm_context *ctx, const void *key, size_t keylen)
{
	ossl_aes_gcm_init_avx512(&ctx->aes_ks, &ctx->gcm);
}

static void
gcm_setiv_avx512(struct ossl_gcm_context *ctx, const unsigned char *iv,
    size_t len)
{
	KASSERT(len == AES_GCM_IV_LEN,
	    ("%s: invalid IV length %zu", __func__, len));

	ctx->gcm.Yi.u[0] = 0;		/* Current counter */
	ctx->gcm.Yi.u[1] = 0;
	ctx->gcm.Xi.u[0] = 0;		/* AAD hash */
	ctx->gcm.Xi.u[1] = 0;
	ctx->gcm.len.u[0] = 0;		/* AAD length */
	ctx->gcm.len.u[1] = 0;		/* Message length */
	ctx->gcm.ares = 0;
	ctx->gcm.mres = 0;

	ossl_aes_gcm_setiv_avx512(&ctx->aes_ks, ctx, iv, len);
}

static int
gcm_aad_avx512(struct ossl_gcm_context *ctx, const unsigned char *aad,
    size_t len)
{
	uint64_t alen = ctx->gcm.len.u[0];
	size_t lenblks;
	unsigned int ares;

	/* Bad sequence: call of AAD update after message processing */
	if (ctx->gcm.len.u[1])
		return -2;

	alen += len;
	/* AAD is limited by 2^64 bits, thus 2^61 bytes */
	if (alen > (1ull << 61) || (sizeof(len) == 8 && alen < len))
		return -1;
	ctx->gcm.len.u[0] = alen;

	ares = ctx->gcm.ares;
	/* Partial AAD block left from previous AAD update calls */
	if (ares > 0) {
		/*
		 * Fill partial block buffer till full block
		 * (note, the hash is stored reflected)
		 */
		while (ares > 0 && len > 0) {
			ctx->gcm.Xi.c[15 - ares] ^= *(aad++);
			--len;
			ares = (ares + 1) % AES_BLOCK_LEN;
		}
		/* Full block gathered */
		if (ares == 0) {
			ossl_gcm_gmult_avx512(ctx->gcm.Xi.u, ctx);
		} else { /* no more AAD */
			ctx->gcm.ares = ares;
			return 0;
		}
	}

	/* Bulk AAD processing */
	lenblks = len & ((size_t)(-AES_BLOCK_LEN));
	if (lenblks > 0) {
		ossl_aes_gcm_update_aad_avx512(ctx, aad, lenblks);
		aad += lenblks;
		len -= lenblks;
	}

	/* Add remaining AAD to the hash (note, the hash is stored reflected) */
	if (len > 0) {
		ares = (unsigned int)len;
		for (size_t i = 0; i < len; ++i)
			ctx->gcm.Xi.c[15 - i] ^= aad[i];
	}

	ctx->gcm.ares = ares;

	return 0;
}

static int
_gcm_encrypt_avx512(struct ossl_gcm_context *ctx, const unsigned char *in,
    unsigned char *out, size_t len, bool encrypt)
{
	uint64_t mlen = ctx->gcm.len.u[1];

	mlen += len;
	if (mlen > ((1ull << 36) - 32) || (sizeof(len) == 8 && mlen < len))
		return -1;

	ctx->gcm.len.u[1] = mlen;

	/* Finalize GHASH(AAD) if AAD partial blocks left unprocessed */
	if (ctx->gcm.ares > 0) {
		ossl_gcm_gmult_avx512(ctx->gcm.Xi.u, ctx);
		ctx->gcm.ares = 0;
	}

	if (encrypt) {
		ossl_aes_gcm_encrypt_avx512(&ctx->aes_ks, ctx, &ctx->gcm.mres,
		    in, len, out);
	} else {
		ossl_aes_gcm_decrypt_avx512(&ctx->aes_ks, ctx, &ctx->gcm.mres,
		    in, len, out);
	}

	return 0;
}

static int
gcm_encrypt_avx512(struct ossl_gcm_context *ctx, const unsigned char *in,
    unsigned char *out, size_t len)
{
	return _gcm_encrypt_avx512(ctx, in, out, len, true);
}

static int
gcm_decrypt_avx512(struct ossl_gcm_context *ctx, const unsigned char *in,
    unsigned char *out, size_t len)
{
	return _gcm_encrypt_avx512(ctx, in, out, len, false);
}

static int
gcm_finish_avx512(struct ossl_gcm_context *ctx, const unsigned char *tag,
    size_t len)
{
	unsigned int *res = &ctx->gcm.mres;

	/* Finalize AAD processing */
	if (ctx->gcm.ares > 0)
		res = &ctx->gcm.ares;

	ossl_aes_gcm_finalize_avx512(ctx, *res);

	ctx->gcm.ares = ctx->gcm.mres = 0;

	if (tag != NULL)
		return timingsafe_bcmp(ctx->gcm.Xi.c, tag, len);
	return 0;
}

static const struct ossl_aes_gcm_ops gcm_ops_avx512 = {
	.init = gcm_init_avx512,
	.setiv = gcm_setiv_avx512,
	.aad = gcm_aad_avx512,
	.encrypt = gcm_encrypt_avx512,
	.decrypt = gcm_decrypt_avx512,
	.finish = gcm_finish_avx512,
	.tag = gcm_tag,
};

int ossl_aes_gcm_setkey_avx512(const unsigned char *key, int klen, void *_ctx);

int
ossl_aes_gcm_setkey_avx512(const unsigned char *key, int klen,
    void *_ctx)
{
	struct ossl_gcm_context *ctx;

	ctx = _ctx;
	ctx->ops = &gcm_ops_avx512;
	gcm_init(ctx, key, klen);
	return (0);
}
