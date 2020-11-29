// SPDX-License-Identifier: GPL-2.0 OR MIT
/*
 * Copyright (C) 2015-2019 Jason A. Donenfeld <Jason@zx2c4.com>. All Rights Reserved.
 *
 * This is based in part on Andrew Moon's poly1305-donna, which is in the
 * public domain.
 */

typedef __uint128_t u128;

struct poly1305_internal {
	u64 r[3];
	u64 h[3];
	u64 s[2];
};

static void poly1305_init_generic(void *ctx, const u8 key[16])
{
	struct poly1305_internal *st = (struct poly1305_internal *)ctx;
	u64 t0, t1;

	/* r &= 0xffffffc0ffffffc0ffffffc0fffffff */
	t0 = get_unaligned_le64(&key[0]);
	t1 = get_unaligned_le64(&key[8]);

	st->r[0] = t0 & 0xffc0fffffffULL;
	st->r[1] = ((t0 >> 44) | (t1 << 20)) & 0xfffffc0ffffULL;
	st->r[2] = ((t1 >> 24)) & 0x00ffffffc0fULL;

	/* s = 20*r */
	st->s[0] = st->r[1] * 20;
	st->s[1] = st->r[2] * 20;

	/* h = 0 */
	st->h[0] = 0;
	st->h[1] = 0;
	st->h[2] = 0;
}

static void poly1305_blocks_generic(void *ctx, const u8 *input, size_t len,
				    const u32 padbit)
{
	struct poly1305_internal *st = (struct poly1305_internal *)ctx;
	const u64 hibit = ((u64)padbit) << 40;
	u64 r0, r1, r2;
	u64 s1, s2;
	u64 h0, h1, h2;
	u64 c;
	u128 d0, d1, d2, d;

	r0 = st->r[0];
	r1 = st->r[1];
	r2 = st->r[2];

	h0 = st->h[0];
	h1 = st->h[1];
	h2 = st->h[2];

	s1 = st->s[0];
	s2 = st->s[1];

	while (len >= POLY1305_BLOCK_SIZE) {
		u64 t0, t1;

		/* h += m[i] */
		t0 = get_unaligned_le64(&input[0]);
		t1 = get_unaligned_le64(&input[8]);

		h0 += t0 & 0xfffffffffffULL;
		h1 += ((t0 >> 44) | (t1 << 20)) & 0xfffffffffffULL;
		h2 += (((t1 >> 24)) & 0x3ffffffffffULL) | hibit;

		/* h *= r */
		d0 = (u128)h0 * r0;
		d = (u128)h1 * s2;
		d0 += d;
		d = (u128)h2 * s1;
		d0 += d;
		d1 = (u128)h0 * r1;
		d = (u128)h1 * r0;
		d1 += d;
		d = (u128)h2 * s2;
		d1 += d;
		d2 = (u128)h0 * r2;
		d = (u128)h1 * r1;
		d2 += d;
		d = (u128)h2 * r0;
		d2 += d;

		/* (partial) h %= p */
		c = (u64)(d0 >> 44);
		h0 = (u64)d0 & 0xfffffffffffULL;
		d1 += c;
		c = (u64)(d1 >> 44);
		h1 = (u64)d1 & 0xfffffffffffULL;
		d2 += c;
		c = (u64)(d2 >> 42);
		h2 = (u64)d2 & 0x3ffffffffffULL;
		h0 += c * 5;
		c = h0 >> 44;
		h0 = h0 & 0xfffffffffffULL;
		h1 += c;

		input += POLY1305_BLOCK_SIZE;
		len -= POLY1305_BLOCK_SIZE;
	}

	st->h[0] = h0;
	st->h[1] = h1;
	st->h[2] = h2;
}

static void poly1305_emit_generic(void *ctx, u8 mac[16], const u32 nonce[4])
{
	struct poly1305_internal *st = (struct poly1305_internal *)ctx;
	u64 h0, h1, h2, c;
	u64 g0, g1, g2;
	u64 t0, t1;

	/* fully carry h */
	h0 = st->h[0];
	h1 = st->h[1];
	h2 = st->h[2];

	c = h1 >> 44;
	h1 &= 0xfffffffffffULL;
	h2 += c;
	c = h2 >> 42;
	h2 &= 0x3ffffffffffULL;
	h0 += c * 5;
	c = h0 >> 44;
	h0 &= 0xfffffffffffULL;
	h1 += c;
	c = h1 >> 44;
	h1 &= 0xfffffffffffULL;
	h2 += c;
	c = h2 >> 42;
	h2 &= 0x3ffffffffffULL;
	h0 += c * 5;
	c = h0 >> 44;
	h0 &= 0xfffffffffffULL;
	h1 += c;

	/* compute h + -p */
	g0 = h0 + 5;
	c  = g0 >> 44;
	g0 &= 0xfffffffffffULL;
	g1 = h1 + c;
	c  = g1 >> 44;
	g1 &= 0xfffffffffffULL;
	g2 = h2 + c - (1ULL << 42);

	/* select h if h < p, or h + -p if h >= p */
	c = (g2 >> ((sizeof(u64) * 8) - 1)) - 1;
	g0 &= c;
	g1 &= c;
	g2 &= c;
	c  = ~c;
	h0 = (h0 & c) | g0;
	h1 = (h1 & c) | g1;
	h2 = (h2 & c) | g2;

	/* h = (h + nonce) */
	t0 = ((u64)nonce[1] << 32) | nonce[0];
	t1 = ((u64)nonce[3] << 32) | nonce[2];

	h0 += t0 & 0xfffffffffffULL;
	c = h0 >> 44;
	h0 &= 0xfffffffffffULL;
	h1 += (((t0 >> 44) | (t1 << 20)) & 0xfffffffffffULL) + c;
	c = h1 >> 44;
	h1 &= 0xfffffffffffULL;
	h2 += (((t1 >> 24)) & 0x3ffffffffffULL) + c;
	h2 &= 0x3ffffffffffULL;

	/* mac = h % (2^128) */
	h0 = h0 | (h1 << 44);
	h1 = (h1 >> 20) | (h2 << 24);

	put_unaligned_le64(h0, &mac[0]);
	put_unaligned_le64(h1, &mac[8]);
}
