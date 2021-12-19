/*-
 * Copyright (c) 2010 Isilon Systems, Inc.
 * Copyright (c) 2010 iX Systems, Inc.
 * Copyright (c) 2010 Panasas, Inc.
 * Copyright (c) 2013, 2014 Mellanox Technologies, Ltd.
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
 *
 * $FreeBSD$
 */
#ifndef	_LINUXKPI_LINUX_RBTREE_H_
#define	_LINUXKPI_LINUX_RBTREE_H_

#ifndef _STANDALONE
#include <sys/stddef.h>
#endif

#include <sys/types.h>
#include <sys/tree.h>

struct rb_node {
	RB_ENTRY(rb_node)	__entry;
};
#define	rb_left		__entry.rbe_left
#define	rb_right	__entry.rbe_right

/*
 * We provide a false structure that has the same bit pattern as tree.h
 * presents so it matches the member names expected by linux.
 */
struct rb_root {
	struct	rb_node	*rb_node;
};

struct rb_root_cached {
	struct rb_root rb_root;
	struct rb_node *rb_leftmost;
};

/*
 * In linux all of the comparisons are done by the caller.
 */
int panic_cmp(struct rb_node *one, struct rb_node *two);

RB_HEAD(linux_root, rb_node);
RB_PROTOTYPE(linux_root, rb_node, __entry, panic_cmp);

#define	rb_parent(r)	RB_PARENT(r, __entry)
#define	rb_entry(ptr, type, member)	container_of(ptr, type, member)
#define	rb_entry_safe(ptr, type, member) \
	((ptr) != NULL ? rb_entry(ptr, type, member) : NULL)

#define	RB_EMPTY_ROOT(root)	((root)->rb_node == NULL)
#define RB_EMPTY_NODE(node)     (RB_PARENT(node, __entry) == node)
#define RB_CLEAR_NODE(node)     RB_SET_PARENT(node, node, __entry)

#define	rb_insert_color(node, root)					\
	linux_root_RB_INSERT_COLOR((struct linux_root *)(root), (node))
#define	rb_erase(node, root)						\
	linux_root_RB_REMOVE((struct linux_root *)(root), (node))
#define	rb_next(node)	RB_NEXT(linux_root, NULL, (node))
#define	rb_prev(node)	RB_PREV(linux_root, NULL, (node))
#define	rb_first(root)	RB_MIN(linux_root, (struct linux_root *)(root))
#define	rb_last(root)	RB_MAX(linux_root, (struct linux_root *)(root))
#define	rb_first_cached(root)	(root)->rb_leftmost

static inline struct rb_node *
__rb_deepest_left(struct rb_node *node)
{
	struct rb_node *parent = NULL;
	while (node != NULL) {
		parent = node;
		if (RB_LEFT(node, __entry))
			node = RB_LEFT(node, __entry);
		else
			node = RB_RIGHT(node, __entry);
	}
	return (parent);
}

static inline struct rb_node *
rb_next_postorder(const struct rb_node *node)
{
	struct rb_node *parent =
	    RB_PARENT(__DECONST(struct rb_node *, node), __entry);
	/* left -> right, right -> root */
	if (parent != NULL &&
	    (node == RB_LEFT(parent, __entry)) &&
	    (RB_RIGHT(parent, __entry)))
		return (__rb_deepest_left(RB_RIGHT(parent, __entry)));
	else
		return (parent);
}

#define	rbtree_postorder_for_each_entry_safe(x, y, head, member)	\
	for ((x) = rb_entry_safe(__rb_deepest_left((head)->rb_node),	\
	    __typeof(*x), member);					\
	    ((x) != NULL) && ((y) =					\
	    rb_entry_safe(rb_next_postorder(&x->member), typeof(*x), member), 1); \
	    (x) = (y))

static inline void
rb_link_node(struct rb_node *node, struct rb_node *parent,
    struct rb_node **rb_link)
{
	RB_SET(node, parent, __entry);
	*rb_link = node;
}

static inline void
rb_replace_node(struct rb_node *victim, struct rb_node *new,
    struct rb_root *root)
{

	RB_SWAP_CHILD((struct linux_root *)root, victim, new, __entry);
	if (victim->rb_left)
		RB_SET_PARENT(victim->rb_left, new, __entry);
	if (victim->rb_right)
		RB_SET_PARENT(victim->rb_right, new, __entry);
	*new = *victim;
}

static inline void
rb_insert_color_cached(struct rb_node *node, struct rb_root_cached *root,
    bool leftmost)
{
	linux_root_RB_INSERT_COLOR((struct linux_root *)&root->rb_root, node);
	if (leftmost)
		root->rb_leftmost = node;
}

static inline struct rb_node *
rb_erase_cached(struct rb_node *node, struct rb_root_cached *root)
{
	struct rb_node *retval;

	if (node == root->rb_leftmost)
		retval = root->rb_leftmost = linux_root_RB_NEXT(node);
	else
		retval = NULL;
	linux_root_RB_REMOVE((struct linux_root *)&root->rb_root, node);
	return (retval);
}

static inline void
rb_replace_node_cached(struct rb_node *old, struct rb_node *new,
    struct rb_root_cached *root)
{
	rb_replace_node(old, new, &root->rb_root);
	if (root->rb_leftmost == old)
		root->rb_leftmost = new;
}

#undef RB_ROOT
#define RB_ROOT		(struct rb_root) { NULL }
#define	RB_ROOT_CACHED	(struct rb_root_cached) { RB_ROOT, NULL }

#endif	/* _LINUXKPI_LINUX_RBTREE_H_ */
