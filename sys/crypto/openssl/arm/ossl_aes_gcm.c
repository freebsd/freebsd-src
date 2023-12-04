/*
 * Copyright 2010-2022 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

#include <sys/types.h>
#include <sys/endian.h>
#include <sys/systm.h>

#include <crypto/openssl/ossl.h>
#include <crypto/openssl/ossl_arm.h>
#include <crypto/openssl/ossl_aes_gcm.h>
#include <crypto/openssl/ossl_cipher.h>
#include <crypto/openssl/arm_arch.h>

#include <opencrypto/cryptodev.h>

_Static_assert(
    sizeof(struct ossl_gcm_context) <= sizeof(struct ossl_cipher_context),
    "ossl_gcm_context too large");

void AES_encrypt(const void *in, void *out, const void *ks);
void AES_set_encrypt_key(const void *key, int keylen, void *ks);

void gcm_init_neon(__uint128_t Htable[16], const uint64_t Xi[2]);
void gcm_gmult_neon(uint64_t Xi[2], const __uint128_t Htable[16]);
void gcm_ghash_neon(uint64_t Xi[2], const __uint128_t Htable[16],
    const void *in, size_t len);

void ossl_bsaes_ctr32_encrypt_blocks(const unsigned char *in,
    unsigned char *out, size_t blocks, void *ks, const unsigned char *iv);

static void
gcm_init(struct ossl_gcm_context *ctx, const void *key, size_t keylen)
{
	memset(&ctx->gcm, 0, sizeof(ctx->gcm));
	memset(&ctx->aes_ks, 0, sizeof(ctx->aes_ks));

	AES_set_encrypt_key(key, keylen, &ctx->aes_ks);
	AES_encrypt(ctx->gcm.H.c, ctx->gcm.H.c, &ctx->aes_ks);

#if BYTE_ORDER == LITTLE_ENDIAN
	ctx->gcm.H.u[0] = bswap64(ctx->gcm.H.u[0]);
	ctx->gcm.H.u[1] = bswap64(ctx->gcm.H.u[1]);
#endif

	gcm_init_neon(ctx->gcm.Htable, ctx->gcm.H.u);
}

static void
gcm_setiv(struct ossl_gcm_context *ctx, const unsigned char *iv, size_t len)
{
	uint32_t ctr;

	KASSERT(len == AES_GCM_IV_LEN,
	    ("%s: invalid IV length %zu", __func__, len));

	ctx->gcm.len.u[0] = 0;
	ctx->gcm.len.u[1] = 0;
	ctx->gcm.ares = ctx->gcm.mres = 0;

	memcpy(ctx->gcm.Yi.c, iv, len);
	ctx->gcm.Yi.c[12] = 0;
	ctx->gcm.Yi.c[13] = 0;
	ctx->gcm.Yi.c[14] = 0;
	ctx->gcm.Yi.c[15] = 1;
	ctr = 1;

	ctx->gcm.Xi.u[0] = 0;
	ctx->gcm.Xi.u[1] = 0;

	AES_encrypt(ctx->gcm.Yi.c, ctx->gcm.EK0.c, &ctx->aes_ks);
	ctr++;

#if BYTE_ORDER == LITTLE_ENDIAN
	ctx->gcm.Yi.d[3] = bswap32(ctr);
#else
	ctx->gcm.Yi.d[3] = ctr;
#endif
}

static int
gcm_finish(struct ossl_gcm_context *ctx, const unsigned char *tag, size_t len)
{
	uint64_t alen = ctx->gcm.len.u[0] << 3;
	uint64_t clen = ctx->gcm.len.u[1] << 3;

	if (ctx->gcm.mres || ctx->gcm.ares)
		gcm_gmult_neon(ctx->gcm.Xi.u, ctx->gcm.Htable);

#if BYTE_ORDER == LITTLE_ENDIAN
	alen = bswap64(alen);
	clen = bswap64(clen);
#endif

	ctx->gcm.Xi.u[0] ^= alen;
	ctx->gcm.Xi.u[1] ^= clen;
	gcm_gmult_neon(ctx->gcm.Xi.u, ctx->gcm.Htable);

	ctx->gcm.Xi.u[0] ^= ctx->gcm.EK0.u[0];
	ctx->gcm.Xi.u[1] ^= ctx->gcm.EK0.u[1];

	if (tag != NULL)
		return timingsafe_bcmp(ctx->gcm.Xi.c, tag, len);
	return 0;
}

static int
gcm_aad(struct ossl_gcm_context *ctx, const unsigned char *aad, size_t len)
{
	size_t i;
	unsigned int n;
	uint64_t alen = ctx->gcm.len.u[0];

	if (ctx->gcm.len.u[1])
		return -2;

	alen += len;
	if (alen > ((uint64_t)1 << 61) || (sizeof(len) == 8 && alen < len))
		return -1;
	ctx->gcm.len.u[0] = alen;

	n = ctx->gcm.ares;
	if (n) {
		while (n && len) {
			ctx->gcm.Xi.c[n] ^= *(aad++);
			--len;
			n = (n + 1) % 16;
		}
		if (n == 0)
			gcm_gmult_neon(ctx->gcm.Xi.u, ctx->gcm.Htable);
		else {
			ctx->gcm.ares = n;
			return 0;
		}
	}
	if ((i = (len & (size_t)-AES_BLOCK_LEN))) {
		gcm_ghash_neon(ctx->gcm.Xi.u, ctx->gcm.Htable, aad, i);
		aad += i;
		len -= i;
	}
	if (len) {
		n = (unsigned int)len;
		for (i = 0; i < len; ++i)
			ctx->gcm.Xi.c[i] ^= aad[i];
	}

	ctx->gcm.ares = n;
	return 0;
}

static int
gcm_encrypt(struct ossl_gcm_context *ctx, const unsigned char *in,
    unsigned char *out, size_t len)
{
	struct bsaes_key bsks;
	unsigned int n, ctr, mres;
	size_t i;
	uint64_t mlen = ctx->gcm.len.u[1];

	mlen += len;
	if (mlen > (((uint64_t)1 << 36) - 32) ||
	    (sizeof(len) == 8 && mlen < len))
		return -1;
	ctx->gcm.len.u[1] = mlen;

	mres = ctx->gcm.mres;

	if (ctx->gcm.ares) {
		/* First call to encrypt finalizes GHASH(AAD) */
		gcm_gmult_neon(ctx->gcm.Xi.u, ctx->gcm.Htable);
		ctx->gcm.ares = 0;
	}

#if BYTE_ORDER == LITTLE_ENDIAN
	ctr = bswap32(ctx->gcm.Yi.d[3]);
#else
	ctr = ctx->gcm.Yi.d[3];
#endif

	n = mres % 16;
	if (n) {
		while (n && len) {
			ctx->gcm.Xi.c[n] ^= *(out++) = *(in++) ^ ctx->gcm.EKi.c[n];
			--len;
			n = (n + 1) % 16;
		}
		if (n == 0) {
			gcm_gmult_neon(ctx->gcm.Xi.u, ctx->gcm.Htable);
			mres = 0;
		} else {
			ctx->gcm.mres = n;
			return 0;
		}
	}
	if ((i = (len & (size_t)-16))) {
		size_t j = i / 16;

		memcpy(&bsks.ks, &ctx->aes_ks, sizeof(bsks.ks));
		bsks.converted = 0;
		ossl_bsaes_ctr32_encrypt_blocks(in, out, j, &bsks,
		    ctx->gcm.Yi.c);
		ctr += (unsigned int)j;
#if BYTE_ORDER == LITTLE_ENDIAN
		ctx->gcm.Yi.d[3] = bswap32(ctr);
#else
		ctx->gcm.Yi.d[3] = ctr;
#endif
		in += i;
		len -= i;
		while (j--) {
			for (i = 0; i < 16; ++i)
				ctx->gcm.Xi.c[i] ^= out[i];
			gcm_gmult_neon(ctx->gcm.Xi.u, ctx->gcm.Htable);
			out += 16;
		}
	}
	if (len) {
		AES_encrypt(ctx->gcm.Yi.c, ctx->gcm.EKi.c, &ctx->aes_ks);
		++ctr;
#if BYTE_ORDER == LITTLE_ENDIAN
		ctx->gcm.Yi.d[3] = bswap32(ctr);
#else
		ctx->gcm.Yi.d[3] = ctr;
#endif
		while (len--) {
			ctx->gcm.Xi.c[mres++] ^= out[n] = in[n] ^ ctx->gcm.EKi.c[n];
			++n;
		}
	}

	ctx->gcm.mres = mres;
	return 0;
}

static int
gcm_decrypt(struct ossl_gcm_context *ctx, const unsigned char *in,
    unsigned char *out, size_t len)
{
	struct bsaes_key bsks;
	unsigned int n, ctr, mres;
	size_t i;
	uint64_t mlen = ctx->gcm.len.u[1];

	mlen += len;
	if (mlen > ((1ull << 36) - 32) || (sizeof(len) == 8 && mlen < len))
		return -1;
	ctx->gcm.len.u[1] = mlen;

	mres = ctx->gcm.mres;

	if (ctx->gcm.ares) {
		/* First call to decrypt finalizes GHASH(AAD) */
		gcm_gmult_neon(ctx->gcm.Xi.u, ctx->gcm.Htable);
		ctx->gcm.ares = 0;
	}

#if BYTE_ORDER == LITTLE_ENDIAN
	ctr = bswap32(ctx->gcm.Yi.d[3]);
#else
	ctr = ctx->gcm.Yi.d[3];
#endif

	n = mres % 16;
	if (n) {
		while (n && len) {
			uint8_t c = *(in++);
			*(out++) = c ^ ctx->gcm.EKi.c[n];
			ctx->gcm.Xi.c[n] ^= c;
			--len;
			n = (n + 1) % 16;
		}
		if (n == 0) {
			gcm_gmult_neon(ctx->gcm.Xi.u, ctx->gcm.Htable);
			mres = 0;
		} else {
			ctx->gcm.mres = n;
			return 0;
		}
	}
	if ((i = (len & (size_t)-16))) {
		size_t j = i / 16;

		while (j--) {
			size_t k;
			for (k = 0; k < 16; ++k)
				ctx->gcm.Xi.c[k] ^= in[k];
			gcm_gmult_neon(ctx->gcm.Xi.u, ctx->gcm.Htable);
			in += 16;
		}
		j = i / 16;
		in -= i;
		memcpy(&bsks.ks, &ctx->aes_ks, sizeof(bsks.ks));
		bsks.converted = 0;
		ossl_bsaes_ctr32_encrypt_blocks(in, out, j, &bsks,
		    ctx->gcm.Yi.c);
		ctr += (unsigned int)j;
#if BYTE_ORDER == LITTLE_ENDIAN
		ctx->gcm.Yi.d[3] = bswap32(ctr);
#else
		ctx->gcm.Yi.d[3] = ctr;
#endif
		out += i;
		in += i;
		len -= i;
	}
	if (len) {
		AES_encrypt(ctx->gcm.Yi.c, ctx->gcm.EKi.c, &ctx->aes_ks);
		++ctr;
#if BYTE_ORDER == LITTLE_ENDIAN
		ctx->gcm.Yi.d[3] = bswap32(ctr);
#else
		ctx->gcm.Yi.d[3] = ctr;
#endif
		while (len--) {
			uint8_t c = in[n];
			ctx->gcm.Xi.c[mres++] ^= c;
			out[n] = c ^ ctx->gcm.EKi.c[n];
			++n;
		}
	}

	ctx->gcm.mres = mres;
	return 0;
}

static void
gcm_tag(struct ossl_gcm_context *ctx, unsigned char *tag, size_t len)
{
	gcm_finish(ctx, NULL, 0);
	memcpy(tag, ctx->gcm.Xi.c, len);
}

static const struct ossl_aes_gcm_ops gcm_ops_neon = {
	.init = gcm_init,
	.setiv = gcm_setiv,
	.aad = gcm_aad,
	.encrypt = gcm_encrypt,
	.decrypt = gcm_decrypt,
	.finish = gcm_finish,
	.tag = gcm_tag,
};

int ossl_aes_gcm_setkey(const unsigned char *key, int klen, void *_ctx);

int
ossl_aes_gcm_setkey(const unsigned char *key, int klen, void *_ctx)
{
	struct ossl_gcm_context *ctx;

	ctx = _ctx;
	ctx->ops = &gcm_ops_neon;
	gcm_init(ctx, key, klen);
	return (0);
}
