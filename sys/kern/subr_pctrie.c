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
	uint16_t	pn_count;			/* Valid children. */
	uint8_t		pn_clev;			/* Current level. */
	int8_t		pn_last;			/* Zero last ptr. */
	smr_pctnode_t	pn_child[PCTRIE_COUNT];		/* Child nodes. */
};

enum pctrie_access { PCTRIE_SMR, PCTRIE_LOCKED, PCTRIE_UNSERIALIZED };

static __inline void pctrie_node_store(smr_pctnode_t *p, void *val,
    enum pctrie_access access);

/*
 * Allocate a node.  Pre-allocation should ensure that the request
 * will always be satisfied.
 */
static struct pctrie_node *
pctrie_node_get(struct pctrie *ptree, pctrie_alloc_t allocfn, uint64_t owner,
    uint16_t count, uint16_t clevel)
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
	if (node->pn_last != 0) {
		pctrie_node_store(&node->pn_child[node->pn_last - 1], NULL,
		    PCTRIE_UNSERIALIZED);
		node->pn_last = 0;
	}
	node->pn_owner = owner;
	node->pn_count = count;
	node->pn_clev = clevel;
	return (node);
}

/*
 * Free radix node.
 */
static __inline void
pctrie_node_put(struct pctrie *ptree, struct pctrie_node *node,
    pctrie_free_t freefn, int8_t last)
{
#ifdef INVARIANTS
	int slot;

	KASSERT(node->pn_count == 0,
	    ("pctrie_node_put: node %p has %d children", node,
	    node->pn_count));
	for (slot = 0; slot < PCTRIE_COUNT; slot++) {
		if (slot == last)
			continue;
		KASSERT(smr_unserialized_load(&node->pn_child[slot], true) ==
		    NULL, ("pctrie_node_put: node %p has a child", node));
	}
#endif
	node->pn_last = last + 1;
	freefn(ptree, node);
}

/*
 * Return the position in the array for a given level.
 */
static __inline int
pctrie_slot(uint64_t index, uint16_t level)
{

	return ((index >> (level * PCTRIE_WIDTH)) & PCTRIE_MASK);
}

/* Trims the key after the specified level. */
static __inline uint64_t
pctrie_trimkey(uint64_t index, uint16_t level)
{
	uint64_t ret;

	ret = index;
	if (level > 0) {
		ret >>= level * PCTRIE_WIDTH;
		ret <<= level * PCTRIE_WIDTH;
	}
	return (ret);
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
 * Returns the associated val extracted from node.
 */
static __inline uint64_t *
pctrie_toval(struct pctrie_node *node)
{

	return ((uint64_t *)((uintptr_t)node & ~PCTRIE_FLAGS));
}

/*
 * Adds the val as a child of the provided node.
 */
static __inline void
pctrie_addval(struct pctrie_node *node, uint64_t index, uint16_t clev,
    uint64_t *val, enum pctrie_access access)
{
	int slot;

	slot = pctrie_slot(index, clev);
	pctrie_node_store(&node->pn_child[slot],
	    (void *)((uintptr_t)val | PCTRIE_ISLEAF), access);
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

	KASSERT(node->pn_count <= PCTRIE_COUNT,
	    ("pctrie_reclaim_allnodes_int: bad count in node %p", node));
	for (slot = 0; node->pn_count != 0; slot++) {
		child = pctrie_node_load(&node->pn_child[slot], NULL,
		    PCTRIE_UNSERIALIZED);
		if (child == NULL)
			continue;
		if (!pctrie_isleaf(child))
			pctrie_reclaim_allnodes_int(ptree, child, freefn);
		pctrie_node_store(&node->pn_child[slot], NULL,
		    PCTRIE_UNSERIALIZED);
		node->pn_count--;
	}
	pctrie_node_put(ptree, node, freefn, -1);
}

/*
 * pctrie node zone initializer.
 */
int
pctrie_zone_init(void *mem, int size __unused, int flags __unused)
{
	struct pctrie_node *node;

	node = mem;
	node->pn_last = 0;
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
	struct pctrie_node *node, *tmp;
	smr_pctnode_t *parentp;
	uint64_t *m;
	int slot;
	uint16_t clev;

	index = *val;

	/*
	 * The owner of record for root is not really important because it
	 * will never be used.
	 */
	node = pctrie_root_load(ptree, NULL, PCTRIE_LOCKED);
	if (node == NULL) {
		ptree->pt_root = (uintptr_t)val | PCTRIE_ISLEAF;
		return (0);
	}
	parentp = (smr_pctnode_t *)&ptree->pt_root;
	for (;;) {
		if (pctrie_isleaf(node)) {
			m = pctrie_toval(node);
			if (*m == index)
				panic("%s: key %jx is already present",
				    __func__, (uintmax_t)index);
			clev = pctrie_keydiff(*m, index);
			tmp = pctrie_node_get(ptree, allocfn,
			    pctrie_trimkey(index, clev + 1), 2, clev);
			if (tmp == NULL)
				return (ENOMEM);
			/* These writes are not yet visible due to ordering. */
			pctrie_addval(tmp, index, clev, val,
			    PCTRIE_UNSERIALIZED);
			pctrie_addval(tmp, *m, clev, m, PCTRIE_UNSERIALIZED);
			/* Synchronize to make leaf visible. */
			pctrie_node_store(parentp, tmp, PCTRIE_LOCKED);
			return (0);
		} else if (pctrie_keybarr(node, index))
			break;
		slot = pctrie_slot(index, node->pn_clev);
		parentp = &node->pn_child[slot];
		tmp = pctrie_node_load(parentp, NULL, PCTRIE_LOCKED);
		if (tmp == NULL) {
			node->pn_count++;
			pctrie_addval(node, index, node->pn_clev, val,
			    PCTRIE_LOCKED);
			return (0);
		}
		node = tmp;
	}

	/*
	 * A new node is needed because the right insertion level is reached.
	 * Setup the new intermediate node and add the 2 children: the
	 * new object and the older edge.
	 */
	newind = node->pn_owner;
	clev = pctrie_keydiff(newind, index);
	tmp = pctrie_node_get(ptree, allocfn,
	    pctrie_trimkey(index, clev + 1), 2, clev);
	if (tmp == NULL)
		return (ENOMEM);
	slot = pctrie_slot(newind, clev);
	/* These writes are not yet visible due to ordering. */
	pctrie_addval(tmp, index, clev, val, PCTRIE_UNSERIALIZED);
	pctrie_node_store(&tmp->pn_child[slot], node, PCTRIE_UNSERIALIZED);
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
	uint64_t inc;
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

		/*
		 * Look for an available edge or val within the current
		 * bisection node.
		 */
                if (slot < (PCTRIE_COUNT - 1)) {
			inc = PCTRIE_UNITLEVEL(node->pn_clev);
			index = pctrie_trimkey(index, node->pn_clev);
			do {
				index += inc;
				slot++;
				child = pctrie_node_load(&node->pn_child[slot],
				    NULL, PCTRIE_LOCKED);
				if (pctrie_isleaf(child)) {
					m = pctrie_toval(child);
					if (*m >= index)
						return (m);
				} else if (child != NULL)
					goto descend;
			} while (slot < (PCTRIE_COUNT - 1));
		}
		KASSERT(child == NULL || pctrie_isleaf(child),
		    ("pctrie_lookup_ge: child is radix node"));

		/*
		 * If a value or edge greater than the search slot is not found
		 * in the current node, ascend to the next higher-level node.
		 */
		goto ascend;
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
	uint64_t inc;
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

		/*
		 * Look for an available edge or value within the current
		 * bisection node.
		 */
		if (slot > 0) {
			inc = PCTRIE_UNITLEVEL(node->pn_clev);
			index |= inc - 1;
			do {
				index -= inc;
				slot--;
				child = pctrie_node_load(&node->pn_child[slot],
				    NULL, PCTRIE_LOCKED);
				if (pctrie_isleaf(child)) {
					m = pctrie_toval(child);
					if (*m <= index)
						return (m);
				} else if (child != NULL)
					goto descend;
			} while (slot > 0);
		}
		KASSERT(child == NULL || pctrie_isleaf(child),
		    ("pctrie_lookup_le: child is radix node"));

		/*
		 * If a value or edge smaller than the search slot is not found
		 * in the current node, ascend to the next higher-level node.
		 */
		goto ascend;
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
	int i, slot;

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
			pctrie_node_store(&node->pn_child[slot], NULL,
			    PCTRIE_LOCKED);
			node->pn_count--;
			if (node->pn_count > 1)
				break;
			for (i = 0; i < PCTRIE_COUNT; i++) {
				tmp = pctrie_node_load(&node->pn_child[i],
				    NULL, PCTRIE_LOCKED);
				if (tmp != NULL)
					break;
			}
			KASSERT(i != PCTRIE_COUNT,
			    ("%s: invalid node configuration", __func__));
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
			node->pn_count--;
			pctrie_node_put(ptree, node, freefn, i);
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
	int i;

        if (!have_addr)
                return;
	node = (struct pctrie_node *)addr;
	db_printf("node %p, owner %jx, children count %u, level %u:\n",
	    (void *)node, (uintmax_t)node->pn_owner, node->pn_count,
	    node->pn_clev);
	for (i = 0; i < PCTRIE_COUNT; i++) {
		tmp = pctrie_node_load(&node->pn_child[i], NULL,
		    PCTRIE_UNSERIALIZED);
		if (tmp != NULL)
			db_printf("slot: %d, val: %p, value: %p, clev: %d\n",
			    i, (void *)tmp,
			    pctrie_isleaf(tmp) ? pctrie_toval(tmp) : NULL,
			    node->pn_clev);
	}
}
#endif /* DDB */
