/* SPDX-License-Identifier: MIT
 *
 * Copyright (C) 2015-2021 Jason A. Donenfeld <Jason@zx2c4.com>. All Rights Reserved.
 * Copyright (c) 2022 The FreeBSD Foundation
 */

#include <sys/types.h>
#include <sys/systm.h>
#include <sys/endian.h>
#include <sys/mbuf.h>
#include <opencrypto/cryptodev.h>

#include "crypto.h"

#ifndef COMPAT_NEED_CHACHA20POLY1305_MBUF
static crypto_session_t chacha20_poly1305_sid;
#endif

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))
#endif
#ifndef noinline
#define noinline __attribute__((noinline))
#endif
#ifndef __aligned
#define __aligned(x) __attribute__((aligned(x)))
#endif
#ifndef DIV_ROUND_UP
#define DIV_ROUND_UP(n,d) (((n) + (d) - 1) / (d))
#endif

#define le32_to_cpup(a) le32toh(*(a))
#define le64_to_cpup(a) le64toh(*(a))
#define cpu_to_le32(a) htole32(a)
#define cpu_to_le64(a) htole64(a)

static inline __unused uint32_t get_unaligned_le32(const uint8_t *a)
{
	uint32_t l;
	__builtin_memcpy(&l, a, sizeof(l));
	return le32_to_cpup(&l);
}
static inline __unused uint64_t get_unaligned_le64(const uint8_t *a)
{
	uint64_t l;
	__builtin_memcpy(&l, a, sizeof(l));
	return le64_to_cpup(&l);
}
static inline __unused void put_unaligned_le32(uint32_t s, uint8_t *d)
{
	uint32_t l = cpu_to_le32(s);
	__builtin_memcpy(d, &l, sizeof(l));
}
static inline __unused void cpu_to_le32_array(uint32_t *buf, unsigned int words)
{
        while (words--) {
		*buf = cpu_to_le32(*buf);
		++buf;
	}
}
static inline __unused void le32_to_cpu_array(uint32_t *buf, unsigned int words)
{
        while (words--) {
		*buf = le32_to_cpup(buf);
		++buf;
        }
}
static inline __unused uint32_t rol32(uint32_t word, unsigned int shift)
{
        return (word << (shift & 31)) | (word >> ((-shift) & 31));
}
static inline __unused uint32_t ror32(uint32_t word, unsigned int shift)
{
	return (word >> (shift & 31)) | (word << ((-shift) & 31));
}

#if defined(COMPAT_NEED_CHACHA20POLY1305) || defined(COMPAT_NEED_CHACHA20POLY1305_MBUF)
static void xor_cpy(uint8_t *dst, const uint8_t *src1, const uint8_t *src2, size_t len)
{
	size_t i;

	for (i = 0; i < len; ++i)
		dst[i] = src1[i] ^ src2[i];
}

#define QUARTER_ROUND(x, a, b, c, d) ( \
	x[a] += x[b], \
	x[d] = rol32((x[d] ^ x[a]), 16), \
	x[c] += x[d], \
	x[b] = rol32((x[b] ^ x[c]), 12), \
	x[a] += x[b], \
	x[d] = rol32((x[d] ^ x[a]), 8), \
	x[c] += x[d], \
	x[b] = rol32((x[b] ^ x[c]), 7) \
)

#define C(i, j) (i * 4 + j)

#define DOUBLE_ROUND(x) ( \
	/* Column Round */ \
	QUARTER_ROUND(x, C(0, 0), C(1, 0), C(2, 0), C(3, 0)), \
	QUARTER_ROUND(x, C(0, 1), C(1, 1), C(2, 1), C(3, 1)), \
	QUARTER_ROUND(x, C(0, 2), C(1, 2), C(2, 2), C(3, 2)), \
	QUARTER_ROUND(x, C(0, 3), C(1, 3), C(2, 3), C(3, 3)), \
	/* Diagonal Round */ \
	QUARTER_ROUND(x, C(0, 0), C(1, 1), C(2, 2), C(3, 3)), \
	QUARTER_ROUND(x, C(0, 1), C(1, 2), C(2, 3), C(3, 0)), \
	QUARTER_ROUND(x, C(0, 2), C(1, 3), C(2, 0), C(3, 1)), \
	QUARTER_ROUND(x, C(0, 3), C(1, 0), C(2, 1), C(3, 2)) \
)

#define TWENTY_ROUNDS(x) ( \
	DOUBLE_ROUND(x), \
	DOUBLE_ROUND(x), \
	DOUBLE_ROUND(x), \
	DOUBLE_ROUND(x), \
	DOUBLE_ROUND(x), \
	DOUBLE_ROUND(x), \
	DOUBLE_ROUND(x), \
	DOUBLE_ROUND(x), \
	DOUBLE_ROUND(x), \
	DOUBLE_ROUND(x) \
)

enum chacha20_lengths {
	CHACHA20_NONCE_SIZE = 16,
	CHACHA20_KEY_SIZE = 32,
	CHACHA20_KEY_WORDS = CHACHA20_KEY_SIZE / sizeof(uint32_t),
	CHACHA20_BLOCK_SIZE = 64,
	CHACHA20_BLOCK_WORDS = CHACHA20_BLOCK_SIZE / sizeof(uint32_t),
	HCHACHA20_NONCE_SIZE = CHACHA20_NONCE_SIZE,
	HCHACHA20_KEY_SIZE = CHACHA20_KEY_SIZE
};

enum chacha20_constants { /* expand 32-byte k */
	CHACHA20_CONSTANT_EXPA = 0x61707865U,
	CHACHA20_CONSTANT_ND_3 = 0x3320646eU,
	CHACHA20_CONSTANT_2_BY = 0x79622d32U,
	CHACHA20_CONSTANT_TE_K = 0x6b206574U
};

struct chacha20_ctx {
	union {
		uint32_t state[16];
		struct {
			uint32_t constant[4];
			uint32_t key[8];
			uint32_t counter[4];
		};
	};
};

static void chacha20_init(struct chacha20_ctx *ctx,
			  const uint8_t key[CHACHA20_KEY_SIZE],
			  const uint64_t nonce)
{
	ctx->constant[0] = CHACHA20_CONSTANT_EXPA;
	ctx->constant[1] = CHACHA20_CONSTANT_ND_3;
	ctx->constant[2] = CHACHA20_CONSTANT_2_BY;
	ctx->constant[3] = CHACHA20_CONSTANT_TE_K;
	ctx->key[0] = get_unaligned_le32(key + 0);
	ctx->key[1] = get_unaligned_le32(key + 4);
	ctx->key[2] = get_unaligned_le32(key + 8);
	ctx->key[3] = get_unaligned_le32(key + 12);
	ctx->key[4] = get_unaligned_le32(key + 16);
	ctx->key[5] = get_unaligned_le32(key + 20);
	ctx->key[6] = get_unaligned_le32(key + 24);
	ctx->key[7] = get_unaligned_le32(key + 28);
	ctx->counter[0] = 0;
	ctx->counter[1] = 0;
	ctx->counter[2] = nonce & 0xffffffffU;
	ctx->counter[3] = nonce >> 32;
}

static void chacha20_block(struct chacha20_ctx *ctx, uint32_t *stream)
{
	uint32_t x[CHACHA20_BLOCK_WORDS];
	int i;

	for (i = 0; i < ARRAY_SIZE(x); ++i)
		x[i] = ctx->state[i];

	TWENTY_ROUNDS(x);

	for (i = 0; i < ARRAY_SIZE(x); ++i)
		stream[i] = cpu_to_le32(x[i] + ctx->state[i]);

	ctx->counter[0] += 1;
}

static void chacha20(struct chacha20_ctx *ctx, uint8_t *out, const uint8_t *in,
		     uint32_t len)
{
	uint32_t buf[CHACHA20_BLOCK_WORDS];

	while (len >= CHACHA20_BLOCK_SIZE) {
		chacha20_block(ctx, buf);
		xor_cpy(out, in, (uint8_t *)buf, CHACHA20_BLOCK_SIZE);
		len -= CHACHA20_BLOCK_SIZE;
		out += CHACHA20_BLOCK_SIZE;
		in += CHACHA20_BLOCK_SIZE;
	}
	if (len) {
		chacha20_block(ctx, buf);
		xor_cpy(out, in, (uint8_t *)buf, len);
	}
}

static void hchacha20(uint32_t derived_key[CHACHA20_KEY_WORDS],
		      const uint8_t nonce[HCHACHA20_NONCE_SIZE],
		      const uint8_t key[HCHACHA20_KEY_SIZE])
{
	uint32_t x[] = { CHACHA20_CONSTANT_EXPA,
		    CHACHA20_CONSTANT_ND_3,
		    CHACHA20_CONSTANT_2_BY,
		    CHACHA20_CONSTANT_TE_K,
		    get_unaligned_le32(key +  0),
		    get_unaligned_le32(key +  4),
		    get_unaligned_le32(key +  8),
		    get_unaligned_le32(key + 12),
		    get_unaligned_le32(key + 16),
		    get_unaligned_le32(key + 20),
		    get_unaligned_le32(key + 24),
		    get_unaligned_le32(key + 28),
		    get_unaligned_le32(nonce +  0),
		    get_unaligned_le32(nonce +  4),
		    get_unaligned_le32(nonce +  8),
		    get_unaligned_le32(nonce + 12)
	};

	TWENTY_ROUNDS(x);

	memcpy(derived_key + 0, x +  0, sizeof(uint32_t) * 4);
	memcpy(derived_key + 4, x + 12, sizeof(uint32_t) * 4);
}

enum poly1305_lengths {
	POLY1305_BLOCK_SIZE = 16,
	POLY1305_KEY_SIZE = 32,
	POLY1305_MAC_SIZE = 16
};

struct poly1305_internal {
	uint32_t h[5];
	uint32_t r[5];
	uint32_t s[4];
};

struct poly1305_ctx {
	struct poly1305_internal state;
	uint32_t nonce[4];
	uint8_t data[POLY1305_BLOCK_SIZE];
	size_t num;
};

static void poly1305_init_core(struct poly1305_internal *st,
			       const uint8_t key[16])
{
	/* r &= 0xffffffc0ffffffc0ffffffc0fffffff */
	st->r[0] = (get_unaligned_le32(&key[0])) & 0x3ffffff;
	st->r[1] = (get_unaligned_le32(&key[3]) >> 2) & 0x3ffff03;
	st->r[2] = (get_unaligned_le32(&key[6]) >> 4) & 0x3ffc0ff;
	st->r[3] = (get_unaligned_le32(&key[9]) >> 6) & 0x3f03fff;
	st->r[4] = (get_unaligned_le32(&key[12]) >> 8) & 0x00fffff;

	/* s = 5*r */
	st->s[0] = st->r[1] * 5;
	st->s[1] = st->r[2] * 5;
	st->s[2] = st->r[3] * 5;
	st->s[3] = st->r[4] * 5;

	/* h = 0 */
	st->h[0] = 0;
	st->h[1] = 0;
	st->h[2] = 0;
	st->h[3] = 0;
	st->h[4] = 0;
}

static void poly1305_blocks_core(struct poly1305_internal *st,
				 const uint8_t *input, size_t len,
				 const uint32_t padbit)
{
	const uint32_t hibit = padbit << 24;
	uint32_t r0, r1, r2, r3, r4;
	uint32_t s1, s2, s3, s4;
	uint32_t h0, h1, h2, h3, h4;
	uint64_t d0, d1, d2, d3, d4;
	uint32_t c;

	r0 = st->r[0];
	r1 = st->r[1];
	r2 = st->r[2];
	r3 = st->r[3];
	r4 = st->r[4];

	s1 = st->s[0];
	s2 = st->s[1];
	s3 = st->s[2];
	s4 = st->s[3];

	h0 = st->h[0];
	h1 = st->h[1];
	h2 = st->h[2];
	h3 = st->h[3];
	h4 = st->h[4];

	while (len >= POLY1305_BLOCK_SIZE) {
		/* h += m[i] */
		h0 += (get_unaligned_le32(&input[0])) & 0x3ffffff;
		h1 += (get_unaligned_le32(&input[3]) >> 2) & 0x3ffffff;
		h2 += (get_unaligned_le32(&input[6]) >> 4) & 0x3ffffff;
		h3 += (get_unaligned_le32(&input[9]) >> 6) & 0x3ffffff;
		h4 += (get_unaligned_le32(&input[12]) >> 8) | hibit;

		/* h *= r */
		d0 = ((uint64_t)h0 * r0) + ((uint64_t)h1 * s4) +
		     ((uint64_t)h2 * s3) + ((uint64_t)h3 * s2) +
		     ((uint64_t)h4 * s1);
		d1 = ((uint64_t)h0 * r1) + ((uint64_t)h1 * r0) +
		     ((uint64_t)h2 * s4) + ((uint64_t)h3 * s3) +
		     ((uint64_t)h4 * s2);
		d2 = ((uint64_t)h0 * r2) + ((uint64_t)h1 * r1) +
		     ((uint64_t)h2 * r0) + ((uint64_t)h3 * s4) +
		     ((uint64_t)h4 * s3);
		d3 = ((uint64_t)h0 * r3) + ((uint64_t)h1 * r2) +
		     ((uint64_t)h2 * r1) + ((uint64_t)h3 * r0) +
		     ((uint64_t)h4 * s4);
		d4 = ((uint64_t)h0 * r4) + ((uint64_t)h1 * r3) +
		     ((uint64_t)h2 * r2) + ((uint64_t)h3 * r1) +
		     ((uint64_t)h4 * r0);

		/* (partial) h %= p */
		c = (uint32_t)(d0 >> 26);
		h0 = (uint32_t)d0 & 0x3ffffff;
		d1 += c;
		c = (uint32_t)(d1 >> 26);
		h1 = (uint32_t)d1 & 0x3ffffff;
		d2 += c;
		c = (uint32_t)(d2 >> 26);
		h2 = (uint32_t)d2 & 0x3ffffff;
		d3 += c;
		c = (uint32_t)(d3 >> 26);
		h3 = (uint32_t)d3 & 0x3ffffff;
		d4 += c;
		c = (uint32_t)(d4 >> 26);
		h4 = (uint32_t)d4 & 0x3ffffff;
		h0 += c * 5;
		c = (h0 >> 26);
		h0 = h0 & 0x3ffffff;
		h1 += c;

		input += POLY1305_BLOCK_SIZE;
		len -= POLY1305_BLOCK_SIZE;
	}

	st->h[0] = h0;
	st->h[1] = h1;
	st->h[2] = h2;
	st->h[3] = h3;
	st->h[4] = h4;
}

static void poly1305_emit_core(struct poly1305_internal *st, uint8_t mac[16],
			       const uint32_t nonce[4])
{
	uint32_t h0, h1, h2, h3, h4, c;
	uint32_t g0, g1, g2, g3, g4;
	uint64_t f;
	uint32_t mask;

	/* fully carry h */
	h0 = st->h[0];
	h1 = st->h[1];
	h2 = st->h[2];
	h3 = st->h[3];
	h4 = st->h[4];

	c = h1 >> 26;
	h1 = h1 & 0x3ffffff;
	h2 += c;
	c = h2 >> 26;
	h2 = h2 & 0x3ffffff;
	h3 += c;
	c = h3 >> 26;
	h3 = h3 & 0x3ffffff;
	h4 += c;
	c = h4 >> 26;
	h4 = h4 & 0x3ffffff;
	h0 += c * 5;
	c = h0 >> 26;
	h0 = h0 & 0x3ffffff;
	h1 += c;

	/* compute h + -p */
	g0 = h0 + 5;
	c = g0 >> 26;
	g0 &= 0x3ffffff;
	g1 = h1 + c;
	c = g1 >> 26;
	g1 &= 0x3ffffff;
	g2 = h2 + c;
	c = g2 >> 26;
	g2 &= 0x3ffffff;
	g3 = h3 + c;
	c = g3 >> 26;
	g3 &= 0x3ffffff;
	g4 = h4 + c - (1UL << 26);

	/* select h if h < p, or h + -p if h >= p */
	mask = (g4 >> ((sizeof(uint32_t) * 8) - 1)) - 1;
	g0 &= mask;
	g1 &= mask;
	g2 &= mask;
	g3 &= mask;
	g4 &= mask;
	mask = ~mask;

	h0 = (h0 & mask) | g0;
	h1 = (h1 & mask) | g1;
	h2 = (h2 & mask) | g2;
	h3 = (h3 & mask) | g3;
	h4 = (h4 & mask) | g4;

	/* h = h % (2^128) */
	h0 = ((h0) | (h1 << 26)) & 0xffffffff;
	h1 = ((h1 >> 6) | (h2 << 20)) & 0xffffffff;
	h2 = ((h2 >> 12) | (h3 << 14)) & 0xffffffff;
	h3 = ((h3 >> 18) | (h4 << 8)) & 0xffffffff;

	/* mac = (h + nonce) % (2^128) */
	f = (uint64_t)h0 + nonce[0];
	h0 = (uint32_t)f;
	f = (uint64_t)h1 + nonce[1] + (f >> 32);
	h1 = (uint32_t)f;
	f = (uint64_t)h2 + nonce[2] + (f >> 32);
	h2 = (uint32_t)f;
	f = (uint64_t)h3 + nonce[3] + (f >> 32);
	h3 = (uint32_t)f;

	put_unaligned_le32(h0, &mac[0]);
	put_unaligned_le32(h1, &mac[4]);
	put_unaligned_le32(h2, &mac[8]);
	put_unaligned_le32(h3, &mac[12]);
}

static void poly1305_init(struct poly1305_ctx *ctx,
			  const uint8_t key[POLY1305_KEY_SIZE])
{
	ctx->nonce[0] = get_unaligned_le32(&key[16]);
	ctx->nonce[1] = get_unaligned_le32(&key[20]);
	ctx->nonce[2] = get_unaligned_le32(&key[24]);
	ctx->nonce[3] = get_unaligned_le32(&key[28]);

	poly1305_init_core(&ctx->state, key);

	ctx->num = 0;
}

static void poly1305_update(struct poly1305_ctx *ctx, const uint8_t *input,
			    size_t len)
{
	const size_t num = ctx->num;
	size_t rem;

	if (num) {
		rem = POLY1305_BLOCK_SIZE - num;
		if (len < rem) {
			memcpy(ctx->data + num, input, len);
			ctx->num = num + len;
			return;
		}
		memcpy(ctx->data + num, input, rem);
		poly1305_blocks_core(&ctx->state, ctx->data,
				     POLY1305_BLOCK_SIZE, 1);
		input += rem;
		len -= rem;
	}

	rem = len % POLY1305_BLOCK_SIZE;
	len -= rem;

	if (len >= POLY1305_BLOCK_SIZE) {
		poly1305_blocks_core(&ctx->state, input, len, 1);
		input += len;
	}

	if (rem)
		memcpy(ctx->data, input, rem);

	ctx->num = rem;
}

static void poly1305_final(struct poly1305_ctx *ctx,
			   uint8_t mac[POLY1305_MAC_SIZE])
{
	size_t num = ctx->num;

	if (num) {
		ctx->data[num++] = 1;
		while (num < POLY1305_BLOCK_SIZE)
			ctx->data[num++] = 0;
		poly1305_blocks_core(&ctx->state, ctx->data,
				     POLY1305_BLOCK_SIZE, 0);
	}

	poly1305_emit_core(&ctx->state, mac, ctx->nonce);

	explicit_bzero(ctx, sizeof(*ctx));
}
#endif

#ifdef COMPAT_NEED_CHACHA20POLY1305
static const uint8_t pad0[16] = { 0 };

void
chacha20poly1305_encrypt(uint8_t *dst, const uint8_t *src, const size_t src_len,
			 const uint8_t *ad, const size_t ad_len,
			 const uint64_t nonce,
			 const uint8_t key[CHACHA20POLY1305_KEY_SIZE])
{
	struct poly1305_ctx poly1305_state;
	struct chacha20_ctx chacha20_state;
	union {
		uint8_t block0[POLY1305_KEY_SIZE];
		uint64_t lens[2];
	} b = { { 0 } };

	chacha20_init(&chacha20_state, key, nonce);
	chacha20(&chacha20_state, b.block0, b.block0, sizeof(b.block0));
	poly1305_init(&poly1305_state, b.block0);

	poly1305_update(&poly1305_state, ad, ad_len);
	poly1305_update(&poly1305_state, pad0, (0x10 - ad_len) & 0xf);

	chacha20(&chacha20_state, dst, src, src_len);

	poly1305_update(&poly1305_state, dst, src_len);
	poly1305_update(&poly1305_state, pad0, (0x10 - src_len) & 0xf);

	b.lens[0] = cpu_to_le64(ad_len);
	b.lens[1] = cpu_to_le64(src_len);
	poly1305_update(&poly1305_state, (uint8_t *)b.lens, sizeof(b.lens));

	poly1305_final(&poly1305_state, dst + src_len);

	explicit_bzero(&chacha20_state, sizeof(chacha20_state));
	explicit_bzero(&b, sizeof(b));
}

bool
chacha20poly1305_decrypt(uint8_t *dst, const uint8_t *src, const size_t src_len,
			 const uint8_t *ad, const size_t ad_len,
			 const uint64_t nonce,
			 const uint8_t key[CHACHA20POLY1305_KEY_SIZE])
{
	struct poly1305_ctx poly1305_state;
	struct chacha20_ctx chacha20_state;
	bool ret;
	size_t dst_len;
	union {
		uint8_t block0[POLY1305_KEY_SIZE];
		uint8_t mac[POLY1305_MAC_SIZE];
		uint64_t lens[2];
	} b = { { 0 } };

	if (src_len < POLY1305_MAC_SIZE)
		return false;

	chacha20_init(&chacha20_state, key, nonce);
	chacha20(&chacha20_state, b.block0, b.block0, sizeof(b.block0));
	poly1305_init(&poly1305_state, b.block0);

	poly1305_update(&poly1305_state, ad, ad_len);
	poly1305_update(&poly1305_state, pad0, (0x10 - ad_len) & 0xf);

	dst_len = src_len - POLY1305_MAC_SIZE;
	poly1305_update(&poly1305_state, src, dst_len);
	poly1305_update(&poly1305_state, pad0, (0x10 - dst_len) & 0xf);

	b.lens[0] = cpu_to_le64(ad_len);
	b.lens[1] = cpu_to_le64(dst_len);
	poly1305_update(&poly1305_state, (uint8_t *)b.lens, sizeof(b.lens));

	poly1305_final(&poly1305_state, b.mac);

	ret = timingsafe_bcmp(b.mac, src + dst_len, POLY1305_MAC_SIZE) == 0;
	if (ret)
		chacha20(&chacha20_state, dst, src, dst_len);

	explicit_bzero(&chacha20_state, sizeof(chacha20_state));
	explicit_bzero(&b, sizeof(b));

	return ret;
}

void
xchacha20poly1305_encrypt(uint8_t *dst, const uint8_t *src,
			  const size_t src_len, const uint8_t *ad,
			  const size_t ad_len,
			  const uint8_t nonce[XCHACHA20POLY1305_NONCE_SIZE],
			  const uint8_t key[CHACHA20POLY1305_KEY_SIZE])
{
	uint32_t derived_key[CHACHA20_KEY_WORDS];

	hchacha20(derived_key, nonce, key);
	cpu_to_le32_array(derived_key, ARRAY_SIZE(derived_key));
	chacha20poly1305_encrypt(dst, src, src_len, ad, ad_len,
				 get_unaligned_le64(nonce + 16),
				 (uint8_t *)derived_key);
	explicit_bzero(derived_key, CHACHA20POLY1305_KEY_SIZE);
}

bool
xchacha20poly1305_decrypt(uint8_t *dst, const uint8_t *src,
			  const size_t src_len,  const uint8_t *ad,
			  const size_t ad_len,
			  const uint8_t nonce[XCHACHA20POLY1305_NONCE_SIZE],
			  const uint8_t key[CHACHA20POLY1305_KEY_SIZE])
{
	bool ret;
	uint32_t derived_key[CHACHA20_KEY_WORDS];

	hchacha20(derived_key, nonce, key);
	cpu_to_le32_array(derived_key, ARRAY_SIZE(derived_key));
	ret = chacha20poly1305_decrypt(dst, src, src_len, ad, ad_len,
				       get_unaligned_le64(nonce + 16),
				       (uint8_t *)derived_key);
	explicit_bzero(derived_key, CHACHA20POLY1305_KEY_SIZE);
	return ret;
}
#endif

#ifdef COMPAT_NEED_CHACHA20POLY1305_MBUF
static inline int
chacha20poly1305_crypt_mbuf(struct mbuf *m0, uint64_t nonce,
			    const uint8_t key[CHACHA20POLY1305_KEY_SIZE], bool encrypt)
{
	struct poly1305_ctx poly1305_state;
	struct chacha20_ctx chacha20_state;
	uint8_t *buf, mbuf_mac[POLY1305_MAC_SIZE];
	size_t len, leftover = 0;
	struct mbuf *m;
	int ret;
	union {
		uint32_t stream[CHACHA20_BLOCK_WORDS];
		uint8_t block0[POLY1305_KEY_SIZE];
		uint8_t mac[POLY1305_MAC_SIZE];
		uint64_t lens[2];
	} b = { { 0 } };

	if (!encrypt) {
		if (m0->m_pkthdr.len < POLY1305_MAC_SIZE)
			return EMSGSIZE;
		m_copydata(m0, m0->m_pkthdr.len - POLY1305_MAC_SIZE, POLY1305_MAC_SIZE, mbuf_mac);
		m_adj(m0, -POLY1305_MAC_SIZE);
	}

	chacha20_init(&chacha20_state, key, nonce);
	chacha20(&chacha20_state, b.block0, b.block0, sizeof(b.block0));
	poly1305_init(&poly1305_state, b.block0);

	for (m = m0; m; m = m->m_next) {
		len = m->m_len;
		buf = m->m_data;

		if (!encrypt)
			poly1305_update(&poly1305_state, m->m_data, m->m_len);

		if (leftover != 0) {
			size_t l = min(len, leftover);
			xor_cpy(buf, buf, ((uint8_t *)b.stream) + (CHACHA20_BLOCK_SIZE - leftover), l);
			leftover -= l;
			buf += l;
			len -= l;
		}

		while (len >= CHACHA20_BLOCK_SIZE) {
			chacha20_block(&chacha20_state, b.stream);
			xor_cpy(buf, buf, (uint8_t *)b.stream, CHACHA20_BLOCK_SIZE);
			buf += CHACHA20_BLOCK_SIZE;
			len -= CHACHA20_BLOCK_SIZE;
		}

		if (len) {
			chacha20_block(&chacha20_state, b.stream);
			xor_cpy(buf, buf, (uint8_t *)b.stream, len);
			leftover = CHACHA20_BLOCK_SIZE - len;
		}

		if (encrypt)
			poly1305_update(&poly1305_state, m->m_data, m->m_len);
	}
	poly1305_update(&poly1305_state, pad0, (0x10 - m0->m_pkthdr.len) & 0xf);

	b.lens[0] = 0;
	b.lens[1] = cpu_to_le64(m0->m_pkthdr.len);
	poly1305_update(&poly1305_state, (uint8_t *)b.lens, sizeof(b.lens));

	poly1305_final(&poly1305_state, b.mac);

	if (encrypt)
		ret = m_append(m0, POLY1305_MAC_SIZE, b.mac) ? 0 : ENOMEM;
	else
		ret = timingsafe_bcmp(b.mac, mbuf_mac, POLY1305_MAC_SIZE) == 0 ? 0 : EBADMSG;

	explicit_bzero(&chacha20_state, sizeof(chacha20_state));
	explicit_bzero(&b, sizeof(b));

	return ret;
}

int
chacha20poly1305_encrypt_mbuf(struct mbuf *m, const uint64_t nonce,
			      const uint8_t key[CHACHA20POLY1305_KEY_SIZE])
{
	return chacha20poly1305_crypt_mbuf(m, nonce, key, true);
}

int
chacha20poly1305_decrypt_mbuf(struct mbuf *m, const uint64_t nonce,
			      const uint8_t key[CHACHA20POLY1305_KEY_SIZE])
{
	return chacha20poly1305_crypt_mbuf(m, nonce, key, false);
}
#else
static int
crypto_callback(struct cryptop *crp)
{
	return (0);
}

int
chacha20poly1305_encrypt_mbuf(struct mbuf *m, const uint64_t nonce,
			      const uint8_t key[CHACHA20POLY1305_KEY_SIZE])
{
	static const char blank_tag[POLY1305_HASH_LEN];
	struct cryptop crp;
	int ret;

	if (!m_append(m, POLY1305_HASH_LEN, blank_tag))
		return (ENOMEM);
	crypto_initreq(&crp, chacha20_poly1305_sid);
	crp.crp_op = CRYPTO_OP_ENCRYPT | CRYPTO_OP_COMPUTE_DIGEST;
	crp.crp_flags = CRYPTO_F_IV_SEPARATE | CRYPTO_F_CBIMM;
	crypto_use_mbuf(&crp, m);
	crp.crp_payload_length = m->m_pkthdr.len - POLY1305_HASH_LEN;
	crp.crp_digest_start = crp.crp_payload_length;
	le64enc(crp.crp_iv, nonce);
	crp.crp_cipher_key = key;
	crp.crp_callback = crypto_callback;
	ret = crypto_dispatch(&crp);
	crypto_destroyreq(&crp);
	return (ret);
}

int
chacha20poly1305_decrypt_mbuf(struct mbuf *m, const uint64_t nonce,
			      const uint8_t key[CHACHA20POLY1305_KEY_SIZE])
{
	struct cryptop crp;
	int ret;

	if (m->m_pkthdr.len < POLY1305_HASH_LEN)
		return (EMSGSIZE);
	crypto_initreq(&crp, chacha20_poly1305_sid);
	crp.crp_op = CRYPTO_OP_DECRYPT | CRYPTO_OP_VERIFY_DIGEST;
	crp.crp_flags = CRYPTO_F_IV_SEPARATE | CRYPTO_F_CBIMM;
	crypto_use_mbuf(&crp, m);
	crp.crp_payload_length = m->m_pkthdr.len - POLY1305_HASH_LEN;
	crp.crp_digest_start = crp.crp_payload_length;
	le64enc(crp.crp_iv, nonce);
	crp.crp_cipher_key = key;
	crp.crp_callback = crypto_callback;
	ret = crypto_dispatch(&crp);
	crypto_destroyreq(&crp);
	if (ret)
		return (ret);
	m_adj(m, -POLY1305_HASH_LEN);
	return (0);
}
#endif

#ifdef COMPAT_NEED_BLAKE2S
static const uint32_t blake2s_iv[8] = {
	0x6A09E667UL, 0xBB67AE85UL, 0x3C6EF372UL, 0xA54FF53AUL,
	0x510E527FUL, 0x9B05688CUL, 0x1F83D9ABUL, 0x5BE0CD19UL
};

static const uint8_t blake2s_sigma[10][16] = {
	{ 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15 },
	{ 14, 10, 4, 8, 9, 15, 13, 6, 1, 12, 0, 2, 11, 7, 5, 3 },
	{ 11, 8, 12, 0, 5, 2, 15, 13, 10, 14, 3, 6, 7, 1, 9, 4 },
	{ 7, 9, 3, 1, 13, 12, 11, 14, 2, 6, 5, 10, 4, 0, 15, 8 },
	{ 9, 0, 5, 7, 2, 4, 10, 15, 14, 1, 11, 12, 6, 8, 3, 13 },
	{ 2, 12, 6, 10, 0, 11, 8, 3, 4, 13, 7, 5, 15, 14, 1, 9 },
	{ 12, 5, 1, 15, 14, 13, 4, 10, 0, 7, 6, 3, 9, 2, 8, 11 },
	{ 13, 11, 7, 14, 12, 1, 3, 9, 5, 0, 15, 4, 8, 6, 2, 10 },
	{ 6, 15, 14, 9, 11, 3, 0, 8, 12, 2, 13, 7, 1, 4, 10, 5 },
	{ 10, 2, 8, 4, 7, 6, 1, 5, 15, 11, 9, 14, 3, 12, 13, 0 },
};

static inline void blake2s_set_lastblock(struct blake2s_state *state)
{
	state->f[0] = -1;
}

static inline void blake2s_increment_counter(struct blake2s_state *state,
					     const uint32_t inc)
{
	state->t[0] += inc;
	state->t[1] += (state->t[0] < inc);
}

static inline void blake2s_init_param(struct blake2s_state *state,
				      const uint32_t param)
{
	int i;

	memset(state, 0, sizeof(*state));
	for (i = 0; i < 8; ++i)
		state->h[i] = blake2s_iv[i];
	state->h[0] ^= param;
}

void blake2s_init(struct blake2s_state *state, const size_t outlen)
{
	blake2s_init_param(state, 0x01010000 | outlen);
	state->outlen = outlen;
}

void blake2s_init_key(struct blake2s_state *state, const size_t outlen,
		      const uint8_t *key, const size_t keylen)
{
	uint8_t block[BLAKE2S_BLOCK_SIZE] = { 0 };

	blake2s_init_param(state, 0x01010000 | keylen << 8 | outlen);
	state->outlen = outlen;
	memcpy(block, key, keylen);
	blake2s_update(state, block, BLAKE2S_BLOCK_SIZE);
	explicit_bzero(block, BLAKE2S_BLOCK_SIZE);
}

static inline void blake2s_compress(struct blake2s_state *state,
				    const uint8_t *block, size_t nblocks,
				    const uint32_t inc)
{
	uint32_t m[16];
	uint32_t v[16];
	int i;

	while (nblocks > 0) {
		blake2s_increment_counter(state, inc);
		memcpy(m, block, BLAKE2S_BLOCK_SIZE);
		le32_to_cpu_array(m, ARRAY_SIZE(m));
		memcpy(v, state->h, 32);
		v[ 8] = blake2s_iv[0];
		v[ 9] = blake2s_iv[1];
		v[10] = blake2s_iv[2];
		v[11] = blake2s_iv[3];
		v[12] = blake2s_iv[4] ^ state->t[0];
		v[13] = blake2s_iv[5] ^ state->t[1];
		v[14] = blake2s_iv[6] ^ state->f[0];
		v[15] = blake2s_iv[7] ^ state->f[1];

#define G(r, i, a, b, c, d) do { \
	a += b + m[blake2s_sigma[r][2 * i + 0]]; \
	d = ror32(d ^ a, 16); \
	c += d; \
	b = ror32(b ^ c, 12); \
	a += b + m[blake2s_sigma[r][2 * i + 1]]; \
	d = ror32(d ^ a, 8); \
	c += d; \
	b = ror32(b ^ c, 7); \
} while (0)

#define ROUND(r) do { \
	G(r, 0, v[0], v[ 4], v[ 8], v[12]); \
	G(r, 1, v[1], v[ 5], v[ 9], v[13]); \
	G(r, 2, v[2], v[ 6], v[10], v[14]); \
	G(r, 3, v[3], v[ 7], v[11], v[15]); \
	G(r, 4, v[0], v[ 5], v[10], v[15]); \
	G(r, 5, v[1], v[ 6], v[11], v[12]); \
	G(r, 6, v[2], v[ 7], v[ 8], v[13]); \
	G(r, 7, v[3], v[ 4], v[ 9], v[14]); \
} while (0)
		ROUND(0);
		ROUND(1);
		ROUND(2);
		ROUND(3);
		ROUND(4);
		ROUND(5);
		ROUND(6);
		ROUND(7);
		ROUND(8);
		ROUND(9);

#undef G
#undef ROUND

		for (i = 0; i < 8; ++i)
			state->h[i] ^= v[i] ^ v[i + 8];

		block += BLAKE2S_BLOCK_SIZE;
		--nblocks;
	}
}

void blake2s_update(struct blake2s_state *state, const uint8_t *in, size_t inlen)
{
	const size_t fill = BLAKE2S_BLOCK_SIZE - state->buflen;

	if (!inlen)
		return;
	if (inlen > fill) {
		memcpy(state->buf + state->buflen, in, fill);
		blake2s_compress(state, state->buf, 1, BLAKE2S_BLOCK_SIZE);
		state->buflen = 0;
		in += fill;
		inlen -= fill;
	}
	if (inlen > BLAKE2S_BLOCK_SIZE) {
		const size_t nblocks = DIV_ROUND_UP(inlen, BLAKE2S_BLOCK_SIZE);
		/* Hash one less (full) block than strictly possible */
		blake2s_compress(state, in, nblocks - 1, BLAKE2S_BLOCK_SIZE);
		in += BLAKE2S_BLOCK_SIZE * (nblocks - 1);
		inlen -= BLAKE2S_BLOCK_SIZE * (nblocks - 1);
	}
	memcpy(state->buf + state->buflen, in, inlen);
	state->buflen += inlen;
}

void blake2s_final(struct blake2s_state *state, uint8_t *out)
{
	blake2s_set_lastblock(state);
	memset(state->buf + state->buflen, 0,
	       BLAKE2S_BLOCK_SIZE - state->buflen); /* Padding */
	blake2s_compress(state, state->buf, 1, state->buflen);
	cpu_to_le32_array(state->h, ARRAY_SIZE(state->h));
	memcpy(out, state->h, state->outlen);
	explicit_bzero(state, sizeof(*state));
}
#endif

#ifdef COMPAT_NEED_CURVE25519
/* Below here is fiat's implementation of x25519.
 *
 * Copyright (C) 2015-2016 The fiat-crypto Authors.
 * Copyright (C) 2018-2021 Jason A. Donenfeld <Jason@zx2c4.com>. All Rights Reserved.
 *
 * This is a machine-generated formally verified implementation of Curve25519
 * ECDH from: <https://github.com/mit-plv/fiat-crypto>. Though originally
 * machine generated, it has been tweaked to be suitable for use in the kernel.
 * It is optimized for 32-bit machines and machines that cannot work efficiently
 * with 128-bit integer types.
 */

/* fe means field element. Here the field is \Z/(2^255-19). An element t,
 * entries t[0]...t[9], represents the integer t[0]+2^26 t[1]+2^51 t[2]+2^77
 * t[3]+2^102 t[4]+...+2^230 t[9].
 * fe limbs are bounded by 1.125*2^26,1.125*2^25,1.125*2^26,1.125*2^25,etc.
 * Multiplication and carrying produce fe from fe_loose.
 */
typedef struct fe { uint32_t v[10]; } fe;

/* fe_loose limbs are bounded by 3.375*2^26,3.375*2^25,3.375*2^26,3.375*2^25,etc
 * Addition and subtraction produce fe_loose from (fe, fe).
 */
typedef struct fe_loose { uint32_t v[10]; } fe_loose;

static inline void fe_frombytes_impl(uint32_t h[10], const uint8_t *s)
{
	/* Ignores top bit of s. */
	uint32_t a0 = get_unaligned_le32(s);
	uint32_t a1 = get_unaligned_le32(s+4);
	uint32_t a2 = get_unaligned_le32(s+8);
	uint32_t a3 = get_unaligned_le32(s+12);
	uint32_t a4 = get_unaligned_le32(s+16);
	uint32_t a5 = get_unaligned_le32(s+20);
	uint32_t a6 = get_unaligned_le32(s+24);
	uint32_t a7 = get_unaligned_le32(s+28);
	h[0] = a0&((1<<26)-1);                    /* 26 used, 32-26 left.   26 */
	h[1] = (a0>>26) | ((a1&((1<<19)-1))<< 6); /* (32-26) + 19 =  6+19 = 25 */
	h[2] = (a1>>19) | ((a2&((1<<13)-1))<<13); /* (32-19) + 13 = 13+13 = 26 */
	h[3] = (a2>>13) | ((a3&((1<< 6)-1))<<19); /* (32-13) +  6 = 19+ 6 = 25 */
	h[4] = (a3>> 6);                          /* (32- 6)              = 26 */
	h[5] = a4&((1<<25)-1);                    /*                        25 */
	h[6] = (a4>>25) | ((a5&((1<<19)-1))<< 7); /* (32-25) + 19 =  7+19 = 26 */
	h[7] = (a5>>19) | ((a6&((1<<12)-1))<<13); /* (32-19) + 12 = 13+12 = 25 */
	h[8] = (a6>>12) | ((a7&((1<< 6)-1))<<20); /* (32-12) +  6 = 20+ 6 = 26 */
	h[9] = (a7>> 6)&((1<<25)-1); /*                                     25 */
}

static inline void fe_frombytes(fe *h, const uint8_t *s)
{
	fe_frombytes_impl(h->v, s);
}

static inline uint8_t /*bool*/
addcarryx_u25(uint8_t /*bool*/ c, uint32_t a, uint32_t b, uint32_t *low)
{
	/* This function extracts 25 bits of result and 1 bit of carry
	 * (26 total), so a 32-bit intermediate is sufficient.
	 */
	uint32_t x = a + b + c;
	*low = x & ((1 << 25) - 1);
	return (x >> 25) & 1;
}

static inline uint8_t /*bool*/
addcarryx_u26(uint8_t /*bool*/ c, uint32_t a, uint32_t b, uint32_t *low)
{
	/* This function extracts 26 bits of result and 1 bit of carry
	 * (27 total), so a 32-bit intermediate is sufficient.
	 */
	uint32_t x = a + b + c;
	*low = x & ((1 << 26) - 1);
	return (x >> 26) & 1;
}

static inline uint8_t /*bool*/
subborrow_u25(uint8_t /*bool*/ c, uint32_t a, uint32_t b, uint32_t *low)
{
	/* This function extracts 25 bits of result and 1 bit of borrow
	 * (26 total), so a 32-bit intermediate is sufficient.
	 */
	uint32_t x = a - b - c;
	*low = x & ((1 << 25) - 1);
	return x >> 31;
}

static inline uint8_t /*bool*/
subborrow_u26(uint8_t /*bool*/ c, uint32_t a, uint32_t b, uint32_t *low)
{
	/* This function extracts 26 bits of result and 1 bit of borrow
	 *(27 total), so a 32-bit intermediate is sufficient.
	 */
	uint32_t x = a - b - c;
	*low = x & ((1 << 26) - 1);
	return x >> 31;
}

static inline uint32_t cmovznz32(uint32_t t, uint32_t z, uint32_t nz)
{
	t = -!!t; /* all set if nonzero, 0 if 0 */
	return (t&nz) | ((~t)&z);
}

static inline void fe_freeze(uint32_t out[10], const uint32_t in1[10])
{
	const uint32_t x17 = in1[9];
	const uint32_t x18 = in1[8];
	const uint32_t x16 = in1[7];
	const uint32_t x14 = in1[6];
	const uint32_t x12 = in1[5];
	const uint32_t x10 = in1[4];
	const uint32_t x8 = in1[3];
	const uint32_t x6 = in1[2];
	const uint32_t x4 = in1[1];
	const uint32_t x2 = in1[0];
	uint32_t x20; uint8_t/*bool*/ x21 = subborrow_u26(0x0, x2, 0x3ffffed, &x20);
	uint32_t x23; uint8_t/*bool*/ x24 = subborrow_u25(x21, x4, 0x1ffffff, &x23);
	uint32_t x26; uint8_t/*bool*/ x27 = subborrow_u26(x24, x6, 0x3ffffff, &x26);
	uint32_t x29; uint8_t/*bool*/ x30 = subborrow_u25(x27, x8, 0x1ffffff, &x29);
	uint32_t x32; uint8_t/*bool*/ x33 = subborrow_u26(x30, x10, 0x3ffffff, &x32);
	uint32_t x35; uint8_t/*bool*/ x36 = subborrow_u25(x33, x12, 0x1ffffff, &x35);
	uint32_t x38; uint8_t/*bool*/ x39 = subborrow_u26(x36, x14, 0x3ffffff, &x38);
	uint32_t x41; uint8_t/*bool*/ x42 = subborrow_u25(x39, x16, 0x1ffffff, &x41);
	uint32_t x44; uint8_t/*bool*/ x45 = subborrow_u26(x42, x18, 0x3ffffff, &x44);
	uint32_t x47; uint8_t/*bool*/ x48 = subborrow_u25(x45, x17, 0x1ffffff, &x47);
	uint32_t x49 = cmovznz32(x48, 0x0, 0xffffffff);
	uint32_t x50 = (x49 & 0x3ffffed);
	uint32_t x52; uint8_t/*bool*/ x53 = addcarryx_u26(0x0, x20, x50, &x52);
	uint32_t x54 = (x49 & 0x1ffffff);
	uint32_t x56; uint8_t/*bool*/ x57 = addcarryx_u25(x53, x23, x54, &x56);
	uint32_t x58 = (x49 & 0x3ffffff);
	uint32_t x60; uint8_t/*bool*/ x61 = addcarryx_u26(x57, x26, x58, &x60);
	uint32_t x62 = (x49 & 0x1ffffff);
	uint32_t x64; uint8_t/*bool*/ x65 = addcarryx_u25(x61, x29, x62, &x64);
	uint32_t x66 = (x49 & 0x3ffffff);
	uint32_t x68; uint8_t/*bool*/ x69 = addcarryx_u26(x65, x32, x66, &x68);
	uint32_t x70 = (x49 & 0x1ffffff);
	uint32_t x72; uint8_t/*bool*/ x73 = addcarryx_u25(x69, x35, x70, &x72);
	uint32_t x74 = (x49 & 0x3ffffff);
	uint32_t x76; uint8_t/*bool*/ x77 = addcarryx_u26(x73, x38, x74, &x76);
	uint32_t x78 = (x49 & 0x1ffffff);
	uint32_t x80; uint8_t/*bool*/ x81 = addcarryx_u25(x77, x41, x78, &x80);
	uint32_t x82 = (x49 & 0x3ffffff);
	uint32_t x84; uint8_t/*bool*/ x85 = addcarryx_u26(x81, x44, x82, &x84);
	uint32_t x86 = (x49 & 0x1ffffff);
	uint32_t x88; addcarryx_u25(x85, x47, x86, &x88);
	out[0] = x52;
	out[1] = x56;
	out[2] = x60;
	out[3] = x64;
	out[4] = x68;
	out[5] = x72;
	out[6] = x76;
	out[7] = x80;
	out[8] = x84;
	out[9] = x88;
}

static inline void fe_tobytes(uint8_t s[32], const fe *f)
{
	uint32_t h[10];
	fe_freeze(h, f->v);
	s[0] = h[0] >> 0;
	s[1] = h[0] >> 8;
	s[2] = h[0] >> 16;
	s[3] = (h[0] >> 24) | (h[1] << 2);
	s[4] = h[1] >> 6;
	s[5] = h[1] >> 14;
	s[6] = (h[1] >> 22) | (h[2] << 3);
	s[7] = h[2] >> 5;
	s[8] = h[2] >> 13;
	s[9] = (h[2] >> 21) | (h[3] << 5);
	s[10] = h[3] >> 3;
	s[11] = h[3] >> 11;
	s[12] = (h[3] >> 19) | (h[4] << 6);
	s[13] = h[4] >> 2;
	s[14] = h[4] >> 10;
	s[15] = h[4] >> 18;
	s[16] = h[5] >> 0;
	s[17] = h[5] >> 8;
	s[18] = h[5] >> 16;
	s[19] = (h[5] >> 24) | (h[6] << 1);
	s[20] = h[6] >> 7;
	s[21] = h[6] >> 15;
	s[22] = (h[6] >> 23) | (h[7] << 3);
	s[23] = h[7] >> 5;
	s[24] = h[7] >> 13;
	s[25] = (h[7] >> 21) | (h[8] << 4);
	s[26] = h[8] >> 4;
	s[27] = h[8] >> 12;
	s[28] = (h[8] >> 20) | (h[9] << 6);
	s[29] = h[9] >> 2;
	s[30] = h[9] >> 10;
	s[31] = h[9] >> 18;
}

/* h = f */
static inline void fe_copy(fe *h, const fe *f)
{
	memmove(h, f, sizeof(uint32_t) * 10);
}

static inline void fe_copy_lt(fe_loose *h, const fe *f)
{
	memmove(h, f, sizeof(uint32_t) * 10);
}

/* h = 0 */
static inline void fe_0(fe *h)
{
	memset(h, 0, sizeof(uint32_t) * 10);
}

/* h = 1 */
static inline void fe_1(fe *h)
{
	memset(h, 0, sizeof(uint32_t) * 10);
	h->v[0] = 1;
}

static void fe_add_impl(uint32_t out[10], const uint32_t in1[10], const uint32_t in2[10])
{
	const uint32_t x20 = in1[9];
	const uint32_t x21 = in1[8];
	const uint32_t x19 = in1[7];
	const uint32_t x17 = in1[6];
	const uint32_t x15 = in1[5];
	const uint32_t x13 = in1[4];
	const uint32_t x11 = in1[3];
	const uint32_t x9 = in1[2];
	const uint32_t x7 = in1[1];
	const uint32_t x5 = in1[0];
	const uint32_t x38 = in2[9];
	const uint32_t x39 = in2[8];
	const uint32_t x37 = in2[7];
	const uint32_t x35 = in2[6];
	const uint32_t x33 = in2[5];
	const uint32_t x31 = in2[4];
	const uint32_t x29 = in2[3];
	const uint32_t x27 = in2[2];
	const uint32_t x25 = in2[1];
	const uint32_t x23 = in2[0];
	out[0] = (x5 + x23);
	out[1] = (x7 + x25);
	out[2] = (x9 + x27);
	out[3] = (x11 + x29);
	out[4] = (x13 + x31);
	out[5] = (x15 + x33);
	out[6] = (x17 + x35);
	out[7] = (x19 + x37);
	out[8] = (x21 + x39);
	out[9] = (x20 + x38);
}

/* h = f + g
 * Can overlap h with f or g.
 */
static inline void fe_add(fe_loose *h, const fe *f, const fe *g)
{
	fe_add_impl(h->v, f->v, g->v);
}

static void fe_sub_impl(uint32_t out[10], const uint32_t in1[10], const uint32_t in2[10])
{
	const uint32_t x20 = in1[9];
	const uint32_t x21 = in1[8];
	const uint32_t x19 = in1[7];
	const uint32_t x17 = in1[6];
	const uint32_t x15 = in1[5];
	const uint32_t x13 = in1[4];
	const uint32_t x11 = in1[3];
	const uint32_t x9 = in1[2];
	const uint32_t x7 = in1[1];
	const uint32_t x5 = in1[0];
	const uint32_t x38 = in2[9];
	const uint32_t x39 = in2[8];
	const uint32_t x37 = in2[7];
	const uint32_t x35 = in2[6];
	const uint32_t x33 = in2[5];
	const uint32_t x31 = in2[4];
	const uint32_t x29 = in2[3];
	const uint32_t x27 = in2[2];
	const uint32_t x25 = in2[1];
	const uint32_t x23 = in2[0];
	out[0] = ((0x7ffffda + x5) - x23);
	out[1] = ((0x3fffffe + x7) - x25);
	out[2] = ((0x7fffffe + x9) - x27);
	out[3] = ((0x3fffffe + x11) - x29);
	out[4] = ((0x7fffffe + x13) - x31);
	out[5] = ((0x3fffffe + x15) - x33);
	out[6] = ((0x7fffffe + x17) - x35);
	out[7] = ((0x3fffffe + x19) - x37);
	out[8] = ((0x7fffffe + x21) - x39);
	out[9] = ((0x3fffffe + x20) - x38);
}

/* h = f - g
 * Can overlap h with f or g.
 */
static inline void fe_sub(fe_loose *h, const fe *f, const fe *g)
{
	fe_sub_impl(h->v, f->v, g->v);
}

static void fe_mul_impl(uint32_t out[10], const uint32_t in1[10], const uint32_t in2[10])
{
	const uint32_t x20 = in1[9];
	const uint32_t x21 = in1[8];
	const uint32_t x19 = in1[7];
	const uint32_t x17 = in1[6];
	const uint32_t x15 = in1[5];
	const uint32_t x13 = in1[4];
	const uint32_t x11 = in1[3];
	const uint32_t x9 = in1[2];
	const uint32_t x7 = in1[1];
	const uint32_t x5 = in1[0];
	const uint32_t x38 = in2[9];
	const uint32_t x39 = in2[8];
	const uint32_t x37 = in2[7];
	const uint32_t x35 = in2[6];
	const uint32_t x33 = in2[5];
	const uint32_t x31 = in2[4];
	const uint32_t x29 = in2[3];
	const uint32_t x27 = in2[2];
	const uint32_t x25 = in2[1];
	const uint32_t x23 = in2[0];
	uint64_t x40 = ((uint64_t)x23 * x5);
	uint64_t x41 = (((uint64_t)x23 * x7) + ((uint64_t)x25 * x5));
	uint64_t x42 = ((((uint64_t)(0x2 * x25) * x7) + ((uint64_t)x23 * x9)) + ((uint64_t)x27 * x5));
	uint64_t x43 = (((((uint64_t)x25 * x9) + ((uint64_t)x27 * x7)) + ((uint64_t)x23 * x11)) + ((uint64_t)x29 * x5));
	uint64_t x44 = (((((uint64_t)x27 * x9) + (0x2 * (((uint64_t)x25 * x11) + ((uint64_t)x29 * x7)))) + ((uint64_t)x23 * x13)) + ((uint64_t)x31 * x5));
	uint64_t x45 = (((((((uint64_t)x27 * x11) + ((uint64_t)x29 * x9)) + ((uint64_t)x25 * x13)) + ((uint64_t)x31 * x7)) + ((uint64_t)x23 * x15)) + ((uint64_t)x33 * x5));
	uint64_t x46 = (((((0x2 * ((((uint64_t)x29 * x11) + ((uint64_t)x25 * x15)) + ((uint64_t)x33 * x7))) + ((uint64_t)x27 * x13)) + ((uint64_t)x31 * x9)) + ((uint64_t)x23 * x17)) + ((uint64_t)x35 * x5));
	uint64_t x47 = (((((((((uint64_t)x29 * x13) + ((uint64_t)x31 * x11)) + ((uint64_t)x27 * x15)) + ((uint64_t)x33 * x9)) + ((uint64_t)x25 * x17)) + ((uint64_t)x35 * x7)) + ((uint64_t)x23 * x19)) + ((uint64_t)x37 * x5));
	uint64_t x48 = (((((((uint64_t)x31 * x13) + (0x2 * (((((uint64_t)x29 * x15) + ((uint64_t)x33 * x11)) + ((uint64_t)x25 * x19)) + ((uint64_t)x37 * x7)))) + ((uint64_t)x27 * x17)) + ((uint64_t)x35 * x9)) + ((uint64_t)x23 * x21)) + ((uint64_t)x39 * x5));
	uint64_t x49 = (((((((((((uint64_t)x31 * x15) + ((uint64_t)x33 * x13)) + ((uint64_t)x29 * x17)) + ((uint64_t)x35 * x11)) + ((uint64_t)x27 * x19)) + ((uint64_t)x37 * x9)) + ((uint64_t)x25 * x21)) + ((uint64_t)x39 * x7)) + ((uint64_t)x23 * x20)) + ((uint64_t)x38 * x5));
	uint64_t x50 = (((((0x2 * ((((((uint64_t)x33 * x15) + ((uint64_t)x29 * x19)) + ((uint64_t)x37 * x11)) + ((uint64_t)x25 * x20)) + ((uint64_t)x38 * x7))) + ((uint64_t)x31 * x17)) + ((uint64_t)x35 * x13)) + ((uint64_t)x27 * x21)) + ((uint64_t)x39 * x9));
	uint64_t x51 = (((((((((uint64_t)x33 * x17) + ((uint64_t)x35 * x15)) + ((uint64_t)x31 * x19)) + ((uint64_t)x37 * x13)) + ((uint64_t)x29 * x21)) + ((uint64_t)x39 * x11)) + ((uint64_t)x27 * x20)) + ((uint64_t)x38 * x9));
	uint64_t x52 = (((((uint64_t)x35 * x17) + (0x2 * (((((uint64_t)x33 * x19) + ((uint64_t)x37 * x15)) + ((uint64_t)x29 * x20)) + ((uint64_t)x38 * x11)))) + ((uint64_t)x31 * x21)) + ((uint64_t)x39 * x13));
	uint64_t x53 = (((((((uint64_t)x35 * x19) + ((uint64_t)x37 * x17)) + ((uint64_t)x33 * x21)) + ((uint64_t)x39 * x15)) + ((uint64_t)x31 * x20)) + ((uint64_t)x38 * x13));
	uint64_t x54 = (((0x2 * ((((uint64_t)x37 * x19) + ((uint64_t)x33 * x20)) + ((uint64_t)x38 * x15))) + ((uint64_t)x35 * x21)) + ((uint64_t)x39 * x17));
	uint64_t x55 = (((((uint64_t)x37 * x21) + ((uint64_t)x39 * x19)) + ((uint64_t)x35 * x20)) + ((uint64_t)x38 * x17));
	uint64_t x56 = (((uint64_t)x39 * x21) + (0x2 * (((uint64_t)x37 * x20) + ((uint64_t)x38 * x19))));
	uint64_t x57 = (((uint64_t)x39 * x20) + ((uint64_t)x38 * x21));
	uint64_t x58 = ((uint64_t)(0x2 * x38) * x20);
	uint64_t x59 = (x48 + (x58 << 0x4));
	uint64_t x60 = (x59 + (x58 << 0x1));
	uint64_t x61 = (x60 + x58);
	uint64_t x62 = (x47 + (x57 << 0x4));
	uint64_t x63 = (x62 + (x57 << 0x1));
	uint64_t x64 = (x63 + x57);
	uint64_t x65 = (x46 + (x56 << 0x4));
	uint64_t x66 = (x65 + (x56 << 0x1));
	uint64_t x67 = (x66 + x56);
	uint64_t x68 = (x45 + (x55 << 0x4));
	uint64_t x69 = (x68 + (x55 << 0x1));
	uint64_t x70 = (x69 + x55);
	uint64_t x71 = (x44 + (x54 << 0x4));
	uint64_t x72 = (x71 + (x54 << 0x1));
	uint64_t x73 = (x72 + x54);
	uint64_t x74 = (x43 + (x53 << 0x4));
	uint64_t x75 = (x74 + (x53 << 0x1));
	uint64_t x76 = (x75 + x53);
	uint64_t x77 = (x42 + (x52 << 0x4));
	uint64_t x78 = (x77 + (x52 << 0x1));
	uint64_t x79 = (x78 + x52);
	uint64_t x80 = (x41 + (x51 << 0x4));
	uint64_t x81 = (x80 + (x51 << 0x1));
	uint64_t x82 = (x81 + x51);
	uint64_t x83 = (x40 + (x50 << 0x4));
	uint64_t x84 = (x83 + (x50 << 0x1));
	uint64_t x85 = (x84 + x50);
	uint64_t x86 = (x85 >> 0x1a);
	uint32_t x87 = ((uint32_t)x85 & 0x3ffffff);
	uint64_t x88 = (x86 + x82);
	uint64_t x89 = (x88 >> 0x19);
	uint32_t x90 = ((uint32_t)x88 & 0x1ffffff);
	uint64_t x91 = (x89 + x79);
	uint64_t x92 = (x91 >> 0x1a);
	uint32_t x93 = ((uint32_t)x91 & 0x3ffffff);
	uint64_t x94 = (x92 + x76);
	uint64_t x95 = (x94 >> 0x19);
	uint32_t x96 = ((uint32_t)x94 & 0x1ffffff);
	uint64_t x97 = (x95 + x73);
	uint64_t x98 = (x97 >> 0x1a);
	uint32_t x99 = ((uint32_t)x97 & 0x3ffffff);
	uint64_t x100 = (x98 + x70);
	uint64_t x101 = (x100 >> 0x19);
	uint32_t x102 = ((uint32_t)x100 & 0x1ffffff);
	uint64_t x103 = (x101 + x67);
	uint64_t x104 = (x103 >> 0x1a);
	uint32_t x105 = ((uint32_t)x103 & 0x3ffffff);
	uint64_t x106 = (x104 + x64);
	uint64_t x107 = (x106 >> 0x19);
	uint32_t x108 = ((uint32_t)x106 & 0x1ffffff);
	uint64_t x109 = (x107 + x61);
	uint64_t x110 = (x109 >> 0x1a);
	uint32_t x111 = ((uint32_t)x109 & 0x3ffffff);
	uint64_t x112 = (x110 + x49);
	uint64_t x113 = (x112 >> 0x19);
	uint32_t x114 = ((uint32_t)x112 & 0x1ffffff);
	uint64_t x115 = (x87 + (0x13 * x113));
	uint32_t x116 = (uint32_t) (x115 >> 0x1a);
	uint32_t x117 = ((uint32_t)x115 & 0x3ffffff);
	uint32_t x118 = (x116 + x90);
	uint32_t x119 = (x118 >> 0x19);
	uint32_t x120 = (x118 & 0x1ffffff);
	out[0] = x117;
	out[1] = x120;
	out[2] = (x119 + x93);
	out[3] = x96;
	out[4] = x99;
	out[5] = x102;
	out[6] = x105;
	out[7] = x108;
	out[8] = x111;
	out[9] = x114;
}

static inline void fe_mul_ttt(fe *h, const fe *f, const fe *g)
{
	fe_mul_impl(h->v, f->v, g->v);
}

static inline void fe_mul_tlt(fe *h, const fe_loose *f, const fe *g)
{
	fe_mul_impl(h->v, f->v, g->v);
}

static inline void
fe_mul_tll(fe *h, const fe_loose *f, const fe_loose *g)
{
	fe_mul_impl(h->v, f->v, g->v);
}

static void fe_sqr_impl(uint32_t out[10], const uint32_t in1[10])
{
	const uint32_t x17 = in1[9];
	const uint32_t x18 = in1[8];
	const uint32_t x16 = in1[7];
	const uint32_t x14 = in1[6];
	const uint32_t x12 = in1[5];
	const uint32_t x10 = in1[4];
	const uint32_t x8 = in1[3];
	const uint32_t x6 = in1[2];
	const uint32_t x4 = in1[1];
	const uint32_t x2 = in1[0];
	uint64_t x19 = ((uint64_t)x2 * x2);
	uint64_t x20 = ((uint64_t)(0x2 * x2) * x4);
	uint64_t x21 = (0x2 * (((uint64_t)x4 * x4) + ((uint64_t)x2 * x6)));
	uint64_t x22 = (0x2 * (((uint64_t)x4 * x6) + ((uint64_t)x2 * x8)));
	uint64_t x23 = ((((uint64_t)x6 * x6) + ((uint64_t)(0x4 * x4) * x8)) + ((uint64_t)(0x2 * x2) * x10));
	uint64_t x24 = (0x2 * ((((uint64_t)x6 * x8) + ((uint64_t)x4 * x10)) + ((uint64_t)x2 * x12)));
	uint64_t x25 = (0x2 * (((((uint64_t)x8 * x8) + ((uint64_t)x6 * x10)) + ((uint64_t)x2 * x14)) + ((uint64_t)(0x2 * x4) * x12)));
	uint64_t x26 = (0x2 * (((((uint64_t)x8 * x10) + ((uint64_t)x6 * x12)) + ((uint64_t)x4 * x14)) + ((uint64_t)x2 * x16)));
	uint64_t x27 = (((uint64_t)x10 * x10) + (0x2 * ((((uint64_t)x6 * x14) + ((uint64_t)x2 * x18)) + (0x2 * (((uint64_t)x4 * x16) + ((uint64_t)x8 * x12))))));
	uint64_t x28 = (0x2 * ((((((uint64_t)x10 * x12) + ((uint64_t)x8 * x14)) + ((uint64_t)x6 * x16)) + ((uint64_t)x4 * x18)) + ((uint64_t)x2 * x17)));
	uint64_t x29 = (0x2 * (((((uint64_t)x12 * x12) + ((uint64_t)x10 * x14)) + ((uint64_t)x6 * x18)) + (0x2 * (((uint64_t)x8 * x16) + ((uint64_t)x4 * x17)))));
	uint64_t x30 = (0x2 * (((((uint64_t)x12 * x14) + ((uint64_t)x10 * x16)) + ((uint64_t)x8 * x18)) + ((uint64_t)x6 * x17)));
	uint64_t x31 = (((uint64_t)x14 * x14) + (0x2 * (((uint64_t)x10 * x18) + (0x2 * (((uint64_t)x12 * x16) + ((uint64_t)x8 * x17))))));
	uint64_t x32 = (0x2 * ((((uint64_t)x14 * x16) + ((uint64_t)x12 * x18)) + ((uint64_t)x10 * x17)));
	uint64_t x33 = (0x2 * ((((uint64_t)x16 * x16) + ((uint64_t)x14 * x18)) + ((uint64_t)(0x2 * x12) * x17)));
	uint64_t x34 = (0x2 * (((uint64_t)x16 * x18) + ((uint64_t)x14 * x17)));
	uint64_t x35 = (((uint64_t)x18 * x18) + ((uint64_t)(0x4 * x16) * x17));
	uint64_t x36 = ((uint64_t)(0x2 * x18) * x17);
	uint64_t x37 = ((uint64_t)(0x2 * x17) * x17);
	uint64_t x38 = (x27 + (x37 << 0x4));
	uint64_t x39 = (x38 + (x37 << 0x1));
	uint64_t x40 = (x39 + x37);
	uint64_t x41 = (x26 + (x36 << 0x4));
	uint64_t x42 = (x41 + (x36 << 0x1));
	uint64_t x43 = (x42 + x36);
	uint64_t x44 = (x25 + (x35 << 0x4));
	uint64_t x45 = (x44 + (x35 << 0x1));
	uint64_t x46 = (x45 + x35);
	uint64_t x47 = (x24 + (x34 << 0x4));
	uint64_t x48 = (x47 + (x34 << 0x1));
	uint64_t x49 = (x48 + x34);
	uint64_t x50 = (x23 + (x33 << 0x4));
	uint64_t x51 = (x50 + (x33 << 0x1));
	uint64_t x52 = (x51 + x33);
	uint64_t x53 = (x22 + (x32 << 0x4));
	uint64_t x54 = (x53 + (x32 << 0x1));
	uint64_t x55 = (x54 + x32);
	uint64_t x56 = (x21 + (x31 << 0x4));
	uint64_t x57 = (x56 + (x31 << 0x1));
	uint64_t x58 = (x57 + x31);
	uint64_t x59 = (x20 + (x30 << 0x4));
	uint64_t x60 = (x59 + (x30 << 0x1));
	uint64_t x61 = (x60 + x30);
	uint64_t x62 = (x19 + (x29 << 0x4));
	uint64_t x63 = (x62 + (x29 << 0x1));
	uint64_t x64 = (x63 + x29);
	uint64_t x65 = (x64 >> 0x1a);
	uint32_t x66 = ((uint32_t)x64 & 0x3ffffff);
	uint64_t x67 = (x65 + x61);
	uint64_t x68 = (x67 >> 0x19);
	uint32_t x69 = ((uint32_t)x67 & 0x1ffffff);
	uint64_t x70 = (x68 + x58);
	uint64_t x71 = (x70 >> 0x1a);
	uint32_t x72 = ((uint32_t)x70 & 0x3ffffff);
	uint64_t x73 = (x71 + x55);
	uint64_t x74 = (x73 >> 0x19);
	uint32_t x75 = ((uint32_t)x73 & 0x1ffffff);
	uint64_t x76 = (x74 + x52);
	uint64_t x77 = (x76 >> 0x1a);
	uint32_t x78 = ((uint32_t)x76 & 0x3ffffff);
	uint64_t x79 = (x77 + x49);
	uint64_t x80 = (x79 >> 0x19);
	uint32_t x81 = ((uint32_t)x79 & 0x1ffffff);
	uint64_t x82 = (x80 + x46);
	uint64_t x83 = (x82 >> 0x1a);
	uint32_t x84 = ((uint32_t)x82 & 0x3ffffff);
	uint64_t x85 = (x83 + x43);
	uint64_t x86 = (x85 >> 0x19);
	uint32_t x87 = ((uint32_t)x85 & 0x1ffffff);
	uint64_t x88 = (x86 + x40);
	uint64_t x89 = (x88 >> 0x1a);
	uint32_t x90 = ((uint32_t)x88 & 0x3ffffff);
	uint64_t x91 = (x89 + x28);
	uint64_t x92 = (x91 >> 0x19);
	uint32_t x93 = ((uint32_t)x91 & 0x1ffffff);
	uint64_t x94 = (x66 + (0x13 * x92));
	uint32_t x95 = (uint32_t) (x94 >> 0x1a);
	uint32_t x96 = ((uint32_t)x94 & 0x3ffffff);
	uint32_t x97 = (x95 + x69);
	uint32_t x98 = (x97 >> 0x19);
	uint32_t x99 = (x97 & 0x1ffffff);
	out[0] = x96;
	out[1] = x99;
	out[2] = (x98 + x72);
	out[3] = x75;
	out[4] = x78;
	out[5] = x81;
	out[6] = x84;
	out[7] = x87;
	out[8] = x90;
	out[9] = x93;
}

static inline void fe_sq_tl(fe *h, const fe_loose *f)
{
	fe_sqr_impl(h->v, f->v);
}

static inline void fe_sq_tt(fe *h, const fe *f)
{
	fe_sqr_impl(h->v, f->v);
}

static inline void fe_loose_invert(fe *out, const fe_loose *z)
{
	fe t0;
	fe t1;
	fe t2;
	fe t3;
	int i;

	fe_sq_tl(&t0, z);
	fe_sq_tt(&t1, &t0);
	for (i = 1; i < 2; ++i)
		fe_sq_tt(&t1, &t1);
	fe_mul_tlt(&t1, z, &t1);
	fe_mul_ttt(&t0, &t0, &t1);
	fe_sq_tt(&t2, &t0);
	fe_mul_ttt(&t1, &t1, &t2);
	fe_sq_tt(&t2, &t1);
	for (i = 1; i < 5; ++i)
		fe_sq_tt(&t2, &t2);
	fe_mul_ttt(&t1, &t2, &t1);
	fe_sq_tt(&t2, &t1);
	for (i = 1; i < 10; ++i)
		fe_sq_tt(&t2, &t2);
	fe_mul_ttt(&t2, &t2, &t1);
	fe_sq_tt(&t3, &t2);
	for (i = 1; i < 20; ++i)
		fe_sq_tt(&t3, &t3);
	fe_mul_ttt(&t2, &t3, &t2);
	fe_sq_tt(&t2, &t2);
	for (i = 1; i < 10; ++i)
		fe_sq_tt(&t2, &t2);
	fe_mul_ttt(&t1, &t2, &t1);
	fe_sq_tt(&t2, &t1);
	for (i = 1; i < 50; ++i)
		fe_sq_tt(&t2, &t2);
	fe_mul_ttt(&t2, &t2, &t1);
	fe_sq_tt(&t3, &t2);
	for (i = 1; i < 100; ++i)
		fe_sq_tt(&t3, &t3);
	fe_mul_ttt(&t2, &t3, &t2);
	fe_sq_tt(&t2, &t2);
	for (i = 1; i < 50; ++i)
		fe_sq_tt(&t2, &t2);
	fe_mul_ttt(&t1, &t2, &t1);
	fe_sq_tt(&t1, &t1);
	for (i = 1; i < 5; ++i)
		fe_sq_tt(&t1, &t1);
	fe_mul_ttt(out, &t1, &t0);
}

static inline void fe_invert(fe *out, const fe *z)
{
	fe_loose l;
	fe_copy_lt(&l, z);
	fe_loose_invert(out, &l);
}

/* Replace (f,g) with (g,f) if b == 1;
 * replace (f,g) with (f,g) if b == 0.
 *
 * Preconditions: b in {0,1}
 */
static inline void fe_cswap(fe *f, fe *g, unsigned int b)
{
	unsigned i;
	b = 0 - b;
	for (i = 0; i < 10; i++) {
		uint32_t x = f->v[i] ^ g->v[i];
		x &= b;
		f->v[i] ^= x;
		g->v[i] ^= x;
	}
}

/* NOTE: based on fiat-crypto fe_mul, edited for in2=121666, 0, 0.*/
static inline void fe_mul_121666_impl(uint32_t out[10], const uint32_t in1[10])
{
	const uint32_t x20 = in1[9];
	const uint32_t x21 = in1[8];
	const uint32_t x19 = in1[7];
	const uint32_t x17 = in1[6];
	const uint32_t x15 = in1[5];
	const uint32_t x13 = in1[4];
	const uint32_t x11 = in1[3];
	const uint32_t x9 = in1[2];
	const uint32_t x7 = in1[1];
	const uint32_t x5 = in1[0];
	const uint32_t x38 = 0;
	const uint32_t x39 = 0;
	const uint32_t x37 = 0;
	const uint32_t x35 = 0;
	const uint32_t x33 = 0;
	const uint32_t x31 = 0;
	const uint32_t x29 = 0;
	const uint32_t x27 = 0;
	const uint32_t x25 = 0;
	const uint32_t x23 = 121666;
	uint64_t x40 = ((uint64_t)x23 * x5);
	uint64_t x41 = (((uint64_t)x23 * x7) + ((uint64_t)x25 * x5));
	uint64_t x42 = ((((uint64_t)(0x2 * x25) * x7) + ((uint64_t)x23 * x9)) + ((uint64_t)x27 * x5));
	uint64_t x43 = (((((uint64_t)x25 * x9) + ((uint64_t)x27 * x7)) + ((uint64_t)x23 * x11)) + ((uint64_t)x29 * x5));
	uint64_t x44 = (((((uint64_t)x27 * x9) + (0x2 * (((uint64_t)x25 * x11) + ((uint64_t)x29 * x7)))) + ((uint64_t)x23 * x13)) + ((uint64_t)x31 * x5));
	uint64_t x45 = (((((((uint64_t)x27 * x11) + ((uint64_t)x29 * x9)) + ((uint64_t)x25 * x13)) + ((uint64_t)x31 * x7)) + ((uint64_t)x23 * x15)) + ((uint64_t)x33 * x5));
	uint64_t x46 = (((((0x2 * ((((uint64_t)x29 * x11) + ((uint64_t)x25 * x15)) + ((uint64_t)x33 * x7))) + ((uint64_t)x27 * x13)) + ((uint64_t)x31 * x9)) + ((uint64_t)x23 * x17)) + ((uint64_t)x35 * x5));
	uint64_t x47 = (((((((((uint64_t)x29 * x13) + ((uint64_t)x31 * x11)) + ((uint64_t)x27 * x15)) + ((uint64_t)x33 * x9)) + ((uint64_t)x25 * x17)) + ((uint64_t)x35 * x7)) + ((uint64_t)x23 * x19)) + ((uint64_t)x37 * x5));
	uint64_t x48 = (((((((uint64_t)x31 * x13) + (0x2 * (((((uint64_t)x29 * x15) + ((uint64_t)x33 * x11)) + ((uint64_t)x25 * x19)) + ((uint64_t)x37 * x7)))) + ((uint64_t)x27 * x17)) + ((uint64_t)x35 * x9)) + ((uint64_t)x23 * x21)) + ((uint64_t)x39 * x5));
	uint64_t x49 = (((((((((((uint64_t)x31 * x15) + ((uint64_t)x33 * x13)) + ((uint64_t)x29 * x17)) + ((uint64_t)x35 * x11)) + ((uint64_t)x27 * x19)) + ((uint64_t)x37 * x9)) + ((uint64_t)x25 * x21)) + ((uint64_t)x39 * x7)) + ((uint64_t)x23 * x20)) + ((uint64_t)x38 * x5));
	uint64_t x50 = (((((0x2 * ((((((uint64_t)x33 * x15) + ((uint64_t)x29 * x19)) + ((uint64_t)x37 * x11)) + ((uint64_t)x25 * x20)) + ((uint64_t)x38 * x7))) + ((uint64_t)x31 * x17)) + ((uint64_t)x35 * x13)) + ((uint64_t)x27 * x21)) + ((uint64_t)x39 * x9));
	uint64_t x51 = (((((((((uint64_t)x33 * x17) + ((uint64_t)x35 * x15)) + ((uint64_t)x31 * x19)) + ((uint64_t)x37 * x13)) + ((uint64_t)x29 * x21)) + ((uint64_t)x39 * x11)) + ((uint64_t)x27 * x20)) + ((uint64_t)x38 * x9));
	uint64_t x52 = (((((uint64_t)x35 * x17) + (0x2 * (((((uint64_t)x33 * x19) + ((uint64_t)x37 * x15)) + ((uint64_t)x29 * x20)) + ((uint64_t)x38 * x11)))) + ((uint64_t)x31 * x21)) + ((uint64_t)x39 * x13));
	uint64_t x53 = (((((((uint64_t)x35 * x19) + ((uint64_t)x37 * x17)) + ((uint64_t)x33 * x21)) + ((uint64_t)x39 * x15)) + ((uint64_t)x31 * x20)) + ((uint64_t)x38 * x13));
	uint64_t x54 = (((0x2 * ((((uint64_t)x37 * x19) + ((uint64_t)x33 * x20)) + ((uint64_t)x38 * x15))) + ((uint64_t)x35 * x21)) + ((uint64_t)x39 * x17));
	uint64_t x55 = (((((uint64_t)x37 * x21) + ((uint64_t)x39 * x19)) + ((uint64_t)x35 * x20)) + ((uint64_t)x38 * x17));
	uint64_t x56 = (((uint64_t)x39 * x21) + (0x2 * (((uint64_t)x37 * x20) + ((uint64_t)x38 * x19))));
	uint64_t x57 = (((uint64_t)x39 * x20) + ((uint64_t)x38 * x21));
	uint64_t x58 = ((uint64_t)(0x2 * x38) * x20);
	uint64_t x59 = (x48 + (x58 << 0x4));
	uint64_t x60 = (x59 + (x58 << 0x1));
	uint64_t x61 = (x60 + x58);
	uint64_t x62 = (x47 + (x57 << 0x4));
	uint64_t x63 = (x62 + (x57 << 0x1));
	uint64_t x64 = (x63 + x57);
	uint64_t x65 = (x46 + (x56 << 0x4));
	uint64_t x66 = (x65 + (x56 << 0x1));
	uint64_t x67 = (x66 + x56);
	uint64_t x68 = (x45 + (x55 << 0x4));
	uint64_t x69 = (x68 + (x55 << 0x1));
	uint64_t x70 = (x69 + x55);
	uint64_t x71 = (x44 + (x54 << 0x4));
	uint64_t x72 = (x71 + (x54 << 0x1));
	uint64_t x73 = (x72 + x54);
	uint64_t x74 = (x43 + (x53 << 0x4));
	uint64_t x75 = (x74 + (x53 << 0x1));
	uint64_t x76 = (x75 + x53);
	uint64_t x77 = (x42 + (x52 << 0x4));
	uint64_t x78 = (x77 + (x52 << 0x1));
	uint64_t x79 = (x78 + x52);
	uint64_t x80 = (x41 + (x51 << 0x4));
	uint64_t x81 = (x80 + (x51 << 0x1));
	uint64_t x82 = (x81 + x51);
	uint64_t x83 = (x40 + (x50 << 0x4));
	uint64_t x84 = (x83 + (x50 << 0x1));
	uint64_t x85 = (x84 + x50);
	uint64_t x86 = (x85 >> 0x1a);
	uint32_t x87 = ((uint32_t)x85 & 0x3ffffff);
	uint64_t x88 = (x86 + x82);
	uint64_t x89 = (x88 >> 0x19);
	uint32_t x90 = ((uint32_t)x88 & 0x1ffffff);
	uint64_t x91 = (x89 + x79);
	uint64_t x92 = (x91 >> 0x1a);
	uint32_t x93 = ((uint32_t)x91 & 0x3ffffff);
	uint64_t x94 = (x92 + x76);
	uint64_t x95 = (x94 >> 0x19);
	uint32_t x96 = ((uint32_t)x94 & 0x1ffffff);
	uint64_t x97 = (x95 + x73);
	uint64_t x98 = (x97 >> 0x1a);
	uint32_t x99 = ((uint32_t)x97 & 0x3ffffff);
	uint64_t x100 = (x98 + x70);
	uint64_t x101 = (x100 >> 0x19);
	uint32_t x102 = ((uint32_t)x100 & 0x1ffffff);
	uint64_t x103 = (x101 + x67);
	uint64_t x104 = (x103 >> 0x1a);
	uint32_t x105 = ((uint32_t)x103 & 0x3ffffff);
	uint64_t x106 = (x104 + x64);
	uint64_t x107 = (x106 >> 0x19);
	uint32_t x108 = ((uint32_t)x106 & 0x1ffffff);
	uint64_t x109 = (x107 + x61);
	uint64_t x110 = (x109 >> 0x1a);
	uint32_t x111 = ((uint32_t)x109 & 0x3ffffff);
	uint64_t x112 = (x110 + x49);
	uint64_t x113 = (x112 >> 0x19);
	uint32_t x114 = ((uint32_t)x112 & 0x1ffffff);
	uint64_t x115 = (x87 + (0x13 * x113));
	uint32_t x116 = (uint32_t) (x115 >> 0x1a);
	uint32_t x117 = ((uint32_t)x115 & 0x3ffffff);
	uint32_t x118 = (x116 + x90);
	uint32_t x119 = (x118 >> 0x19);
	uint32_t x120 = (x118 & 0x1ffffff);
	out[0] = x117;
	out[1] = x120;
	out[2] = (x119 + x93);
	out[3] = x96;
	out[4] = x99;
	out[5] = x102;
	out[6] = x105;
	out[7] = x108;
	out[8] = x111;
	out[9] = x114;
}

static inline void fe_mul121666(fe *h, const fe_loose *f)
{
	fe_mul_121666_impl(h->v, f->v);
}

static const uint8_t curve25519_null_point[CURVE25519_KEY_SIZE];

bool curve25519(uint8_t out[CURVE25519_KEY_SIZE],
		const uint8_t scalar[CURVE25519_KEY_SIZE],
		const uint8_t point[CURVE25519_KEY_SIZE])
{
	fe x1, x2, z2, x3, z3;
	fe_loose x2l, z2l, x3l;
	unsigned swap = 0;
	int pos;
	uint8_t e[32];

	memcpy(e, scalar, 32);
	curve25519_clamp_secret(e);

	/* The following implementation was transcribed to Coq and proven to
	 * correspond to unary scalar multiplication in affine coordinates given
	 * that x1 != 0 is the x coordinate of some point on the curve. It was
	 * also checked in Coq that doing a ladderstep with x1 = x3 = 0 gives
	 * z2' = z3' = 0, and z2 = z3 = 0 gives z2' = z3' = 0. The statement was
	 * quantified over the underlying field, so it applies to Curve25519
	 * itself and the quadratic twist of Curve25519. It was not proven in
	 * Coq that prime-field arithmetic correctly simulates extension-field
	 * arithmetic on prime-field values. The decoding of the byte array
	 * representation of e was not considered.
	 *
	 * Specification of Montgomery curves in affine coordinates:
	 * <https://github.com/mit-plv/fiat-crypto/blob/2456d821825521f7e03e65882cc3521795b0320f/src/Spec/MontgomeryCurve.v#L27>
	 *
	 * Proof that these form a group that is isomorphic to a Weierstrass
	 * curve:
	 * <https://github.com/mit-plv/fiat-crypto/blob/2456d821825521f7e03e65882cc3521795b0320f/src/Curves/Montgomery/AffineProofs.v#L35>
	 *
	 * Coq transcription and correctness proof of the loop
	 * (where scalarbits=255):
	 * <https://github.com/mit-plv/fiat-crypto/blob/2456d821825521f7e03e65882cc3521795b0320f/src/Curves/Montgomery/XZ.v#L118>
	 * <https://github.com/mit-plv/fiat-crypto/blob/2456d821825521f7e03e65882cc3521795b0320f/src/Curves/Montgomery/XZProofs.v#L278>
	 * preconditions: 0 <= e < 2^255 (not necessarily e < order),
	 * fe_invert(0) = 0
	 */
	fe_frombytes(&x1, point);
	fe_1(&x2);
	fe_0(&z2);
	fe_copy(&x3, &x1);
	fe_1(&z3);

	for (pos = 254; pos >= 0; --pos) {
		fe tmp0, tmp1;
		fe_loose tmp0l, tmp1l;
		/* loop invariant as of right before the test, for the case
		 * where x1 != 0:
		 *   pos >= -1; if z2 = 0 then x2 is nonzero; if z3 = 0 then x3
		 *   is nonzero
		 *   let r := e >> (pos+1) in the following equalities of
		 *   projective points:
		 *   to_xz (r*P)     === if swap then (x3, z3) else (x2, z2)
		 *   to_xz ((r+1)*P) === if swap then (x2, z2) else (x3, z3)
		 *   x1 is the nonzero x coordinate of the nonzero
		 *   point (r*P-(r+1)*P)
		 */
		unsigned b = 1 & (e[pos / 8] >> (pos & 7));
		swap ^= b;
		fe_cswap(&x2, &x3, swap);
		fe_cswap(&z2, &z3, swap);
		swap = b;
		/* Coq transcription of ladderstep formula (called from
		 * transcribed loop):
		 * <https://github.com/mit-plv/fiat-crypto/blob/2456d821825521f7e03e65882cc3521795b0320f/src/Curves/Montgomery/XZ.v#L89>
		 * <https://github.com/mit-plv/fiat-crypto/blob/2456d821825521f7e03e65882cc3521795b0320f/src/Curves/Montgomery/XZProofs.v#L131>
		 * x1 != 0 <https://github.com/mit-plv/fiat-crypto/blob/2456d821825521f7e03e65882cc3521795b0320f/src/Curves/Montgomery/XZProofs.v#L217>
		 * x1  = 0 <https://github.com/mit-plv/fiat-crypto/blob/2456d821825521f7e03e65882cc3521795b0320f/src/Curves/Montgomery/XZProofs.v#L147>
		 */
		fe_sub(&tmp0l, &x3, &z3);
		fe_sub(&tmp1l, &x2, &z2);
		fe_add(&x2l, &x2, &z2);
		fe_add(&z2l, &x3, &z3);
		fe_mul_tll(&z3, &tmp0l, &x2l);
		fe_mul_tll(&z2, &z2l, &tmp1l);
		fe_sq_tl(&tmp0, &tmp1l);
		fe_sq_tl(&tmp1, &x2l);
		fe_add(&x3l, &z3, &z2);
		fe_sub(&z2l, &z3, &z2);
		fe_mul_ttt(&x2, &tmp1, &tmp0);
		fe_sub(&tmp1l, &tmp1, &tmp0);
		fe_sq_tl(&z2, &z2l);
		fe_mul121666(&z3, &tmp1l);
		fe_sq_tl(&x3, &x3l);
		fe_add(&tmp0l, &tmp0, &z3);
		fe_mul_ttt(&z3, &x1, &z2);
		fe_mul_tll(&z2, &tmp1l, &tmp0l);
	}
	/* here pos=-1, so r=e, so to_xz (e*P) === if swap then (x3, z3)
	 * else (x2, z2)
	 */
	fe_cswap(&x2, &x3, swap);
	fe_cswap(&z2, &z3, swap);

	fe_invert(&z2, &z2);
	fe_mul_ttt(&x2, &x2, &z2);
	fe_tobytes(out, &x2);

	explicit_bzero(&x1, sizeof(x1));
	explicit_bzero(&x2, sizeof(x2));
	explicit_bzero(&z2, sizeof(z2));
	explicit_bzero(&x3, sizeof(x3));
	explicit_bzero(&z3, sizeof(z3));
	explicit_bzero(&x2l, sizeof(x2l));
	explicit_bzero(&z2l, sizeof(z2l));
	explicit_bzero(&x3l, sizeof(x3l));
	explicit_bzero(&e, sizeof(e));

	return timingsafe_bcmp(out, curve25519_null_point, CURVE25519_KEY_SIZE) != 0;
}
#endif

int
crypto_init(void)
{
#ifndef COMPAT_NEED_CHACHA20POLY1305_MBUF
	struct crypto_session_params csp = {
		.csp_mode = CSP_MODE_AEAD,
		.csp_ivlen = sizeof(uint64_t),
		.csp_cipher_alg = CRYPTO_CHACHA20_POLY1305,
		.csp_cipher_klen = CHACHA20POLY1305_KEY_SIZE,
		.csp_flags = CSP_F_SEPARATE_AAD | CSP_F_SEPARATE_OUTPUT
	};
	int ret = crypto_newsession(&chacha20_poly1305_sid, &csp, CRYPTOCAP_F_SOFTWARE);
	if (ret != 0)
		return (ret);
#endif
	return (0);
}

void
crypto_deinit(void)
{
#ifndef COMPAT_NEED_CHACHA20POLY1305_MBUF
	crypto_freesession(chacha20_poly1305_sid);
#endif
}
