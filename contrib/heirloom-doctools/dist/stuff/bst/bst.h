/*
 * Copyright (c) 2015-2016, Carsten Kunze
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <time.h>
#include <stdint.h>

#define BST_EEXIST -2
#define BST_ENOENT -4

union bst_val {
	void *p;
	int i;
	long l;
	uint64_t u64;
	time_t t;
};

struct bst_node {
	union bst_val key;
	union bst_val data;
	int bf;
	struct bst_node *parent, *left, *right;
};

struct bst {
	struct bst_node *root;
	int (*cmp)(union bst_val, union bst_val);
};

int bst_padd(struct bst *, union bst_val, union bst_val, int);
int bst_srch(struct bst *, union bst_val, struct bst_node **);
int bst_pdel(struct bst *, union bst_val, int);
void bst_pdel_node(struct bst *, struct bst_node *, int);

#define avl_add(t, k, v)   bst_padd(t, k, v, 1)
#define avl_del(t, k)      bst_pdel(t, k, 1)
#define avl_del_node(t, n) bst_pdel_node(t, n, 1)

/* The following functions perform non-balancing BST operations. These are
 * useful when deleting (visited) notes while walking through the tree. */

#define bst_add(t, k, v)   bst_padd(t, k, v, 0)
#define bst_del(t, k)      bst_pdel(t, k, 0)
#define bst_del_node(t, n) bst_pdel_node(t, n, 0)
