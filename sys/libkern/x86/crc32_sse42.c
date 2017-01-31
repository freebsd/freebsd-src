/*
 * Derived from crc32c.c version 1.1 by Mark Adler.
 *
 * Copyright (C) 2013 Mark Adler
 *
 * This software is provided 'as-is', without any express or implied warranty.
 * In no event will the author be held liable for any damages arising from the
 * use of this software.
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely, subject to the following restrictions:
 *
 * 1. The origin of this software must not be misrepresented; you must not
 *    claim that you wrote the original software. If you use this software
 *    in a product, an acknowledgment in the product documentation would be
 *    appreciated but is not required.
 * 2. Altered source versions must be plainly marked as such, and must not be
 *    misrepresented as being the original software.
 * 3. This notice may not be removed or altered from any source distribution.
 *
 * Mark Adler
 * madler@alumni.caltech.edu
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

/*
 * This file is compiled in userspace in order to run ATF unit tests.
 */
#ifdef USERSPACE_TESTING
#include <stdint.h>
#else
#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/libkern.h>
#include <sys/systm.h>
#endif

#include <nmmintrin.h>

/* CRC-32C (iSCSI) polynomial in reversed bit order. */
#define POLY	0x82f63b78

/*
 * Block sizes for three-way parallel crc computation.  LONG and SHORT must
 * both be powers of two.
 */
#define LONG	8192
#define SHORT	256

/* Tables for hardware crc that shift a crc by LONG and SHORT zeros. */
static uint32_t crc32c_long[4][256];
static uint32_t crc32c_short[4][256];

/*
 * Multiply a matrix times a vector over the Galois field of two elements,
 * GF(2).  Each element is a bit in an unsigned integer.  mat must have at
 * least as many entries as the power of two for most significant one bit in
 * vec.
 */
static inline uint32_t
gf2_matrix_times(uint32_t *mat, uint32_t vec)
{
	uint32_t sum;

	sum = 0;
	while (vec) {
		if (vec & 1)
			sum ^= *mat;
		vec >>= 1;
		mat++;
	}
	return (sum);
}

/*
 * Multiply a matrix by itself over GF(2).  Both mat and square must have 32
 * rows.
 */
static inline void
gf2_matrix_square(uint32_t *square, uint32_t *mat)
{
	int n;

	for (n = 0; n < 32; n++)
		square[n] = gf2_matrix_times(mat, mat[n]);
}

/*
 * Construct an operator to apply len zeros to a crc.  len must be a power of
 * two.  If len is not a power of two, then the result is the same as for the
 * largest power of two less than len.  The result for len == 0 is the same as
 * for len == 1.  A version of this routine could be easily written for any
 * len, but that is not needed for this application.
 */
static void
crc32c_zeros_op(uint32_t *even, size_t len)
{
	uint32_t odd[32];       /* odd-power-of-two zeros operator */
	uint32_t row;
	int n;

	/* put operator for one zero bit in odd */
	odd[0] = POLY;              /* CRC-32C polynomial */
	row = 1;
	for (n = 1; n < 32; n++) {
		odd[n] = row;
		row <<= 1;
	}

	/* put operator for two zero bits in even */
	gf2_matrix_square(even, odd);

	/* put operator for four zero bits in odd */
	gf2_matrix_square(odd, even);

	/*
	 * first square will put the operator for one zero byte (eight zero
	 * bits), in even -- next square puts operator for two zero bytes in
	 * odd, and so on, until len has been rotated down to zero
	 */
	do {
		gf2_matrix_square(even, odd);
		len >>= 1;
		if (len == 0)
			return;
		gf2_matrix_square(odd, even);
		len >>= 1;
	} while (len);

	/* answer ended up in odd -- copy to even */
	for (n = 0; n < 32; n++)
		even[n] = odd[n];
}

/*
 * Take a length and build four lookup tables for applying the zeros operator
 * for that length, byte-by-byte on the operand.
 */
static void
crc32c_zeros(uint32_t zeros[][256], size_t len)
{
	uint32_t op[32];
	uint32_t n;

	crc32c_zeros_op(op, len);
	for (n = 0; n < 256; n++) {
		zeros[0][n] = gf2_matrix_times(op, n);
		zeros[1][n] = gf2_matrix_times(op, n << 8);
		zeros[2][n] = gf2_matrix_times(op, n << 16);
		zeros[3][n] = gf2_matrix_times(op, n << 24);
	}
}

/* Apply the zeros operator table to crc. */
static inline uint32_t
crc32c_shift(uint32_t zeros[][256], uint32_t crc)
{

	return (zeros[0][crc & 0xff] ^ zeros[1][(crc >> 8) & 0xff] ^
	    zeros[2][(crc >> 16) & 0xff] ^ zeros[3][crc >> 24]);
}

/* Initialize tables for shifting crcs. */
static void
#ifdef USERSPACE_TESTING
__attribute__((__constructor__))
#endif
crc32c_init_hw(void)
{
	crc32c_zeros(crc32c_long, LONG);
	crc32c_zeros(crc32c_short, SHORT);
}
#ifdef _KERNEL
SYSINIT(crc32c_sse42, SI_SUB_LOCK, SI_ORDER_ANY, crc32c_init_hw, NULL);
#endif

/* Compute CRC-32C using the Intel hardware instruction. */
#ifdef USERSPACE_TESTING
uint32_t sse42_crc32c(uint32_t, const unsigned char *, unsigned);
#endif
uint32_t
sse42_crc32c(uint32_t crc, const unsigned char *buf, unsigned len)
{
#ifdef __amd64__
	const size_t align = 8;
#else
	const size_t align = 4;
#endif
	const unsigned char *next, *end;
	uint64_t crc0, crc1, crc2;      /* need to be 64 bits for crc32q */

	next = buf;
	crc0 = crc;

	/* Compute the crc to bring the data pointer to an aligned boundary. */
	while (len && ((uintptr_t)next & (align - 1)) != 0) {
		crc0 = _mm_crc32_u8(crc0, *next);
		next++;
		len--;
	}

	/*
	 * Compute the crc on sets of LONG*3 bytes, executing three independent
	 * crc instructions, each on LONG bytes -- this is optimized for the
	 * Nehalem, Westmere, Sandy Bridge, and Ivy Bridge architectures, which
	 * have a throughput of one crc per cycle, but a latency of three
	 * cycles.
	 */
	while (len >= LONG * 3) {
		crc1 = 0;
		crc2 = 0;
		end = next + LONG;
		do {
#ifdef __amd64__
			crc0 = _mm_crc32_u64(crc0, *(const uint64_t *)next);
			crc1 = _mm_crc32_u64(crc1,
			    *(const uint64_t *)(next + LONG));
			crc2 = _mm_crc32_u64(crc2,
			    *(const uint64_t *)(next + (LONG * 2)));
#else
			crc0 = _mm_crc32_u32(crc0, *(const uint32_t *)next);
			crc1 = _mm_crc32_u32(crc1,
			    *(const uint32_t *)(next + LONG));
			crc2 = _mm_crc32_u32(crc2,
			    *(const uint32_t *)(next + (LONG * 2)));
#endif
			next += align;
		} while (next < end);
		crc0 = crc32c_shift(crc32c_long, crc0) ^ crc1;
		crc0 = crc32c_shift(crc32c_long, crc0) ^ crc2;
		next += LONG * 2;
		len -= LONG * 3;
	}

	/*
	 * Do the same thing, but now on SHORT*3 blocks for the remaining data
	 * less than a LONG*3 block
	 */
	while (len >= SHORT * 3) {
		crc1 = 0;
		crc2 = 0;
		end = next + SHORT;
		do {
#ifdef __amd64__
			crc0 = _mm_crc32_u64(crc0, *(const uint64_t *)next);
			crc1 = _mm_crc32_u64(crc1,
			    *(const uint64_t *)(next + SHORT));
			crc2 = _mm_crc32_u64(crc2,
			    *(const uint64_t *)(next + (SHORT * 2)));
#else
			crc0 = _mm_crc32_u32(crc0, *(const uint32_t *)next);
			crc1 = _mm_crc32_u32(crc1,
			    *(const uint32_t *)(next + SHORT));
			crc2 = _mm_crc32_u32(crc2,
			    *(const uint32_t *)(next + (SHORT * 2)));
#endif
			next += align;
		} while (next < end);
		crc0 = crc32c_shift(crc32c_short, crc0) ^ crc1;
		crc0 = crc32c_shift(crc32c_short, crc0) ^ crc2;
		next += SHORT * 2;
		len -= SHORT * 3;
	}

	/* Compute the crc on the remaining bytes at native word size. */
	end = next + (len - (len & (align - 1)));
	while (next < end) {
#ifdef __amd64__
		crc0 = _mm_crc32_u64(crc0, *(const uint64_t *)next);
#else
		crc0 = _mm_crc32_u32(crc0, *(const uint32_t *)next);
#endif
		next += align;
	}
	len &= (align - 1);

	/* Compute the crc for any trailing bytes. */
	while (len) {
		crc0 = _mm_crc32_u8(crc0, *next);
		next++;
		len--;
	}

	return ((uint32_t)crc0);
}
