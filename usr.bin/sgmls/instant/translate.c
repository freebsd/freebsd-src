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
 *  This module is for "translating" an instance to another form, usually
 *  suitable for a formatting application.
 *
 *  Entry points for this module:
 *	DoTranslate(elem, fp)
 *      ExpandVariables(in, out, e)
 * ________________________________________________________________________
 */

#ifndef lint
static char *RCSid =
  "$Header: /usr/src/docbook-to-man/Instant/RCS/translate.c,v 1.11 1996/06/15 22:49:00 fld Exp $";
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
#define STORAGE
#include "translate.h"

static Trans_t	NullTrans;		/* an empty one */

/* forward references */
void	ProcesOutputSpec(char *, Element_t *, FILE *, int);
static void	WasProcessed(Element_t *);

/* ______________________________________________________________________ */
/*  Translate the subtree starting at 'e'. Output goes to 'fp'.
 *  This is the entry point for translating an instance.
 *  Arguments:
 *	Pointer to element under consideration.
 *	FILE pointer to where to write output.
 */

void
DoTranslate(
    Element_t	*e,
    FILE	*fp
)
{
    Trans_t	*t, *tn;

    /* Find transpec for each node. */
    DescendTree(e, PrepTranspecs, 0, 0, 0);

    /* Stuff to do at start of processing */
    if ((t = FindTransByName("_Start"))) {
	if (t->starttext) ProcesOutputSpec(t->starttext, 0, fp, 1);
	if (t->replace)   ProcesOutputSpec(t->replace, 0, fp, 1);
	if (t->message)   ProcesOutputSpec(t->message, 0, stderr, 0);
	if (t->endtext)   ProcesOutputSpec(t->endtext, 0, fp, 1);
    }

    /* Translate topmost/first element.  This is recursive. */
    TransElement(e, fp, NULL);

    /* Stuff to do at end of processing */
    if ((t = FindTransByName("_End"))) {
	if (t->starttext) ProcesOutputSpec(t->starttext, 0, fp, 1);
	if (t->replace)   ProcesOutputSpec(t->replace, 0, fp, 1);
	if (t->message)   ProcesOutputSpec(t->message, 0, stderr, 0);
	if (t->endtext)   ProcesOutputSpec(t->endtext, 0, fp, 1);
    }

    /* Warn about unprocessed elements in this doc tree, if verbose mode. */
    if (verbose)
	DescendTree(e, WasProcessed, 0, 0, 0);

    /* Clean up. This is not yet complete, which is no big deal (since the
     * program is normally done at this point anyway.  */
    for (t=TrSpecs; t; ) {
	tn = t->next;
	/* free the contents of t here ... */
	(void)free((void* )t);
	t = tn;
    }
    TrSpecs = 0;
}

/* ______________________________________________________________________ */
/*  Print warning about unprocessed elements in this doc tree (if they
 *  were not explicitely ignored).
 *  Arguments:
 *	Pointer to element under consideration.
 */
static void
WasProcessed(
    Element_t	*e
)
{
    Trans_t	*t;
    t = e->trans;
    if (!e->processed && (t && !t->ignore)) {
	fprintf(stderr, "Warning: element '%s' was not processed:\n", e->gi);
	PrintLocation(e, stderr);
    }
}

/* ______________________________________________________________________ */
/*  For each element find transpec.
 *  Arguments:
 *	Pointer to element under consideration.
 */
void
PrepTranspecs(
    Element_t	*e
)
{
    Trans_t	*t;
    t = FindTrans(e, 0);
    e->trans = t;
}

/* ______________________________________________________________________ */
/*  Copy a buffer/string into another, expanding regular variables and immediate
 *  variables. (Special variables are done later.)
 *  Arguments:
 *	Pointer to string to expand.
 *	Pointer to expanded string. (return)
 *	Pointer to element under consideration.
 */
void
ExpandVariables(
    char	*in,
    char	*out,
    Element_t	*e
)
{
    register int i, j;
    char	*ip, *vp, *op;
    char	*def_val, *s, *atval, *modifier;
    char	vbuf[500];
    int		lev;

    ip = in;
    op = out;
    while (*ip) {
	/* start of regular variable? */
	if (*ip == '$' && *(ip+1) == L_CURLY && *(ip+2) != '_') {
	    ip++;
	    ip++;		/* point at variable name */
	    vp = vbuf;
	    /*	Look for matching (closing) curly. (watch for nesting)
	     *	We store the variable content in a tmp buffer, so we don't
	     *	clobber the input buffer.
	     */
	    lev = 0;
	    while (*ip) {
		if (*ip == L_CURLY) lev++;
		if (*ip == R_CURLY) {
		    if (lev == 0) {
			ip++;
			break;
		    }
		    else lev--;
		}
		*vp++ = *ip++;	/* copy to variable buffer */
	    }
	    *vp = EOS;
	    /* vbuf now contains the variable name (stuff between curlys). */
	    if (lev != 0) {
		fprintf(stderr, "Botched variable use: %s\n", in);
		/* copy rest of string if we can't recover  ?? */
		return;
	    }
	    /* Now, expand variable. */
	    vp = vbuf;

	    /* Check for immediate variables -- like _special variables but
	     * interpreted right now.  These start with a "+" */

	    if ( *vp == '+' )	{

	    	if ( ! strcmp(vp, "+content") )	{
	    	    for ( i=0;  i<e->ncont; i++ )	{
	    	    	if ( IsContData(e, i) )	{
	    	    	    j = strlen(ContData(e,i));
	    	    	    memcpy(op, ContData(e,i), j);
	    	    	    op += j;
	    	    	} else	{
	    	    	    fprintf(stderr, "warning: ${+current} skipped element content\n");
	    	    	}
	    	    }

	    	} else	{
	    	    fprintf(stderr, "unknown immediate variable: %s\n", vp);
	    	}

	    } else	{

		/* See if this variable has a default [ format: ${varname def} ] */

		def_val = vp;
		while (*def_val && *def_val != ' ') def_val++;
		if (*def_val) *def_val++ = EOS;
		else def_val = 0;
		/* def_val now points to default, if it exists, null if not. */

		modifier = vp;
		while (*modifier && *modifier != ':') modifier++;
		if (*modifier) *modifier++ = EOS;
		else modifier = 0;
		/* modifier now points to modifier if it exists, null if not. */

		s = 0;
		/* if attribute of current elem with this name found, use value */
		if (e && (atval = FindAttValByName(e, vp)))
		    s = atval;
	   	 else	/* else try for (global) variable with this name */
		    s = FindMappingVal(Variables, vp);

		/* If we found a value, copy it to the output buffer. */

		if (s) {
		    if ( modifier && *modifier == 'l' ) {
			while (*s) {
			    *op = tolower(*s);
			    op++, *s++;
			}
		    } else
			while (*s) *op++ = *s++;
		} else
		if (def_val)	{
		    while (*def_val) *op++ = *def_val++;
		}
	    }
	    continue;
	}
	*op++ = *ip++;
    }
    *op = EOS;		/* terminate string */
}

/* ______________________________________________________________________ */
/*  Process an "output" translation spec - one of StartText, EndText,
 *  Replace, Message.  (These are the ones that produce output.)
 *  Steps done:
 *	Expand attributes and regular varaibles in input string.
 *	Pass thru string, accumulating chars to be sent to output stream.
 *	If we find the start of a special variable, output what we've
 *	  accumulated, then find the special variable's "bounds" (ie, the
 *	  stuff between the curly brackets), and expand that by passing to
 *	  ExpandSpecialVar().  Continue until done the input string.
 *  Arguments:
 *	Input buffer (string) to be expanded and output.
 *	Pointer to element under consideration.
 *	FILE pointer to where to write output.
 *	Flag saying whether to track the character position we're on
 *	  (passed to OutputString).
 */
void
ProcesOutputSpec(
    char	*ib,
    Element_t	*e,
    FILE	*fp,
    int		track_pos
)
{
    char	obuf[LINESIZE];
    char	vbuf[LINESIZE];
    char	*dest, vname[LINESIZE], *cp;
    int		esc;

    obuf[0] = EOS;			/* start with empty output buffer */

    ExpandVariables(ib, vbuf, e);	/* expand regular variables */
    ib = vbuf;
    dest = obuf;

    esc = 0;
    while (*ib) {
	/* If not a $, it's a regular char.  Just copy it and go to next. */
	if (*ib != '$') {		/* look for att/variable marker */
	    *dest++ = *ib++;		/* it's not. just copy character */
	    continue;
	}

	/* We have a $.  What we have must be a "special variable" since
	 * regular variables have already been expanded, or just a lone $. */

	if (ib[1] != L_CURLY) {	/* just a stray dollar sign (no variable) */
	    *dest++ = *ib++;
	    continue;
	}

	ib++;				/* point past $ */

	/* Output what we have in buffer so far. */
	*dest = EOS;			/* terminate string */
	if (obuf[0]) OutputString(obuf, fp, track_pos);
	dest = obuf;			/* ready for new stuff in buffer */

	if (!strchr(ib, R_CURLY)) {
	    fprintf(stderr, "Mismatched braces in TranSpec: %s\n", ib);
	    /* how do we recover from this? */
	}
	ib++;
	cp = vname;
	while (*ib && *ib != R_CURLY) *cp++ = *ib++;
	*cp = EOS;			/* terminate att/var name */
	ib++;				/* point past closing curly */
	/* we now have special variable name (stuff in curly {}'s) in vname */
	ExpandSpecialVar(&vname[1], e, fp, track_pos);
    }
    *dest = EOS;			/* terminate string in output buffer */

    if (obuf[0]) OutputString(obuf, fp, track_pos);
}

/* ______________________________________________________________________ */
/*  Find the translation spec for the given tag.
 *  Returns pointer to first spec that matches (name, depth, etc., of tag).
 *  Arguments:
 *	e -- Pointer to element under consideration.
 *	specID -- name of specid that we're looking for
 *  Return:
 *	Pointer to translation spec that matches given element's context.
 */

Trans_t *
FindTrans(
    Element_t	*e,
    int		specID
)
{
    char	context[LINESIZE], buf[LINESIZE], *cp, **vec, *atval;
    int		i, a, match;
    Trans_t	*t, *tt;

    /* loop through all transpecs */
    for (t=TrSpecs; t; t=t->next)
    {
	/* Only one of gi or gilist will be set. */
	/* Check if elem name matches */
	if (t->gi && !StrEq(t->gi, e->gi) && !specID) continue;

	/* test if we're looking for a specific specID and then if
	 * this is it.. */
	if (specID)
	    if (!t->my_id || (specID != t->my_id))
		continue;

	/* Match one in the list of GIs? */
	if (t->gilist) {
	    for (match=0,vec=t->gilist; *vec; vec++) {
		if (StrEq(*vec, e->gi)) {
		    match = 1;
		    break;
		}
	    }
	    if (!match) continue;
	}

	/* Check context */

	/* Special case of context */
	if (t->parent)
	    if (!QRelation(e, t->parent, REL_Parent)) continue;

	if (t->context) {	/* no context specified -> a match */
	    FindContext(e, t->depth, context);

	    /* If reg expr set, do regex compare; else just string compare. */
	    if (t->context_re) {
		if (! regexec(t->context_re, context)) continue;
	    }
	    else {
		/* Is depth of spec deeper than element's depth? */
		if (t->depth > e->depth) continue;

		/* See if context of element matches "context" of transpec */
		match = ( (t->context[0] == context[0]) &&
			    !strcmp(t->context, context) );
		if (!match) continue;
	    }
	}

	/* Check attributes.  Loop through list, comparing each. */
	if (t->nattpairs) {	/* no att specified -> a match */
	    for (match=1,a=0; a<t->nattpairs; a++) {
		if (!(atval = FindAttValByName(e, t->attpair[a].name))) {
		    match = 0;
		    break;
		}
		if (!regexec(t->attpair[a].rex, atval)) match = 0;
	    }
	    if (!match) continue;
	}

	/* Check relationships:  child, parent, ancestor, sib, ...  */
	if (t->relations) {
	    Mapping_t *r;
	    match = 1;
	    for (r=t->relations->maps,i=0; i<t->relations->n_used; i++) {
		if (!CheckRelation(e, r[i].name, r[i].sval, 0, 0, RA_Current)) {
		    match = 0;
		    break;
		}
	    }
	    if (!match) continue;
	}

	/* check this element's parent's attribute */
	if (t->pattrset && e->parent) {
	    char *p, **tok;

	    i = 2;
	    match = 1;
	    tok = Split(t->pattrset, &i, S_STRDUP);
	    if ( i == 2 ) {
		p = FindAttValByName(e->parent, tok[0]);
		ExpandVariables(tok[1], buf, 0);
		if ( !p || strcmp(p, buf) )
		    match = 0;
	    } else {
		if (!FindAttValByName(e->parent, t->pattrset))
		    match = 0;
	    }
	    free(tok[0]);
	    if (!match) continue;
	}

	/* check this element's "birth order" */
	if (t->nth_child) {
	    /* First one is called "1" by the user.  Internally called "0". */
	    i = t->nth_child;
	    if (i > 0) {	/* positive # -- count from beginning */
		if (e->my_eorder != (i-1)) continue;
	    }
	    else {		/* negative # -- count from end */
		i = e->parent->necont - i;
		if (e->my_eorder != i) continue;
	    }
	}

	/* check that variables match */
	if (t->var_name) {
	    cp = FindMappingVal(Variables, t->var_name);
	    if (!cp || strcmp(cp, t->var_value)) continue;
	}

	/* check for variable regular expression match */
	if ( t->var_RE_name )	{
	    cp = FindMappingVal(Variables, t->var_RE_name);
	    if (!cp || !regexec(t->var_RE_value, cp)) continue;
	}

	/* check content */
	if (t->content) {		/* no att specified -> a match */
	    for (match=0,i=0; i<e->ndcont; i++) {
		if (regexec(t->content_re, e->dcont[i])) {
		    match = 1;
		    break;
		}
	    }
	    if (!match) continue;
	}

	/* -------- at this point we've passed all criteria -------- */

	/* See if we should be using another transpec's actions. */
	if (t->use_id) {
	    if (t->use_id < 0) return &NullTrans;	/* missing? */
	    /* see if we have a pointer to that transpec */
	    if (t->use_trans) return t->use_trans;
	    for (tt=TrSpecs; tt; tt=tt->next) {
		if (t->use_id == tt->my_id) {
		    /* remember pointer for next time */
		    t->use_trans = tt;
		    return t->use_trans;
		}
	    }
	    t->use_id = -1;	/* flag it as missing */
	    fprintf(stderr, "Warning: transpec ID (%d) not found for %s.\n",
		t->use_id, e->gi);
	    return &NullTrans;
	}

	return t;
    }

    /* At this point, we have not found a matching spec.  See if there
     * is a wildcard, and if so, use it. (Wildcard GI is named "*".) */
    if ((t = FindTransByName("*"))) return t;

    if (warnings && !specID)
	fprintf(stderr, "Warning: transpec not found for %s\n", e->gi);

    /* default spec - pass character data and descend node */
    return &NullTrans;
}

/* ______________________________________________________________________ */
/*  Find translation spec by (GI) name.  Returns the first one that matches.
 *  Arguments:
 *	Pointer to name of transpec (the "gi" field of the Trans structure).
 *  Return:
 *	Pointer to translation spec that matches name.
 */

Trans_t *
FindTransByName(
    char *s
)
{
    Trans_t *t;

    for (t=TrSpecs; t; t=t->next) {
	/* check if tag name matches (first check 1st char, for efficiency) */
	if (t->gi) {
	    if (*(t->gi) != *s) continue;	/* check 1st character */
	    if (!strcmp(t->gi, s)) return t;
	}
    }
    return NULL;
}

/*  Find translation spec by its ID (SpecID).
 *  Arguments:
 *	Spec ID (an int).
 *  Return:
 *	Pointer to translation spec that matches name.
 */
Trans_t *
FindTranByID(int n)
{
    Trans_t	*t;

    for (t=TrSpecs; t; t=t->next)
	if (n == t->my_id) return t;
    return NULL;
}

/* ______________________________________________________________________ */
/*  Process a "chunk" of content data of an element.
 *  Arguments:
 *	Pointer to data content to process
 *	FILE pointer to where to write output.
 */

void
DoData(
    char *data,
    FILE *fp,
    int  verbatim
)
{
    if (!fp) return;
    OutputString(data, fp, 1);
}

/* ______________________________________________________________________ */
/*  Handle a processing instruction.  This is done similarly to elements,
 *  where we find a transpec, then do what it says.  Differences: PI names
 *  start with '_' in the spec file (if a GI does not start with '_', it
 *  may be forced to upper case, sgmls keeps PIs as mixed case); the args
 *  to the PI are treated as the data of an element.  Note that a PI wildcard
 *  is "_*"
 *  Arguments:
 *	Pointer to the PI.
 *	FILE pointer to where to write output.
 */

void
DoPI(
    char *pi,
    FILE *fp
)
{
    char	buf[250], **tok;
    int		n;
    Trans_t	*t;

    buf[0] = '_';
    strcpy(&buf[1], pi);
    n = 2;
    tok = Split(buf, &n, 0);
    if ((t = FindTransByName(tok[0])) ||
        (t = FindTransByName("_*"))) {
	if (t->replace)   ProcesOutputSpec(t->replace, 0, fp, 1);
	else {
	    if (t->starttext) ProcesOutputSpec(t->starttext, 0, fp, 1);
	    if (t->ignore != IGN_DATA)	/* skip data nodes? */
		if (n > 1) OutputString(tok[1], fp, 1);
	    if (t->endtext)   ProcesOutputSpec(t->endtext, 0, fp, 1);
	}
	if (t->message)   ProcesOutputSpec(t->message, 0, stderr, 0);
    }
    else {
	/* If not found, just print the PI in square brackets, along
	 * with a warning message. */
	fprintf(fp, "[%s]", pi);
	if (warnings) fprintf(stderr, "Warning: Unrecognized PI: [%s]\n", pi);
    }
}

/* ______________________________________________________________________ */
/*  Set and increment variables, as appropriate, if the transpec says to.
 *  Arguments:
 *	Pointer to translation spec for current element.
 */

static void
set_and_increment(
    Trans_t	*t,
    Element_t	*e
)
{
    Mapping_t	*m;
    int		i, inc, n;
    char	*cp, buf[50];
    char	ebuf[500];

    /* set/reset variables */
    if (t->set_var) {
	for (m=t->set_var->maps,i=0; i<t->set_var->n_used; i++)	{
	    ExpandVariables(m[i].sval, ebuf, e);	/* do some expansion */
	    SetMappingNV(Variables, m[i].name, ebuf);
    	}
    }

    /* increment counters */
    if (t->incr_var) {
	for (m=t->incr_var->maps,i=0; i<t->incr_var->n_used; i++) {
	    cp = FindMappingVal(Variables, m[i].name);
	    /* if not set at all, set to 1 */
	    if (!cp) SetMappingNV(Variables, m[i].name, "1");
	    else {
		if (isdigit(*cp) || (*cp == '-' && isdigit(cp[1]))) {
		    n = atoi(cp);
		    if (m[i].sval && isdigit(*m[i].sval)) inc = atoi(m[i].sval);
		    else inc = 1;
		    sprintf(buf, "%d", (n + inc));
		    SetMappingNV(Variables, m[i].name, buf);
		} else
		if (!*(cp+1) && isalpha(*cp))	{
		    buf[0] = *cp + 1;
		    buf[1] = 0;
		    SetMappingNV(Variables, m[i].name, buf);
		}
	    }
	}
    }
}

/* ______________________________________________________________________ */
/*  Translate one element.
 *  Arguments:
 *	Pointer to element under consideration.
 *	FILE pointer to where to write output.
 *	Pointer to translation spec for current element, or null.
 */
void
TransElement(
    Element_t	*e,
    FILE	*fp,
    Trans_t	*t
)
{
    int		i;

    if (!t) t = ((e && e->trans) ? e->trans : &NullTrans);

    /* see if we should quit. */
    if (t->quit) {
	fprintf(stderr, "Quitting at location:\n");
	PrintLocation(e, fp);
	fprintf(stderr, "%s\n", t->quit);
	exit(1);
    }

    /* See if we want to replace subtree (do text, don't descend subtree) */
    if (t->replace) {
	ProcesOutputSpec(t->replace, e, fp, 1);
	if (t->message) ProcesOutputSpec(t->message, e, stderr, 0);
	set_and_increment(t, e);	/* adjust variables, if appropriate */
	return;
    }

    if (t->starttext) ProcesOutputSpec(t->starttext, e, fp, 1);
    if (t->message)   ProcesOutputSpec(t->message, e, stderr, 0);

    /* Process data for this node and descend child elements/nodes. */
    if (t->ignore != IGN_ALL) {
	/* Is there a "generated" node at the front of this one? */
	if (e->gen_trans[0]) {
	    Trans_t *tp;
	    if ((tp = FindTranByID(e->gen_trans[0]))) {
		if (tp->starttext) ProcesOutputSpec(tp->starttext, e, fp, 1);
		if (tp->message)   ProcesOutputSpec(tp->message, e, stderr, 0);
		if (tp->endtext)   ProcesOutputSpec(tp->endtext, e, fp, 1);
	    }
	}
	/* Loop thruthe "nodes", whether data, child element, or PI. */
	for (i=0; i<e->ncont; i++) {
	    if (IsContElem(e,i)) {
		if (t->ignore != IGN_CHILDREN)	/* skip child nodes? */
		    TransElement(ContElem(e,i), fp, NULL);
	    }
	    else if (IsContData(e,i)) {
		if (t->ignore != IGN_DATA)	/* skip data nodes? */
		    DoData(ContData(e,i), fp, t->verbatim);
	    }
	    else if (IsContPI(e,i))
		DoPI(e->cont[i].ch.data, fp);
	}
	/* Is there a "generated" node at the end of this one? */
	if (e->gen_trans[1]) {
	    Trans_t *tp;
	    if ((tp = FindTranByID(e->gen_trans[1]))) {
		if (tp->starttext) ProcesOutputSpec(tp->starttext, e, fp, 1);
		if (tp->message)   ProcesOutputSpec(tp->message, e, stderr, 0);
		if (tp->endtext)   ProcesOutputSpec(tp->endtext, e, fp, 1);
	    }
	}
    }

    set_and_increment(t, e);		/* adjust variables, if appropriate */

    if (t->endtext) ProcesOutputSpec(t->endtext, e, fp, 1);

    e->processed = 1;
}

/* ______________________________________________________________________ */
/* Check if element matches specified relationship, and, if it does, perform
 * action on either current element or matching element (depends on flag).
 *  Arguments:
 *	Pointer to element under consideration.
 *	Pointer to relationship name.
 *	Pointer to related element name (GI).
 *	Pointer to action to take (string - turned into an int).
 *	FILE pointer to where to write output.
 *	Flag saying whether to do action on related element (RA_Related)
 *		or on current element (RA_Current).
 *  Return:
 *	Bool, saying whether (1) or not (0) relationship matches.
 */

int
CheckRelation(
    Element_t	*e,
    char	*relname,	/* relationship name */
    char	*related,	/* related element */
    char	*actname,	/* action to take */
    FILE	*fp,
    RelAction_t	flag
)
{
    Element_t	*ep;
    Relation_t	r;

    if ((r = FindRelByName(relname)) == REL_Unknown) return 0;
    if (!(ep=QRelation(e, related, r)))	return 0;

    if (!actname) return 1;		/* no action - return what we found */

    switch (flag) {
	case RA_Related:	TranTByAction(ep, actname, fp);	break;
	case RA_Current:	TranTByAction(e, actname, fp);	break;
    }
    return 1;
}

/* ______________________________________________________________________ */
/* Perform action given by a SpecID on the given element.
 *  Arguments:
 *	Pointer to element under consideration.
 *	SpecID of action to perform.
 *	FILE pointer to where to write output.
 *
 */
void
TranByAction(
    Element_t	*e,
    int		n,
    FILE	*fp
)
{
    Trans_t	*t;

    t = FindTranByID(n);
    if (!t) {
	fprintf(stderr, "Could not find named action for %d.\n", n);
	return;
    }
    TransElement(e, fp, t);
}

/* ______________________________________________________________________ */
/* Perhaps perform action given by a SpecID on the given element.
 *  Arguments:
 *	Pointer to element under consideration.
 *	SpecID of action to perform.  Unlike TranByAction, this is the argument
 *	  as it occurred in the transpec (ASCII) and may end with the letter
 *	  "t" which means that the transpec mustpass criteria selection.
 *	FILE pointer to where to write output.
 */
void
TranTByAction(
    Element_t	*e,
    char	*strn,
    FILE	*fp
)
{
    int n;
    Trans_t	*t;

    n = atoi(strn);
    if ( strn[strlen(strn)-1] != 't' )	{
    	t = FindTranByID(n);
    	if (!t) {
	    fprintf(stderr, "Could not find named action for %d.\n", n);
	    return;
    	}
    } else	{
	t = FindTrans(e, n);
	if ( !t || !t->my_id )
	    return;
    }
    TransElement(e, fp, t);
}

/* ______________________________________________________________________ */
