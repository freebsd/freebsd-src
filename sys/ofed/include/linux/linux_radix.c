/*-
 * Copyright (c) 2010 Isilon Systems, Inc.
 * Copyright (c) 2010 iX Systems, Inc.
 * Copyright (c) 2010 Panasas, Inc.
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
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <sys/sysctl.h>

#include <linux/slab.h>
#include <linux/kernel.h>
#include <linux/radix-tree.h>
#include <linux/err.h>

MALLOC_DEFINE(M_RADIX, "radix", "Linux radix compat");

static inline int
radix_max(struct radix_tree_root *root)
{
	return (1 << (root->height * RADIX_TREE_MAP_SHIFT)) - 1;
}

static inline int
radix_pos(long id, int height)
{
	return (id >> (RADIX_TREE_MAP_SHIFT * height)) & RADIX_TREE_MAP_MASK;
}

void *
radix_tree_lookup(struct radix_tree_root *root, unsigned long index)
{
	struct radix_tree_node *node;
	void *item;
	int height;

	item = NULL;
	node = root->rnode;
	height = root->height - 1;
	if (index > radix_max(root))
		goto out;
	while (height && node)
		node = node->slots[radix_pos(index, height--)];
	if (node)
		item = node->slots[radix_pos(index, 0)];

out:
	return (item);
}

void *
radix_tree_delete(struct radix_tree_root *root, unsigned long index)
{
	struct radix_tree_node *stack[RADIX_TREE_MAX_HEIGHT];
	struct radix_tree_node *node;
	void *item;
	int height;
	int idx;

	item = NULL;
	node = root->rnode;
	height = root->height - 1;
	if (index > radix_max(root))
		goto out;
	/*
	 * Find the node and record the path in stack.
	 */
	while (height && node) {
		stack[height] = node;
		node = node->slots[radix_pos(index, height--)];
	}
	idx = radix_pos(index, 0);
	if (node)
		item = node->slots[idx];
	/*
	 * If we removed something reduce the height of the tree.
	 */
	if (item)
		for (;;) {
			node->slots[idx] = NULL;
			node->count--;
			if (node->count > 0)
				break;
			free(node, M_RADIX);
			if (node == root->rnode) {
				root->rnode = NULL;
				root->height = 0;
				break;
			}
			height++;
			node = stack[height];
			idx = radix_pos(index, height);
		}
out:
	return (item);
}

int
radix_tree_insert(struct radix_tree_root *root, unsigned long index, void *item)
{
	struct radix_tree_node *node;
	int height;
	int idx;

	/*
 	 * Expand the tree to fit indexes as big as requested.
	 */
	while (root->rnode == NULL || radix_max(root) < index) {
		node = malloc(sizeof(*node), M_RADIX, root->gfp_mask | M_ZERO);
		if (node == NULL)
			return (-ENOMEM);
		node->slots[0] = root->rnode;
		if (root->rnode)
			node->count++;
		root->rnode = node;
		root->height++;
	}
	node = root->rnode;
	height = root->height - 1;
	/*
	 * Walk down the tree finding the correct node and allocating any
	 * missing nodes along the way.
	 */
	while (height) {
		idx = radix_pos(index, height);
		if (node->slots[idx] == NULL) {
			node->slots[idx] = malloc(sizeof(*node), M_RADIX,
			    root->gfp_mask | M_ZERO);
			if (node->slots[idx] == NULL)
				return (-ENOMEM);
			node->count++;
		}
		node = node->slots[idx];
		height--;
	}
	/*
	 * Insert and adjust count if the item does not already exist.
	 */
	idx = radix_pos(index, 0);
	if (node->slots[idx])
		return (-EEXIST);
	node->slots[idx] = item;
	node->count++;
	
	return (0);
}
