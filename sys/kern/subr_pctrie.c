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
#include <sys/pctrie.h>
#include <sys/proc.h>	/* smr.h depends on struct thread. */
#include <sys/smr.h>
#include <sys/smr_types.h>

#ifdef DDB
#include <ddb/ddb.h>
#endif

#define	PCTRIE_MASK	(PCTRIE_COUNT - 1)
#define	PCTRIE_LIMIT	(howmany(sizeof(uint64_t) * NBBY, PCTRIE_WIDTH) - 1)

#if PCTRIE_WIDTH == 3
typedef uint8_t pn_popmap_t;
#elif PCTRIE_WIDTH == 4
typedef uint16_t pn_popmap_t;
#elif PCTRIE_WIDTH == 5
typedef uint32_t pn_popmap_t;
#else
#error Unsupported width
#endif
_Static_assert(sizeof(pn_popmap_t) <= sizeof(int),
    "pn_popmap_t too wide");

/* Flag bits stored in node pointers. */
#define	PCTRIE_ISLEAF	0x1
#define	PCTRIE_FLAGS	0x1
#define	PCTRIE_PAD	PCTRIE_FLAGS

/* Returns one unit associated with specified level. */
#define	PCTRIE_UNITLEVEL(lev)						\
	((uint64_t)1 << ((lev) * PCTRIE_WIDTH))

struct pctrie_node;
typedef SMR_POINTER(struct pctrie_node *) smr_pctnode_t;

struct pctrie_node {
	uint64_t	pn_owner;			/* Owner of record. */
	pn_popmap_t	pn_popmap;			/* Valid children. */
	uint8_t		pn_clev;			/* Current level. */
	smr_pctnode_t	pn_child[PCTRIE_COUNT];		/* Child nodes. */
};

enum pctrie_access { PCTRIE_SMR, PCTRIE_LOCKED, PCTRIE_UNSERIALIZED };

static __inline void pctrie_node_store(smr_pctnode_t *p, void *val,
    enum pctrie_access access);

/*
 * Return the position in the array for a given level.
 */
static __inline int
pctrie_slot(uint64_t index, uint16_t level)
{
	return ((index >> (level * PCTRIE_WIDTH)) & PCTRIE_MASK);
}

/* Computes the key (index) with the low-order 'level' radix-digits zeroed. */
static __inline uint64_t
pctrie_trimkey(uint64_t index, uint16_t level)
{
	return (index & -PCTRIE_UNITLEVEL(level));
}

/*
 * Allocate a node.  Pre-allocation should ensure that the request
 * will always be satisfied.
 */
static struct pctrie_node *
pctrie_node_get(struct pctrie *ptree, pctrie_alloc_t allocfn, uint64_t index,
    uint16_t clevel)
{
	struct pctrie_node *node;

	node = allocfn(ptree);
	if (node == NULL)
		return (NULL);

	/*
	 * We want to clear the last child pointer after the final section
	 * has exited so lookup can not return false negatives.  It is done
	 * here because it will be cache-cold in the dtor callback.
	 */
	if (node->pn_popmap != 0) {
		pctrie_node_store(&node->pn_child[ffs(node->pn_popmap) - 1],
		    NULL, PCTRIE_UNSERIALIZED);
		node->pn_popmap = 0;
	}
	node->pn_owner = pctrie_trimkey(index, clevel + 1);
	node->pn_clev = clevel;
	return (node);
}

/*
 * Free radix node.
 */
static __inline void
pctrie_node_put(struct pctrie *ptree, struct pctrie_node *node,
    pctrie_free_t freefn)
{
#ifdef INVARIANTS
	int slot;

	KASSERT(powerof2(node->pn_popmap),
	    ("pctrie_node_put: node %p has too many children %04x", node,
	    node->pn_popmap));
	for (slot = 0; slot < PCTRIE_COUNT; slot++) {
		if ((node->pn_popmap & (1 << slot)) != 0)
			continue;
		KASSERT(smr_unserialized_load(&node->pn_child[slot], true) ==
		    NULL, ("pctrie_node_put: node %p has a child", node));
	}
#endif
	freefn(ptree, node);
}

/*
 * Fetch a node pointer from a slot.
 */
static __inline struct pctrie_node *
pctrie_node_load(smr_pctnode_t *p, smr_t smr, enum pctrie_access access)
{
	switch (access) {
	case PCTRIE_UNSERIALIZED:
		return (smr_unserialized_load(p, true));
	case PCTRIE_LOCKED:
		return (smr_serialized_load(p, true));
	case PCTRIE_SMR:
		return (smr_entered_load(p, smr));
	}
	__assert_unreachable();
}

static __inline void
pctrie_node_store(smr_pctnode_t *p, void *v, enum pctrie_access access)
{
	switch (access) {
	case PCTRIE_UNSERIALIZED:
		smr_unserialized_store(p, v, true);
		break;
	case PCTRIE_LOCKED:
		smr_serialized_store(p, v, true);
		break;
	case PCTRIE_SMR:
		panic("%s: Not supported in SMR section.", __func__);
		break;
	default:
		__assert_unreachable();
		break;
	}
}

/*
 * Get the root node for a tree.
 */
static __inline struct pctrie_node *
pctrie_root_load(struct pctrie *ptree, smr_t smr, enum pctrie_access access)
{
	return (pctrie_node_load((smr_pctnode_t *)&ptree->pt_root, smr, access));
}

/*
 * Set the root node for a tree.
 */
static __inline void
pctrie_root_store(struct pctrie *ptree, struct pctrie_node *node,
    enum pctrie_access access)
{
	pctrie_node_store((smr_pctnode_t *)&ptree->pt_root, node, access);
}

/*
 * Returns TRUE if the specified node is a leaf and FALSE otherwise.
 */
static __inline bool
pctrie_isleaf(struct pctrie_node *node)
{

	return (((uintptr_t)node & PCTRIE_ISLEAF) != 0);
}

/*
 * Returns val with leaf bit set.
 */
static __inline void *
pctrie_toleaf(uint64_t *val)
{
	return ((void *)((uintptr_t)val | PCTRIE_ISLEAF));
}

/*
 * Returns the associated val extracted from node.
 */
static __inline uint64_t *
pctrie_toval(struct pctrie_node *node)
{

	return ((uint64_t *)((uintptr_t)node & ~PCTRIE_FLAGS));
}

/*
 * Make 'child' a child of 'node'.
 */
static __inline void
pctrie_addnode(struct pctrie_node *node, uint64_t index, uint16_t clev,
    struct pctrie_node *child, enum pctrie_access access)
{
	int slot;

	slot = pctrie_slot(index, clev);
	pctrie_node_store(&node->pn_child[slot], child, access);
	node->pn_popmap ^= 1 << slot;
	KASSERT((node->pn_popmap & (1 << slot)) != 0,
	    ("%s: bad popmap slot %d in node %p", __func__, slot, node));
}

/*
 * Returns the level where two keys differ.
 * It cannot accept 2 equal keys.
 */
static __inline uint16_t
pctrie_keydiff(uint64_t index1, uint64_t index2)
{

	KASSERT(index1 != index2, ("%s: passing the same key value %jx",
	    __func__, (uintmax_t)index1));
	CTASSERT(sizeof(long long) >= sizeof(uint64_t));

	/*
	 * From the highest-order bit where the indexes differ,
	 * compute the highest level in the trie where they differ.
	 */
	return ((flsll(index1 ^ index2) - 1) / PCTRIE_WIDTH);
}

/*
 * Returns TRUE if it can be determined that key does not belong to the
 * specified node.  Otherwise, returns FALSE.
 */
static __inline bool
pctrie_keybarr(struct pctrie_node *node, uint64_t idx)
{

	if (node->pn_clev < PCTRIE_LIMIT) {
		idx = pctrie_trimkey(idx, node->pn_clev + 1);
		return (idx != node->pn_owner);
	}
	return (false);
}

/*
 * Internal helper for pctrie_reclaim_allnodes().
 * This function is recursive.
 */
static void
pctrie_reclaim_allnodes_int(struct pctrie *ptree, struct pctrie_node *node,
    pctrie_free_t freefn)
{
	struct pctrie_node *child;
	int slot;

	while (node->pn_popmap != 0) {
		slot = ffs(node->pn_popmap) - 1;
		child = pctrie_node_load(&node->pn_child[slot], NULL,
		    PCTRIE_UNSERIALIZED);
		KASSERT(child != NULL, ("%s: bad popmap slot %d in node %p",
		    __func__, slot, node));
		if (!pctrie_isleaf(child))
			pctrie_reclaim_allnodes_int(ptree, child, freefn);
		node->pn_popmap ^= 1 << slot;
		pctrie_node_store(&node->pn_child[slot], NULL,
		    PCTRIE_UNSERIALIZED);
	}
	pctrie_node_put(ptree, node, freefn);
}

/*
 * pctrie node zone initializer.
 */
int
pctrie_zone_init(void *mem, int size __unused, int flags __unused)
{
	struct pctrie_node *node;

	node = mem;
	node->pn_popmap = 0;
	memset(node->pn_child, 0, sizeof(node->pn_child));
	return (0);
}

size_t
pctrie_node_size(void)
{

	return (sizeof(struct pctrie_node));
}

/*
 * Inserts the key-value pair into the trie.
 * Panics if the key already exists.
 */
int
pctrie_insert(struct pctrie *ptree, uint64_t *val, pctrie_alloc_t allocfn)
{
	uint64_t index, newind;
	struct pctrie_node *leaf, *node, *tmp;
	smr_pctnode_t *parentp;
	int slot;
	uint16_t clev;

	index = *val;
	leaf = pctrie_toleaf(val);

	/*
	 * The owner of record for root is not really important because it
	 * will never be used.
	 */
	node = pctrie_root_load(ptree, NULL, PCTRIE_LOCKED);
	if (node == NULL) {
		ptree->pt_root = (uintptr_t)leaf;
		return (0);
	}
	for (parentp = (smr_pctnode_t *)&ptree->pt_root;; node = tmp) {
		if (pctrie_isleaf(node)) {
			newind = *pctrie_toval(node);
			if (newind == index)
				panic("%s: key %jx is already present",
				    __func__, (uintmax_t)index);
			break;
		} else if (pctrie_keybarr(node, index)) {
			newind = node->pn_owner;
			break;
		}
		slot = pctrie_slot(index, node->pn_clev);
		parentp = &node->pn_child[slot];
		tmp = pctrie_node_load(parentp, NULL, PCTRIE_LOCKED);
		if (tmp == NULL) {
			pctrie_addnode(node, index, node->pn_clev, leaf,
			    PCTRIE_LOCKED);
			return (0);
		}
	}

	/*
	 * A new node is needed because the right insertion level is reached.
	 * Setup the new intermediate node and add the 2 children: the
	 * new object and the older edge or object.
	 */
	clev = pctrie_keydiff(newind, index);
	tmp = pctrie_node_get(ptree, allocfn, index, clev);
	if (tmp == NULL)
		return (ENOMEM);
	/* These writes are not yet visible due to ordering. */
	pctrie_addnode(tmp, index, clev, leaf, PCTRIE_UNSERIALIZED);
	pctrie_addnode(tmp, newind, clev, node, PCTRIE_UNSERIALIZED);
	/* Synchronize to make the above visible. */
	pctrie_node_store(parentp, tmp, PCTRIE_LOCKED);
	return (0);
}

/*
 * Returns the value stored at the index.  If the index is not present,
 * NULL is returned.
 */
static __always_inline uint64_t *
_pctrie_lookup(struct pctrie *ptree, uint64_t index, smr_t smr,
    enum pctrie_access access)
{
	struct pctrie_node *node;
	uint64_t *m;
	int slot;

	node = pctrie_root_load(ptree, smr, access);
	while (node != NULL) {
		if (pctrie_isleaf(node)) {
			m = pctrie_toval(node);
			if (*m == index)
				return (m);
			break;
		}
		if (pctrie_keybarr(node, index))
			break;
		slot = pctrie_slot(index, node->pn_clev);
		node = pctrie_node_load(&node->pn_child[slot], smr, access);
	}
	return (NULL);
}

/*
 * Returns the value stored at the index, assuming access is externally
 * synchronized by a lock.
 *
 * If the index is not present, NULL is returned.
 */
uint64_t *
pctrie_lookup(struct pctrie *ptree, uint64_t index)
{
	return (_pctrie_lookup(ptree, index, NULL, PCTRIE_LOCKED));
}

/*
 * Returns the value stored at the index without requiring an external lock.
 *
 * If the index is not present, NULL is returned.
 */
uint64_t *
pctrie_lookup_unlocked(struct pctrie *ptree, uint64_t index, smr_t smr)
{
	uint64_t *res;

	smr_enter(smr);
	res = _pctrie_lookup(ptree, index, smr, PCTRIE_SMR);
	smr_exit(smr);
	return (res);
}

/*
 * Look up the nearest entry at a position bigger than or equal to index,
 * assuming access is externally synchronized by a lock.
 */
uint64_t *
pctrie_lookup_ge(struct pctrie *ptree, uint64_t index)
{
	struct pctrie_node *stack[PCTRIE_LIMIT];
	uint64_t *m;
	struct pctrie_node *child, *node;
#ifdef INVARIANTS
	int loops = 0;
#endif
	unsigned tos;
	int slot;

	node = pctrie_root_load(ptree, NULL, PCTRIE_LOCKED);
	if (node == NULL)
		return (NULL);
	else if (pctrie_isleaf(node)) {
		m = pctrie_toval(node);
		if (*m >= index)
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
		if (pctrie_keybarr(node, index)) {
			if (index > node->pn_owner) {
ascend:
				KASSERT(++loops < 1000,
				    ("pctrie_lookup_ge: too many loops"));

				/*
				 * Pop nodes from the stack until either the
				 * stack is empty or a node that could have a
				 * matching descendant is found.
				 */
				do {
					if (tos == 0)
						return (NULL);
					node = stack[--tos];
				} while (pctrie_slot(index,
				    node->pn_clev) == (PCTRIE_COUNT - 1));

				/*
				 * The following computation cannot overflow
				 * because index's slot at the current level
				 * is less than PCTRIE_COUNT - 1.
				 */
				index = pctrie_trimkey(index,
				    node->pn_clev);
				index += PCTRIE_UNITLEVEL(node->pn_clev);
			} else
				index = node->pn_owner;
			KASSERT(!pctrie_keybarr(node, index),
			    ("pctrie_lookup_ge: keybarr failed"));
		}
		slot = pctrie_slot(index, node->pn_clev);
		child = pctrie_node_load(&node->pn_child[slot], NULL,
		    PCTRIE_LOCKED);
		if (pctrie_isleaf(child)) {
			m = pctrie_toval(child);
			if (*m >= index)
				return (m);
		} else if (child != NULL)
			goto descend;

		/* Find the first set bit beyond the first slot+1 bits. */
		slot = ffs(node->pn_popmap & (-2 << slot)) - 1;
		if (slot < 0) {
			/*
			 * A value or edge greater than the search slot is not
			 * found in the current node; ascend to the next
			 * higher-level node.
			 */
			goto ascend;
		}
		child = pctrie_node_load(&node->pn_child[slot],
		    NULL, PCTRIE_LOCKED);
		KASSERT(child != NULL, ("%s: bad popmap slot %d in node %p",
		    __func__, slot, node));
		if (pctrie_isleaf(child))
			return (pctrie_toval(child));
		index = pctrie_trimkey(index, node->pn_clev + 1) +
		    slot * PCTRIE_UNITLEVEL(node->pn_clev);
descend:
		KASSERT(node->pn_clev > 0,
		    ("pctrie_lookup_ge: pushing leaf's parent"));
		KASSERT(tos < PCTRIE_LIMIT,
		    ("pctrie_lookup_ge: stack overflow"));
		stack[tos++] = node;
		node = child;
	}
}

/*
 * Look up the nearest entry at a position less than or equal to index,
 * assuming access is externally synchronized by a lock.
 */
uint64_t *
pctrie_lookup_le(struct pctrie *ptree, uint64_t index)
{
	struct pctrie_node *stack[PCTRIE_LIMIT];
	uint64_t *m;
	struct pctrie_node *child, *node;
#ifdef INVARIANTS
	int loops = 0;
#endif
	unsigned tos;
	int slot;

	node = pctrie_root_load(ptree, NULL, PCTRIE_LOCKED);
	if (node == NULL)
		return (NULL);
	else if (pctrie_isleaf(node)) {
		m = pctrie_toval(node);
		if (*m <= index)
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
		if (pctrie_keybarr(node, index)) {
			if (index > node->pn_owner) {
				index = node->pn_owner + PCTRIE_COUNT *
				    PCTRIE_UNITLEVEL(node->pn_clev);
			} else {
ascend:
				KASSERT(++loops < 1000,
				    ("pctrie_lookup_le: too many loops"));

				/*
				 * Pop nodes from the stack until either the
				 * stack is empty or a node that could have a
				 * matching descendant is found.
				 */
				do {
					if (tos == 0)
						return (NULL);
					node = stack[--tos];
				} while (pctrie_slot(index,
				    node->pn_clev) == 0);

				/*
				 * The following computation cannot overflow
				 * because index's slot at the current level
				 * is greater than 0.
				 */
				index = pctrie_trimkey(index,
				    node->pn_clev);
			}
			index--;
			KASSERT(!pctrie_keybarr(node, index),
			    ("pctrie_lookup_le: keybarr failed"));
		}
		slot = pctrie_slot(index, node->pn_clev);
		child = pctrie_node_load(&node->pn_child[slot], NULL,
		    PCTRIE_LOCKED);
		if (pctrie_isleaf(child)) {
			m = pctrie_toval(child);
			if (*m <= index)
				return (m);
		} else if (child != NULL)
			goto descend;

		/* Find the last set bit among the first slot bits. */
		slot = fls(node->pn_popmap & ((1 << slot) - 1)) - 1;
		if (slot < 0) {
			/*
			 * A value or edge smaller than the search slot is not
			 * found in the current node; ascend to the next
			 * higher-level node.
			 */
			goto ascend;
		}
		child = pctrie_node_load(&node->pn_child[slot],
		    NULL, PCTRIE_LOCKED);
		if (pctrie_isleaf(child))
			return (pctrie_toval(child));
		index = pctrie_trimkey(index, node->pn_clev + 1) +
		    (slot + 1) * PCTRIE_UNITLEVEL(node->pn_clev) - 1;
descend:
		KASSERT(node->pn_clev > 0,
		    ("pctrie_lookup_le: pushing leaf's parent"));
		KASSERT(tos < PCTRIE_LIMIT,
		    ("pctrie_lookup_le: stack overflow"));
		stack[tos++] = node;
		node = child;
	}
}

/*
 * Remove the specified index from the tree.
 * Panics if the key is not present.
 */
void
pctrie_remove(struct pctrie *ptree, uint64_t index, pctrie_free_t freefn)
{
	struct pctrie_node *node, *parent, *tmp;
	uint64_t *m;
	int slot;

	node = pctrie_root_load(ptree, NULL, PCTRIE_LOCKED);
	if (pctrie_isleaf(node)) {
		m = pctrie_toval(node);
		if (*m != index)
			panic("%s: invalid key found", __func__);
		pctrie_root_store(ptree, NULL, PCTRIE_LOCKED);
		return;
	}
	parent = NULL;
	for (;;) {
		if (node == NULL)
			panic("pctrie_remove: impossible to locate the key");
		slot = pctrie_slot(index, node->pn_clev);
		tmp = pctrie_node_load(&node->pn_child[slot], NULL,
		    PCTRIE_LOCKED);
		if (pctrie_isleaf(tmp)) {
			m = pctrie_toval(tmp);
			if (*m != index)
				panic("%s: invalid key found", __func__);
			KASSERT((node->pn_popmap & (1 << slot)) != 0,
			    ("%s: bad popmap slot %d in node %p",
			    __func__, slot, node));
			node->pn_popmap ^= 1 << slot;
			pctrie_node_store(&node->pn_child[slot], NULL,
			    PCTRIE_LOCKED);
			if (!powerof2(node->pn_popmap))
				break;
			KASSERT(node->pn_popmap != 0,
			    ("%s: bad popmap all zeroes", __func__));
			slot = ffs(node->pn_popmap) - 1;
			tmp = pctrie_node_load(&node->pn_child[slot],
			    NULL, PCTRIE_LOCKED);
			KASSERT(tmp != NULL,
			    ("%s: bad popmap slot %d in node %p",
			    __func__, slot, node));
			if (parent == NULL)
				pctrie_root_store(ptree, tmp, PCTRIE_LOCKED);
			else {
				slot = pctrie_slot(index, parent->pn_clev);
				KASSERT(pctrie_node_load(
					&parent->pn_child[slot], NULL,
					PCTRIE_LOCKED) == node,
				    ("%s: invalid child value", __func__));
				pctrie_node_store(&parent->pn_child[slot], tmp,
				    PCTRIE_LOCKED);
			}
			/*
			 * The child is still valid and we can not zero the
			 * pointer until all SMR references are gone.
			 */
			pctrie_node_put(ptree, node, freefn);
			break;
		}
		parent = node;
		node = tmp;
	}
}

/*
 * Remove and free all the nodes from the tree.
 * This function is recursive but there is a tight control on it as the
 * maximum depth of the tree is fixed.
 */
void
pctrie_reclaim_allnodes(struct pctrie *ptree, pctrie_free_t freefn)
{
	struct pctrie_node *root;

	root = pctrie_root_load(ptree, NULL, PCTRIE_LOCKED);
	if (root == NULL)
		return;
	pctrie_root_store(ptree, NULL, PCTRIE_UNSERIALIZED);
	if (!pctrie_isleaf(root))
		pctrie_reclaim_allnodes_int(ptree, root, freefn);
}

#ifdef DDB
/*
 * Show details about the given node.
 */
DB_SHOW_COMMAND(pctrienode, db_show_pctrienode)
{
	struct pctrie_node *node, *tmp;
	int slot;
	pn_popmap_t popmap;

        if (!have_addr)
                return;
	node = (struct pctrie_node *)addr;
	db_printf("node %p, owner %jx, children popmap %04x, level %u:\n",
	    (void *)node, (uintmax_t)node->pn_owner, node->pn_popmap,
	    node->pn_clev);
	for (popmap = node->pn_popmap; popmap != 0; popmap ^= 1 << slot) {
		slot = ffs(popmap) - 1;
		tmp = pctrie_node_load(&node->pn_child[slot], NULL,
		    PCTRIE_UNSERIALIZED);
		db_printf("slot: %d, val: %p, value: %p, clev: %d\n",
		    slot, (void *)tmp,
		    pctrie_isleaf(tmp) ? pctrie_toval(tmp) : NULL,
		    node->pn_clev);
	}
}
#endif /* DDB */
