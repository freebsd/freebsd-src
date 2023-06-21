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

/* Flag bits stored in node pointers. */
#define	VM_RADIX_ISLEAF	0x1
#define	VM_RADIX_FLAGS	0x1
#define	VM_RADIX_PAD	VM_RADIX_FLAGS

/* Returns one unit associated with specified level. */
#define	VM_RADIX_UNITLEVEL(lev)						\
	((vm_pindex_t)1 << ((lev) * VM_RADIX_WIDTH))

enum vm_radix_access { SMR, LOCKED, UNSERIALIZED };

struct vm_radix_node;
typedef SMR_POINTER(struct vm_radix_node *) smrnode_t;

struct vm_radix_node {
	vm_pindex_t	rn_owner;			/* Owner of record. */
	uint16_t	rn_count;			/* Valid children. */
	uint8_t		rn_clev;			/* Current level. */
	int8_t		rn_last;			/* zero last ptr. */
	smrnode_t	rn_child[VM_RADIX_COUNT];	/* Child nodes. */
};

static uma_zone_t vm_radix_node_zone;
static smr_t vm_radix_smr;

static void vm_radix_node_store(smrnode_t *p, struct vm_radix_node *v,
    enum vm_radix_access access);

/*
 * Allocate a radix node.
 */
static struct vm_radix_node *
vm_radix_node_get(vm_pindex_t owner, uint16_t count, uint16_t clevel)
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
	if (rnode->rn_last != 0) {
		vm_radix_node_store(&rnode->rn_child[rnode->rn_last - 1],
		    NULL, UNSERIALIZED);
		rnode->rn_last = 0;
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
vm_radix_node_put(struct vm_radix_node *rnode, int8_t last)
{
#ifdef INVARIANTS
	int slot;

	KASSERT(rnode->rn_count == 0,
	    ("vm_radix_node_put: rnode %p has %d children", rnode,
	    rnode->rn_count));
	for (slot = 0; slot < VM_RADIX_COUNT; slot++) {
		if (slot == last)
			continue;
		KASSERT(smr_unserialized_load(&rnode->rn_child[slot], true) ==
		    NULL, ("vm_radix_node_put: rnode %p has a child", rnode));
	}
#endif
	/* Off by one so a freshly zero'd node is not assigned to. */
	rnode->rn_last = last + 1;
	uma_zfree_smr(vm_radix_node_zone, rnode);
}

/*
 * Return the position in the array for a given level.
 */
static __inline int
vm_radix_slot(vm_pindex_t index, uint16_t level)
{

	return ((index >> (level * VM_RADIX_WIDTH)) & VM_RADIX_MASK);
}

/* Trims the key after the specified level. */
static __inline vm_pindex_t
vm_radix_trimkey(vm_pindex_t index, uint16_t level)
{
	vm_pindex_t ret;

	ret = index;
	if (level > 0) {
		ret >>= level * VM_RADIX_WIDTH;
		ret <<= level * VM_RADIX_WIDTH;
	}
	return (ret);
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
 * Returns the associated page extracted from rnode.
 */
static __inline vm_page_t
vm_radix_topage(struct vm_radix_node *rnode)
{

	return ((vm_page_t)((uintptr_t)rnode & ~VM_RADIX_FLAGS));
}

/*
 * Adds the page as a child of the provided node.
 */
static __inline void
vm_radix_addpage(struct vm_radix_node *rnode, vm_pindex_t index, uint16_t clev,
    vm_page_t page, enum vm_radix_access access)
{
	int slot;

	slot = vm_radix_slot(index, clev);
	vm_radix_node_store(&rnode->rn_child[slot],
	    (struct vm_radix_node *)((uintptr_t)page | VM_RADIX_ISLEAF), access);
}

/*
 * Returns the level where two keys differ.
 * It cannot accept 2 equal keys.
 */
static __inline uint16_t
vm_radix_keydiff(vm_pindex_t index1, vm_pindex_t index2)
{

	KASSERT(index1 != index2, ("%s: passing the same key value %jx",
	    __func__, (uintmax_t)index1));
	CTASSERT(sizeof(long long) >= sizeof(vm_pindex_t));

	/*
	 * From the highest-order bit where the indexes differ,
	 * compute the highest level in the trie where they differ.
	 */
	return ((flsll(index1 ^ index2) - 1) / VM_RADIX_WIDTH);
}

/*
 * Returns TRUE if it can be determined that key does not belong to the
 * specified rnode.  Otherwise, returns FALSE.
 */
static __inline bool
vm_radix_keybarr(struct vm_radix_node *rnode, vm_pindex_t idx)
{

	if (rnode->rn_clev < VM_RADIX_LIMIT) {
		idx = vm_radix_trimkey(idx, rnode->rn_clev + 1);
		return (idx != rnode->rn_owner);
	}
	return (false);
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

	KASSERT(rnode->rn_count <= VM_RADIX_COUNT,
	    ("vm_radix_reclaim_allnodes_int: bad count in rnode %p", rnode));
	for (slot = 0; rnode->rn_count != 0; slot++) {
		child = vm_radix_node_load(&rnode->rn_child[slot], UNSERIALIZED);
		if (child == NULL)
			continue;
		if (!vm_radix_isleaf(child))
			vm_radix_reclaim_allnodes_int(child);
		vm_radix_node_store(&rnode->rn_child[slot], NULL, UNSERIALIZED);
		rnode->rn_count--;
	}
	vm_radix_node_put(rnode, -1);
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
	    sizeof(struct vm_radix_node), NULL, NULL, NULL, NULL,
	    VM_RADIX_PAD, UMA_ZONE_VM | UMA_ZONE_SMR | UMA_ZONE_ZINIT);
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
	struct vm_radix_node *rnode, *tmp;
	smrnode_t *parentp;
	vm_page_t m;
	int slot;
	uint16_t clev;

	index = page->pindex;

	/*
	 * The owner of record for root is not really important because it
	 * will never be used.
	 */
	rnode = vm_radix_root_load(rtree, LOCKED);
	if (rnode == NULL) {
		rtree->rt_root = (uintptr_t)page | VM_RADIX_ISLEAF;
		return (0);
	}
	parentp = (smrnode_t *)&rtree->rt_root;
	for (;;) {
		if (vm_radix_isleaf(rnode)) {
			m = vm_radix_topage(rnode);
			if (m->pindex == index)
				panic("%s: key %jx is already present",
				    __func__, (uintmax_t)index);
			clev = vm_radix_keydiff(m->pindex, index);
			tmp = vm_radix_node_get(vm_radix_trimkey(index,
			    clev + 1), 2, clev);
			if (tmp == NULL)
				return (ENOMEM);
			/* These writes are not yet visible due to ordering. */
			vm_radix_addpage(tmp, index, clev, page, UNSERIALIZED);
			vm_radix_addpage(tmp, m->pindex, clev, m, UNSERIALIZED);
			/* Synchronize to make leaf visible. */
			vm_radix_node_store(parentp, tmp, LOCKED);
			return (0);
		} else if (vm_radix_keybarr(rnode, index))
			break;
		slot = vm_radix_slot(index, rnode->rn_clev);
		parentp = &rnode->rn_child[slot];
		tmp = vm_radix_node_load(parentp, LOCKED);
		if (tmp == NULL) {
			rnode->rn_count++;
			vm_radix_addpage(rnode, index, rnode->rn_clev, page,
			    LOCKED);
			return (0);
		}
		rnode = tmp;
	}

	/*
	 * A new node is needed because the right insertion level is reached.
	 * Setup the new intermediate node and add the 2 children: the
	 * new object and the older edge.
	 */
	newind = rnode->rn_owner;
	clev = vm_radix_keydiff(newind, index);
	tmp = vm_radix_node_get(vm_radix_trimkey(index, clev + 1), 2, clev);
	if (tmp == NULL)
		return (ENOMEM);
	slot = vm_radix_slot(newind, clev);
	/* These writes are not yet visible due to ordering. */
	vm_radix_addpage(tmp, index, clev, page, UNSERIALIZED);
	vm_radix_node_store(&tmp->rn_child[slot], rnode, UNSERIALIZED);
	/* Serializing write to make the above visible. */
	vm_radix_node_store(parentp, tmp, LOCKED);

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
	while (rnode != NULL) {
		if (vm_radix_isleaf(rnode)) {
			m = vm_radix_topage(rnode);
			if (m->pindex == index)
				return (m);
			break;
		}
		if (vm_radix_keybarr(rnode, index))
			break;
		slot = vm_radix_slot(index, rnode->rn_clev);
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
 * Look up the nearest entry at a position greater than or equal to index.
 */
vm_page_t
vm_radix_lookup_ge(struct vm_radix *rtree, vm_pindex_t index)
{
	struct vm_radix_node *stack[VM_RADIX_LIMIT];
	vm_pindex_t inc;
	vm_page_t m;
	struct vm_radix_node *child, *rnode;
#ifdef INVARIANTS
	int loops = 0;
#endif
	int slot, tos;

	rnode = vm_radix_root_load(rtree, LOCKED);
	if (rnode == NULL)
		return (NULL);
	else if (vm_radix_isleaf(rnode)) {
		m = vm_radix_topage(rnode);
		if (m->pindex >= index)
			return (m);
		else
			return (NULL);
	}
	tos = 0;
	for (;;) {
		/*
		 * If the keys differ before the current bisection node,
		 * then the search key might rollback to the earliest
		 * available bisection node or to the smallest key
		 * in the current node (if the owner is greater than the
		 * search key).
		 */
		if (vm_radix_keybarr(rnode, index)) {
			if (index > rnode->rn_owner) {
ascend:
				KASSERT(++loops < 1000,
				    ("vm_radix_lookup_ge: too many loops"));

				/*
				 * Pop nodes from the stack until either the
				 * stack is empty or a node that could have a
				 * matching descendant is found.
				 */
				do {
					if (tos == 0)
						return (NULL);
					rnode = stack[--tos];
				} while (vm_radix_slot(index,
				    rnode->rn_clev) == (VM_RADIX_COUNT - 1));

				/*
				 * The following computation cannot overflow
				 * because index's slot at the current level
				 * is less than VM_RADIX_COUNT - 1.
				 */
				index = vm_radix_trimkey(index,
				    rnode->rn_clev);
				index += VM_RADIX_UNITLEVEL(rnode->rn_clev);
			} else
				index = rnode->rn_owner;
			KASSERT(!vm_radix_keybarr(rnode, index),
			    ("vm_radix_lookup_ge: keybarr failed"));
		}
		slot = vm_radix_slot(index, rnode->rn_clev);
		child = vm_radix_node_load(&rnode->rn_child[slot], LOCKED);
		if (vm_radix_isleaf(child)) {
			m = vm_radix_topage(child);
			if (m->pindex >= index)
				return (m);
		} else if (child != NULL)
			goto descend;

		/*
		 * Look for an available edge or page within the current
		 * bisection node.
		 */
                if (slot < (VM_RADIX_COUNT - 1)) {
			inc = VM_RADIX_UNITLEVEL(rnode->rn_clev);
			index = vm_radix_trimkey(index, rnode->rn_clev);
			do {
				index += inc;
				slot++;
				child = vm_radix_node_load(&rnode->rn_child[slot],
				    LOCKED);
				if (vm_radix_isleaf(child)) {
					m = vm_radix_topage(child);
					if (m->pindex >= index)
						return (m);
				} else if (child != NULL)
					goto descend;
			} while (slot < (VM_RADIX_COUNT - 1));
		}
		KASSERT(child == NULL || vm_radix_isleaf(child),
		    ("vm_radix_lookup_ge: child is radix node"));

		/*
		 * If a page or edge greater than the search slot is not found
		 * in the current node, ascend to the next higher-level node.
		 */
		goto ascend;
descend:
		KASSERT(rnode->rn_clev > 0,
		    ("vm_radix_lookup_ge: pushing leaf's parent"));
		KASSERT(tos < VM_RADIX_LIMIT,
		    ("vm_radix_lookup_ge: stack overflow"));
		stack[tos++] = rnode;
		rnode = child;
	}
}

/*
 * Look up the nearest entry at a position less than or equal to index.
 */
vm_page_t
vm_radix_lookup_le(struct vm_radix *rtree, vm_pindex_t index)
{
	struct vm_radix_node *stack[VM_RADIX_LIMIT];
	vm_pindex_t inc;
	vm_page_t m;
	struct vm_radix_node *child, *rnode;
#ifdef INVARIANTS
	int loops = 0;
#endif
	int slot, tos;

	rnode = vm_radix_root_load(rtree, LOCKED);
	if (rnode == NULL)
		return (NULL);
	else if (vm_radix_isleaf(rnode)) {
		m = vm_radix_topage(rnode);
		if (m->pindex <= index)
			return (m);
		else
			return (NULL);
	}
	tos = 0;
	for (;;) {
		/*
		 * If the keys differ before the current bisection node,
		 * then the search key might rollback to the earliest
		 * available bisection node or to the largest key
		 * in the current node (if the owner is smaller than the
		 * search key).
		 */
		if (vm_radix_keybarr(rnode, index)) {
			if (index > rnode->rn_owner) {
				index = rnode->rn_owner + VM_RADIX_COUNT *
				    VM_RADIX_UNITLEVEL(rnode->rn_clev);
			} else {
ascend:
				KASSERT(++loops < 1000,
				    ("vm_radix_lookup_le: too many loops"));

				/*
				 * Pop nodes from the stack until either the
				 * stack is empty or a node that could have a
				 * matching descendant is found.
				 */
				do {
					if (tos == 0)
						return (NULL);
					rnode = stack[--tos];
				} while (vm_radix_slot(index,
				    rnode->rn_clev) == 0);

				/*
				 * The following computation cannot overflow
				 * because index's slot at the current level
				 * is greater than 0.
				 */
				index = vm_radix_trimkey(index,
				    rnode->rn_clev);
			}
			index--;
			KASSERT(!vm_radix_keybarr(rnode, index),
			    ("vm_radix_lookup_le: keybarr failed"));
		}
		slot = vm_radix_slot(index, rnode->rn_clev);
		child = vm_radix_node_load(&rnode->rn_child[slot], LOCKED);
		if (vm_radix_isleaf(child)) {
			m = vm_radix_topage(child);
			if (m->pindex <= index)
				return (m);
		} else if (child != NULL)
			goto descend;

		/*
		 * Look for an available edge or page within the current
		 * bisection node.
		 */
		if (slot > 0) {
			inc = VM_RADIX_UNITLEVEL(rnode->rn_clev);
			index |= inc - 1;
			do {
				index -= inc;
				slot--;
				child = vm_radix_node_load(&rnode->rn_child[slot],
				    LOCKED);
				if (vm_radix_isleaf(child)) {
					m = vm_radix_topage(child);
					if (m->pindex <= index)
						return (m);
				} else if (child != NULL)
					goto descend;
			} while (slot > 0);
		}
		KASSERT(child == NULL || vm_radix_isleaf(child),
		    ("vm_radix_lookup_le: child is radix node"));

		/*
		 * If a page or edge smaller than the search slot is not found
		 * in the current node, ascend to the next higher-level node.
		 */
		goto ascend;
descend:
		KASSERT(rnode->rn_clev > 0,
		    ("vm_radix_lookup_le: pushing leaf's parent"));
		KASSERT(tos < VM_RADIX_LIMIT,
		    ("vm_radix_lookup_le: stack overflow"));
		stack[tos++] = rnode;
		rnode = child;
	}
}

/*
 * Remove the specified index from the trie, and return the value stored at
 * that index.  If the index is not present, return NULL.
 */
vm_page_t
vm_radix_remove(struct vm_radix *rtree, vm_pindex_t index)
{
	struct vm_radix_node *rnode, *parent, *tmp;
	vm_page_t m;
	int i, slot;

	rnode = vm_radix_root_load(rtree, LOCKED);
	if (vm_radix_isleaf(rnode)) {
		m = vm_radix_topage(rnode);
		if (m->pindex != index)
			return (NULL);
		vm_radix_root_store(rtree, NULL, LOCKED);
		return (m);
	}
	parent = NULL;
	for (;;) {
		if (rnode == NULL)
			return (NULL);
		slot = vm_radix_slot(index, rnode->rn_clev);
		tmp = vm_radix_node_load(&rnode->rn_child[slot], LOCKED);
		if (vm_radix_isleaf(tmp)) {
			m = vm_radix_topage(tmp);
			if (m->pindex != index)
				return (NULL);
			vm_radix_node_store(&rnode->rn_child[slot], NULL, LOCKED);
			rnode->rn_count--;
			if (rnode->rn_count > 1)
				return (m);
			for (i = 0; i < VM_RADIX_COUNT; i++)
				if (vm_radix_node_load(&rnode->rn_child[i],
				    LOCKED) != NULL)
					break;
			KASSERT(i != VM_RADIX_COUNT,
			    ("%s: invalid node configuration", __func__));
			tmp = vm_radix_node_load(&rnode->rn_child[i], LOCKED);
			if (parent == NULL)
				vm_radix_root_store(rtree, tmp, LOCKED);
			else {
				slot = vm_radix_slot(index, parent->rn_clev);
				KASSERT(vm_radix_node_load(
				    &parent->rn_child[slot], LOCKED) == rnode,
				    ("%s: invalid child value", __func__));
				vm_radix_node_store(&parent->rn_child[slot],
				    tmp, LOCKED);
			}
			/*
			 * The child is still valid and we can not zero the
			 * pointer until all smr references are gone.
			 */
			rnode->rn_count--;
			vm_radix_node_put(rnode, i);
			return (m);
		}
		parent = rnode;
		rnode = tmp;
	}
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
	if (root == NULL)
		return;
	vm_radix_root_store(rtree, NULL, UNSERIALIZED);
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
	struct vm_radix_node *rnode, *tmp;
	vm_page_t m;
	vm_pindex_t index;
	int slot;

	index = newpage->pindex;
	rnode = vm_radix_root_load(rtree, LOCKED);
	if (rnode == NULL)
		panic("%s: replacing page on an empty trie", __func__);
	if (vm_radix_isleaf(rnode)) {
		m = vm_radix_topage(rnode);
		if (m->pindex != index)
			panic("%s: original replacing root key not found",
			    __func__);
		rtree->rt_root = (uintptr_t)newpage | VM_RADIX_ISLEAF;
		return (m);
	}
	for (;;) {
		slot = vm_radix_slot(index, rnode->rn_clev);
		tmp = vm_radix_node_load(&rnode->rn_child[slot], LOCKED);
		if (vm_radix_isleaf(tmp)) {
			m = vm_radix_topage(tmp);
			if (m->pindex == index) {
				vm_radix_node_store(&rnode->rn_child[slot],
				    (struct vm_radix_node *)((uintptr_t)newpage |
				    VM_RADIX_ISLEAF), LOCKED);
				return (m);
			} else
				break;
		} else if (tmp == NULL || vm_radix_keybarr(tmp, index))
			break;
		rnode = tmp;
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
	int i;

        if (!have_addr)
                return;
	rnode = (struct vm_radix_node *)addr;
	db_printf("radixnode %p, owner %jx, children count %u, level %u:\n",
	    (void *)rnode, (uintmax_t)rnode->rn_owner, rnode->rn_count,
	    rnode->rn_clev);
	for (i = 0; i < VM_RADIX_COUNT; i++) {
		tmp = vm_radix_node_load(&rnode->rn_child[i], UNSERIALIZED);
		if (tmp != NULL)
			db_printf("slot: %d, val: %p, page: %p, clev: %d\n",
			    i, (void *)tmp,
			    vm_radix_isleaf(tmp) ?  vm_radix_topage(tmp) : NULL,
			    rnode->rn_clev);
	}
}
#endif /* DDB */
