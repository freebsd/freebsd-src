/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2015 Intel Corporation.
 */

#ifndef _RTE_JHASH_H
#define _RTE_JHASH_H

/**
 * @file
 *
 * jhash functions.
 */

#ifdef __cplusplus
extern "C" {
#endif

//#include <rte_byteorder.h>

/* jhash.h: Jenkins hash support.
 *
 * Copyright (C) 2006 Bob Jenkins (bob_jenkins@burtleburtle.net)
 *
 * http://burtleburtle.net/bob/hash/
 *
 * These are the credits from Bob's sources:
 *
 * lookup3.c, by Bob Jenkins, May 2006, Public Domain.
 *
 * These are functions for producing 32-bit hashes for hash table lookup.
 * hashword(), hashlittle(), hashlittle2(), hashbig(), mix(), and final()
 * are externally useful functions.  Routines to test the hash are included
 * if SELF_TEST is defined.  You can use this free for any purpose.  It's in
 * the public domain.  It has no warranty.
 *
 * $FreeBSD$
 */

#define rot(x, k) (((x) << (k)) | ((x) >> (32-(k))))

/** @internal Internal function. NOTE: Arguments are modified. */
#define __rte_jhash_mix(a, b, c) do { \
	a -= c; a ^= rot(c, 4); c += b; \
	b -= a; b ^= rot(a, 6); a += c; \
	c -= b; c ^= rot(b, 8); b += a; \
	a -= c; a ^= rot(c, 16); c += b; \
	b -= a; b ^= rot(a, 19); a += c; \
	c -= b; c ^= rot(b, 4); b += a; \
} while (0)

#define __rte_jhash_final(a, b, c) do { \
	c ^= b; c -= rot(b, 14); \
	a ^= c; a -= rot(c, 11); \
	b ^= a; b -= rot(a, 25); \
	c ^= b; c -= rot(b, 16); \
	a ^= c; a -= rot(c, 4);  \
	b ^= a; b -= rot(a, 14); \
	c ^= b; c -= rot(b, 24); \
} while (0)

/** The golden ratio: an arbitrary value. */
#define RTE_JHASH_GOLDEN_RATIO      0xdeadbeef

#if RTE_BYTE_ORDER == RTE_LITTLE_ENDIAN
#define BIT_SHIFT(x, y, k) (((x) >> (k)) | ((uint64_t)(y) << (32-(k))))
#else
#define BIT_SHIFT(x, y, k) (((uint64_t)(x) << (k)) | ((y) >> (32-(k))))
#endif

#define LOWER8b_MASK rte_le_to_cpu_32(0xff)
#define LOWER16b_MASK rte_le_to_cpu_32(0xffff)
#define LOWER24b_MASK rte_le_to_cpu_32(0xffffff)

static inline void
__rte_jhash_2hashes(const void *key, uint32_t length, uint32_t *pc,
		uint32_t *pb, unsigned check_align)
{
	uint32_t a, b, c;

	/* Set up the internal state */
	a = b = c = RTE_JHASH_GOLDEN_RATIO + ((uint32_t)length) + *pc;
	c += *pb;

	/*
	 * Check key alignment. For x86 architecture, first case is always optimal
	 * If check_align is not set, first case will be used
	 */
#if defined(RTE_ARCH_X86_64) || defined(RTE_ARCH_I686) || defined(RTE_ARCH_X86_X32)
	const uint32_t *k = (const uint32_t *)key;
	const uint32_t s = 0;
#else
	const uint32_t *k = (uint32_t *)((uintptr_t)key & (uintptr_t)~3);
	const uint32_t s = ((uintptr_t)key & 3) * CHAR_BIT;
#endif
	if (!check_align || s == 0) {
		while (length > 12) {
			a += k[0];
			b += k[1];
			c += k[2];

			__rte_jhash_mix(a, b, c);

			k += 3;
			length -= 12;
		}

		switch (length) {
		case 12:
			c += k[2]; b += k[1]; a += k[0]; break;
		case 11:
			c += k[2] & LOWER24b_MASK; b += k[1]; a += k[0]; break;
		case 10:
			c += k[2] & LOWER16b_MASK; b += k[1]; a += k[0]; break;
		case 9:
			c += k[2] & LOWER8b_MASK; b += k[1]; a += k[0]; break;
		case 8:
			b += k[1]; a += k[0]; break;
		case 7:
			b += k[1] & LOWER24b_MASK; a += k[0]; break;
		case 6:
			b += k[1] & LOWER16b_MASK; a += k[0]; break;
		case 5:
			b += k[1] & LOWER8b_MASK; a += k[0]; break;
		case 4:
			a += k[0]; break;
		case 3:
			a += k[0] & LOWER24b_MASK; break;
		case 2:
			a += k[0] & LOWER16b_MASK; break;
		case 1:
			a += k[0] & LOWER8b_MASK; break;
		/* zero length strings require no mixing */
		case 0:
			*pc = c;
			*pb = b;
			return;
		};
	} else {
		/* all but the last block: affect some 32 bits of (a, b, c) */
		while (length > 12) {
			a += BIT_SHIFT(k[0], k[1], s);
			b += BIT_SHIFT(k[1], k[2], s);
			c += BIT_SHIFT(k[2], k[3], s);
			__rte_jhash_mix(a, b, c);

			k += 3;
			length -= 12;
		}

		/* last block: affect all 32 bits of (c) */
		switch (length) {
		case 12:
			a += BIT_SHIFT(k[0], k[1], s);
			b += BIT_SHIFT(k[1], k[2], s);
			c += BIT_SHIFT(k[2], k[3], s);
			break;
		case 11:
			a += BIT_SHIFT(k[0], k[1], s);
			b += BIT_SHIFT(k[1], k[2], s);
			c += BIT_SHIFT(k[2], k[3], s) & LOWER24b_MASK;
			break;
		case 10:
			a += BIT_SHIFT(k[0], k[1], s);
			b += BIT_SHIFT(k[1], k[2], s);
			c += BIT_SHIFT(k[2], k[3], s) & LOWER16b_MASK;
			break;
		case 9:
			a += BIT_SHIFT(k[0], k[1], s);
			b += BIT_SHIFT(k[1], k[2], s);
			c += BIT_SHIFT(k[2], k[3], s) & LOWER8b_MASK;
			break;
		case 8:
			a += BIT_SHIFT(k[0], k[1], s);
			b += BIT_SHIFT(k[1], k[2], s);
			break;
		case 7:
			a += BIT_SHIFT(k[0], k[1], s);
			b += BIT_SHIFT(k[1], k[2], s) & LOWER24b_MASK;
			break;
		case 6:
			a += BIT_SHIFT(k[0], k[1], s);
			b += BIT_SHIFT(k[1], k[2], s) & LOWER16b_MASK;
			break;
		case 5:
			a += BIT_SHIFT(k[0], k[1], s);
			b += BIT_SHIFT(k[1], k[2], s) & LOWER8b_MASK;
			break;
		case 4:
			a += BIT_SHIFT(k[0], k[1], s);
			break;
		case 3:
			a += BIT_SHIFT(k[0], k[1], s) & LOWER24b_MASK;
			break;
		case 2:
			a += BIT_SHIFT(k[0], k[1], s) & LOWER16b_MASK;
			break;
		case 1:
			a += BIT_SHIFT(k[0], k[1], s) & LOWER8b_MASK;
			break;
		/* zero length strings require no mixing */
		case 0:
			*pc = c;
			*pb = b;
			return;
		}
	}

	__rte_jhash_final(a, b, c);

	*pc = c;
	*pb = b;
}

/**
 * Same as rte_jhash, but takes two seeds and return two uint32_ts.
 * pc and pb must be non-null, and *pc and *pb must both be initialized
 * with seeds. If you pass in (*pb)=0, the output (*pc) will be
 * the same as the return value from rte_jhash.
 *
 * @param key
 *   Key to calculate hash of.
 * @param length
 *   Length of key in bytes.
 * @param pc
 *   IN: seed OUT: primary hash value.
 * @param pb
 *   IN: second seed OUT: secondary hash value.
 */
static inline void
rte_jhash_2hashes(const void *key, uint32_t length, uint32_t *pc, uint32_t *pb)
{
	__rte_jhash_2hashes(key, length, pc, pb, 1);
}

/**
 * Same as rte_jhash_32b, but takes two seeds and return two uint32_ts.
 * pc and pb must be non-null, and *pc and *pb must both be initialized
 * with seeds. If you pass in (*pb)=0, the output (*pc) will be
 * the same as the return value from rte_jhash_32b.
 *
 * @param k
 *   Key to calculate hash of.
 * @param length
 *   Length of key in units of 4 bytes.
 * @param pc
 *   IN: seed OUT: primary hash value.
 * @param pb
 *   IN: second seed OUT: secondary hash value.
 */
static inline void
rte_jhash_32b_2hashes(const uint32_t *k, uint32_t length, uint32_t *pc, uint32_t *pb)
{
	__rte_jhash_2hashes((const void *) k, (length << 2), pc, pb, 0);
}

/**
 * The most generic version, hashes an arbitrary sequence
 * of bytes.  No alignment or length assumptions are made about
 * the input key.  For keys not aligned to four byte boundaries
 * or a multiple of four bytes in length, the memory region
 * just after may be read (but not used in the computation).
 * This may cross a page boundary.
 *
 * @param key
 *   Key to calculate hash of.
 * @param length
 *   Length of key in bytes.
 * @param initval
 *   Initialising value of hash.
 * @return
 *   Calculated hash value.
 */
static inline uint32_t
rte_jhash(const void *key, uint32_t length, uint32_t initval)
{
	uint32_t initval2 = 0;

	rte_jhash_2hashes(key, length, &initval, &initval2);

	return initval;
}

/**
 * A special optimized version that handles 1 or more of uint32_ts.
 * The length parameter here is the number of uint32_ts in the key.
 *
 * @param k
 *   Key to calculate hash of.
 * @param length
 *   Length of key in units of 4 bytes.
 * @param initval
 *   Initialising value of hash.
 * @return
 *   Calculated hash value.
 */
static inline uint32_t
rte_jhash_32b(const uint32_t *k, uint32_t length, uint32_t initval)
{
	uint32_t initval2 = 0;

	rte_jhash_32b_2hashes(k, length, &initval, &initval2);

	return initval;
}

static inline uint32_t
__rte_jhash_3words(uint32_t a, uint32_t b, uint32_t c, uint32_t initval)
{
	a += RTE_JHASH_GOLDEN_RATIO + initval;
	b += RTE_JHASH_GOLDEN_RATIO + initval;
	c += RTE_JHASH_GOLDEN_RATIO + initval;

	__rte_jhash_final(a, b, c);

	return c;
}

/**
 * A special ultra-optimized versions that knows it is hashing exactly
 * 3 words.
 *
 * @param a
 *   First word to calculate hash of.
 * @param b
 *   Second word to calculate hash of.
 * @param c
 *   Third word to calculate hash of.
 * @param initval
 *   Initialising value of hash.
 * @return
 *   Calculated hash value.
 */
static inline uint32_t
rte_jhash_3words(uint32_t a, uint32_t b, uint32_t c, uint32_t initval)
{
	return __rte_jhash_3words(a + 12, b + 12, c + 12, initval);
}

/**
 * A special ultra-optimized versions that knows it is hashing exactly
 * 2 words.
 *
 * @param a
 *   First word to calculate hash of.
 * @param b
 *   Second word to calculate hash of.
 * @param initval
 *   Initialising value of hash.
 * @return
 *   Calculated hash value.
 */
static inline uint32_t
rte_jhash_2words(uint32_t a, uint32_t b, uint32_t initval)
{
	return __rte_jhash_3words(a + 8, b + 8, 8, initval);
}

/**
 * A special ultra-optimized versions that knows it is hashing exactly
 * 1 word.
 *
 * @param a
 *   Word to calculate hash of.
 * @param initval
 *   Initialising value of hash.
 * @return
 *   Calculated hash value.
 */
static inline uint32_t
rte_jhash_1word(uint32_t a, uint32_t initval)
{
	return __rte_jhash_3words(a + 4, 4, 4, initval);
}

#ifdef __cplusplus
}
#endif

#endif /* _RTE_JHASH_H */
