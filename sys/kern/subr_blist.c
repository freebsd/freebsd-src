/*-
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 1998 Matthew Dillon.  All Rights Reserved.
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
 * GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
/*
 * BLIST.C -	Bitmap allocator/deallocator, using a radix tree with hinting
 *
 *	This module implements a general bitmap allocator/deallocator.  The
 *	allocator eats around 2 bits per 'block'.  The module does not
 *	try to interpret the meaning of a 'block' other than to return
 *	SWAPBLK_NONE on an allocation failure.
 *
 *	A radix tree controls access to pieces of the bitmap, and includes
 *	auxiliary information at each interior node about the availabilty of
 *	contiguous free blocks in the subtree rooted at that node.  A radix
 *	constant defines the size of the bitmaps contained in a leaf node
 *	and the number of descendents of each of the meta (interior) nodes.
 *	Each subtree is associated with a range of blocks.  The root of any
 *	subtree stores a hint field that defines an upper bound on the size
 *	of the largest allocation that can begin in the associated block
 *	range.  A hint is an upper bound on a potential allocation, but not
 *	necessarily a tight upper bound.
 *
 *	The bitmap field in each node directs the search for available blocks.
 *	For a leaf node, a bit is set if the corresponding block is free.  For a
 *	meta node, a bit is set if the corresponding subtree contains a free
 *	block somewhere within it.  The search at a meta node considers only
 *	children of that node that represent a range that includes a free block.
 *
 * 	The hinting greatly increases code efficiency for allocations while
 *	the general radix structure optimizes both allocations and frees.  The
 *	radix tree should be able to operate well no matter how much
 *	fragmentation there is and no matter how large a bitmap is used.
 *
 *	The blist code wires all necessary memory at creation time.  Neither
 *	allocations nor frees require interaction with the memory subsystem.
 *	The non-blocking nature of allocations and frees is required by swap
 *	code (vm/swap_pager.c).
 *
 *	LAYOUT: The radix tree is laid out recursively using a linear array.
 *	Each meta node is immediately followed (laid out sequentially in
 *	memory) by BLIST_RADIX lower-level nodes.  This is a recursive
 *	structure but one that can be easily scanned through a very simple
 *	'skip' calculation.  The memory allocation is only large enough to
 *	cover the number of blocks requested at creation time.  Nodes that
 *	represent blocks beyond that limit, nodes that would never be read
 *	or written, are not allocated, so that the last of the
 *	BLIST_RADIX lower-level nodes of a some nodes may not be allocated.
 *
 *	NOTE: the allocator cannot currently allocate more than
 *	BLIST_RADIX blocks per call.  It will panic with 'allocation too
 *	large' if you try.  This is an area that could use improvement.  The
 *	radix is large enough that this restriction does not effect the swap
 *	system, though.  Currently only the allocation code is affected by
 *	this algorithmic unfeature.  The freeing code can handle arbitrary
 *	ranges.
 *
 *	This code can be compiled stand-alone for debugging.
 */

#include <sys/cdefs.h>
#ifdef _KERNEL

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/lock.h>
#include <sys/kernel.h>
#include <sys/blist.h>
#include <sys/malloc.h>
#include <sys/sbuf.h>
#include <sys/proc.h>
#include <sys/mutex.h>

#else

#ifndef BLIST_NO_DEBUG
#define BLIST_DEBUG
#endif

#include <sys/errno.h>
#include <sys/types.h>
#include <sys/malloc.h>
#include <sys/sbuf.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdbool.h>

#define	bitcount64(x)	__bitcount64((uint64_t)(x))
#define malloc(a,b,c)	calloc(a, 1)
#define free(a,b)	free(a)
#define ummin(a,b)	((a) < (b) ? (a) : (b))
#define imin(a,b)	((a) < (b) ? (a) : (b))
#define KASSERT(a,b)	assert(a)

#include <sys/blist.h>

#endif

/*
 * static support functions
 */
static daddr_t	blst_leaf_alloc(blmeta_t *scan, daddr_t blk,
    int *count, int maxcount);
static daddr_t	blst_meta_alloc(blmeta_t *scan, daddr_t cursor, int *count,
    int maxcount, u_daddr_t radix);
static void blst_leaf_free(blmeta_t *scan, daddr_t relblk, int count);
static void blst_meta_free(blmeta_t *scan, daddr_t freeBlk, daddr_t count,
		    u_daddr_t radix);
static void blst_copy(blmeta_t *scan, daddr_t blk, daddr_t radix,
		    blist_t dest, daddr_t count);
static daddr_t blst_leaf_fill(blmeta_t *scan, daddr_t blk, int count);
static daddr_t blst_meta_fill(blmeta_t *scan, daddr_t allocBlk, daddr_t count,
		    u_daddr_t radix);
#ifndef _KERNEL
static void	blst_radix_print(blmeta_t *scan, daddr_t blk, daddr_t radix,
		    int tab);
#endif

#ifdef _KERNEL
static MALLOC_DEFINE(M_SWAP, "SWAP", "Swap space");
#endif

#define	BLIST_MASK	(BLIST_RADIX - 1)

/*
 * For a subtree that can represent the state of up to 'radix' blocks, the
 * number of leaf nodes of the subtree is L=radix/BLIST_RADIX.  If 'm'
 * is short for BLIST_RADIX, then for a tree of height h with L=m**h
 * leaf nodes, the total number of tree nodes is 1 + m + m**2 + ... + m**h,
 * or, equivalently, (m**(h+1)-1)/(m-1).  This quantity is called 'skip'
 * in the 'meta' functions that process subtrees.  Since integer division
 * discards remainders, we can express this computation as
 * skip = (m * m**h) / (m - 1)
 * skip = (m * (radix / m)) / (m - 1)
 * skip = radix / (m - 1)
 * so that simple integer division by a constant can safely be used for the
 * calculation.
 */
static inline daddr_t
radix_to_skip(daddr_t radix)
{

	return (radix / BLIST_MASK);
}

/*
 * Provide a mask with count bits set, starting as position n.
 */
static inline u_daddr_t
bitrange(int n, int count)
{

	return (((u_daddr_t)-1 << n) &
	    ((u_daddr_t)-1 >> (BLIST_RADIX - (n + count))));
}

static inline int
bitpos(u_daddr_t mask)
{

	_Static_assert(sizeof(long long) >= sizeof(mask),
	    "mask too big for ffsll()");
	return (ffsll(mask) - 1);
}

/*
 * blist_create() - create a blist capable of handling up to the specified
 *		    number of blocks
 *
 *	blocks - must be greater than 0
 * 	flags  - malloc flags
 *
 *	The smallest blist consists of a single leaf node capable of
 *	managing BLIST_RADIX blocks.
 */
blist_t
blist_create(daddr_t blocks, int flags)
{
	blist_t bl;
	u_daddr_t nodes, radix;

	KASSERT(blocks > 0, ("invalid block count"));

	/*
	 * Calculate the radix and node count used for scanning.
	 */
	nodes = 1;
	for (radix = 1; (blocks - 1) / BLIST_RADIX / radix > 0;
	    radix *= BLIST_RADIX)
		nodes += 1 + (blocks - 1) / BLIST_RADIX / radix;

	/*
	 * Include a sentinel node to ensure that cross-leaf scans stay within
	 * the bounds of the allocation.
	 */
	if (blocks % BLIST_RADIX == 0)
		nodes++;

	bl = malloc(offsetof(struct blist, bl_root[nodes]), M_SWAP, flags |
	    M_ZERO);
	if (bl == NULL)
		return (NULL);

	bl->bl_blocks = blocks;
	bl->bl_radix = radix;

#if defined(BLIST_DEBUG)
	printf(
		"BLIST representing %lld blocks (%lld MB of swap)"
		", requiring %lldK of ram\n",
		(long long)bl->bl_blocks,
		(long long)bl->bl_blocks * 4 / 1024,
		(long long)(nodes * sizeof(blmeta_t) + 1023) / 1024
	);
	printf("BLIST raw radix tree contains %lld records\n",
	    (long long)nodes);
#endif

	return (bl);
}

void
blist_destroy(blist_t bl)
{

	free(bl, M_SWAP);
}

/*
 * blist_alloc() -   reserve space in the block bitmap.  Return the base
 *		     of a contiguous region or SWAPBLK_NONE if space could
 *		     not be allocated.
 */
daddr_t
blist_alloc(blist_t bl, int *count, int maxcount)
{
	daddr_t blk, cursor;

	KASSERT(*count <= maxcount,
	    ("invalid parameters %d > %d", *count, maxcount));
	KASSERT(*count <= BLIST_MAX_ALLOC,
	    ("minimum allocation too large: %d", *count));

	/*
	 * This loop iterates at most twice.  An allocation failure in the
	 * first iteration leads to a second iteration only if the cursor was
	 * non-zero.  When the cursor is zero, an allocation failure will
	 * stop further iterations.
	 */
	for (cursor = bl->bl_cursor;; cursor = 0) {
		blk = blst_meta_alloc(bl->bl_root, cursor, count, maxcount,
		    bl->bl_radix);
		if (blk != SWAPBLK_NONE) {
			bl->bl_avail -= *count;
			bl->bl_cursor = blk + *count;
			if (bl->bl_cursor == bl->bl_blocks)
				bl->bl_cursor = 0;
			return (blk);
		}
		if (cursor == 0)
			return (SWAPBLK_NONE);
	}
}

/*
 * blist_avail() -	return the number of free blocks.
 */
daddr_t
blist_avail(blist_t bl)
{

	return (bl->bl_avail);
}

/*
 * blist_free() -	free up space in the block bitmap.  Return the base
 *		     	of a contiguous region.
 */
void
blist_free(blist_t bl, daddr_t blkno, daddr_t count)
{

	KASSERT(blkno >= 0 && blkno + count <= bl->bl_blocks,
	    ("freeing invalid range: blkno %jx, count %d, blocks %jd",
	    (uintmax_t)blkno, (int)count, (uintmax_t)bl->bl_blocks));
	blst_meta_free(bl->bl_root, blkno, count, bl->bl_radix);
	bl->bl_avail += count;
}

/*
 * blist_fill() -	mark a region in the block bitmap as off-limits
 *			to the allocator (i.e. allocate it), ignoring any
 *			existing allocations.  Return the number of blocks
 *			actually filled that were free before the call.
 */
daddr_t
blist_fill(blist_t bl, daddr_t blkno, daddr_t count)
{
	daddr_t filled;

	KASSERT(blkno >= 0 && blkno + count <= bl->bl_blocks,
	    ("filling invalid range: blkno %jx, count %d, blocks %jd",
	    (uintmax_t)blkno, (int)count, (uintmax_t)bl->bl_blocks));
	filled = blst_meta_fill(bl->bl_root, blkno, count, bl->bl_radix);
	bl->bl_avail -= filled;
	return (filled);
}

/*
 * blist_resize() -	resize an existing radix tree to handle the
 *			specified number of blocks.  This will reallocate
 *			the tree and transfer the previous bitmap to the new
 *			one.  When extending the tree you can specify whether
 *			the new blocks are to left allocated or freed.
 */
void
blist_resize(blist_t *pbl, daddr_t count, int freenew, int flags)
{
    blist_t newbl = blist_create(count, flags);
    blist_t save = *pbl;

    *pbl = newbl;
    if (count > save->bl_blocks)
	    count = save->bl_blocks;
    blst_copy(save->bl_root, 0, save->bl_radix, newbl, count);

    /*
     * If resizing upwards, should we free the new space or not?
     */
    if (freenew && count < newbl->bl_blocks) {
	    blist_free(newbl, count, newbl->bl_blocks - count);
    }
    blist_destroy(save);
}

#ifdef BLIST_DEBUG

/*
 * blist_print()    - dump radix tree
 */
void
blist_print(blist_t bl)
{
	printf("BLIST avail = %jd, cursor = %08jx {\n",
	    (uintmax_t)bl->bl_avail, (uintmax_t)bl->bl_cursor);

	if (bl->bl_root->bm_bitmap != 0)
		blst_radix_print(bl->bl_root, 0, bl->bl_radix, 4);
	printf("}\n");
}

#endif

static const u_daddr_t fib[] = {
	1, 2, 3, 5, 8, 13, 21, 34, 55, 89, 144, 233, 377, 610, 987, 1597, 2584,
	4181, 6765, 10946, 17711, 28657, 46368, 75025, 121393, 196418, 317811,
	514229, 832040, 1346269, 2178309, 3524578,
};

/*
 * Use 'gap' to describe a maximal range of unallocated blocks/bits.
 */
struct gap_stats {
	daddr_t	start;		/* current gap start, or SWAPBLK_NONE */
	daddr_t	num;		/* number of gaps observed */
	daddr_t	max;		/* largest gap size */
	daddr_t	avg;		/* average gap size */
	daddr_t	err;		/* sum - num * avg */
	daddr_t	histo[nitems(fib)]; /* # gaps in each size range */
	int	max_bucket;	/* last histo elt with nonzero val */
};

/*
 * gap_stats_counting()    - is the state 'counting 1 bits'?
 *                           or 'skipping 0 bits'?
 */
static inline bool
gap_stats_counting(const struct gap_stats *stats)
{

	return (stats->start != SWAPBLK_NONE);
}

/*
 * init_gap_stats()    - initialize stats on gap sizes
 */
static inline void
init_gap_stats(struct gap_stats *stats)
{

	bzero(stats, sizeof(*stats));
	stats->start = SWAPBLK_NONE;
}

/*
 * update_gap_stats()    - update stats on gap sizes
 */
static void
update_gap_stats(struct gap_stats *stats, daddr_t posn)
{
	daddr_t size;
	int hi, lo, mid;

	if (!gap_stats_counting(stats)) {
		stats->start = posn;
		return;
	}
	size = posn - stats->start;
	stats->start = SWAPBLK_NONE;
	if (size > stats->max)
		stats->max = size;

	/*
	 * Find the fibonacci range that contains size,
	 * expecting to find it in an early range.
	 */
	lo = 0;
	hi = 1;
	while (hi < nitems(fib) && fib[hi] <= size) {
		lo = hi;
		hi *= 2;
	}
	if (hi >= nitems(fib))
		hi = nitems(fib);
	while (lo + 1 != hi) {
		mid = (lo + hi) >> 1;
		if (fib[mid] <= size)
			lo = mid;
		else
			hi = mid;
	}
	stats->histo[lo]++;
	if (lo > stats->max_bucket)
		stats->max_bucket = lo;
	stats->err += size - stats->avg;
	stats->num++;
	stats->avg += stats->err / stats->num;
	stats->err %= stats->num;
}

/*
 * dump_gap_stats()    - print stats on gap sizes
 */
static inline void
dump_gap_stats(const struct gap_stats *stats, struct sbuf *s)
{
	int i;

	sbuf_printf(s, "number of maximal free ranges: %jd\n",
	    (intmax_t)stats->num);
	sbuf_printf(s, "largest free range: %jd\n", (intmax_t)stats->max);
	sbuf_printf(s, "average maximal free range size: %jd\n",
	    (intmax_t)stats->avg);
	sbuf_cat(s, "number of maximal free ranges of different sizes:\n");
	sbuf_cat(s, "               count  |  size range\n");
	sbuf_cat(s, "               -----  |  ----------\n");
	for (i = 0; i < stats->max_bucket; i++) {
		if (stats->histo[i] != 0) {
			sbuf_printf(s, "%20jd  |  ",
			    (intmax_t)stats->histo[i]);
			if (fib[i] != fib[i + 1] - 1)
				sbuf_printf(s, "%jd to %jd\n", (intmax_t)fib[i],
				    (intmax_t)fib[i + 1] - 1);
			else
				sbuf_printf(s, "%jd\n", (intmax_t)fib[i]);
		}
	}
	sbuf_printf(s, "%20jd  |  ", (intmax_t)stats->histo[i]);
	if (stats->histo[i] > 1)
		sbuf_printf(s, "%jd to %jd\n", (intmax_t)fib[i],
		    (intmax_t)stats->max);
	else
		sbuf_printf(s, "%jd\n", (intmax_t)stats->max);
}

/*
 * blist_stats()    - dump radix tree stats
 */
void
blist_stats(blist_t bl, struct sbuf *s)
{
	struct gap_stats gstats;
	struct gap_stats *stats = &gstats;
	daddr_t i, nodes, radix;
	u_daddr_t diff, mask;
	int digit;

	init_gap_stats(stats);
	nodes = 0;
	radix = bl->bl_radix;
	for (i = 0; i < bl->bl_blocks; ) {
		/*
		 * Check for skippable subtrees starting at i.
		 */
		while (radix != 1) {
			if (bl->bl_root[nodes].bm_bitmap == 0) {
				if (gap_stats_counting(stats))
					update_gap_stats(stats, i);
				break;
			}

			/*
			 * Skip subtree root.
			 */
			nodes++;
			radix /= BLIST_RADIX;
		}
		if (radix == 1) {
			/*
			 * Scan leaf.
			 */
			mask = bl->bl_root[nodes].bm_bitmap;
			diff = mask ^ (mask << 1);
			if (gap_stats_counting(stats))
				diff ^= 1;
			while (diff != 0) {
				digit = bitpos(diff);
				update_gap_stats(stats, i + digit);
				diff ^= bitrange(digit, 1);
			}
		}
		nodes += radix_to_skip(radix * BLIST_RADIX);
		i += radix * BLIST_RADIX;

		/*
		 * Find max size subtree starting at i.
		 */
		for (radix = 1; 
		    ((i / BLIST_RADIX / radix) & BLIST_MASK) == 0;
		    radix *= BLIST_RADIX)
			;
	}
	update_gap_stats(stats, i);
	dump_gap_stats(stats, s);
}

/************************************************************************
 *			  ALLOCATION SUPPORT FUNCTIONS			*
 ************************************************************************
 *
 *	These support functions do all the actual work.  They may seem
 *	rather longish, but that's because I've commented them up.  The
 *	actual code is straight forward.
 *
 */

/*
 * BLST_NEXT_LEAF_ALLOC() - allocate the blocks starting with the next leaf.
 *
 *	'scan' is a leaf node, and its first block is at address 'start'.  The
 *	next leaf node could be adjacent, or several nodes away if the least
 *	common ancestor of 'scan' and its neighbor is several levels up.  Use
 *	addresses to determine how many meta-nodes lie between the leaves.  If
 *	sequence of leaves starting with the next one has enough initial bits
 *	set, clear them and clear the bits in the meta nodes on the path up to
 *	the least common ancestor to mark any subtrees made completely empty.
 */
static int
blst_next_leaf_alloc(blmeta_t *scan, daddr_t start, int count, int maxcount)
{
	u_daddr_t radix;
	daddr_t blk;
	int avail, digit;

	start += BLIST_RADIX;
	for (blk = start; blk - start < maxcount; blk += BLIST_RADIX) {
		/* Skip meta-nodes, as long as they promise more free blocks. */
		radix = BLIST_RADIX;
		while (((++scan)->bm_bitmap & 1) == 1 &&
		    ((blk / radix) & BLIST_MASK) == 0)
			radix *= BLIST_RADIX;
		if (~scan->bm_bitmap != 0) {
			/*
			 * Either there is no next leaf with any free blocks,
			 * or we've reached the next leaf and found that some
			 * of its blocks are not free.  In the first case,
			 * bitpos() returns zero here.
			 */
			avail = blk - start + bitpos(~scan->bm_bitmap);
			if (avail < count || avail == 0) {
				/*
				 * There isn't a next leaf with enough free
				 * blocks at its beginning to bother
				 * allocating.
				 */
				return (avail);
			}
			maxcount = imin(avail, maxcount);
			if (maxcount % BLIST_RADIX == 0) {
				/*
				 * There was no next leaf.  Back scan up to
				 * last leaf.
				 */
				do {
					radix /= BLIST_RADIX;
					--scan;
				} while (radix != 1);
				blk -= BLIST_RADIX;
			}
		}
	}

	/*
	 * 'scan' is the last leaf that provides blocks.  Clear from 1 to
	 * BLIST_RADIX bits to represent the allocation of those last blocks.
	 */
	if (maxcount % BLIST_RADIX != 0)
		scan->bm_bitmap &= ~bitrange(0, maxcount % BLIST_RADIX);
	else
		scan->bm_bitmap = 0;

	for (;;) {
		/* Back up over meta-nodes, clearing bits if necessary. */
		blk -= BLIST_RADIX;
		for (radix = BLIST_RADIX;
		    (digit = ((blk / radix) & BLIST_MASK)) == 0;
		    radix *= BLIST_RADIX) {
			if ((scan--)->bm_bitmap == 0)
				scan->bm_bitmap ^= 1;
		}
		if ((scan--)->bm_bitmap == 0)
			scan[-digit * radix_to_skip(radix)].bm_bitmap ^=
			    (u_daddr_t)1 << digit;

		if (blk == start)
			break;
		/* Clear all the bits of this leaf. */
		scan->bm_bitmap = 0;
	}
	return (maxcount);
}

/*
 * BLST_LEAF_ALLOC() -	allocate at a leaf in the radix tree (a bitmap).
 *
 *	This function is the core of the allocator.  Its execution time is
 *	proportional to log(count), plus height of the tree if the allocation
 *	crosses a leaf boundary.
 */
static daddr_t
blst_leaf_alloc(blmeta_t *scan, daddr_t blk, int *count, int maxcount)
{
	u_daddr_t mask;
	int bighint, count1, hi, lo, num_shifts;

	count1 = *count - 1;
	num_shifts = fls(count1);
	mask = ~scan->bm_bitmap;
	while ((mask & (mask + 1)) != 0 && num_shifts > 0) {
		/*
		 * If bit i is 0 in mask, then bits in [i, i + (count1 >>
		 * num_shifts)] are 1 in scan->bm_bitmap.  Reduce num_shifts to
		 * 0, while preserving this invariant.  The updates to mask
		 * leave fewer bits 0, but each bit that remains 0 represents a
		 * longer string of consecutive 1-bits in scan->bm_bitmap.  If
		 * more updates to mask cannot set more bits, because mask is
		 * partitioned with all 1 bits following all 0 bits, the loop
		 * terminates immediately.
		 */
		num_shifts--;
		mask |= mask >> ((count1 >> num_shifts) + 1) / 2;
	}
	bighint = count1 >> num_shifts;
	if (~mask == 0) {
		/*
		 * Update bighint.  There is no allocation bigger than
		 * count1 >> num_shifts starting in this leaf.
		 */
		scan->bm_bighint = bighint;
		return (SWAPBLK_NONE);
	}

	/* Discard any candidates that appear before blk. */
	if ((blk & BLIST_MASK) != 0) {
		if ((~mask & bitrange(0, blk & BLIST_MASK)) != 0) {
			/* Grow bighint in case all discarded bits are set. */
			bighint += blk & BLIST_MASK;
			mask |= bitrange(0, blk & BLIST_MASK);
			if (~mask == 0) {
				scan->bm_bighint = bighint;
				return (SWAPBLK_NONE);
			}
		}
		blk -= blk & BLIST_MASK;
	}

	/*
	 * The least significant set bit in mask marks the start of the first
	 * available range of sufficient size.  Find its position.
	 */
	lo = bitpos(~mask);

	/*
	 * Find how much space is available starting at that position.
	 */
	if ((mask & (mask + 1)) != 0) {
		/* Count the 1 bits starting at position lo. */
		hi = bitpos(mask & (mask + 1)) + count1;
		if (maxcount < hi - lo)
			hi = lo + maxcount;
		*count = hi - lo;
		mask = ~bitrange(lo, *count);
	} else if (maxcount <= BLIST_RADIX - lo) {
		/* All the blocks we can use are available here. */
		hi = lo + maxcount;
		*count = maxcount;
		mask = ~bitrange(lo, *count);
		if (hi == BLIST_RADIX)
			scan->bm_bighint = bighint;
	} else {
		/* Check next leaf for some of the blocks we want or need. */
		count1 = *count - (BLIST_RADIX - lo);
		maxcount -= BLIST_RADIX - lo;
		hi = blst_next_leaf_alloc(scan, blk, count1, maxcount);
		if (hi < count1)
			/*
			 * The next leaf cannot supply enough blocks to reach
			 * the minimum required allocation.  The hint cannot be
			 * updated, because the same allocation request could
			 * be satisfied later, by this leaf, if the state of
			 * the next leaf changes, and without any changes to
			 * this leaf.
			 */
			return (SWAPBLK_NONE);
		*count = BLIST_RADIX - lo + hi;
		scan->bm_bighint = bighint;
	}

	/* Clear the allocated bits from this leaf. */
	scan->bm_bitmap &= mask;
	return (blk + lo);
}

/*
 * blist_meta_alloc() -	allocate at a meta in the radix tree.
 *
 *	Attempt to allocate at a meta node.  If we can't, we update
 *	bighint and return a failure.  Updating bighint optimize future
 *	calls that hit this node.  We have to check for our collapse cases
 *	and we have a few optimizations strewn in as well.
 */
static daddr_t
blst_meta_alloc(blmeta_t *scan, daddr_t cursor, int *count,
    int maxcount, u_daddr_t radix)
{
	daddr_t blk, i, r, skip;
	u_daddr_t mask;
	bool scan_from_start;
	int digit;

	if (radix == 1)
		return (blst_leaf_alloc(scan, cursor, count, maxcount));
	blk = cursor & -(radix * BLIST_RADIX);
	scan_from_start = (cursor == blk);
	skip = radix_to_skip(radix);
	mask = scan->bm_bitmap;

	/* Discard any candidates that appear before cursor. */
	digit = (cursor / radix) & BLIST_MASK;
	mask &= (u_daddr_t)-1 << digit;
	if (mask == 0)
		return (SWAPBLK_NONE);

	/*
	 * If the first try is for a block that includes the cursor, pre-undo
	 * the digit * radix offset in the first call; otherwise, ignore the
	 * cursor entirely.
	 */
	if (((mask >> digit) & 1) == 1)
		cursor -= digit * radix;
	else
		cursor = blk;

	/*
	 * Examine the nonempty subtree associated with each bit set in mask.
	 */
	do {
		digit = bitpos(mask);
		i = 1 + digit * skip;
		if (*count <= scan[i].bm_bighint) {
			/*
			 * The allocation might fit beginning in the i'th subtree.
			 */
			r = blst_meta_alloc(&scan[i], cursor + digit * radix,
			    count, maxcount, radix / BLIST_RADIX);
			if (r != SWAPBLK_NONE) {
				if (scan[i].bm_bitmap == 0)
					scan->bm_bitmap ^= bitrange(digit, 1);
				return (r);
			}
		}
		cursor = blk;
	} while ((mask ^= bitrange(digit, 1)) != 0);

	/*
	 * We couldn't allocate count in this subtree.  If the whole tree was
	 * scanned, and the last tree node is allocated, update bighint.
	 */
	if (scan_from_start && !(digit == BLIST_RADIX - 1 &&
	    scan[i].bm_bighint == BLIST_MAX_ALLOC))
		scan->bm_bighint = *count - 1;

	return (SWAPBLK_NONE);
}

/*
 * BLST_LEAF_FREE() -	free allocated block from leaf bitmap
 *
 */
static void
blst_leaf_free(blmeta_t *scan, daddr_t blk, int count)
{
	u_daddr_t mask;

	/*
	 * free some data in this bitmap
	 * mask=0000111111111110000
	 *          \_________/\__/
	 *		count   n
	 */
	mask = bitrange(blk & BLIST_MASK, count);
	KASSERT((scan->bm_bitmap & mask) == 0,
	    ("freeing free block: %jx, size %d, mask %jx",
	    (uintmax_t)blk, count, (uintmax_t)scan->bm_bitmap & mask));
	scan->bm_bitmap |= mask;
}

/*
 * BLST_META_FREE() - free allocated blocks from radix tree meta info
 *
 *	This support routine frees a range of blocks from the bitmap.
 *	The range must be entirely enclosed by this radix node.  If a
 *	meta node, we break the range down recursively to free blocks
 *	in subnodes (which means that this code can free an arbitrary
 *	range whereas the allocation code cannot allocate an arbitrary
 *	range).
 */
static void
blst_meta_free(blmeta_t *scan, daddr_t freeBlk, daddr_t count, u_daddr_t radix)
{
	daddr_t blk, endBlk, i, skip;
	int digit, endDigit;

	/*
	 * We could probably do a better job here.  We are required to make
	 * bighint at least as large as the biggest allocable block of data.
	 * If we just shoehorn it, a little extra overhead will be incurred
	 * on the next allocation (but only that one typically).
	 */
	scan->bm_bighint = BLIST_MAX_ALLOC;

	if (radix == 1)
		return (blst_leaf_free(scan, freeBlk, count));

	endBlk = freeBlk + count;
	blk = (freeBlk + radix * BLIST_RADIX) & -(radix * BLIST_RADIX);
	/*
	 * blk is first block past the end of the range of this meta node,
	 * or 0 in case of overflow.
	 */
	if (blk != 0)
		endBlk = ummin(endBlk, blk);
	skip = radix_to_skip(radix);
	blk = freeBlk & -radix;
	digit = (blk / radix) & BLIST_MASK;
	endDigit = 1 + (((endBlk - 1) / radix) & BLIST_MASK);
	scan->bm_bitmap |= bitrange(digit, endDigit - digit);
	for (i = 1 + digit * skip; blk < endBlk; i += skip) {
		blk += radix;
		count = ummin(blk, endBlk) - freeBlk;
		blst_meta_free(&scan[i], freeBlk, count, radix / BLIST_RADIX);
		freeBlk = blk;
	}
}

/*
 * BLST_COPY() - copy one radix tree to another
 *
 *	Locates free space in the source tree and frees it in the destination
 *	tree.  The space may not already be free in the destination.
 */
static void
blst_copy(blmeta_t *scan, daddr_t blk, daddr_t radix, blist_t dest,
    daddr_t count)
{
	daddr_t endBlk, i, skip;

	/*
	 * Leaf node
	 */

	if (radix == 1) {
		u_daddr_t v = scan->bm_bitmap;

		if (v == (u_daddr_t)-1) {
			blist_free(dest, blk, count);
		} else if (v != 0) {
			int i;

			for (i = 0; i < count; ++i) {
				if (v & ((u_daddr_t)1 << i))
					blist_free(dest, blk + i, 1);
			}
		}
		return;
	}

	/*
	 * Meta node
	 */

	if (scan->bm_bitmap == 0) {
		/*
		 * Source all allocated, leave dest allocated
		 */
		return;
	}

	endBlk = blk + count;
	skip = radix_to_skip(radix);
	for (i = 1; blk < endBlk; i += skip) {
		blk += radix;
		count = radix;
		if (blk >= endBlk)
			count -= blk - endBlk;
		blst_copy(&scan[i], blk - radix,
		    radix / BLIST_RADIX, dest, count);
	}
}

/*
 * BLST_LEAF_FILL() -	allocate specific blocks in leaf bitmap
 *
 *	This routine allocates all blocks in the specified range
 *	regardless of any existing allocations in that range.  Returns
 *	the number of blocks allocated by the call.
 */
static daddr_t
blst_leaf_fill(blmeta_t *scan, daddr_t blk, int count)
{
	daddr_t nblks;
	u_daddr_t mask;

	mask = bitrange(blk & BLIST_MASK, count);

	/* Count the number of blocks that we are allocating. */
	nblks = bitcount64(scan->bm_bitmap & mask);

	scan->bm_bitmap &= ~mask;
	return (nblks);
}

/*
 * BLIST_META_FILL() -	allocate specific blocks at a meta node
 *
 *	This routine allocates the specified range of blocks,
 *	regardless of any existing allocations in the range.  The
 *	range must be within the extent of this node.  Returns the
 *	number of blocks allocated by the call.
 */
static daddr_t
blst_meta_fill(blmeta_t *scan, daddr_t allocBlk, daddr_t count, u_daddr_t radix)
{
	daddr_t blk, endBlk, i, nblks, skip;
	int digit;

	if (radix == 1)
		return (blst_leaf_fill(scan, allocBlk, count));

	endBlk = allocBlk + count;
	blk = (allocBlk + radix * BLIST_RADIX) & -(radix * BLIST_RADIX);
	/*
	 * blk is first block past the end of the range of this meta node,
	 * or 0 in case of overflow.
	 */
	if (blk != 0)
		endBlk = ummin(endBlk, blk);
	skip = radix_to_skip(radix);
	blk = allocBlk & -radix;
	nblks = 0;
	while (blk < endBlk) {
		digit = (blk / radix) & BLIST_MASK;
		i = 1 + digit * skip;
		blk += radix;
		count = ummin(blk, endBlk) - allocBlk;
		nblks += blst_meta_fill(&scan[i], allocBlk, count,
		    radix / BLIST_RADIX);
		if (scan[i].bm_bitmap == 0)
			scan->bm_bitmap &= ~((u_daddr_t)1 << digit);
		allocBlk = blk;
	}
	return (nblks);
}

#ifdef BLIST_DEBUG

static void
blst_radix_print(blmeta_t *scan, daddr_t blk, daddr_t radix, int tab)
{
	daddr_t skip;
	u_daddr_t mask;
	int digit;

	if (radix == 1) {
		printf(
		    "%*.*s(%08llx,%lld): bitmap %0*llx big=%lld\n",
		    tab, tab, "",
		    (long long)blk, (long long)BLIST_RADIX,
		    (int)(1 + (BLIST_RADIX - 1) / 4),
		    (long long)scan->bm_bitmap,
		    (long long)scan->bm_bighint
		);
		return;
	}

	printf(
	    "%*.*s(%08llx): subtree (%lld/%lld) bitmap %0*llx big=%lld {\n",
	    tab, tab, "",
	    (long long)blk, (long long)radix * BLIST_RADIX,
	    (long long)radix * BLIST_RADIX,
	    (int)(1 + (BLIST_RADIX - 1) / 4),
	    (long long)scan->bm_bitmap,
	    (long long)scan->bm_bighint
	);

	skip = radix_to_skip(radix);
	tab += 4;

	mask = scan->bm_bitmap;
	/* Examine the nonempty subtree associated with each bit set in mask */
	do {
		digit = bitpos(mask);
		blst_radix_print(&scan[1 + digit * skip], blk + digit * radix,
		    radix / BLIST_RADIX, tab);
	} while ((mask ^= bitrange(digit, 1)) != 0);
	tab -= 4;

	printf(
	    "%*.*s}\n",
	    tab, tab, ""
	);
}

#endif

#ifdef BLIST_DEBUG

int
main(int ac, char **av)
{
	daddr_t size = BLIST_RADIX * BLIST_RADIX;
	int i;
	blist_t bl;
	struct sbuf *s;

	for (i = 1; i < ac; ++i) {
		const char *ptr = av[i];
		if (*ptr != '-') {
			size = strtoll(ptr, NULL, 0);
			continue;
		}
		ptr += 2;
		fprintf(stderr, "Bad option: %s\n", ptr - 2);
		exit(1);
	}
	bl = blist_create(size, M_WAITOK);
	if (bl == NULL) {
		fprintf(stderr, "blist_create failed\n");
		exit(1);
	}
	blist_free(bl, 0, size);

	for (;;) {
		char buf[1024];
		long long da = 0;
		int count = 0, maxcount = 0;

		printf("%lld/%lld/%lld> ", (long long)blist_avail(bl),
		    (long long)size, (long long)bl->bl_radix * BLIST_RADIX);
		fflush(stdout);
		if (fgets(buf, sizeof(buf), stdin) == NULL)
			break;
		switch(buf[0]) {
		case 'r':
			if (sscanf(buf + 1, "%d", &count) == 1) {
				blist_resize(&bl, count, 1, M_WAITOK);
			} else {
				printf("?\n");
			}
		case 'p':
			blist_print(bl);
			break;
		case 's':
			s = sbuf_new_auto();
			blist_stats(bl, s);
			sbuf_finish(s);
			printf("%s", sbuf_data(s));
			sbuf_delete(s);
			break;
		case 'a':
			if (sscanf(buf + 1, "%d%d", &count, &maxcount) == 2) {
				daddr_t blk = blist_alloc(bl, &count, maxcount);
				printf("    R=%08llx, c=%08d\n",
				    (long long)blk, count);
			} else {
				printf("?\n");
			}
			break;
		case 'f':
			if (sscanf(buf + 1, "%llx %d", &da, &count) == 2) {
				blist_free(bl, da, count);
			} else {
				printf("?\n");
			}
			break;
		case 'l':
			if (sscanf(buf + 1, "%llx %d", &da, &count) == 2) {
				printf("    n=%jd\n",
				    (intmax_t)blist_fill(bl, da, count));
			} else {
				printf("?\n");
			}
			break;
		case '?':
		case 'h':
			puts(
			    "p          -print\n"
			    "s          -stats\n"
			    "a %d %d    -allocate\n"
			    "f %x %d    -free\n"
			    "l %x %d    -fill\n"
			    "r %d       -resize\n"
			    "h/?        -help\n"
			    "q          -quit"
			);
			break;
		case 'q':
			break;
		default:
			printf("?\n");
			break;
		}
		if (buf[0] == 'q')
			break;
	}
	return (0);
}

#endif
