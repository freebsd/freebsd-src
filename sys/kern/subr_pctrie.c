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

/* Set of all flag bits stored in node pointers. */
#define	PCTRIE_FLAGS	(PCTRIE_ISLEAF)
#define	PCTRIE_PAD	PCTRIE_FLAGS

struct pctrie_node;
typedef SMR_POINTER(struct pctrie_node *) smr_pctnode_t;

struct pctrie_node {
	uint64_t	pn_owner;			/* Owner of record. */
	pn_popmap_t	pn_popmap;			/* Valid children. */
	uint8_t		pn_clev;			/* Level * WIDTH. */
	smr_pctnode_t	pn_child[PCTRIE_COUNT];		/* Child nodes. */
};

enum pctrie_access { PCTRIE_SMR, PCTRIE_LOCKED, PCTRIE_UNSERIALIZED };

static __inline void pctrie_node_store(smr_pctnode_t *p, void *val,
    enum pctrie_access access);

/*
 * Map index to an array position for the children of node,
 */
static __inline int
pctrie_slot(struct pctrie_node *node, uint64_t index)
{
	return ((index >> node->pn_clev) & PCTRIE_MASK);
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

/*
 * Allocate a node.  Pre-allocation should ensure that the request
 * will always be satisfied.
 */
static struct pctrie_node *
pctrie_node_get(struct pctrie *ptree, pctrie_alloc_t allocfn, uint64_t index,
    uint64_t newind)
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
		    PCTRIE_NULL, PCTRIE_UNSERIALIZED);
		node->pn_popmap = 0;
	}

	/*
	 * From the highest-order bit where the indexes differ,
	 * compute the highest level in the trie where they differ.  Then,
	 * compute the least index of this subtrie.
	 */
	KASSERT(index != newind, ("%s: passing the same key value %jx",
	    __func__, (uintmax_t)index));
	_Static_assert(sizeof(long long) >= sizeof(uint64_t),
	    "uint64 too wide");
	_Static_assert(sizeof(uint64_t) * NBBY <=
	    (1 << (sizeof(node->pn_clev) * NBBY)), "pn_clev too narrow");
	node->pn_clev = rounddown(flsll(index ^ newind) - 1, PCTRIE_WIDTH);
	node->pn_owner = PCTRIE_COUNT;
	node->pn_owner = index & -(node->pn_owner << node->pn_clev);
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
		    PCTRIE_NULL,
		    ("pctrie_node_put: node %p has a child", node));
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
		KASSERT(child != PCTRIE_NULL,
		    ("%s: bad popmap slot %d in node %p",
		    __func__, slot, node));
		if (!pctrie_isleaf(child))
			pctrie_reclaim_allnodes_int(ptree, child, freefn);
		node->pn_popmap ^= 1 << slot;
		pctrie_node_store(&node->pn_child[slot], PCTRIE_NULL,
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
 * Inserts the key-value pair into the trie.
 * Panics if the key already exists.
 */
int
pctrie_insert(struct pctrie *ptree, uint64_t *val, pctrie_alloc_t allocfn)
{
	uint64_t index, newind;
	struct pctrie_node *leaf, *node, *parent;
	smr_pctnode_t *parentp;
	int slot;

	index = *val;
	leaf = pctrie_toleaf(val);

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
					ptree->pt_root = leaf;
				else
					pctrie_addnode(parent, index, leaf,
					    PCTRIE_LOCKED);
				return (0);
			}
			newind = *pctrie_toval(node);
			if (newind == index)
				panic("%s: key %jx is already present",
				    __func__, (uintmax_t)index);
			break;
		}
		if (pctrie_keybarr(node, index, &slot)) {
			newind = node->pn_owner;
			break;
		}
		parent = node;
		node = pctrie_node_load(&node->pn_child[slot], NULL,
		    PCTRIE_LOCKED);
	}

	/*
	 * A new node is needed because the right insertion level is reached.
	 * Setup the new intermediate node and add the 2 children: the
	 * new object and the older edge or object.
	 */
	parentp = (parent != NULL) ? &parent->pn_child[slot]:
	    (smr_pctnode_t *)&ptree->pt_root;
	parent = pctrie_node_get(ptree, allocfn, index, newind);
	if (parent == NULL)
		return (ENOMEM);
	/* These writes are not yet visible due to ordering. */
	pctrie_addnode(parent, index, leaf, PCTRIE_UNSERIALIZED);
	pctrie_addnode(parent, newind, node, PCTRIE_UNSERIALIZED);
	/* Synchronize to make the above visible. */
	pctrie_node_store(parentp, parent, PCTRIE_LOCKED);
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
	for (;;) {
		if (pctrie_isleaf(node)) {
			if ((m = pctrie_toval(node)) != NULL && *m == index)
				return (m);
			break;
		}
		if (pctrie_keybarr(node, index, &slot))
			break;
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
 * Returns the value with the least index that is greater than or equal to the
 * specified index, or NULL if there are no such values.
 *
 * Requires that access be externally synchronized by a lock.
 */
uint64_t *
pctrie_lookup_ge(struct pctrie *ptree, uint64_t index)
{
	struct pctrie_node *node, *succ;
	uint64_t *m;
	int slot;

	/*
	 * Descend the trie as if performing an ordinary lookup for the
	 * specified value.  However, unlike an ordinary lookup, as we descend
	 * the trie, we use "succ" to remember the last branching-off point,
	 * that is, the interior node under which the least value that is both
	 * outside our current path down the trie and greater than the specified
	 * index resides.  (The node's popmap makes it fast and easy to
	 * recognize a branching-off point.)  If our ordinary lookup fails to
	 * yield a value that is greater than or equal to the specified index,
	 * then we will exit this loop and perform a lookup starting from
	 * "succ".  If "succ" is not NULL, then that lookup is guaranteed to
	 * succeed.
	 */
	node = pctrie_root_load(ptree, NULL, PCTRIE_LOCKED);
	succ = NULL;
	for (;;) {
		if (pctrie_isleaf(node)) {
			if ((m = pctrie_toval(node)) != NULL && *m >= index)
				return (m);
			break;
		}
		if (pctrie_keybarr(node, index, &slot)) {
			/*
			 * If all values in this subtree are > index, then the
			 * least value in this subtree is the answer.
			 */
			if (node->pn_owner > index)
				succ = node;
			break;
		}

		/*
		 * Just in case the next search step leads to a subtree of all
		 * values < index, check popmap to see if a next bigger step, to
		 * a subtree of all pages with values > index, is available.  If
		 * so, remember to restart the search here.
		 */
		if ((node->pn_popmap >> slot) > 1)
			succ = node;
		node = pctrie_node_load(&node->pn_child[slot], NULL,
		    PCTRIE_LOCKED);
	}

	/*
	 * Restart the search from the last place visited in the subtree that
	 * included some values > index, if there was such a place.
	 */
	if (succ == NULL)
		return (NULL);
	if (succ != node) {
		/*
		 * Take a step to the next bigger sibling of the node chosen
		 * last time.  In that subtree, all values > index.
		 */
		slot = pctrie_slot(succ, index) + 1;
		KASSERT((succ->pn_popmap >> slot) != 0,
		    ("%s: no popmap siblings past slot %d in node %p",
		    __func__, slot, succ));
		slot += ffs(succ->pn_popmap >> slot) - 1;
		succ = pctrie_node_load(&succ->pn_child[slot], NULL,
		    PCTRIE_LOCKED);
	}

	/* 
	 * Find the value in the subtree rooted at "succ" with the least index.
	 */
	while (!pctrie_isleaf(succ)) {
		KASSERT(succ->pn_popmap != 0,
		    ("%s: no popmap children in node %p",  __func__, succ));
		slot = ffs(succ->pn_popmap) - 1;
		succ = pctrie_node_load(&succ->pn_child[slot], NULL,
		    PCTRIE_LOCKED);
	}
	return (pctrie_toval(succ));
}

/*
 * Returns the value with the greatest index that is less than or equal to the
 * specified index, or NULL if there are no such values.
 *
 * Requires that access be externally synchronized by a lock.
 */
uint64_t *
pctrie_lookup_le(struct pctrie *ptree, uint64_t index)
{
	struct pctrie_node *node, *pred;
	uint64_t *m;
	int slot;

	/*
	 * Mirror the implementation of pctrie_lookup_ge, described above.
	 */
	node = pctrie_root_load(ptree, NULL, PCTRIE_LOCKED);
	pred = NULL;
	for (;;) {
		if (pctrie_isleaf(node)) {
			if ((m = pctrie_toval(node)) != NULL && *m <= index)
				return (m);
			break;
		}
		if (pctrie_keybarr(node, index, &slot)) {
			if (node->pn_owner < index)
				pred = node;
			break;
		}
		if ((node->pn_popmap & ((1 << slot) - 1)) != 0)
			pred = node;
		node = pctrie_node_load(&node->pn_child[slot], NULL,
		    PCTRIE_LOCKED);
	}
	if (pred == NULL)
		return (NULL);
	if (pred != node) {
		slot = pctrie_slot(pred, index);
		KASSERT((pred->pn_popmap & ((1 << slot) - 1)) != 0,
		    ("%s: no popmap siblings before slot %d in node %p",
		    __func__, slot, pred));
		slot = fls(pred->pn_popmap & ((1 << slot) - 1)) - 1;
		pred = pctrie_node_load(&pred->pn_child[slot], NULL,
		    PCTRIE_LOCKED);
	}
	while (!pctrie_isleaf(pred)) {
		KASSERT(pred->pn_popmap != 0,
		    ("%s: no popmap children in node %p",  __func__, pred));
		slot = fls(pred->pn_popmap) - 1;
		pred = pctrie_node_load(&pred->pn_child[slot], NULL,
		    PCTRIE_LOCKED);
	}
	return (pctrie_toval(pred));
}

/*
 * Remove the specified index from the tree.
 * Panics if the key is not present.
 */
void
pctrie_remove(struct pctrie *ptree, uint64_t index, pctrie_free_t freefn)
{
	struct pctrie_node *child, *node, *parent;
	uint64_t *m;
	int slot;

	node = NULL;
	child = pctrie_root_load(ptree, NULL, PCTRIE_LOCKED);
	for (;;) {
		if (pctrie_isleaf(child))
			break;
		parent = node;
		node = child;
		slot = pctrie_slot(node, index);
		child = pctrie_node_load(&node->pn_child[slot], NULL,
		    PCTRIE_LOCKED);
	}
	if ((m = pctrie_toval(child)) == NULL || *m != index)
		panic("%s: key not found", __func__);
	if (node == NULL) {
		pctrie_root_store(ptree, PCTRIE_NULL, PCTRIE_LOCKED);
		return;
	}
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
	if (parent == NULL)
		pctrie_root_store(ptree, child, PCTRIE_LOCKED);
	else {
		slot = pctrie_slot(parent, index);
		KASSERT(node ==
		    pctrie_node_load(&parent->pn_child[slot], NULL,
		    PCTRIE_LOCKED), ("%s: invalid child value", __func__));
		pctrie_node_store(&parent->pn_child[slot], child,
		    PCTRIE_LOCKED);
	}
	/*
	 * The child is still valid and we can not zero the
	 * pointer until all SMR references are gone.
	 */
	pctrie_node_put(ptree, node, freefn);
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
	if (root == PCTRIE_NULL)
		return;
	pctrie_root_store(ptree, PCTRIE_NULL, PCTRIE_UNSERIALIZED);
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
