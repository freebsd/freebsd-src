/*-
 * Copyright (c) 2020 Mellanox Technologies, Ltd.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice unmodified, this list of conditions, and the following
 *    disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <linux/xarray.h>

#include <vm/vm_pageout.h>

/*
 * This function removes the element at the given index and returns
 * the pointer to the removed element, if any.
 */
void *
__xa_erase(struct xarray *xa, uint32_t index)
{
	XA_ASSERT_LOCKED(xa);

	return (radix_tree_delete(&xa->root, index));
}

void *
xa_erase(struct xarray *xa, uint32_t index)
{
	void *retval;

	xa_lock(xa);
	retval = __xa_erase(xa, index);
	xa_unlock(xa);

	return (retval);
}

/*
 * This function returns the element pointer at the given index. A
 * value of NULL is returned if the element does not exist.
 */
void *
xa_load(struct xarray *xa, uint32_t index)
{
	void *retval;

	xa_lock(xa);
	retval = radix_tree_lookup(&xa->root, index);
	xa_unlock(xa);

	return (retval);
}

/*
 * This is an internal function used to sleep until more memory
 * becomes available.
 */
static void
xa_vm_wait_locked(struct xarray *xa)
{
	xa_unlock(xa);
	vm_wait(NULL);
	xa_lock(xa);
}

/*
 * This function iterates the xarray until it finds a free slot where
 * it can insert the element pointer to by "ptr". It starts at the
 * index pointed to by "pindex" and updates this value at return. The
 * "mask" argument defines the maximum index allowed, inclusivly, and
 * must be a power of two minus one value. The "gfp" argument
 * basically tells if we can wait for more memory to become available
 * or not. This function returns zero upon success or a negative error
 * code on failure. A typical error code is -ENOMEM which means either
 * the xarray is full, or there was not enough internal memory
 * available to complete the radix tree insertion.
 */
int
__xa_alloc(struct xarray *xa, uint32_t *pindex, void *ptr, uint32_t mask, gfp_t gfp)
{
	int retval;

	XA_ASSERT_LOCKED(xa);

	/* mask should allow to allocate at least one item */
	MPASS(mask > (xa->flags & XA_FLAGS_ALLOC1) != 0 ? 1 : 0);

	/* mask can be any power of two value minus one */
	MPASS((mask & (mask + 1)) == 0);

	*pindex = (xa->flags & XA_FLAGS_ALLOC1) != 0 ? 1 : 0;
retry:
	retval = radix_tree_insert(&xa->root, *pindex, ptr);

	switch (retval) {
	case -EEXIST:
		if (likely(*pindex != mask)) {
			(*pindex)++;
			goto retry;
		}
		retval = -ENOMEM;
		break;
	case -ENOMEM:
		if (likely(gfp & M_WAITOK)) {
			xa_vm_wait_locked(xa);
			goto retry;
		}
		break;
	default:
		break;
	}
	return (retval);
}

int
xa_alloc(struct xarray *xa, uint32_t *pindex, void *ptr, uint32_t mask, gfp_t gfp)
{
	int retval;

	xa_lock(xa);
	retval = __xa_alloc(xa, pindex, ptr, mask, gfp);
	xa_unlock(xa);

	return (retval);
}

/*
 * This function works the same like the "xa_alloc" function, except
 * it wraps the next index value to zero when there are no entries
 * left at the end of the xarray searching for a free slot from the
 * beginning of the array. If the xarray is full -ENOMEM is returned.
 */
int
__xa_alloc_cyclic(struct xarray *xa, uint32_t *pindex, void *ptr, uint32_t mask,
    uint32_t *pnext_index, gfp_t gfp)
{
	int retval;
	int timeout = 1;

	XA_ASSERT_LOCKED(xa);

	/* mask should allow to allocate at least one item */
	MPASS(mask > (xa->flags & XA_FLAGS_ALLOC1) != 0 ? 1 : 0);

	/* mask can be any power of two value minus one */
	MPASS((mask & (mask + 1)) == 0);

	*pnext_index = (xa->flags & XA_FLAGS_ALLOC1) != 0 ? 1 : 0;
retry:
	retval = radix_tree_insert(&xa->root, *pnext_index, ptr);

	switch (retval) {
	case -EEXIST:
		if (unlikely(*pnext_index == mask) && !timeout--) {
			retval = -ENOMEM;
			break;
		}
		(*pnext_index)++;
		(*pnext_index) &= mask;
		if (*pnext_index == 0 && (xa->flags & XA_FLAGS_ALLOC1) != 0)
			(*pnext_index)++;
		goto retry;
	case -ENOMEM:
		if (likely(gfp & M_WAITOK)) {
			xa_vm_wait_locked(xa);
			goto retry;
		}
		break;
	default:
		break;
	}
	*pindex = *pnext_index;

	return (retval);
}

int
xa_alloc_cyclic(struct xarray *xa, uint32_t *pindex, void *ptr, uint32_t mask,
    uint32_t *pnext_index, gfp_t gfp)
{
	int retval;

	xa_lock(xa);
	retval = __xa_alloc_cyclic(xa, pindex, ptr, mask, pnext_index, gfp);
	xa_unlock(xa);

	return (retval);
}

/*
 * This function tries to insert an element at the given index. The
 * "gfp" argument basically decides of this function can sleep or not
 * trying to allocate internal memory for its radix tree.  The
 * function returns an error code upon failure. Typical error codes
 * are element exists (-EEXIST) or out of memory (-ENOMEM).
 */
int
__xa_insert(struct xarray *xa, uint32_t index, void *ptr, gfp_t gfp)
{
	int retval;

	XA_ASSERT_LOCKED(xa);
retry:
	retval = radix_tree_insert(&xa->root, index, ptr);

	switch (retval) {
	case -ENOMEM:
		if (likely(gfp & M_WAITOK)) {
			xa_vm_wait_locked(xa);
			goto retry;
		}
		break;
	default:
		break;
	}
	return (retval);
}

int
xa_insert(struct xarray *xa, uint32_t index, void *ptr, gfp_t gfp)
{
	int retval;

	xa_lock(xa);
	retval = __xa_insert(xa, index, ptr, gfp);
	xa_unlock(xa);

	return (retval);
}

/*
 * This function updates the element at the given index and returns a
 * pointer to the old element. The "gfp" argument basically decides of
 * this function can sleep or not trying to allocate internal memory
 * for its radix tree. The function returns an XA_ERROR() pointer code
 * upon failure. Code using this function must always check if the
 * return value is an XA_ERROR() code before using the returned value.
 */
void *
__xa_store(struct xarray *xa, uint32_t index, void *ptr, gfp_t gfp)
{
	int retval;

	XA_ASSERT_LOCKED(xa);
retry:
	retval = radix_tree_store(&xa->root, index, &ptr);

	switch (retval) {
	case 0:
		break;
	case -ENOMEM:
		if (likely(gfp & M_WAITOK)) {
			xa_vm_wait_locked(xa);
			goto retry;
		}
		ptr = XA_ERROR(retval);
		break;
	default:
		ptr = XA_ERROR(retval);
		break;
	}
	return (ptr);
}

void *
xa_store(struct xarray *xa, uint32_t index, void *ptr, gfp_t gfp)
{
	void *retval;

	xa_lock(xa);
	retval = __xa_store(xa, index, ptr, gfp);
	xa_unlock(xa);

	return (retval);
}

/*
 * This function initialize an xarray structure.
 */
void
xa_init_flags(struct xarray *xa, uint32_t flags)
{
	memset(xa, 0, sizeof(*xa));

	mtx_init(&xa->mtx, "lkpi-xarray", NULL, MTX_DEF | MTX_RECURSE);
	xa->root.gfp_mask = GFP_NOWAIT;
	xa->flags = flags;
}

/*
 * This function destroys an xarray structure and all its internal
 * memory and locks.
 */
void
xa_destroy(struct xarray *xa)
{
	struct radix_tree_iter iter;
	void **ppslot;

	radix_tree_for_each_slot(ppslot, &xa->root, &iter, 0)
		radix_tree_iter_delete(&xa->root, &iter, ppslot);
	mtx_destroy(&xa->mtx);
}

/*
 * This function checks if an xarray is empty or not.
 * It returns true if empty, else false.
 */
bool
__xa_empty(struct xarray *xa)
{
	struct radix_tree_iter iter = {};
	void **temp;

	XA_ASSERT_LOCKED(xa);

	return (!radix_tree_iter_find(&xa->root, &iter, &temp));
}

bool
xa_empty(struct xarray *xa)
{
	bool retval;

	xa_lock(xa);
	retval = __xa_empty(xa);
	xa_unlock(xa);

	return (retval);
}

/*
 * This function returns the next valid xarray entry based on the
 * index given by "pindex". The valued pointed to by "pindex" is
 * updated before return.
 */
void *
__xa_next(struct xarray *xa, unsigned long *pindex, bool not_first)
{
	struct radix_tree_iter iter = { .index = *pindex };
	void **ppslot;
	void *retval;
	bool found;

	XA_ASSERT_LOCKED(xa);

	if (not_first) {
		/* advance to next index, if any */
		iter.index++;
		if (iter.index == 0)
			return (NULL);
	}

	found = radix_tree_iter_find(&xa->root, &iter, &ppslot);
	if (likely(found)) {
		retval = *ppslot;
		*pindex = iter.index;
	} else {
		retval = NULL;
	}
	return (retval);
}

void *
xa_next(struct xarray *xa, unsigned long *pindex, bool not_first)
{
	void *retval;

	xa_lock(xa);
	retval = __xa_next(xa, pindex, not_first);
	xa_unlock(xa);

	return (retval);
}
