/*
 * Copyright 2010-2022 The OpenSSL Project Authors. All Rights Reserved.
 * Copyright (c) 2021, Intel Corporation. All Rights Reserved.
 * Copyright (c) 2023, Raptor Engineering, LLC. All Rights Reserved.
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

/*
 * This file contains an AES-GCM wrapper implementation from OpenSSL, using
 * AES-NI (x86) or POWER8 Crypto Extensions (ppc). It was ported from
 * cipher_aes_gcm_hw_aesni.inc and it makes use of a generic C implementation
 * for partial blocks, ported from gcm128.c with OPENSSL_SMALL_FOOTPRINT defined.
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

#if defined(__amd64__) || defined(__i386__)
#define	AES_set_encrypt_key	aesni_set_encrypt_key
#define	AES_gcm_encrypt	aesni_gcm_encrypt
#define	AES_gcm_decrypt	aesni_gcm_decrypt
#define	AES_encrypt	aesni_encrypt
#define	AES_ctr32_encrypt_blocks	aesni_ctr32_encrypt_blocks
#define	GCM_init 	gcm_init_avx
#define	GCM_gmult	gcm_gmult_avx
#define	GCM_ghash	gcm_ghash_avx

void AES_set_encrypt_key(const void *key, int bits, void *ctx);
size_t AES_gcm_encrypt(const unsigned char *in, unsigned char *out, size_t len,
    const void *key, unsigned char ivec[16], uint64_t *Xi);
size_t AES_gcm_decrypt(const unsigned char *in, unsigned char *out, size_t len,
    const void *key, unsigned char ivec[16], uint64_t *Xi);
void AES_encrypt(const unsigned char *in, unsigned char *out, void *ks);
void AES_ctr32_encrypt_blocks(const unsigned char *in, unsigned char *out,
    size_t blocks, void *ks, const unsigned char *iv);

void GCM_init(__uint128_t Htable[16], uint64_t Xi[2]);
void GCM_gmult(uint64_t Xi[2], const __uint128_t Htable[16]);
void GCM_ghash(uint64_t Xi[2], const __uint128_t Htable[16], const void *in,
    size_t len);

#elif defined(__powerpc64__)
#define	AES_set_encrypt_key	aes_p8_set_encrypt_key
#define AES_gcm_encrypt(i,o,l,k,v,x) 	ppc_aes_gcm_crypt(i,o,l,k,v,x,1)
#define AES_gcm_decrypt(i,o,l,k,v,x) 	ppc_aes_gcm_crypt(i,o,l,k,v,x,0)
#define	AES_encrypt	aes_p8_encrypt
#define	AES_ctr32_encrypt_blocks	aes_p8_ctr32_encrypt_blocks
#define	GCM_init	gcm_init_p8
#define	GCM_gmult	gcm_gmult_p8
#define	GCM_ghash	gcm_ghash_p8

size_t ppc_aes_gcm_encrypt(const unsigned char *in, unsigned char *out, size_t len,
    const void *key, unsigned char ivec[16], uint64_t *Xi);
size_t ppc_aes_gcm_decrypt(const unsigned char *in, unsigned char *out, size_t len,
    const void *key, unsigned char ivec[16], uint64_t *Xi);

void AES_set_encrypt_key(const void *key, int bits, void *ctx);
void AES_encrypt(const unsigned char *in, unsigned char *out, void *ks);
void AES_ctr32_encrypt_blocks(const unsigned char *in, unsigned char *out,
    size_t blocks, void *ks, const unsigned char *iv);

void GCM_init(__uint128_t Htable[16], uint64_t Xi[2]);
void GCM_gmult(uint64_t Xi[2], const __uint128_t Htable[16]);
void GCM_ghash(uint64_t Xi[2], const __uint128_t Htable[16], const void *in,
    size_t len);

static size_t
ppc_aes_gcm_crypt(const unsigned char *in, unsigned char *out,
    size_t len, const void *key, unsigned char ivec_[16], uint64_t *Xi,
    int encrypt)
{
	union {
		uint32_t d[4];
		uint8_t c[16];
	} *ivec = (void *)ivec_;
	int s = 0;
	int ndone = 0;
	int ctr_reset = 0;
	uint32_t ivec_val;
	uint64_t blocks_unused;
	uint64_t nb = len / 16;
	uint64_t next_ctr = 0;
	unsigned char ctr_saved[12];

	memcpy(ctr_saved, ivec, 12);

	while (nb) {
		ivec_val = ivec->d[3];
#if BYTE_ORDER == LITTLE_ENDIAN
		ivec_val = bswap32(ivec_val);
#endif

		blocks_unused = (uint64_t)0xffffffffU + 1 - (uint64_t)ivec_val;
		if (nb > blocks_unused) {
			len = blocks_unused * 16;
			nb -= blocks_unused;
			next_ctr = blocks_unused;
			ctr_reset = 1;
		} else {
			len = nb * 16;
			next_ctr = nb;
			nb = 0;
		}

		s = encrypt ? ppc_aes_gcm_encrypt(in, out, len, key, ivec->c, Xi) :
		    ppc_aes_gcm_decrypt(in, out, len, key, ivec->c, Xi);

		/* add counter to ivec */
#if BYTE_ORDER == LITTLE_ENDIAN
		ivec->d[3] = bswap32(ivec_val + next_ctr);
#else
		ivec->d[3] += next_ctr;
#endif
		if (ctr_reset) {
			ctr_reset = 0;
			in += len;
			out += len;
		}
		memcpy(ivec, ctr_saved, 12);
		ndone += s;
	}

	return ndone;
}

#else
#error "Unsupported architecture!"
#endif

static void
gcm_init(struct ossl_gcm_context *ctx, const void *key, size_t keylen)
{
	KASSERT(keylen == 128 || keylen == 192 || keylen == 256,
	    ("%s: invalid key length %zu", __func__, keylen));

	memset(&ctx->gcm, 0, sizeof(ctx->gcm));
	memset(&ctx->aes_ks, 0, sizeof(ctx->aes_ks));
	AES_set_encrypt_key(key, keylen, &ctx->aes_ks);
	ctx->ops->init(ctx, key, keylen);
}

static void
gcm_tag_op(struct ossl_gcm_context *ctx, unsigned char *tag, size_t len)
{
	(void)ctx->ops->finish(ctx, NULL, 0);
	memcpy(tag, ctx->gcm.Xi.c, len);
}

static void
gcm_init_op(struct ossl_gcm_context *ctx, const void *key, size_t keylen)
{
	AES_encrypt(ctx->gcm.H.c, ctx->gcm.H.c, &ctx->aes_ks);

#if BYTE_ORDER == LITTLE_ENDIAN
	ctx->gcm.H.u[0] = bswap64(ctx->gcm.H.u[0]);
	ctx->gcm.H.u[1] = bswap64(ctx->gcm.H.u[1]);
#endif

	GCM_init(ctx->gcm.Htable, ctx->gcm.H.u);
}

static void
gcm_setiv_op(struct ossl_gcm_context *ctx, const unsigned char *iv,
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

	AES_encrypt(ctx->gcm.Yi.c, ctx->gcm.EK0.c, &ctx->aes_ks);
	ctr++;

#if BYTE_ORDER == LITTLE_ENDIAN
	ctx->gcm.Yi.d[3] = bswap32(ctr);
#else
	ctx->gcm.Yi.d[3] = ctr;
#endif
}

static int
gcm_aad_op(struct ossl_gcm_context *ctx, const unsigned char *aad,
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
			GCM_gmult(ctx->gcm.Xi.u, ctx->gcm.Htable);
		else {
			ctx->gcm.ares = n;
			return 0;
		}
	}
	if ((i = (len & (size_t)-AES_BLOCK_LEN))) {
		GCM_ghash(ctx->gcm.Xi.u, ctx->gcm.Htable, aad, i);
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
		GCM_gmult(ctx->gcm.Xi.u, ctx->gcm.Htable);
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
			AES_encrypt(ctx->gcm.Yi.c, ctx->gcm.EKi.c,
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
			GCM_gmult(ctx->gcm.Xi.u, ctx->gcm.Htable);
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
		GCM_gmult(ctx->gcm.Xi.u, ctx->gcm.Htable);
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
			GCM_gmult(ctx->gcm.Xi.u, ctx->gcm.Htable);
			mres = 0;
		} else {
			ctx->gcm.mres = n;
			return 0;
		}
	}
	if ((i = (len & (size_t)-16))) {
		size_t j = i / 16;

		AES_ctr32_encrypt_blocks(in, out, j, &ctx->aes_ks, ctx->gcm.Yi.c);
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
			GCM_gmult(ctx->gcm.Xi.u, ctx->gcm.Htable);
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
gcm_encrypt_op(struct ossl_gcm_context *ctx, const unsigned char *in,
    unsigned char *out, size_t len)
{
	size_t bulk = 0, res;
	int error;

	res = MIN(len, (AES_BLOCK_LEN - ctx->gcm.mres) % AES_BLOCK_LEN);
	if ((error = gcm_encrypt(ctx, in, out, res)) != 0)
		return error;

	bulk = AES_gcm_encrypt(in + res, out + res, len - res,
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
		GCM_gmult(ctx->gcm.Xi.u, ctx->gcm.Htable);
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
			AES_encrypt(ctx->gcm.Yi.c, ctx->gcm.EKi.c,
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
			GCM_gmult(ctx->gcm.Xi.u, ctx->gcm.Htable);
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
		GCM_gmult(ctx->gcm.Xi.u, ctx->gcm.Htable);
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
			GCM_gmult(ctx->gcm.Xi.u, ctx->gcm.Htable);
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
			GCM_gmult(ctx->gcm.Xi.u, ctx->gcm.Htable);
			in += 16;
		}
		j = i / 16;
		in -= i;
		AES_ctr32_encrypt_blocks(in, out, j, &ctx->aes_ks, ctx->gcm.Yi.c);
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

static int
gcm_decrypt_op(struct ossl_gcm_context *ctx, const unsigned char *in,
    unsigned char *out, size_t len)
{
	size_t bulk = 0, res;
	int error;

	res = MIN(len, (AES_BLOCK_LEN - ctx->gcm.mres) % AES_BLOCK_LEN);
	if ((error = gcm_decrypt(ctx, in, out, res)) != 0)
		return error;

	bulk = AES_gcm_decrypt(in + res, out + res, len - res, &ctx->aes_ks,
	    ctx->gcm.Yi.c, ctx->gcm.Xi.u);
	ctx->gcm.len.u[1] += bulk;
	bulk += res;

	if ((error = gcm_decrypt_ctr32(ctx, in + bulk, out + bulk, len - bulk)) != 0)
		return error;

	return 0;
}

static int
gcm_finish_op(struct ossl_gcm_context *ctx, const unsigned char *tag,
    size_t len)
{
	uint64_t alen = ctx->gcm.len.u[0] << 3;
	uint64_t clen = ctx->gcm.len.u[1] << 3;

	if (ctx->gcm.mres || ctx->gcm.ares)
		GCM_gmult(ctx->gcm.Xi.u, ctx->gcm.Htable);

#if BYTE_ORDER == LITTLE_ENDIAN
	alen = bswap64(alen);
	clen = bswap64(clen);
#endif

	ctx->gcm.Xi.u[0] ^= alen;
	ctx->gcm.Xi.u[1] ^= clen;
	GCM_gmult(ctx->gcm.Xi.u, ctx->gcm.Htable);

	ctx->gcm.Xi.u[0] ^= ctx->gcm.EK0.u[0];
	ctx->gcm.Xi.u[1] ^= ctx->gcm.EK0.u[1];

	if (tag != NULL)
		return timingsafe_bcmp(ctx->gcm.Xi.c, tag, len);
	return 0;
}

static const struct ossl_aes_gcm_ops gcm_ops = {
	.init = gcm_init_op,
	.setiv = gcm_setiv_op,
	.aad = gcm_aad_op,
	.encrypt = gcm_encrypt_op,
	.decrypt = gcm_decrypt_op,
	.finish = gcm_finish_op,
	.tag = gcm_tag_op,
};

int ossl_aes_gcm_setkey(const unsigned char *key, int klen, void *_ctx);

int
ossl_aes_gcm_setkey(const unsigned char *key, int klen,
    void *_ctx)
{
	struct ossl_gcm_context *ctx;

	ctx = _ctx;
	ctx->ops = &gcm_ops;
	gcm_init(ctx, key, klen);
	return (0);
}
