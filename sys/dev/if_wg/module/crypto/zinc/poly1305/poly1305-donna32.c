// SPDX-License-Identifier: GPL-2.0 OR MIT
/*
 * Copyright (C) 2015-2019 Jason A. Donenfeld <Jason@zx2c4.com>. All Rights Reserved.
 *
 * This is based in part on Andrew Moon's poly1305-donna, which is in the
 * public domain.
 */

struct poly1305_internal {
	u32 h[5];
	u32 r[5];
	u32 s[4];
};

static void poly1305_init_generic(void *ctx, const u8 key[16])
{
	struct poly1305_internal *st = (struct poly1305_internal *)ctx;

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

static void poly1305_blocks_generic(void *ctx, const u8 *input, size_t len,
				    const u32 padbit)
{
	struct poly1305_internal *st = (struct poly1305_internal *)ctx;
	const u32 hibit = padbit << 24;
	u32 r0, r1, r2, r3, r4;
	u32 s1, s2, s3, s4;
	u32 h0, h1, h2, h3, h4;
	u64 d0, d1, d2, d3, d4;
	u32 c;

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
		d0 = ((u64)h0 * r0) + ((u64)h1 * s4) +
		     ((u64)h2 * s3) + ((u64)h3 * s2) +
		     ((u64)h4 * s1);
		d1 = ((u64)h0 * r1) + ((u64)h1 * r0) +
		     ((u64)h2 * s4) + ((u64)h3 * s3) +
		     ((u64)h4 * s2);
		d2 = ((u64)h0 * r2) + ((u64)h1 * r1) +
		     ((u64)h2 * r0) + ((u64)h3 * s4) +
		     ((u64)h4 * s3);
		d3 = ((u64)h0 * r3) + ((u64)h1 * r2) +
		     ((u64)h2 * r1) + ((u64)h3 * r0) +
		     ((u64)h4 * s4);
		d4 = ((u64)h0 * r4) + ((u64)h1 * r3) +
		     ((u64)h2 * r2) + ((u64)h3 * r1) +
		     ((u64)h4 * r0);

		/* (partial) h %= p */
		c = (u32)(d0 >> 26);
		h0 = (u32)d0 & 0x3ffffff;
		d1 += c;
		c = (u32)(d1 >> 26);
		h1 = (u32)d1 & 0x3ffffff;
		d2 += c;
		c = (u32)(d2 >> 26);
		h2 = (u32)d2 & 0x3ffffff;
		d3 += c;
		c = (u32)(d3 >> 26);
		h3 = (u32)d3 & 0x3ffffff;
		d4 += c;
		c = (u32)(d4 >> 26);
		h4 = (u32)d4 & 0x3ffffff;
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

static void poly1305_emit_generic(void *ctx, u8 mac[16], const u32 nonce[4])
{
	struct poly1305_internal *st = (struct poly1305_internal *)ctx;
	u32 h0, h1, h2, h3, h4, c;
	u32 g0, g1, g2, g3, g4;
	u64 f;
	u32 mask;

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
	mask = (g4 >> ((sizeof(u32) * 8) - 1)) - 1;
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
	f = (u64)h0 + nonce[0];
	h0 = (u32)f;
	f = (u64)h1 + nonce[1] + (f >> 32);
	h1 = (u32)f;
	f = (u64)h2 + nonce[2] + (f >> 32);
	h2 = (u32)f;
	f = (u64)h3 + nonce[3] + (f >> 32);
	h3 = (u32)f;

	put_unaligned_le32(h0, &mac[0]);
	put_unaligned_le32(h1, &mac[4]);
	put_unaligned_le32(h2, &mac[8]);
	put_unaligned_le32(h3, &mac[12]);
}
