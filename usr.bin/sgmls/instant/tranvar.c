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
 *  instant - a program to manipulate SGML instances.
 *
 *  This module is for handling "special variables".  These act a lot like
 *  procedure calls
 * ________________________________________________________________________
 */

#ifndef lint
static char *RCSid =
  "$Header: /usr/src/docbook-to-man/Instant/RCS/tranvar.c,v 1.5 1996/06/11 22:43:15 fld Exp $";
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

static char	**idrefs;		/* list of IDREF att names to follow */
static char	*def_idrefs[] = { "LINKEND", "LINKENDS", "IDREF", 0 };
static char	*each_A = 0;	/* last seen _eachatt */
static char	*each_C = 0;	/* last seen _eachcon */

/* forward references */
void	ChaseIDRefs(Element_t *, char *, char *, FILE *);
void	Find(Element_t *, int, char **, FILE *);
void	GetIDREFnames();

/* ______________________________________________________________________ */
/*  Handle "special" variable - read file, run command, do action, etc.
 *  Arguments:
 *	Name of special variable to expand.
 *	Pointer to element under consideration.
 *	FILE pointer to where to write output.
 *	Flag saying whether to track the character position we're on
 *	  (passed to OutputString).
 */

void
ExpandSpecialVar(
    char	*name,
    Element_t	*e,
    FILE	*fp,
    int		track_pos
)
{
    FILE	*infile;
    char	buf[LINESIZE], *cp, *atval;
    char	**tok;
    int		ntok, n, i, actioni;
    char	*action, *action1;
    Element_t	*ep;
    Trans_t	*t, *tt;

    /* Run a command.
     * Format: _! command args ... */
    if (*name == '!') {
	name++;
	if ((infile = popen(name, "r"))) {
	    while (fgets(buf, LINESIZE, infile)) fputs(buf, fp);
	    pclose(infile);
	    fflush(fp);
	}
	else {
	    fprintf(stderr, "Could not start program '%s': %s",
		name, strerror(errno));
	}
	return;
    }

    /* See if caller wants one of the tokens from _eachatt or _eachcon.
     * If so, output it and return.  (Yes, I admit that this is a hack.)
     */
    if (*name == 'A' && name[1] == EOS && each_A) {
	OutputString(each_A, fp, track_pos);
	return;
    }
    if (*name == 'C' && name[1] == EOS && each_C) {
	OutputString(each_C, fp, track_pos);
	return;
    }

    ntok = 0;
    tok = Split(name, &ntok, 0);

    /* Include another file.
     * Format: _include filename */
    if (StrEq(tok[0], "include")) {
	name = tok[1];
	if (ntok > 1 ) {
	    if ((infile=OpenFile(name)) == NULL) {
		sprintf(buf, "Can not open included file '%s'", name);
		perror(buf);
		return;
	    }
	    while (fgets(buf, LINESIZE, infile)) fputs(buf, fp);
	    fclose(infile);
	}
	else fprintf(stderr, "No file name specified for include\n");
	return;
    }

    /* Print location (nearest title, line no, path).
     * Format: _location */
    else if (StrEq(tok[0], "location")) {
	PrintLocation(e, fp);
    }

    /* Print path to this element.
     * Format: _path */
    else if (StrEq(tok[0], "path")) {
	(void)FindElementPath(e, buf);
	OutputString(buf, fp, track_pos);
    }

    /* Print name of this element (gi).
     * Format: _gi [M|L|U] */
    else if (StrEq(tok[0], "gi")) {
	strcpy(buf, e->gi);
	if (ntok >= 2) {
	    if (*tok[1] == 'L' || *tok[1] == 'l' ||
		*tok[1] == 'M' || *tok[1] == 'm') {
		for (cp=buf; *cp; cp++)
		    if (isupper(*cp)) *cp = tolower(*cp);
	    }
	    if (*tok[1] == 'M' || *tok[1] == 'm')
		if (islower(buf[0])) buf[0] = toupper(buf[0]);
	}
	OutputString(buf, fp, track_pos);
    }

    /* Print filename of this element's associated external entity.
     * Format: _filename */
    else if (StrEq(tok[0], "filename")) {
	if (!e->entity) {
	    fprintf(stderr, "Expected ext entity (internal error? bug?):\n");
	    PrintLocation(e, stderr);
	    return;
	}
	if (!e->entity->fname) {
	    fprintf(stderr, "Expected filename (internal error? bug?):\n");
	    PrintLocation(e, stderr);
	    return;
	}
	OutputString(e->entity->fname, fp, track_pos);
    }

    /* Value of parent's attribute, by attr name.
     * Format: _pattr attname */
    else if (StrEq(tok[0], "pattr")) {
	ep = e->parent;
	if (!ep) {
	    fprintf(stderr, "Element does not have a parent:\n");
	    PrintLocation(ep, stderr);
	    return;
	}
	if ((atval = FindAttValByName(ep, tok[1]))) {
	    OutputString(atval, fp, track_pos);
	}
    }

    /* Use an action, given transpec's SID.
     * Format: _action action */
    else if (StrEq(tok[0], "action")) {
	TranTByAction(e, tok[1], fp);
    }

    /* Number of child elements of this element.
     * Format: _nchild */
    else if (StrEq(tok[0], "nchild")) {
	if (ntok > 1) {
	    for (n=0,i=0; i<e->necont; i++)
		if (StrEq(e->econt[i]->gi, tok[1])) n++;
	}
	else n = e->necont;
	sprintf(buf, "%d", n);
	OutputString(buf, fp, track_pos);
    }

    /* number of 1st child's child elements (grandchildren from first child).
     * Format: _n1gchild */
    else if (StrEq(tok[0], "n1gchild")) {
	if (e->necont) {
	    sprintf(buf, "%d", e->econt[0]->necont);
	    OutputString(buf, fp, track_pos);
	}
    }

    /* Chase this element's pointers until we hit the named GI.
     * Do the action if it matches.
     * Format: _chasetogi gi action */
    else if (StrEq(tok[0], "chasetogi")) {
	if (ntok < 3) {
	    fprintf(stderr, "Error: Not enough args for _chasetogi.\n");
	    return;
	}
	actioni = atoi(tok[2]);
	if (actioni) ChaseIDRefs(e, tok[1], tok[2], fp);
    }

    /* Follow link to element pointed to, then do action.
     * Format: _followlink [attname] action. */
    else if (StrEq(tok[0], "followlink")) {
	char **s;
	if (ntok > 2) {
	    if ((atval = FindAttValByName(e, tok[1]))) {
		if ((ep = FindElemByID(atval))) {
		    TranTByAction(ep, tok[2], fp);
		    return;
		}
	    }
	    else fprintf(stderr, "Error: Did not find attr: %s.\n", tok[1]);
	    return;
	}
	GetIDREFnames();
	for (s=idrefs; *s; s++) {
	    /* is this IDREF attr set? */
	    if ((atval = FindAttValByName(e, *s))) {
		ntok = 0;
		tok = Split(atval, &ntok, S_STRDUP);
		/* we'll follow the first one... */
		if ((ep = FindElemByID(tok[0]))) {
		    TranTByAction(ep, tok[1], fp);
		    return;
		}
		else fprintf(stderr, "Error: Can not find elem for ID: %s.\n",
			tok[0]);
	    }
	}
	fprintf(stderr, "Error: Element does not have IDREF attribute set:\n");
	PrintLocation(e, stderr);
	return;
    }

    /* Starting at this element, decend tree (in-order), finding GI.
     * Do the action if it matches.
     * Format: _find args ... */
    else if (StrEq(tok[0], "find")) {
	Find(e, ntok, tok, fp);
    }

    /* Starting at this element's parent, decend tree (in-order), finding GI.
     * Do the action if it matches.
     * Format: _pfind args ... */
    else if (StrEq(tok[0], "pfind")) {
	Find(e->parent ? e->parent : e, ntok, tok, fp);
    }

    /* Content is supposed to be a list of IDREFs.  Follow each, doing action.
     * If 2 actions are specified, use 1st for the 1st ID, 2nd for the rest.
     * Format: _namelist action [action2] */
    else if (StrEq(tok[0], "namelist")) {
	int id;
	action1 = tok[1];
	if (ntok > 2) action = tok[2];
	else action = action1;
	for (i=0; i<e->ndcont; i++) {
	    n = 0;
	    tok = Split(e->dcont[i], &n, S_STRDUP);
	    for (id=0; id<n; id++) {
		if (fold_case)
		    for (cp=tok[id]; *cp; cp++)
			if (islower(*cp)) *cp = toupper(*cp);
		if ((e = FindElemByID(tok[id]))) {
		    if (id) TranTByAction(e, action, fp);
		    else TranTByAction(e, action1, fp);	/* first one */
		}
		else fprintf(stderr, "Error: Can not find ID: %s.\n", tok[id]);
	    }
	}
    }

    /* For each word in the element's content, do action.
     * Format: _eachcon action [action] */
    else if (StrEq(tok[0], "eachcon")) {
	int id;
	action1 = tok[1];
	if (ntok > 3) action = tok[2];
	else action = action1;
	for (i=0; i<e->ndcont; i++) {
	    n = 0;
	    tok = Split(e->dcont[i], &n, S_STRDUP|S_ALVEC);
	    for (id=0; id<n; id++) {
		each_C = tok[id];
		TranTByAction(e, action, fp);
	    }
	    free(*tok);
	}
    }
    /* For each word in the given attribute's value, do action.
     * Format: _eachatt attname action [action] */
    else if (StrEq(tok[0], "eachatt")) {
	int id;
	action1 = tok[2];
	if (ntok > 3) action = tok[3];
	else action = action1;
	if ((atval = FindAttValByName(e, tok[1]))) {
	    n = 0;
	    tok = Split(atval, &n, S_STRDUP|S_ALVEC);
	    for (id=0; id<n; id++) {
		each_A = tok[id];
		if (id) TranTByAction(e, action, fp);
		else TranTByAction(e, action1, fp);	/* first one */
	    }
	    free(*tok);
	}
    }

    /* Do action on this element if element has [relationship] with gi.
     * Format: _relation relationship gi action [action] */
    else if (StrEq(tok[0], "relation")) {
	if (ntok >= 4) {
	    if (!CheckRelation(e, tok[1], tok[2], tok[3], fp, RA_Current)) {
		/* action not done, see if alt action specified */
		if (ntok >= 5)
		    TranTByAction(e, tok[4], fp);
	    }
	}
    }

    /* Do action on followed element if element has [relationship] with gi.
     * If [relationship] is not met, do alternate action on this element.
     * Format: _followrel relationship gi action [action] */
    else if (StrEq(tok[0], "followrel")) {
	if (ntok >= 4) {
	    if (!CheckRelation(e, tok[1], tok[2], tok[3], fp, RA_Related)) {
	    	/* action not done, see if an alt action specified */
	    	if (ntok >= 5)
	    	    TranTByAction(e, tok[4], fp);
	    }
	}
    }

    /* Find element with matching ID and do action.  If action not specified,
     * choose the right one appropriate for its context.
     * Format: _id id [action] */
    else if (StrEq(tok[0], "id")) {
	if ((ep = FindElemByID(tok[1]))) {
	    if (ntok > 2) TranTByAction(ep, tok[2], fp);
	    else {
		t = FindTrans(ep, 0);
		TransElement(ep, fp, t);
	    }
	}
    }

    /* Set variable to value.
     * Format: _set name value */
    else if (StrEq(tok[0], "set")) {
	SetMappingNV(Variables, tok[1], tok[2]);
    }

    /* Do action if variable is set, optionally to value.
     * If not set, do nothing.
     * Format: _isset varname [value] action */
    else if (StrEq(tok[0], "isset")) {
	if ((cp = FindMappingVal(Variables, tok[1]))) {
	    if (ntok == 3) TranTByAction(e, tok[2], fp);
	    else if (ntok > 3 && !strcmp(cp, tok[2]))
		TranTByAction(e, tok[3], fp);
	}
    }

    /* Insert a node into the tree at start/end, pointing to action to perform.
     * Format: _insertnode S|E action */
    else if (StrEq(tok[0], "insertnode")) {
	actioni = atoi(tok[2]);
	if (*tok[1] == 'S') e->gen_trans[0] = actioni;
	else if (*tok[1] == 'E') e->gen_trans[1] = actioni;
    }

    /* Do an CALS DTD table spec for TeX or troff.  Looks through attributes
     * and determines what to output. "check" means to check consistency,
     * and print error messages.
     * This is (hopefully) the only hard-coded part of instant.
     *
     * This was originally written for the OSF DTDs and recoded by FLD for
     * CALS tables (since no one will ever use the OSF tables).  Although
     * TeX was addressed first, it seems that a fresh approach was required,
     * and so, tbl is the first to be really *fixed*.  Once tbl is stable,
     * and there is a need for TeX again, that part will be recoded.
     *
     * *Obsolete* form (viz, for TeX):
     *    Format: _calstable [clear|check|tex]
     *			  [cellstart|cellend|rowstart|rowend|top|bottom]
     *
     * New, good form:
     *
     *    Format: _calstable [tbl]
     *			  [tablestart|tableend|tablegroup|tablefoot|rowstart|
     *			   rowend|entrystart|entryend]
     */

    else if (StrEq(tok[0], "calstable")) {
	CALStable(e, fp, tok, ntok);
    }

    /* Do action if element's attr is set, optionally to value.
     * If not set, do nothing.
     * Format: _attval att [value] action */
    else if (StrEq(tok[0], "attval")) {
	if ((atval = FindAttValByName(e, tok[1]))) {
	    if (ntok == 3) TranTByAction(e, tok[2], fp);
	    else if (ntok > 3 && !strcmp(atval, tok[2]))
		TranTByAction(e, tok[3], fp);
	}
    }
    /* Same thing, but look at parent */
    else if (StrEq(tok[0], "pattval")) {
	if ((atval = FindAttValByName(e->parent, tok[1]))) {
	    if (ntok == 3) {
		TranTByAction(e, tok[2], fp);
	    }
	    if (ntok > 3 && !strcmp(atval, tok[2]))
		TranTByAction(e, tok[3], fp);
	}
    }

    /* Print each attribute and value for the current element, hopefully
     * in a legal sgml form: <elem-name att1="value1" att2="value2:> .
     * Format: _allatts */
    else if (StrEq(tok[0], "allatts")) {
	for (i=0; i<e->natts; i++) {
	    if (i != 0) putc(' ', fp);
	    fputs(e->atts[i].name, fp);
	    fputs("=\"", fp);
	    fputs(e->atts[i].sval, fp);
	    putc('"', fp);
	}
    }

    /* Print the element's input filename, and optionally, the line number.
     * Format: _infile [line] */
    else if (StrEq(tok[0], "infile")) {
	if (e->infile) {
	    if (ntok > 1 && !strcmp(tok[1], "root")) {
		strcpy(buf, e->infile);
		if ((cp = strrchr(buf, '.'))) *cp = EOS;
		fputs(buf, fp);
	    }
	    else {
		fputs(e->infile, fp);
		if (ntok > 1 && !strcmp(tok[1], "line"))
		    fprintf(fp, " %d", e->lineno);
	    }
	    return;
	}
	else fputs("input-file??", fp);
    }

    /* Get value of an environement variable */
    else if (StrEq(tok[0], "env")) {
	if (ntok > 1 && (cp = getenv(tok[1]))) {
	    OutputString(cp, fp, track_pos);
	}
    }

    /* If the element is not empty do specid.
     * Format: _notempty spec-id */
    else if (StrEq(tok[0], "notempty")) {
	if (ntok > 1 && e->ncont) {
	    TranTByAction(e, tok[1], fp);
	}
    }

    /* Something unknown */
    else {
	fprintf(stderr, "Unknown special variable: %s\n", tok[0]);
	tt = e->trans;
	if (tt && tt->lineno)
	    fprintf(stderr, "Used in transpec, line %d\n", tt->lineno);
    }
    return;
}

/* ______________________________________________________________________ */
/*  return the value for the special variables _A (last processed _eachatt)
 *  and _C (last processed _eachcon)
 */

char *
Get_A_C_value(const char * name)
{
    if ( !strcmp(name, "each_A") )	{
	if ( each_A )	{
	    return each_A;
	} else	{
	    fprintf(stderr, "Requested value for unset _A variable\n");
	}
    } else
    if ( !strcmp(name, "each_C") )	{
	if ( each_C )	{
	    return each_C;
	} else	{
	    fprintf(stderr, "Requested value for unset _C variable\n");
	}
    } else	{
	fprintf(stderr, "Requested value for unknown special variable '%s'\n",
				name);
    }
    return "";
}

/* ______________________________________________________________________ */
/*  Chase IDs until we find an element whose GI matches.  We also check
 *  child element names, not just the names of elements directly pointed
 *  at (by IDREF attributes).
 */

void
GetIDREFnames()
{
    char	*cp;

    if (!idrefs) {
	/* did user or transpec set the variable */
	if ((cp = FindMappingVal(Variables, "link_atts")))
	    idrefs = Split(cp, 0, S_STRDUP|S_ALVEC);
	else
	    idrefs = def_idrefs;
    }
}

/* ______________________________________________________________________ */
/*  Chase ID references - follow IDREF(s) attributes until we find
 *  a GI named 'gi', then perform given action on that GI.
 *  Arguments:
 *	Pointer to element under consideration.
 *	Name of GI we're looking for.
 *	Spec ID of action to take.
 *	FILE pointer to where to write output.
 */
void
ChaseIDRefs(
    Element_t	*e,
    char	*gi,
    char *	action,
    FILE	*fp
)
{
    int		ntok, i, ei;
    char	**tok, **s, *atval;

    /* First, see if we got what we came for with this element */
    if (StrEq(e->gi, gi)) {
	TranTByAction(e, action, fp);
	return;
    }
    GetIDREFnames();

    /* loop for each attribute of type IDREF(s) */
    for (s=idrefs; *s; s++) {
	/* is this IDREF attr set? */
	if ((atval = FindAttValByName(e, *s))) {
	    ntok = 0;
	    tok = Split(atval, &ntok, 0);
	    for (i=0; i<ntok; i++) {
		/* get element pointed to */
		if ((e = FindElemByID(tok[i]))) {
		    /* OK, we found a matching GI name */
		    if (StrEq(e->gi, gi)) {
			/* process using named action */
			TranTByAction(e, action, fp);
			return;
		    }
		    else {
			/* this elem itself did not match, try its children */
			for (ei=0; ei<e->necont; ei++) {
			    if (StrEq(e->econt[ei]->gi, gi)) {
				TranTByAction(e->econt[ei], action, fp);
				return;
			    }
			}
			/* try this elem's IDREF attributes */
			ChaseIDRefs(e, gi, action, fp);
			return;
		    }
		}
		else {
		    /* should not happen, since parser checks ID/IDREFs */
		    fprintf(stderr, "Error: Could not find ID %s\n", atval);
		    return;
		}
	    }
	}
    }
    /* if the pointers didn't lead to the GI, give error */
    if (!s)
	fprintf(stderr, "Error: Could not find '%s'\n", gi);
}

/* ______________________________________________________________________ */

/* state to pass to recursive routines - so we don't have to use
 * global variables. */
typedef struct {
    char	*gi;
    char	*gi2;
    char	action[10];
    Element_t	*elem;
    FILE	*fp;
} Descent_t;

static void
tr_find_gi(
    Element_t	*e,
    Descent_t	*ds
)
{
    if (StrEq(ds->gi, e->gi))
	if (ds->action[0]) TranTByAction(e, ds->action, ds->fp);
}

static void
tr_find_gipar(
    Element_t	*e,
    Descent_t	*ds
)
{
    if (StrEq(ds->gi, e->gi) && e->parent &&
		StrEq(ds->gi2, e->parent->gi))
	if (ds->action[0]) TranTByAction(e, ds->action, ds->fp);
}

static void
tr_find_attr(
    Element_t	*e,
    Descent_t	*ds
)
{
    char	*atval;
    if ((atval = FindAttValByName(e, ds->gi)) && StrEq(ds->gi2, atval))
	TranTByAction(e, ds->action, ds->fp);
}

static void
tr_find_parent(
    Element_t	*e,
    Descent_t	*ds
)
{
    if (QRelation(e, ds->gi, REL_Parent)) {
	if (ds->action[0]) TranTByAction(e, ds->action, ds->fp);
    }
}

/* ______________________________________________________________________ */
/*  Descend tree, finding elements that match criteria, then perform
 *  given action.
 *  Arguments:
 *	Pointer to element under consideration.
 *	Number of tokens in special variable.
 *	Vector of tokens in special variable (eg, "find" "gi" "TITLE")
 *	FILE pointer to where to write output.
 */
void
Find(
    Element_t	*e,
    int		ac,
    char	**av,
    FILE	*fp
)
{
    Descent_t	DS;		/* state passed to recursive routine */

    memset(&DS, 0, sizeof(Descent_t));
    DS.elem = e;
    DS.fp   = fp;

    /* see if we should start at the top of instance tree */
    if (StrEq(av[1], "top")) {
	av++;
	ac--;
	e = DocTree;
    }
    if (ac < 4) {
	fprintf(stderr, "Bad '_find' specification - missing args.\n");
	return;
    }
    /* Find elem whose GI is av[2] */
    if (StrEq(av[1], "gi")) {
	DS.gi     = av[2];
	strcpy(DS.action, av[3]);
	DescendTree(e, tr_find_gi, 0, 0, &DS);
    }
    /* Find elem whose GI is av[2] and whose parent GI is av[3] */
    else if (StrEq(av[1], "gi-parent")) {
	DS.gi     = av[2];
	DS.gi2    = av[3];
	strcpy(DS.action, av[4]);
	DescendTree(e, tr_find_gipar, 0, 0, &DS);
    }
    /* Find elem whose parent GI is av[2] */
    else if (StrEq(av[0], "parent")) {
	DS.gi     = av[2];
	strcpy(DS.action, av[3]);
	DescendTree(e, tr_find_parent, 0, 0, &DS);
    }
    /* Find elem whose attribute av[2] has value av[3] */
    else if (StrEq(av[0], "attr")) {
	DS.gi     = av[2];
	DS.gi2    = av[3];
	strcpy(DS.action, av[4]);
	DescendTree(e, tr_find_attr, 0, 0, &DS);
    }
}

/* ______________________________________________________________________ */

