/*	$NetBSD: twalk.c,v 1.4 2012/03/20 16:38:45 matt Exp $	*/

/*
 * Tree search generalized from Knuth (6.2.2) Algorithm T just like
 * the AT&T man page says.
 *
 * The node_t structure is for internal use only, lint doesn't grok it.
 *
 * Written by reading the System V Interface Definition, not the code.
 *
 * Totally public domain.
 */

#include <sys/cdefs.h>
#if 0
#if defined(LIBC_SCCS) && !defined(lint)
__RCSID("$NetBSD: twalk.c,v 1.4 2012/03/20 16:38:45 matt Exp $");
#endif /* LIBC_SCCS and not lint */
#endif
__FBSDID("$FreeBSD$");

#define _SEARCH_PRIVATE
#include <search.h>
#include <stdlib.h>

typedef void (*cmp_fn_t)(const void *, VISIT, int);

/* Walk the nodes of a tree */
static void
trecurse(const node_t *root,	/* Root of the tree to be walked */
	cmp_fn_t action, int level)
{

	if (root->llink == NULL && root->rlink == NULL)
		(*action)(root, leaf, level);
	else {
		(*action)(root, preorder, level);
		if (root->llink != NULL)
			trecurse(root->llink, action, level + 1);
		(*action)(root, postorder, level);
		if (root->rlink != NULL)
			trecurse(root->rlink, action, level + 1);
		(*action)(root, endorder, level);
	}
}

/* Walk the nodes of a tree */
void
twalk(const void *vroot, cmp_fn_t action) /* Root of the tree to be walked */
{
	if (vroot != NULL && action != NULL)
		trecurse(vroot, action, 0);
}
