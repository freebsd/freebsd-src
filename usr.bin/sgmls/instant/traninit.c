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
 *  This module contains the initialization routines for translation module.
 *  They mostly deal with reading data files (translation specs, SDATA
 *  mappings, character mappings).
 *
 *  Entry points:
 *	ReadTransSpec(transfile)	read/store translation spec from file
 * ________________________________________________________________________
 */

#ifndef lint
static char *RCSid =
  "$Header: /usr/local/home/jfieber/src/cvsroot/nsgmlfmt/traninit.c,v 1.1.1.1 1996/01/16 05:14:10 jfieber Exp $";
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

#include "sgmls.h"
#include "config.h"
#include "std.h"

#ifndef TRUE
#define TRUE	(1 == 1)
#endif

#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define MIN(a, b) ((a) < (b) ? (a) : (b))

/* forward references */
void	RememberTransSpec(Trans_t *, int);
static void do_data(char *gi, struct sgmls_data *v, int n);
static void build_ts(char *gi, char* cp);
void	AddCharMap(const char *from, const char* to);
void	AddSDATA(const char *from, const char *to);

/* ______________________________________________________________________ */
/*  Read the translation specs from the input file, storing in memory.
 *  Arguments:
 *	Name of translation spec file.
 */

static Trans_t T;


static
void input_error(num, str, lineno)
     int num;
     char *str;
     unsigned long lineno;
{
  fprintf(stderr, "Error at input line %lu: %s\n", lineno, str);
}

void
ReadTransSpec(
    char *transfile
)
{
    FILE *fp;
    struct sgmls *sp;
    struct sgmls_event e;
    char gi[LINESIZE];
    char buf[LINESIZE];
    char buf2[LINESIZE];
    char *command;
    char *sgmls = "sgmls ";
    char maptype = '\0';

    (void)sgmls_set_errhandler(input_error);
    transfile = FilePath(transfile);
    if (!transfile)
    {
    	fprintf(stderr, "Error: Could not locate specified transfile\n");
    	exit(1);
    }

    /* XXX this is a quick, gross hack.  Should write a parse() function. */
    Malloc(strlen(sgmls) + strlen(transfile) + 2, command, char);
    sprintf(command, "%s %s", sgmls, transfile);
    fp = popen(command, "r");

    sp = sgmls_create(fp);
    while (sgmls_next(sp, &e))
    switch (e.type) {
    	case SGMLS_EVENT_DATA:
    	    do_data(gi, e.u.data.v, e.u.data.n);
    	    break;
    	case SGMLS_EVENT_ENTITY:
    	    fprintf(stderr, "Hm... got an entity\n");
    	    break;
    	case SGMLS_EVENT_PI:
    	    break;
    	case SGMLS_EVENT_START:
    	    if (strncmp("RULE", e.u.start.gi, 4) == 0) {
    	    	/* A new transpec, so clear the data structure
    	    	 * and look for an ID attribute.
    	    	 */
	    	struct sgmls_attribute *attr = e.u.start.attributes;
	    	memset(&T, 0, sizeof T);
    	    	while (attr) {
    	    	    if (attr->type == SGMLS_ATTR_CDATA
    	    	    	&& strncmp("ID", attr->name, 2) == 0) {
    	    	    	strncpy(buf, attr->value.data.v->s, 
    	    	    	    MIN(attr->value.data.v->len, LINESIZE));
    	    	    	buf[MIN(attr->value.data.v->len, LINESIZE - 1)] = '\0';
    	    	    	T.my_id = atoi(buf);
    	    	    }
    	    	    attr = attr->next;
    	    	}
    	    }
    	    else if (strncmp("CMAP", e.u.start.gi, 4) == 0)
    	    	maptype = 'c';
    	    else if (strncmp("SMAP", e.u.start.gi, 4) == 0)
    	    	maptype = 's';
    	    else if (strncmp("MAP", e.u.start.gi, 3) == 0) {
    	    	struct sgmls_attribute *attr = e.u.start.attributes;
    	    	char *from = 0;
    	    	char *to = 0;
    	    	
    	    	while (attr) {
    	    	    if (attr->value.data.v && strncmp("FROM", attr->name, 4) == 0) {
    	    	    	strncpy(buf, attr->value.data.v->s, 
    	    	    	    MIN(attr->value.data.v->len, LINESIZE - 1));
    	    	    	buf[MIN(attr->value.data.v->len, LINESIZE - 1)] = '\0';
    	    	    }
   	    	    if (attr->value.data.v && strncmp("TO", attr->name, 2) == 0) {
    	    	    	strncpy(buf2, attr->value.data.v->s, 
    	    	    	    MIN(attr->value.data.v->len, LINESIZE - 1));
    	    	    	buf2[MIN(attr->value.data.v->len, LINESIZE - 1)] = '\0';
    	    	    }
    	    	    attr = attr->next;
    	    	}
    	    	if (maptype == 'c')
    	    	    AddCharMap(buf, buf2);
    	    	else if (maptype == 's')
    	    	    AddSDATA(buf, buf2);
    	    	else
    	    	    fprintf(stderr, "Unknown map type!\n");
    	    }
    	    else {
    	    	strncpy(gi, e.u.start.gi, 512);
    	    	sgmls_free_attributes(e.u.start.attributes);
    	    }
    	    break;
    	case SGMLS_EVENT_END:
    	    if (strncmp("RULE", e.u.start.gi, 4) == 0)
    	    	RememberTransSpec(&T, e.lineno);
    	    break;
    	case SGMLS_EVENT_SUBSTART:
    	    break;
    	case SGMLS_EVENT_SUBEND:
    	    break;
    	case SGMLS_EVENT_APPINFO:
    	    break;
    	case SGMLS_EVENT_CONFORMING:
    	    break;
    	default:
    	    abort();
    }
    sgmls_free(sp);
    pclose(fp);
    free(command);
}


static void do_data(char *gi, struct sgmls_data *v, int n)
{
    int i;
    char *cp;
    static char *buf = 0;
    static int buf_size = 0;
    int buf_pos = 0;

  
    /* figure out how much space this element will really 
       take, inculding expanded sdata entities. */

    if (!buf)
    {
    	buf_size = 1024;
    	Malloc(buf_size, buf, char);
    }

    for (i = 0; i < n; i++)
    {
    	char *s;
    	int len;
    	
    	/* Mark the current position.  If this is SDATA
    	   we will have to return here. */
    	int tmp_buf_pos = buf_pos;
    	
    	/* Make sure the buffer is big enough. */
    	if (buf_size - buf_pos <= v[i].len)
    	{
    	    buf_size += v[i].len * (n - i);
    	    Realloc(buf_size, buf, char);
    	}

    	s = v[i].s;
    	len = v[i].len;
    	for (; len > 0; len--, s++)
     	{
	    if (*s != RSCHAR) {
		if (*s == RECHAR)
		    buf[buf_pos] = '\n';
		else
		    buf[buf_pos] = *s;
		buf_pos++;
	    }
    	}
    	if (v[i].is_sdata)
    	{
    	    char *p;
    	    buf[buf_pos] = '\0';
    	    p = LookupSDATA(buf + tmp_buf_pos);
    	    if (p)
    	    {
    	    	if (buf_size - tmp_buf_pos <= strlen(p))
    	    	{
    		    buf_size += strlen(p) * (n - i);
    		    Realloc(buf_size, buf, char);
    	    	}
    	    	strcpy(buf + tmp_buf_pos, p);
    	    	buf_pos = tmp_buf_pos + strlen(p);
    	    }
    	}
    }

    /* Clean up the trailing end of the data. */
    buf[buf_pos] = '\0';
    buf_pos--;
    while (buf_pos > 0  && isspace(buf[buf_pos]) && buf[buf_pos] != '\n')
    	buf_pos--;
    if (buf[buf_pos] == '\n')
    	buf[buf_pos] = '\0';
    
    /* Skip over whitespace at the beginning of the data. */
    cp = buf;
    while (*cp && isspace(*cp))
    	cp++;
    build_ts(gi, cp);
}

/* ______________________________________________________________________ */
/*  Set a transpec parameter
 *  Arguments:
 *	gi - the parameter to set
 *	cp - the value of the parameter
 */
static void build_ts(char *gi, char* cp)
{
    if (strcmp("GI", gi) == 0)
    {
    	char *cp2;
	/* if we are folding the case of GIs, make all upper (unless
	   it's an internal pseudo-GI name, which starts with '_') */
	if (fold_case && cp[0] != '_' && cp[0] != '#')
	{
	    for (cp2=cp; *cp2; cp2++)
		if (islower(*cp2)) *cp2 = toupper(*cp2);
	}
	T.gi = AddElemName(cp);
    }
    else if (strcmp("START", gi) == 0)
    	T.starttext = strdup(cp);
    else if (strcmp("END", gi) == 0)
    	T.endtext = strdup(cp);
    else if (strcmp("RELATION", gi) == 0)
    {
	if (!T.relations)
	    T.relations = NewMap(IMS_relations);
	SetMapping(T.relations, cp);
    }
    else if (strcmp("REPLACE", gi) == 0)
    	T.replace = strdup(cp);
    else if (strcmp("ATTVAL", gi) == 0)
    {
	if (!T.nattpairs) 
	{
	    Malloc(1, T.attpair, AttPair_t);
	}
	else
	    Realloc((T.nattpairs+1), T.attpair, AttPair_t);
	/* we'll split name/value pairs later */
	T.attpair[T.nattpairs].name = strdup(cp);
	T.nattpairs++;
    }
    else if (strcmp("CONTEXT", gi) == 0)
    	T.context = strdup(cp);
    else if (strcmp("MESSAGE", gi) == 0)
    	T.message = strdup(cp);
    else if (strcmp("DO", gi) == 0)
    	T.use_id = atoi(cp);
    else if (strcmp("CONTENT", gi) == 0)
    	T.content = strdup(cp);
    else if (strcmp("PATTSET", gi) == 0)
    	T.pattrset = strdup(cp);
    else if (strcmp("VERBATIM", gi) == 0)
    	T.verbatim = TRUE;
    else if (strcmp("IGNORE", gi) == 0)
    {
	if (!strcmp(cp, "all"))
	    T.ignore = IGN_ALL;
	else if (!strcmp(cp, "data"))
	    T.ignore = IGN_DATA;
	else if (!strcmp(cp, "children"))
	    T.ignore = IGN_CHILDREN;
	else
	    fprintf(stderr, "Bad 'Ignore:' arg in transpec %s: %s\n",
		    gi, cp);
    }
    else if (strcmp("VARVAL", gi) == 0)
    {
	char **tok;
	int i = 2;
	tok = Split(cp, &i, S_STRDUP);
	T.var_name	= tok[0];
	T.var_value	= tok[1];
    }
    else if (strcmp("VARREVAL", gi) == 0)
    {
    	char buf[1000];
    	char **tok;
	int i = 2;
	tok = Split(cp, &i, S_STRDUP);
	T.var_RE_name = tok[0];
	ExpandVariables(tok[1], buf, 0);
	if (!(T.var_RE_value=regcomp(buf)))	{
	    fprintf(stderr, "Regex error in VarREValue Content: %s\n",
				    tok[1]);
	}
    }
    else if (strcmp("SET", gi) == 0)
    {
	if (!T.set_var)
	    T.set_var = NewMap(IMS_setvar);
	SetMapping(T.set_var, cp);
    }
    else if (strcmp("INCR", gi) == 0)
    {
	if (!T.incr_var)
	    T.incr_var = NewMap(IMS_incvar);
	SetMapping(T.incr_var, cp);
    }
    else if (strcmp("NTHCHILD", gi) == 0)
    	T.nth_child = atoi(cp);
    else if (strcmp("VAR", gi) == 0)
    	SetMapping(Variables, cp);
    else if (strcmp("QUIT", gi) == 0)
    	T.quit = strdup(cp);
    else
	fprintf(stderr, "Unknown translation spec (skipping it): %s\n",	gi);
    
}


/* ______________________________________________________________________ */
/*  Store translation spec 't' in memory.
 *  Arguments:
 *	Pointer to translation spec to remember.
 *	Line number where translation spec ends.
 */
void
RememberTransSpec(
    Trans_t	*t,
    int		lineno
)
{
    char	*cp;
    int		i, do_regex;
    static Trans_t *last_t;
    char buf[1000];

    /* If context testing, check some details and set things up for later. */
    if (t->context) {
	/* See if the context specified is a regular expression.
	 * If so, compile the reg expr.  It is assumed to be a regex if
	 * it contains a character other than what's allowed for GIs in the
	 * OSF sgml declaration (alphas, nums, '-', and '.').
	 */
	for (do_regex=0,cp=t->context; *cp; cp++) {
	    if (!isalnum(*cp) && *cp != '-' && *cp != '.' && *cp != ' ') {
		do_regex = 1;
		break;
	    }
	}

	if (do_regex) {
	    t->depth = MAX_DEPTH;
	    if (!(t->context_re=regcomp(t->context))) {
		fprintf(stderr, "Regex error in Context: %s\n", t->context);
	    }
	}
	else {
	    /* If there's only one item in context, it's the parent.  Treat
	     * it specially, since it's faster to just check parent gi.
	     */
	    cp = t->context;
	    if (!strchr(cp, ' ')) {
		t->parent  = t->context;
		t->context = NULL;
	    }
	    else {
		/* Figure out depth of context string */
		t->depth = 0;
		while (*cp) {
		    if (*cp) t->depth++;
		    while (*cp && !IsWhite(*cp)) cp++;	/* find end of gi */
		    while (*cp && IsWhite(*cp)) cp++;	/* skip space */
		}
	    }
	}
    }

    /* Compile regular expressions for each attribute */
    for (i=0; i<t->nattpairs; i++) {
	/* Initially, name points to "name value".  Split them... */
	cp = t->attpair[i].name;
	while (*cp && !IsWhite(*cp)) cp++;	/* point past end of name */
	if (*cp) {	/* value found */
	    *cp++ = EOS;			/* terminate name */
	    while (*cp && IsWhite(*cp)) cp++;	/* point to value */
	    ExpandVariables(cp, buf, 0);	/* expand any variables */
	    t->attpair[i].val = strdup(buf);
	}
	else {		/* value not found */
	    t->attpair[i].val = ".";
	}
	if (!(t->attpair[i].rex=regcomp(t->attpair[i].val))) {
	    fprintf(stderr, "Regex error in AttValue: %s %s\n",
		    t->attpair[i].name, t->attpair[i].val);
	}
    }

    /* Compile regular expression for content */
    t->content_re = 0;
    if (t->content) {
	ExpandVariables(t->content, buf, 0);
	if (!(t->content_re=regcomp(buf)))
	    fprintf(stderr, "Regex error in Content: %s\n",
		    t->content);
    }

    /* If multiple GIs, break up into a vector, then remember it.  We either
     * sture the individual, or the list - not both. */
    if (t->gi && strchr(t->gi, ' ')) {
	t->gilist = Split(t->gi, 0, S_ALVEC);
	t->gi = NULL;
    }

    /* Now, store structure in linked list. */
    if (!TrSpecs) {
	Malloc(1, TrSpecs, Trans_t);
	last_t = TrSpecs;
    }
    else {
	Malloc(1, last_t->next, Trans_t);
	last_t = last_t->next;
    }
    *last_t = *t;
}


/* ______________________________________________________________________ */
/*  Add an entry to the character mapping table, allocating or
 *  expanding the table if necessary.
 *  Arguments:
 *	Character to map
 *      String to map the character to
 *  	A 'c' or an 's' for character or sdata map
 */

void
AddCharMap(
    const char *from,
    const char* to
)
{
    static int n_alloc = 0;

    if (from && to) {
	if (nCharMap >= n_alloc) {
	    n_alloc += 32;
    	    if (!CharMap) {
    		Malloc(n_alloc, CharMap, Mapping_t);
    	    }
    	    else {
		Realloc(n_alloc, CharMap, Mapping_t);
	    }
	}
    	CharMap[nCharMap].name = strdup(from);
    	CharMap[nCharMap].sval = strdup(to);
        nCharMap++;
    }
}

/* ______________________________________________________________________ */
/*  Add an entry to the SDATA mapping table.
 *  Arguments:
 *	String to map
 *      String to map to
 */

void
AddSDATA(
    const char *from,
    const char *to
)
{
    if (from && to) {
        if (!SDATAmap)
            SDATAmap = NewMap(IMS_sdata);
    	SetMappingNV(SDATAmap, from, to);
    }
}

/* ______________________________________________________________________ */
