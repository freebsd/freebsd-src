/*
 * Copyright (c) 1989 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Michael Rendell of Memorial University of Newfoundland.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef lint
char copyright[] =
"@(#) Copyright (c) 1989 The Regents of the University of California.\n\
 All rights reserved.\n";
#endif /* not lint */

#ifndef lint
static char sccsid[] = "@(#)tsort.c	5.3 (Berkeley) 6/1/90";
#endif /* not lint */

#include <sys/types.h>
#include <errno.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>

/*
 *  Topological sort.  Input is a list of pairs of strings seperated by
 *  white space (spaces, tabs, and/or newlines); strings are written to
 *  standard output in sorted order, one per line.
 *
 *  usage:
 *     tsort [inputfile]
 *  If no input file is specified, standard input is read.
 *
 *  Should be compatable with AT&T tsort HOWEVER the output is not identical
 *  (i.e. for most graphs there is more than one sorted order, and this tsort
 *  usually generates a different one then the AT&T tsort).  Also, cycle
 *  reporting seems to be more accurate in this version (the AT&T tsort
 *  sometimes says a node is in a cycle when it isn't).
 *
 *  Michael Rendell, michael@stretch.cs.mun.ca - Feb 26, '90
 */
#define	HASHSIZE	53		/* doesn't need to be big */
#define	NF_MARK		0x1		/* marker for cycle detection */
#define	NF_ACYCLIC	0x2		/* this node is cycle free */

typedef struct node_str NODE;

struct node_str {
	char *n_name;			/* name of this node */
	NODE **n_prevp;			/* pointer to previous node's n_next */
	NODE *n_next;			/* next node in graph */
	NODE *n_hash;			/* next node in hash table */
	int n_narcs;			/* number of arcs in n_arcs[] */
	int n_arcsize;			/* size of n_arcs[] array */
	NODE **n_arcs;			/* array of arcs to other nodes */
	int n_refcnt;			/* # of arcs pointing to this node */
	int n_flags;			/* NF_* */
};

typedef struct _buf {
	char *b_buf;
	int b_bsize;
} BUF;

NODE *add_node(), *find_node();
void add_arc(), no_memory(), remove_node(), tsort();
char *grow_buf(), *malloc();

extern int errno;
NODE *graph;
NODE *hashtable[HASHSIZE];
NODE **cycle_buf;
NODE **longest_cycle;

main(argc, argv)
	int argc;
	char **argv;
{
	register BUF *b;
	register int c, n;
	FILE *fp;
	int bsize, nused;
	BUF bufs[2];

	if (argc < 2)
		fp = stdin;
	else if (argc == 2) {
		(void)fprintf(stderr, "usage: tsort [ inputfile ]\n");
		exit(1);
	} else if (!(fp = fopen(argv[1], "r"))) {
		(void)fprintf(stderr, "tsort: %s.\n", strerror(errno));
		exit(1);
	}

	for (b = bufs, n = 2; --n >= 0; b++)
		b->b_buf = grow_buf((char *)NULL, b->b_bsize = 1024);

	/* parse input and build the graph */
	for (n = 0, c = getc(fp);;) {
		while (c != EOF && isspace(c))
			c = getc(fp);
		if (c == EOF)
			break;

		nused = 0;
		b = &bufs[n];
		bsize = b->b_bsize;
		do {
			b->b_buf[nused++] = c;
			if (nused == bsize) {
				bsize *= 2;
				b->b_buf = grow_buf(b->b_buf, bsize);
			}
			c = getc(fp);
		} while (c != EOF && !isspace(c));

		b->b_buf[nused] = '\0';
		b->b_bsize = bsize;
		if (n)
			add_arc(bufs[0].b_buf, bufs[1].b_buf);
		n = !n;
	}
	(void)fclose(fp);
	if (n) {
		(void)fprintf(stderr, "tsort: odd data count.\n");
		exit(1);
	}

	/* do the sort */
	tsort();
	exit(0);
}

/* double the size of oldbuf and return a pointer to the new buffer. */
char *
grow_buf(bp, size)
	char *bp;
	int size;
{
	char *realloc();

	if (!(bp = realloc(bp, (u_int)size)))
		no_memory();
	return(bp);
}

/*
 * add an arc from node s1 to node s2 in the graph.  If s1 or s2 are not in
 * the graph, then add them.
 */
void
add_arc(s1, s2)
	char *s1, *s2;
{
	register NODE *n1;
	NODE *n2;
	int bsize;

	n1 = find_node(s1);
	if (!n1)
		n1 = add_node(s1);

	if (!strcmp(s1, s2))
		return;

	n2 = find_node(s2);
	if (!n2)
		n2 = add_node(s2);

	/*
	 * could check to see if this arc is here already, but it isn't
	 * worth the bother -- there usually isn't and it doesn't hurt if
	 * there is (I think :-).
	 */
	if (n1->n_narcs == n1->n_arcsize) {
		if (!n1->n_arcsize)
			n1->n_arcsize = 10;
		bsize = n1->n_arcsize * sizeof(*n1->n_arcs) * 2;
		n1->n_arcs = (NODE **)grow_buf((char *)n1->n_arcs, bsize);
		n1->n_arcsize = bsize / sizeof(*n1->n_arcs);
	}
	n1->n_arcs[n1->n_narcs++] = n2;
	++n2->n_refcnt;
}

hash_string(s)
	char *s;
{
	register int hash, i;

	for (hash = 0, i = 1; *s; s++, i++)
		hash += *s * i;
	return(hash % HASHSIZE);
}

/*
 * find a node in the graph and return a pointer to it - returns null if not
 * found.
 */
NODE *
find_node(name)
	char *name;
{
	register NODE *n;

	for (n = hashtable[hash_string(name)]; n; n = n->n_hash)
		if (!strcmp(n->n_name, name))
			return(n);
	return((NODE *)NULL);
}

/* Add a node to the graph and return a pointer to it. */
NODE *
add_node(name)
	char *name;
{
	register NODE *n;
	int hash;

	if (!(n = (NODE *)malloc(sizeof(NODE))) || !(n->n_name = strdup(name)))
		no_memory();

	n->n_narcs = 0;
	n->n_arcsize = 0;
	n->n_arcs = (NODE **)NULL;
	n->n_refcnt = 0;
	n->n_flags = 0;

	/* add to linked list */
	if (n->n_next = graph)
		graph->n_prevp = &n->n_next;
	n->n_prevp = &graph;
	graph = n;

	/* add to hash table */
	hash = hash_string(name);
	n->n_hash = hashtable[hash];
	hashtable[hash] = n;
	return(n);
}

/* do topological sort on graph */
void
tsort()
{
	register NODE *n, *next;
	register int cnt;

	while (graph) {
		/*
		 * keep getting rid of simple cases until there are none left,
		 * if there are any nodes still in the graph, then there is
		 * a cycle in it.
		 */
		do {
			for (cnt = 0, n = graph; n; n = next) {
				next = n->n_next;
				if (n->n_refcnt == 0) {
					remove_node(n);
					++cnt;
				}
			}
		} while (graph && cnt);

		if (!graph)
			break;

		if (!cycle_buf) {
			/*
			 * allocate space for two cycle logs - one to be used
			 * as scratch space, the other to save the longest
			 * cycle.
			 */
			for (cnt = 0, n = graph; n; n = n->n_next)
				++cnt;
			cycle_buf =
			    (NODE **)malloc((u_int)sizeof(NODE *) * cnt);
			longest_cycle =
			    (NODE **)malloc((u_int)sizeof(NODE *) * cnt);
			if (!cycle_buf || !longest_cycle)
				no_memory();
		}
		for (n = graph; n; n = n->n_next)
			if (!(n->n_flags & NF_ACYCLIC)) {
				if (cnt = find_cycle(n, n, 0, 0)) {
					register int i;

					(void)fprintf(stderr,
					    "tsort: cycle in data.\n");
					for (i = 0; i < cnt; i++)
						(void)fprintf(stderr,
				"tsort: %s.\n", longest_cycle[i]->n_name);
					remove_node(n);
					break;
				} else
					/* to avoid further checks */
					n->n_flags  = NF_ACYCLIC;
			}

		if (!n) {
			(void)fprintf(stderr,
			    "tsort: internal error -- could not find cycle.\n");
			exit(1);
		}
	}
}

/* print node and remove from graph (does not actually free node) */
void
remove_node(n)
	register NODE *n;
{
	register NODE **np;
	register int i;

	(void)printf("%s\n", n->n_name);
	for (np = n->n_arcs, i = n->n_narcs; --i >= 0; np++)
		--(*np)->n_refcnt;
	n->n_narcs = 0;
	*n->n_prevp = n->n_next;
	if (n->n_next)
		n->n_next->n_prevp = n->n_prevp;
}

/* look for the longest cycle from node from to node to. */
find_cycle(from, to, longest_len, depth)
	NODE *from, *to;
	int depth, longest_len;
{
	register NODE **np;
	register int i, len;

	/*
	 * avoid infinite loops and ignore portions of the graph known
	 * to be acyclic
	 */
	if (from->n_flags & (NF_MARK|NF_ACYCLIC))
		return(0);
	from->n_flags = NF_MARK;

	for (np = from->n_arcs, i = from->n_narcs; --i >= 0; np++) {
		cycle_buf[depth] = *np;
		if (*np == to) {
			if (depth + 1 > longest_len) {
				longest_len = depth + 1;
				(void)memcpy((char *)longest_cycle,
				    (char *)cycle_buf,
				    longest_len * sizeof(NODE *));
			}
		} else {
			len = find_cycle(*np, to, longest_len, depth + 1);
			if (len > longest_len)
				longest_len = len;
		}
	}
	from->n_flags &= ~NF_MARK;
	return(longest_len);
}

void
no_memory()
{
	(void)fprintf(stderr, "tsort: %s.\n", strerror(ENOMEM));
	exit(1);
}
