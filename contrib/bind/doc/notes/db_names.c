/*
 * Copyright (c) 1996,1999 by Internet Software Consortium.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND INTERNET SOFTWARE CONSORTIUM DISCLAIMS
 * ALL WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL INTERNET SOFTWARE
 * CONSORTIUM BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS
 * ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
 * SOFTWARE.
 */

#include <sys/types.h>
#include <sys/param.h>
#include <netinet/in.h>
#include <arpa/nameser.h>

#include <ctype.h>
#include <errno.h>
#include <resolv.h>
#include <stdio.h>

#include "named.h"
#include "tree.h"

struct node {
	struct node	*parent;		/* NULL for "."'s node. */
	tree		*children;		/* Nodes using us as parent. */
	/*void		*userdata;*/		/* For future use. */
	char		name[sizeof(void*)];	/* Open array. */
};

static	struct node	rootNode;

static int
nodeCompare(t1, t2)
	const tree_t t1, t2;
{
	const char	*n1 = ((struct node *)t1)->name + sizeof(u_char),
			*n2 = ((struct node *)t2)->name + sizeof(u_char);

	return (strcasecmp(n1, n2));
}

/* void *
 * db_findname(const char *name, int storeflag)
 *	find or store a presentation format domain name.
 * returns:
 *	NULL if an error occurred (check errno)
 *	else, node's unique, opaque address.
 */
void *
db_findname(name, storeflag)
	const char *name;
	int storeflag;
{
	struct node *node, *tnode;
	const char *tname;
	size_t len;
	int ch;

	/* The root domain has its own static node. */
	if (name[0] == '\0')
		return (&rootNode);

	/* Locate the end of the first label. */
	for (tname = name; (ch = *tname) != '\0'; tname++) {
		/* Is this the end of the first label? */
		if (ch == '.')
			break;
		/* Is it an escaped character? */
		if (ch == '\\') {
			ch = *++tname;
			if (ch == '\0')
				break;
		}
	}

	/* Make sure the label's length will fit in our length byte. */
	len = tname - name;
	if (len > 255) {
		errno = ENAMETOOLONG;
		return (NULL);
	}

	/* If nothing but unescaped dots after this, elide them. */
	while (ch == '.')
		ch = *tname++;

	/*
	 * Make a new node since the comparison function needs it
	 * and we may yet end up adding it to our parent's tree.
	 *
	 * Note that by recursing for tnode->parent, we might be
	 * creating our parents and grandparents and so on.
	 */
	tnode = (struct node *)malloc(sizeof(struct node) - sizeof(void *)
				      + sizeof(u_char) + len + sizeof(char));
	tnode->parent = db_findname(tname);
	tnode->children = NULL;
	*((u_char *)tnode->name) = (u_char)len;
	memcpy(tnode->name + sizeof(u_char), name, len);
	tnode->name[sizeof(u_char) + len] = '\0';

	/* If our first label isn't in our parent's tree, put it there. */
	node = tree_srch(&tnode->parent->children, nodeCompare, (tree_t)tnode);
	if (node == NULL)
		if (storeflag)
			if (tree_add(&tnode->parent->children, nodeCompare,
				     (tree_t)tnode, NULL))
				node = tnode, tnode = NULL;
			else
				errno = ENOMEM;
		else
			errno = ENOENT;

	/* Get rid of tnode if we didn't consume it. */
	if (tnode != NULL)
		free(tnode);

	/* Return the (possibly new) node, or NULL, as appropriate. */
	return (node);
}

/* int
 * db_getname(void *node, char *name, size_t size)
 *	given a node's unique, opaque address, format its name.
 * returns:
 *	-1 = error occurred, check errno
 *	0 = success
 */
int
db_getname(vnode, name, size)
	const void *vnode;
	char *name;
	size_t size;
{
	const struct node *node = vnode;

	while (node != NULL) {
		size_t len = (size_t)node->name[0];

		if (size < len + 1)
			goto too_long;
		memcpy(name, node->name + sizeof(u_char), len);
		name += len;
		*name++ = '.';
		size -= len + sizeof(char);
		node = node->parent;
	}

	if (size < sizeof(char)) {
 too_long:
		errno = ENAMETOOLONG;
		return (-1);
	}
	*name = '\0';
	return (0);
}

/*
 * char *
 * db_makename(void *node)
 *	given a node's unique, opaque address, format and return its name.
 * returns:
 *	pointer to the name or NULL on errors (check errno).
 * notes:
 *	returns pointer to a static buffer, be careful how you call it.
 */
char *
db_makename(vnode)
	void *vnode;
{
	static char name[MAXDNAME*2];

	if (db_getname(vnode, name, sizeof name) < 0)
		return (NULL);
	return (name);
}
