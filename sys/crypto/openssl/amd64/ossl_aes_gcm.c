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
 * This file contains 2 AES-GCM wrapper implementations from OpenSSL, using
 * AES-NI and VAES extensions respectively.  These were ported from
 * cipher_aes_gcm_hw_aesni.inc and cipher_aes_gcm_hw_vaes_avx512.inc.  The
 * AES-NI implementation makes use of a generic C implementation for partial
 * blocks, ported from gcm128.c with OPENSSL_SMALL_FOOTPRINT defined.
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

size_t aesni_gcm_encrypt(const unsigned char *in, unsigned char *out, size_t len,
    const void *key, unsigned char ivec[16], uint64_t *Xi);
size_t aesni_gcm_decrypt(const unsigned char *in, unsigned char *out, size_t len,
    const void *key, unsigned char ivec[16], uint64_t *Xi);
void aesni_encrypt(const unsigned char *in, unsigned char *out, void *ks);
void aesni_ctr32_encrypt_blocks(const unsigned char *in, unsigned char *out,
    size_t blocks, void *ks, const unsigned char *iv);

void gcm_init_avx(__uint128_t Htable[16], uint64_t Xi[2]);
void gcm_gmult_avx(uint64_t Xi[2], const __uint128_t Htable[16]);
void gcm_ghash_avx(uint64_t Xi[2], const __uint128_t Htable[16], const void *in,
    size_t len);

static void
gcm_init_aesni(struct ossl_gcm_context *ctx, const void *key, size_t keylen)
{
	aesni_encrypt(ctx->gcm.H.c, ctx->gcm.H.c, &ctx->aes_ks);

#if BYTE_ORDER == LITTLE_ENDIAN
	ctx->gcm.H.u[0] = bswap64(ctx->gcm.H.u[0]);
	ctx->gcm.H.u[1] = bswap64(ctx->gcm.H.u[1]);
#endif

	gcm_init_avx(ctx->gcm.Htable, ctx->gcm.H.u);
}

static void
gcm_setiv_aesni(struct ossl_gcm_context *ctx, const unsigned char *iv,
    size_t len)
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

	aesni_encrypt(ctx->gcm.Yi.c, ctx->gcm.EK0.c, &ctx->aes_ks);
	ctr++;

#if BYTE_ORDER == LITTLE_ENDIAN
	ctx->gcm.Yi.d[3] = bswap32(ctr);
#else
	ctx->gcm.Yi.d[3] = ctr;
#endif
}

static int
gcm_aad_aesni(struct ossl_gcm_context *ctx, const unsigned char *aad,
    size_t len)
{
	size_t i;
	unsigned int n;
	uint64_t alen = ctx->gcm.len.u[0];

	if (ctx->gcm.len.u[1])
		return -2;

	alen += len;
	if (alen > (1ull << 61) || (sizeof(len) == 8 && alen < len))
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
			gcm_gmult_avx(ctx->gcm.Xi.u, ctx->gcm.Htable);
		else {
			ctx->gcm.ares = n;
			return 0;
		}
	}
	if ((i = (len & (size_t)-AES_BLOCK_LEN))) {
		gcm_ghash_avx(ctx->gcm.Xi.u, ctx->gcm.Htable, aad, i);
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
	unsigned int n, ctr, mres;
	size_t i;
	uint64_t mlen = ctx->gcm.len.u[1];

	mlen += len;
	if (mlen > ((1ull << 36) - 32) || (sizeof(len) == 8 && mlen < len))
		return -1;
	ctx->gcm.len.u[1] = mlen;

	mres = ctx->gcm.mres;

	if (ctx->gcm.ares) {
		/* First call to encrypt finalizes GHASH(AAD) */
		gcm_gmult_avx(ctx->gcm.Xi.u, ctx->gcm.Htable);
		ctx->gcm.ares = 0;
	}

#if BYTE_ORDER == LITTLE_ENDIAN
	ctr = bswap32(ctx->gcm.Yi.d[3]);
#else
	ctr = ctx->gcm.Yi.d[3];
#endif

	n = mres % 16;
	for (i = 0; i < len; ++i) {
		if (n == 0) {
			aesni_encrypt(ctx->gcm.Yi.c, ctx->gcm.EKi.c,
			    &ctx->aes_ks);
			++ctr;
#if BYTE_ORDER == LITTLE_ENDIAN
			ctx->gcm.Yi.d[3] = bswap32(ctr);
#else
			ctx->gcm.Yi.d[3] = ctr;
#endif
		}
		ctx->gcm.Xi.c[n] ^= out[i] = in[i] ^ ctx->gcm.EKi.c[n];
		mres = n = (n + 1) % 16;
		if (n == 0)
			gcm_gmult_avx(ctx->gcm.Xi.u, ctx->gcm.Htable);
	}

	ctx->gcm.mres = mres;
	return 0;
}

static int
gcm_encrypt_ctr32(struct ossl_gcm_context *ctx, const unsigned char *in,
    unsigned char *out, size_t len)
{
	unsigned int n, ctr, mres;
	size_t i;
	uint64_t mlen = ctx->gcm.len.u[1];

	mlen += len;
	if (mlen > ((1ull << 36) - 32) || (sizeof(len) == 8 && mlen < len))
		return -1;
	ctx->gcm.len.u[1] = mlen;

	mres = ctx->gcm.mres;

	if (ctx->gcm.ares) {
		/* First call to encrypt finalizes GHASH(AAD) */
		gcm_gmult_avx(ctx->gcm.Xi.u, ctx->gcm.Htable);
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
			gcm_gmult_avx(ctx->gcm.Xi.u, ctx->gcm.Htable);
			mres = 0;
		} else {
			ctx->gcm.mres = n;
			return 0;
		}
	}
	if ((i = (len & (size_t)-16))) {
		size_t j = i / 16;

		aesni_ctr32_encrypt_blocks(in, out, j, &ctx->aes_ks, ctx->gcm.Yi.c);
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
			gcm_gmult_avx(ctx->gcm.Xi.u, ctx->gcm.Htable);
			out += 16;
		}
	}
	if (len) {
		aesni_encrypt(ctx->gcm.Yi.c, ctx->gcm.EKi.c, &ctx->aes_ks);
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
gcm_encrypt_aesni(struct ossl_gcm_context *ctx, const unsigned char *in,
    unsigned char *out, size_t len)
{
	size_t bulk = 0, res;
	int error;

	res = (AES_BLOCK_LEN - ctx->gcm.mres) % AES_BLOCK_LEN;
	if ((error = gcm_encrypt(ctx, in, out, res)) != 0)
		return error;

	bulk = aesni_gcm_encrypt(in + res, out + res, len - res,
	    &ctx->aes_ks, ctx->gcm.Yi.c, ctx->gcm.Xi.u);
	ctx->gcm.len.u[1] += bulk;
	bulk += res;

	if ((error = gcm_encrypt_ctr32(ctx, in + bulk, out + bulk,
	    len - bulk)) != 0)
		return error;

	return 0;
}

static int
gcm_decrypt(struct ossl_gcm_context *ctx, const unsigned char *in,
    unsigned char *out, size_t len)
{
	unsigned int n, ctr, mres;
	size_t i;
	uint64_t mlen = ctx->gcm.len.u[1];

	mlen += len;
	if (mlen > ((1ull << 36) - 32) || (sizeof(len) == 8 && mlen < len))
		return -1;
	ctx->gcm.len.u[1] = mlen;

	mres = ctx->gcm.mres;

	if (ctx->gcm.ares) {
		/* First call to encrypt finalizes GHASH(AAD) */
		gcm_gmult_avx(ctx->gcm.Xi.u, ctx->gcm.Htable);
		ctx->gcm.ares = 0;
	}

#if BYTE_ORDER == LITTLE_ENDIAN
	ctr = bswap32(ctx->gcm.Yi.d[3]);
#else
	ctr = ctx->gcm.Yi.d[3];
#endif

	n = mres % 16;
	for (i = 0; i < len; ++i) {
		uint8_t c;
		if (n == 0) {
			aesni_encrypt(ctx->gcm.Yi.c, ctx->gcm.EKi.c,
			    &ctx->aes_ks);
			++ctr;
#if BYTE_ORDER == LITTLE_ENDIAN
			ctx->gcm.Yi.d[3] = bswap32(ctr);
#else
			ctx->gcm.Yi.d[3] = ctr;
#endif
		}
		c = in[i];
		out[i] = c ^ ctx->gcm.EKi.c[n];
		ctx->gcm.Xi.c[n] ^= c;
		mres = n = (n + 1) % 16;
		if (n == 0)
			gcm_gmult_avx(ctx->gcm.Xi.u, ctx->gcm.Htable);
	}

	ctx->gcm.mres = mres;
	return 0;
}

static int
gcm_decrypt_ctr32(struct ossl_gcm_context *ctx, const unsigned char *in,
    unsigned char *out, size_t len)
{
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
		gcm_gmult_avx(ctx->gcm.Xi.u, ctx->gcm.Htable);
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
			gcm_gmult_avx(ctx->gcm.Xi.u, ctx->gcm.Htable);
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
			gcm_gmult_avx(ctx->gcm.Xi.u, ctx->gcm.Htable);
			in += 16;
		}
		j = i / 16;
		in -= i;
		aesni_ctr32_encrypt_blocks(in, out, j, &ctx->aes_ks, ctx->gcm.Yi.c);
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
		aesni_encrypt(ctx->gcm.Yi.c, ctx->gcm.EKi.c, &ctx->aes_ks);
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

static int
gcm_decrypt_aesni(struct ossl_gcm_context *ctx, const unsigned char *in,
    unsigned char *out, size_t len)
{
	size_t bulk = 0, res;
	int error;

	res = (AES_BLOCK_LEN - ctx->gcm.mres) % AES_BLOCK_LEN;
	if ((error = gcm_decrypt(ctx, in, out, res)) != 0)
		return error;

	bulk = aesni_gcm_decrypt(in, out, len, &ctx->aes_ks, ctx->gcm.Yi.c,
	    ctx->gcm.Xi.u);
	ctx->gcm.len.u[1] += bulk;
	bulk += res;

	if ((error = gcm_decrypt_ctr32(ctx, in + bulk, out + bulk, len - bulk)) != 0)
		return error;

	return 0;
}

static int
gcm_finish_aesni(struct ossl_gcm_context *ctx, const unsigned char *tag,
    size_t len)
{
	uint64_t alen = ctx->gcm.len.u[0] << 3;
	uint64_t clen = ctx->gcm.len.u[1] << 3;

	if (ctx->gcm.mres || ctx->gcm.ares)
		gcm_gmult_avx(ctx->gcm.Xi.u, ctx->gcm.Htable);

#if BYTE_ORDER == LITTLE_ENDIAN
	alen = bswap64(alen);
	clen = bswap64(clen);
#endif

	ctx->gcm.Xi.u[0] ^= alen;
	ctx->gcm.Xi.u[1] ^= clen;
	gcm_gmult_avx(ctx->gcm.Xi.u, ctx->gcm.Htable);

	ctx->gcm.Xi.u[0] ^= ctx->gcm.EK0.u[0];
	ctx->gcm.Xi.u[1] ^= ctx->gcm.EK0.u[1];

	if (tag != NULL)
		return timingsafe_bcmp(ctx->gcm.Xi.c, tag, len);
	return 0;
}

static const struct ossl_aes_gcm_ops gcm_ops_aesni = {
	.init = gcm_init_aesni,
	.setiv = gcm_setiv_aesni,
	.aad = gcm_aad_aesni,
	.encrypt = gcm_encrypt_aesni,
	.decrypt = gcm_decrypt_aesni,
	.finish = gcm_finish_aesni,
	.tag = gcm_tag,
};

int ossl_aes_gcm_setkey_aesni(const unsigned char *key, int klen, void *_ctx);

int
ossl_aes_gcm_setkey_aesni(const unsigned char *key, int klen,
    void *_ctx)
{
	struct ossl_gcm_context *ctx;

	ctx = _ctx;
	ctx->ops = &gcm_ops_aesni;
	gcm_init(ctx, key, klen);
	return (0);
}

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
