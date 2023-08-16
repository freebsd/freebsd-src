/*-
 * Copyright (c) 2016 The FreeBSD Foundation
 * Copyright (c) 2020 Ampere Computing
 * All rights reserved.
 *
 * This software was developed by Andrew Turner under
 * sponsorship from the FreeBSD Foundation.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * This file is derived from aesni_wrap.c:
 * Copyright (C) 2008 Damien Miller <djm@mindrot.org>
 * Copyright (c) 2010 Konstantin Belousov <kib@FreeBSD.org>
 * Copyright (c) 2010-2011 Pawel Jakub Dawidek <pawel@dawidek.net>
 * Copyright 2012-2013 John-Mark Gurney <jmg@FreeBSD.org>
 * Copyright (c) 2014 The FreeBSD Foundation
 */

/*
 * This code is built with floating-point enabled. Make sure to have entered
 * into floating-point context before calling any of these functions.
 */

#include <sys/cdefs.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/queue.h>

#include <opencrypto/cryptodev.h>
#include <opencrypto/gmac.h>
#include <crypto/rijndael/rijndael.h>
#include <crypto/armv8/armv8_crypto.h>

#include <arm_neon.h>

static uint8x16_t
armv8_aes_enc(int rounds, const uint8x16_t *keysched, const uint8x16_t from)
{
	uint8x16_t tmp;
	int i;

	tmp = from;
	for (i = 0; i < rounds - 1; i += 2) {
		tmp = vaeseq_u8(tmp, keysched[i]);
		tmp = vaesmcq_u8(tmp);
		tmp = vaeseq_u8(tmp, keysched[i + 1]);
		tmp = vaesmcq_u8(tmp);
	}

	tmp = vaeseq_u8(tmp, keysched[rounds - 1]);
	tmp = vaesmcq_u8(tmp);
	tmp = vaeseq_u8(tmp, keysched[rounds]);
	tmp = veorq_u8(tmp, keysched[rounds + 1]);

	return (tmp);
}

static uint8x16_t
armv8_aes_dec(int rounds, const uint8x16_t *keysched, const uint8x16_t from)
{
	uint8x16_t tmp;
	int i;

	tmp = from;
	for (i = 0; i < rounds - 1; i += 2) {
		tmp = vaesdq_u8(tmp, keysched[i]);
		tmp = vaesimcq_u8(tmp);
		tmp = vaesdq_u8(tmp, keysched[i+1]);
		tmp = vaesimcq_u8(tmp);
	}

	tmp = vaesdq_u8(tmp, keysched[rounds - 1]);
	tmp = vaesimcq_u8(tmp);
	tmp = vaesdq_u8(tmp, keysched[rounds]);
	tmp = veorq_u8(tmp, keysched[rounds + 1]);

	return (tmp);
}

void
armv8_aes_encrypt_cbc(const AES_key_t *key, size_t len,
    struct crypto_buffer_cursor *fromc, struct crypto_buffer_cursor *toc,
    const uint8_t iv[static AES_BLOCK_LEN])
{
	uint8x16_t tot, ivreg, tmp;
	uint8_t block[AES_BLOCK_LEN], *from, *to;
	size_t fromseglen, oseglen, seglen, toseglen;

	KASSERT(len % AES_BLOCK_LEN == 0,
	    ("%s: length %zu not a multiple of the block size", __func__, len));

	ivreg = vld1q_u8(iv);
	for (; len > 0; len -= seglen) {
		from = crypto_cursor_segment(fromc, &fromseglen);
		to = crypto_cursor_segment(toc, &toseglen);

		seglen = ulmin(len, ulmin(fromseglen, toseglen));
		if (seglen < AES_BLOCK_LEN) {
			crypto_cursor_copydata(fromc, AES_BLOCK_LEN, block);
			tmp = vld1q_u8(block);
			tot = armv8_aes_enc(key->aes_rounds - 1,
			    (const void *)key->aes_key, veorq_u8(tmp, ivreg));
			ivreg = tot;
			vst1q_u8(block, tot);
			crypto_cursor_copyback(toc, AES_BLOCK_LEN, block);
			seglen = AES_BLOCK_LEN;
		} else {
			for (oseglen = seglen; seglen >= AES_BLOCK_LEN;
			    seglen -= AES_BLOCK_LEN) {
				tmp = vld1q_u8(from);
				tot = armv8_aes_enc(key->aes_rounds - 1,
				    (const void *)key->aes_key,
				    veorq_u8(tmp, ivreg));
				ivreg = tot;
				vst1q_u8(to, tot);
				from += AES_BLOCK_LEN;
				to += AES_BLOCK_LEN;
			}
			seglen = oseglen - seglen;
			crypto_cursor_advance(fromc, seglen);
			crypto_cursor_advance(toc, seglen);
		}
	}

	explicit_bzero(block, sizeof(block));
}

void
armv8_aes_decrypt_cbc(const AES_key_t *key, size_t len,
    struct crypto_buffer_cursor *fromc, struct crypto_buffer_cursor *toc,
    const uint8_t iv[static AES_BLOCK_LEN])
{
	uint8x16_t ivreg, nextiv, tmp;
	uint8_t block[AES_BLOCK_LEN], *from, *to;
	size_t fromseglen, oseglen, seglen, toseglen;

	KASSERT(len % AES_BLOCK_LEN == 0,
	    ("%s: length %zu not a multiple of the block size", __func__, len));

	ivreg = vld1q_u8(iv);
	for (; len > 0; len -= seglen) {
		from = crypto_cursor_segment(fromc, &fromseglen);
		to = crypto_cursor_segment(toc, &toseglen);

		seglen = ulmin(len, ulmin(fromseglen, toseglen));
		if (seglen < AES_BLOCK_LEN) {
			crypto_cursor_copydata(fromc, AES_BLOCK_LEN, block);
			nextiv = vld1q_u8(block);
			tmp = armv8_aes_dec(key->aes_rounds - 1,
			    (const void *)key->aes_key, nextiv);
			vst1q_u8(block, veorq_u8(tmp, ivreg));
			ivreg = nextiv;
			crypto_cursor_copyback(toc, AES_BLOCK_LEN, block);
			seglen = AES_BLOCK_LEN;
		} else {
			for (oseglen = seglen; seglen >= AES_BLOCK_LEN;
			    seglen -= AES_BLOCK_LEN) {
				nextiv = vld1q_u8(from);
				tmp = armv8_aes_dec(key->aes_rounds - 1,
				    (const void *)key->aes_key, nextiv);
				vst1q_u8(to, veorq_u8(tmp, ivreg));
				ivreg = nextiv;
				from += AES_BLOCK_LEN;
				to += AES_BLOCK_LEN;
			}
			crypto_cursor_advance(fromc, oseglen - seglen);
			crypto_cursor_advance(toc, oseglen - seglen);
			seglen = oseglen - seglen;
		}
	}

	explicit_bzero(block, sizeof(block));
}

#define	AES_XTS_BLOCKSIZE	16
#define	AES_XTS_IVSIZE		8
#define	AES_XTS_ALPHA		0x87	/* GF(2^128) generator polynomial */

static inline int32x4_t
xts_crank_lfsr(int32x4_t inp)
{
	const int32x4_t alphamask = {AES_XTS_ALPHA, 1, 1, 1};
	int32x4_t xtweak, ret;

	/* set up xor mask */
	xtweak = vextq_s32(inp, inp, 3);
	xtweak = vshrq_n_s32(xtweak, 31);
	xtweak &= alphamask;

	/* next term */
	ret = vshlq_n_s32(inp, 1);
	ret ^= xtweak;

	return ret;
}

static void
armv8_aes_crypt_xts_block(int rounds, const uint8x16_t *key_schedule,
    uint8x16_t *tweak, const uint8_t *from, uint8_t *to, int do_encrypt)
{
	uint8x16_t block;

	block = vld1q_u8(from) ^ *tweak;

	if (do_encrypt)
		block = armv8_aes_enc(rounds - 1, key_schedule, block);
	else
		block = armv8_aes_dec(rounds - 1, key_schedule, block);

	vst1q_u8(to, block ^ *tweak);

	*tweak = vreinterpretq_u8_s32(xts_crank_lfsr(vreinterpretq_s32_u8(*tweak)));
}

static void
armv8_aes_crypt_xts(int rounds, const uint8x16_t *data_schedule,
    const uint8x16_t *tweak_schedule, size_t len,
    struct crypto_buffer_cursor *fromc, struct crypto_buffer_cursor *toc,
    const uint8_t iv[static AES_BLOCK_LEN], int do_encrypt)
{
	uint8x16_t tweakreg;
	uint8_t block[AES_XTS_BLOCKSIZE] __aligned(16);
	uint8_t tweak[AES_XTS_BLOCKSIZE] __aligned(16);
	uint8_t *from, *to;
	size_t fromseglen, oseglen, seglen, toseglen;

	KASSERT(len % AES_XTS_BLOCKSIZE == 0,
	    ("%s: length %zu not a multiple of the block size", __func__, len));

	/*
	 * Prepare tweak as E_k2(IV). IV is specified as LE representation
	 * of a 64-bit block number which we allow to be passed in directly.
	 */
#if BYTE_ORDER == LITTLE_ENDIAN
	bcopy(iv, tweak, AES_XTS_IVSIZE);
	/* Last 64 bits of IV are always zero. */
	bzero(tweak + AES_XTS_IVSIZE, AES_XTS_IVSIZE);
#else
#error Only LITTLE_ENDIAN architectures are supported.
#endif
	tweakreg = vld1q_u8(tweak);
	tweakreg = armv8_aes_enc(rounds - 1, tweak_schedule, tweakreg);

	for (; len > 0; len -= seglen) {
		from = crypto_cursor_segment(fromc, &fromseglen);
		to = crypto_cursor_segment(toc, &toseglen);

		seglen = ulmin(len, ulmin(fromseglen, toseglen));
		if (seglen < AES_XTS_BLOCKSIZE) {
			crypto_cursor_copydata(fromc, AES_XTS_BLOCKSIZE, block);
			armv8_aes_crypt_xts_block(rounds, data_schedule,
			    &tweakreg, block, block, do_encrypt);
			crypto_cursor_copyback(toc, AES_XTS_BLOCKSIZE, block);
			seglen = AES_XTS_BLOCKSIZE;
		} else {
			for (oseglen = seglen; seglen >= AES_XTS_BLOCKSIZE;
			    seglen -= AES_XTS_BLOCKSIZE) {
				armv8_aes_crypt_xts_block(rounds, data_schedule,
				    &tweakreg, from, to, do_encrypt);
				from += AES_XTS_BLOCKSIZE;
				to += AES_XTS_BLOCKSIZE;
			}
			seglen = oseglen - seglen;
			crypto_cursor_advance(fromc, seglen);
			crypto_cursor_advance(toc, seglen);
		}
	}

	explicit_bzero(block, sizeof(block));
}

void
armv8_aes_encrypt_xts(AES_key_t *data_schedule,
    const void *tweak_schedule, size_t len, struct crypto_buffer_cursor *fromc,
    struct crypto_buffer_cursor *toc, const uint8_t iv[static AES_BLOCK_LEN])
{
	armv8_aes_crypt_xts(data_schedule->aes_rounds,
	    (const void *)&data_schedule->aes_key, tweak_schedule, len, fromc,
	    toc, iv, 1);
}

void
armv8_aes_decrypt_xts(AES_key_t *data_schedule,
    const void *tweak_schedule, size_t len,
    struct crypto_buffer_cursor *fromc, struct crypto_buffer_cursor *toc,
    const uint8_t iv[static AES_BLOCK_LEN])
{
	armv8_aes_crypt_xts(data_schedule->aes_rounds,
	    (const void *)&data_schedule->aes_key, tweak_schedule, len, fromc,
	    toc, iv, 0);

}
#define	AES_INC_COUNTER(counter)				\
	do {							\
		for (int pos = AES_BLOCK_LEN - 1;		\
		     pos >= 0; pos--)				\
			if (++(counter)[pos])			\
				break;				\
	} while (0)

struct armv8_gcm_state {
	__uint128_val_t EK0;
	__uint128_val_t EKi;
	__uint128_val_t Xi;
	__uint128_val_t lenblock;
	uint8_t aes_counter[AES_BLOCK_LEN];
};

static void
armv8_aes_gmac_setup(struct armv8_gcm_state *s, AES_key_t *aes_key,
    const uint8_t *authdata, size_t authdatalen,
    const uint8_t iv[static AES_GCM_IV_LEN], const __uint128_val_t *Htable)
{
	uint8_t block[AES_BLOCK_LEN];
	size_t trailer;

	bzero(s->aes_counter, AES_BLOCK_LEN);
	memcpy(s->aes_counter, iv, AES_GCM_IV_LEN);

	/* Setup the counter */
	s->aes_counter[AES_BLOCK_LEN - 1] = 1;

	/* EK0 for a final GMAC round */
	aes_v8_encrypt(s->aes_counter, s->EK0.c, aes_key);

	/* GCM starts with 2 as counter, 1 is used for final xor of tag. */
	s->aes_counter[AES_BLOCK_LEN - 1] = 2;

	memset(s->Xi.c, 0, sizeof(s->Xi.c));
	trailer = authdatalen % AES_BLOCK_LEN;
	if (authdatalen - trailer > 0) {
		gcm_ghash_v8(s->Xi.u, Htable, authdata, authdatalen - trailer);
		authdata += authdatalen - trailer;
	}
	if (trailer > 0 || authdatalen == 0) {
		memset(block, 0, sizeof(block));
		memcpy(block, authdata, trailer);
		gcm_ghash_v8(s->Xi.u, Htable, block, AES_BLOCK_LEN);
	}
}

static void
armv8_aes_gmac_finish(struct armv8_gcm_state *s, size_t len,
    size_t authdatalen, const __uint128_val_t *Htable)
{
	/* Lengths block */
	s->lenblock.u[0] = s->lenblock.u[1] = 0;
	s->lenblock.d[1] = htobe32(authdatalen * 8);
	s->lenblock.d[3] = htobe32(len * 8);
	gcm_ghash_v8(s->Xi.u, Htable, s->lenblock.c, AES_BLOCK_LEN);

	s->Xi.u[0] ^= s->EK0.u[0];
	s->Xi.u[1] ^= s->EK0.u[1];
}

static void
armv8_aes_encrypt_gcm_block(struct armv8_gcm_state *s, AES_key_t *aes_key,
    const uint64_t *from, uint64_t *to)
{
	aes_v8_encrypt(s->aes_counter, s->EKi.c, aes_key);
	AES_INC_COUNTER(s->aes_counter);
	to[0] = from[0] ^ s->EKi.u[0];
	to[1] = from[1] ^ s->EKi.u[1];
}

static void
armv8_aes_decrypt_gcm_block(struct armv8_gcm_state *s, AES_key_t *aes_key,
    const uint64_t *from, uint64_t *to)
{
	armv8_aes_encrypt_gcm_block(s, aes_key, from, to);
}

void
armv8_aes_encrypt_gcm(AES_key_t *aes_key, size_t len,
    struct crypto_buffer_cursor *fromc, struct crypto_buffer_cursor *toc,
    size_t authdatalen, const uint8_t *authdata,
    uint8_t tag[static GMAC_DIGEST_LEN],
    const uint8_t iv[static AES_GCM_IV_LEN],
    const __uint128_val_t *Htable)
{
	struct armv8_gcm_state s;
	uint8_t block[AES_BLOCK_LEN] __aligned(AES_BLOCK_LEN);
	uint64_t *from64, *to64;
	size_t fromseglen, i, olen, oseglen, seglen, toseglen;

	armv8_aes_gmac_setup(&s, aes_key, authdata, authdatalen, iv, Htable);

	for (olen = len; len > 0; len -= seglen) {
		from64 = crypto_cursor_segment(fromc, &fromseglen);
		to64 = crypto_cursor_segment(toc, &toseglen);

		seglen = ulmin(len, ulmin(fromseglen, toseglen));
		if (seglen < AES_BLOCK_LEN) {
			seglen = ulmin(len, AES_BLOCK_LEN);

			memset(block, 0, sizeof(block));
			crypto_cursor_copydata(fromc, (int)seglen, block);

			if (seglen == AES_BLOCK_LEN) {
				armv8_aes_encrypt_gcm_block(&s, aes_key,
				    (uint64_t *)block, (uint64_t *)block);
			} else {
				aes_v8_encrypt(s.aes_counter, s.EKi.c, aes_key);
				AES_INC_COUNTER(s.aes_counter);
				for (i = 0; i < seglen; i++)
					block[i] ^= s.EKi.c[i];
			}
			gcm_ghash_v8(s.Xi.u, Htable, block, seglen);

			crypto_cursor_copyback(toc, (int)seglen, block);
		} else {
			for (oseglen = seglen; seglen >= AES_BLOCK_LEN;
			    seglen -= AES_BLOCK_LEN) {
				armv8_aes_encrypt_gcm_block(&s, aes_key, from64,
				    to64);
				gcm_ghash_v8(s.Xi.u, Htable, (uint8_t *)to64,
				    AES_BLOCK_LEN);

				from64 += 2;
				to64 += 2;
			}

			seglen = oseglen - seglen;
			crypto_cursor_advance(fromc, seglen);
			crypto_cursor_advance(toc, seglen);
		}
	}

	armv8_aes_gmac_finish(&s, olen, authdatalen, Htable);
	memcpy(tag, s.Xi.c, GMAC_DIGEST_LEN);

	explicit_bzero(block, sizeof(block));
	explicit_bzero(&s, sizeof(s));
}

int
armv8_aes_decrypt_gcm(AES_key_t *aes_key, size_t len,
    struct crypto_buffer_cursor *fromc, struct crypto_buffer_cursor *toc,
    size_t authdatalen, const uint8_t *authdata,
    const uint8_t tag[static GMAC_DIGEST_LEN],
    const uint8_t iv[static AES_GCM_IV_LEN],
    const __uint128_val_t *Htable)
{
	struct armv8_gcm_state s;
	struct crypto_buffer_cursor fromcc;
	uint8_t block[AES_BLOCK_LEN] __aligned(AES_BLOCK_LEN), *from;
	uint64_t *block64, *from64, *to64;
	size_t fromseglen, olen, oseglen, seglen, toseglen;
	int error;

	armv8_aes_gmac_setup(&s, aes_key, authdata, authdatalen, iv, Htable);

	crypto_cursor_copy(fromc, &fromcc);
	for (olen = len; len > 0; len -= seglen) {
		from = crypto_cursor_segment(&fromcc, &fromseglen);
		seglen = ulmin(len, fromseglen);
		seglen -= seglen % AES_BLOCK_LEN;
		if (seglen > 0) {
			gcm_ghash_v8(s.Xi.u, Htable, from, seglen);
			crypto_cursor_advance(&fromcc, seglen);
		} else {
			memset(block, 0, sizeof(block));
			seglen = ulmin(len, AES_BLOCK_LEN);
			crypto_cursor_copydata(&fromcc, seglen, block);
			gcm_ghash_v8(s.Xi.u, Htable, block, seglen);
		}
	}

	armv8_aes_gmac_finish(&s, olen, authdatalen, Htable);

	if (timingsafe_bcmp(tag, s.Xi.c, GMAC_DIGEST_LEN) != 0) {
		error = EBADMSG;
		goto out;
	}

	block64 = (uint64_t *)block;
	for (len = olen; len > 0; len -= seglen) {
		from64 = crypto_cursor_segment(fromc, &fromseglen);
		to64 = crypto_cursor_segment(toc, &toseglen);

		seglen = ulmin(len, ulmin(fromseglen, toseglen));
		if (seglen < AES_BLOCK_LEN) {
			seglen = ulmin(len, AES_BLOCK_LEN);

			memset(block, 0, sizeof(block));
			crypto_cursor_copydata(fromc, seglen, block);

			armv8_aes_decrypt_gcm_block(&s, aes_key, block64,
			    block64);

			crypto_cursor_copyback(toc, (int)seglen, block);
		} else {
			for (oseglen = seglen; seglen >= AES_BLOCK_LEN;
			    seglen -= AES_BLOCK_LEN) {
				armv8_aes_decrypt_gcm_block(&s, aes_key, from64,
				    to64);

				from64 += 2;
				to64 += 2;
			}

			seglen = oseglen - seglen;
			crypto_cursor_advance(fromc, seglen);
			crypto_cursor_advance(toc, seglen);
		}
	}

	error = 0;
out:
	explicit_bzero(block, sizeof(block));
	explicit_bzero(&s, sizeof(s));
	return (error);
}
