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
 *  Functions for printing information about an instance in the 'instant'
 *  program.  Most of these are fairly short and simple.
 *
 *  Entry points for this module:
 *	PrintElemSummary(elem)	print summary info of each element
 *	PrintContext(elem)	print context of each element
 *	PrintElemTree(elem)	print tree of document
 *	PrintStats(elem)	print statistics about doc tree
 *	PrintIDList(elem)	print list of IDs and element context
 *  Most Print*() functions start at subtree pointed to by 'elem'.
 * ________________________________________________________________________
 */

#ifndef lint
static char *RCSid =
  "$Header: /usr/src/docbook-to-man/Instant/RCS/info.c,v 1.2 1996/06/02 21:46:10 fld Exp $";
#endif

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>

#include "general.h"

/* ______________________________________________________________________ */
/*  Print a summary of each tag use in the instance.  Things like depth in
 *  the tree, number of children, parent, attributes.
 */

/*  Do the actual printing.  Print the info about the node.  If null,
 *  print a header for the columns.
 *  Arguments:
 *	Pointer to element structure of the node to print.
 */
static void
print_summ(
    Element_t	*e
)
{
    int i, n, dsize;
    char *hfmt="%-18.18s %4s %5s %4s %4s %s\n";
    char *fmt ="%-18.18s %4d %5d %4d %4d %s\n";

    if (e == NULL) {
	fprintf(outfp, hfmt, "Element", "Att", "Data", "Chd", "Dep", "Parent");
	return;
    }
    for (i=0,n=0; i<e->ncont; i++) if (IsContElem(e,i)) n++;
    for (i=0,dsize=0; i<e->ncont; i++)
	if (IsContElem(e,i)) dsize += strlen(e->cont[i].ch.data);
    fprintf(outfp, fmt, e->gi, e->natts, dsize, n, e->depth,
	e->parent ? e->parent->gi : "-");

    for (i=0; i<e->natts; i++) {
	fprintf(outfp, "%45d: %s = %s\n", i, e->atts[i].name,
	    e->atts[i].sval ? e->atts[i].sval : "empty");
    }
}

/*  Descend the tree, calling processing routine.
 *  Arguments:
 *	Pointer to element structure at top of tree to traverse.
 */
void
PrintElemSummary(
    Element_t	*e
)
{
    print_summ(0);
    DescendTree(e, print_summ, 0, 0, 0);
}

/* ______________________________________________________________________ */
/*  Print the context of each tag in the instance (i.e. the tag with its
 *  ancestors).
 */

/*  Do the actual printing.  Print the context of the node.
 *  Arguments:
 *	Pointer to element structure of the node to print.
 */
static void
print_context(
    Element_t	*e
)
{
    char buf[LINESIZE];

    fprintf(outfp, "%-22s %s\n", e->gi, FindContext(e, 10, buf));
}

/*  Descend the tree, calling processing routine.
 *  Arguments:
 *	Pointer to element structure at top of tree to traverse.
 */
void
PrintContext(
    Element_t	*e
)
{
    fprintf(outfp, "%-22s %s\n", "Element", "Context");
    fprintf(outfp, "%-22s %s\n", "---------------", "-----------");
    DescendTree(e, print_context, 0, 0, 0);

    putc(NL, outfp);
}

/* ______________________________________________________________________ */
/*  Print tree of the instance.  GI's are printed indented by their depth
 *  in the tree.
 */

/*  Do the actual printing.  Print the element name, indented the right amount.
 *  Arguments:
 *	Pointer to element structure of the node to print.
 */
static void
print_indent(
    Element_t	*e
)
{
    int		i, ne, nd;
    for(i=0; i<e->depth; i++) fputs(".  ", outfp);
    for(i=0,ne=0; i<e->ncont; i++) if (IsContElem(e,i)) ne++;
    for(i=0,nd=0; i<e->ncont; i++) if IsContData(e,i) nd++;
    fprintf(outfp, "%s  (%d,%d)\n", e->gi, ne, nd);
}

/*  Descend the tree, calling processing routine.
 *  Arguments:
 *	Pointer to element structure at top of tree to traverse.
 */
void
PrintElemTree(
    Element_t	*e
)
{
    DescendTree(e, print_indent, 0, 0, 0);
    putc(NL, outfp);
}

/* ______________________________________________________________________ */
/*  Print some statistics about the instance.
 */

/*  Accumulate the totals for the statistics.
 *  Arguments:
 *	Pointer to element structure of the node to print.
 *	Pointer to the total number of elements.
 *	Pointer to the total amount of content data.
 *	Pointer to the maximum depth of tree.
 */
static void
acc_tots(
    Element_t	*e,
    int		*tot_el,
    int		*tot_data,
    int		*max_depth
)
{
    int		i;
    for(i=0; i<e->necont; i++)
	acc_tots(e->econt[i], tot_el, tot_data, max_depth);
    for (i=0; i<e->necont; i++) (*tot_el)++;
    for (i=0; i<e->ndcont; i++) (*tot_data) += strlen(e->dcont[i]);
    if (e->depth > (*max_depth)) *max_depth = e->depth;
}

/*  Descend the tree (recursively), collecting the statistics.
 *  Arguments:
 *	Pointer to element structure of the node to print.
 *	Pointer to the total number of elements.
 *	Pointer to the total amount of content data.
 *	Pointer to the maximum depth of tree.
 */
static void
elem_usage(
    Element_t	*e,
    char	*name,
    int		*n_used,
    int		*nchars
)
{
    int		i;
    if (!strcmp(name, e->gi)) {
	(*n_used)++;
	for (i=0; i<e->ncont; i++)
	    if (IsContData(e,i)) (*nchars) += strlen(ContData(e,i));
    }
    for(i=0; i<e->necont; i++)
	elem_usage(e->econt[i], name, n_used, nchars);
}

/*  Descend the tree, calling processing routine.
 *  Arguments:
 *	Pointer to element structure at top of tree to traverse.
 */
void
PrintStats(
    Element_t	*top
)
{
    int		i, n;
    int		dif_el=0, tot_el=0, tot_data=0, nchars, max_depth=0;
    float	pct;

    fprintf(outfp, "%-22s %s   %s\n", "Element name",    "Occurrances", "Character Content");
    fprintf(outfp, "%-22s %s   %s\n", "---------------", "-----------", "-----------------");

    acc_tots(top, &tot_el, &tot_data, &max_depth);

    for (i=0; i<nUsedElem; i++) {
	n = 0;
	nchars = 0;
	elem_usage(top, UsedElem[i], &n, &nchars);
	if (n > 0) {
	    pct = 100.0 * (float)n / (float)tot_el;
	    fprintf(outfp, "%-22s %4d  %4.1f%%   %6d  %4d\n", UsedElem[i],
		n, pct, nchars, (nchars/n));
	    dif_el++;
	}
    }

    fprintf(outfp, "\nTotal of %d elements used, %d different ones.\n",
	tot_el, dif_el);
    fprintf(outfp, "Total character data: %d.\n", tot_data);
    fprintf(outfp, "Maximum element depth: %d.\n", max_depth);
    putc(NL, outfp);
}

/* ______________________________________________________________________ */
/* Print list of: ID, GI, input file, line number, separated by colons.
 * This is better for other programs to manipulate (like for keeping a
 * database of IDs in documents) than humans to read.
 */

void
PrintIDList()
{
    ID_t	*id;
    Element_t	*ep;

    for (id=IDList; id; id=id->next) {
	ep = id->elem;
	fprintf(outfp, "%s:%s:%s:%d\n", id->id, ep->gi,
		ep->infile?ep->infile:"-", ep->lineno);
    }
}

/* ______________________________________________________________________ */

