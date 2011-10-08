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

#include <config.h>

#include "roken.h"
#include "search.h"

struct node {
    char *string;
    int order;
};

extern void *rk_tdelete(const void *, void **,
		 int (*)(const void *, const void *));
extern void *rk_tfind(const void *, void * const *,
	       int (*)(const void *, const void *));
extern void *rk_tsearch(const void *, void **, int (*)(const void *, const void *));
extern void rk_twalk(const void *, void (*)(const void *, VISIT, int));

void *rootnode = NULL;
int numerr = 0;

/*
 *  This routine compares two nodes, based on an
 *  alphabetical ordering of the string field.
 */
int
node_compare(const void *node1, const void *node2)
{
    return strcmp(((const struct node *) node1)->string,
		  ((const struct node *) node2)->string);
}

static int walkorder = -1;

void
list_node(const void *ptr, VISIT order, int level)
{
    const struct node *p = *(const struct node **) ptr;

    if (order == postorder || order == leaf)  {
	walkorder++;
	if (p->order != walkorder) {
	    warnx("sort failed: expected %d next, got %d\n", walkorder,
		  p->order);
	    numerr++;
	}
    }
}

int
main(int argc, char **argv)
{
    int numtest = 1;
    struct node *t, *p, tests[] = {
	{ "", 0 },
	{ "ab", 3 },
	{ "abc", 4 },
	{ "abcdefg", 8 },
	{ "abcd", 5 },
	{ "a", 2 },
	{ "abcdef", 7 },
	{ "abcde", 6 },
	{ "=", 1 },
	{ NULL }
    };

    for(t = tests; t->string; t++) {
	/* Better not be there */
	p = (struct node *)rk_tfind((void *)t, (void **)&rootnode,
				    node_compare);

	if (p) {
	    warnx("erroneous list: found %d\n", p->order);
	    numerr++;
	}

	/* Put node into the tree. */
	p = (struct node *) rk_tsearch((void *)t, (void **)&rootnode,
				       node_compare);

	if (!p) {
	    warnx("erroneous list: missing %d\n", t->order);
	    numerr++;
	}
    }

    rk_twalk(rootnode, list_node);

    for(t = tests; t->string; t++) {
	/* Better be there */
	p =  (struct node *) rk_tfind((void *)t, (void **)&rootnode,
				      node_compare);

	if (!p) {
	    warnx("erroneous list: missing %d\n", t->order);
	    numerr++;
	}

	/* pull out node */
	(void) rk_tdelete((void *)t, (void **)&rootnode,
			  node_compare);

	/* Better not be there */
	p =  (struct node *) rk_tfind((void *)t, (void **)&rootnode,
				      node_compare);

	if (p) {
	    warnx("erroneous list: found %d\n", p->order);
	    numerr++;
	}

    }

    return numerr;
}
