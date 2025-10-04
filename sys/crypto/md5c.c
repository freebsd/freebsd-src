/*-
 * Copyright (c) 2024, 2025 Robert Clausecker <fuz@FreeBSD.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <sys/endian.h>
#include <sys/types.h>
#include <sys/md5.h>

#ifdef _KERNEL
#include <sys/param.h>
#include <sys/stdint.h>
#include <sys/systm.h>
#define assert(expr) MPASS(expr)
#else
#include <assert.h>
#include <stdint.h>
#include <string.h>
#include <strings.h>
#endif /* defined(_KERNEL) */

#define md5block _libmd_md5block
#ifdef MD5_ASM
extern void	md5block(MD5_CTX *, const void *, size_t);
#else
static void	md5block(MD5_CTX *, const void *, size_t);
#endif

/* don't unroll in bootloader */
#ifdef STANDALONE_SMALL
#define UNROLL
#else
#define UNROLL _Pragma("unroll")
#endif

void
MD5Init(MD5_CTX *ctx)
{
	ctx->state[0] = 0x67452301;
	ctx->state[1] = 0xefcdab89;
	ctx->state[2] = 0x98badcfe;
	ctx->state[3] = 0x10325476;

	ctx->count[0] = 0;
	ctx->count[1] = 0;
}

void
MD5Update(MD5_CTX *ctx, const void *data, unsigned int len)
{
	uint64_t nn;
	const char *p = data;
	unsigned num;

	num = ctx->count[0] % MD5_BLOCK_LENGTH;
	nn = (uint64_t)ctx->count[0] | (uint64_t)ctx->count[1] << 32;
	nn += len;
	ctx->count[0] = (uint32_t)nn;
	ctx->count[1] = (uint32_t)(nn >> 32);

	if (num > 0) {
		unsigned int n = MD5_BLOCK_LENGTH - num;

		if (n > len)
			n = len;

		memcpy((char *)ctx->buffer + num, p, n);
		num += n;
		if (num == MD5_BLOCK_LENGTH)
			md5block(ctx, (void *)ctx->buffer, MD5_BLOCK_LENGTH);

		p += n;
		len -= n;
	}

	if (len >= MD5_BLOCK_LENGTH) {
		unsigned n = len & ~(unsigned)(MD5_BLOCK_LENGTH - 1);

		md5block(ctx, p, n);
		p += n;
		len -= n;
	}

	if (len > 0)
		memcpy((void *)ctx->buffer, p, len);
}

static void
MD5Pad(MD5_CTX *ctx)
{
	uint64_t len;
	unsigned t;
	unsigned char tmp[MD5_BLOCK_LENGTH + sizeof(uint64_t)] = {0x80, 0};

	len = (uint64_t)ctx->count[0] | (uint64_t)ctx->count[1] << 32;
	t = 64 + 56 - ctx->count[0] % 64;
	if (t > 64)
		t -= 64;

	/* length in bits */
	len <<= 3;
	le64enc(tmp + t, len);
	MD5Update(ctx, tmp, t + 8);
	assert(ctx->count[0] % MD5_BLOCK_LENGTH == 0);
}

void
MD5Final(unsigned char md[16], MD5_CTX *ctx)
{
	MD5Pad(ctx);

	le32enc(md +  0, ctx->state[0]);
	le32enc(md +  4, ctx->state[1]);
	le32enc(md +  8, ctx->state[2]);
	le32enc(md + 12, ctx->state[3]);

	explicit_bzero(ctx, sizeof(ctx));
}

#ifndef MD5_ASM
static const uint32_t K[64] = {
	0xd76aa478, 0xe8c7b756, 0x242070db, 0xc1bdceee,
	0xf57c0faf, 0x4787c62a, 0xa8304613, 0xfd469501,
	0x698098d8, 0x8b44f7af, 0xffff5bb1, 0x895cd7be,
	0x6b901122, 0xfd987193, 0xa679438e, 0x49b40821,
	0xf61e2562, 0xc040b340, 0x265e5a51, 0xe9b6c7aa,
	0xd62f105d, 0x02441453, 0xd8a1e681, 0xe7d3fbc8,
	0x21e1cde6, 0xc33707d6, 0xf4d50d87, 0x455a14ed,
	0xa9e3e905, 0xfcefa3f8, 0x676f02d9, 0x8d2a4c8a,
	0xfffa3942, 0x8771f681, 0x6d9d6122, 0xfde5380c,
	0xa4beea44, 0x4bdecfa9, 0xf6bb4b60, 0xbebfbc70,
	0x289b7ec6, 0xeaa127fa, 0xd4ef3085, 0x04881d05,
	0xd9d4d039, 0xe6db99e5, 0x1fa27cf8, 0xc4ac5665,
	0xf4292244, 0x432aff97, 0xab9423a7, 0xfc93a039,
	0x655b59c3, 0x8f0ccc92, 0xffeff47d, 0x85845dd1,
	0x6fa87e4f, 0xfe2ce6e0, 0xa3014314, 0x4e0811a1,
	0xf7537e82, 0xbd3af235, 0x2ad7d2bb, 0xeb86d391,
};

static inline uint32_t
rol32(uint32_t a, int b)
{
	return (a << b | a >> (32 - b));
}

static void
md5block(MD5_CTX *ctx, const void *data, size_t len)
{
	uint32_t m[16], a0, b0, c0, d0;
	const char *p = data;

	a0 = ctx->state[0];
	b0 = ctx->state[1];
	c0 = ctx->state[2];
	d0 = ctx->state[3];

	while (len >= MD5_BLOCK_LENGTH) {
		size_t i;
		uint32_t a = a0, b = b0, c = c0, d = d0, f, tmp;

		UNROLL
		for (i = 0; i < 16; i++)
			m[i] = le32dec(p + 4*i);

		UNROLL
		for (i = 0; i < 16; i += 4) {
			f = d ^ (b & (c ^ d));
			tmp = d;
			d = c;
			c = b;
			b += rol32(a + f + K[i] + m[i], 7);
			a = tmp;

			f = d ^ (b & (c ^ d));
			tmp = d;
			d = c;
			c = b;
			b += rol32(a + f + K[i + 1] + m[i + 1], 12);
			a = tmp;

			f = d ^ (b & (c ^ d));
			tmp = d;
			d = c;
			c = b;
			b += rol32(a + f + K[i + 2] + m[i + 2], 17);
			a = tmp;

			f = d ^ (b & (c ^ d));
			tmp = d;
			d = c;
			c = b;
			b += rol32(a + f + K[i + 3] + m[i + 3], 22);
			a = tmp;
		}

		UNROLL
		for (; i < 32; i += 4) {
			f = c ^ (d & (b ^ c));
			tmp = d;
			d = c;
			c = b;
			b += rol32(a + f + K[i] + m[(5*i + 1) % 16], 5);
			a = tmp;

			f = c ^ (d & (b ^ c));
			tmp = d;
			d = c;
			c = b;
			b += rol32(a + f + K[i + 1] + m[(5*i + 6) % 16], 9);
			a = tmp;

			f = c ^ (d & (b ^ c));
			tmp = d;
			d = c;
			c = b;
			b += rol32(a + f + K[i + 2] + m[(5*i + 11) % 16], 14);
			a = tmp;

			f = c ^ (d & (b ^ c));
			tmp = d;
			d = c;
			c = b;
			b += rol32(a + f + K[i + 3] + m[5*i % 16], 20);
			a = tmp;
		}

		UNROLL
		for (; i < 48; i += 4) {
			f = b ^ c ^ d;
			tmp = d;
			d = c;
			c = b;
			b += rol32(a + f + K[i] + m[(3*i + 5) % 16], 4);
			a = tmp;

			f = b ^ c ^ d;
			tmp = d;
			d = c;
			c = b;
			b += rol32(a + f + K[i + 1] + m[(3*i + 8) % 16], 11);
			a = tmp;

			f = b ^ c ^ d;
			tmp = d;
			d = c;
			c = b;
			b += rol32(a + f + K[i + 2] + m[(3*i + 11) % 16], 16);
			a = tmp;

			f = b ^ c ^ d;
			tmp = d;
			d = c;
			c = b;
			b += rol32(a + f + K[i + 3] + m[(3*i + 14) % 16], 23);
			a = tmp;
		}

		UNROLL
		for (; i < 64; i += 4) {
			f = c ^ (b | ~d);
			tmp = d;
			d = c;
			c = b;
			b += rol32(a + f + K[i] + m[7*i % 16], 6);
			a = tmp;

			f = c ^ (b | ~d);
			tmp = d;
			d = c;
			c = b;
			b += rol32(a + f + K[i + 1] + m[(7*i + 7) % 16], 10);
			a = tmp;

			f = c ^ (b | ~d);
			tmp = d;
			d = c;
			c = b;
			b += rol32(a + f + K[i + 2] + m[(7*i + 14) % 16], 15);
			a = tmp;

			f = c ^ (b | ~d);
			tmp = d;
			d = c;
			c = b;
			b += rol32(a + f + K[i + 3] + m[(7*i + 5) % 16], 21);
			a = tmp;
		}

		a0 += a;
		b0 += b;
		c0 += c;
		d0 += d;

		p += MD5_BLOCK_LENGTH;
		len -= MD5_BLOCK_LENGTH;
	}

	ctx->state[0] = a0;
	ctx->state[1] = b0;
	ctx->state[2] = c0;
	ctx->state[3] = d0;
}
#endif /* !defined(MD5_ASM) */

#ifdef WEAK_REFS
/* When building libmd, provide weak references. Note: this is not
   activated in the context of compiling these sources for internal
   use in libcrypt.
 */
#undef MD5Init
__weak_reference(_libmd_MD5Init, MD5Init);
#undef MD5Update
__weak_reference(_libmd_MD5Update, MD5Update);
#undef MD5Final
__weak_reference(_libmd_MD5Final, MD5Final);
#endif
