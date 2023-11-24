/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2019 Mark Kettenis <kettenis@OpenBSD.org>
 * Copyright (c) 2021 Vladimir Kondratyev <wulf@FreeBSD.org>
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
 */

#include <linux/rbtree.h>

#define	INTERVAL_TREE_DEFINE(type, field, valtype, dummy, START, LAST, 	\
		attr, name)						\
	__IT_DEFINE_ITER_FROM(type, field, valtype, START, LAST, name)	\
	__IT_DEFINE_ITER_FIRST(type, valtype, attr, name)		\
	__IT_DEFINE_ITER_NEXT(type, field, valtype, attr, name)		\
	__IT_DEFINE_INSERT(type, field, START, attr, name)		\
	__IT_DEFINE_REMOVE(type, field, attr, name)

#define	__IT_DEFINE_ITER_FROM(type, field, valtype, START, LAST, name)	\
static inline type *							\
name##_iter_from(struct rb_node *rb, valtype start, valtype last)	\
{									\
	type *node;							\
									\
	while (rb != NULL) {						\
		node = rb_entry(rb, type, field);			\
		if (LAST(node) >= start && START(node) <= last)		\
			return (node);					\
		else if (START(node) > last)				\
			break;						\
		rb = rb_next(rb);					\
	}								\
	return (NULL);							\
}

#define	__IT_DEFINE_ITER_FIRST(type, valtype, attr, name)		\
attr type *								\
name##_iter_first(struct rb_root_cached *root, valtype start, valtype last) \
{									\
	return (name##_iter_from(rb_first_cached(root), start, last));	\
}

#define	__IT_DEFINE_ITER_NEXT(type, field, valtype, attr, name)		\
attr type *								\
name##_iter_next(type *node, valtype start, valtype last)		\
{									\
	return (name##_iter_from(rb_next(&node->field), start, last));	\
}

#define	__IT_DEFINE_INSERT(type, field, START, attr, name)		\
attr void								\
name##_insert(type *node, struct rb_root_cached *root)			\
{									\
	struct rb_node **iter = &root->rb_root.rb_node;			\
	struct rb_node *parent = NULL;					\
	type *iter_node;						\
	bool min_entry = true;						\
									\
	while (*iter != NULL) {						\
		parent = *iter;						\
		iter_node = rb_entry(parent, type, field);		\
		if (START(node) < START(iter_node))			\
			iter = &parent->rb_left;			\
		else {							\
			iter = &parent->rb_right;			\
			min_entry = false;				\
		}							\
	}								\
									\
	rb_link_node(&node->field, parent, iter);			\
	rb_insert_color_cached(&node->field, root, min_entry);		\
}

#define	__IT_DEFINE_REMOVE(type, field, attr, name)			\
attr void								\
name##_remove(type *node, struct rb_root_cached *root)			\
{									\
	rb_erase_cached(&node->field, root);				\
}
