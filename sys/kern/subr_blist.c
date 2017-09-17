/*-
 * Copyright (c) 1998 Matthew Dillon.  All Rights Reserved.
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 4. Neither the name of the University nor the names of its contributors
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
 *	A radix tree is used to maintain the bitmap.  Two radix constants are
 *	involved:  One for the bitmaps contained in the leaf nodes (typically
 *	64), and one for the meta nodes (typically 16).  Both meta and leaf
 *	nodes have a hint field.  This field gives us a hint as to the largest
 *	free contiguous range of blocks under the node.  It may contain a
 *	value that is too high, but will never contain a value that is too
 *	low.  When the radix tree is searched, allocation failures in subtrees
 *	update the hint.
 *
 *	The radix tree also implements two collapsed states for meta nodes:
 *	the ALL-ALLOCATED state and the ALL-FREE state.  If a meta node is
 *	in either of these two states, all information contained underneath
 *	the node is considered stale.  These states are used to optimize
 *	allocation and freeing operations.
 *
 * 	The hinting greatly increases code efficiency for allocations while
 *	the general radix structure optimizes both allocations and frees.  The
 *	radix tree should be able to operate well no matter how much
 *	fragmentation there is and no matter how large a bitmap is used.
 *
 *	The blist code wires all necessary memory at creation time.  Neither
 *	allocations nor frees require interaction with the memory subsystem.
 *	The non-blocking features of the blist code are used in the swap code
 *	(vm/swap_pager.c).
 *
 *	LAYOUT: The radix tree is laid out recursively using a
 *	linear array.  Each meta node is immediately followed (laid out
 *	sequentially in memory) by BLIST_META_RADIX lower level nodes.  This
 *	is a recursive structure but one that can be easily scanned through
 *	a very simple 'skip' calculation.  In order to support large radixes,
 *	portions of the tree may reside outside our memory allocation.  We
 *	handle this with an early-termination optimization (when bighint is
 *	set to -1) on the scan.  The memory allocation is only large enough
 *	to cover the number of blocks requested at creation time even if it
 *	must be encompassed in larger root-node radix.
 *
 *	NOTE: the allocator cannot currently allocate more than
 *	BLIST_BMAP_RADIX blocks per call.  It will panic with 'allocation too
 *	large' if you try.  This is an area that could use improvement.  The
 *	radix is large enough that this restriction does not effect the swap
 *	system, though.  Currently only the allocation code is affected by
 *	this algorithmic unfeature.  The freeing code can handle arbitrary
 *	ranges.
 *
 *	This code can be compiled stand-alone for debugging.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#ifdef _KERNEL

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/lock.h>
#include <sys/kernel.h>
#include <sys/blist.h>
#include <sys/malloc.h>
#include <sys/proc.h>
#include <sys/mutex.h>

#else

#ifndef BLIST_NO_DEBUG
#define BLIST_DEBUG
#endif

#include <sys/types.h>
#include <sys/malloc.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdbool.h>

#define	bitcount64(x)	__bitcount64((uint64_t)(x))
#define malloc(a,b,c)	calloc(a, 1)
#define free(a,b)	free(a)
#define CTASSERT(expr)

#include <sys/blist.h>

void panic(const char *ctl, ...);

#endif

/*
 * static support functions
 */
static daddr_t	blst_leaf_alloc(blmeta_t *scan, daddr_t blk, int count,
		    daddr_t cursor);
static daddr_t	blst_meta_alloc(blmeta_t *scan, daddr_t blk, daddr_t count,
		    daddr_t radix, daddr_t cursor);
static void blst_leaf_free(blmeta_t *scan, daddr_t relblk, int count);
static void blst_meta_free(blmeta_t *scan, daddr_t freeBlk, daddr_t count,
		    daddr_t radix, daddr_t blk);
static void blst_copy(blmeta_t *scan, daddr_t blk, daddr_t radix,
		    blist_t dest, daddr_t count);
static daddr_t blst_leaf_fill(blmeta_t *scan, daddr_t blk, int count);
static daddr_t blst_meta_fill(blmeta_t *scan, daddr_t allocBlk, daddr_t count,
		    daddr_t radix, daddr_t blk);
static daddr_t	blst_radix_init(blmeta_t *scan, daddr_t radix, daddr_t count);
#ifndef _KERNEL
static void	blst_radix_print(blmeta_t *scan, daddr_t blk, daddr_t radix,
		    int tab);
#endif

#ifdef _KERNEL
static MALLOC_DEFINE(M_SWAP, "SWAP", "Swap space");
#endif

CTASSERT(BLIST_BMAP_RADIX % BLIST_META_RADIX == 0);

/*
 * For a subtree that can represent the state of up to 'radix' blocks, the
 * number of leaf nodes of the subtree is L=radix/BLIST_BMAP_RADIX.  If 'm'
 * is short for BLIST_META_RADIX, then for a tree of height h with L=m**h
 * leaf nodes, the total number of tree nodes is 1 + m + m**2 + ... + m**h,
 * or, equivalently, (m**(h+1)-1)/(m-1).  This quantity is called 'skip'
 * in the 'meta' functions that process subtrees.  Since integer division
 * discards remainders, we can express this computation as
 * skip = (m * m**h) / (m - 1)
 * skip = (m * (radix / BLIST_BMAP_RADIX)) / (m - 1)
 * and since m divides BLIST_BMAP_RADIX, we can simplify further to
 * skip = (radix / (BLIST_BMAP_RADIX / m)) / (m - 1)
 * skip = radix / ((BLIST_BMAP_RADIX / m) * (m - 1))
 * so that simple integer division by a constant can safely be used for the
 * calculation.
 */
static inline daddr_t
radix_to_skip(daddr_t radix)
{

	return (radix /
	    ((BLIST_BMAP_RADIX / BLIST_META_RADIX) * (BLIST_META_RADIX - 1)));
}

/*
 * blist_create() - create a blist capable of handling up to the specified
 *		    number of blocks
 *
 *	blocks - must be greater than 0
 * 	flags  - malloc flags
 *
 *	The smallest blist consists of a single leaf node capable of
 *	managing BLIST_BMAP_RADIX blocks.
 */
blist_t
blist_create(daddr_t blocks, int flags)
{
	blist_t bl;
	daddr_t nodes, radix;

	/*
	 * Calculate the radix field used for scanning.
	 */
	radix = BLIST_BMAP_RADIX;
	while (radix < blocks) {
		radix *= BLIST_META_RADIX;
	}
	nodes = 1 + blst_radix_init(NULL, radix, blocks);

	bl = malloc(sizeof(struct blist), M_SWAP, flags);
	if (bl == NULL)
		return (NULL);

	bl->bl_blocks = blocks;
	bl->bl_radix = radix;
	bl->bl_cursor = 0;
	bl->bl_root = malloc(nodes * sizeof(blmeta_t), M_SWAP, flags);
	if (bl->bl_root == NULL) {
		free(bl, M_SWAP);
		return (NULL);
	}
	blst_radix_init(bl->bl_root, radix, blocks);

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
	free(bl->bl_root, M_SWAP);
	free(bl, M_SWAP);
}

/*
 * blist_alloc() -   reserve space in the block bitmap.  Return the base
 *		     of a contiguous region or SWAPBLK_NONE if space could
 *		     not be allocated.
 */
daddr_t
blist_alloc(blist_t bl, daddr_t count)
{
	daddr_t blk;

	/*
	 * This loop iterates at most twice.  An allocation failure in the
	 * first iteration leads to a second iteration only if the cursor was
	 * non-zero.  When the cursor is zero, an allocation failure will
	 * reduce the hint, stopping further iterations.
	 */
	while (count <= bl->bl_root->bm_bighint) {
		blk = blst_meta_alloc(bl->bl_root, 0, count, bl->bl_radix,
		    bl->bl_cursor);
		if (blk != SWAPBLK_NONE) {
			bl->bl_cursor = blk + count;
			return (blk);
		} else if (bl->bl_cursor != 0)
			bl->bl_cursor = 0;
	}
	return (SWAPBLK_NONE);
}

/*
 * blist_avail() -	return the number of free blocks.
 */
daddr_t
blist_avail(blist_t bl)
{

	if (bl->bl_radix == BLIST_BMAP_RADIX)
		return (bitcount64(bl->bl_root->u.bmu_bitmap));
	else
		return (bl->bl_root->u.bmu_avail);
}

/*
 * blist_free() -	free up space in the block bitmap.  Return the base
 *		     	of a contiguous region.  Panic if an inconsistancy is
 *			found.
 */
void
blist_free(blist_t bl, daddr_t blkno, daddr_t count)
{

	blst_meta_free(bl->bl_root, blkno, count, bl->bl_radix, 0);
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

	return (blst_meta_fill(bl->bl_root, blkno, count, bl->bl_radix, 0));
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
	printf("BLIST cursor = %08jx {\n", (uintmax_t)bl->bl_cursor);
	blst_radix_print(bl->bl_root, 0, bl->bl_radix, 4);
	printf("}\n");
}

#endif

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
 * blist_leaf_alloc() -	allocate at a leaf in the radix tree (a bitmap).
 *
 *	This is the core of the allocator and is optimized for the
 *	BLIST_BMAP_RADIX block allocation case.  Otherwise, execution
 *	time is proportional to log2(count) + log2(BLIST_BMAP_RADIX).
 */
static daddr_t
blst_leaf_alloc(blmeta_t *scan, daddr_t blk, int count, daddr_t cursor)
{
	u_daddr_t mask;
	int count1, hi, lo, mid, num_shifts, range1, range_ext;

	if (count == BLIST_BMAP_RADIX) {
		/*
		 * Optimize allocation of BLIST_BMAP_RADIX bits.  If this wasn't
		 * a special case, then forming the final value of 'mask' below
		 * would require special handling to avoid an invalid left shift
		 * when count equals the number of bits in mask.
		 */
		if (~scan->u.bmu_bitmap != 0) {
			scan->bm_bighint = BLIST_BMAP_RADIX - 1;
			return (SWAPBLK_NONE);
		}
		if (cursor != blk)
			return (SWAPBLK_NONE);
		scan->u.bmu_bitmap = 0;
		scan->bm_bighint = 0;
		return (blk);
	}
	range1 = 0;
	count1 = count - 1;
	num_shifts = fls(count1);
	mask = scan->u.bmu_bitmap;
	while (mask != 0 && num_shifts > 0) {
		/*
		 * If bit i is set in mask, then bits in [i, i+range1] are set
		 * in scan->u.bmu_bitmap.  The value of range1 is equal to
		 * count1 >> num_shifts.  Grow range and reduce num_shifts to 0,
		 * while preserving these invariants.  The updates to mask leave
		 * fewer bits set, but each bit that remains set represents a
		 * longer string of consecutive bits set in scan->u.bmu_bitmap.
		 */
		num_shifts--;
		range_ext = range1 + ((count1 >> num_shifts) & 1);
		mask &= mask >> range_ext;
		range1 += range_ext;
	}
	if (mask == 0) {
		/*
		 * Update bighint.  There is no allocation bigger than range1
		 * available in this leaf.
		 */
		scan->bm_bighint = range1;
		return (SWAPBLK_NONE);
	}

	/*
	 * Discard any candidates that appear before the cursor.
	 */
	lo = cursor - blk;
	mask &= ~(u_daddr_t)0 << lo;

	if (mask == 0)
		return (SWAPBLK_NONE);

	/*
	 * The least significant set bit in mask marks the start of the first
	 * available range of sufficient size.  Clear all the bits but that one,
	 * and then perform a binary search to find its position.
	 */
	mask &= -mask;
	hi = BLIST_BMAP_RADIX - count1;
	while (lo + 1 < hi) {
		mid = (lo + hi) >> 1;
		if ((mask >> mid) != 0)
			lo = mid;
		else
			hi = mid;
	}

	/*
	 * Set in mask exactly the bits being allocated, and clear them from
	 * the set of available bits.
	 */
	mask = (mask << count) - mask;
	scan->u.bmu_bitmap &= ~mask;
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
blst_meta_alloc(blmeta_t *scan, daddr_t blk, daddr_t count, daddr_t radix,
    daddr_t cursor)
{
	daddr_t i, next_skip, r, skip;
	int child;
	bool scan_from_start;

	if (radix == BLIST_BMAP_RADIX)
		return (blst_leaf_alloc(scan, blk, count, cursor));
	if (scan->u.bmu_avail < count) {
		/*
		 * The meta node's hint must be too large if the allocation
		 * exceeds the number of free blocks.  Reduce the hint, and
		 * return failure.
		 */
		scan->bm_bighint = scan->u.bmu_avail;
		return (SWAPBLK_NONE);
	}
	skip = radix_to_skip(radix);
	next_skip = skip / BLIST_META_RADIX;

	/*
	 * An ALL-FREE meta node requires special handling before allocating
	 * any of its blocks.
	 */
	if (scan->u.bmu_avail == radix) {
		radix /= BLIST_META_RADIX;

		/*
		 * Reinitialize each of the meta node's children.  An ALL-FREE
		 * meta node cannot have a terminator in any subtree.
		 */
		for (i = 1; i < skip; i += next_skip) {
			if (next_skip == 1)
				scan[i].u.bmu_bitmap = (u_daddr_t)-1;
			else
				scan[i].u.bmu_avail = radix;
			scan[i].bm_bighint = radix;
		}
	} else {
		radix /= BLIST_META_RADIX;
	}

	if (count > radix) {
		/*
		 * The allocation exceeds the number of blocks that are
		 * managed by a subtree of this meta node.
		 */
		panic("allocation too large");
	}
	scan_from_start = cursor == blk;
	child = (cursor - blk) / radix;
	blk += child * radix;
	for (i = 1 + child * next_skip; i < skip; i += next_skip) {
		if (count <= scan[i].bm_bighint) {
			/*
			 * The allocation might fit in the i'th subtree.
			 */
			r = blst_meta_alloc(&scan[i], blk, count, radix,
			    cursor > blk ? cursor : blk);
			if (r != SWAPBLK_NONE) {
				scan->u.bmu_avail -= count;
				return (r);
			}
		} else if (scan[i].bm_bighint == (daddr_t)-1) {
			/*
			 * Terminator
			 */
			break;
		}
		blk += radix;
	}

	/*
	 * We couldn't allocate count in this subtree, update bighint.
	 */
	if (scan_from_start && scan->bm_bighint >= count)
		scan->bm_bighint = count - 1;

	return (SWAPBLK_NONE);
}

/*
 * BLST_LEAF_FREE() -	free allocated block from leaf bitmap
 *
 */
static void
blst_leaf_free(blmeta_t *scan, daddr_t blk, int count)
{
	/*
	 * free some data in this bitmap
	 *
	 * e.g.
	 *	0000111111111110000
	 *          \_________/\__/
	 *		v        n
	 */
	int n = blk & (BLIST_BMAP_RADIX - 1);
	u_daddr_t mask;

	mask = ((u_daddr_t)-1 << n) &
	    ((u_daddr_t)-1 >> (BLIST_BMAP_RADIX - count - n));

	if (scan->u.bmu_bitmap & mask)
		panic("blst_radix_free: freeing free block");
	scan->u.bmu_bitmap |= mask;

	/*
	 * We could probably do a better job here.  We are required to make
	 * bighint at least as large as the biggest contiguous block of
	 * data.  If we just shoehorn it, a little extra overhead will
	 * be incured on the next allocation (but only that one typically).
	 */
	scan->bm_bighint = BLIST_BMAP_RADIX;
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
blst_meta_free(blmeta_t *scan, daddr_t freeBlk, daddr_t count, daddr_t radix,
    daddr_t blk)
{
	daddr_t i, next_skip, skip, v;
	int child;

	if (scan->bm_bighint == (daddr_t)-1)
		panic("freeing invalid range");
	if (radix == BLIST_BMAP_RADIX)
		return (blst_leaf_free(scan, freeBlk, count));
	skip = radix_to_skip(radix);
	next_skip = skip / BLIST_META_RADIX;

	if (scan->u.bmu_avail == 0) {
		/*
		 * ALL-ALLOCATED special case, with possible
		 * shortcut to ALL-FREE special case.
		 */
		scan->u.bmu_avail = count;
		scan->bm_bighint = count;

		if (count != radix)  {
			for (i = 1; i < skip; i += next_skip) {
				if (scan[i].bm_bighint == (daddr_t)-1)
					break;
				scan[i].bm_bighint = 0;
				if (next_skip == 1) {
					scan[i].u.bmu_bitmap = 0;
				} else {
					scan[i].u.bmu_avail = 0;
				}
			}
			/* fall through */
		}
	} else {
		scan->u.bmu_avail += count;
		/* scan->bm_bighint = radix; */
	}

	/*
	 * ALL-FREE special case.
	 */

	if (scan->u.bmu_avail == radix)
		return;
	if (scan->u.bmu_avail > radix)
		panic("blst_meta_free: freeing already free blocks (%lld) %lld/%lld",
		    (long long)count, (long long)scan->u.bmu_avail,
		    (long long)radix);

	/*
	 * Break the free down into its components
	 */

	radix /= BLIST_META_RADIX;

	child = (freeBlk - blk) / radix;
	blk += child * radix;
	i = 1 + child * next_skip;
	while (i < skip && blk < freeBlk + count) {
		v = blk + radix - freeBlk;
		if (v > count)
			v = count;
		blst_meta_free(&scan[i], freeBlk, v, radix, blk);
		if (scan->bm_bighint < scan[i].bm_bighint)
			scan->bm_bighint = scan[i].bm_bighint;
		count -= v;
		freeBlk += v;
		blk += radix;
		i += next_skip;
	}
}

/*
 * BLIST_RADIX_COPY() - copy one radix tree to another
 *
 *	Locates free space in the source tree and frees it in the destination
 *	tree.  The space may not already be free in the destination.
 */
static void
blst_copy(blmeta_t *scan, daddr_t blk, daddr_t radix, blist_t dest,
    daddr_t count)
{
	daddr_t i, next_skip, skip;

	/*
	 * Leaf node
	 */

	if (radix == BLIST_BMAP_RADIX) {
		u_daddr_t v = scan->u.bmu_bitmap;

		if (v == (u_daddr_t)-1) {
			blist_free(dest, blk, count);
		} else if (v != 0) {
			int i;

			for (i = 0; i < BLIST_BMAP_RADIX && i < count; ++i) {
				if (v & ((u_daddr_t)1 << i))
					blist_free(dest, blk + i, 1);
			}
		}
		return;
	}

	/*
	 * Meta node
	 */

	if (scan->u.bmu_avail == 0) {
		/*
		 * Source all allocated, leave dest allocated
		 */
		return;
	}
	if (scan->u.bmu_avail == radix) {
		/*
		 * Source all free, free entire dest
		 */
		if (count < radix)
			blist_free(dest, blk, count);
		else
			blist_free(dest, blk, radix);
		return;
	}


	skip = radix_to_skip(radix);
	next_skip = skip / BLIST_META_RADIX;
	radix /= BLIST_META_RADIX;

	for (i = 1; count && i < skip; i += next_skip) {
		if (scan[i].bm_bighint == (daddr_t)-1)
			break;

		if (count >= radix) {
			blst_copy(&scan[i], blk, radix, dest, radix);
			count -= radix;
		} else {
			if (count) {
				blst_copy(&scan[i], blk, radix, dest, count);
			}
			count = 0;
		}
		blk += radix;
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
	int n = blk & (BLIST_BMAP_RADIX - 1);
	daddr_t nblks;
	u_daddr_t mask;

	mask = ((u_daddr_t)-1 << n) &
	    ((u_daddr_t)-1 >> (BLIST_BMAP_RADIX - count - n));

	/* Count the number of blocks that we are allocating. */
	nblks = bitcount64(scan->u.bmu_bitmap & mask);

	scan->u.bmu_bitmap &= ~mask;
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
blst_meta_fill(blmeta_t *scan, daddr_t allocBlk, daddr_t count, daddr_t radix,
    daddr_t blk)
{
	daddr_t i, nblks, next_skip, skip, v;
	int child;

	if (scan->bm_bighint == (daddr_t)-1)
		panic("filling invalid range");
	if (count > radix) {
		/*
		 * The allocation exceeds the number of blocks that are
		 * managed by this node.
		 */
		panic("fill too large");
	}
	if (radix == BLIST_BMAP_RADIX)
		return (blst_leaf_fill(scan, allocBlk, count));
	if (count == radix || scan->u.bmu_avail == 0)  {
		/*
		 * ALL-ALLOCATED special case
		 */
		nblks = scan->u.bmu_avail;
		scan->u.bmu_avail = 0;
		scan->bm_bighint = 0;
		return (nblks);
	}
	skip = radix_to_skip(radix);
	next_skip = skip / BLIST_META_RADIX;

	/*
	 * An ALL-FREE meta node requires special handling before allocating
	 * any of its blocks.
	 */
	if (scan->u.bmu_avail == radix) {
		radix /= BLIST_META_RADIX;

		/*
		 * Reinitialize each of the meta node's children.  An ALL-FREE
		 * meta node cannot have a terminator in any subtree.
		 */
		for (i = 1; i < skip; i += next_skip) {
			if (next_skip == 1)
				scan[i].u.bmu_bitmap = (u_daddr_t)-1;
			else
				scan[i].u.bmu_avail = radix;
			scan[i].bm_bighint = radix;
		}
	} else {
		radix /= BLIST_META_RADIX;
	}

	nblks = 0;
	child = (allocBlk - blk) / radix;
	blk += child * radix;
	i = 1 + child * next_skip;
	while (i < skip && blk < allocBlk + count) {
		v = blk + radix - allocBlk;
		if (v > count)
			v = count;
		nblks += blst_meta_fill(&scan[i], allocBlk, v, radix, blk);
		count -= v;
		allocBlk += v;
		blk += radix;
		i += next_skip;
	}
	scan->u.bmu_avail -= nblks;
	return (nblks);
}

/*
 * BLST_RADIX_INIT() - initialize radix tree
 *
 *	Initialize our meta structures and bitmaps and calculate the exact
 *	amount of space required to manage 'count' blocks - this space may
 *	be considerably less than the calculated radix due to the large
 *	RADIX values we use.
 */
static daddr_t
blst_radix_init(blmeta_t *scan, daddr_t radix, daddr_t count)
{
	daddr_t i, memindex, next_skip, skip;

	memindex = 0;

	/*
	 * Leaf node
	 */

	if (radix == BLIST_BMAP_RADIX) {
		if (scan) {
			scan->bm_bighint = 0;
			scan->u.bmu_bitmap = 0;
		}
		return (memindex);
	}

	/*
	 * Meta node.  If allocating the entire object we can special
	 * case it.  However, we need to figure out how much memory
	 * is required to manage 'count' blocks, so we continue on anyway.
	 */

	if (scan) {
		scan->bm_bighint = 0;
		scan->u.bmu_avail = 0;
	}

	skip = radix_to_skip(radix);
	next_skip = skip / BLIST_META_RADIX;
	radix /= BLIST_META_RADIX;

	for (i = 1; i < skip; i += next_skip) {
		if (count >= radix) {
			/*
			 * Allocate the entire object
			 */
			memindex = i +
			    blst_radix_init(((scan) ? &scan[i] : NULL), radix,
			    radix);
			count -= radix;
		} else if (count > 0) {
			/*
			 * Allocate a partial object
			 */
			memindex = i +
			    blst_radix_init(((scan) ? &scan[i] : NULL), radix,
			    count);
			count = 0;
		} else {
			/*
			 * Add terminator and break out
			 */
			if (scan)
				scan[i].bm_bighint = (daddr_t)-1;
			break;
		}
	}
	if (memindex < i)
		memindex = i;
	return (memindex);
}

#ifdef BLIST_DEBUG

static void
blst_radix_print(blmeta_t *scan, daddr_t blk, daddr_t radix, int tab)
{
	daddr_t i, next_skip, skip;

	if (radix == BLIST_BMAP_RADIX) {
		printf(
		    "%*.*s(%08llx,%lld): bitmap %016llx big=%lld\n",
		    tab, tab, "",
		    (long long)blk, (long long)radix,
		    (long long)scan->u.bmu_bitmap,
		    (long long)scan->bm_bighint
		);
		return;
	}

	if (scan->u.bmu_avail == 0) {
		printf(
		    "%*.*s(%08llx,%lld) ALL ALLOCATED\n",
		    tab, tab, "",
		    (long long)blk,
		    (long long)radix
		);
		return;
	}
	if (scan->u.bmu_avail == radix) {
		printf(
		    "%*.*s(%08llx,%lld) ALL FREE\n",
		    tab, tab, "",
		    (long long)blk,
		    (long long)radix
		);
		return;
	}

	printf(
	    "%*.*s(%08llx,%lld): subtree (%lld/%lld) big=%lld {\n",
	    tab, tab, "",
	    (long long)blk, (long long)radix,
	    (long long)scan->u.bmu_avail,
	    (long long)radix,
	    (long long)scan->bm_bighint
	);

	skip = radix_to_skip(radix);
	next_skip = skip / BLIST_META_RADIX;
	radix /= BLIST_META_RADIX;
	tab += 4;

	for (i = 1; i < skip; i += next_skip) {
		if (scan[i].bm_bighint == (daddr_t)-1) {
			printf(
			    "%*.*s(%08llx,%lld): Terminator\n",
			    tab, tab, "",
			    (long long)blk, (long long)radix
			);
			break;
		}
		blst_radix_print(&scan[i], blk, radix, tab);
		blk += radix;
	}
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
	int size = 1024;
	int i;
	blist_t bl;

	for (i = 1; i < ac; ++i) {
		const char *ptr = av[i];
		if (*ptr != '-') {
			size = strtol(ptr, NULL, 0);
			continue;
		}
		ptr += 2;
		fprintf(stderr, "Bad option: %s\n", ptr - 2);
		exit(1);
	}
	bl = blist_create(size, M_WAITOK);
	blist_free(bl, 0, size);

	for (;;) {
		char buf[1024];
		long long da = 0;
		long long count = 0;

		printf("%lld/%lld/%lld> ", (long long)blist_avail(bl),
		    (long long)size, (long long)bl->bl_radix);
		fflush(stdout);
		if (fgets(buf, sizeof(buf), stdin) == NULL)
			break;
		switch(buf[0]) {
		case 'r':
			if (sscanf(buf + 1, "%lld", &count) == 1) {
				blist_resize(&bl, count, 1, M_WAITOK);
			} else {
				printf("?\n");
			}
		case 'p':
			blist_print(bl);
			break;
		case 'a':
			if (sscanf(buf + 1, "%lld", &count) == 1) {
				daddr_t blk = blist_alloc(bl, count);
				printf("    R=%08llx\n", (long long)blk);
			} else {
				printf("?\n");
			}
			break;
		case 'f':
			if (sscanf(buf + 1, "%llx %lld", &da, &count) == 2) {
				blist_free(bl, da, count);
			} else {
				printf("?\n");
			}
			break;
		case 'l':
			if (sscanf(buf + 1, "%llx %lld", &da, &count) == 2) {
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
			    "a %d       -allocate\n"
			    "f %x %d    -free\n"
			    "l %x %d    -fill\n"
			    "r %d       -resize\n"
			    "h/?        -help"
			);
			break;
		default:
			printf("?\n");
			break;
		}
	}
	return(0);
}

void
panic(const char *ctl, ...)
{
	va_list va;

	va_start(va, ctl);
	vfprintf(stderr, ctl, va);
	fprintf(stderr, "\n");
	va_end(va);
	exit(1);
}

#endif
