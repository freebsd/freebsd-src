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
 *  Program to manipulate SGML instances.
 *
 * Originally coded for OSF DTD tables, now recoded (fld 3/27/95)
 * for CALS-type tables (fragment taken from the DocBook DTD).  Then,
 * *really* upgraded to CALS tables by FLD on 5/28/96.
 *
 *  This module is for handling table markup, printing TeX or tbl
 *  (tbl) markup to the output stream.  Also, table markup checking is
 *  done here.  Yes, this depends on the DTD, but it makes translation
 *  specs much cleaner (and makes some things possible).
 *
 *  Incomplete / not implemented / limitations / notes:
 *	vertical alignment (valign attr)
 *	vertical spanning
 *	row separators are for the whole line, not per cell (the prog looks
 *		at rowsep for the 1st cell and applies it to the whole row)
 *	trusts that units in colwidths are acceptable to LaTeX and tbl
 *	"s" is an acceptable shorthand for "span" in model attributes
 *
 *  A note on use of OutputString():  Strings with backslashes (\) need lots
 *  of backslashes.  You have to escape them for the C compiler, and escape
 *  them again for OutputString() itself.
 * ________________________________________________________________________
 */

#ifndef lint
static char *RCSid =
  "$Header: /usr/src/docbook-to-man/Instant/RCS/tables.c,v 1.11 1996/06/15 03:45:02 fld Exp $";
#endif

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <memory.h>
#include <sys/types.h>
#include <errno.h>

#include <regexp.h>
#include "general.h"
#include "translate.h"

/* text width of page, in inches */
#define TEXTWIDTH	5.5
#define MAXCOLS		100
#define SPAN_NOT	0
#define SPAN_START	1
#define SPAN_CONT	2

/*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/
/*table parameters */

#define TBLMAXCOL	30	/* max number of columns in tbl table */
#define NAMELEN		40	/* max length of a name */
#define BOFTTHRESHOLD	35	/* text length over which to consider
				 * generating a block of filled text */


/* handy declarations */

typedef enum { Left, Right, Center, Justify, Char, Span } tblalign;

typedef enum { TGroup, THead, TFoot, TBody } tblsource;	/* source of a spec */


/* table line format information structures */

struct tblcolspec	{

	char		name[NAMELEN];	/* colspec's name */
	short		num;		/* column number */
	tblsource	source;		/* where defined */

	tblalign	align;		/* column's alignment */
	char		alignchar;	/* character for alignment */
	short		aligncharoff;	/* offset for alignment */
	char		colwidth[10];	/* width for column */
	char		colpwidth[10];	/* proportional widths for column */
	bool		colsep;		/* separator to right of column? */
	bool		rowsep;		/* separator to bottom of column? */
	short		moreRows;	/* value for Morerows */

	struct tblcolspec * next;	/* next colspec */
};

struct tblspanspec	{

	char		name[NAMELEN];	/* spanspec's name */
	tblsource	source;		/* where defined */

	struct tblcolspec * start;	/* start column */
	struct tblcolspec * end;	/* end column */
	tblalign	align;		/* span's alignment */
	char		alignchar;	/* character for alignment */
	short		aligncharoff;	/* offset for alignment */
	bool		colsep;		/* separator to right of column? */
	bool		rowsep;		/* separator to bottom of column? */

	struct tblspanspec * next;	/* next spanspec */
};

struct tblformat	{
	short	count;			/* count of rows matching this spec */

	short	cols;			/* # of columns */
	short	rowNum;			/* row number */
	char	colformat[TBLMAXCOL];	/* per-column formats */
	char	colwidth[TBLMAXCOL][10]; /* per-column widths */
	char	colpwidth[TBLMAXCOL][10]; /* per-column proportional widths */
	char	font[TBLMAXCOL][3];	/* column fonts (headers) */
	bool	colsep[TBLMAXCOL];	/* column separators */
	bool	rowsep[TBLMAXCOL];	/* row separators */
	short	moreRows[TBLMAXCOL];	/* moreRows indicator */

	struct tblformat * next;	/* for the next row */
};


/* table state info */

static short	tblcols = 0;		/* number of columns in the table */
static short	tblrow = 0;		/* the current row in the table */

static bool tblTGroupSeen = FALSE;	/* seen a TGroup in this table yet? */

static char *	tblFrame;		/* table frame info */
static bool	tblgcolsep;		/* global colsep (in table) */
static bool	tblgrowsep;		/* global rowsep (in table) */

static int	tblBOFTCount = 0;	/* count of bofts that we've created
					 * (per table) */
int	BOFTTextThresh = BOFTTHRESHOLD;
					/* length of text before we
					 * call it a BOFT */
static bool	tblboft = FALSE;	/* within a block of filled text? */
static bool	tblinBOFT = FALSE;	/* within a boft now? */

static struct tblformat * formP = 0;	/* THead/TBody format lines */

static struct tblcolspec * tblColSpec = 0;   /* colspec structure for table */
static struct tblspanspec * tblSpanSpec = 0; /* spanspec structure for table */

/*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/

/* these cover the attributes on the Table, TGroup, Colspec elements */
typedef struct {
    char	*cols;
    char	*align,     **align_v;
    char	*colwidth,  **colwidth_v;
    char	*colsep,    **colsep_v;
    char	*rowsep,    **rowsep_v;
    char	*frame;
    char	*orient;
    int		pgwide;
    int		n_align, n_model, n_colwidth, n_colsep;
    int		nc;
} TableInfo;


/* some flags, set when the table tag is processed, used later */
static int	rowsep, siderules;
static int	frametop, framebot, frameall;
static char	basemodel[128];	/* model for table (in formatting language) */
static int	spaninfo[MAXCOLS];	/* 100 columns, max */
static TableInfo	TheTab;

/* forward references */
void	SetTabAtts(Element_t *, TableInfo *, int);
void	FreeTabAtts(TableInfo	*);
void	ClearTable(TableInfo *);
void	CheckTable(Element_t *);
void	TblTStart(Element_t *, FILE *);
void	TblTEnd(Element_t *, FILE *);
void	TblTGroup(Element_t *, FILE *);
void	TblTGroupEnd(Element_t *, FILE *);
void	TblTFoot(Element_t *, FILE *);
void	TblBuildFormat(Element_t *, struct tblformat **, tblsource);
struct tblformat * TblBuild1Format(Element_t *, bool, tblsource);
char	TblGetAlign(short, Element_t *, tblsource);
char *	TblGetWidth(short, Element_t *, bool, tblsource);
char *	TblGetFont(short, Element_t *, tblsource);
bool	TblGetColSep(short, Element_t *, tblsource);
bool	TblGetRowSep(short, Element_t *, tblsource);
short	TblGetMoreRows(short, Element_t *, tblsource);
bool	TblColAdv(short, Element_t *, struct tblformat *, tblsource);
struct tblcolspec * TblEntryColSpec(short, Element_t *, tblsource);
struct tblspanspec * TblEntrySpanSpec(short, Element_t *, tblsource);
bool	TblFormatMatch(struct tblformat *, struct tblformat *);
void	TblPrintFormat(FILE *, struct tblformat *);
void	TblTRowStart(Element_t *, FILE *);
void	TblTRowEnd(Element_t *, FILE *);
void	TblTCellStart(Element_t *, FILE *);
int	TblCountContent(Element_t *);
void	TblTCellEnd(Element_t *, FILE *);
struct tblcolspec * TblDoColSpec(short, Element_t *, struct tblcolspec *, tblsource);
struct tblspanspec * TblDoSpanSpec(Element_t *, struct tblspanspec *, tblsource);
struct tblcolspec * TblFindColSpec(char *, tblsource);
struct tblcolspec * TblFindColNum(short, tblsource);
struct tblspanspec * TblFindSpanSpec(char *, tblsource);
void	TexTable(Element_t *, FILE *);
void	TexTableCellStart(Element_t *, FILE *);
void	TexTableCellEnd(Element_t *, FILE *);
void	TexTableRowStart(Element_t *, FILE *);
void	TexTableRowEnd(Element_t *, FILE *);
void	TexTableTop(Element_t *, FILE *);
void	TexTableBottom(Element_t *, FILE *);

/* ______________________________________________________________________ */
/*  Hard-coded stuff for CALS-style DTD tables.
 *  Here are the TABLE attributes (for handy reference):
 *
 *  Table/InformalTable:
 *	Colsep	   NUMBER	separate all columns in table?
 *	Frame	   (Top|Bottom|Topbot|All|Sides|None)	frame style
 *	Orient	   (Port | Land)	orientation
 *	Pgwide	   NUMBER	wide table?
 *	Rowsep	   NUMBER	separate all rows in the table?
 *	Tabstyle   NMTOKEN	FOSI table style
 *
 *  TGroup:
 *	Align	   (Left|Right|Center|Justify|Char)	alignment of cols
 *	Char	   CDATA	Alignment specifier
 *	Charoff	   NUTOKEN	    ""       ""
 *	Cols	   NUMBER	number of columns
 *	Colsep	   NUMBER	separate all columns in tgroup?
 *	Rowsep	   NUMBER	separate all rows in tgroup?
 *	TGroupstyle NMTOKEN	FOSI table group style
 *
 *  Colspec:
 *	Align      (Left|Right|Center|Justify|Char)	entry align
 *	Char       CDATA	Alignment specifier
 *	Charoff    NUTOKEN	    ""       ""
 *	Colname    NMTOKEN	Column identifier
 *	Colnum	   NUMBER	number of column
 *	Colsep     NUMBER	separate this col from next?
 *	Colwidth   CDATA	width spec
 *	Rowsep     NUMBER	serarate entry from following row?
 *
 *  SpanSpec:
 *	Align      (Left|Right|Center|Justify|Char)	entry align
 *	Char       CDATA	Alignment specifier
 *	Charoff    NUTOKEN	    ""       ""
 *	Colsep     NUMBER	separate this col from next?
 *	Nameend    NMTOKEN	name of rightmost col of a span
 *	Namest     NMTOKEN	name of leftmost col of a span
 *	Rowsep     NUMBER	serarate entry from following row?
 *	Spanname   NMTOKEN	name of a horiz. span
 *
 *  THead/TFoot/TBody:
 *	VAlign	   (Top | Middle | Bottom)	group placement
 *
 *  Row:
 *	Rowsep	   NUMBER	separate this row from next?
 *	VAlign	   (Top | Middle | Bottom)	row placement
 *
 *  Entry:
 *	Align      (Left|Right|Center|Justify|Char)	entry align
 *	Char       CDATA	Alignment specifier
 *	Charoff    NUTOKEN	    ""       ""
 *	Colname    NMTOKEN	Column identifier
 *	Colsep     NUMBER	separate this col from next?
 *	Morerows   NUMBER	number of addn'l rows in vert straddle
 *	Nameend    NMTOKEN	name of rightmost col of a span
 *	Namest     NMTOKEN	name of leftmost col of a span
 *	Rotate     NUMBER	90 degree rotation counterclockwise to table?
 *	Rowsep     NUMBER	serarate entry from following row?
 *	Spanname   NMTOKEN	name of a horiz. span
 *	VAlign     (Top | Middle | Bottom)	text vert alignment
 *  
 *
 ** OBSOLETE OSF DTD FORM (still used for TeX form):
 **  Usage in transpec: _calstable [tex|check|clear] ['aspect']
 **  where 'aspect' is:
 **	rowstart	stuff to do at start of a row (tests for spanning)
 **	rowend		stuff to do at end of a row (eg, rules, etc.)
 **	cellstart	stuff to do at start of a cell (eg, handle actual
 **			spanning instructions, etc.)
 **	cellend		stuff to do at end of a cell  (eg, cell separator)
 **	top		stuff to do at top of the table
 **			(like whether or not it needs a starting horiz rule)
 **	bottom		stuff to do at bottom of the table
 **			(like whether or not it needs an ending horiz rule)
 **	(nothing)	the 'cols' param to LaTeX's \begin{tabular}[pos]{cols}
 **			or 'options' and 'formats' part in tbl
 *
 *
 * New tbl form:
 *  Usage in transpec: _calstable [tbl] ['aspect']
 *  where 'aspect' is:
 *	tablestart	start a table and do style info
 *	tableend	end the table and clean up
 *	tablegroup	table TGroup (.T& if not 1st, line format info)
 *	tablegroupend	end a TGroup
 *	tablefoot	TFoot within a TGroup
 *	rowstart	start of a row
 *	rowend		end of a row
 *	entrystart	start of an entry (block of filled text, if
 *				appropriate)
 *	entryend	end of a cell  (eg, cell separator)
 */

/*  Procedure to
 *  Arguments:
 *	Pointer to element under consideration.
 *	FILE pointer to where to write output.
 *	Vector of args to _osftable
 *	Count of args to _osftable
 */
void
CALStable(
    Element_t	*e,
    FILE	*fp,
    char	**av,
    int		ac
)
{
    /* Check params and dispatch to appropriate routine */

    if (!strcmp(av[1], "tbl")) {

	if (ac > 2) {
	    if (!strcmp(av[2], "tablestart"))		TblTStart(e, fp);
	    else if (!strcmp(av[2], "tableend"))	TblTEnd(e, fp);
	    else if (!strcmp(av[2], "tablegroup"))	TblTGroup(e, fp);
	    else if (!strcmp(av[2], "tablegroupend"))	TblTGroupEnd(e, fp);
	    else if (!strcmp(av[2], "tablefoot"))	TblTFoot(e, fp);
	    else if (!strcmp(av[2], "rowstart"))	TblTRowStart(e, fp);
	    else if (!strcmp(av[2], "rowend"))		TblTRowEnd(e, fp);
	    else if (!strcmp(av[2], "entrystart"))	TblTCellStart(e, fp);
	    else if (!strcmp(av[2], "entryend"))	TblTCellEnd(e, fp);
	    else fprintf(stderr, "Unknown %s table instruction: %s\n",
		av[1], av[2]);
	}
	else	{
		fprintf(stderr, "Incomplete %s table instruction\n");
	}
    }

    else if (!strcmp(av[1], "tex")) {

        if (ac > 1 && !strcmp(av[1], "check")) CheckTable(e);

        else
        if (ac > 1 && !strcmp(av[1], "clear")) ClearTable(&TheTab);

	if (ac > 2) {
	    if (!strcmp(av[2], "cellstart"))	TexTableCellStart(e, fp);
	    else if (!strcmp(av[2], "cellend"))	TexTableCellEnd(e, fp);
	    else if (!strcmp(av[2], "rowstart")) TexTableRowStart(e, fp);
	    else if (!strcmp(av[2], "rowend"))	TexTableRowEnd(e, fp);
	    else if (!strcmp(av[2], "top"))	TexTableTop(e, fp);
	    else if (!strcmp(av[2], "bottom"))	TexTableBottom(e, fp);
	    else fprintf(stderr, "Unknown %s table instruction: %s\n",
		av[1], av[2]);
	}
	else TexTable(e, fp);
    }

    else fprintf(stderr, "Unknown table type: %s\n", av[1]);

}

/*  ClearTable -- start a new table process
 *
 */


void
ClearTable( TableInfo * t )
{
    memset(t, 0, sizeof(TableInfo));
}


/* ______________________________________________________________________ */
/*  Set values of the our internal table structure based on the table's
 *  attributes.  (This is called for tables, tgroups, colspecs, and rows,
 *  since tables and rows share many of the same attributes.)
 *  Arguments:
 *	Pointer to element under consideration.
 *	Pointer table info structure which will be filled in.
 *	Flag saying whether or not to set global variables based on attrs.
 */
void
SetTabAtts(
    Element_t	*e,
    TableInfo	*t,
    int		set_globals
)
{
    char	*at;
    Element_t	* ep;

    /* remember values of attributes */
    if ((at = FindAttValByName(e, "ALIGN")))	  t->align      = at;
    if ((at = FindAttValByName(e, "COLWIDTH")))	  t->colwidth   = at;
    if ((at = FindAttValByName(e, "COLSEP")))	  t->colsep     = at;
    if ((at = FindAttValByName(e, "FRAME")))	  t->frame      = at;
    if ((at = FindAttValByName(e, "COLS")))	  t->cols	= at;

    /* Set some things for later when processing this table */
    if (set_globals) {

	rowsep = 1;
	frametop = framebot = 1;		/* default style */

	/* For now we look at the first number of rowsep - it controls the
	 * horiz rule for then entire row.  (not easy to specify lines that
	 * span only some columns in tex or tbl. */
	if ((at = FindAttValByName(e, "ROWSEP")))	rowsep = atoi(at);
    }

    if (t->frame) {
	/* Top|Bottom|Topbot|All|Sides|None */
	if (!strcmp(t->frame, "NONE") || !strcmp(t->frame, "SIDES"))
	    frametop = framebot = 0;
	else if (!strcmp(t->frame, "TOP"))    framebot = 0;
	else if (!strcmp(t->frame, "BOTTOM")) frametop = 0;
    }

    /* tbl and tex like lower case for units. convert. */
    if (t->colwidth) {
	char *cp;
	for (cp=t->colwidth; *cp; cp++)
	    if (isupper(*cp)) *cp = tolower(*cp);
    }

    /* Now, split (space-separated) strings into vectors.  Hopefully, the
     * number of elements in each vector matches the number of columns.
     */
    t->align_v     = Split(t->align, &t->n_align, S_STRDUP|S_ALVEC);
    t->colwidth_v  = Split(t->colwidth, &t->n_colwidth, S_STRDUP|S_ALVEC);
    t->colsep_v    = Split(t->colsep, &t->n_colsep, S_STRDUP|S_ALVEC);

    /* Determin the _numeric_ number of columns, "nc".  MUST be specified
     * in Cols attribute of TGroup element.
     */
    if (t->cols) t->nc = atoi(t->cols);
}

/* ______________________________________________________________________ */

/*  Free the storage of info use by the table info structure.  (not the
 *  structure itself, but the strings its elements point to)
 *  Arguments:
 *	Pointer table info structure to be freed.
 */
void
FreeTabAtts(
    TableInfo	*t
)
{
    if (!t) return;
    if (t->align_v)     free(*t->align_v);
    if (t->colwidth_v)  free(*t->colwidth_v);
    if (t->colsep_v)    free(*t->colsep_v);
}

/* ______________________________________________________________________ */
/*  Check the attributes and children of the table pointed to by e.
 *  Report problems and inconsistencies to stderr.
 *  Arguments:
 *	Pointer to element (table) under consideration.
 */

void
CheckTable(
    Element_t	*e
)
{
    int		pr_loc=0;	/* flag to say if we printed location */
    int		i, r, c;
    Element_t	*ep, *ep2;
    float	wt;
    char	*tpref = "Table Check";		/* prefix for err messages */
    char	*ncolchk =
	"Table Check: %s ('%s') has wrong number of tokens.  Expecting %d.\n";

    if (strcmp(e->gi, "TABLE") &&
	strcmp(e->gi, "INFORMALTABLE") &&
	strcmp(e->gi, "TGROUP") &&
	strcmp(e->gi, "COLSPEC") &&
	strcmp(e->gi, "ROW") ) {
	fprintf(stderr, "%s: Not pointing to a table element(%s)!\n",
						tpref, e->gi);
	return;
    }

    FreeTabAtts(&TheTab);	/* free storage, if allocated earlier */
    SetTabAtts(e, &TheTab, 1);	/* look at attributes */

#if FALSE
    /* NCOLS attribute set? */
    if (!TheTab.ncols) {
	pr_loc++;
	fprintf(stderr, "%s: NCOLS attribute missing. Inferred as %d.\n",
		tpref, TheTab.nc);
    }

    /* ALIGN attribute set? */
    if (!TheTab.align) {
	pr_loc++;
	fprintf(stderr, "%s: ALIGN attribute missing.\n", tpref);
    }

    /* See if the number of cells in each row matches */
    for (r=0; r<e->necont && (ep=e->econt[r]); r++) {	/* each TGroup */
	for (i=0;  i<ep->necont && (ep2=ep->econt[i]);  i++)	{
	    if ( strcmp(ep2->gi, "TBODY") )	/* only TBodys */
		continue;

	    for (c=0;  c<ep2->necont;  c++)	{
	    	if (ep2->econt[c]->necont != TheTab.nc) {
		    pr_loc++;
		    fprintf(stderr, "%s: COLS (%d) differs from actual number of cells (%d) in row %d.\n",
				tpref, TheTab.nc, ep2->econt[c]->necont, c);
		}
	    }
	}
    }
#endif

    /* Check ALIGN */
    if (TheTab.align) {
	if (TheTab.nc != TheTab.n_align) {	/* number of tokens OK? */
	    pr_loc++;
	    fprintf(stderr, ncolchk, "ALIGN", TheTab.align, TheTab.nc);
	}
	else {				/* values OK? */
	    for (i=0; i<TheTab.nc; i++) {
		if (*TheTab.align_v[i] != 'C' && *TheTab.align_v[i] != 'L' &&
			*TheTab.align_v[i] != 'R') {
		    pr_loc++;
		    fprintf(stderr, "%s: ALIGN (%d) value wrong: %s\n",
			tpref, i, TheTab.align_v[i]);
		}
	    }
	}
    }

    /* check COLWIDTH */
    if (TheTab.colwidth) {
	if (TheTab.nc != TheTab.n_colwidth) {	/* number of tokens OK? */
	    pr_loc++;
	    fprintf(stderr, ncolchk, "COLWIDTH", TheTab.colwidth, TheTab.nc);
	}
	else {				/* values OK? */
	    for (i=0; i<TheTab.nc; i++) {

		/* check that the units after the numbers are OK
		    we want "in", "cm".
		 */
	    }
	}
    }

    /* check COLSEP */
    if (TheTab.colsep) {
	if (TheTab.nc != TheTab.n_colsep) {	/* number of tokens OK? */
	    pr_loc++;
	    fprintf(stderr, ncolchk, "COLSEP", TheTab.colsep, TheTab.nc);
	}
	else {				/* values OK? */
	    for (i=0; i<TheTab.nc; i++) {
	    }
	}
    }

    if (pr_loc) {
	fprintf(stderr, "%s: Above problem in table located at:\n", tpref);
	PrintLocation(e, stderr);
    }
}

/* ______________________________________________________________________ */

/*  Look at colspec attribute for spanning.  If set, remember info for when
 *  doing the cells.  Called by TblTableRowStart() and TexTableRowStart().
 *  Arguments:
 *	Pointer to element (row) under consideration.
 */
int
check_for_spans(
    Element_t	*e
)
{
    char	*at;
    char	**spans;
    int		n, i, inspan;

#if FALSE	/* NOT IMPLEMENTED RIGHT NOW */

    /* See if COLSPEC element present */
    for (i=0;  i < e->necont;  i++)	{
	
    }


    if ((at = FindAttValByName(e, "MODEL"))) {

	/* Split into tokens, then look at each for the word "span" */
	n = TheTab.nc;
	spans = Split(at, &n, S_STRDUP|S_ALVEC);

	/* Mark columns as start-of-span, in-span, or not spanned.  Remember
	 * in at list, "spaningo".  (Span does not make sense in 1st column.)
	 */
	for (i=1,inspan=0; i<n; i++) {
	    if (StrEq(spans[i], "span") || StrEq(spans[i], "s")) {
		if (inspan == 0) spaninfo[i-1] = SPAN_START;
		spaninfo[i] = SPAN_CONT;
		inspan = 1;
	    }
	    else {
		spaninfo[i] = SPAN_NOT;
		inspan = 0;
	    }
	}
	free(*spans);				/* free string */
	free(spans);				/* free vector */
	spaninfo[TheTab.nc] = SPAN_NOT;		/* after last cell */
	return 1;
    }
    /* if model not set, mark all as not spanning */
    else

#endif	/* NOT CURRENTLY IMPLEMENTED */

	for (i=0; i<MAXCOLS; i++) spaninfo[i] = SPAN_NOT;
    return 0;
}

/* ______________________________________________________________________ */
/* Do the "right thing" for the table spec for TeX tables.  This will
 * generate the arg to \begin{tabular}[xxx].
 *  Arguments:
 *	Pointer to element (table) under consideration.
 *	FILE pointer to where to write output.
 */
void
TexTable(
    Element_t	*e,
    FILE	*fp
)
{
    int		i, n;
    float	tot;
    char	*cp, wbuf[1500], **widths=0, **widths_v=0;

    FreeTabAtts(&TheTab);	/* free storage, if allocated earlier */
    SetTabAtts(e, &TheTab, 1);	/* look at attributes */
    SetTabAtts(e->econt[0], &TheTab, 1);	/* attrs of TGroup */

    /* Figure out the widths, based either on "colwidth".
     */
    if (TheTab.colwidth && TheTab.nc == TheTab.n_colwidth) {
	widths = TheTab.colwidth_v;
    }

    siderules = 1;
    if (TheTab.frame)
	if (strcmp(TheTab.frame, "ALL") && strcmp(TheTab.frame, "SIDES"))
	    siderules = 0;

    if (siderules) OutputString("|", fp, 1);
    for (i=0; i<TheTab.nc; i++) {
	/* If width specified, use it; else if align set, use it; else left. */
	if (widths && widths[i][0] != '0' && widths[i][1] != EOS) {
	    fprintf(fp, "%sp{%s}", (i?" ":""), widths[i]);
	}
	else if (TheTab.align && TheTab.nc == TheTab.n_align) {
	    fprintf(fp, "%s%s", (i?" ":""), TheTab.align_v[i]);
	}
	else
	    fprintf(fp, "%sl", (i?" ":""));
	/* See if we want column separators. */
	if (TheTab.colsep) {

	    if ( (i+1) < TheTab.nc ) {
		if ( *TheTab.colsep_v[i] == '1' ) {
		    fprintf(fp, " |");
		}
		if ( *TheTab.colsep_v[i] == '2' ) {
		    fprintf(fp, " ||");
		}
	    }

	}
    }
    if (siderules) OutputString("|", fp, 1);

    if (widths_v) free(widths_v);
}

/*
 *  Arguments:
 *	Pointer to element (cell) under consideration.
 *	FILE pointer to where to write output.
 */
void
TexTableCellStart(
    Element_t	*e,
    FILE	*fp
)
{
    int		n, i;
    char	buf[50], *at;

    if (spaninfo[e->my_eorder] == SPAN_START) {
	for (i=e->my_eorder+1,n=1; ; i++) {
	    if (spaninfo[i] == SPAN_CONT) n++;
	    else break;
	}
	sprintf(buf, "\\\\multicolumn{%d}{%sc%s}", n,
		(siderules?"|":""), (siderules?"|":""));
	OutputString(buf, fp, 1);
    }
#ifdef New
    if ((at = FindAttValByName(e->parent, "ALIGN"))) {
	/* no span, but user wants to change the alignment */
	h_v = Split(wbuf, 0, S_ALVEC|S_STRDUP);
	OutputString("\\\\multicolumn{1}{%sc%s}", n,
		fp, 1);
    }
#endif

    if (spaninfo[e->my_eorder] != SPAN_CONT) OutputString("{", fp, 1);
}

/*
 *  Arguments:
 *	Pointer to element (cell) under consideration.
 *	FILE pointer to where to write output.
 */
void
TexTableCellEnd(
    Element_t	*e,
    FILE	*fp
)
{
    if (spaninfo[e->my_eorder] != SPAN_CONT) OutputString("} ", fp, 1);

    /* do cell/col separators */
    if (e->my_eorder < (TheTab.nc-1)) {
	if (spaninfo[e->my_eorder] == SPAN_NOT ||
			spaninfo[e->my_eorder+1] != SPAN_CONT)
	    OutputString("& ", fp, 1);
    }
}

/*  Look at model for spanning.  If set, remember it for when doing the cells.
 *  Arguments:
 *	Pointer to element (row) under consideration.
 *	FILE pointer to where to write output.
 */
void
TexTableRowStart(
    Element_t	*e,
    FILE	*fp
)
{
    check_for_spans(e);
}

/*
 *  Arguments:
 *	Pointer to element (row) under consideration.
 *	FILE pointer to where to write output.
 */
void
TexTableRowEnd(
    Element_t	*e,
    FILE	*fp
)
{
    char	*at;

    /* check this row's attributes */
    if ((at = FindAttValByName(e, "ROWSEP"))) {
	if (at[0] == '1') OutputString("\\\\\\\\[2mm] \\\\hline ", fp, 1);
    }
    else if (rowsep) OutputString("\\\\\\\\ ", fp, 1);
    else 
        OutputString("\\\\\\\\ ", fp, 1);

}

/*
 *  Arguments:
 *	Pointer to element (table) under consideration.
 *	FILE pointer to where to write output.
 */
void
TexTableTop(Element_t *e, FILE *fp)
{
    if (frametop) OutputString("\\\\hline", fp, 1);
}

void
TexTableBottom(Element_t *e, FILE *fp)
{
    if (framebot) OutputString("\\\\hline", fp, 1);
}

/* ______________________________________________________________________ */
/* ______________________________________________________________________ */
/* ______________________________________________________________________ */
/* ______________________________________________________________________ */
/* ______________________________________________________________________ */
/* ___________________________|             |____________________________ */
/* ___________________________|  TBL STUFF  |____________________________ */
/* ___________________________|             |____________________________ */
/* ___________________________|_____________|____________________________ */
/* ______________________________________________________________________ */
/* ______________________________________________________________________ */
/* ______________________________________________________________________ */
/* ______________________________________________________________________ */



/*	TblTStart()  --  start a table and do style information
 *
 *  TO DO:
 *
 *	do .TS
 *	find global rowsep and colsep
 */


void
TblTStart(Element_t * ep,
	  FILE * fP)
{
	register char * cp;
	register struct Element_t * ep2;



	OutputString("^.TS^", fP, 1);

	tblTGroupSeen = FALSE;
	tblinBOFT = FALSE;	/* within a boft? */
	tblBOFTCount = 0;	/* count of Blocks of Filled Text that
				 * we've created */

	tblgcolsep = (cp = FindAttValByName(ep, "COLSEP")) && !strcmp(cp, "1");
	tblgrowsep = (cp = FindAttValByName(ep, "ROWSEP")) && !strcmp(cp, "1");
}

/*      TblTEnd()  --  end a table and do any cleanup
 *
 *  TO DO:
 *
 *	do .TE
 *
 *	deallocate format line info
 */



void
TblTEnd(Element_t * ep,
	FILE * fP)
{
	register struct tblformat * ffp, * ffp2;


	if ( tblBOFTCount > 31 )	{
		fprintf(stderr, "# warning, line %d: created %d blocks of filled text in one table\n",
					ep->lineno, tblBOFTCount);
		fprintf(stderr, "#\t\t(31 is the limit in some systems)\n");
	}

	OutputString("^.TE^", fP, 1);

	for ( ffp=formP;  ffp;  ffp=ffp2 )	{
		ffp2 = ffp->next;
		free(ffp);		/* clear entire list */
	}
	formP = 0;
}

/*      TblTTGroup()  --  do body work (row format info)
 *
 *  TO DO:
 *
 *	set number of columns
 *
 *	if this is the first TGroup of this table, do style info:
 *	   a. alignment
 *	   b. defaults:  tab
 *	   c. box vx allbox
 *
 *	do format info:
 *	   a. generate tableformat structure
 *	   b. output it
 *
 *	prepare structures for colspecs and spanspecs
 *
 */



void
TblTGroup(Element_t * ep,
	  FILE * fP)
{
	register int i, j, k;
	register char * cp, * cp2;
	register Element_t * ep2, ep3;
	register struct tblcolspec * tcsp, * tcsp2;
	register struct tblspanspec * tssp, * tssp2;


	tblColSpec = 0;		/* make sure they're clear */
	tblSpanSpec = 0;

    /* set the number of columns */

    	tblcols = atoi(FindAttValByName(ep, "COLS"));

    /* do colspecs */

    	tblColSpec = tcsp = TblDoColSpec(0, ep, 0, TGroup);
    			/* do TGroup first -- it becomes the default */

	for ( i=0, k=1;  i < ep->necont;  i++ )	{

		if ( !strcmp(ep->econt[i]->gi, "COLSPEC") )	{
			tcsp2 = TblDoColSpec(k, ep->econt[i], tblColSpec, TGroup);
			tcsp->next = tcsp2;	/* put into list */
			tcsp = tcsp2;
			k = tcsp2->num + 1;	/* next column number */
		}

		if ( !strcmp(ep->econt[i]->gi, "THEAD") )	{
			ep2 = ep->econt[i];
			for ( j=0, k=1;  j < ep2->necont;  j++ )	{
				if ( !strcmp(ep2->econt[j]->gi, "COLSPEC") )	{
					tcsp2 = TblDoColSpec(k, ep2->econt[j], tblColSpec, THead);
					tcsp->next = tcsp2;	/* put into list */
					tcsp = tcsp2;
					k = tcsp2->num + 1;	/* next column number */
				}
			}
		}

		if ( !strcmp(ep->econt[i]->gi, "TFOOT") )	{
			ep2 = ep->econt[i];
			for ( j=0, k=1;  j < ep2->necont;  j++ )	{
				if ( !strcmp(ep2->econt[j]->gi, "COLSPEC") )	{
					tcsp2 = TblDoColSpec(k, ep2->econt[j], tblColSpec, TFoot);
					tcsp->next = tcsp2;	/* put into list */
					tcsp = tcsp2;
					k = tcsp2->num + 1;	/* next column number */
				}
			}
		}

		if ( !strcmp(ep->econt[i]->gi, "TBODY") )	{
			ep2 = ep->econt[i];
			for ( j=0, k=1;  j < ep2->necont;  j++ )	{
				if ( !strcmp(ep2->econt[j]->gi, "COLSPEC") )	{
					tcsp2 = TblDoColSpec(k, ep2->econt[j], tblColSpec, TBody);
					tcsp->next = tcsp2;	/* put into list */
					tcsp = tcsp2;
					k = tcsp2->num + 1;	/* next column number */
				}
			}
		}
	}

    /* do spanspecs */

	tblSpanSpec = tssp = TblDoSpanSpec(ep, 0, TGroup);
			/* do TGroup first -- it becomes the default */

	for ( i=0;  i < ep->necont;  i++ )	{
		if ( !strcmp(ep->econt[i]->gi, "SPANSPEC") )	{
			tssp2 = TblDoSpanSpec(ep->econt[i], tblSpanSpec, TGroup);
			tssp->next = tssp2;	/* put into list */
			tssp = tssp2;
		}
	}


    /* if this is the first TGroup in this table, do style stuff */

	if ( ! tblTGroupSeen )	{

		OutputString("tab(\007)", fP, 1);

		ep2 = ep->parent;
		if ( ! (tblFrame = FindAttValByName(ep2, "FRAME")) )
			tblFrame = "";

		if ( !strcmp(tblFrame, "ALL") )	{
			if ( tcsp->colsep && tcsp->rowsep )
				OutputString(" allbox", fP, 1);
			else
				OutputString(" box", fP, 1);
		}

		if ( (cp = FindAttValByName(ep, "ALIGN")) &&
		     !strcmp(cp, "CENTER") )	{
		     	OutputString(" center", fP, 1);
		}

		OutputString(";\n", fP, 1);
		
		tblTGroupSeen = TRUE;
	}


    /* do format stuff -- step through all THead rows then all TBody
     * rows.  Build a list of tblformats that describe all of them.
     * then output the resulting list.
     */

    	for ( i=0;  i < ep->necont;  i++ )	{
		if ( !strcmp(ep->econt[i]->gi, "THEAD") )	{
			TblBuildFormat(ep->econt[i], &formP, THead);
						/* add in those rows */
			break;
    		}
    	}

    	for ( i=0;  i < ep->necont;  i++ )	{
		if ( !strcmp(ep->econt[i]->gi, "TBODY") )	{
			TblBuildFormat(ep->econt[i], &formP, TBody);
						/* add in those rows */
			break;
    		}
    	}

	TblPrintFormat(fP, formP);

	tblrow = 0;		/* the current row within this format */
}

/*      TblTGroupEnd()  --  end a TGroup
 *
 *  TO DO:
 *
 *	deallocate colspecs and spanspecs
 */


void
TblTGroupEnd(Element_t * ep,
	      FILE * fP)
{
	register struct tblcolspec * tcsp, * tcsp2;
	register struct tblspanspec * tssp, * tssp2;


	for ( tcsp=tblColSpec;  tcsp;  tcsp=tcsp2 )	{
		tcsp2 = tcsp->next;
		free(tcsp);
	}
	for ( tssp=tblSpanSpec;  tssp;  tssp=tssp2 )	{
		tssp2 = tssp->next;
		free(tssp);
	}
}

/*      TblTTFoot()  --  do body foot work (row format info)
 *
 *  TO DO:
 *
 *	do format info:
 *	   a. generate tableformat structure
 *	      i. if it is only 1 line long and matches the
 *		 prevailing format, just output rows.
 *	     ii. else, output a .T& and the new format specs
 */



void
TblTFoot(Element_t * ep,
	 FILE * fP)
{
	register struct tblformat * ffp, * ffp2;
	static struct tblformat * tfp, * tfp2;


	TblBuildFormat(ep, &tfp, TFoot);	/* gen format for the foot */

	for ( tfp2=formP;  tfp2 && tfp2->next;  tfp2=tfp2->next )
		;

	if ( tfp->next || !TblFormatMatch(tfp, tfp2) )	{

		for ( ffp=formP;  ffp;  ffp=ffp2 )	{
			ffp2 = ffp->next;
			free(ffp);		/* clear entire list */
		}

		formP = tfp;	/* this becomes the prevailing format */

		OutputString("^.T&^", fP, 1);
		TblPrintFormat(fP, formP);
	}

	tblrow = 0;		/* the current row within this format */
}

/*	TblBuildFormat()  --  build a format structure out of a set of
 *				rows and columns
 *
 */


void
TblBuildFormat(Element_t * ep,		/* parent of rows.. */
	       struct tblformat ** fp,	/* pointer to head of struct we're
	       				 * building */
	       tblsource source)	/* type of record */
{
	register int i;
	register struct tblformat * lfp; /* "current" format */
	register struct tblformat * nfp; /* the next format */


	for ( lfp= *fp;  lfp && lfp->next;  lfp=lfp->next )
		;			/* find end of format list */

    	for ( i=0;  i < ep->necont;  i++ )
		if ( !strcmp(ep->econt[i]->gi, "ROW") )
			break;		/* find where rows start */

	for (  ;  i < ep->necont;  i++ )	{

		nfp = TblBuild1Format(ep->econt[i], FALSE, source);
						/* do one row */

		if ( !lfp )
			lfp = *fp = nfp;	/* first one */
		else
		if ( TblFormatMatch(lfp, nfp) )
			lfp->count++;		/* matches */
		else	{
			lfp->count = 1;		/* only 1 so far */
			lfp->next = nfp;	/* new one */
			lfp = nfp;
		}
	}
}

/*	TblBuild1Format()  --  build one row's worth of format information
 *
 */



struct tblformat *
TblBuild1Format(Element_t * rp,		/* the row to deal with */
		bool addinRowsep,	/* insert rowsep into model? */
		tblsource source)	/* type type of row */
{
	register int i;
	register bool allProp;
	float totalProp;
	register struct tblformat * tfp;
	register Element_t * ep;	/* entry pointer */


	Calloc(1, tfp, struct tblformat);
	tfp->cols = tblcols;
	ep = (rp->necont) ? rp->econt[0] : 0;	/* first entry */
	allProp = TRUE;
	totalProp = 0;

	for ( i=1;  i <= tblcols;  i++ )	{
		tfp->colformat[i] = TblGetAlign(i, ep, source);
		strcpy(tfp->colwidth[i], TblGetWidth(i, ep, TRUE, source));
		strcpy(tfp->colpwidth[i], TblGetWidth(i, ep, FALSE, source));
		if ( allProp )	{
			allProp = tfp->colpwidth[i][0];
			totalProp += atof(tfp->colpwidth[i]);
		}
		strcpy(tfp->font[i], TblGetFont(i, ep, source));
		tfp->colsep[i] = tblgcolsep || TblGetColSep(i, ep, source);
		if ( addinRowsep )
			tfp->rowsep[i] = tblgrowsep || TblGetRowSep(i, ep, source);
		tfp->moreRows[i] = TblGetMoreRows(i, ep, source);

		if ( (i < rp->necont) && TblColAdv(i, ep, tfp, source) )	{
			ep = rp->econt[i];
		}
	}

    /* turn proportional widths into real widths */

    	if ( allProp )	{
    		for ( i=1;  i <= tblcols;  i++ )	{
    			sprintf(tfp->colwidth[i], "%fi",
    				(atof(tfp->colpwidth[i]) / totalProp) * TEXTWIDTH);
    		}
    	}

	return tfp;
}

/*	TblGetAlign()  --  get alignment spec for a entry
 *
 */


char
TblGetAlign(short col,			/* column number */
	    Element_t * entry,		/* the entry */
	    tblsource	source)		/* context */
{
	register struct tblcolspec * tcsp;
	register struct tblspanspec * tssp;
	register tblalign talign;


	if ( entry && (tssp = TblEntrySpanSpec(col, entry, source)) )	{
		talign = tssp->align;
		free(tssp);
	} else
	if ( entry && (tcsp = TblEntryColSpec(col, entry, source)) )	{
		talign = tcsp->align;
		free(tcsp);
	} else	{
		return 'l';
	}

	switch ( talign )	{
	case Left:	return 'l';
	case Right:	return 'r';
	case Center:	return 'c';
	case Justify:	return 'l';
	case Char:	return 'd';
	case Span:	return 's';
	}
}

/*	TblGetWidth()  --  get width spec, if any, for a entry
 *
 */


char *
TblGetWidth(short col,			/* column number */
	    Element_t * entry,		/* the entry */
	    bool	literal,	/* literal (or proportional) */
	    tblsource	source)		/* context */
{
	register struct tblcolspec * tcsp;
	register struct tblspanspec * tssp;
	static char colWidth[10];


	colWidth[0] = 0;

	if ( entry &&
	     (tcsp = TblEntryColSpec(col, entry, source)) &&
	     tcsp->colwidth[0] )	{

		if ( !strstr(tcsp->colwidth, "*") )	{
			if ( literal )
				strcpy(colWidth, tcsp->colwidth);
		} else	{
			if ( ! literal )
				strcpy(colWidth, tcsp->colwidth);
		}
		free(tcsp);
	}

	return colWidth;
}

/*	TblGetFont()  --  get font spec, if any, for a entry
 *
 */


char *
TblGetFont(short col,			/* column number */
	   Element_t * entry,		/* the entry */
	   tblsource source)		/* context */
{
	register struct tblcolspec * tcsp;
	register struct tblspanspec * tssp;


	return "";
}

/*	TblGetColSep()  --  get column separater spec, if any, for a entry
 *
 */


bool
TblGetColSep(short col,			/* column number */
	     Element_t * entry,		/* the entry */
	     tblsource	source)		/* context */
{
	register struct tblcolspec * tcsp;
	register struct tblspanspec * tssp;
	register bool colsep;


	if ( entry && (tssp = TblEntrySpanSpec(col, entry, source)) )	{
		colsep = tssp->colsep;
		free(tssp);
	} else
	if ( entry && (tcsp = TblEntryColSpec(col, entry, source)) )	{
		colsep = tcsp->colsep;
		free(tcsp);
	} else
		colsep = FALSE;

	return colsep;
}

/*	TblGetRowSep()  --  get row separater spec, if any, for a entry
 *
 */


bool
TblGetRowSep(short col,			/* column number */
	     Element_t * entry,		/* the entry */
	     tblsource	source)		/* context */
{
	register struct tblcolspec * tcsp;
	register struct tblspanspec * tssp;
	register bool rowsep;

	if ( entry && (tssp = TblEntrySpanSpec(col, entry, source)) )	{
		rowsep = tssp->rowsep;
		free(tssp);
	} else
	if ( entry && (tcsp = TblEntryColSpec(col, entry, source)) )	{
		rowsep = tcsp->rowsep;
		free(tcsp);
	} else	{
		rowsep = FALSE;
	}

	return rowsep;
}

/*	TblGetmoreRows()  --  get moreRows value
 *
 */


bool
TblGetMoreRows(short col,		/* column number */
	       Element_t * entry,	/* the entry */
	       tblsource	source)	/* context */
{
	register char * cp;


	if ( cp = FindAttValByName(entry, "MOREROWS") )
		return atoi(cp);
	else
		return 0;
}

/*	TblColAdv()  --  advance pointer to next entry, if appropriate
 *
 */


bool
TblColAdv(short col,		/* the current column */
	  Element_t *ep,	/* pointer to entry */
	  struct tblformat * tfp, /* pointer to prevailing format */
	  tblsource source)	/* context */
{
	register bool bump;
	register struct tblspanspec * tssp;


	bump = TRUE;

	if ( tssp = TblEntrySpanSpec(col, ep, source) )	{
		bump = tssp->align != Span;
		free(tssp);
	}

	return bump;
}

/*	TblEntryColSpec()  --  get a completely localized colspec for an entry
 *
 */


struct tblcolspec *
TblEntryColSpec(short num,		/* column number */
		Element_t * ep,		/* entry */
		tblsource source)	/* context */
{
	register int i;
	register bool throwAway;
	register char * cp;
	register struct tblcolspec * tcsp, * tcsp2;


	tcsp = tcsp2 = 0;
	throwAway = FALSE;

	if ( (cp = FindAttValByName(ep, "COLNAME")) )	{
		if ( ! (tcsp = TblFindColSpec(cp, source)) )	{
			fprintf(stderr, "? can't find column name '%s'\n", cp);
		}
	}

	if ( tcsp2 = TblFindColNum(num, source) )	{
		tcsp = TblDoColSpec(num, ep, tcsp2, source);
		throwAway = TRUE;
	}

	tcsp2 = TblDoColSpec(num, ep, tcsp, source);

	if ( throwAway )
		free(tcsp);

	return tcsp2;
}

/*	TblEntrySpanSpec()  --  get a completely localized spanspec for an entry
 *
 */


struct tblspanspec *
TblEntrySpanSpec(short num,		/* column number */
		 Element_t * ep,	/* entry */
		 tblsource source)	/* context */
{
	register char * cp, * cp2;
	register struct tblspanspec * tssp, * tssp2;


	tssp2 = 0;

	if ( !(cp = FindAttValByName(ep, "SPANNAME")) ||
	     !(tssp2 = TblFindSpanSpec(cp, source)) )	{

	     	if ( !FindAttValByName(ep, "NAMEST") )
	     		return 0;
	}

	tssp = TblDoSpanSpec(ep, tssp2, source);

	if ( tssp->start && tssp->end &&
	     (tssp->start->num < num) && (tssp->end->num >= num) )	{
		tssp->align = Span;
	}

	return tssp;
}

/*	TblFormatMatch()  --  compare two format rows for consistency
 *
 */


bool
TblFormatMatch(struct tblformat * tf1,	/* one row */
	       struct tblformat * tf2)	/* the other */
{
	register int i;

	if ( tf1->cols != tf2->cols )	{
		return FALSE;
	}

	for ( i=0;  i < tf1->cols;  i++ )	{

		if ( tf1->colformat[i] != tf2->colformat[i] )	{
			return FALSE;
		}
		if ( strcmp(tf1->colwidth[i], tf2->colwidth[i]) )	{
			return FALSE;
		}
		if ( strcmp(tf1->font[i], tf2->font[i]) )	{
			return FALSE;
		}
		if ( tf1->colsep[i] != tf2->colsep[i] )	{
			return FALSE;
		}
		if ( tf1->rowsep[i] != tf2->rowsep[i] )	{
			return FALSE;
		}
		if ( tf1->moreRows[i] || tf2->moreRows[i] )	{
			return FALSE;
		}
	}

	return TRUE;
}

/*	TblPrintFormat()  --  print a tbl format structure
 *
 */


void
TblPrintFormat(FILE * fP,		/* where to print */
	       struct tblformat * tfp)	/* the structure */
{
	register int i;
	register struct tblformat * tfp2, * tfp3;
	static char buf[3] = "\000\000";


	for ( tfp2=tfp, tfp3=0;  tfp2;  tfp2=tfp2->next )	{
		for ( i=1;  i <= tfp->cols;  i++ )	{
			if ( i > 1 )
				OutputString(" ", fP, 1);
			if ( tfp3 && tfp3->moreRows[i] )
				OutputString("\\^", fP, 1);
			else	{
				buf[0] = tfp2->colformat[i];
				OutputString(buf, fP, 1);
			}
			if ( tfp2->colwidth[i][0] )	{
				OutputString("w(", fP, 1);
				OutputString(tfp2->colwidth[i], fP, 1);
				OutputString(")", fP, 1);
			}
			if ( tfp2->font[i][0] )
				OutputString(tfp2->font[i], fP, 1);
			if ( tfp2->colsep[i] )
				OutputString("|", fP, 1);
		}
		if ( ! tfp2->next )
			OutputString(".", fP, 1);
		OutputString("^", fP, 1);
		tfp3 = tfp2;
	}
}

/*      TblTRowStart()  --  start a row (not much to do)
 *
 *  TO DO:
 *
 *	nothing..
 *
 */



void
TblTRowStart(Element_t * ep,
	     FILE * fP)
{

    /* nothing to do */

	tblrow++;	/* except note that we're within a new row */

}

/*      TblTRowEnd()  --  end a row
 *
 *  TO DO:
 *
 *	output a row end character (newline)
 *	if the current row had a rowsep, then output a "fake" row
 *	with underlines in the proper place(s).
 */



void
TblTRowEnd(Element_t * ep,
	   FILE * fP)
{
	register int i, k;
	register tblsource source;
	register bool startedRow, didSep;
	register struct tblformat * rfp;


	OutputString("^", fP, 1);

    /* get the format for this row */

    	if ( !strcmp(ep->parent->gi, "TFoot") )
    		source = TFoot;
	else
    	if ( !strcmp(ep->parent->gi, "THead") )
		source = THead;
	else
		source = TBody;

    	rfp = TblBuild1Format(ep, TRUE, source);
	startedRow = FALSE;
	didSep = FALSE;

	for ( i=1;  i <= formP->cols;  i++ )	{
		if ( rfp->rowsep[i] ||
		     (didSep && (rfp->colformat[i] == 's')) )	{
			if ( ! startedRow )	{
				OutputString("^", fP, 1);
				for ( k=1;  k < i;  k++ )
					OutputString("\007", fP, 1);
				startedRow = TRUE;
			}
			OutputString("_\007", fP, 1);
			didSep = TRUE;
		} else	{
		if ( startedRow )
			OutputString("\007", fP, 1);
		}
		didSep = FALSE;
	}
	free(rfp);		/* clear that row.. */

	if ( startedRow )
		OutputString("^", fP, 1);
}

/*      TblTEntryStart()  --  start an entry (block of filled text if
 *				appropriate)
 *
 *  TO DO:
 *
 *	if text length > BOFTTextThresh or there is PI,
 *	then output "T{\n", else do nothing
 *
 */



void
TblTCellStart(Element_t * ep,
	      FILE * fP)
{
	register int i;
	register Element_t * ep2;
	register bool sawPI;


	for ( i=0, sawPI=FALSE;  (i < ep->ncont) && !sawPI;  i++ )
		if ( ep->cont[i].type == '?' )
			sawPI = TRUE;

	if ( sawPI || (TblCountContent(ep) > BOFTTextThresh) )	{
		tblBOFTCount++;
		OutputString("T{^", fP, 1);
		tblinBOFT = TRUE;	/* within a boft now */
	}
}

/*	TblCountContent()  --  count all content below the given element
 *
 *
 */



int
TblCountContent(Element_t * ep)		/* the element to look under */
{
	register int i, count;


	count = 0;

	for ( i=0;  i < ep->ncont;  i++ )	{
		if ( ep->cont[i].type == '-' )	{
			count += strlen(ep->cont[i].ch.data);
		} else
		if ( ep->cont[i].type == '(' )	{
			count += TblCountContent(ep->cont[i].ch.elem);
		}
	}

	return count;
}

/*      TblTEntryEnd()  --  end an entry
 *
 *  TO DO:
 *
 *	if within BOFT, output "T}"
 *	if not last entry, output tab character
 *
 */



void
TblTCellEnd(Element_t * ep,
	    FILE * fP)
{
	register Element_t * ep2;


	if ( tblinBOFT )	{
		OutputString("^T}", fP, 1);
		tblinBOFT = FALSE;	/* back out again */
	}

	for ( ep2=ep->next;  ep2;  ep2=ep2->next )	{
		if ( !strcmp(ep2->gi, "ENTRY") || !strcmp(ep2->gi, "ENTRYTBL") )	{
			OutputString("\007", fP, 1);
			break;
		}
		if ( !strcmp(ep2->gi, "ROW") )
			break;
	}
}

/*	TblDoColSpec()  --  process one element to create a new colspec
 *
 *
 */



struct tblcolspec *
TblDoColSpec(short number,		/* this column number */
	     Element_t * ep,		/* element containing colspec stuff */
	     struct tblcolspec * pcsp,	/* prevailing colspec (with defaults) */
	     tblsource source)		/* precedence level of the resulting spec */
{
	register char * cp;
	register struct tblcolspec * tcsp;


	Calloc(1, tcsp, struct tblcolspec);

	if ( cp = FindAttValByName(ep, "COLNAME") )
		strcpy(tcsp->name, cp);

	tcsp->num = number;
	tcsp->source = source;

	if ( cp = FindAttValByName(ep, "ALIGN") )	{
		if      ( !strcmp(cp, "LEFT") )		tcsp->align = Left;
		else if ( !strcmp(cp, "RIGHT") )	tcsp->align = Right;
		else if ( !strcmp(cp, "CENTER") )	tcsp->align = Center;
		else if ( !strcmp(cp, "JUSTIFY") )	tcsp->align = Justify;
		else if ( !strcmp(cp, "CHAR") )		tcsp->align = Char;
	} else
		tcsp->align = ( pcsp ) ? pcsp->align : Left;

	if ( cp = FindAttValByName(ep, "CHAR") )
		tcsp->alignchar = cp[0];
	else
		tcsp->alignchar = ( pcsp ) ? pcsp->alignchar : 0;

	if ( cp = FindAttValByName(ep, "CHAROFF") )
		tcsp->aligncharoff = atoi(cp);
	else
		tcsp->aligncharoff = ( pcsp ) ? pcsp->aligncharoff : 0;

	if ( cp = FindAttValByName(ep, "COLWIDTH") )
		strcpy(tcsp->colwidth, cp);
	else
		strcpy(tcsp->colwidth, ( pcsp ) ? pcsp->colwidth : "");

	if ( cp = FindAttValByName(ep, "COLSEP") )
		tcsp->colsep = !strcmp(cp, "1");
	else
		tcsp->colsep = ( pcsp ) ? pcsp->colsep : FALSE;

	if ( cp = FindAttValByName(ep, "ROWSEP") )
		tcsp->rowsep = !strcmp(cp, "1");
	else
		tcsp->rowsep = ( pcsp ) ? pcsp->rowsep : FALSE;

	return tcsp;
}

/*	TblDoSpanSpec()  --  process one element to create a new spanspec
 *
 *	Note that there's a hack inside here...  NameSt and NameEnd are
 *	supposed to point at colnames, but if no colname is found, this
 *	code will look for a colnum by the same value.
 */



struct tblspanspec *
TblDoSpanSpec(Element_t * ep,		/* element containing spanspec stuff */
	      struct tblspanspec * pssp, /* prevailing spanspec (with defaults) */
	      tblsource source)		/* precedence level of the resulting spec */
{
	register char * cp;
	register struct tblspanspec * tssp;
	register struct tblcolspec * tcsp;


	Calloc(1, tssp, struct tblspanspec);

	if ( cp = FindAttValByName(ep, "SPANNAME") ) strcpy(tssp->name, cp);
	tssp->source = source;

	if ( cp = FindAttValByName(ep, "NAMEST") )	{
		if ( (tcsp = TblFindColSpec(cp, source)) ||
		     (tcsp = TblFindColNum(atoi(cp), source)) )	{
		     	tssp->start = tcsp;
		} else	{
			fprintf(stderr, "? spanspec namest points to unknown column '%s'\n", cp);
			tssp->start = 0;
		}
	} else	{
		if ( pssp && pssp->start )	{
			tssp->start = pssp->start;
		}
	}

	if ( cp = FindAttValByName(ep, "NAMEEND") )	{
		if ( (tcsp = TblFindColSpec(cp, source)) ||
		     (tcsp = TblFindColNum(atoi(cp), source)) )	{
		     	tssp->end = tcsp;
		} else	{
			fprintf(stderr, "? spanspec nameend points to unknown column '%s'\n", cp);
			tssp->end = 0;
		}
	} else	{
		if ( pssp && pssp->end )	{
			tssp->end = pssp->end;
		}
	}

	if ( cp = FindAttValByName(ep, "ALIGN") )	{
		if      ( !strcmp(cp, "LEFT") )		tssp->align = Left;
		else if ( !strcmp(cp, "RIGHT") )	tssp->align = Right;
		else if ( !strcmp(cp, "CENTER") )	tssp->align = Center;
		else if ( !strcmp(cp, "JUSTIFY") )	tssp->align = Justify;
		else if ( !strcmp(cp, "CHAR") )		tssp->align = Char;
	} else	{
		if ( pssp )
			tssp->align = pssp->align;
	}

	if ( cp = FindAttValByName(ep, "CHAR") )
		tssp->alignchar = cp[0];
	else	{
		if ( pssp )
			tssp->alignchar = pssp->alignchar;
	}
	if ( cp = FindAttValByName(ep, "CHAROFF") )
		tssp->aligncharoff = atoi(cp);
	else	{
		if ( pssp )
			tssp->alignchar = pssp->alignchar;
	}

	if ( cp = FindAttValByName(ep, "COLSEP") )
		tssp->colsep = !strcmp(cp, "1");
	else	{
		if ( pssp )
			tssp->colsep = pssp->colsep;
	}
	if ( cp = FindAttValByName(ep, "ROWSEP") )
		tssp->rowsep = !strcmp(cp, "1");
	else	{
		if ( pssp )
			tssp->rowsep = pssp->rowsep;
	}

	return tssp;
}

/*	TblFindColSpec()  --  find a table colspec by name (colname)
 *
 */



struct tblcolspec *
TblFindColSpec(char * name,		/* the name we're looking for */
	       tblsource source)	/* the context in which to find it */
{
	register struct tblcolspec * tcsp;


    /* first, try to find the one in the right "source" */

    	for ( tcsp=tblColSpec;  tcsp;  tcsp=tcsp->next )	{
    		if ( (tcsp->source == source) && !strcmp(tcsp->name, name) )
    			return tcsp;
    	}

    /* else, try to find one from a TGroup.. */

    	for ( tcsp=tblColSpec;  tcsp;  tcsp=tcsp->next )	{
    		if ( (tcsp->source == TGroup) && !strcmp(tcsp->name, name) )
    			return tcsp;
    	}

    /* else not found.. */

	return 0;
}       

/*	TblFindColNum()  --  find a table colspec by number
 *
 */



struct tblcolspec *
TblFindColNum(short number,		/* the number we're looking for */
	      tblsource source)		/* the context in which to find it */
{
	register struct tblcolspec * tcsp;



    /* first, try to find the one in the right "source" */

    	for ( tcsp=tblColSpec;  tcsp;  tcsp=tcsp->next )	{
    		if ( (tcsp->num == number) &&
    		     ((tcsp->source == source) ||
    		      ((source == THead) && (tcsp->source == TGroup))) )
    			return tcsp;
    	}

    /* else, try to find one from a TGroup.. */

    	for ( tcsp=tblColSpec;  tcsp;  tcsp=tcsp->next )	{
    		if ( (tcsp->source == TGroup) && (tcsp->num == number) )
    			return tcsp;
    	}

    /* else not found.. */

	return 0;
}

/*	TblFindSpanSpec()  --  find a table spanspec by name (spanname)
 *
 */



struct tblspanspec *
TblFindSpanSpec(char * name,		/* the name we're looking for */
	        tblsource source)	/* the context in which to find it */
{
	register struct tblspanspec * tssp;


    /* first, try to find the one in the right "source" */

    	for ( tssp=tblSpanSpec;  tssp;  tssp=tssp->next )	{
    		if ( !strcmp(tssp->name, name) &&
    		     ((tssp->source == source) ||
    		      ((source == THead) && (tssp->source == TGroup))) )
    			return tssp;
    	}

    /* else not found.. */

	return 0;
}
