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

#include <stdio.h>
#include <stdlib.h>
#include "bst.h"

#define NODE_FOUND   0
#define INSERT_LEFT  1
#define INSERT_RIGHT 2
#define TREE_EMPTY   3

static int srch_node(struct bst *, union bst_val, struct bst_node **);

/* Returns:
 *   0           No error
 *   BST_EEXIST  Key already exists */

int
bst_padd(struct bst *bst, union bst_val key, union bst_val data, int bal) {
	struct bst_node *n, *c, *gc, *t;
	int i;
	if ((i = srch_node(bst, key, &n)) == NODE_FOUND) {
		fprintf(stderr, "bst_add: Key does already exist\n");
		return BST_EEXIST;
	}
	c = malloc(sizeof(struct bst_node));
	c->left = c->right = NULL;
	c->bf = 0;
	switch (i) {
	case INSERT_LEFT:
		n->left = c;
		break;
	case INSERT_RIGHT:
		n->right = c;
		break;
	default:
		bst->root = c;
	}
	c->parent = n;
	c->key = key;
	c->data = data;
	if (!bal)
		return 0;
	gc = NULL;
	while (n) {
		if (n->left == c)
			n->bf++;
		else
			n->bf--;
		if (!n->bf)
			break;
		if (n->bf == -2) {
			if (c->bf == -1) {
				if (!(t = c->parent = n->parent))
					bst->root = c;
				else if (t->left == n)
					t->left = c;
				else
					t->right = c;
				n->parent = c;
				if ((t = n->right = c->left))
					t->parent = n;
				c->left = n;
				n->bf = c->bf = 0;
			} else if (c->bf == 1) {
				if ((t = c->left = gc->right))
					t->parent = c;
				gc->right = c;
				c->parent = gc;
				if (!(t = gc->parent = n->parent))
					bst->root = gc;
				else if (t->left == n)
					t->left = gc;
				else
					t->right = gc;
				n->parent = gc;
				if ((t = n->right = gc->left))
					t->parent = n;
				gc->left = n;
				if (!gc->bf)
					n->bf = c->bf = 0;
				else if (gc->bf > 0) {
					n->bf = 0;
					c->bf = -1;
				} else {
					n->bf = 1;
					c->bf = 0;
				}
				gc->bf = 0;
			}
			break;
		}
		if (n->bf == 2) {
			if (c->bf == 1) {
				if (!(t = c->parent = n->parent))
					bst->root = c;
				else if (t->left == n)
					t->left = c;
				else
					t->right = c;
				n->parent = c;
				if ((t = n->left = c->right))
					t->parent = n;
				c->right = n;
				n->bf = c->bf = 0;
			} else if (c->bf == -1) {
				if ((t = c->right = gc->left))
					t->parent = c;
				gc->left = c;
				c->parent = gc;
				if (!(t = gc->parent = n->parent))
					bst->root = gc;
				else if (t->left == n)
					t->left = gc;
				else
					t->right = gc;
				n->parent = gc;
				if ((t = n->left = gc->right))
					t->parent = n;
				gc->right = n;
				if (!gc->bf)
					n->bf = c->bf = 0;
				else if (gc->bf > 0) {
					c->bf = 0;
					n->bf = -1;
				} else {
					c->bf = 1;
					n->bf = 0;
				}
				gc->bf = 0;
			}
			break;
		}
		gc = c;
		c = n;
		n = n->parent;
	}
	return 0;
}

/* Delete node
 *
 * Returns:
 *   0           No error
 *   BST_ENOENT  Key not found */

int
bst_pdel(struct bst *bst, union bst_val key, int bal) {
	struct bst_node *n;
	if (srch_node(bst, key, &n) != NODE_FOUND) {
		return BST_ENOENT;
	}
	bst_pdel_node(bst, n, bal);
	return 0;
}

void
bst_pdel_node(struct bst *bst, struct bst_node *n, int bal) {
	struct bst_node *p, **pp, *t, *x;
	int bfc;
	if (!(t = n->parent)) {
		p = NULL;
		pp = &bst->root;
	} else {
		p = t;
		if (p->left == n) {
			pp = &p->left;
			bfc = -1;
		} else {
			pp = &p->right;
			bfc = 1;
		}
	}
	if (!n->left) {
		if ((*pp = t = n->right))
			t->parent = p;
	} else if (!n->right) {
		*pp = t = n->left;
		t->parent = p;
	} else {
		for (t = n->right; t->left; t = t->left)
			;
		if (t == n->right) {
			p = t;
			bfc = 1;
		} else {
			if ((t->parent->left = t->right))
				t->right->parent = t->parent;
			t->right         = n->right;
			t->right->parent = t;
			p                = t->parent;
			bfc              = -1;
		}
		*pp             = t;
		t->parent       = n->parent;
		t->left         = n->left;
		t->left->parent = t;
		t->bf           = n->bf;
	}
	free(n);
	if (!bal)
		return;
	while (p) {
		int bf;
		switch(p->bf += bfc) {
		case -1:
		case  1:
			return;
		case -2:
			if ((bf = (n = p->right)->bf) != 1) {
				if (!(t = n->parent = p->parent))
					bst->root = n;
				else if (t->left == p)
					t->left = n;
				else
					t->right = n;
				p->parent = n;
				if ((t = p->right = n->left))
					t->parent = p;
				n->left = p;
				if (bf) {
					p->bf = n->bf = 0;
				} else {
					n->bf = 1;
					p->bf = -1;
					return;
				}
				p = n;
			} else {
				x = n->left;
				if ((t = n->left = x->right))
					t->parent = n;
				x->right = n;
				n->parent = x;
				if (!(t = x->parent = p->parent))
					bst->root = x;
				else if (t->left == p)
					t->left = x;
				else
					t->right = x;
				p->parent = x;
				if ((t = p->right = x->left))
					t->parent = p;
				x->left = p;
				if (!(bf = x->bf))
					p->bf = n->bf = 0;
				else if (bf > 0) {
					p->bf = 0;
					n->bf = -1;
				} else {
					p->bf = 1;
					n->bf = 0;
				}
				x->bf = 0;
				p = x;
			}
			break;
		case 2:
			if ((bf = (n = p->left)->bf) != -1) {
				if (!(t = n->parent = p->parent))
					bst->root = n;
				else if (t->left == p)
					t->left = n;
				else
					t->right = n;
				p->parent = n;
				if ((t = p->left = n->right))
					t->parent = p;
				n->right = p;
				if (bf) {
					p->bf = n->bf = 0;
				} else {
					n->bf = -1;
					p->bf = 1;
					return;
				}
				p = n;
			} else {
				x = n->right;
				if ((t = n->right = x->left))
					t->parent = n;
				x->left = n;
				n->parent = x;
				if (!(t = x->parent = p->parent))
					bst->root = x;
				else if (t->left == p)
					t->left = x;
				else
					t->right = x;
				p->parent = x;
				if ((t = p->left = x->right))
					t->parent = p;
				x->right = p;
				if (!(bf = x->bf))
					p->bf = n->bf = 0;
				else if (bf > 0) {
					n->bf = 0;
					p->bf = -1;
				} else {
					n->bf = 1;
					p->bf = 0;
				}
				x->bf = 0;
				p = x;
			}
		}
		n = p;
		if ((p = p->parent)) {
			if (n == p->left)
				bfc = -1;
			else
				bfc = 1;
		}
	}
}

int /* 0: found, !0: not found */
bst_srch(struct bst *bst, union bst_val key, struct bst_node **node)
{
	struct bst_node *n;
	int retval;
	if ((retval = srch_node(bst, key, &n)) == NODE_FOUND && node)
		*node = n;
	return retval;
}

/* Returns:
 *   NODE_FOUND    Found in node
 *   INSERT_LEFT   Not found, insert to node->left
 *   INSERT_RIGHT  Not found, insert to node->right
 *   TREE_EMPTY    Tree empty (node = NULL) */

static int
srch_node(struct bst *bst, union bst_val key, struct bst_node **node) {
	struct bst_node *n = bst->root, *c;
	int retval = TREE_EMPTY;
	int d;
	while (n) {
		d = bst->cmp(key, n->key);
		if (d < 0) {
			c = n->left;
		} else if (d > 0) {
			c = n->right;
		} else {
			retval = NODE_FOUND;
			goto end;
		}
		if (!c) {
			retval = d > 0 ? INSERT_RIGHT : INSERT_LEFT;
			goto end;
		}
		n = c;
	}
end:
	*node = n;
	return retval;
}
