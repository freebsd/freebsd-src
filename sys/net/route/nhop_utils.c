/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2020 Alexander V. Chernikov
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
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");
#include "opt_inet.h"
#include "opt_route.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/kernel.h>

#include <net/route/nhop_utils.h>

#define	BLOCK_ITEMS	(8 * sizeof(u_long))	/* Number of items for ffsl() */

#define	_BLOCKS_TO_SZ(_blocks)		((size_t)(_blocks) * sizeof(u_long))
#define	_BLOCKS_TO_ITEMS(_blocks)	((uint32_t)(_blocks) * BLOCK_ITEMS)
#define	_ITEMS_TO_BLOCKS(_items)	((_items) / BLOCK_ITEMS)

static void _bitmask_init_idx(void *index, uint32_t items);

void
bitmask_init(struct bitmask_head *bh, void *idx, uint32_t num_items)
{

	if (idx != NULL)
		_bitmask_init_idx(idx, num_items);

	memset(bh, 0, sizeof(struct bitmask_head));
	bh->blocks = _ITEMS_TO_BLOCKS(num_items);
	bh->idx = (u_long *)idx;
}

uint32_t
bitmask_get_resize_items(const struct bitmask_head *bh)
{
	if ((bh->items_count * 2 > _BLOCKS_TO_ITEMS(bh->blocks)) && bh->items_count < 65536)
		return (_BLOCKS_TO_ITEMS(bh->blocks) * 2);

	return (0);
}

int
bitmask_should_resize(const struct bitmask_head *bh)
{

	return (bitmask_get_resize_items(bh) != 0);
}

#if 0
uint32_t
_bitmask_get_blocks(uint32_t items)
{

	return (items / BLOCK_ITEMS);
}
#endif

size_t
bitmask_get_size(uint32_t items)
{
#if _KERNEL
	KASSERT((items % BLOCK_ITEMS) == 0,
	   ("bitmask size needs to power of 2 and greater or equal to %zu",
	    BLOCK_ITEMS));
#else
	assert((items % BLOCK_ITEMS) == 0);
#endif

	return (items / 8);
}

static void
_bitmask_init_idx(void *_idx, uint32_t items)
{
	size_t size = bitmask_get_size(items);
	u_long *idx = (u_long *)_idx;

	/* Mark all as free */
	memset(idx, 0xFF, size);
	*idx &= ~(u_long)1; /* Always skip index 0 */
}

/*
 * _try_merge api to allow shrinking?
 */
int
bitmask_copy(const struct bitmask_head *bi, void *new_idx, uint32_t new_items)
{
	uint32_t new_blocks = _BLOCKS_TO_ITEMS(new_items);

	_bitmask_init_idx(new_idx, new_items);

	if (bi->blocks < new_blocks) {
		/* extend current blocks */
		if (bi->blocks > 0)
			memcpy(new_idx, bi->idx, _BLOCKS_TO_SZ(bi->blocks));
		return (0);
	} else {
		/* XXX: ensure all other blocks are non-zero */
		for (int i = new_blocks; i < bi->blocks; i++) {
		}

		return (1);
	}
}

void
bitmask_swap(struct bitmask_head *bh, void *new_idx, uint32_t new_items, void **pidx)
{
	void *old_ptr;

	old_ptr = bh->idx;

	bh->idx = (u_long *)new_idx;
	bh->blocks = _ITEMS_TO_BLOCKS(new_items);

	if (pidx != NULL)
		*pidx = old_ptr;
}

/*
 * Allocate new index in given instance and stores in in @pidx.
 * Returns 0 on success.
 */
int
bitmask_alloc_idx(struct bitmask_head *bi, uint16_t *pidx)
{
	u_long *mask;
	int i, off, v;

	off = bi->free_off;
	mask = &bi->idx[off];

	for (i = off; i < bi->blocks; i++, mask++) {
		if ((v = ffsl(*mask)) == 0)
			continue;

		/* Mark as busy */
		*mask &= ~ ((u_long)1 << (v - 1));

		bi->free_off = i;

		v = BLOCK_ITEMS * i + v - 1;

		*pidx = v;
		bi->items_count++;
		return (0);
	}

	return (1);
}

/*
 * Removes index from given set.
 * Returns 0 on success.
 */
int
bitmask_free_idx(struct bitmask_head *bi, uint16_t idx)
{
	u_long *mask;
	int i, v;

	if (idx == 0)
		return (1);

	i = idx / BLOCK_ITEMS;
	v = idx % BLOCK_ITEMS;

	if (i >= bi->blocks)
		return (1);

	mask = &bi->idx[i];

	if ((*mask & ((u_long)1 << v)) != 0)
		return (1);

	/* Mark as free */
	*mask |= (u_long)1 << v;
	bi->items_count--;

	/* Update free offset */
	if (bi->free_off > i)
		bi->free_off = i;

	return (0);
}
