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
 *  General utility functions for 'instant' program.  These are used
 *  throughout the rest of the program.
 *
 *  Entry points for this module:
 *	Split(s, &n, flags)		split string into n tokens
 *	NewMap(slot_incr)		create a new mapping structure
 *	FindMapping(map, name)		find mapping by name; return mapping
 *	FindMappingVal(map, name)	find mapping by name; return value
 *	SetMapping(map, s)		set mapping based on string
 *	OpenFile(filename)		open file, looking in inst path
 *	FilePath(filename)		find path to a file
 *	FindElementPath(elem, s)	find path to element
 *	PrintLocation(ele, fp)		print location of element in tree
 *	NearestOlderElem(elem, name)	find prev elem up tree with name
 *	OutputString(s, fp, track_pos)	output string
 *	AddElemName(name)		add elem to list of known elements
 *	AddAttName(name)		add att name to list of known atts
 *	FindAttByName(elem, name)	find an elem's att by name
 *	FindContext(elem, lev, context)	find context of elem
 *	QRelation(elem, name, rel_flag)	find relation elem has to named elem
 *	DescendTree(elem, enter_f, leave_f, data_f, dp)	descend doc tree,
 *					calling functions for each elem/node
 * ________________________________________________________________________
 */

#ifndef lint
static char *RCSid =
  "$Header: /usr/src/docbook-to-man/Instant/RCS/util.c,v 1.4 1996/06/02 21:47:32 fld Exp $";
#endif

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <memory.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <errno.h>
#include <regexp.h>
/* CSS don't have it and I don't see where it's used
#include <values.h>
*/

#include "general.h"
#include "translate.h"

/* ______________________________________________________________________ */
/*  "Split" a string into tokens.  Given a string that has space-separated
 *  (space/tab) tokens, return a pointer to an array of pointers to the
 *  tokens.  Like what the shell does with *argv[].  The array can be is
 *  static or allocated.  Space can be allocated for string, or allocated.
 *  Arguments:
 *	Pointer to string to pick apart.
 *	Pointer to max number of tokens to find; actual number found is
 *	  returned. If 0 or null pointer, use a 'sane' maximum number (hard-
 *	  code). If more tokens than the number specified, make last token be
 *	  a single string composed of the rest of the tokens (includes spaces).
 *	Flag. Bit 0 says whether to make a copy of input string (since we'll
 *	  clobber parts of it).  To free the string, use the pointer to
 *	  the first token returned by the function (or *ret_value).
 *	  Bit 1 says whether to allocate the vector itself.  If not, use
 *	  (and return) a static vector.
 *  Return:
 *	Pointer to the provided string (for convenience of caller).
 */

char **
Split(
    char	*s,		/* input string */
    int		*ntok,		/* # of tokens desired (input)/found (return) */
    int		flag		/* dup string? allocate a vector? */
)
{
    int		maxnt, i=0;
    int		n_alloc;
    char	**tokens;
    static char	*local_tokens[100];

    /* Figure max number of tokens (maxnt) to find.  0 means find them all. */
    if (ntok == NULL)
	maxnt = 100;
    else {
	if (*ntok <= 0 || *ntok > 100) maxnt = 100;	/* arbitrary size */
	else maxnt = *ntok;
	*ntok = 0;
    }

    if (!s) return 0;			/* no string */

    /* Point to 1st token (there may be initial space) */
    while (*s && IsWhite(*s)) s++;	/* skip initial space, if any */
    if (*s == EOS) return 0;		/* none found? */

    /* See if caller wants us to copy the input string. */
    if (flag & S_STRDUP) s = strdup(s);

    /* See if caller wants us to allocate the returned vector. */
    if (flag & S_ALVEC) {
	n_alloc = 20;
	Malloc(n_alloc, tokens, char *);
	/* if caller did not specify max tokens to find, set to more than
	 * there will possibly ever be */
	if (!ntok || !(*ntok)) maxnt = 10000;
    }
    else tokens = local_tokens;

    i = 0;			/* index into vector */
    tokens[0] = s;		/* s already points to 1st token */
    while (i<maxnt) {
	tokens[i] = s;		/* point vector member at start of token */
	i++;
	/* If we allocated vector, see if we need more space. */
	if ((flag & S_ALVEC) && i >= n_alloc) {
	    n_alloc += 20;
	    Realloc(n_alloc, tokens, char *);
	}
	if (i >= maxnt) break;			/* is this the last one? */
	while (*s && !IsWhite(*s)) s++;		/* skip past end of token */
	if (*s == EOS) break;			/* at end of input string? */
	if (*s) *s++ = EOS;			/* terminate token string */
	while (*s && IsWhite(*s)) s++;		/* skip space - to next token */
    }
    if (ntok) *ntok = i;		/* return number of tokens found */
    tokens[i] = 0;			/* null-terminate vector */
    return tokens;
}

/* ______________________________________________________________________ */
/*  Mapping routines.  These are used for name-value pairs, like attributes,
 *  variables, and counters.  A "Map" is an opaque data structure used
 *  internally by these routines.  The caller gets one when creating a new
 *  map, then hands it to other routines that need it.  A "Mapping" is a
 *  name/value pair.  The user has access to this.
 *  Here's some sample usage:
 *
 *	Map *V;
 *	V = NewMap(20);
 *	SetMappingNV(V, "home", "/users/bowe");
 *	printf("Home: %s\n", FindMappingVal(V, "home");
 */

/*  Allocate new map structure.  Only done once for each map/variable list.
 *  Arg:
 *	Number of initial slots to allocate space for.  This is also the
 *	"chunk size" - how much to allocate when we use up the given space.
 *  Return:
 *	Pointer to the (opaque) map structure. (User passes this to other
 *	mapping routines.)
 */
Map_t *
NewMap(
    int		slot_increment
)
{
    Map_t	*M;
    Calloc(1, M, Map_t);
    /* should really do the memset's in Calloc/Malloc/Realloc
       macros, but that will have to wait until time permits -CSS */
    memset((char *)M, 0, sizeof(Map_t));
    if (!slot_increment) slot_increment = 1;
    M->slot_incr = slot_increment;
    return M;
}

/*  Given pointer to a Map and a name, find the mapping.
 *  Arguments:
 *	Pointer to map structure (as returned by NewMap().
 *	Variable name.
 *  Return:
 *	Pointer to the matching mapping structure, or null if not found.
 */
Mapping_t *
FindMapping(
    Map_t	*M,
    const char	*name
)
{
    int		i;
    Mapping_t	*m;

    if (!M || M->n_used == 0) return NULL;
    for (m=M->maps,i=0; i<M->n_used; i++)
	if (m[i].name[0] == name[0] && !strcmp(m[i].name, name)) return &m[i];
    return NULL;

}

/*  Given pointer to a Map and a name, return string value of the mapping.
 *  Arguments:
 *	Pointer to map structure (as returned by NewMap().
 *	Variable name.
 *  Return:
 *	Pointer to the value (string), or null if not found.
 */
char *
FindMappingVal(
    Map_t	*M,
    const char	*name
)
{
    Mapping_t	*m;

    if ( !strcmp(name, "each_A") || !strcmp(name, "each_C") )	{
	return Get_A_C_value(name);
    }

    /*
    if (!M || M->n_used == 0) return NULL;
    if ((m = FindMapping(M, name))) return m->sval;
    return NULL;
    */
    if (!M || M->n_used == 0) {
	return NULL;
    }
    if ((m = FindMapping(M, name))) {
	return m->sval;
    }
    return NULL;

}

/*  Set a mapping/variable in Map M.  Input string is a name-value pair where
 *  there is some amount of space after the name.  The correct mapping is done.
 *  Arguments:
 *	Pointer to map structure (as returned by NewMap().
 *	Pointer to variable name (string).
 *	Pointer to variable value (string).
 */
void
SetMappingNV(
    Map_t	*M,
    const char	*name,
    const char	*value
)
{
    FILE	*pp;
    char	buf[LINESIZE], *cp;
    int		i;
    Mapping_t	*m;

    /* First, look to see if it's a "well-known" variable. */
    if (!strcmp(name, "verbose"))  { verbose   = atoi(value); return; }
    if (!strcmp(name, "warnings")) { warnings  = atoi(value); return; }
    if (!strcmp(name, "foldcase")) { fold_case = atoi(value); return; }

    m = FindMapping(M, name);		/* find existing mapping (if set) */

    /* OK, we have a string mapping */
    if (m) {				/* exists - just replace value */
	free(m->sval);
	if (value) m->sval = strdup(value);
	else m->sval = NULL;
    }
    else {
	if (name) {		/* just in case */
	    /* Need more slots for mapping structures?  Allocate in clumps. */
	    if (M->n_used == 0) {
		M->n_alloc = M->slot_incr;
		Malloc(M->n_alloc, M->maps, Mapping_t);
	    }
	    else if (M->n_used >= M->n_alloc) {
		M->n_alloc += M->slot_incr;
		Realloc(M->n_alloc, M->maps, Mapping_t);
	    }

	    m = &M->maps[M->n_used];
	    M->n_used++;
	    m->name = strdup(name);
	    if (value) m->sval = strdup(value);
	    else m->sval = NULL;
	}
    }

    if (value)
    {
	/* See if the value is a command to run.  If so, run the command
	 * and replace the value with the output.
	 */
	if (*value == '!') {
	    if ((pp = popen(value+1, "r"))) {	/* run cmd, read its output */
		i = 0;
		cp = buf;
		while (fgets(cp, LINESIZE-i, pp)) {
		    i += strlen(cp);
		    cp = &buf[i];
		    if (i >= LINESIZE) {
			fprintf(stderr,
			    "Prog execution of variable '%s' too long.\n",
			    m->name);
			break;
		    }
		}
		free(m->sval);
		stripNL(buf);
		m->sval = strdup(buf);
		pclose(pp);
	    }
	    else {
		sprintf(buf, "Could not start program '%s'", value+1);
		perror(buf);
	    }
	}
    }
}

/*  Separate name and value from input string, then pass to SetMappingNV.
 *  Arguments:
 *	Pointer to map structure (as returned by NewMap().
 *	Pointer to variable name and value (string), in form "name value".
 */
void
SetMapping(
    Map_t	*M,
    const char	*s
)
{
    char	buf[LINESIZE];
    char	*name, *val;

    if (!M) {
	fprintf(stderr, "SetMapping: Map not initialized.\n");
	return;
    }
    strcpy(buf, s);
    name = val = buf;
    while (*val && !IsWhite(*val)) val++;	/* point past end of name */
    if (*val) {
	*val++ = EOS;				/* terminate name */
	while (*val && IsWhite(*val)) val++;	/* point to value */
    }
    if (name) SetMappingNV(M, name, val);
}

/* ______________________________________________________________________ */
/*  Opens a file for reading.  If not found in current directory, try
 *  lib directories (from TPT_LIB env variable, or -l option).
 *  Arguments:
 *	Filename (string).
 *  Return:
 *	FILE pointer to open file, or null if it not found or can't open.
 */

FILE *
OpenFile(
    char	*filename
)
{
    FILE	*fp;
    
    filename = FilePath(filename);
    if ((fp=fopen(filename, "r"))) return fp;
    return NULL;
}

/* ______________________________________________________________________ */
/*  Opens a file for reading.  If not found in current directory, try
 *  lib directories (from TPT_LIB env variable, or -l option).
 *  Arguments:
 *	Filename (string).
 *  Return:
 *	FILE pointer to open file, or null if it not found or can't open.
 */

char *
FilePath(
    char	*filename
)
{
    FILE	*fp;
    static char	buf[LINESIZE];
    int		i;
    static char	**libdirs;
    static int	nlibdirs = -1;

    if ((fp=fopen(filename, "r")))
    {
    	fclose(fp);
    	strncpy(buf, filename, LINESIZE);
    	return buf;
    }

    if (*filename == '/') return NULL;		/* full path specified? */

    if (nlibdirs < 0) {
	char *cp, *s;
	if (tpt_lib) {
	    s = strdup(tpt_lib);
	    for (cp=s; *cp; cp++) if (*cp == ':') *cp = ' ';
	    nlibdirs = 0;
	    libdirs = Split(s, &nlibdirs, S_ALVEC);
	}
	else nlibdirs = 0;
    }
    for (i=0; i<nlibdirs; i++) {
	sprintf(buf, "%s/%s", libdirs[i], filename);
	if ((fp=fopen(buf, "r")))
	{
	    fclose(fp);
	    return buf;
	}
    }
    return NULL;
}

/* ______________________________________________________________________ */
/*  This will find the path to an tag.  The format is the:
 *	tag1(n1):tag2(n2):tag3
 *  where the tags are going down the tree and the numbers indicate which
 *  child (the first is numbered 1) the next tag is.
 *  Returns pointer to the string just written to (so you can use this
 *  function as a printf arg).
 *  Arguments:
 *	Pointer to element under consideration.
 *	String to write path into (provided by caller).
 *  Return:
 *	Pointer to the provided string (for convenience of caller).
 */
char *
FindElementPath(
    Element_t	*e,
    char	*s
)
{
    Element_t	*ep;
    int		i, e_path[MAX_DEPTH];
    char	*cp;

    /* Move up the tree, noting "birth order" of each element encountered */
    for (ep=e; ep; ep=ep->parent)
	e_path[ep->depth-1] = ep->my_eorder;
    /* Move down the tree, printing the element names to the string. */
    for (cp=s,i=0,ep=DocTree; i<e->depth; ep=ep->econt[e_path[i]],i++) {
	sprintf(cp, "%s(%d) ", ep->gi, e_path[i]);
	cp += strlen(cp);
    }
    sprintf(cp, "%s", e->gi);
    return s;
}

/* ______________________________________________________________________ */
/*  Print some location info about a tag.  Helps user locate error.
 *  Messages are indented 2 spaces (convention for multi-line messages).
 *  Arguments:
 *	Pointer to element under consideration.
 *	FILE pointer of where to print.
 */

void
PrintLocation(
    Element_t	*e,
    FILE	*fp
)
{
    char	*s, buf[LINESIZE];

    if (!e || !fp) return;
    fprintf(fp, "  Path: %s\n", FindElementPath(e, buf));
    if ((s=NearestOlderElem(e, "TITLE")))
	fprintf(fp, "  Position hint: TITLE='%s'\n", s);
    if (e->lineno) {
	if (e->infile)
	    fprintf(fp, "  At or near instance file: %s, line: %d\n",
			e->infile, e->lineno);
	else
	    fprintf(fp, "  At or near instance line: %d\n", e->lineno);
    }
    if (e->id)
	fprintf(fp, "  ID: %s\n", e->id);
}

/* ______________________________________________________________________ */
/*  Finds the data part of the nearest "older" tag (up the tree, and
 *  preceding) whose tag name matches the argument, or "TITLE", if null.
 *  Returns a pointer to the first chunk of character data.
 *  Arguments:
 *	Pointer to element under consideration.
 *	Name (GI) of element we'll return data from.
 *  Return:
 *	Pointer to that element's data content.
 */
char *
NearestOlderElem(
    Element_t	*e,
    char	*name
)
{
    int		i;
    Element_t	*ep;

    if (!e) return 0;
    if (!name) name = "TITLE";			/* useful default */

    for (; e->parent; e=e->parent)		/* move up tree */
	for (i=0; i<=e->my_eorder; i++) {	/* check preceding sibs */
	    ep = e->parent;
	    if (!strcmp(name, ep->econt[i]->gi))
		return ep->econt[i]->ndcont ?
			ep->econt[i]->dcont[0] : "-empty-";
	}

    return NULL;
}

/* ______________________________________________________________________ */
/*  Expands escaped strings in the input buffer (things like tabs, newlines,
 *  octal characters - using C style escapes).
 */

char *ExpandString(
    char *s
)
{
    char	c, *sdata, *cp, *ns;
    int     	len, pos, addn;

    if (!s) return s;

    len = strlen(s);
    pos = 0;
    Malloc(len + 1, ns, char);
    ns[pos] = EOS;
    
    for ( ; *s; s++) {
    	c = *s;
    	cp = NULL;

    	/* Check for escaped characters from sgmls. */
	if (*s == '\\') {
	    s++;
	    switch (*s) {
		case 'n':
		    c = NL;
	    	    break;

	    	case '\\':
	    	    c = '\\';
	    	    break;

		case '0': case '1': case '2': case '3':
		case '4': case '5': case '6': case '7':
		    /* for octal numbers (C style) of the form \012 */
		    c = *s++ - '0';
    	    	    if (*s >= '0' && *s <= '7') {
    	    	    	c = c * 8 + (*s++ - '0');
    	    	    	if (*s >= '0' && *s <= '7')
    	    	    	    c = c * 8 + (*s - '0');
    	    	    }
		    break;

		case '|':		/* SDATA */
		    s++;		/* point past \| */
		    sdata = s;
		    /* find matching/closing \| */
		    cp = s;
		    while (*cp && *cp != '\\' && cp[1] != '|')
		    	cp++;
		    if (!*cp)
		    	break;

		    *cp = EOS;		/* terminate sdata string */
		    cp++;
		    s = cp;		/* s now points to | */

		    cp = LookupSDATA(sdata);
    	    	    if (!cp)
    	    	    	cp = sdata;
    	    	    c = 0;
		    break;

    	    	/* This shouldn't happen. */
    	    	default:
    	    	    s--;
    	    	    break;
	    }
	}

    	/* Check for character re-mappings. */
	if (nCharMap && c) {
	    int i;
	    
    	    for (i = 0; i < nCharMap; i++) {
		if (c != CharMap[i].name[0])
		    continue;
		cp = CharMap[i].sval;
		c = 0;
		break;
	    }
	}

    	/* See if there is enough space for the data. */
    	/* XXX this should be MUCH smarter about predicting
    	   how much extra memory it should allocate */
    	if (c)
    	    addn = 1;
    	else
    	    addn = strlen(cp);

    	/* If not, make some. */
    	if (addn > len - pos) {
    	    len += addn - (len - pos);
    	    Realloc(len + 1, ns, char);
    	}

    	/* Then copy the data. */
    	if (c)
    	    ns[pos] = c;
    	else
	    strcpy(&ns[pos], cp);

    	pos += addn;
    	ns[pos] = EOS;
    }
    return(ns);
}

/* ______________________________________________________________________ */
/*  Expands escaped strings in the input buffer (things like tabs, newlines,
 *  octal characters - using C style escapes) and outputs buffer to specified
 *  fp.  The hat/anchor character forces that position to appear at the
 *  beginning of a line.  The cursor position is kept track of (optionally)
 *  so that this can be done.
 *  Arguments:
 *	Pointer to element under consideration.
 *	FILE pointer of where to print.
 *	Flag saying whether or not to keep track of our position in the output
 *	  stream. (We want to when writing to a file, but not for stderr.)
 */

void
OutputString(
    char	*s,
    FILE	*fp,
    int		track_pos
)
{
    char	c;
    static int	char_pos = 0;		/* remembers our character position */
    char    	*p;

    if (!fp) return;
    if (!s) s = "^";		/* no string - go to start of line */

    for (p = s; *p; p++) {
    	c = *p;
	/* If caller wants us to track position, see if it's an anchor
	 * (ie, align at a newline). */
	if (track_pos) {
	    if (c == ANCHOR && (p == s || *(p + 1) == EOS)) {
		/* If we're already at the start of a line, don't do
		 * another newline. */
		if (char_pos != 0) c = NL;
		else c = 0;
	    }
	    else char_pos++;
	    if (c == NL) char_pos = 0;
	}
	else if (c == ANCHOR && (p == s || *(p + 1) == EOS)) c = NL;
	if (c) putc(c, fp);
    }
}

/* ______________________________________________________________________ */
/* Figure out value of SDATA entity.
 * We rememeber lookup hits in a "cache" (a shorter list), and look in
 * cache before general list.  Typically there will be LOTS of entries
 * in the general list and only a handful in the hit list.  Often, if an
 * entity is used once, it'll be used again.
 *  Arguments:
 *	Pointer to SDATA entity token in ESIS.
 *  Return:
 *	Mapped value of the SDATA entity.
 */

char *
LookupSDATA(
    char	*s
)
{
    char	*v;
    static Map_t *Hits;		/* remember lookup hits */

    /* If we have a hit list, check it. */
    if (Hits) {
	if ((v = FindMappingVal(Hits, s))) return v;
    }

    v = FindMappingVal(SDATAmap, s);

    /* If mapping found, remember it, then return it. */
    if ((v = FindMappingVal(SDATAmap, s))) {
	if (!Hits) Hits = NewMap(IMS_sdatacache);
	SetMappingNV(Hits, s, v);
	return v;
    }

    fprintf(stderr, "Error: Could not find SDATA substitution '%s'.\n", s);
    return NULL;
}

/* ______________________________________________________________________ */
/*  Add tag 'name' of length 'len' to list of tag names (if not there).
 *  This is a list of null-terminated strings so that we don't have to
 *  keep using the name length.
 *  Arguments:
 *	Pointer to element name (GI) to remember.
 *  Return:
 *	Pointer to the SAVED element name (GI).
 */

char *
AddElemName(
    char	*name
)
{
    int		i;
    static int	n_alloc=0;	/* number of slots allocated so far */

    /* See if it's already in the list. */
    for (i=0; i<nUsedElem; i++)
	if (UsedElem[i][0] == name[0] && !strcmp(UsedElem[i], name))
	    return UsedElem[i];

    /* Allocate slots in blocks of N, so we don't have to call malloc
     * so many times. */
    if (n_alloc == 0) {
	n_alloc = IMS_elemnames;
	Calloc(n_alloc, UsedElem, char *);
    }
    else if (nUsedElem >= n_alloc) {
	n_alloc += IMS_elemnames;
	Realloc(n_alloc, UsedElem, char *);
    }
    UsedElem[nUsedElem] = strdup(name);
    return UsedElem[nUsedElem++];
}
/* ______________________________________________________________________ */
/*  Add attrib name to list of attrib names (if not there).
 *  This is a list of null-terminated strings so that we don't have to
 *  keep using the name length.
 *  Arguments:
 *	Pointer to attr name to remember.
 *  Return:
 *	Pointer to the SAVED attr name.
 */

char *
AddAttName(
    char	*name
)
{
    int		i;
    static int	n_alloc=0;	/* number of slots allocated so far */

    /* See if it's already in the list. */
    for (i=0; i<nUsedAtt; i++)
	if (UsedAtt[i][0] == name[0] && !strcmp(UsedAtt[i], name))
	    return UsedAtt[i];

    /* Allocate slots in blocks of N, so we don't have to call malloc
     * so many times. */
    if (n_alloc == 0) {
	n_alloc = IMS_attnames;
	Calloc(n_alloc, UsedAtt, char *);
    }
    else if (nUsedAtt >= n_alloc) {
	n_alloc += IMS_attnames;
	Realloc(n_alloc, UsedAtt, char *);
    }
    UsedAtt[nUsedAtt] = strdup(name);
    return UsedAtt[nUsedAtt++];
}

/* ______________________________________________________________________ */
/*  Find an element's attribute value given element pointer and attr name.
 *  Typical use: 
 *	a=FindAttByName("TYPE", t); 
 *	do something with a->val;
 *  Arguments:
 *	Pointer to element under consideration.
 *	Pointer to attribute name.
 *  Return:
 *	Pointer to the value of the attribute.
 */

/*
Mapping_t *
FindAttByName(
    Element_t	*e,
    char	*name
)
{
    int		i;
    if (!e) return NULL;
    for (i=0; i<e->natts; i++)
	if (e->atts[i].name[0] == name[0] && !strcmp(e->atts[i].name, name))
		return &(e->atts[i]);
    return NULL;
}
*/

char *
FindAttValByName(
    Element_t	*e,
    char	*name
)
{
    int		i;
    if (!e) return NULL;
    for (i=0; i<e->natts; i++)
	if (e->atts[i].name[0] == name[0] && !strcmp(e->atts[i].name, name))
	    return e->atts[i].sval;
    return NULL;
}

/* ______________________________________________________________________ */
/*  Find context of a tag, 'levels' levels up the tree.
 *  Space for string is passed by caller.
 *  Arguments:
 *	Pointer to element under consideration.
 *	Number of levels to look up tree.
 *	String to write path into (provided by caller).
 *  Return:
 *	Pointer to the provided string (for convenience of caller).
 */

char *
FindContext(
    Element_t	*e,
    int		levels,
    char	*con
)
{
    char	*s;
    Element_t	*ep;
    int		i;

    if (!e) return NULL;
    s = con;
    *s = EOS;
    for (i=0,ep=e->parent; ep && levels; ep=ep->parent,i++,levels--) {
	if (i != 0) *s++ = ' ';
	strcpy(s, ep->gi);
	s += strlen(s);
    }
    return con;
}


/* ______________________________________________________________________ */
/*  Tests relationship (specified by argument/flag) between given element
 *  (structure pointer) and named element.
 *  Returns pointer to matching tag if found, null otherwise.
 *  Arguments:
 *	Pointer to element under consideration.
 *	Pointer to name of elem whose relationsip we are trying to determine.
 *	Relationship we are testing.
 *  Return:
 *	Pointer to the provided string (for convenience of caller).
 */

Element_t *
QRelation(
    Element_t	*e,
    char	*s,
    Relation_t	rel
)
{
    int		i;
    Element_t	*ep;

    if (!e) return 0;

    /* we'll call e the "given element" */
    switch (rel)
    {
	case REL_Parent:
	    if (!e->parent || !e->parent->gi) return 0;
	    if (!strcmp(e->parent->gi, s)) return e->parent;
	    break;
	case REL_Child:
	    for (i=0; i<e->necont; i++)
		if (!strcmp(s, e->econt[i]->gi)) return e->econt[i];
	    break;
	case REL_Ancestor:
	    if (!e->parent || !e->parent->gi) return 0;
	    for (ep=e->parent; ep; ep=ep->parent)
		if (!strcmp(ep->gi, s)) return ep;
	    break;
	case REL_Descendant:
	    if (e->necont == 0) return 0;
	    /* check immediate children first */
	    for (i=0; i<e->necont; i++)
		if (!strcmp(s, e->econt[i]->gi)) return e->econt[i];
	    /* then children's children (recursively) */
	    for (i=0; i<e->necont; i++)
		if ((ep=QRelation(e->econt[i], s, REL_Descendant)))
		    return ep;
	    break;
	case REL_Sibling:
	    if (!e->parent) return 0;
	    ep = e->parent;
	    for (i=0; i<ep->necont; i++)
		if (!strcmp(s, ep->econt[i]->gi) && i != e->my_eorder)
		    return ep->econt[i];
	    break;
	case REL_Preceding:
	    if (!e->parent || e->my_eorder == 0) return 0;
	    ep = e->parent;
	    for (i=0; i<e->my_eorder; i++)
		if (!strcmp(s, ep->econt[i]->gi)) return ep->econt[i];
	    break;
	case REL_ImmPreceding:
	    if (!e->parent || e->my_eorder == 0) return 0;
	    ep = e->parent->econt[e->my_eorder-1];
	    if (!strcmp(s, ep->gi)) return ep;
	    break;
	case REL_Following:
	    if (!e->parent || e->my_eorder == (e->parent->necont-1))
		return 0;	/* last? */
	    ep = e->parent;
	    for (i=(e->my_eorder+1); i<ep->necont; i++)
		if (!strcmp(s, ep->econt[i]->gi)) return ep->econt[i];
	    break;
	case REL_ImmFollowing:
	    if (!e->parent || e->my_eorder == (e->parent->necont-1))
		return 0;	/* last? */
	    ep = e->parent->econt[e->my_eorder+1];
	    if (!strcmp(s, ep->gi)) return ep;
	    break;
	case REL_Cousin:
	    if (!e->parent) return 0;
	    /* Now, see if element's parent has that thing as a child. */
	    return QRelation(e->parent, s, REL_Child);
	    break;
	case REL_None:
	case REL_Unknown:
	    fprintf(stderr, "You can not query 'REL_None' or 'REL_Unknown'.\n");
	    break;
    }
    return NULL;
}

/*  Given a relationship name (string), determine enum symbol for it.
 *  Arguments:
 *	Pointer to relationship name.
 *  Return:
 *	Relation_t enum.
 */
Relation_t
FindRelByName(
    char	*relname
)
{
    if (!strcmp(relname, "?")) {
	fprintf(stderr, "Supported query/relationships %s\n%s.\n", 
	    "child, parent, ancestor, descendant,",
	    "sibling, sibling+, sibling+1, sibling-, sibling-1");
	return REL_None;
    }
    else if (StrEq(relname, "child"))		return REL_Child;
    else if (StrEq(relname, "parent"))		return REL_Parent;
    else if (StrEq(relname, "ancestor"))	return REL_Ancestor;
    else if (StrEq(relname, "descendant"))	return REL_Descendant;
    else if (StrEq(relname, "sibling"))		return REL_Sibling;
    else if (StrEq(relname, "sibling-"))	return REL_Preceding;
    else if (StrEq(relname, "sibling-1"))	return REL_ImmPreceding;
    else if (StrEq(relname, "sibling+"))	return REL_Following;
    else if (StrEq(relname, "sibling+1"))	return REL_ImmFollowing;
    else if (StrEq(relname, "cousin"))		return REL_Cousin;
    else fprintf(stderr, "Unknown relationship: %s\n", relname);
    return REL_Unknown;
}

/* ______________________________________________________________________ */
/*  This will descend the element tree in-order. (enter_f)() is called
 *  upon entering the node.  Then all children (data and child elements)
 *  are operated on, calling either DescendTree() with a pointer to
 *  the child element or (data_f)() for each non-element child node.
 *  Before leaving the node (ascending), (leave_f)() is called.  enter_f
 *  and leave_f are passed a pointer to this node and data_f is passed
 *  a pointer to the data/content (which includes the data itself and
 *  type information).  dp is an opaque pointer to any data the caller
 *  wants to pass.
 *  Arguments:
 *	Pointer to element under consideration.
 *	Pointer to procedure to call when entering element.
 *	Pointer to procedure to call when leaving element.
 *	Pointer to procedure to call for each "chunk" of content data.
 *	Void data pointer, passed to the avobe 3 procedures.
 */

void
DescendTree(
    Element_t	*e,
    void	(*enter_f)(),
    void	(*leave_f)(),
    void	(*data_f)(),
    void	*dp
)
{
    int		i;
    if (enter_f) (enter_f)(e, dp);
    for (i=0; i<e->ncont; i++) {
	if (e->cont[i].type == CMD_OPEN)
	    DescendTree(e->cont[i].ch.elem, enter_f, leave_f, data_f, dp);
	else
	    if (data_f) (data_f)(&e->cont[i], dp);
    }
    if (leave_f) (leave_f)(e, dp);
}

/* ______________________________________________________________________ */
/*  Add element, 'e', whose ID is 'idval', to a list of IDs.
 *  This makes it easier to find an element by ID later.
 *  Arguments:
 *	Pointer to element under consideration.
 *	Element's ID attribute value (a string).
 */

void
AddID(
    Element_t	*e,
    char	*idval
)
{
    static ID_t	*id_last;

    if (!IDList) {
	Malloc(1, id_last, ID_t);
	IDList = id_last;
    }
    else {
	Malloc(1, id_last->next, ID_t);
	id_last = id_last->next;
    }
    id_last->elem = e;
    id_last->id   = idval;
}

/* ______________________________________________________________________ */
/*  Return pointer to element who's ID is given.
 *  Arguments:
 *	Element's ID attribute value (a string).
 *  Return:
 *	Pointer to element whose ID matches.
 */

Element_t *
FindElemByID(
    char	*idval
)
{
    ID_t	*id;
    for (id=IDList; id; id=id->next)
	if (id->id[0] == idval[0] && !strcmp(id->id, idval)) return id->elem;
    return 0;
}

/* ______________________________________________________________________ */

