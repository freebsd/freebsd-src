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

void
tdestroy(void *rootp, void (*node_free)(void *))
{
	posix_tnode *back, *curr, **front;

	if (rootp == NULL)
		return;
	if (node_free == NULL)
		node_free = nul_node_free;

	back = rootp;
	front = &back;

	for (;;) {
		/*
		 * The sequence of nodes from back to just before *front linked
		 * by llink have been found to have non-NULL rlink.
		 *
		 * Extend *front to (*front)->llink, deleting *front from the
		 * sequence if it has a NULL rlink.
		 */
		curr = *front;
		if (curr->rlink != NULL)
			front = &curr->llink;
		else {
			*front = curr->llink;
			node_free(curr->key);
			free(curr);
		}
		if (*front != NULL)
			continue;
		if (back == NULL)
			break;

		/*
		 * The sequence cannot be extended because *front is NULL.  Make
		 * the rlink of the back node the new *front, the llink of the
		 * back node the new back, and free the old back node.
		 */
		curr = back;
		back = curr->llink;
		if (back == NULL)
			front = &back;
		*front = curr->rlink;
		node_free(curr->key);
		free(curr);
	}
}
