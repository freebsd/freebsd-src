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

struct pctrie_node;
typedef SMR_POINTER(struct pctrie_node *) smr_pctnode_t;

struct pctrie_node {
	uint64_t	pn_owner;			/* Owner of record. */
	pn_popmap_t	pn_popmap;			/* Valid children. */
	uint8_t		pn_clev;			/* Level * WIDTH. */
	smr_pctnode_t	pn_parent;			/* Parent node. */
	smr_pctnode_t	pn_child[PCTRIE_COUNT];		/* Child nodes. */
};

/*
 * Map index to an array position for the children of node,
 */
static __inline int
pctrie_slot(struct pctrie_node *node, uint64_t index)
{
	return ((index >> node->pn_clev) & (PCTRIE_COUNT - 1));
}

/*
 * Returns true if index does not belong to the specified node.  Otherwise,
 * sets slot value, and returns false.
 */
static __inline bool
pctrie_keybarr(struct pctrie_node *node, uint64_t index, int *slot)
{
	index = (index - node->pn_owner) >> node->pn_clev;
	if (index >= PCTRIE_COUNT)
		return (true);
	*slot = index;
	return (false);
}

enum pctrie_access { PCTRIE_SMR, PCTRIE_LOCKED, PCTRIE_UNSERIALIZED };

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
 * Get the root address, cast to proper type for load/store.
 */
static __inline smr_pctnode_t *
pctrie_root(struct pctrie *ptree)
{
	return ((smr_pctnode_t *)&ptree->pt_root);
}

/*
 * Get the root node for a tree.
 */
static __inline struct pctrie_node *
pctrie_root_load(struct pctrie *ptree, smr_t smr, enum pctrie_access access)
{
	return (pctrie_node_load(pctrie_root(ptree), smr, access));
}

/*
 * Get the child of a node.
 */
static __inline smr_pctnode_t *
pctrie_child(struct pctrie *ptree, struct pctrie_node *node, uint64_t index)
{
	return (node == NULL ? pctrie_root(ptree) :
	    &node->pn_child[pctrie_slot(node, index)]);
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
 * Returns the associated pointer extracted from node and field offset.
 */
static __inline void *
pctrie_toptr(struct pctrie_node *node, int keyoff)
{
	return ((void *)(((uintptr_t)node & ~PCTRIE_FLAGS) - keyoff));
}

/*
 * Make 'parent' a parent of 'child'.
 */
static __inline void
pctrie_setparent(struct pctrie_node *child, struct pctrie_node *parent)
{
	pctrie_node_store(&child->pn_parent, parent, PCTRIE_UNSERIALIZED);
}

/*
 * Return the parent of 'node'.
 */
static __inline struct pctrie_node *
pctrie_parent(struct pctrie_node *node)
{
	return (pctrie_node_load(&node->pn_parent, NULL, PCTRIE_UNSERIALIZED));
}

/*
 * Make 'child' a child of 'node'.
 */
static __inline void
pctrie_addnode(struct pctrie_node *node, uint64_t index,
    struct pctrie_node *child, enum pctrie_access access)
{
	int slot;

	slot = pctrie_slot(node, index);
	pctrie_node_store(&node->pn_child[slot], child, access);
	node->pn_popmap ^= 1 << slot;
	KASSERT((node->pn_popmap & (1 << slot)) != 0,
	    ("%s: bad popmap slot %d in node %p", __func__, slot, node));
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
	for (int i = 0; i < nitems(node->pn_child); i++)
		pctrie_node_store(&node->pn_child[i], PCTRIE_NULL,
		    PCTRIE_UNSERIALIZED);
	return (0);
}

size_t
pctrie_node_size(void)
{

	return (sizeof(struct pctrie_node));
}

/*
 * Look for where to insert the key-value pair into the trie.  Complete the
 * insertion if it replaces a null leaf.  Return the insertion location if the
 * insertion needs to be completed by the caller; otherwise return NULL.
 *
 * If the key is already present in the trie, populate *found_out as if by
 * pctrie_lookup().
 */
static __always_inline void *
pctrie_insert_lookup_compound(struct pctrie *ptree, uint64_t *val,
    struct pctrie_node **parent_out, uint64_t **found_out)
{
	uint64_t index;
	struct pctrie_node *node, *parent;
	int slot;

	index = *val;

	/*
	 * The owner of record for root is not really important because it
	 * will never be used.
	 */
	node = pctrie_root_load(ptree, NULL, PCTRIE_LOCKED);
	parent = NULL;
	for (;;) {
		if (pctrie_isleaf(node)) {
			if (node == PCTRIE_NULL) {
				if (parent == NULL)
					pctrie_node_store(pctrie_root(ptree),
					    pctrie_toleaf(val), PCTRIE_LOCKED);
				else
					pctrie_addnode(parent, index,
					    pctrie_toleaf(val), PCTRIE_LOCKED);
				*parent_out = parent;
				return (NULL);
			}
			if (*pctrie_toval(node) == index) {
				*found_out = pctrie_toval(node);
				*parent_out = parent;
				return (NULL);
			}
			break;
		}
		if (pctrie_keybarr(node, index, &slot))
			break;
		parent = node;
		node = pctrie_node_load(&node->pn_child[slot], NULL,
		    PCTRIE_LOCKED);
	}

	/*
	 * 'node' must be replaced in the tree with a new branch node, with
	 * children 'node' and 'val'. Return the place that points to 'node'
	 * now, and will point to to the new branching node later.
	 */
	*parent_out = parent;
	return ((parent == NULL) ? pctrie_root(ptree): &parent->pn_child[slot]);
}

/*
 * Wrap pctrie_insert_lookup_compound to implement a strict insertion.  Panic
 * if the key already exists, and do not look for neighboring entries.
 */
void *
pctrie_insert_lookup_strict(struct pctrie *ptree, uint64_t *val,
    struct pctrie_node **parent_out)
{
	void *parentp;
	uint64_t *found;

	found = NULL;
	parentp = pctrie_insert_lookup_compound(ptree, val, parent_out,
	    &found);
	if (__predict_false(found != NULL))
		panic("%s: key %jx is already present", __func__,
		    (uintmax_t)*val);
	return (parentp);
}

/*
 * Wrap pctrie_insert_lookup_compound to implement find-or-insert.  Do not look
 * for neighboring entries.
 */
void *
pctrie_insert_lookup(struct pctrie *ptree, uint64_t *val,
    struct pctrie_node **parent_out, uint64_t **found_out)
{
	*found_out = NULL;
	return (pctrie_insert_lookup_compound(ptree, val, parent_out,
	    found_out));
}

/*
 * Inserts newly allocated node 'child' into trie at location 'parentp', with
 * parent 'parent' and two children, 'val' and whatever non-NULL node or leaf
 * was at 'parentp' to begin with.
 */
void
pctrie_insert_node(uint64_t *val, struct pctrie_node *parent, void *parentp,
    struct pctrie_node *child)
{
	struct pctrie_node *node;
	uint64_t index, newind;

	/*
	 * Clear the last child pointer of the newly allocated child.  We want
	 * to clear it after the final section has exited so lookup can not
	 * return false negatives.  It is done here because it will be
	 * cache-cold in the dtor callback.
	 */
	if (child->pn_popmap != 0) {
		pctrie_node_store(&child->pn_child[ffs(child->pn_popmap) - 1],
		    PCTRIE_NULL, PCTRIE_UNSERIALIZED);
		child->pn_popmap = 0;
	}

	/*
	 * Recover the values of the two children of the new child node.  If
	 * 'node' is not a leaf, this stores into 'newind' the 'owner' field,
	 * which must be first in the node.
	 */
	index = *val;
	node = pctrie_node_load(parentp, NULL, PCTRIE_UNSERIALIZED);
	pctrie_setparent(child, parent);
	if (!pctrie_isleaf(node))
		pctrie_setparent(node, child);
	newind = *pctrie_toval(node);

	/*
	 * From the highest-order bit where the indexes differ,
	 * compute the highest level in the trie where they differ.  Then,
	 * compute the least index of this subtrie.
	 */
	_Static_assert(sizeof(long long) >= sizeof(uint64_t),
	    "uint64 too wide");
	_Static_assert(sizeof(uint64_t) * NBBY <=
	    (1 << (sizeof(child->pn_clev) * NBBY)), "pn_clev too narrow");
	child->pn_clev = rounddown(ilog2(index ^ newind), PCTRIE_WIDTH);
	child->pn_owner = PCTRIE_COUNT;
	child->pn_owner = index & -(child->pn_owner << child->pn_clev);


	/* These writes are not yet visible due to ordering. */
	pctrie_addnode(child, index, pctrie_toleaf(val), PCTRIE_UNSERIALIZED);
	pctrie_addnode(child, newind, node, PCTRIE_UNSERIALIZED);
	/* Synchronize to make the above visible. */
	pctrie_node_store(parentp, child, PCTRIE_LOCKED);
}

/*
 * Return the value associated with the node, if the node is a leaf that matches
 * the index; otherwise NULL.
 */
static __always_inline uint64_t *
pctrie_match_value(struct pctrie_node *node, uint64_t index)
{
	uint64_t *m;

	if (!pctrie_isleaf(node) || (m = pctrie_toval(node)) == NULL ||
	    *m != index)
		m = NULL;
	return (m);
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
	int slot;

	node = pctrie_root_load(ptree, smr, access);
	/* Seek a node that matches index. */
	while (!pctrie_isleaf(node) && !pctrie_keybarr(node, index, &slot))
		node = pctrie_node_load(&node->pn_child[slot], smr, access);
	return (pctrie_match_value(node, index));
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
 * Returns the last node examined in the search for the index, and sets the
 * parent of that node.
 */
static __always_inline struct pctrie_node *
_pctrie_lookup_node(struct pctrie *ptree, struct pctrie_node *node,
    uint64_t index, struct pctrie_node **parent_out,
    smr_t smr, enum pctrie_access access)
{
	struct pctrie_node *parent;
	int slot;

	/*
	 * Climb the search path to find the lowest node from which to start the
	 * search for a value matching 'index'.
	 */
	while (node != NULL) {
		KASSERT(!powerof2(node->pn_popmap),
		    ("%s: freed node in iter path", __func__));
		if (!pctrie_keybarr(node, index, &slot))
			break;
		node = pctrie_parent(node);
	}

	if (node == NULL) {
		parent = NULL;
		node = pctrie_root_load(ptree, smr, access);
	} else {
		parent = node;
		node = pctrie_node_load(&node->pn_child[slot], smr, access);
	}

	/* Seek a node that matches index. */
	while (!pctrie_isleaf(node) && !pctrie_keybarr(node, index, &slot)) {
		parent = node;
		node = pctrie_node_load(&node->pn_child[slot], smr, access);
	}
	if (parent_out != NULL)
		*parent_out = parent;
	return (node);
}

/*
 * Returns the value stored at a given index value, possibly NULL.
 */
static __always_inline uint64_t *
_pctrie_iter_lookup(struct pctrie_iter *it, uint64_t index, smr_t smr,
    enum pctrie_access access)
{
	struct pctrie_node *node;

	it->index = index;
	node = _pctrie_lookup_node(it->ptree, it->node, index, &it->node,
	    smr, access);
	return (pctrie_match_value(node, index));
}

/*
 * Returns the value stored at a given index value, possibly NULL.
 */
uint64_t *
pctrie_iter_lookup(struct pctrie_iter *it, uint64_t index)
{
	return (_pctrie_iter_lookup(it, index, NULL, PCTRIE_LOCKED));
}

/*
 * Insert the val in the trie, starting search with iterator.  Return a pointer
 * to indicate where a new node must be allocated to complete insertion.
 * Assumes access is externally synchronized by a lock.
 */
void *
pctrie_iter_insert_lookup(struct pctrie_iter *it, uint64_t *val)
{
	struct pctrie_node *node;

	it->index = *val;
	node = _pctrie_lookup_node(it->ptree, it->node, *val, &it->node,
	    NULL, PCTRIE_LOCKED);
	if (node == PCTRIE_NULL) {
		if (it->node == NULL)
			pctrie_node_store(pctrie_root(it->ptree),
			    pctrie_toleaf(val), PCTRIE_LOCKED);
		else
			pctrie_addnode(it->node, it->index,
			    pctrie_toleaf(val), PCTRIE_LOCKED);
		return (NULL);
	}
	if (__predict_false(pctrie_match_value(node, it->index) != NULL))
		panic("%s: key %jx is already present", __func__,
		    (uintmax_t)it->index);

	/*
	 * 'node' must be replaced in the tree with a new branch node, with
	 * children 'node' and 'val'. Return the place that points to 'node'
	 * now, and will point to to the new branching node later.
	 */
	return (pctrie_child(it->ptree, it->node, it->index));
}

/*
 * Returns the value stored at a fixed offset from the current index value,
 * possibly NULL.
 */
static __always_inline uint64_t *
_pctrie_iter_stride(struct pctrie_iter *it, int stride, smr_t smr,
    enum pctrie_access access)
{
	uint64_t index = it->index + stride;

	/* Detect stride overflow. */
	if ((stride > 0) != (index > it->index))
		return (NULL);
	/* Detect crossing limit */
	if ((index < it->limit) != (it->index < it->limit))
		return (NULL);

	return (_pctrie_iter_lookup(it, index, smr, access));
}

/*
 * Returns the value stored at a fixed offset from the current index value,
 * possibly NULL.
 */
uint64_t *
pctrie_iter_stride(struct pctrie_iter *it, int stride)
{
	return (_pctrie_iter_stride(it, stride, NULL, PCTRIE_LOCKED));
}

/*
 * Returns the value stored at one more than the current index value, possibly
 * NULL, assuming access is externally synchronized by a lock.
 */
uint64_t *
pctrie_iter_next(struct pctrie_iter *it)
{
	return (_pctrie_iter_stride(it, 1, NULL, PCTRIE_LOCKED));
}

/*
 * Returns the value stored at one less than the current index value, possibly
 * NULL, assuming access is externally synchronized by a lock.
 */
uint64_t *
pctrie_iter_prev(struct pctrie_iter *it)
{
	return (_pctrie_iter_stride(it, -1, NULL, PCTRIE_LOCKED));
}

/*
 * Find first leaf >= index, and fill iter with the path to the parent of that
 * leaf.  Return NULL if there is no such leaf less than limit.
 */
static __inline uint64_t *
_pctrie_lookup_ge(struct pctrie *ptree, struct pctrie_node *node,
    uint64_t index, struct pctrie_node **parent_out, uint64_t limit)
{
	struct pctrie_node *parent;
	uint64_t *m;
	int slot;

	/* Seek a node that matches index. */
	node = _pctrie_lookup_node(ptree, node, index, &parent,
	    NULL, PCTRIE_LOCKED);

	/*
	 * If no such node was found, and instead this path leads only to nodes
	 * < index, back up to find a subtrie with the least value > index.
	 */
	if (node == PCTRIE_NULL || *pctrie_toval(node) < index) {
		/* Climb the path to find a node with a descendant > index. */
		for (node = parent; node != NULL; node = pctrie_parent(node)) {
			slot = pctrie_slot(node, index) + 1;
			if ((node->pn_popmap >> slot) != 0)
				break;
		}
		if (node == NULL) {
			if (parent_out != NULL)
				*parent_out = NULL;
			return (NULL);
		}

		/* Step to the least child with a descendant > index. */
		slot += ffs(node->pn_popmap >> slot) - 1;
		parent = node;
		node = pctrie_node_load(&node->pn_child[slot], NULL,
		    PCTRIE_LOCKED);
	}
	/* Descend to the least leaf of the subtrie. */
	while (!pctrie_isleaf(node)) {
		if (limit != 0 && node->pn_owner >= limit)
			return (NULL);
		slot = ffs(node->pn_popmap) - 1;
		parent = node;
		node = pctrie_node_load(&node->pn_child[slot], NULL,
		    PCTRIE_LOCKED);
	}
	if (parent_out != NULL)
		*parent_out = parent;
	m = pctrie_toval(node);
	if (limit != 0 && *m >= limit)
		return (NULL);
	return (m);
}

uint64_t *
pctrie_lookup_ge(struct pctrie *ptree, uint64_t index)
{
	return (_pctrie_lookup_ge(ptree, NULL, index, NULL, 0));
}

/*
 * Find first leaf >= index, and fill iter with the path to the parent of that
 * leaf.  Return NULL if there is no such leaf less than limit.
 */
uint64_t *
pctrie_iter_lookup_ge(struct pctrie_iter *it, uint64_t index)
{
	uint64_t *m;

	m = _pctrie_lookup_ge(it->ptree, it->node, index, &it->node, it->limit);
	if (m != NULL)
		it->index = *m;
	return (m);
}

/*
 * Find the first leaf with value at least 'jump' greater than the previous
 * leaf.  Return NULL if that value is >= limit.
 */
uint64_t *
pctrie_iter_jump_ge(struct pctrie_iter *it, int64_t jump)
{
	uint64_t index = it->index + jump;

	/* Detect jump overflow. */
	if ((jump > 0) != (index > it->index))
		return (NULL);
	if (it->limit != 0 && index >= it->limit)
		return (NULL);
	return (pctrie_iter_lookup_ge(it, index));
}

/*
 * Find first leaf <= index, and fill iter with the path to the parent of that
 * leaf.  Return NULL if there is no such leaf greater than limit.
 */
static __inline uint64_t *
_pctrie_lookup_le(struct pctrie *ptree, struct pctrie_node *node,
    uint64_t index, struct pctrie_node **parent_out, uint64_t limit)
{
	struct pctrie_node *parent;
	uint64_t *m;
	int slot;

	/* Seek a node that matches index. */
	node = _pctrie_lookup_node(ptree, node, index, &parent, NULL,
	    PCTRIE_LOCKED);

	/*
	 * If no such node was found, and instead this path leads only to nodes
	 * > index, back up to find a subtrie with the greatest value < index.
	 */
	if (node == PCTRIE_NULL || *pctrie_toval(node) > index) {
		/* Climb the path to find a node with a descendant < index. */
		for (node = parent; node != NULL; node = pctrie_parent(node)) {
			slot = pctrie_slot(node, index);
			if ((node->pn_popmap & ((1 << slot) - 1)) != 0)
				break;
		}
		if (node == NULL) {
			if (parent_out != NULL)
				*parent_out = NULL;
			return (NULL);
		}

		/* Step to the greatest child with a descendant < index. */
		slot = ilog2(node->pn_popmap & ((1 << slot) - 1));
		parent = node;
		node = pctrie_node_load(&node->pn_child[slot], NULL,
		    PCTRIE_LOCKED);
	}
	/* Descend to the greatest leaf of the subtrie. */
	while (!pctrie_isleaf(node)) {
		if (limit != 0 && limit >= node->pn_owner +
		    ((uint64_t)PCTRIE_COUNT << node->pn_clev) - 1)
			return (NULL);
		slot = ilog2(node->pn_popmap);
		parent = node;
		node = pctrie_node_load(&node->pn_child[slot], NULL,
		    PCTRIE_LOCKED);
	}
	if (parent_out != NULL)
		*parent_out = parent;
	m = pctrie_toval(node);
	if (limit != 0 && *m <= limit)
		return (NULL);
	return (m);
}

uint64_t *
pctrie_lookup_le(struct pctrie *ptree, uint64_t index)
{
	return (_pctrie_lookup_le(ptree, NULL, index, NULL, 0));
}

uint64_t *
pctrie_subtree_lookup_lt(struct pctrie *ptree, struct pctrie_node *node,
    uint64_t index)
{
	if (index == 0)
		return (NULL);
	return (_pctrie_lookup_le(ptree, node, index - 1, NULL, 0));
}

/*
 * Find first leaf <= index, and fill iter with the path to the parent of that
 * leaf.  Return NULL if there is no such leaf greater than limit.
 */
uint64_t *
pctrie_iter_lookup_le(struct pctrie_iter *it, uint64_t index)
{
	uint64_t *m;

	m = _pctrie_lookup_le(it->ptree, it->node, index, &it->node, it->limit);
	if (m != NULL)
		it->index = *m;
	return (m);
}

/*
 * Find the first leaf with value at most 'jump' less than the previous
 * leaf.  Return NULL if that value is <= limit.
 */
uint64_t *
pctrie_iter_jump_le(struct pctrie_iter *it, int64_t jump)
{
	uint64_t index = it->index - jump;

	/* Detect jump overflow. */
	if ((jump > 0) != (index < it->index))
		return (NULL);
	if (it->limit != 0 && index <= it->limit)
		return (NULL);
	return (pctrie_iter_lookup_le(it, index));
}

/*
 * Remove the non-NULL child identified by 'index' from the set of children of
 * 'node'.  If doing so causes 'node' to have only one child, purge it from the
 * pctrie and save it in *freenode for later disposal.
 */
static void
pctrie_remove(struct pctrie *ptree, struct pctrie_node *node, uint64_t index,
    struct pctrie_node **freenode)
{
	struct pctrie_node *child;
	int slot;

	*freenode = NULL;
	if (node == NULL) {
		pctrie_node_store(pctrie_root(ptree),
		    PCTRIE_NULL, PCTRIE_LOCKED);
		return;
	}
	slot = pctrie_slot(node, index);
	KASSERT((node->pn_popmap & (1 << slot)) != 0,
	    ("%s: bad popmap slot %d in node %p",
	    __func__, slot, node));
	node->pn_popmap ^= 1 << slot;
	pctrie_node_store(&node->pn_child[slot], PCTRIE_NULL, PCTRIE_LOCKED);
	if (!powerof2(node->pn_popmap))
		return;
	KASSERT(node->pn_popmap != 0, ("%s: bad popmap all zeroes", __func__));
	slot = ffs(node->pn_popmap) - 1;
	child = pctrie_node_load(&node->pn_child[slot], NULL, PCTRIE_LOCKED);
	KASSERT(child != PCTRIE_NULL,
	    ("%s: bad popmap slot %d in node %p", __func__, slot, node));
	*freenode = node;
	node = pctrie_parent(node);
	if (!pctrie_isleaf(child))
		pctrie_setparent(child, node);
	pctrie_node_store(pctrie_child(ptree, node, index), child,
	    PCTRIE_LOCKED);
}

/*
 * Remove the specified index from the tree, and return the value stored at
 * that index.  If the index is not present, return NULL.
 */
uint64_t *
pctrie_remove_lookup(struct pctrie *ptree, uint64_t index,
    struct pctrie_node **freenode)
{
	struct pctrie_node *child, *node;
	uint64_t *m;
	int slot;

	node = NULL;
	child = pctrie_root_load(ptree, NULL, PCTRIE_LOCKED);
	while (!pctrie_isleaf(child)) {
		node = child;
		slot = pctrie_slot(node, index);
		child = pctrie_node_load(&node->pn_child[slot], NULL,
		    PCTRIE_LOCKED);
	}
	if ((m = pctrie_match_value(child, index)) != NULL)
		pctrie_remove(ptree, node, index, freenode);
	else
		*freenode = NULL;
	return (m);
}

/*
 * Remove from the trie the leaf last chosen by the iterator, and
 * adjust the path if it's last member is to be freed.
 */
void
pctrie_iter_remove(struct pctrie_iter *it, struct pctrie_node **freenode)
{
	KASSERT(NULL != pctrie_match_value(pctrie_node_load(pctrie_child(
	    it->ptree, it->node, it->index), NULL, PCTRIE_LOCKED), it->index),
	    ("%s: removing value %jx not at iter", __func__,
	    (uintmax_t)it->index));
	pctrie_remove(it->ptree, it->node, it->index, freenode);
	if (*freenode != NULL)
		it->node = pctrie_parent(it->node);
}

/*
 * Return the current leaf, assuming access is externally synchronized by a
 * lock.
 */
uint64_t *
pctrie_iter_value(struct pctrie_iter *it)
{
	struct pctrie_node *node;

	node = pctrie_node_load(pctrie_child(it->ptree, it->node, it->index),
	    NULL, PCTRIE_LOCKED);
	return (pctrie_toval(node));
}

/*
 * Walk the subtrie rooted at *pnode in order, invoking callback on leaves,
 * until an interior node is stripped of all children, and returned for
 * deallocation, with *pnode left pointing to the parent of that node.
 */
static __always_inline struct pctrie_node *
pctrie_reclaim_prune(struct pctrie_node **pnode, struct pctrie_node *parent,
    pctrie_cb_t callback, int keyoff, void *arg)
{
	struct pctrie_node *child, *node;
	int slot;

	node = *pnode;
	while (node->pn_popmap != 0) {
		slot = ffs(node->pn_popmap) - 1;
		node->pn_popmap ^= 1 << slot;
		child = pctrie_node_load(&node->pn_child[slot], NULL,
		    PCTRIE_UNSERIALIZED);
		pctrie_node_store(&node->pn_child[slot], PCTRIE_NULL,
		    PCTRIE_UNSERIALIZED);
		if (pctrie_isleaf(child)) {
			if (callback != NULL)
				callback(pctrie_toptr(child, keyoff), arg);
			continue;
		}
		/* Climb one level down the trie. */
		parent = node;
		node = child;
	}
	*pnode = parent;
	return (node);
}

/*
 * Recover the node parent from its first child and continue pruning.
 */
static __always_inline struct pctrie_node *
pctrie_reclaim_resume_compound(struct pctrie_node **pnode,
    pctrie_cb_t callback, int keyoff, void *arg)
{
	if (*pnode == NULL)
		return (NULL);
	/* Climb one level up the trie. */
	return (pctrie_reclaim_prune(pnode, pctrie_parent(*pnode), callback,
	    keyoff, arg));
}

/*
 * Find the trie root, and start pruning with a NULL parent.
 */
static __always_inline struct pctrie_node *
pctrie_reclaim_begin_compound(struct pctrie_node **pnode,
    struct pctrie *ptree,
    pctrie_cb_t callback, int keyoff, void *arg)
{
	struct pctrie_node *node;

	node = pctrie_root_load(ptree, NULL, PCTRIE_UNSERIALIZED);
	pctrie_node_store(pctrie_root(ptree), PCTRIE_NULL, PCTRIE_UNSERIALIZED);
	if (pctrie_isleaf(node)) {
		if (callback != NULL && node != PCTRIE_NULL)
			callback(pctrie_toptr(node, keyoff), arg);
		return (NULL);
	}
	*pnode = node;
	return (pctrie_reclaim_prune(pnode, NULL, callback, keyoff, arg));
}

struct pctrie_node *
pctrie_reclaim_resume(struct pctrie_node **pnode)
{
	return (pctrie_reclaim_resume_compound(pnode, NULL, 0, NULL));
}

struct pctrie_node *
pctrie_reclaim_begin(struct pctrie_node **pnode, struct pctrie *ptree)
{
	return (pctrie_reclaim_begin_compound(pnode, ptree, NULL, 0, NULL));
}

struct pctrie_node *
pctrie_reclaim_resume_cb(struct pctrie_node **pnode,
    pctrie_cb_t callback, int keyoff, void *arg)
{
	return (pctrie_reclaim_resume_compound(pnode, callback, keyoff, arg));
}

struct pctrie_node *
pctrie_reclaim_begin_cb(struct pctrie_node **pnode, struct pctrie *ptree,
    pctrie_cb_t callback, int keyoff, void *arg)
{
	return (pctrie_reclaim_begin_compound(pnode, ptree,
	    callback, keyoff, arg));
}

/*
 * Replace an existing value in the trie with another one.
 * Panics if there is not an old value in the trie at the new value's index.
 */
uint64_t *
pctrie_replace(struct pctrie *ptree, uint64_t *newval)
{
	struct pctrie_node *leaf, *parent, *node;
	uint64_t *m;
	uint64_t index;
	int slot;

	leaf = pctrie_toleaf(newval);
	index = *newval;
	node = pctrie_root_load(ptree, NULL, PCTRIE_LOCKED);
	parent = NULL;
	for (;;) {
		if (pctrie_isleaf(node)) {
			if ((m = pctrie_toval(node)) != NULL && *m == index) {
				if (parent == NULL)
					pctrie_node_store(pctrie_root(ptree),
					    leaf, PCTRIE_LOCKED);
				else
					pctrie_node_store(
					    &parent->pn_child[slot], leaf,
					    PCTRIE_LOCKED);
				return (m);
			}
			break;
		}
		if (pctrie_keybarr(node, index, &slot))
			break;
		parent = node;
		node = pctrie_node_load(&node->pn_child[slot], NULL,
		    PCTRIE_LOCKED);
	}
	panic("%s: original replacing value not found", __func__);
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
	    node->pn_clev / PCTRIE_WIDTH);
	for (popmap = node->pn_popmap; popmap != 0; popmap ^= 1 << slot) {
		slot = ffs(popmap) - 1;
		tmp = pctrie_node_load(&node->pn_child[slot], NULL,
		    PCTRIE_UNSERIALIZED);
		db_printf("slot: %d, val: %p, value: %p, clev: %d\n",
		    slot, (void *)tmp,
		    pctrie_isleaf(tmp) ? pctrie_toval(tmp) : NULL,
		    node->pn_clev / PCTRIE_WIDTH);
	}
}
#endif /* DDB */
