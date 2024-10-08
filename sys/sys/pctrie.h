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
 */

#ifndef _SYS_PCTRIE_H_
#define _SYS_PCTRIE_H_

#include <sys/_pctrie.h>
#include <sys/_smr.h>

#ifdef _KERNEL

typedef void (*pctrie_cb_t)(void *ptr, void *arg);

#define	PCTRIE_DEFINE_SMR(name, type, field, allocfn, freefn, smr)	\
    PCTRIE_DEFINE(name, type, field, allocfn, freefn)			\
									\
static __inline struct type *						\
name##_PCTRIE_LOOKUP_UNLOCKED(struct pctrie *ptree, uint64_t key)	\
{									\
									\
	return name##_PCTRIE_VAL2PTR(pctrie_lookup_unlocked(ptree,	\
	    key, (smr)));						\
}									\

#ifdef INVARIANTS
void		pctrie_subtree_lookup_gt_assert(struct pctrie_node *node,
		    uint64_t key, struct pctrie *ptree, uint64_t *res);
void		pctrie_subtree_lookup_lt_assert(struct pctrie_node *node,
		    uint64_t key, struct pctrie *ptree, uint64_t *res);
#else
#define	pctrie_subtree_lookup_gt_assert(node, key, ptree, res)
#define	pctrie_subtree_lookup_lt_assert(node, key, ptree, res)
#endif

#define	PCTRIE_DEFINE(name, type, field, allocfn, freefn)		\
									\
CTASSERT(sizeof(((struct type *)0)->field) == sizeof(uint64_t));	\
/*									\
 * XXX This assert protects flag bits, it does not enforce natural	\
 * alignment.  32bit architectures do not naturally align 64bit fields.	\
 */									\
CTASSERT((__offsetof(struct type, field) & (sizeof(uint32_t) - 1)) == 0); \
									\
static __inline struct type *						\
name##_PCTRIE_VAL2PTR(uint64_t *val)					\
{									\
									\
	if (val == NULL)						\
		return (NULL);						\
	return (struct type *)						\
	    ((uintptr_t)val - __offsetof(struct type, field));		\
}									\
									\
static __inline uint64_t *						\
name##_PCTRIE_PTR2VAL(struct type *ptr)					\
{									\
									\
	return &ptr->field;						\
}									\
									\
static __inline __unused int						\
name##_PCTRIE_INSERT(struct pctrie *ptree, struct type *ptr)		\
{									\
	struct pctrie_node *parent;					\
	void *parentp;							\
	uint64_t *val = name##_PCTRIE_PTR2VAL(ptr);			\
									\
	parentp = pctrie_insert_lookup_strict(ptree, val);		\
	if (parentp == NULL)						\
		return (0);						\
	parent = allocfn(ptree);					\
	if (__predict_false(parent == NULL))				\
		return (ENOMEM);					\
	pctrie_insert_node(parentp, parent, val);			\
	return (0);							\
}									\
									\
static __inline __unused int						\
name##_PCTRIE_FIND_OR_INSERT(struct pctrie *ptree, struct type *ptr,	\
    struct type **found_out_opt)					\
{									\
	struct pctrie_node *parent;					\
	void *parentp;							\
	uint64_t *val = name##_PCTRIE_PTR2VAL(ptr);			\
	uint64_t *found;						\
									\
	parentp = pctrie_insert_lookup(ptree, val, &found);		\
	if (found != NULL) {						\
		if (found_out_opt != NULL)				\
			*found_out_opt = name##_PCTRIE_VAL2PTR(found);	\
		return (EEXIST);					\
	}								\
	if (parentp == NULL)						\
		return (0);						\
	parent = allocfn(ptree);					\
	if (__predict_false(parent == NULL))				\
		return (ENOMEM);					\
	pctrie_insert_node(parentp, parent, val);			\
	return (0);							\
}									\
									\
static __inline __unused int						\
name##_PCTRIE_INSERT_LOOKUP_GE(struct pctrie *ptree, struct type *ptr,	\
    struct type **found_out)						\
{									\
	struct pctrie_node *parent, *neighbor;				\
	void *parentp;							\
	uint64_t *val = name##_PCTRIE_PTR2VAL(ptr);			\
	uint64_t *found;						\
									\
	parentp = pctrie_insert_lookup_gt(ptree, val, &found,		\
	    &neighbor);							\
	if (__predict_false(found != NULL)) {				\
		*found_out = name##_PCTRIE_VAL2PTR(found);		\
		return (EEXIST);					\
	}								\
	if (parentp != NULL) {						\
		parent = allocfn(ptree);				\
		if (__predict_false(parent == NULL)) {			\
			*found_out = NULL;				\
			return (ENOMEM);				\
		}							\
		if (neighbor == parentp)				\
			neighbor = parent;				\
		pctrie_insert_node(parentp, parent, val);		\
	}								\
	found = pctrie_subtree_lookup_gt(neighbor, *val);		\
	*found_out = name##_PCTRIE_VAL2PTR(found);			\
	pctrie_subtree_lookup_gt_assert(neighbor, *val, ptree, found);	\
	return (0);							\
}									\
									\
static __inline __unused int						\
name##_PCTRIE_INSERT_LOOKUP_LE(struct pctrie *ptree, struct type *ptr,	\
    struct type **found_out)						\
{									\
	struct pctrie_node *parent, *neighbor;				\
	void *parentp;							\
	uint64_t *val = name##_PCTRIE_PTR2VAL(ptr);			\
	uint64_t *found;						\
									\
	parentp = pctrie_insert_lookup_lt(ptree, val, &found,		\
	    &neighbor);							\
	if (__predict_false(found != NULL)) {				\
		*found_out = name##_PCTRIE_VAL2PTR(found);		\
		return (EEXIST);					\
	}								\
	if (parentp != NULL) {						\
		parent = allocfn(ptree);				\
		if (__predict_false(parent == NULL)) {			\
			*found_out = NULL;				\
			return (ENOMEM);				\
		}							\
		if (neighbor == parentp)				\
			neighbor = parent;				\
		pctrie_insert_node(parentp, parent, val);		\
	}								\
	found = pctrie_subtree_lookup_lt(neighbor, *val);		\
	*found_out = name##_PCTRIE_VAL2PTR(found);			\
	pctrie_subtree_lookup_lt_assert(neighbor, *val, ptree, found);	\
	return (0);							\
}									\
									\
static __inline __unused struct type *					\
name##_PCTRIE_LOOKUP(struct pctrie *ptree, uint64_t key)		\
{									\
									\
	return name##_PCTRIE_VAL2PTR(pctrie_lookup(ptree, key));	\
}									\
									\
static __inline __unused struct type *					\
name##_PCTRIE_LOOKUP_LE(struct pctrie *ptree, uint64_t key)		\
{									\
									\
	return name##_PCTRIE_VAL2PTR(pctrie_lookup_le(ptree, key));	\
}									\
									\
static __inline __unused struct type *					\
name##_PCTRIE_LOOKUP_GE(struct pctrie *ptree, uint64_t key)		\
{									\
									\
	return name##_PCTRIE_VAL2PTR(pctrie_lookup_ge(ptree, key));	\
}									\
									\
static __inline __unused void						\
name##_PCTRIE_RECLAIM(struct pctrie *ptree)				\
{									\
	struct pctrie_node *freenode, *node;				\
									\
	for (freenode = pctrie_reclaim_begin(&node, ptree);		\
	    freenode != NULL;						\
	    freenode = pctrie_reclaim_resume(&node))			\
		freefn(ptree, freenode);				\
}									\
									\
/*									\
 * While reclaiming all internal trie nodes, invoke callback(leaf, arg)	\
 * on every leaf in the trie, in order.					\
 */									\
static __inline __unused void						\
name##_PCTRIE_RECLAIM_CALLBACK(struct pctrie *ptree,			\
    void (*typed_cb)(struct type *, void *), void *arg)			\
{									\
	struct pctrie_node *freenode, *node;				\
	pctrie_cb_t callback = (pctrie_cb_t)typed_cb;			\
									\
	for (freenode = pctrie_reclaim_begin_cb(&node, ptree,		\
	    callback, __offsetof(struct type, field), arg);		\
	    freenode != NULL;						\
	    freenode = pctrie_reclaim_resume_cb(&node,			\
	    callback, __offsetof(struct type, field), arg))		\
		freefn(ptree, freenode);				\
}									\
									\
static __inline __unused int						\
name##_PCTRIE_ITER_INSERT(struct pctrie_iter *it, struct type *ptr)	\
{									\
	struct pctrie_node *parent;					\
	void *parentp;							\
	uint64_t *val = name##_PCTRIE_PTR2VAL(ptr);			\
									\
	parentp = pctrie_iter_insert_lookup(it, val);			\
	if (parentp == NULL)						\
		return (0);						\
	parent = allocfn(it->ptree);					\
	if (__predict_false(parent == NULL))				\
		return (ENOMEM);					\
	pctrie_insert_node(parentp, parent, val);			\
	it->path[it->top++] = parent;					\
	return (0);							\
}									\
									\
static __inline __unused struct type *					\
name##_PCTRIE_ITER_LOOKUP(struct pctrie_iter *it, uint64_t index)	\
{									\
	return name##_PCTRIE_VAL2PTR(pctrie_iter_lookup(it, index));	\
}									\
									\
static __inline __unused struct type *					\
name##_PCTRIE_ITER_STRIDE(struct pctrie_iter *it, int stride)		\
{									\
	return name##_PCTRIE_VAL2PTR(pctrie_iter_stride(it, stride));	\
}									\
									\
static __inline __unused struct type *					\
name##_PCTRIE_ITER_NEXT(struct pctrie_iter *it)				\
{									\
	return name##_PCTRIE_VAL2PTR(pctrie_iter_next(it));		\
}									\
									\
static __inline __unused struct type *					\
name##_PCTRIE_ITER_PREV(struct pctrie_iter *it)				\
{									\
	return name##_PCTRIE_VAL2PTR(pctrie_iter_prev(it));		\
}									\
									\
static __inline __unused struct type *					\
name##_PCTRIE_ITER_VALUE(struct pctrie_iter *it)			\
{									\
	return name##_PCTRIE_VAL2PTR(pctrie_iter_value(it));		\
}									\
									\
static __inline __unused struct type *					\
name##_PCTRIE_ITER_LOOKUP_GE(struct pctrie_iter *it, uint64_t index)	\
{									\
	return name##_PCTRIE_VAL2PTR(pctrie_iter_lookup_ge(it, index));	\
}									\
									\
static __inline __unused struct type *					\
name##_PCTRIE_ITER_JUMP_GE(struct pctrie_iter *it, int64_t jump)	\
{									\
	return name##_PCTRIE_VAL2PTR(pctrie_iter_jump_ge(it, jump));	\
}									\
									\
static __inline __unused struct type *					\
name##_PCTRIE_ITER_STEP_GE(struct pctrie_iter *it)			\
{									\
	return name##_PCTRIE_VAL2PTR(pctrie_iter_jump_ge(it, 1));	\
}									\
									\
static __inline __unused struct type *					\
name##_PCTRIE_ITER_LOOKUP_LE(struct pctrie_iter *it, uint64_t index)	\
{									\
	return name##_PCTRIE_VAL2PTR(pctrie_iter_lookup_le(it, index));	\
}									\
									\
static __inline __unused struct type *					\
name##_PCTRIE_ITER_JUMP_LE(struct pctrie_iter *it, int64_t jump)	\
{									\
	return name##_PCTRIE_VAL2PTR(pctrie_iter_jump_le(it, jump));	\
}									\
									\
static __inline __unused struct type *					\
name##_PCTRIE_ITER_STEP_LE(struct pctrie_iter *it)			\
{									\
	return name##_PCTRIE_VAL2PTR(pctrie_iter_jump_le(it, 1));	\
}									\
									\
static __inline __unused void						\
name##_PCTRIE_ITER_REMOVE(struct pctrie_iter *it)			\
{									\
	uint64_t *val;							\
	struct pctrie_node *freenode;					\
									\
	val = pctrie_iter_remove(it, &freenode);			\
	if (val == NULL)						\
		panic("%s: key not found", __func__);			\
	if (freenode != NULL)						\
		freefn(it->ptree, freenode);				\
}									\
									\
static __inline __unused struct type *					\
name##_PCTRIE_REPLACE(struct pctrie *ptree, struct type *ptr)		\
{									\
									\
	return name##_PCTRIE_VAL2PTR(					\
	    pctrie_replace(ptree, name##_PCTRIE_PTR2VAL(ptr)));		\
}									\
									\
static __inline __unused void						\
name##_PCTRIE_REMOVE(struct pctrie *ptree, uint64_t key)		\
{									\
	uint64_t *val;							\
	struct pctrie_node *freenode;					\
									\
	val = pctrie_remove_lookup(ptree, key, &freenode);		\
	if (val == NULL)						\
		panic("%s: key not found", __func__);			\
	if (freenode != NULL)						\
		freefn(ptree, freenode);				\
}									\
									\
static __inline __unused struct type *					\
name##_PCTRIE_REMOVE_LOOKUP(struct pctrie *ptree, uint64_t key)		\
{									\
	uint64_t *val;							\
	struct pctrie_node *freenode;					\
									\
	val = pctrie_remove_lookup(ptree, key, &freenode);		\
	if (freenode != NULL)						\
		freefn(ptree, freenode);				\
	return name##_PCTRIE_VAL2PTR(val);				\
}

struct pctrie_iter;
void		*pctrie_insert_lookup(struct pctrie *ptree, uint64_t *val,
		    uint64_t **found_out);
void		*pctrie_insert_lookup_gt(struct pctrie *ptree, uint64_t *val,
		    uint64_t **found_out, struct pctrie_node **neighbor_out);
void		*pctrie_insert_lookup_lt(struct pctrie *ptree, uint64_t *val,
		    uint64_t **found_out, struct pctrie_node **neighbor_out);
void		*pctrie_insert_lookup_strict(struct pctrie *ptree,
		    uint64_t *val);
void		pctrie_insert_node(void *parentp,
		    struct pctrie_node *parent, uint64_t *val);
uint64_t	*pctrie_lookup(struct pctrie *ptree, uint64_t key);
uint64_t	*pctrie_lookup_unlocked(struct pctrie *ptree, uint64_t key,
		    smr_t smr);
uint64_t	*pctrie_iter_lookup(struct pctrie_iter *it, uint64_t index);
uint64_t	*pctrie_iter_stride(struct pctrie_iter *it, int stride);
uint64_t	*pctrie_iter_next(struct pctrie_iter *it);
uint64_t	*pctrie_iter_prev(struct pctrie_iter *it);
void		*pctrie_iter_insert_lookup(struct pctrie_iter *it,
		    uint64_t *val);
uint64_t	*pctrie_lookup_ge(struct pctrie *ptree, uint64_t key);
uint64_t	*pctrie_subtree_lookup_gt(struct pctrie_node *node,
		    uint64_t key);
uint64_t	*pctrie_iter_lookup_ge(struct pctrie_iter *it, uint64_t index);
uint64_t	*pctrie_iter_jump_ge(struct pctrie_iter *it, int64_t jump);
uint64_t	*pctrie_lookup_le(struct pctrie *ptree, uint64_t key);
uint64_t	*pctrie_subtree_lookup_lt(struct pctrie_node *node,
		    uint64_t key);
uint64_t	*pctrie_iter_lookup_le(struct pctrie_iter *it, uint64_t index);
uint64_t	*pctrie_iter_jump_le(struct pctrie_iter *it, int64_t jump);
struct pctrie_node *pctrie_reclaim_begin(struct pctrie_node **pnode,
		    struct pctrie *ptree);
struct pctrie_node *pctrie_reclaim_resume(struct pctrie_node **pnode);
struct pctrie_node *pctrie_reclaim_begin_cb(struct pctrie_node **pnode,
		    struct pctrie *ptree,
		    pctrie_cb_t callback, int keyoff, void *arg);
struct pctrie_node *pctrie_reclaim_resume_cb(struct pctrie_node **pnode,
		    pctrie_cb_t callback, int keyoff, void *arg);
uint64_t	*pctrie_remove_lookup(struct pctrie *ptree, uint64_t index,
		    struct pctrie_node **killnode);
uint64_t	*pctrie_iter_remove(struct pctrie_iter *it,
		    struct pctrie_node **freenode);
uint64_t	*pctrie_iter_value(struct pctrie_iter *it);
uint64_t	*pctrie_replace(struct pctrie *ptree, uint64_t *newval);
size_t		pctrie_node_size(void);
int		pctrie_zone_init(void *mem, int size, int flags);

/*
 * Each search path in the trie terminates at a leaf, which is a pointer to a
 * value marked with a set 1-bit.  A leaf may be associated with a null pointer
 * to indicate no value there.
 */
#define	PCTRIE_ISLEAF	0x1
#define PCTRIE_NULL (struct pctrie_node *)PCTRIE_ISLEAF

static __inline void
pctrie_init(struct pctrie *ptree)
{
	ptree->pt_root = PCTRIE_NULL;
}

static __inline bool
pctrie_is_empty(struct pctrie *ptree)
{
	return (ptree->pt_root == PCTRIE_NULL);
}

/* Set of all flag bits stored in node pointers. */
#define	PCTRIE_FLAGS	(PCTRIE_ISLEAF)
/* Minimum align parameter for uma_zcreate. */
#define	PCTRIE_PAD	PCTRIE_FLAGS

/*
 * These widths should allow the pointers to a node's children to fit within
 * a single cache line.  The extra levels from a narrow width should not be
 * a problem thanks to path compression.
 */
#ifdef __LP64__
#define	PCTRIE_WIDTH	4
#else
#define	PCTRIE_WIDTH	3
#endif

#define	PCTRIE_COUNT	(1 << PCTRIE_WIDTH)
#define PCTRIE_LIMIT	howmany(sizeof(uint64_t) * NBBY, PCTRIE_WIDTH)

struct pctrie_iter {
	struct pctrie *ptree;
	struct pctrie_node *path[PCTRIE_LIMIT];
	uint64_t index;
	uint64_t limit;
	int top;
};

static __inline void
pctrie_iter_reset(struct pctrie_iter *it)
{
	it->top = 0;
}

static __inline void
pctrie_iter_init(struct pctrie_iter *it, struct pctrie *ptree)
{
	it->ptree = ptree;
	it->top = 0;
	it->limit = 0;
}

static __inline void
pctrie_iter_limit_init(struct pctrie_iter *it, struct pctrie *ptree,
    uint64_t limit)
{
	pctrie_iter_init(it, ptree);
	it->limit = limit;
}

#endif /* _KERNEL */
#endif /* !_SYS_PCTRIE_H_ */
