/*
 *  Copyright 1993 Open Software Foundation, Inc., Cambridge, Massachusetts.
 *  All rights reserved.
 */
/*
 * Copyright (c) 1994  
 * Open Software Foundation, Inc. 
 *  
 * Permission is hereby granted to use, copy, modify and freely distribute 
 * the software in this file and its documentation for any purpose without 
 * fee, provided that the above copyright notice appears in all copies and 
 * that both the copyright notice and this permission notice appear in 
 * supporting documentation.  Further, provided that the name of Open 
 * Software Foundation, Inc. ("OSF") not be used in advertising or 
 * publicity pertaining to distribution of the software without prior 
 * written permission from OSF.  OSF makes no representations about the 
 * suitability of this software for any purpose.  It is provided "as is" 
 * without express or implied warranty. 
 */
/*
 * Copyright (c) 1996 X Consortium
 * Copyright (c) 1995, 1996 Dalrymple Consulting
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
 * X CONSORTIUM OR DALRYMPLE CONSULTING BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 * 
 * Except as contained in this notice, the names of the X Consortium and
 * Dalrymple Consulting shall not be used in advertising or otherwise to
 * promote the sale, use or other dealings in this Software without prior
 * written authorization.
 */

/* ________________________________________________________________________
 *
 *  Module for interactive browsing.
 *
 *  Entry points for this module:
 *	Browse()		interactive browser
 * ________________________________________________________________________
 */

#ifndef lint
static char *RCSid =
  "$Header: /usr/src/docbook-to-man/Instant/RCS/browse.c,v 1.2 1996/06/02 21:46:10 fld Exp $";
#endif

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>

#include "general.h"

static void	PrElemPlusID(Element_t *);
static void	ls_node(Element_t *, int, char **);
static void	do_query(Element_t *, char *, char *);
static void	do_find(Element_t *, char **);

/* ______________________________________________________________________ */

static char *br_help_msg[] = {
  "  ls            List info about current element in tree",
  "                (context, children, attributes, etc.)",
  "  cd N ...      Change to Nth elememt child, where N is shown by 'ls'.",
  "                N may also be '/' (top) or '..' (up).",
  "  cd id I       Change to elememt whose ID is I",
  "  data N        Show data of Nth data node",
  "  where         Show current position in the tree",
  "  id I          Show path to element with id I",
  "                (using '?' for I will lists all IDs and their paths)",
  "  find S        Find elements matching spec S. Recognized syntaxes:",
  "                  find attr <name> <value>",
  "                  find cont <string>",
  "                  find parent <gi-name>",
  "                  find child <gi-name>",
  "                  find gi <gi-name>",
  "  q rel gi      Query: report if elem 'gi' has relation to current elem",
  "                ('rel' is one of 'child parent ancestor descendant",
  "                  sibling sibling+ sibling+1 sibling- sibling-1 cousin')",
  "",
  "  tran [outfile]",
  "                Translate into 'outfile' (stdout)",
  "  stat          Print statistics (how often elements occur, etc.)",
  "  sum           Print elem usage summary (# of children, depth, etc.)",
  "  tree          Print document hierarchy as a tree",
  "  cont          Print context of each element",
  NULL
};

/* ______________________________________________________________________ */

void
Browse()
{
    char	buf[256], *cmd, **av, **sv;
    char	*Prompt;
    Element_t	*ce;	/* current element */
    Element_t	*e;
    int		i, n, ac;

    if (slave) Prompt = "=>\n";
    else Prompt = "=> ";

    ce = DocTree;
    while (fputs(Prompt, stdout)) {
	if (!fgets(buf, 256, stdin)) break;
	stripNL(buf);
	if (buf[0] == EOS) {
	    fputs(Prompt, stdout);
	    continue;
	}
	ac = 20;
	av = Split(buf, &ac, S_ALVEC);
	if (ac > 0) cmd = av[0];
	if (!cmd || !(*cmd)) continue;

	if (!strcmp(cmd, "ls")) ls_node(ce, ac, av);

	else if (!strcmp(cmd, "cd")) {
	    if (av[1]) {
		if (ac == 3 && !strcmp(av[1], "id")) {
		    if ((e = FindElemByID(av[2]))) ce = e;
		    else printf("Element with ID '%s' not found.\n", av[2]);
		    continue;
		}
		for (i=1; i<ac; i++) {
		    if (!strcmp(av[i], "..")) {
			if (ce->parent) ce = ce->parent;
			continue;
		    }
		    if (!strcmp(av[i], "/")) {
			if (ce->parent) ce = DocTree;
			continue;
		    }
		    if (!isdigit(*av[i])) {
			printf("Expecting digit, '..', or '/', got '%s'.\n",
				    av[i]);
			break;
		    }
		    n = atoi(av[i]);
		    if (n < ce->necont) ce = ce->econt[n];
		    else {
			printf("Must be in range 0 - %d.\n", ce->necont);
			break;
		    }
		}
	    }
	}

	else if (!strcmp(cmd, "data")) {
	    if (av[1] && isdigit(*av[1])) {
		n = atoi(av[1]);
		if (n < ce->ndcont) {
		    printf(ce->dcont[n]);
		    fputs("\n", stdout);
		}
		else if (ce->ndcont == 0)
		    printf("No data at this node.\n");
		else printf("Must be in range 0 - %d.\n", ce->ndcont);
	    }
	}

	/* show where we are in the tree */
	else if (!strcmp(cmd, "where")) PrintLocation(ce, stdout);

	/* show where we are in the tree */
	else if (!strcmp(cmd, "pwd")) PrElemPlusID(ce);

	/* perform query with yes/no answer */
	else if (!strcmp(cmd, "q") && av[1] && av[2])
	    do_query(ce, av[1], av[2]);

	/* perform query printing paths to matching elements */
	else if (!strcmp(cmd, "find") && av[1] && av[2])
	    do_find(ce, av);

	/* list locations where specified ID(s) occur */
	else if (!strcmp(cmd, "id")) {
	    if (ac <= 1) continue;
	    if (*av[1] == '?') PrintIDList();
	    else {
		/* short: "id i1 i2 ...", long: "id -l i1 i2 ..." */
		if (!strcmp(av[1], "-l")) n = 2;
		else n = 1;
		for (i=n; i<ac; i++) {
		    if ((e = FindElemByID(av[i]))) {
			if (n == 2) {	/* long (multiline) format */
			    if (n != i) putchar('\n');
			    PrintLocation(e, stdout);
			}
			else PrElemPlusID(e);
		    }
		    else printf("Element with ID '%s' not found.\n", av[i]);
		}
	    }
	}

	/* show and set variables */
	else if (!strcmp(cmd, "show") && av[1]) {
	    printf("%s\n", FindMappingVal(Variables, av[1]));
	}
	else if (!strcmp(cmd, "set") && av[1] && av[2]) {
	    SetMappingNV(Variables, av[1], av[2]);
	}

	/* print summary of tag usage */
	else if (!strcmp(cmd, "sum")) {
	    if (ac > 1) PrintElemSummary(ce);
	    else PrintElemSummary(DocTree);
	}
	/* print element tree */
	else if (!strcmp(cmd, "tree")) {
	    if (ac > 1) PrintElemTree(ce);
	    else PrintElemTree(DocTree);
	}
	/* print statistics */
	else if (!strcmp(cmd, "stat")) {
	    if (ac > 1) PrintStats(ce);
	    else PrintStats(DocTree);
	}
	/* print context of each element of tree */
	else if (!strcmp(cmd, "cont")) {
	    if (ac > 1) PrintContext(ce);
	    else PrintContext(DocTree);
	}
	/* print translation, given transpec */
	else if (!strcmp(cmd, "tran")) {
	    FILE *fp;
	    if (ac > 1){
		if (!(fp = fopen(av[1], "w"))) {
		    perror("Can not open output file");
		    continue;
		}
	    }
	    else fp = stdout;
	    DoTranslate(ce, fp);
	    if (ac > 1) fclose(fp);
	}

	else if (!strcmp(cmd, "help") || *cmd == '?') {
	    sv = br_help_msg;
	    while (*sv) puts(*sv++);
	}

	/* quit (control-D also works) */
	else if (!strcmp(cmd, "quit")) break;

	else
	    fprintf(stderr, "Unknown command '%s' - ingored.\n", cmd);
    }
    putc(NL, stdout);
}

/* ______________________________________________________________________ */
/*  Do the "ls" command.
 *  Arguments:
 *	Pointer to element under consideration.
 *	Arg count from command line (this command, not the shell command).
 *	Arg vector.
 */

static void
ls_node(
    Element_t	*e,
    int		ac,
    char	**av
)
{
    int i;
    char buf[LINESIZE];

    if (ac > 1 && !strcmp(av[1], "-n")) {
	for(i=0; i<e->ncont; i++) {
	    if (IsContElem(e,i)) printf("%s\n", ContElem(e,i)->gi);
	    else if (IsContData(e,i)) printf("#data %s\n", ContData(e,i));
	    else if (IsContPI(e,i))   printf("#pi %s\n", ContData(e,i));
	}
	return;
    }

    printf("Element: %s\tLineNumber: %d\n", e->gi, e->lineno);
    if (e->parent)
	printf("Context: %s\n", FindContext(e, 20, buf));

    if (e->natts) {
	printf("%d attributes:\n", e->natts);
	for (i=0; i<e->natts; i++)
	    printf("\t%2d: %s = '%s'\n", i, e->atts[i].name, e->atts[i].sval);
    }
    if (e->entity) {
	printf("Entity & notation information:\n");
	if (e->entity->ename)
	    printf("Entity name:   %s\n", e->entity->ename);
	if (e->entity->nname)
	    printf("Notation name: %s\n", e->entity->nname);
	if (e->entity->sysid)
	    printf("Sys id:        %s\n", e->entity->sysid);
	if (e->entity->pubid)
	    printf("Pub id:        %s\n", e->entity->pubid);
	if (e->entity->fname)
	    printf("Filename:      %s\n", e->entity->fname);
    }

    if (e->my_eorder >= 0)
	printf("My order among my siblings: %d\n", e->my_eorder);

    if (e->necont) {
	printf("%d child element nodes:\n", e->necont);
	for(i=0; i<e->necont; i++) printf("\t%2d: %s\n", i, e->econt[i]->gi);
    }

    if (e->ndcont) {
	printf("%d child data nodes:\n", e->ndcont);
	for(i=0; i<e->ndcont; i++) {
	    if (strlen(e->dcont[i]) < 40) 
		printf("\t%2d: %s\n", i, e->dcont[i]);
	    else 
		printf("\t%2d: %-40.40s...\n", i, e->dcont[i]);
	}
    }
}

/* ______________________________________________________________________ */
/*  Perform query.  Syntax: find relationship gi.  Tells whether gi has
 *  given relationship to current element.  Result (message) sent to stdout.
 *  Args:
 *	Pointer to element under consideration.
 *	Pointer to name of relationship. (see FindRelByName() for names)
 *	Pointer to GI to look for.
 */

static void
do_query(
    Element_t	*e,
    char	*rel,
    char	*gi
)
{
    char	*cp;
    Relation_t	 r;
    Element_t	*ep;

    for (cp=gi; *cp; cp++) if (islower(*cp)) *cp = toupper(*cp);

    if ((r = FindRelByName(rel)) == REL_Unknown) {
	return;
    }
    ep = QRelation(e, gi, r);
    printf("%s, '%s' is%s %s of '%s'.\n", (ep ? "Yes" : "No"), gi,
		(ep ? "" : " not"), rel, e->gi);
}

/* ______________________________________________________________________ */
/* Print path to the element and its ID (if it has one) on a single line.
 *  Arguments:
 *	Pointer to element under consideration.
 */
static void
PrElemPlusID(
    Element_t	*e
)
{
    char buf[LINESIZE];

    if (e->id) printf("%s -- ID=%s\n", FindElementPath(e, buf), e->id);
    else printf("%s\n", FindElementPath(e, buf));
}

/* ______________________________________________________________________ */
/* Print path to the element and its ID (if it has one) on a single line.
 *  Arguments:
 *	Pointer to element under consideration.
 */

static void
match_gi(
    Element_t	*e,
    char	**av
)
{
    if (!strcmp(av[1], e->gi)) PrElemPlusID(e);
}

/*  Shorthand for defining simple finctions, which are just interfaces to
 *  calling QRelation().  DescendTree() only passes ptr to element. */
#define MATCH(Fun,Rel)	\
	static void Fun(Element_t *e, char **av) \
	{ if (QRelation(e, av[1], Rel)) PrElemPlusID(e);  }

MATCH(match_parent, REL_Parent)
MATCH(match_child,  REL_Child)
MATCH(match_anc,    REL_Ancestor)
MATCH(match_desc,   REL_Descendant)
MATCH(match_sib,    REL_Sibling)

static void
match_attr(
    Element_t	*e,
    char	**av
)
{
    char	*atval;

    if ((atval = FindAttValByName(e, av[1])) && !strcmp(av[2], atval))
	PrElemPlusID(e);
}

static void
match_cont(
    Element_t	*e,
    char	**av
)
{
    int		i;
    for (i=0; i<e->ncont; i++) {
	if (IsContData(e,i) && strstr(ContData(e,i), av[1])) {
	    PrElemPlusID(e);
	    return;
	}
    }
}

/*  Find an element, given the criteria on its command line.
 *  Arguments:
 *	Pointer to element under consideration.
 */
static void
do_find(
    Element_t	*e,
    char	**av
)
{
    av++;
    if (!strcmp(av[0], ".")) av++;
    else e = DocTree;
    if (!strcmp(av[0], "gi"))		DescendTree(e, match_gi, 0, 0, av);
    else if (!strcmp(av[0], "attr"))	DescendTree(e, match_attr, 0, 0, av);
    else if (!strcmp(av[0], "parent"))	DescendTree(e, match_parent, 0, 0, av);
    else if (!strcmp(av[0], "child"))	DescendTree(e, match_child, 0, 0, av);
    else if (!strcmp(av[0], "cont"))	DescendTree(e, match_cont, 0, 0, av);
    else if (!strcmp(av[0], "sib"))	DescendTree(e, match_sib, 0, 0, av);
    else if (!strcmp(av[0], "desc"))	DescendTree(e, match_desc, 0, 0, av);
    else if (!strcmp(av[0], "anc"))	DescendTree(e, match_anc, 0, 0, av);
    else fprintf(stderr, "Unknown find command: %s.\n", av[0]);
}

/* ______________________________________________________________________ */
