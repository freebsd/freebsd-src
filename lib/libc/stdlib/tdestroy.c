/*
 * Copyright 2025 The FreeBSD Foundation
 *
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * This software was developed by Konstantin Belousov <kib@FreeBSD.org>
 * under sponsorship from the FreeBSD Foundation.
 */

#define	_SEARCH_PRIVATE
#include <search.h>
#include <stdlib.h>

static void
nul_node_free(void *node __unused)
{
}

/* Find the leftmost node. */
static posix_tnode *
tdestroy_find_leftmost(posix_tnode *tn)
{
	while (tn->llink != NULL)
		tn = tn->llink;
	return (tn);
}

/*
 * This algorithm for non-recursive non-allocating destruction of the tree
 * is described in
 * https://codegolf.stackexchange.com/questions/478/free-a-binary-tree/489#489P
 * and in https://devblogs.microsoft.com/oldnewthing/20251107-00/?p=111774.
 */
void
tdestroy(void *rootp, void (*node_free)(void *))
{
	posix_tnode *tn, *tn_leftmost, *xtn;

	tn = rootp;
	if (tn == NULL)
		return;
	if (node_free == NULL)
		node_free = nul_node_free;
	tn_leftmost = tn;

	while (tn != NULL) {
		/*
		 * Make the right subtree the left subtree of the
		 * leftmost node, and recalculate the leftmost.
		 */
		tn_leftmost = tdestroy_find_leftmost(tn_leftmost);
		if (tn->rlink != NULL) {
			tn_leftmost->llink = tn->rlink;
			tn_leftmost = tn_leftmost->llink;
		}

		/*
		 * At this point, all children of tn have been
		 * arranged to be reachable via tn->left. We can
		 * safely delete the current node and advance to its
		 * left child as the new root.
		 */
		xtn = tn->llink;
		node_free(tn->key);
		free(tn);
		tn = xtn;
	}
}
