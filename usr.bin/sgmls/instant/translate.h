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
 *  These are data definitions for the "translating" portion of the program.
 *
 * ________________________________________________________________________
 */

#ifdef STORAGE
#ifndef lint
static char *tr_h_RCSid =
  "$Header: /usr/src/docbook-to-man/Instant/RCS/translate.h,v 1.3 1996/06/02 21:47:32 fld Exp $";
#endif
#endif

#define L_CURLY		'{'
#define R_CURLY		'}'

/* things to ignore when processing an element */
#define IGN_NONE	0
#define IGN_ALL		1
#define IGN_DATA	2
#define IGN_CHILDREN	3

/* for CheckRelation() */
typedef enum { RA_Current, RA_Related } RelAction_t;

typedef struct {
    char	*name;		/* attribute name string */
    char	*val;		/* attribute value string */
    regexp	*rex;		/* attribute value reg expr (compiled) */
} AttPair_t;

typedef struct _Trans {
    /* criteria */
    char	*gi;		/* element name of tag under consideration */
    char	**gilist;	/* list of element names (multiple gi's) */
    char	*context;	/* context in tree - looking depth levels up */
    regexp	*context_re;	/* tree heirarchy looking depth levels up */
    int		depth;		/* number of levels to look up the tree */
    AttPair_t	*attpair;	/* attr name-value pairs */
    int		nattpairs;	/* number of name-value pairs */
    char	*parent;	/* GI has this element as parent */
    int		nth_child;	/* GI is Nth child of this of parent element */
    char	*content;	/* element has this string in content */
    regexp	*content_re;	/* content reg expr (compiled) */
    char	*pattrset;	/* is this attr set (any value) in parent? */
    char	*var_name;	/* variable name */
    char	*var_value;	/* variable value */
    char	*var_RE_name;	/* variable name (for VarREValue) */
    regexp	*var_RE_value;	/* variable value (compiled, for VarREValue) */
    Map_t	*relations;	/* various relations to check */

    /* actions */
    char	*starttext;	/* string to output at the start tag */
    char	*endtext;	/* string to output at the end tag */
    char	*replace;	/* string to replace this subtree with */
    char	*message;	/* message for stderr, if element encountered */
    int		ignore;		/* flag - ignore content or data of element? */
    int		verbatim;	/* flag - pass content verbatim or do cmap? */
    char	*var_reset;
    char	*increment;	/* increment these variables */
    Map_t	*set_var;	/* set these variables */
    Map_t	*incr_var;	/* increment these variables */
    char	*quit;		/* print message and exit */

    /* pointers and bookkeeping */
    int		my_id;		/* unique (hopefully) ID of this transpec */
    int		use_id;		/* use transpec whose ID is this */
    struct _Trans *use_trans;	/* pointer to other transpec */
    struct _Trans *next;	/* linked list */
    int		lineno;		/* line number of end of transpec */
} Trans_t;

#ifdef def
#undef def
#endif
#ifdef STORAGE
# define def
#else
# define def    extern
#endif

def Trans_t	*TrSpecs;
def Mapping_t	*CharMap;
def int		nCharMap;

/* prototypes for things defined in translate.c */
int	CheckRelation(Element_t *, char *, char *, char *, FILE*, RelAction_t);
Trans_t	*FindTrans(Element_t *, int);
Trans_t	*FindTransByName(char *);
Trans_t	*FindTransByID(int);
void	PrepTranspecs(Element_t *);
void	ProcessOneSpec(char *, Element_t *, FILE *, int);
void	TransElement(Element_t *, FILE *, Trans_t *);
void	TranByAction(Element_t *, int, FILE *);
void	TranTByAction(Element_t *, char *, FILE *);

/* prototypes for things defined in tranvar.c */
void	ExpandSpecialVar(char *, Element_t *, FILE *, int);

/* prototypes for things defined in tables.c */
void	OSFtable(Element_t *, FILE *, char **, int);

/* ______________________________________________________________________ */

