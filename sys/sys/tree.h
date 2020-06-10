/*	$NetBSD: tree.h,v 1.8 2004/03/28 19:38:30 provos Exp $	*/
/*	$OpenBSD: tree.h,v 1.7 2002/10/17 21:51:54 art Exp $	*/
/* $FreeBSD$ */

/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
 * Copyright 2002 Niels Provos <provos@citi.umich.edu>
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

#ifndef	_SYS_TREE_H_
#define	_SYS_TREE_H_

#include <sys/cdefs.h>

/*
 * This file defines data structures for different types of trees:
 * splay trees and red-black trees.
 *
 * A splay tree is a self-organizing data structure.  Every operation
 * on the tree causes a splay to happen.  The splay moves the requested
 * node to the root of the tree and partly rebalances it.
 *
 * This has the benefit that request locality causes faster lookups as
 * the requested nodes move to the top of the tree.  On the other hand,
 * every lookup causes memory writes.
 *
 * The Balance Theorem bounds the total access time for m operations
 * and n inserts on an initially empty tree as O((m + n)lg n).  The
 * amortized cost for a sequence of m accesses to a splay tree is O(lg n);
 *
 * A red-black tree is a binary search tree with the node color as an
 * extra attribute.  It fulfills a set of conditions:
 *	- every search path from the root to a leaf consists of the
 *	  same number of black nodes,
 *	- each red node (except for the root) has a black parent,
 *	- each leaf node is black.
 *
 * Every operation on a red-black tree is bounded as O(lg n).
 * The maximum height of a red-black tree is 2lg (n+1).
 */

#define SPLAY_HEAD(name, type)						\
struct name {								\
	struct type *sph_root; /* root of the tree */			\
}

#define SPLAY_INITIALIZER(root)						\
	{ NULL }

#define SPLAY_INIT(root) do {						\
	(root)->sph_root = NULL;					\
} while (/*CONSTCOND*/ 0)

#define SPLAY_ENTRY(type)						\
struct {								\
	struct type *spe_left; /* left element */			\
	struct type *spe_right; /* right element */			\
}

#define SPLAY_LEFT(elm, field)		(elm)->field.spe_left
#define SPLAY_RIGHT(elm, field)		(elm)->field.spe_right
#define SPLAY_ROOT(head)		(head)->sph_root
#define SPLAY_EMPTY(head)		(SPLAY_ROOT(head) == NULL)

/* SPLAY_ROTATE_{LEFT,RIGHT} expect that tmp hold SPLAY_{RIGHT,LEFT} */
#define SPLAY_ROTATE_RIGHT(head, tmp, field) do {			\
	SPLAY_LEFT((head)->sph_root, field) = SPLAY_RIGHT(tmp, field);	\
	SPLAY_RIGHT(tmp, field) = (head)->sph_root;			\
	(head)->sph_root = tmp;						\
} while (/*CONSTCOND*/ 0)
	
#define SPLAY_ROTATE_LEFT(head, tmp, field) do {			\
	SPLAY_RIGHT((head)->sph_root, field) = SPLAY_LEFT(tmp, field);	\
	SPLAY_LEFT(tmp, field) = (head)->sph_root;			\
	(head)->sph_root = tmp;						\
} while (/*CONSTCOND*/ 0)

#define SPLAY_LINKLEFT(head, tmp, field) do {				\
	SPLAY_LEFT(tmp, field) = (head)->sph_root;			\
	tmp = (head)->sph_root;						\
	(head)->sph_root = SPLAY_LEFT((head)->sph_root, field);		\
} while (/*CONSTCOND*/ 0)

#define SPLAY_LINKRIGHT(head, tmp, field) do {				\
	SPLAY_RIGHT(tmp, field) = (head)->sph_root;			\
	tmp = (head)->sph_root;						\
	(head)->sph_root = SPLAY_RIGHT((head)->sph_root, field);	\
} while (/*CONSTCOND*/ 0)

#define SPLAY_ASSEMBLE(head, node, left, right, field) do {		\
	SPLAY_RIGHT(left, field) = SPLAY_LEFT((head)->sph_root, field);	\
	SPLAY_LEFT(right, field) = SPLAY_RIGHT((head)->sph_root, field);\
	SPLAY_LEFT((head)->sph_root, field) = SPLAY_RIGHT(node, field);	\
	SPLAY_RIGHT((head)->sph_root, field) = SPLAY_LEFT(node, field);	\
} while (/*CONSTCOND*/ 0)

/* Generates prototypes and inline functions */

#define SPLAY_PROTOTYPE(name, type, field, cmp)				\
void name##_SPLAY(struct name *, struct type *);			\
void name##_SPLAY_MINMAX(struct name *, int);				\
struct type *name##_SPLAY_INSERT(struct name *, struct type *);		\
struct type *name##_SPLAY_REMOVE(struct name *, struct type *);		\
									\
/* Finds the node with the same key as elm */				\
static __unused __inline struct type *					\
name##_SPLAY_FIND(struct name *head, struct type *elm)			\
{									\
	if (SPLAY_EMPTY(head))						\
		return(NULL);						\
	name##_SPLAY(head, elm);					\
	if ((cmp)(elm, (head)->sph_root) == 0)				\
		return (head->sph_root);				\
	return (NULL);							\
}									\
									\
static __unused __inline struct type *					\
name##_SPLAY_NEXT(struct name *head, struct type *elm)			\
{									\
	name##_SPLAY(head, elm);					\
	if (SPLAY_RIGHT(elm, field) != NULL) {				\
		elm = SPLAY_RIGHT(elm, field);				\
		while (SPLAY_LEFT(elm, field) != NULL) {		\
			elm = SPLAY_LEFT(elm, field);			\
		}							\
	} else								\
		elm = NULL;						\
	return (elm);							\
}									\
									\
static __unused __inline struct type *					\
name##_SPLAY_MIN_MAX(struct name *head, int val)			\
{									\
	name##_SPLAY_MINMAX(head, val);					\
        return (SPLAY_ROOT(head));					\
}

/* Main splay operation.
 * Moves node close to the key of elm to top
 */
#define SPLAY_GENERATE(name, type, field, cmp)				\
struct type *								\
name##_SPLAY_INSERT(struct name *head, struct type *elm)		\
{									\
    if (SPLAY_EMPTY(head)) {						\
	    SPLAY_LEFT(elm, field) = SPLAY_RIGHT(elm, field) = NULL;	\
    } else {								\
	    int __comp;							\
	    name##_SPLAY(head, elm);					\
	    __comp = (cmp)(elm, (head)->sph_root);			\
	    if(__comp < 0) {						\
		    SPLAY_LEFT(elm, field) = SPLAY_LEFT((head)->sph_root, field);\
		    SPLAY_RIGHT(elm, field) = (head)->sph_root;		\
		    SPLAY_LEFT((head)->sph_root, field) = NULL;		\
	    } else if (__comp > 0) {					\
		    SPLAY_RIGHT(elm, field) = SPLAY_RIGHT((head)->sph_root, field);\
		    SPLAY_LEFT(elm, field) = (head)->sph_root;		\
		    SPLAY_RIGHT((head)->sph_root, field) = NULL;	\
	    } else							\
		    return ((head)->sph_root);				\
    }									\
    (head)->sph_root = (elm);						\
    return (NULL);							\
}									\
									\
struct type *								\
name##_SPLAY_REMOVE(struct name *head, struct type *elm)		\
{									\
	struct type *__tmp;						\
	if (SPLAY_EMPTY(head))						\
		return (NULL);						\
	name##_SPLAY(head, elm);					\
	if ((cmp)(elm, (head)->sph_root) == 0) {			\
		if (SPLAY_LEFT((head)->sph_root, field) == NULL) {	\
			(head)->sph_root = SPLAY_RIGHT((head)->sph_root, field);\
		} else {						\
			__tmp = SPLAY_RIGHT((head)->sph_root, field);	\
			(head)->sph_root = SPLAY_LEFT((head)->sph_root, field);\
			name##_SPLAY(head, elm);			\
			SPLAY_RIGHT((head)->sph_root, field) = __tmp;	\
		}							\
		return (elm);						\
	}								\
	return (NULL);							\
}									\
									\
void									\
name##_SPLAY(struct name *head, struct type *elm)			\
{									\
	struct type __node, *__left, *__right, *__tmp;			\
	int __comp;							\
\
	SPLAY_LEFT(&__node, field) = SPLAY_RIGHT(&__node, field) = NULL;\
	__left = __right = &__node;					\
\
	while ((__comp = (cmp)(elm, (head)->sph_root)) != 0) {		\
		if (__comp < 0) {					\
			__tmp = SPLAY_LEFT((head)->sph_root, field);	\
			if (__tmp == NULL)				\
				break;					\
			if ((cmp)(elm, __tmp) < 0){			\
				SPLAY_ROTATE_RIGHT(head, __tmp, field);	\
				if (SPLAY_LEFT((head)->sph_root, field) == NULL)\
					break;				\
			}						\
			SPLAY_LINKLEFT(head, __right, field);		\
		} else if (__comp > 0) {				\
			__tmp = SPLAY_RIGHT((head)->sph_root, field);	\
			if (__tmp == NULL)				\
				break;					\
			if ((cmp)(elm, __tmp) > 0){			\
				SPLAY_ROTATE_LEFT(head, __tmp, field);	\
				if (SPLAY_RIGHT((head)->sph_root, field) == NULL)\
					break;				\
			}						\
			SPLAY_LINKRIGHT(head, __left, field);		\
		}							\
	}								\
	SPLAY_ASSEMBLE(head, &__node, __left, __right, field);		\
}									\
									\
/* Splay with either the minimum or the maximum element			\
 * Used to find minimum or maximum element in tree.			\
 */									\
void name##_SPLAY_MINMAX(struct name *head, int __comp) \
{									\
	struct type __node, *__left, *__right, *__tmp;			\
\
	SPLAY_LEFT(&__node, field) = SPLAY_RIGHT(&__node, field) = NULL;\
	__left = __right = &__node;					\
\
	while (1) {							\
		if (__comp < 0) {					\
			__tmp = SPLAY_LEFT((head)->sph_root, field);	\
			if (__tmp == NULL)				\
				break;					\
			if (__comp < 0){				\
				SPLAY_ROTATE_RIGHT(head, __tmp, field);	\
				if (SPLAY_LEFT((head)->sph_root, field) == NULL)\
					break;				\
			}						\
			SPLAY_LINKLEFT(head, __right, field);		\
		} else if (__comp > 0) {				\
			__tmp = SPLAY_RIGHT((head)->sph_root, field);	\
			if (__tmp == NULL)				\
				break;					\
			if (__comp > 0) {				\
				SPLAY_ROTATE_LEFT(head, __tmp, field);	\
				if (SPLAY_RIGHT((head)->sph_root, field) == NULL)\
					break;				\
			}						\
			SPLAY_LINKRIGHT(head, __left, field);		\
		}							\
	}								\
	SPLAY_ASSEMBLE(head, &__node, __left, __right, field);		\
}

#define SPLAY_NEGINF	-1
#define SPLAY_INF	1

#define SPLAY_INSERT(name, x, y)	name##_SPLAY_INSERT(x, y)
#define SPLAY_REMOVE(name, x, y)	name##_SPLAY_REMOVE(x, y)
#define SPLAY_FIND(name, x, y)		name##_SPLAY_FIND(x, y)
#define SPLAY_NEXT(name, x, y)		name##_SPLAY_NEXT(x, y)
#define SPLAY_MIN(name, x)		(SPLAY_EMPTY(x) ? NULL	\
					: name##_SPLAY_MIN_MAX(x, SPLAY_NEGINF))
#define SPLAY_MAX(name, x)		(SPLAY_EMPTY(x) ? NULL	\
					: name##_SPLAY_MIN_MAX(x, SPLAY_INF))

#define SPLAY_FOREACH(x, name, head)					\
	for ((x) = SPLAY_MIN(name, head);				\
	     (x) != NULL;						\
	     (x) = SPLAY_NEXT(name, head, x))

/* Macros that define a red-black tree */
#define RB_HEAD(name, type)						\
struct name {								\
	struct type *rbh_root; /* root of the tree */			\
}

#define RB_INITIALIZER(root)						\
	{ NULL }

#define RB_INIT(root) do {						\
	(root)->rbh_root = NULL;					\
} while (/*CONSTCOND*/ 0)

#define RB_ENTRY(type)							\
struct {								\
	struct type *rbe_left;		/* left element */		\
	struct type *rbe_right;		/* right element */		\
	struct type *rbe_parent;	/* parent element */		\
}

#define RB_LF(elm, field)		(elm)->field.rbe_left
#define RB_RT(elm, field)		(elm)->field.rbe_right
#define RB_FLIP(elm)			(*(__uintptr_t *)&(elm) ^= 1)
#define RB_FLIP_LF(elm, field)		RB_FLIP(RB_LF(elm, field))
#define RB_FLIP_RT(elm, field)		RB_FLIP(RB_RT(elm, field))
#define RB_ISRED(elm)			((*(__uintptr_t *)&(elm) & 1) != 0)
#define RB_RED_LF(elm, field)		RB_ISRED(RB_LF(elm, field))
#define RB_RED_RT(elm, field)		RB_ISRED(RB_RT(elm, field))
#define RB_PTR(elm, field)		((__typeof(elm->field.rbe_parent)) \
					 ((__uintptr_t)(elm) & ~(__uintptr_t)1))
#define RB_LEFT(elm, field)		RB_PTR(RB_LF(elm, field), field)
#define RB_RIGHT(elm, field)		RB_PTR(RB_RT(elm, field), field)
#define RB_PARENT(elm, field)		(elm)->field.rbe_parent
#define RB_ROOT(head)			(head)->rbh_root
#define RB_EMPTY(head)			(RB_ROOT(head) == NULL)
#define RB_BOOL				int
#define RB_TRUE				1
#define RB_FALSE			0

/* For debugging support */
#define RB_COLOR(elm, field)		(RB_PARENT(elm, field) == NULL ? RB_FALSE : \
					    RB_LEFT(RB_PARENT(elm, field), field) == elm ? \
					    RB_RED_LF(RB_PARENT(elm, field), field) : \
					    RB_RED_RT(RB_PARENT(elm, field), field))

/*
 * Something to be invoked in a loop at the root of every modified subtree,
 * from the bottom up to the root, to update augmented node data.
 */
#ifndef RB_AUGMENT
#define RB_AUGMENT(x)	break
#endif

/*
 * Fix pointers to a parent, and from a parent, as part of rotation.
 */
#define RB_ROTATE_PARENT(head, elm, tmp, field) do {			\
	if ((RB_PARENT(tmp, field) = RB_PARENT(elm, field)) == NULL)    \
		RB_ROOT(head) = (tmp);					\
	else if ((elm) == RB_LEFT(RB_PARENT(elm, field), field))	\
		RB_LF(RB_PARENT(elm, field), field) = (tmp);		\
	else								\
		RB_RT(RB_PARENT(elm, field), field) = (tmp);		\
	RB_PARENT(elm, field) = (tmp);					\
} while (/*CONSTCOND*/ 0)

/*
 * Rotation makes the descending node red, and the ascending
 * node not-red.
 */
#define RB_ROTATE_LEFT(head, elm, tmp, field) do {			\
	(tmp) = RB_RIGHT(elm, field);					\
	if ((RB_RT(elm, field) = RB_LF(tmp, field)) != NULL) {		\
		RB_PARENT(RB_LEFT(tmp, field), field) = (elm);		\
	}								\
	RB_ROTATE_PARENT(head, elm, tmp, field);			\
	RB_LF(tmp, field) = (elm);					\
	RB_FLIP_LF(tmp, field);						\
	RB_AUGMENT(elm);						\
} while (/*CONSTCOND*/ 0)

#define RB_ROTATE_RIGHT(head, elm, tmp, field) do {			\
	(tmp) = RB_LEFT(elm, field);					\
	if ((RB_LF(elm, field) = RB_RT(tmp, field)) != NULL) {		\
		RB_PARENT(RB_RIGHT(tmp, field), field) = (elm);		\
	}								\
	RB_ROTATE_PARENT(head, elm, tmp, field);			\
	RB_RT(tmp, field) = (elm);					\
	RB_FLIP_RT(tmp, field);						\
	RB_AUGMENT(elm);						\
} while (/*CONSTCOND*/ 0)

/* Generates prototypes and inline functions */
#define	RB_PROTOTYPE(name, type, field, cmp)				\
	RB_PROTOTYPE_INTERNAL(name, type, field, cmp,)
#define	RB_PROTOTYPE_STATIC(name, type, field, cmp)			\
	RB_PROTOTYPE_INTERNAL(name, type, field, cmp, __unused static)
#define RB_PROTOTYPE_INTERNAL(name, type, field, cmp, attr)		\
	RB_PROTOTYPE_INSERT_COLOR(name, type, attr);			\
	RB_PROTOTYPE_REMOVE_COLOR(name, type, attr);			\
	RB_PROTOTYPE_INSERT(name, type, attr);				\
	RB_PROTOTYPE_REMOVE(name, type, attr);				\
	RB_PROTOTYPE_FIND(name, type, attr);				\
	RB_PROTOTYPE_NFIND(name, type, attr);				\
	RB_PROTOTYPE_NEXT(name, type, attr);				\
	RB_PROTOTYPE_PREV(name, type, attr);				\
	RB_PROTOTYPE_MINMAX(name, type, attr);				\
	RB_PROTOTYPE_REINSERT(name, type, attr);
#define RB_PROTOTYPE_INSERT_COLOR(name, type, attr)			\
	attr void name##_RB_INSERT_COLOR(struct name *, struct type *)
#define RB_PROTOTYPE_REMOVE_COLOR(name, type, attr)			\
	attr void name##_RB_REMOVE_COLOR(struct name *, struct type *)
#define RB_PROTOTYPE_REMOVE(name, type, attr)				\
	attr struct type *name##_RB_REMOVE(struct name *, struct type *)
#define RB_PROTOTYPE_INSERT(name, type, attr)				\
	attr struct type *name##_RB_INSERT(struct name *, struct type *)
#define RB_PROTOTYPE_FIND(name, type, attr)				\
	attr struct type *name##_RB_FIND(struct name *, struct type *)
#define RB_PROTOTYPE_NFIND(name, type, attr)				\
	attr struct type *name##_RB_NFIND(struct name *, struct type *)
#define RB_PROTOTYPE_NEXT(name, type, attr)				\
	attr struct type *name##_RB_NEXT(struct type *)
#define RB_PROTOTYPE_PREV(name, type, attr)				\
	attr struct type *name##_RB_PREV(struct type *)
#define RB_PROTOTYPE_MINMAX(name, type, attr)				\
	attr struct type *name##_RB_MINMAX(struct name *, int)
#define RB_PROTOTYPE_REINSERT(name, type, attr)			\
	attr struct type *name##_RB_REINSERT(struct name *, struct type *)

/* Main rb operation.
 * Moves node close to the key of elm to top
 */
#define	RB_GENERATE(name, type, field, cmp)				\
	RB_GENERATE_INTERNAL(name, type, field, cmp,)
#define	RB_GENERATE_STATIC(name, type, field, cmp)			\
	RB_GENERATE_INTERNAL(name, type, field, cmp, __unused static)
#define RB_GENERATE_INTERNAL(name, type, field, cmp, attr)		\
	RB_GENERATE_INSERT_COLOR(name, type, field, attr)		\
	RB_GENERATE_REMOVE_COLOR(name, type, field, attr)		\
	RB_GENERATE_INSERT(name, type, field, cmp, attr)		\
	RB_GENERATE_REMOVE(name, type, field, attr)			\
	RB_GENERATE_FIND(name, type, field, cmp, attr)			\
	RB_GENERATE_NFIND(name, type, field, cmp, attr)			\
	RB_GENERATE_NEXT(name, type, field, attr)			\
	RB_GENERATE_PREV(name, type, field, attr)			\
	RB_GENERATE_MINMAX(name, type, field, attr)			\
	RB_GENERATE_REINSERT(name, type, field, cmp, attr)


#define RB_GENERATE_INSERT_COLOR(name, type, field, attr)		\
attr void								\
name##_RB_INSERT_COLOR(struct name *head, struct type *elm)		\
{									\
	struct type *gparent, *parent;					\
	while ((parent = RB_PARENT(elm, field)) != NULL) {		\
		if (RB_LEFT(parent, field) == elm)			\
			RB_FLIP_LF(parent, field);			\
		else							\
			RB_FLIP_RT(parent, field);			\
		if ((gparent = RB_PARENT(parent, field)) == NULL)	\
			break;						\
		if (RB_RED_LF(gparent, field) &&			\
		    RB_RED_RT(gparent, field)) {			\
			RB_FLIP_LF(gparent, field);			\
			RB_FLIP_RT(gparent, field);			\
			elm = gparent;					\
			continue;					\
		}							\
		if (RB_RED_LF(gparent, field) &&			\
		    parent == RB_LEFT(gparent, field)) { 		\
			if (RB_RIGHT(parent, field) == elm) {		\
				RB_ROTATE_LEFT(head, parent, elm, field);\
				parent = elm;				\
			}						\
			RB_ROTATE_RIGHT(head, gparent, parent, field);	\
		} else if (RB_RED_RT(gparent, field) &&			\
		    parent == RB_RIGHT(gparent, field)) {		\
			if (RB_LEFT(parent, field) == elm) {		\
				RB_ROTATE_RIGHT(head, parent, elm, field);\
				parent = elm;				\
			}						\
			RB_ROTATE_LEFT(head, gparent, parent, field);	\
		}							\
		break;							\
	}								\
}

#define RB_GENERATE_REMOVE_COLOR(name, type, field, attr)		\
attr void								\
name##_RB_REMOVE_COLOR(struct name *head, struct type *elm)		\
{									\
	struct type *par, *sib, *tmp;					\
	RB_BOOL go_left, left_child, red_par;				\
	left_child = (RB_LEFT(elm, field) == NULL);			\
	do {								\
		go_left = left_child;					\
		if (go_left ?						\
		    !RB_RED_RT(elm, field) :				\
		    !RB_RED_LF(elm, field)) {				\
			par = RB_PARENT(elm, field);			\
			left_child = par != NULL &&			\
			    RB_LEFT(par, field) == elm;			\
			red_par = left_child ? RB_RED_LF(par, field) :	\
			  par == NULL ? RB_TRUE :			\
			  RB_RED_RT(par, field);			\
		}							\
		if (go_left) {						\
			if (RB_RED_RT(elm, field)) {			\
				red_par = RB_TRUE;			\
				RB_ROTATE_LEFT(head, elm, par, field);	\
			}						\
			sib = RB_RIGHT(elm, field);			\
			if (RB_RED_LF(sib, field)) {			\
				RB_ROTATE_RIGHT(head, sib, tmp, field);	\
				sib = tmp;				\
			} else if (!RB_RED_RT(sib, field)) {		\
				RB_FLIP_RT(elm, field);			\
				elm = par;				\
				continue;				\
			}						\
			if (RB_RED_RT(sib, field))			\
				RB_FLIP_RT(sib, field);			\
			RB_ROTATE_LEFT(head, elm, sib, field);		\
			RB_FLIP_LF(sib, field);				\
			break;						\
		} else {						\
			if (RB_RED_LF(elm, field)) {			\
				red_par = RB_TRUE;			\
				RB_ROTATE_RIGHT(head, elm, par, field); \
			}						\
			sib = RB_LEFT(elm, field);			\
			if (RB_RED_RT(sib, field)) {			\
				RB_ROTATE_LEFT(head, sib, tmp, field);	\
				sib = tmp;				\
			} else if (!RB_RED_LF(sib, field)) {		\
				RB_FLIP_LF(elm, field);			\
				elm = par;				\
				continue;				\
			}						\
			if (RB_RED_LF(sib, field))			\
				RB_FLIP_LF(sib, field);			\
			RB_ROTATE_RIGHT(head, elm, sib, field);		\
			RB_FLIP_RT(sib, field);				\
			break;						\
		}							\
	} while (!red_par);						\
	if (par != NULL && red_par) {					\
		if (left_child)						\
			RB_FLIP_LF(par, field);				\
		else							\
			RB_FLIP_RT(par, field);				\
	}								\
}

#define RB_GENERATE_REMOVE(name, type, field, attr)			\
attr struct type *							\
name##_RB_REMOVE(struct name *head, struct type *elm)			\
{									\
	struct type *child, *old, *parent, *parent_old, *right;		\
	RB_BOOL old_red, red;						\
									\
	old = elm;							\
	parent_old = parent = RB_PARENT(elm, field);			\
	right = RB_RIGHT(elm, field);					\
	if (RB_LEFT(elm, field) == NULL)				\
		elm = child = right;					\
	else if (right == NULL)						\
		elm = child = RB_LEFT(elm, field);			\
	else {								\
		if ((child = RB_LEFT(right, field)) == NULL) {		\
			child = RB_RIGHT(right, field);			\
			red = RB_RED_RT(old, field);			\
			RB_RT(old, field) = child;			\
			parent = elm = right;				\
		} else {						\
			do						\
				elm = child;				\
			while ((child = RB_LEFT(elm, field)) != NULL);	\
			child = RB_RIGHT(elm, field);			\
			parent = RB_PARENT(elm, field);			\
			red = RB_RED_LF(parent, field);			\
			RB_LF(parent, field) = child;			\
			RB_PARENT(RB_RIGHT(old, field), field) = elm;	\
		}							\
		RB_PARENT(RB_LEFT(old, field), field) = elm;		\
		elm->field = old->field;				\
	}								\
	if (parent_old == NULL)	{					\
		RB_ROOT(head) = elm;					\
		old_red = RB_FALSE;					\
	} else if (RB_LEFT(parent_old, field) == old) {			\
		old_red = RB_RED_LF(parent_old, field);			\
		RB_LF(parent_old, field) = elm;				\
		if (old_red && parent != parent_old)			\
			RB_FLIP_LF(parent_old, field);			\
	} else {							\
		old_red = RB_RED_RT(parent_old, field);			\
		RB_RT(parent_old, field) = elm;				\
		if (old_red && parent != parent_old)			\
			RB_FLIP_RT(parent_old, field);			\
	}								\
	if (child != NULL)						\
		RB_PARENT(child, field) = parent;			\
	else if (parent != NULL &&					\
		 (parent != parent_old ? !red : !old_red))		\
		name##_RB_REMOVE_COLOR(head, parent);			\
	while (parent != NULL) {					\
		RB_AUGMENT(parent);					\
		parent = RB_PARENT(parent, field);			\
	}								\
	return (old);							\
}

#define RB_GENERATE_INSERT(name, type, field, cmp, attr)		\
/* Inserts a node into the RB tree */					\
attr struct type *							\
name##_RB_INSERT(struct name *head, struct type *elm)			\
{									\
	struct type *tmp;						\
	struct type *parent = NULL;					\
	int comp = 0;							\
	tmp = RB_ROOT(head);						\
	while (tmp) {							\
		parent = tmp;						\
		comp = (cmp)(elm, parent);				\
		if (comp < 0)						\
			tmp = RB_LEFT(tmp, field);			\
		else if (comp > 0)					\
			tmp = RB_RIGHT(tmp, field);			\
		else							\
			return (tmp);					\
	}								\
	RB_PARENT(elm, field) = parent;					\
	RB_LF(elm, field) = RB_RT(elm, field) = NULL;			\
	if (parent == NULL)						\
		RB_ROOT(head) = elm;					\
	else if (comp < 0)						\
		RB_LF(parent, field) = elm;				\
	else								\
		RB_RT(parent, field) = elm;				\
	name##_RB_INSERT_COLOR(head, elm);				\
	while (elm != NULL) {						\
		RB_AUGMENT(elm);					\
		elm = RB_PARENT(elm, field);				\
	}								\
	return (NULL);							\
}

#define RB_GENERATE_FIND(name, type, field, cmp, attr)			\
/* Finds the node with the same key as elm */				\
attr struct type *							\
name##_RB_FIND(struct name *head, struct type *elm)			\
{									\
	struct type *tmp = RB_ROOT(head);				\
	int comp;							\
	while (tmp) {							\
		comp = cmp(elm, tmp);					\
		if (comp < 0)						\
			tmp = RB_LEFT(tmp, field);			\
		else if (comp > 0)					\
			tmp = RB_RIGHT(tmp, field);			\
		else							\
			return (tmp);					\
	}								\
	return (NULL);							\
}

#define RB_GENERATE_NFIND(name, type, field, cmp, attr)			\
/* Finds the first node greater than or equal to the search key */	\
attr struct type *							\
name##_RB_NFIND(struct name *head, struct type *elm)			\
{									\
	struct type *tmp = RB_ROOT(head);				\
	struct type *res = NULL;					\
	int comp;							\
	while (tmp) {							\
		comp = cmp(elm, tmp);					\
		if (comp < 0) {						\
			res = tmp;					\
			tmp = RB_LEFT(tmp, field);			\
		}							\
		else if (comp > 0)					\
			tmp = RB_RIGHT(tmp, field);			\
		else							\
			return (tmp);					\
	}								\
	return (res);							\
}

#define RB_GENERATE_NEXT(name, type, field, attr)			\
/* ARGSUSED */								\
attr struct type *							\
name##_RB_NEXT(struct type *elm)					\
{									\
	if (RB_RIGHT(elm, field)) {					\
		elm = RB_RIGHT(elm, field);				\
		while (RB_LEFT(elm, field))				\
			elm = RB_LEFT(elm, field);			\
	} else {							\
		if (RB_PARENT(elm, field) &&				\
		    (elm == RB_LEFT(RB_PARENT(elm, field), field)))	\
			elm = RB_PARENT(elm, field);			\
		else {							\
			while (RB_PARENT(elm, field) &&			\
			    (elm == RB_RIGHT(RB_PARENT(elm, field), field)))\
				elm = RB_PARENT(elm, field);		\
			elm = RB_PARENT(elm, field);			\
		}							\
	}								\
	return (elm);							\
}

#define RB_GENERATE_PREV(name, type, field, attr)			\
/* ARGSUSED */								\
attr struct type *							\
name##_RB_PREV(struct type *elm)					\
{									\
	if (RB_LEFT(elm, field)) {					\
		elm = RB_LEFT(elm, field);				\
		while (RB_RIGHT(elm, field))				\
			elm = RB_RIGHT(elm, field);			\
	} else {							\
		if (RB_PARENT(elm, field) &&				\
		    (elm == RB_RIGHT(RB_PARENT(elm, field), field)))	\
			elm = RB_PARENT(elm, field);			\
		else {							\
			while (RB_PARENT(elm, field) &&			\
			    (elm == RB_LEFT(RB_PARENT(elm, field), field)))\
				elm = RB_PARENT(elm, field);		\
			elm = RB_PARENT(elm, field);			\
		}							\
	}								\
	return (elm);							\
}

#define RB_GENERATE_MINMAX(name, type, field, attr)			\
attr struct type *							\
name##_RB_MINMAX(struct name *head, int val)				\
{									\
	struct type *tmp = RB_ROOT(head);				\
	struct type *parent = NULL;					\
	while (tmp) {							\
		parent = tmp;						\
		if (val < 0)						\
			tmp = RB_LEFT(tmp, field);			\
		else							\
			tmp = RB_RIGHT(tmp, field);			\
	}								\
	return (parent);						\
}

#define	RB_GENERATE_REINSERT(name, type, field, cmp, attr)		\
attr struct type *							\
name##_RB_REINSERT(struct name *head, struct type *elm)			\
{									\
	struct type *cmpelm;						\
	if (((cmpelm = RB_PREV(name, head, elm)) != NULL &&		\
	    cmp(cmpelm, elm) >= 0) ||					\
	    ((cmpelm = RB_NEXT(name, head, elm)) != NULL &&		\
	    cmp(elm, cmpelm) >= 0)) {					\
		/* XXXLAS: Remove/insert is heavy handed. */		\
		RB_REMOVE(name, head, elm);				\
		return (RB_INSERT(name, head, elm));			\
	}								\
	return (NULL);							\
}									\

#define RB_NEGINF	-1
#define RB_INF	1

#define RB_INSERT(name, x, y)	name##_RB_INSERT(x, y)
#define RB_REMOVE(name, x, y)	name##_RB_REMOVE(x, y)
#define RB_FIND(name, x, y)	name##_RB_FIND(x, y)
#define RB_NFIND(name, x, y)	name##_RB_NFIND(x, y)
#define RB_NEXT(name, x, y)	name##_RB_NEXT(y)
#define RB_PREV(name, x, y)	name##_RB_PREV(y)
#define RB_MIN(name, x)		name##_RB_MINMAX(x, RB_NEGINF)
#define RB_MAX(name, x)		name##_RB_MINMAX(x, RB_INF)
#define RB_REINSERT(name, x, y)	name##_RB_REINSERT(x, y)

#define RB_FOREACH(x, name, head)					\
	for ((x) = RB_MIN(name, head);					\
	     (x) != NULL;						\
	     (x) = name##_RB_NEXT(x))

#define RB_FOREACH_FROM(x, name, y)					\
	for ((x) = (y);							\
	    ((x) != NULL) && ((y) = name##_RB_NEXT(x), (x) != NULL);	\
	     (x) = (y))

#define RB_FOREACH_SAFE(x, name, head, y)				\
	for ((x) = RB_MIN(name, head);					\
	    ((x) != NULL) && ((y) = name##_RB_NEXT(x), (x) != NULL);	\
	     (x) = (y))

#define RB_FOREACH_REVERSE(x, name, head)				\
	for ((x) = RB_MAX(name, head);					\
	     (x) != NULL;						\
	     (x) = name##_RB_PREV(x))

#define RB_FOREACH_REVERSE_FROM(x, name, y)				\
	for ((x) = (y);							\
	    ((x) != NULL) && ((y) = name##_RB_PREV(x), (x) != NULL);	\
	     (x) = (y))

#define RB_FOREACH_REVERSE_SAFE(x, name, head, y)			\
	for ((x) = RB_MAX(name, head);					\
	    ((x) != NULL) && ((y) = name##_RB_PREV(x), (x) != NULL);	\
	     (x) = (y))

#endif	/* _SYS_TREE_H_ */
