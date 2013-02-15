/*
 * Copyright (c) 2013 EMC Corp.
 * Copyright (c) 2011 Jeffrey Roberson <jeff@freebsd.org>
 * Copyright (c) 2008 Mayur Shardul <mayur.shardul@gmail.com>
 * All rights reserved.
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
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

/*
 * Path-compressed radix trie implementation.
 * The following code is not generalized into a general purpose library
 * because there are way too many parameters embedded that should really
 * be decided by the library consumers.  At the same time, consumers
 * of this code must achieve highest possible performance.
 *
 * The implementation takes into account the following rationale:
 * - Size of the nodes might be as small as possible.
 * - There is no bias toward lookup operations over inserts or removes,
 *   and vice-versa.
 * - In average there are not many complete levels, than level
 *   compression may just complicate things.
 */

#include <sys/cdefs.h>

#include "opt_ddb.h"
#include "opt_vm.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>

#include <vm/uma.h>
#include <vm/vm.h>
#include <vm/vm_param.h>
#include <vm/vm_extern.h>
#include <vm/vm_page.h>
#include <vm/vm_radix.h>

#ifdef DDB
#include <ddb/ddb.h>
#endif

#ifndef VM_RADIX_BOOT_CACHE
#define	VM_RADIX_BOOT_CACHE	1500
#endif

/*
 * Such sizes should permit to keep node children contained into a single
 * cache-line, or to at least not span many of those.
 * In particular, sparse tries should however be compressed properly and
 * then make some extra-levels not a big deal.
 */
#ifdef __LP64__
#define	VM_RADIX_WIDTH	4
#else
#define	VM_RADIX_WIDTH	3
#endif

#define	VM_RADIX_COUNT	(1 << VM_RADIX_WIDTH)
#define	VM_RADIX_MASK	(VM_RADIX_COUNT - 1)
#define	VM_RADIX_LIMIT							\
	(howmany((sizeof(vm_pindex_t) * NBBY), VM_RADIX_WIDTH) - 1)

/* Flag bits stored in node pointers. */
#define	VM_RADIX_ISLEAF	0x1
#define	VM_RADIX_FLAGS	0x1
#define	VM_RADIX_PAD	VM_RADIX_FLAGS

/* Returns one unit associated with specified level. */
#define	VM_RADIX_UNITLEVEL(lev)						\
	((vm_pindex_t)1 << ((VM_RADIX_LIMIT - (lev)) * VM_RADIX_WIDTH))

struct vm_radix_node {
	void		*rn_child[VM_RADIX_COUNT];	/* Child nodes. */
	vm_pindex_t	 rn_owner;			/* Owner of record. */
	uint16_t	 rn_count;			/* Valid children. */
	uint16_t	 rn_clev;			/* Current level. */
};

static uma_zone_t vm_radix_node_zone;

/*
 * Boot-time cache of struct vm_radix_node objects.
 * This cache is used to cater page allocations before the UMA zone is
 * actually setup and pre-allocated (ie. pmap_init()).
 */
static u_int boot_cache_cnt;
static struct vm_radix_node boot_cache[VM_RADIX_BOOT_CACHE];

static struct vm_radix_node *
vm_radix_carve_bootcache(void)
{
	struct vm_radix_node *rnode;

	if (boot_cache_cnt == VM_RADIX_BOOT_CACHE)
		panic("%s: Increase VM_RADIX_BOOT_CACHE (%u)", __func__,
		    VM_RADIX_BOOT_CACHE);
	rnode = &boot_cache[boot_cache_cnt];
	boot_cache_cnt++;
	return (rnode);
}

/*
 * Allocate a radix node.  Pre-allocation ensures that the request will be
 * always successfully satisfied.
 */
static __inline struct vm_radix_node *
vm_radix_node_get(vm_pindex_t owner, uint16_t count, uint16_t clevel)
{
	struct vm_radix_node *rnode;

	if (__predict_false(boot_cache_cnt <= VM_RADIX_BOOT_CACHE))
		rnode = vm_radix_carve_bootcache();
	else {
		rnode = uma_zalloc(vm_radix_node_zone, M_NOWAIT | M_ZERO);

		/*
		 * The required number of nodes might be already correctly
		 * pre-allocated in vm_radix_init().  However, UMA can reserve
		 * few nodes on per-cpu specific buckets, which will not be
		 * accessible from the curcpu.  The allocation could then
		 * return NULL when the pre-allocation pool is close to be
		 * exhausted.  Anyway, in practice this should never be a
		 * problem because a new node is not always required for
		 * insert, thus the pre-allocation pool should already have
		 * some extra-pages that indirectly deal with this situation.
		 */
		if (rnode == NULL)
			panic("%s: uma_zalloc() returned NULL for a new node",
			    __func__);
	}
	rnode->rn_owner = owner;
	rnode->rn_count = count;
	rnode->rn_clev = clevel;
	return (rnode);
}

/*
 * Free radix node.
 */
static __inline void
vm_radix_node_put(struct vm_radix_node *rnode)
{

	if (__predict_false(rnode > boot_cache &&
	    rnode <= &boot_cache[VM_RADIX_BOOT_CACHE]))
		return;
	uma_zfree(vm_radix_node_zone, rnode);
}

/*
 * Return the position in the array for a given level.
 */
static __inline int
vm_radix_slot(vm_pindex_t index, uint16_t level)
{

	return ((index >> ((VM_RADIX_LIMIT - level) * VM_RADIX_WIDTH)) &
	    VM_RADIX_MASK);
}

/* Trims the key after the specified level. */
static __inline vm_pindex_t
vm_radix_trimkey(vm_pindex_t index, uint16_t level)
{
	vm_pindex_t ret;

	ret = index;
	if (level < VM_RADIX_LIMIT) {
		ret >>= (VM_RADIX_LIMIT - level) * VM_RADIX_WIDTH;
		ret <<= (VM_RADIX_LIMIT - level) * VM_RADIX_WIDTH;
	}
	return (ret);
}

/*
 * Get the root node for a radix tree.
 */
static __inline struct vm_radix_node *
vm_radix_getroot(struct vm_radix *rtree)
{

	return ((struct vm_radix_node *)(rtree->rt_root & ~VM_RADIX_FLAGS));
}

/*
 * Set the root node for a radix tree.
 */
static __inline void
vm_radix_setroot(struct vm_radix *rtree, struct vm_radix_node *rnode)
{

	rtree->rt_root = (uintptr_t)rnode;
}

/*
 * Returns the associated page extracted from rnode if available,
 * NULL otherwise.
 */
static __inline vm_page_t
vm_radix_node_page(struct vm_radix_node *rnode)
{

	return ((((uintptr_t)rnode & VM_RADIX_ISLEAF) != 0) ?
	    (vm_page_t)((uintptr_t)rnode & ~VM_RADIX_FLAGS) : NULL);
}

/*
 * Adds the page as a child of provided node.
 */
static __inline void
vm_radix_addpage(struct vm_radix_node *rnode, vm_pindex_t index, uint16_t clev,
    vm_page_t page)
{
	int slot;

	slot = vm_radix_slot(index, clev);
	rnode->rn_child[slot] = (void *)((uintptr_t)page | VM_RADIX_ISLEAF);
}

/*
 * Returns the slot where two keys differ.
 * It cannot accept 2 equal keys.
 */
static __inline uint16_t
vm_radix_keydiff(vm_pindex_t index1, vm_pindex_t index2)
{
	uint16_t clev;

	KASSERT(index1 != index2, ("%s: passing the same key value %jx",
	    __func__, (uintmax_t)index1));

	index1 ^= index2;
	for (clev = 0; clev <= VM_RADIX_LIMIT ; clev++)
		if (vm_radix_slot(index1, clev))
			return (clev);
	panic("%s: it might have not reached this point", __func__);
	return (0);
}

/*
 * Returns TRUE if it can be determined that key does not belong to the
 * specified rnode. FALSE otherwise.
 */
static __inline boolean_t
vm_radix_keybarr(struct vm_radix_node *rnode, vm_pindex_t idx)
{

	if (rnode->rn_clev > 0) {
		idx = vm_radix_trimkey(idx, rnode->rn_clev - 1);
		idx -= rnode->rn_owner;
		if (idx != 0)
			return (TRUE);
	}
	return (FALSE);
}

/*
 * Adjusts the idx key to the first upper level available, based on a valid
 * initial level and map of available levels.
 * Returns a value bigger than 0 to signal that there are not valid levels
 * available.
 */
static __inline int
vm_radix_addlev(vm_pindex_t *idx, boolean_t *levels, uint16_t ilev)
{
	vm_pindex_t wrapidx;

	for (; levels[ilev] == FALSE ||
	    vm_radix_slot(*idx, ilev) == (VM_RADIX_COUNT - 1); ilev--)
		if (ilev == 0)
			break;
	KASSERT(ilev > 0 || levels[0] == TRUE,
	    ("%s: levels back-scanning problem", __func__));
	if (ilev == 0 && vm_radix_slot(*idx, ilev) == (VM_RADIX_COUNT - 1))
		return (1);
	wrapidx = *idx;
	*idx = vm_radix_trimkey(*idx, ilev);
	*idx += VM_RADIX_UNITLEVEL(ilev);
	if (*idx < wrapidx)
		return (1);
	return (0);
}

/*
 * Adjusts the idx key to the first lower level available, based on a valid
 * initial level and map of available levels.
 * Returns a value bigger than 0 to signal that there are not valid levels
 * available.
 */
static __inline int
vm_radix_declev(vm_pindex_t *idx, boolean_t *levels, uint16_t ilev)
{
	vm_pindex_t wrapidx;

	for (; levels[ilev] == FALSE ||
	    vm_radix_slot(*idx, ilev) == 0; ilev--)
		if (ilev == 0)
			break;
	KASSERT(ilev > 0 || levels[0] == TRUE,
	    ("%s: levels back-scanning problem", __func__));
	if (ilev == 0 && vm_radix_slot(*idx, ilev) == 0)
		return (1);
	wrapidx = *idx;
	*idx = vm_radix_trimkey(*idx, ilev);
	*idx |= VM_RADIX_UNITLEVEL(ilev) - 1;
	*idx -= VM_RADIX_UNITLEVEL(ilev);
	if (*idx < wrapidx)
		return (1);
	return (0);
}

/*
 * Internal handwork for vm_radix_reclaim_allonodes() primitive.
 * This function is recrusive.
 */
static void
vm_radix_reclaim_allnodes_int(struct vm_radix_node *rnode)
{
	int slot;

	for (slot = 0; slot < VM_RADIX_COUNT && rnode->rn_count != 0; slot++) {
		if (rnode->rn_child[slot] == NULL)
			continue;
		if (vm_radix_node_page(rnode->rn_child[slot]) == NULL)
			vm_radix_reclaim_allnodes_int(rnode->rn_child[slot]);
		rnode->rn_count--;
	}
	vm_radix_node_put(rnode);
}

#ifdef INVARIANTS
/*
 * Radix node zone destructor.
 */
static void
vm_radix_node_zone_dtor(void *mem, int size __unused, void *arg __unused)
{
	struct vm_radix_node *rnode;

	rnode = mem;
	KASSERT(rnode->rn_count == 0,
	    ("vm_radix_node_put: Freeing node %p with %d children\n", mem,
	    rnode->rn_count));
}
#endif

/*
 * Pre-allocate intermediate nodes from the UMA slab zone.
 */
static void
vm_radix_init(void *arg __unused)
{
	int nitems;

	vm_radix_node_zone = uma_zcreate("RADIX NODE",
	    sizeof(struct vm_radix_node), NULL,
#ifdef INVARIANTS
	    vm_radix_node_zone_dtor,
#else
	    NULL,
#endif
	    NULL, NULL, VM_RADIX_PAD, UMA_ZONE_VM | UMA_ZONE_NOFREE);
	nitems = uma_zone_set_max(vm_radix_node_zone, vm_page_array_size);
	uma_prealloc(vm_radix_node_zone, nitems);
	boot_cache_cnt = VM_RADIX_BOOT_CACHE + 1;
}
SYSINIT(vm_radix_init, SI_SUB_KMEM, SI_ORDER_SECOND, vm_radix_init, NULL);

/*
 * Inserts the key-value pair in to the trie.
 * Panics if the key already exists.
 */
void
vm_radix_insert(struct vm_radix *rtree, vm_pindex_t index, vm_page_t page)
{
	vm_pindex_t newind;
	struct vm_radix_node *rnode, *tmp, *tmp2;
	vm_page_t m;
	int slot;
	uint16_t clev;

	/*
	 * The owner of record for root is not really important because it
	 * will never be used.
	 */
	rnode = vm_radix_getroot(rtree);
	if (rnode == NULL) {
		rnode = vm_radix_node_get(0, 1, 0);
		vm_radix_setroot(rtree, rnode);
		vm_radix_addpage(rnode, index, 0, page);
		return;
	}
	while (rnode != NULL) {
		if (vm_radix_keybarr(rnode, index) == TRUE)
			break;
		slot = vm_radix_slot(index, rnode->rn_clev);
		m = vm_radix_node_page(rnode->rn_child[slot]);
		if (m != NULL) {
			if (m->pindex == index)
				panic("%s: key %jx is already present",
				    __func__, (uintmax_t)index);
			clev = vm_radix_keydiff(m->pindex, index);
			tmp = vm_radix_node_get(vm_radix_trimkey(index,
			    clev - 1), 2, clev);
			rnode->rn_child[slot] = tmp;
			vm_radix_addpage(tmp, index, clev, page);
			vm_radix_addpage(tmp, m->pindex, clev, m);
			return;
		}
		if (rnode->rn_child[slot] == NULL) {
			rnode->rn_count++;
			vm_radix_addpage(rnode, index, rnode->rn_clev, page);
			return;
		}
		rnode = rnode->rn_child[slot];
	}
	if (rnode == NULL)
		panic("%s: path traversal ended unexpectedly", __func__);

	/*
	 * Scan the trie from the top and find the parent to insert
	 * the new object.
	 */
	newind = rnode->rn_owner;
	clev = vm_radix_keydiff(newind, index);
	slot = VM_RADIX_COUNT;
	for (rnode = vm_radix_getroot(rtree); ; rnode = tmp) {
		KASSERT(rnode != NULL, ("%s: edge cannot be NULL in the scan",
		    __func__));
		KASSERT(clev >= rnode->rn_clev,
		    ("%s: unexpected trie depth: clev: %d, rnode->rn_clev: %d",
		    __func__, clev, rnode->rn_clev));
		slot = vm_radix_slot(index, rnode->rn_clev);
		tmp = rnode->rn_child[slot];
		KASSERT(tmp != NULL && vm_radix_node_page(tmp) == NULL,
		    ("%s: unexpected lookup interruption", __func__));
		if (tmp->rn_clev > clev)
			break;
	}
	KASSERT(rnode != NULL && tmp != NULL && slot < VM_RADIX_COUNT,
	    ("%s: invalid scan parameters rnode: %p, tmp: %p, slot: %d",
	    __func__, (void *)rnode, (void *)tmp, slot));

	/*
	 * A new node is needed because the right insertion level is reached.
	 * Setup the new intermediate node and add the 2 children: the
	 * new object and the older edge.
	 */
	tmp2 = vm_radix_node_get(vm_radix_trimkey(page->pindex, clev - 1), 2,
	    clev);
	rnode->rn_child[slot] = tmp2;
	vm_radix_addpage(tmp2, index, clev, page);
	slot = vm_radix_slot(newind, clev);
	tmp2->rn_child[slot] = tmp;
}

/*
 * Returns the value stored at the index.  If the index is not present
 * NULL is returned.
 */
vm_page_t
vm_radix_lookup(struct vm_radix *rtree, vm_pindex_t index)
{
	struct vm_radix_node *rnode;
	vm_page_t m;
	int slot;

	rnode = vm_radix_getroot(rtree);
	while (rnode != NULL) {
		if (vm_radix_keybarr(rnode, index) == TRUE)
			return (NULL);
		slot = vm_radix_slot(index, rnode->rn_clev);
		rnode = rnode->rn_child[slot];
		m = vm_radix_node_page(rnode);
		if (m != NULL) {
			if (m->pindex == index)
				return (m);
			else
				return (NULL);
		}
	}
	return (NULL);
}

/*
 * Look up any entry at a position bigger than or equal to index.
 */
vm_page_t
vm_radix_lookup_ge(struct vm_radix *rtree, vm_pindex_t index)
{
	vm_pindex_t inc;
	vm_page_t m;
	struct vm_radix_node *rnode;
	int slot;
	uint16_t difflev;
	boolean_t maplevels[VM_RADIX_LIMIT + 1];
#ifdef INVARIANTS
	int loops = 0;
#endif

restart:
	KASSERT(++loops < 1000, ("%s: too many loops", __func__));
	for (difflev = 0; difflev < (VM_RADIX_LIMIT + 1); difflev++)
		maplevels[difflev] = FALSE;
	rnode = vm_radix_getroot(rtree);
	while (rnode != NULL) {
		maplevels[rnode->rn_clev] = TRUE;

		/*
		 * If the keys differ before the current bisection node
		 * the search key might rollback to the earlierst
		 * available bisection node, or to the smaller value
		 * in the current domain (if the owner is bigger than the
		 * search key).
		 * The search for a valid bisection node is helped through
		 * the use of maplevels array which should bring immediately
		 * a lower useful level, skipping holes.
		 */
		if (vm_radix_keybarr(rnode, index) == TRUE) {
			difflev = vm_radix_keydiff(index, rnode->rn_owner);
			if (index > rnode->rn_owner) {
				if (vm_radix_addlev(&index, maplevels,
				    difflev) > 0)
					break;
			} else
				index = vm_radix_trimkey(rnode->rn_owner,
				    difflev);
			goto restart;
		}
		slot = vm_radix_slot(index, rnode->rn_clev);
		m = vm_radix_node_page(rnode->rn_child[slot]);
		if (m != NULL && m->pindex >= index)
			return (m);
		if (rnode->rn_child[slot] != NULL && m == NULL) {
			rnode = rnode->rn_child[slot];
			continue;
		}

		/*
		 * Look for an available edge or page within the current
		 * bisection node.
		 */
                if (slot < (VM_RADIX_COUNT - 1)) {
			inc = VM_RADIX_UNITLEVEL(rnode->rn_clev);
			index = vm_radix_trimkey(index, rnode->rn_clev);
			index += inc;
			slot++;
			for (;; index += inc, slot++) {
				m = vm_radix_node_page(rnode->rn_child[slot]);
				if (m != NULL && m->pindex >= index)
					return (m);
				if ((rnode->rn_child[slot] != NULL &&
				    m == NULL) || slot == (VM_RADIX_COUNT - 1))
					break;
			}
		}

		/*
		 * If a valid page or edge, bigger than the search slot, is
		 * found in the traversal, skip to the next higher-level key.
		 */
		if (slot == (VM_RADIX_COUNT - 1) &&
		    (rnode->rn_child[slot] == NULL || m != NULL)) {
			if (rnode->rn_clev == 0  || vm_radix_addlev(&index,
			    maplevels, rnode->rn_clev - 1) > 0)
				break;
			goto restart;
		}
		rnode = rnode->rn_child[slot];
	}
	return (NULL);
}

/*
 * Look up any entry at a position less than or equal to index.
 */
vm_page_t
vm_radix_lookup_le(struct vm_radix *rtree, vm_pindex_t index)
{
	vm_pindex_t inc;
	vm_page_t m;
	struct vm_radix_node *rnode;
	int slot;
	uint16_t difflev;
	boolean_t maplevels[VM_RADIX_LIMIT + 1];
#ifdef INVARIANTS
	int loops = 0;
#endif

restart:
	KASSERT(++loops < 1000, ("%s: too many loops", __func__));
	for (difflev = 0; difflev < (VM_RADIX_LIMIT + 1); difflev++)
		maplevels[difflev] = FALSE;
	rnode = vm_radix_getroot(rtree);
	while (rnode != NULL) {
		maplevels[rnode->rn_clev] = TRUE;

		/*
		 * If the keys differ before the current bisection node
		 * the search key might rollback to the earlierst
		 * available bisection node, or to the higher value
		 * in the current domain (if the owner is smaller than the
		 * search key).
		 * The search for a valid bisection node is helped through
		 * the use of maplevels array which should bring immediately
		 * a lower useful level, skipping holes.
		 */
		if (vm_radix_keybarr(rnode, index) == TRUE) {
			difflev = vm_radix_keydiff(index, rnode->rn_owner);
			if (index > rnode->rn_owner) {
				index = vm_radix_trimkey(rnode->rn_owner,
				    difflev);
				index |= VM_RADIX_UNITLEVEL(difflev) - 1;
			} else if (vm_radix_declev(&index, maplevels,
			    difflev) > 0)
				break;
			goto restart;
		}
		slot = vm_radix_slot(index, rnode->rn_clev);
		m = vm_radix_node_page(rnode->rn_child[slot]);
		if (m != NULL && m->pindex <= index)
			return (m);
		if (rnode->rn_child[slot] != NULL && m == NULL) {
			rnode = rnode->rn_child[slot];
			continue;
		}

		/*
		 * Look for an available edge or page within the current
		 * bisection node.
		 */
		if (slot > 0) {
			inc = VM_RADIX_UNITLEVEL(rnode->rn_clev);
			index = vm_radix_trimkey(index, rnode->rn_clev);
			index |= inc - 1;
			index -= inc;
			slot--;
			for (;; index -= inc, slot--) {
				m = vm_radix_node_page(rnode->rn_child[slot]);
				if (m != NULL && m->pindex <= index)
					return (m);
				if ((rnode->rn_child[slot] != NULL &&
				    m == NULL) || slot == 0)
					break;
			}
		}

		/*
		 * If a valid page or edge, smaller than the search slot, is
		 * found in the traversal, skip to the next higher-level key.
		 */
		if (slot == 0 && (rnode->rn_child[slot] == NULL || m != NULL)) {
			if (rnode->rn_clev == 0 || vm_radix_declev(&index,
			    maplevels, rnode->rn_clev - 1) > 0)
				break;
			goto restart;
		}
		rnode = rnode->rn_child[slot];
	}
	return (NULL);
}

/*
 * Remove the specified index from the tree.
 * Panics if the key is not present.
 */
void
vm_radix_remove(struct vm_radix *rtree, vm_pindex_t index)
{
	struct vm_radix_node *rnode, *parent;
	vm_page_t m;
	int i, slot;

	parent = NULL;
	rnode = vm_radix_getroot(rtree);
	for (;;) {
		if (rnode == NULL)
			panic("vm_radix_remove: impossible to locate the key");
		slot = vm_radix_slot(index, rnode->rn_clev);
		m = vm_radix_node_page(rnode->rn_child[slot]);
		if (m != NULL && m->pindex == index) {
			rnode->rn_child[slot] = NULL;
			rnode->rn_count--;
			if (rnode->rn_count > 1)
				break;
			if (parent == NULL) {
				if (rnode->rn_count == 0) {
					vm_radix_node_put(rnode);
					vm_radix_setroot(rtree, NULL);
				}
				break;
			}
			for (i = 0; i < VM_RADIX_COUNT; i++)
				if (rnode->rn_child[i] != NULL)
					break;
			KASSERT(i != VM_RADIX_COUNT,
			    ("%s: invalid node configuration", __func__));
			slot = vm_radix_slot(index, parent->rn_clev);
			KASSERT(parent->rn_child[slot] == rnode,
			    ("%s: invalid child value", __func__));
			parent->rn_child[slot] = rnode->rn_child[i];
			rnode->rn_count--;
			rnode->rn_child[i] = NULL;
			vm_radix_node_put(rnode);
			break;
		}
		if (m != NULL && m->pindex != index)
			panic("%s: invalid key found", __func__);
		parent = rnode;
		rnode = rnode->rn_child[slot];
	}
}

/*
 * Remove and free all the nodes from the radix tree.
 * This function is recrusive but there is a tight control on it as the
 * maximum depth of the tree is fixed.
 */
void
vm_radix_reclaim_allnodes(struct vm_radix *rtree)
{
	struct vm_radix_node *root;

	root = vm_radix_getroot(rtree);
	if (root == NULL)
		return;
	vm_radix_reclaim_allnodes_int(root);
	vm_radix_setroot(rtree, NULL);
}

#ifdef DDB
/*
 * Show details about the given radix node.
 */
DB_SHOW_COMMAND(radixnode, db_show_radixnode)
{
	struct vm_radix_node *rnode;
	int i;

        if (!have_addr)
                return;
	rnode = (struct vm_radix_node *)addr;
	db_printf("radixnode %p, owner %jx, children count %u, level %u:\n",
	    (void *)rnode, (uintmax_t)rnode->rn_owner, rnode->rn_count,
	    rnode->rn_clev);
	for (i = 0; i < VM_RADIX_COUNT; i++)
		if (rnode->rn_child[i] != NULL)
			db_printf("slot: %d, val: %p, page: %p, clev: %d\n",
			    i, (void *)rnode->rn_child[i],
			    (void *)vm_radix_node_page(rnode->rn_child[i]),
			    rnode->rn_clev);
}
#endif /* DDB */
