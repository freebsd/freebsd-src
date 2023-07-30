/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
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
 * - Size of the nodes should be as small as possible but still big enough
 *   to avoid a large maximum depth for the trie.  This is a balance
 *   between the necessity to not wire too much physical memory for the nodes
 *   and the necessity to avoid too much cache pollution during the trie
 *   operations.
 * - There is not a huge bias toward the number of lookup operations over
 *   the number of insert and remove operations.  This basically implies
 *   that optimizations supposedly helping one operation but hurting the
 *   other might be carefully evaluated.
 * - On average not many nodes are expected to be fully populated, hence
 *   level compression may just complicate things.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "opt_ddb.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/libkern.h>
#include <sys/proc.h>
#include <sys/vmmeter.h>
#include <sys/smr.h>
#include <sys/smr_types.h>

#include <vm/uma.h>
#include <vm/vm.h>
#include <vm/vm_param.h>
#include <vm/vm_object.h>
#include <vm/vm_page.h>
#include <vm/vm_radix.h>

#ifdef DDB
#include <ddb/ddb.h>
#endif

/*
 * These widths should allow the pointers to a node's children to fit within
 * a single cache line.  The extra levels from a narrow width should not be
 * a problem thanks to path compression.
 */
#ifdef __LP64__
#define	VM_RADIX_WIDTH	4
#else
#define	VM_RADIX_WIDTH	3
#endif

#define	VM_RADIX_COUNT	(1 << VM_RADIX_WIDTH)
#define	VM_RADIX_MASK	(VM_RADIX_COUNT - 1)
#define	VM_RADIX_LIMIT							\
	(howmany(sizeof(vm_pindex_t) * NBBY, VM_RADIX_WIDTH) - 1)

#if VM_RADIX_WIDTH == 3
typedef uint8_t rn_popmap_t;
#elif VM_RADIX_WIDTH == 4
typedef uint16_t rn_popmap_t;
#elif VM_RADIX_WIDTH == 5
typedef uint32_t rn_popmap_t;
#else
#error Unsupported width
#endif
_Static_assert(sizeof(rn_popmap_t) <= sizeof(int),
    "rn_popmap_t too wide");

/* Set of all flag bits stored in node pointers. */
#define	VM_RADIX_FLAGS	(VM_RADIX_ISLEAF)
#define	VM_RADIX_PAD	VM_RADIX_FLAGS

enum vm_radix_access { SMR, LOCKED, UNSERIALIZED };

struct vm_radix_node;
typedef SMR_POINTER(struct vm_radix_node *) smrnode_t;

struct vm_radix_node {
	vm_pindex_t	rn_owner;			/* Owner of record. */
	rn_popmap_t	rn_popmap;			/* Valid children. */
	uint8_t		rn_clev;			/* Level * WIDTH. */
	smrnode_t	rn_child[VM_RADIX_COUNT];	/* Child nodes. */
};

static uma_zone_t vm_radix_node_zone;
static smr_t vm_radix_smr;

static void vm_radix_node_store(smrnode_t *p, struct vm_radix_node *v,
    enum vm_radix_access access);

/*
 * Map index to an array position for the children of rnode,
 */
static __inline int
vm_radix_slot(struct vm_radix_node *rnode, vm_pindex_t index)
{
	return ((index >> rnode->rn_clev) & VM_RADIX_MASK);
}

/*
 * Returns true if index does not belong to the specified rnode.  Otherwise,
 * sets slot value, and returns false.
 */
static __inline bool
vm_radix_keybarr(struct vm_radix_node *rnode, vm_pindex_t index, int *slot)
{
	index = (index - rnode->rn_owner) >> rnode->rn_clev;
	if (index >= VM_RADIX_COUNT)
		return (true);
	*slot = index;
	return (false);
}

/*
 * Allocate a radix node.
 */
static struct vm_radix_node *
vm_radix_node_get(vm_pindex_t index, vm_pindex_t newind)
{
	struct vm_radix_node *rnode;

	rnode = uma_zalloc_smr(vm_radix_node_zone, M_NOWAIT);
	if (rnode == NULL)
		return (NULL);

	/*
	 * We want to clear the last child pointer after the final section
	 * has exited so lookup can not return false negatives.  It is done
	 * here because it will be cache-cold in the dtor callback.
	 */
	if (rnode->rn_popmap != 0) {
		vm_radix_node_store(&rnode->rn_child[ffs(rnode->rn_popmap) - 1],
		    VM_RADIX_NULL, UNSERIALIZED);
		rnode->rn_popmap = 0;
	}

	/*
	 * From the highest-order bit where the indexes differ,
	 * compute the highest level in the trie where they differ.  Then,
	 * compute the least index of this subtrie.
	 */
	KASSERT(index != newind, ("%s: passing the same key value %jx",
	    __func__, (uintmax_t)index));
	_Static_assert(sizeof(long long) >= sizeof(vm_pindex_t),
	    "vm_pindex_t too wide");
	_Static_assert(sizeof(vm_pindex_t) * NBBY <=
	    (1 << (sizeof(rnode->rn_clev) * NBBY)), "rn_clev too narrow");
	rnode->rn_clev = rounddown(flsll(index ^ newind) - 1, VM_RADIX_WIDTH);
	rnode->rn_owner = VM_RADIX_COUNT;
	rnode->rn_owner = index & -(rnode->rn_owner << rnode->rn_clev);
	return (rnode);
}

/*
 * Free radix node.
 */
static __inline void
vm_radix_node_put(struct vm_radix_node *rnode)
{
#ifdef INVARIANTS
	int slot;

	KASSERT(powerof2(rnode->rn_popmap),
	    ("vm_radix_node_put: rnode %p has too many children %04x", rnode,
	    rnode->rn_popmap));
	for (slot = 0; slot < VM_RADIX_COUNT; slot++) {
		if ((rnode->rn_popmap & (1 << slot)) != 0)
			continue;
		KASSERT(smr_unserialized_load(&rnode->rn_child[slot], true) ==
		    VM_RADIX_NULL,
		    ("vm_radix_node_put: rnode %p has a child", rnode));
	}
#endif
	uma_zfree_smr(vm_radix_node_zone, rnode);
}

/*
 * Fetch a node pointer from a slot in another node.
 */
static __inline struct vm_radix_node *
vm_radix_node_load(smrnode_t *p, enum vm_radix_access access)
{

	switch (access) {
	case UNSERIALIZED:
		return (smr_unserialized_load(p, true));
	case LOCKED:
		return (smr_serialized_load(p, true));
	case SMR:
		return (smr_entered_load(p, vm_radix_smr));
	}
	__assert_unreachable();
}

static __inline void
vm_radix_node_store(smrnode_t *p, struct vm_radix_node *v,
    enum vm_radix_access access)
{

	switch (access) {
	case UNSERIALIZED:
		smr_unserialized_store(p, v, true);
		break;
	case LOCKED:
		smr_serialized_store(p, v, true);
		break;
	case SMR:
		panic("vm_radix_node_store: Not supported in smr section.");
	}
}

/*
 * Get the root node for a radix tree.
 */
static __inline struct vm_radix_node *
vm_radix_root_load(struct vm_radix *rtree, enum vm_radix_access access)
{

	return (vm_radix_node_load((smrnode_t *)&rtree->rt_root, access));
}

/*
 * Set the root node for a radix tree.
 */
static __inline void
vm_radix_root_store(struct vm_radix *rtree, struct vm_radix_node *rnode,
    enum vm_radix_access access)
{

	vm_radix_node_store((smrnode_t *)&rtree->rt_root, rnode, access);
}

/*
 * Returns TRUE if the specified radix node is a leaf and FALSE otherwise.
 */
static __inline bool
vm_radix_isleaf(struct vm_radix_node *rnode)
{

	return (((uintptr_t)rnode & VM_RADIX_ISLEAF) != 0);
}

/*
 * Returns page cast to radix node with leaf bit set.
 */
static __inline struct vm_radix_node *
vm_radix_toleaf(vm_page_t page)
{
	return ((struct vm_radix_node *)((uintptr_t)page | VM_RADIX_ISLEAF));
}

/*
 * Returns the associated page extracted from rnode.
 */
static __inline vm_page_t
vm_radix_topage(struct vm_radix_node *rnode)
{

	return ((vm_page_t)((uintptr_t)rnode & ~VM_RADIX_FLAGS));
}

/*
 * Make 'child' a child of 'rnode'.
 */
static __inline void
vm_radix_addnode(struct vm_radix_node *rnode, vm_pindex_t index,
    struct vm_radix_node *child, enum vm_radix_access access)
{
	int slot;

	slot = vm_radix_slot(rnode, index);
	vm_radix_node_store(&rnode->rn_child[slot], child, access);
	rnode->rn_popmap ^= 1 << slot;
	KASSERT((rnode->rn_popmap & (1 << slot)) != 0,
	    ("%s: bad popmap slot %d in rnode %p", __func__, slot, rnode));
}

/*
 * Internal helper for vm_radix_reclaim_allnodes().
 * This function is recursive.
 */
static void
vm_radix_reclaim_allnodes_int(struct vm_radix_node *rnode)
{
	struct vm_radix_node *child;
	int slot;

	while (rnode->rn_popmap != 0) {
		slot = ffs(rnode->rn_popmap) - 1;
		child = vm_radix_node_load(&rnode->rn_child[slot],
		    UNSERIALIZED);
		KASSERT(child != VM_RADIX_NULL,
		    ("%s: bad popmap slot %d in rnode %p",
		    __func__, slot, rnode));
		if (!vm_radix_isleaf(child))
			vm_radix_reclaim_allnodes_int(child);
		rnode->rn_popmap ^= 1 << slot;
		vm_radix_node_store(&rnode->rn_child[slot], VM_RADIX_NULL,
		    UNSERIALIZED);
	}
	vm_radix_node_put(rnode);
}

/*
 * radix node zone initializer.
 */
static int
vm_radix_zone_init(void *mem, int size, int flags)
{
	struct vm_radix_node *rnode;

	rnode = mem;
	rnode->rn_popmap = 0;
	for (int i = 0; i < nitems(rnode->rn_child); i++)
		vm_radix_node_store(&rnode->rn_child[i], VM_RADIX_NULL,
		    UNSERIALIZED);
	return (0);
}

#ifndef UMA_MD_SMALL_ALLOC
void vm_radix_reserve_kva(void);
/*
 * Reserve the KVA necessary to satisfy the node allocation.
 * This is mandatory in architectures not supporting direct
 * mapping as they will need otherwise to carve into the kernel maps for
 * every node allocation, resulting into deadlocks for consumers already
 * working with kernel maps.
 */
void
vm_radix_reserve_kva(void)
{

	/*
	 * Calculate the number of reserved nodes, discounting the pages that
	 * are needed to store them.
	 */
	if (!uma_zone_reserve_kva(vm_radix_node_zone,
	    ((vm_paddr_t)vm_cnt.v_page_count * PAGE_SIZE) / (PAGE_SIZE +
	    sizeof(struct vm_radix_node))))
		panic("%s: unable to reserve KVA", __func__);
}
#endif

/*
 * Initialize the UMA slab zone.
 */
void
vm_radix_zinit(void)
{

	vm_radix_node_zone = uma_zcreate("RADIX NODE",
	    sizeof(struct vm_radix_node), NULL, NULL, vm_radix_zone_init, NULL,
	    VM_RADIX_PAD, UMA_ZONE_VM | UMA_ZONE_SMR);
	vm_radix_smr = uma_zone_get_smr(vm_radix_node_zone);
}

/*
 * Inserts the key-value pair into the trie.
 * Panics if the key already exists.
 */
int
vm_radix_insert(struct vm_radix *rtree, vm_page_t page)
{
	vm_pindex_t index, newind;
	struct vm_radix_node *leaf, *parent, *rnode;
	smrnode_t *parentp;
	int slot;

	index = page->pindex;
	leaf = vm_radix_toleaf(page);

	/*
	 * The owner of record for root is not really important because it
	 * will never be used.
	 */
	rnode = vm_radix_root_load(rtree, LOCKED);
	parent = NULL;
	for (;;) {
		if (vm_radix_isleaf(rnode)) {
			if (rnode == VM_RADIX_NULL) {
				if (parent == NULL)
					rtree->rt_root = leaf;
				else
					vm_radix_addnode(parent, index, leaf,
					    LOCKED);
				return (0);
			}
			newind = vm_radix_topage(rnode)->pindex;
			if (newind == index)
				panic("%s: key %jx is already present",
				    __func__, (uintmax_t)index);
			break;
		}
		if (vm_radix_keybarr(rnode, index, &slot)) {
			newind = rnode->rn_owner;
			break;
		}
		parent = rnode;
		rnode = vm_radix_node_load(&rnode->rn_child[slot], LOCKED);
	}

	/*
	 * A new node is needed because the right insertion level is reached.
	 * Setup the new intermediate node and add the 2 children: the
	 * new object and the older edge or object.
	 */
	parentp = (parent != NULL) ? &parent->rn_child[slot]:
	    (smrnode_t *)&rtree->rt_root;
	parent = vm_radix_node_get(index, newind);
	if (parent == NULL)
		return (ENOMEM);
	/* These writes are not yet visible due to ordering. */
	vm_radix_addnode(parent, index, leaf, UNSERIALIZED);
	vm_radix_addnode(parent, newind, rnode, UNSERIALIZED);
	/* Serializing write to make the above visible. */
	vm_radix_node_store(parentp, parent, LOCKED);
	return (0);
}

/*
 * Returns the value stored at the index.  If the index is not present,
 * NULL is returned.
 */
static __always_inline vm_page_t
_vm_radix_lookup(struct vm_radix *rtree, vm_pindex_t index,
    enum vm_radix_access access)
{
	struct vm_radix_node *rnode;
	vm_page_t m;
	int slot;

	rnode = vm_radix_root_load(rtree, access);
	for (;;) {
		if (vm_radix_isleaf(rnode)) {
			if ((m = vm_radix_topage(rnode)) != NULL &&
			    m->pindex == index)
				return (m);
			break;
		}
		if (vm_radix_keybarr(rnode, index, &slot))
			break;
		rnode = vm_radix_node_load(&rnode->rn_child[slot], access);
	}
	return (NULL);
}

/*
 * Returns the value stored at the index assuming there is an external lock.
 *
 * If the index is not present, NULL is returned.
 */
vm_page_t
vm_radix_lookup(struct vm_radix *rtree, vm_pindex_t index)
{

	return _vm_radix_lookup(rtree, index, LOCKED);
}

/*
 * Returns the value stored at the index without requiring an external lock.
 *
 * If the index is not present, NULL is returned.
 */
vm_page_t
vm_radix_lookup_unlocked(struct vm_radix *rtree, vm_pindex_t index)
{
	vm_page_t m;

	smr_enter(vm_radix_smr);
	m = _vm_radix_lookup(rtree, index, SMR);
	smr_exit(vm_radix_smr);

	return (m);
}

/*
 * Returns the page with the least pindex that is greater than or equal to the
 * specified pindex, or NULL if there are no such pages.
 *
 * Requires that access be externally synchronized by a lock.
 */
vm_page_t
vm_radix_lookup_ge(struct vm_radix *rtree, vm_pindex_t index)
{
	struct vm_radix_node *rnode, *succ;
	vm_page_t m;
	int slot;

	/*
	 * Descend the trie as if performing an ordinary lookup for the page
	 * with the specified pindex.  However, unlike an ordinary lookup, as we
	 * descend the trie, we use "succ" to remember the last branching-off
	 * point, that is, the interior node under which the page with the least
	 * pindex that is both outside our current path down the trie and more
	 * than the specified pindex resides.  (The node's popmap makes it fast
	 * and easy to recognize a branching-off point.)  If our ordinary lookup
	 * fails to yield a page with a pindex that is greater than or equal to
	 * the specified pindex, then we will exit this loop and perform a
	 * lookup starting from "succ".  If "succ" is not NULL, then that lookup
	 * is guaranteed to succeed.
	 */
	rnode = vm_radix_root_load(rtree, LOCKED);
	succ = NULL;
	for (;;) {
		if (vm_radix_isleaf(rnode)) {
			if ((m = vm_radix_topage(rnode)) != NULL &&
			    m->pindex >= index)
				return (m);
			break;
		}
		if (vm_radix_keybarr(rnode, index, &slot)) {
			/*
			 * If all pages in this subtree have pindex > index,
			 * then the page in this subtree with the least pindex
			 * is the answer.
			 */
			if (rnode->rn_owner > index)
				succ = rnode;
			break;
		}

		/*
		 * Just in case the next search step leads to a subtree of all
		 * pages with pindex < index, check popmap to see if a next
		 * bigger step, to a subtree of all pages with pindex > index,
		 * is available.  If so, remember to restart the search here.
		 */
		if ((rnode->rn_popmap >> slot) > 1)
			succ = rnode;
		rnode = vm_radix_node_load(&rnode->rn_child[slot], LOCKED);
	}

	/*
	 * Restart the search from the last place visited in the subtree that
	 * included some pages with pindex > index, if there was such a place.
	 */
	if (succ == NULL)
		return (NULL);
	if (succ != rnode) {
		/*
		 * Take a step to the next bigger sibling of the node chosen
		 * last time.  In that subtree, all pages have pindex > index.
		 */
		slot = vm_radix_slot(succ, index) + 1;
		KASSERT((succ->rn_popmap >> slot) != 0,
		    ("%s: no popmap siblings past slot %d in node %p",
		    __func__, slot, succ));
		slot += ffs(succ->rn_popmap >> slot) - 1;
		succ = vm_radix_node_load(&succ->rn_child[slot], LOCKED);
	}

	/*
	 * Find the page in the subtree rooted at "succ" with the least pindex.
	 */
	while (!vm_radix_isleaf(succ)) {
		KASSERT(succ->rn_popmap != 0,
		    ("%s: no popmap children in node %p",  __func__, succ));
		slot = ffs(succ->rn_popmap) - 1;
		succ = vm_radix_node_load(&succ->rn_child[slot], LOCKED);
	}
	return (vm_radix_topage(succ));
}

/*
 * Returns the page with the greatest pindex that is less than or equal to the
 * specified pindex, or NULL if there are no such pages.
 *
 * Requires that access be externally synchronized by a lock.
 */
vm_page_t
vm_radix_lookup_le(struct vm_radix *rtree, vm_pindex_t index)
{
	struct vm_radix_node *pred, *rnode;
	vm_page_t m;
	int slot;

	/*
	 * Mirror the implementation of vm_radix_lookup_ge, described above.
	 */
	rnode = vm_radix_root_load(rtree, LOCKED);
	pred = NULL;
	for (;;) {
		if (vm_radix_isleaf(rnode)) {
			if ((m = vm_radix_topage(rnode)) != NULL &&
			    m->pindex <= index)
				return (m);
			break;
		}
		if (vm_radix_keybarr(rnode, index, &slot)) {
			if (rnode->rn_owner < index)
				pred = rnode;
			break;
		}
		if ((rnode->rn_popmap & ((1 << slot) - 1)) != 0)
			pred = rnode;
		rnode = vm_radix_node_load(&rnode->rn_child[slot], LOCKED);
	}
	if (pred == NULL)
		return (NULL);
	if (pred != rnode) {
		slot = vm_radix_slot(pred, index);
		KASSERT((pred->rn_popmap & ((1 << slot) - 1)) != 0,
		    ("%s: no popmap siblings before slot %d in node %p",
		    __func__, slot, pred));
		slot = fls(pred->rn_popmap & ((1 << slot) - 1)) - 1;
		pred = vm_radix_node_load(&pred->rn_child[slot], LOCKED);
	}
	while (!vm_radix_isleaf(pred)) {
		KASSERT(pred->rn_popmap != 0,
		    ("%s: no popmap children in node %p",  __func__, pred));
		slot = fls(pred->rn_popmap) - 1;
		pred = vm_radix_node_load(&pred->rn_child[slot], LOCKED);
	}
	return (vm_radix_topage(pred));
}

/*
 * Remove the specified index from the trie, and return the value stored at
 * that index.  If the index is not present, return NULL.
 */
vm_page_t
vm_radix_remove(struct vm_radix *rtree, vm_pindex_t index)
{
	struct vm_radix_node *child, *parent, *rnode;
	vm_page_t m;
	int slot;

	rnode = NULL;
	child = vm_radix_root_load(rtree, LOCKED);
	for (;;) {
		if (vm_radix_isleaf(child))
			break;
		parent = rnode;
		rnode = child;
		slot = vm_radix_slot(rnode, index);
		child = vm_radix_node_load(&rnode->rn_child[slot], LOCKED);
	}
	if ((m = vm_radix_topage(child)) == NULL || m->pindex != index)
		return (NULL);
	if (rnode == NULL) {
		vm_radix_root_store(rtree, VM_RADIX_NULL, LOCKED);
		return (m);
	}
	KASSERT((rnode->rn_popmap & (1 << slot)) != 0,
	    ("%s: bad popmap slot %d in rnode %p", __func__, slot, rnode));
	rnode->rn_popmap ^= 1 << slot;
	vm_radix_node_store(&rnode->rn_child[slot], VM_RADIX_NULL, LOCKED);
	if (!powerof2(rnode->rn_popmap))
		return (m);
	KASSERT(rnode->rn_popmap != 0, ("%s: bad popmap all zeroes", __func__));
	slot = ffs(rnode->rn_popmap) - 1;
	child = vm_radix_node_load(&rnode->rn_child[slot], LOCKED);
	KASSERT(child != VM_RADIX_NULL,
	    ("%s: bad popmap slot %d in rnode %p", __func__, slot, rnode));
	if (parent == NULL)
		vm_radix_root_store(rtree, child, LOCKED);
	else {
		slot = vm_radix_slot(parent, index);
		KASSERT(rnode ==
		    vm_radix_node_load(&parent->rn_child[slot], LOCKED),
		    ("%s: invalid child value", __func__));
		vm_radix_node_store(&parent->rn_child[slot], child, LOCKED);
	}
	/*
	 * The child is still valid and we can not zero the
	 * pointer until all smr references are gone.
	 */
	vm_radix_node_put(rnode);
	return (m);
}

/*
 * Remove and free all the nodes from the radix tree.
 * This function is recursive but there is a tight control on it as the
 * maximum depth of the tree is fixed.
 */
void
vm_radix_reclaim_allnodes(struct vm_radix *rtree)
{
	struct vm_radix_node *root;

	root = vm_radix_root_load(rtree, LOCKED);
	if (root == VM_RADIX_NULL)
		return;
	vm_radix_root_store(rtree, VM_RADIX_NULL, UNSERIALIZED);
	if (!vm_radix_isleaf(root))
		vm_radix_reclaim_allnodes_int(root);
}

/*
 * Replace an existing page in the trie with another one.
 * Panics if there is not an old page in the trie at the new page's index.
 */
vm_page_t
vm_radix_replace(struct vm_radix *rtree, vm_page_t newpage)
{
	struct vm_radix_node *leaf, *parent, *rnode;
	vm_page_t m;
	vm_pindex_t index;
	int slot;

	leaf = vm_radix_toleaf(newpage);
	index = newpage->pindex;
	rnode = vm_radix_root_load(rtree, LOCKED);
	parent = NULL;
	for (;;) {
		if (vm_radix_isleaf(rnode)) {
			if ((m = vm_radix_topage(rnode)) != NULL &&
			    m->pindex == index) {
				if (parent == NULL)
					rtree->rt_root = leaf;
				else
					vm_radix_node_store(
					    &parent->rn_child[slot], leaf,
					    LOCKED);
				return (m);
			}
			break;
		}
		if (vm_radix_keybarr(rnode, index, &slot))
			break;
		parent = rnode;
		rnode = vm_radix_node_load(&rnode->rn_child[slot], LOCKED);
	}
	panic("%s: original replacing page not found", __func__);
}

void
vm_radix_wait(void)
{
	uma_zwait(vm_radix_node_zone);
}

#ifdef DDB
/*
 * Show details about the given radix node.
 */
DB_SHOW_COMMAND(radixnode, db_show_radixnode)
{
	struct vm_radix_node *rnode, *tmp;
	int slot;
	rn_popmap_t popmap;

        if (!have_addr)
                return;
	rnode = (struct vm_radix_node *)addr;
	db_printf("radixnode %p, owner %jx, children popmap %04x, level %u:\n",
	    (void *)rnode, (uintmax_t)rnode->rn_owner, rnode->rn_popmap,
	    rnode->rn_clev / VM_RADIX_WIDTH);
	for (popmap = rnode->rn_popmap; popmap != 0; popmap ^= 1 << slot) {
		slot = ffs(popmap) - 1;
		tmp = vm_radix_node_load(&rnode->rn_child[slot], UNSERIALIZED);
		db_printf("slot: %d, val: %p, page: %p, clev: %d\n",
		    slot, (void *)tmp,
		    vm_radix_isleaf(tmp) ?  vm_radix_topage(tmp) : NULL,
		    rnode->rn_clev / VM_RADIX_WIDTH);
	}
}
#endif /* DDB */
