/* SPDX-License-Identifier: BSD-3-Clause */
/*  Copyright (c) 2023, Intel Corporation
 *  All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions are met:
 *
 *   1. Redistributions of source code must retain the above copyright notice,
 *      this list of conditions and the following disclaimer.
 *
 *   2. Redistributions in binary form must reproduce the above copyright
 *      notice, this list of conditions and the following disclaimer in the
 *      documentation and/or other materials provided with the distribution.
 *
 *   3. Neither the name of the Intel Corporation nor the names of its
 *      contributors may be used to endorse or promote products derived from
 *      this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 *  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 *  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 *  ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 *  LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 *  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 *  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 *  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 *  CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 *  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *  POSSIBILITY OF SUCH DAMAGE.
 */
/*$FreeBSD$*/

#ifndef _ICE_BITOPS_H_
#define _ICE_BITOPS_H_

#include "ice_defs.h"
#include "ice_osdep.h"

/* Define the size of the bitmap chunk */
typedef u32 ice_bitmap_t;

/* NOTE!
 * Do not use any of the functions declared in this file
 * on memory that was not declared with ice_declare_bitmap.
 * Not following this rule might cause issues like split
 * locks.
 */

/* Number of bits per bitmap chunk */
#define BITS_PER_CHUNK		(BITS_PER_BYTE * sizeof(ice_bitmap_t))
/* Determine which chunk a bit belongs in */
#define BIT_CHUNK(nr)		((nr) / BITS_PER_CHUNK)
/* How many chunks are required to store this many bits */
#define BITS_TO_CHUNKS(sz)	(((sz) + BITS_PER_CHUNK - 1) / BITS_PER_CHUNK)
/* Which bit inside a chunk this bit corresponds to */
#define BIT_IN_CHUNK(nr)	((nr) % BITS_PER_CHUNK)
/* How many bits are valid in the last chunk, assumes nr > 0 */
#define LAST_CHUNK_BITS(nr)	((((nr) - 1) % BITS_PER_CHUNK) + 1)
/* Generate a bitmask of valid bits in the last chunk, assumes nr > 0 */
#define LAST_CHUNK_MASK(nr)	(((ice_bitmap_t)~0) >> \
				 (BITS_PER_CHUNK - LAST_CHUNK_BITS(nr)))

#define ice_declare_bitmap(A, sz) \
	ice_bitmap_t A[BITS_TO_CHUNKS(sz)]

static inline bool ice_is_bit_set_internal(u16 nr, const ice_bitmap_t *bitmap)
{
	return !!(*bitmap & BIT(nr));
}

/*
 * If atomic version of the bitops are required, each specific OS
 * implementation will need to implement OS/platform specific atomic
 * version of the functions below:
 *
 * ice_clear_bit_internal
 * ice_set_bit_internal
 * ice_test_and_clear_bit_internal
 * ice_test_and_set_bit_internal
 *
 * and define macro ICE_ATOMIC_BITOPS to overwrite the default non-atomic
 * implementation.
 */
static inline void ice_clear_bit_internal(u16 nr, ice_bitmap_t *bitmap)
{
	*bitmap &= ~BIT(nr);
}

static inline void ice_set_bit_internal(u16 nr, ice_bitmap_t *bitmap)
{
	*bitmap |= BIT(nr);
}

static inline bool ice_test_and_clear_bit_internal(u16 nr,
						   ice_bitmap_t *bitmap)
{
	if (ice_is_bit_set_internal(nr, bitmap)) {
		ice_clear_bit_internal(nr, bitmap);
		return true;
	}
	return false;
}

static inline bool ice_test_and_set_bit_internal(u16 nr, ice_bitmap_t *bitmap)
{
	if (ice_is_bit_set_internal(nr, bitmap))
		return true;

	ice_set_bit_internal(nr, bitmap);
	return false;
}

/**
 * ice_is_bit_set - Check state of a bit in a bitmap
 * @bitmap: the bitmap to check
 * @nr: the bit to check
 *
 * Returns true if bit nr of bitmap is set. False otherwise. Assumes that nr
 * is less than the size of the bitmap.
 */
static inline bool ice_is_bit_set(const ice_bitmap_t *bitmap, u16 nr)
{
	return ice_is_bit_set_internal(BIT_IN_CHUNK(nr),
				       &bitmap[BIT_CHUNK(nr)]);
}

/**
 * ice_clear_bit - Clear a bit in a bitmap
 * @bitmap: the bitmap to change
 * @nr: the bit to change
 *
 * Clears the bit nr in bitmap. Assumes that nr is less than the size of the
 * bitmap.
 */
static inline void ice_clear_bit(u16 nr, ice_bitmap_t *bitmap)
{
	ice_clear_bit_internal(BIT_IN_CHUNK(nr), &bitmap[BIT_CHUNK(nr)]);
}

/**
 * ice_set_bit - Set a bit in a bitmap
 * @bitmap: the bitmap to change
 * @nr: the bit to change
 *
 * Sets the bit nr in bitmap. Assumes that nr is less than the size of the
 * bitmap.
 */
static inline void ice_set_bit(u16 nr, ice_bitmap_t *bitmap)
{
	ice_set_bit_internal(BIT_IN_CHUNK(nr), &bitmap[BIT_CHUNK(nr)]);
}

/**
 * ice_test_and_clear_bit - Atomically clear a bit and return the old bit value
 * @nr: the bit to change
 * @bitmap: the bitmap to change
 *
 * Check and clear the bit nr in bitmap. Assumes that nr is less than the size
 * of the bitmap.
 */
static inline bool
ice_test_and_clear_bit(u16 nr, ice_bitmap_t *bitmap)
{
	return ice_test_and_clear_bit_internal(BIT_IN_CHUNK(nr),
					       &bitmap[BIT_CHUNK(nr)]);
}

/**
 * ice_test_and_set_bit - Atomically set a bit and return the old bit value
 * @nr: the bit to change
 * @bitmap: the bitmap to change
 *
 * Check and set the bit nr in bitmap. Assumes that nr is less than the size of
 * the bitmap.
 */
static inline bool
ice_test_and_set_bit(u16 nr, ice_bitmap_t *bitmap)
{
	return ice_test_and_set_bit_internal(BIT_IN_CHUNK(nr),
					     &bitmap[BIT_CHUNK(nr)]);
}

/* ice_zero_bitmap - set bits of bitmap to zero.
 * @bmp: bitmap to set zeros
 * @size: Size of the bitmaps in bits
 *
 * Set all of the bits in a bitmap to zero. Note that this function assumes it
 * operates on an ice_bitmap_t which was declared using ice_declare_bitmap. It
 * will zero every bit in the last chunk, even if those bits are beyond the
 * size.
 */
static inline void ice_zero_bitmap(ice_bitmap_t *bmp, u16 size)
{
	ice_memset(bmp, 0, BITS_TO_CHUNKS(size) * sizeof(ice_bitmap_t),
		   ICE_NONDMA_MEM);
}

/**
 * ice_and_bitmap - bitwise AND 2 bitmaps and store result in dst bitmap
 * @dst: Destination bitmap that receive the result of the operation
 * @bmp1: The first bitmap to intersect
 * @bmp2: The second bitmap to intersect wit the first
 * @size: Size of the bitmaps in bits
 *
 * This function performs a bitwise AND on two "source" bitmaps of the same size
 * and stores the result to "dst" bitmap. The "dst" bitmap must be of the same
 * size as the "source" bitmaps to avoid buffer overflows. This function returns
 * a non-zero value if at least one bit location from both "source" bitmaps is
 * non-zero.
 */
static inline int
ice_and_bitmap(ice_bitmap_t *dst, const ice_bitmap_t *bmp1,
	       const ice_bitmap_t *bmp2, u16 size)
{
	ice_bitmap_t res = 0, mask;
	u16 i;

	/* Handle all but the last chunk */
	for (i = 0; i < BITS_TO_CHUNKS(size) - 1; i++) {
		dst[i] = bmp1[i] & bmp2[i];
		res |= dst[i];
	}

	/* We want to take care not to modify any bits outside of the bitmap
	 * size, even in the destination bitmap. Thus, we won't directly
	 * assign the last bitmap, but instead use a bitmask to ensure we only
	 * modify bits which are within the size, and leave any bits above the
	 * size value alone.
	 */
	mask = LAST_CHUNK_MASK(size);
	dst[i] = (dst[i] & ~mask) | ((bmp1[i] & bmp2[i]) & mask);
	res |= dst[i] & mask;

	return res != 0;
}

/**
 * ice_or_bitmap - bitwise OR 2 bitmaps and store result in dst bitmap
 * @dst: Destination bitmap that receive the result of the operation
 * @bmp1: The first bitmap to intersect
 * @bmp2: The second bitmap to intersect wit the first
 * @size: Size of the bitmaps in bits
 *
 * This function performs a bitwise OR on two "source" bitmaps of the same size
 * and stores the result to "dst" bitmap. The "dst" bitmap must be of the same
 * size as the "source" bitmaps to avoid buffer overflows.
 */
static inline void
ice_or_bitmap(ice_bitmap_t *dst, const ice_bitmap_t *bmp1,
	      const ice_bitmap_t *bmp2, u16 size)
{
	ice_bitmap_t mask;
	u16 i;

	/* Handle all but last chunk */
	for (i = 0; i < BITS_TO_CHUNKS(size) - 1; i++)
		dst[i] = bmp1[i] | bmp2[i];

	/* We want to only OR bits within the size. Furthermore, we also do
	 * not want to modify destination bits which are beyond the specified
	 * size. Use a bitmask to ensure that we only modify the bits that are
	 * within the specified size.
	 */
	mask = LAST_CHUNK_MASK(size);
	dst[i] = (dst[i] & ~mask) | ((bmp1[i] | bmp2[i]) & mask);
}

/**
 * ice_xor_bitmap - bitwise XOR 2 bitmaps and store result in dst bitmap
 * @dst: Destination bitmap that receive the result of the operation
 * @bmp1: The first bitmap of XOR operation
 * @bmp2: The second bitmap to XOR with the first
 * @size: Size of the bitmaps in bits
 *
 * This function performs a bitwise XOR on two "source" bitmaps of the same size
 * and stores the result to "dst" bitmap. The "dst" bitmap must be of the same
 * size as the "source" bitmaps to avoid buffer overflows.
 */
static inline void
ice_xor_bitmap(ice_bitmap_t *dst, const ice_bitmap_t *bmp1,
	       const ice_bitmap_t *bmp2, u16 size)
{
	ice_bitmap_t mask;
	u16 i;

	/* Handle all but last chunk */
	for (i = 0; i < BITS_TO_CHUNKS(size) - 1; i++)
		dst[i] = bmp1[i] ^ bmp2[i];

	/* We want to only XOR bits within the size. Furthermore, we also do
	 * not want to modify destination bits which are beyond the specified
	 * size. Use a bitmask to ensure that we only modify the bits that are
	 * within the specified size.
	 */
	mask = LAST_CHUNK_MASK(size);
	dst[i] = (dst[i] & ~mask) | ((bmp1[i] ^ bmp2[i]) & mask);
}

/**
 * ice_andnot_bitmap - bitwise ANDNOT 2 bitmaps and result in dst bitmap
 * @dst: Destination bitmap that receive the result of the operation
 * @bmp1: The first bitmap of ANDNOT operation
 * @bmp2: The second bitmap to ANDNOT operation
 * @size: Size of the bitmaps in bits
 *
 * This function performs a bitwise ANDNOT on two "source" bitmaps of the same
 * size, and stores the result to "dst" bitmap. The "dst" bitmap must be of the
 * same size as the "source" bitmaps to avoid buffer overflows.
 */
static inline void
ice_andnot_bitmap(ice_bitmap_t *dst, const ice_bitmap_t *bmp1,
		  const ice_bitmap_t *bmp2, u16 size)
{
	ice_bitmap_t mask;
	u16 i;

	/* Handle all but last chunk */
	for (i = 0; i < BITS_TO_CHUNKS(size) - 1; i++)
		dst[i] = bmp1[i] & ~bmp2[i];

	/* We want to only clear bits within the size. Furthermore, we also do
	 * not want to modify destination bits which are beyond the specified
	 * size. Use a bitmask to ensure that we only modify the bits that are
	 * within the specified size.
	 */
	mask = LAST_CHUNK_MASK(size);
	dst[i] = (dst[i] & ~mask) | ((bmp1[i] & ~bmp2[i]) & mask);
}

/**
 * ice_find_next_bit - Find the index of the next set bit of a bitmap
 * @bitmap: the bitmap to scan
 * @size: the size in bits of the bitmap
 * @offset: the offset to start at
 *
 * Scans the bitmap and returns the index of the first set bit which is equal
 * to or after the specified offset. Will return size if no bits are set.
 */
static inline u16
ice_find_next_bit(const ice_bitmap_t *bitmap, u16 size, u16 offset)
{
	u16 i, j;

	if (offset >= size)
		return size;

	/* Since the starting position may not be directly on a chunk
	 * boundary, we need to be careful to handle the first chunk specially
	 */
	i = BIT_CHUNK(offset);
	if (bitmap[i] != 0) {
		u16 off = i * BITS_PER_CHUNK;

		for (j = offset % BITS_PER_CHUNK; j < BITS_PER_CHUNK; j++) {
			if (ice_is_bit_set(bitmap, off + j))
				return min(size, (u16)(off + j));
		}
	}

	/* Now we handle the remaining chunks, if any */
	for (i++; i < BITS_TO_CHUNKS(size); i++) {
		if (bitmap[i] != 0) {
			u16 off = i * BITS_PER_CHUNK;

			for (j = 0; j < BITS_PER_CHUNK; j++) {
				if (ice_is_bit_set(bitmap, off + j))
					return min(size, (u16)(off + j));
			}
		}
	}
	return size;
}

/**
 * ice_find_first_bit - Find the index of the first set bit of a bitmap
 * @bitmap: the bitmap to scan
 * @size: the size in bits of the bitmap
 *
 * Scans the bitmap and returns the index of the first set bit. Will return
 * size if no bits are set.
 */
static inline u16 ice_find_first_bit(const ice_bitmap_t *bitmap, u16 size)
{
	return ice_find_next_bit(bitmap, size, 0);
}

#define ice_for_each_set_bit(_bitpos, _addr, _maxlen)	\
	for ((_bitpos) = ice_find_first_bit((_addr), (_maxlen)); \
	     (_bitpos) < (_maxlen); \
	     (_bitpos) = ice_find_next_bit((_addr), (_maxlen), (_bitpos) + 1))

/**
 * ice_is_any_bit_set - Return true of any bit in the bitmap is set
 * @bitmap: the bitmap to check
 * @size: the size of the bitmap
 *
 * Equivalent to checking if ice_find_first_bit returns a value less than the
 * bitmap size.
 */
static inline bool ice_is_any_bit_set(ice_bitmap_t *bitmap, u16 size)
{
	return ice_find_first_bit(bitmap, size) < size;
}

/**
 * ice_cp_bitmap - copy bitmaps.
 * @dst: bitmap destination
 * @src: bitmap to copy from
 * @size: Size of the bitmaps in bits
 *
 * This function copy bitmap from src to dst. Note that this function assumes
 * it is operating on a bitmap declared using ice_declare_bitmap. It will copy
 * the entire last chunk even if this contains bits beyond the size.
 */
static inline void ice_cp_bitmap(ice_bitmap_t *dst, ice_bitmap_t *src, u16 size)
{
	ice_memcpy(dst, src, BITS_TO_CHUNKS(size) * sizeof(ice_bitmap_t),
		   ICE_NONDMA_TO_NONDMA);
}

/**
 * ice_bitmap_set - set a number of bits in bitmap from a starting position
 * @dst: bitmap destination
 * @pos: first bit position to set
 * @num_bits: number of bits to set
 *
 * This function sets bits in a bitmap from pos to (pos + num_bits) - 1.
 * Note that this function assumes it is operating on a bitmap declared using
 * ice_declare_bitmap.
 */
static inline void
ice_bitmap_set(ice_bitmap_t *dst, u16 pos, u16 num_bits)
{
	u16 i;

	for (i = pos; i < pos + num_bits; i++)
		ice_set_bit(i, dst);
}

/**
 * ice_bitmap_hweight - hamming weight of bitmap
 * @bm: bitmap pointer
 * @size: size of bitmap (in bits)
 *
 * This function determines the number of set bits in a bitmap.
 * Note that this function assumes it is operating on a bitmap declared using
 * ice_declare_bitmap.
 */
static inline int
ice_bitmap_hweight(ice_bitmap_t *bm, u16 size)
{
	int count = 0;
	u16 bit = 0;

	while (size > (bit = ice_find_next_bit(bm, size, bit))) {
		count++;
		bit++;
	}

	return count;
}

/**
 * ice_cmp_bitmap - compares two bitmaps.
 * @bmp1: the bitmap to compare
 * @bmp2: the bitmap to compare with bmp1
 * @size: Size of the bitmaps in bits
 *
 * This function compares two bitmaps, and returns result as true or false.
 */
static inline bool
ice_cmp_bitmap(ice_bitmap_t *bmp1, ice_bitmap_t *bmp2, u16 size)
{
	ice_bitmap_t mask;
	u16 i;

	/* Handle all but last chunk */
	for (i = 0; i < BITS_TO_CHUNKS(size) - 1; i++)
		if (bmp1[i] != bmp2[i])
			return false;

	/* We want to only compare bits within the size */
	mask = LAST_CHUNK_MASK(size);
	if ((bmp1[i] & mask) != (bmp2[i] & mask))
		return false;

	return true;
}

/**
 * ice_bitmap_from_array32 - copies u32 array source into bitmap destination
 * @dst: the destination bitmap
 * @src: the source u32 array
 * @size: size of the bitmap (in bits)
 *
 * This function copies the src bitmap stored in an u32 array into the dst
 * bitmap stored as an ice_bitmap_t.
 */
static inline void
ice_bitmap_from_array32(ice_bitmap_t *dst, u32 *src, u16 size)
{
	u32 remaining_bits, i;

#define BITS_PER_U32	(sizeof(u32) * BITS_PER_BYTE)
	/* clear bitmap so we only have to set when iterating */
	ice_zero_bitmap(dst, size);

	for (i = 0; i < (u32)(size / BITS_PER_U32); i++) {
		u32 bit_offset = i * BITS_PER_U32;
		u32 entry = src[i];
		u32 j;

		for (j = 0; j < BITS_PER_U32; j++) {
			if (entry & BIT(j))
				ice_set_bit((u16)(j + bit_offset), dst);
		}
	}

	/* still need to check the leftover bits (i.e. if size isn't evenly
	 * divisible by BITS_PER_U32
	 **/
	remaining_bits = size % BITS_PER_U32;
	if (remaining_bits) {
		u32 bit_offset = i * BITS_PER_U32;
		u32 entry = src[i];
		u32 j;

		for (j = 0; j < remaining_bits; j++) {
			if (entry & BIT(j))
				ice_set_bit((u16)(j + bit_offset), dst);
		}
	}
}

#undef BIT_CHUNK
#undef BIT_IN_CHUNK
#undef LAST_CHUNK_BITS
#undef LAST_CHUNK_MASK

#endif /* _ICE_BITOPS_H_ */
