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
 *  Common definitions for "instant" program.
 * ________________________________________________________________________
 */

#ifdef STORAGE
#ifndef lint
static char *gen_h_RCSid =
  "$Header: /usr/src/docbook-to-man/Instant/RCS/general.h,v 1.5 1996/06/11 20:25:03 fld Exp $";
#endif
#endif

/* string/numeric/character definitions */

#define EOS		'\0'
#define NL		'\n'
#define TAB		'\t'
#define CR		'\r'
#define ANCHOR		'^'

/* bigmask/flags for the Split() function */
#define S_STRDUP	0x01
#define S_ALVEC		0x02

/*  Command codes (1st char of esis lines) from sgmls.  See its manpage. */
#define CMD_DATA	'-'
#define CMD_OPEN	'('
#define CMD_CLOSE	')'
#define CMD_ATT		'A'
#define CMD_D_ATT	'D'
#define CMD_NOTATION	'N'
#define CMD_EXT_ENT	'E'
#define CMD_INT_ENT	'I'
#define CMD_SYSID	's'
#define CMD_PUBID	'p'
#define CMD_FILENAME	'f'
#define CMD_LINE	'L'
#define CMD_PI		'?'
#define CMD_SUBDOC	'S'
#define CMD_SUBDOC_S	'{'
#define CMD_SUBDOC_E	'}'
#define CMD_EXT_REF	'&'
#define CMD_APPINFO	'#'
#define CMD_CONFORM	'C'

/*  Some sizes */
#define MAX_DEPTH	40
#define LINESIZE	60000

/*  Name of library env variable, and default value. */
#ifndef TPT_LIB
#define TPT_LIB	"TPT_LIB"
#endif
#ifndef DEF_TPT_LIB
#define DEF_TPT_LIB	"/usr/share/sgml/transpec"
#endif

/*  Relationships - for querying */
typedef enum {
    REL_None, REL_Parent, REL_Child, REL_Ancestor, REL_Descendant,
    REL_Sibling, REL_Preceding, REL_ImmPreceding, REL_Following,
    REL_ImmFollowing, REL_Cousin, REL_Unknown
} Relation_t;

/* Initial map sizes (IMS) */
#define IMS_relations		3
#define IMS_setvar		3
#define IMS_incvar		3
#define IMS_sdata		50
#define IMS_sdatacache		30
#define IMS_variables		20
#define IMS_attnames		50
#define IMS_elemnames		50

/* ----- typedef and other misc definitions ----- */

#ifndef TRUE
#define TRUE (1 == 1)
#endif

#ifndef FALSE
#define FALSE (1 == 0)
#endif

typedef short bool;


/* ----- structure definitions ----- */

/*  We use this for variables, attributes, etc., so the caller only needs an
 *  opaque handle to the thing below, not worrying about array management.  */
typedef struct {
    char	*name;			/* name of the thing */
    char	*sval;			/* string value */
} Mapping_t;

typedef struct {
    int		n_alloc;		/* number of elements allocated */
    int		n_used;			/* number of elements used */
    int		slot_incr;		/* increment for allocating slots */
    int		flags;			/* info about this set of mappings */
    Mapping_t	*maps;			/* array of mappings */
} Map_t;

/* ______________________________________________________________________ */

/*  Information about an entity reference.  Not all fields will be used
 *  at once.  */
typedef struct _ent {
    char	*type;			/* entity type */
    char	*ename;			/* entity name */
    char	*nname;			/* notation name */
    char	*sysid;			/* sys id */
    char	*pubid;			/* pub id */
    char	*fname;			/* filename */
    struct _ent	*next;			/* next in linked list */
} Entity_t;

/*  Content (child nodes) of an element (node in the tree) -- both data
 *  and other elements.  */
typedef struct {
    char		type;		/* element, data, or pi? */
    union {
	struct _elem	*elem;		/* direct children of this elem */
	char		*data;		/* character data of this elem */
    } ch;
} Content_t;

/*  An element (node in the tree) */
typedef struct _elem {
    char	*gi;			/* element GI */
    Content_t	*cont;			/* content - element & data children */
    int		ncont;			/* # of content/children */
    struct _elem **econt;		/* element children */
    int		necont;			/* # of element children */
    char	**dcont;		/* character data children */
    int		ndcont;			/* # of data children */
    Mapping_t	*atts;			/* array of attributes */
    int		natts;			/* # of attributes */
    Entity_t	*entity;		/* ext entity & notation info */
    char	*id;			/* for linking */
    int		index;			/* an internal bookkeeping mechanism */
    int		depth;			/* how deep in tree */
    int		lineno;			/* line number */
    char	*infile;		/* input filename */
    int		my_eorder;		/* order of this elem of its parent */
    struct _elem *parent;		/* this elem's direct parent */
    struct _elem *next;			/* kept in linked list */
    void	*trans;			/* pointer to translation spec */
    /* I'm not crazy about this, but it works */
    int		gen_trans[2];		/* refs to generated trans specs */
    int		processed;		/* was this node processed? */
} Element_t;

/*  For mapping of element IDs to elements themselves.  */
typedef struct id_s {
    char	*id;			/* ID of the element */
    Element_t	*elem;			/* pointer to it */
    struct id_s	*next;
} ID_t;

/* ----- global variable declarations ----- */

#ifdef STORAGE
# define def
#else
# define def	extern
#endif

def Element_t	*DocTree;		/* root of document tree */
def char	**UsedElem;		/* a unique list of used elem names */
def int		nUsedElem;		/* number of used elem names */
def char	**UsedAtt;		/* a unique list of used attrib names */
def int		nUsedAtt;		/* number of used attrib names */
def ID_t	*IDList;		/* list of IDs used in the doc */
def Map_t	*Variables;		/* general, global variables */
def Map_t	*SDATAmap;		/* SDATA mappings */
def Map_t	*PImap;			/* Processing Instruction mappings */
def Entity_t	*Entities;		/* list of entities */

def FILE	*outfp;			/* where output is written */
def char	*tpt_lib;		/* TPT library directory */
def int		verbose;		/* flag - verbose output? */
def int		warnings;		/* flag - show warnings? */
def int		interactive;		/* flag - interactive browsing? */
def int		slave;			/* are we slave to another process? */
def int		fold_case;		/* flag - fold case of GIs? */

/* ----- some macros for convenience and ease of code reading ----- */

#define stripNL(s)	{ char *_cp; if ((_cp=strchr(s, NL))) *_cp = EOS; }

/*  Similar to calloc(), malloc(), and realloc(), but check for success.
 *  Args to all:
 *	(1) number of 'elements' to allocate
 *	(2) variable to point at allocated space
 *	(3) type of 'element'
 *  Eg:	Calloc(5, e, Element_t) replaces
 *	if (!(e = (Element_t *)calloc(5, sizeof(Element_t))) {
 *		... handle error ... ;
 *	}
 */
#define Calloc(N,V,T)	\
    { if (!((V) = (T *)calloc((size_t)N, sizeof(T)))) { \
	perror("Calloc failed -- out of memory.  Bailing out.");  exit(1); \
    }; memset((void *) (V), 0, (size_t) sizeof(T)); }
#define Malloc(N,V,T)	\
    { if (!((V) = (T *)malloc((size_t)N*sizeof(T)))) { \
	perror("Malloc failed -- out of memory.  Bailing out.");  exit(1); \
    }; memset((void *) (V), 0, (size_t) sizeof(T)); }
#define Realloc(N,V,T)	\
    { if (!((V) = (T *)realloc(V,(size_t)N*sizeof(T)))) { \
	perror("Realloc failed -- out of memory.  Bailing out.");  exit(1); \
    } }

/*  similar to strcmp(), but check first chars first, for efficiency */
#define StrEq(s1,s2)	(s1[0] == s2[0] && !strcmp(s1,s2))

/*  similar to isspace(), but check for blank or tab - without overhead
 *  of procedure call */
#define IsWhite(c)	(c == ' ' || c == TAB || c == NL)

#define ContType(e,i)	(e->cont[i].type)
#define ContData(e,i)	(e->cont[i].ch.data)
#define ContElem(e,i)	(e->cont[i].ch.elem)
#define IsContData(e,i)	(e->cont[i].type == CMD_DATA)
#define IsContElem(e,i)	(e->cont[i].type == CMD_OPEN)
#define IsContPI(e,i)	(e->cont[i].type == CMD_PI)

/* ----- function prototypes ----- */

/* things defined in util.c */
Element_t	*QRelation(Element_t *, char *, Relation_t);
Relation_t	FindRelByName(char *);
char		*FindAttValByName(Element_t *, char *);
char		*FindContext(Element_t *, int, char *);
char		*AddElemName(char *);
char		*AddAttName(char *);
char 	    	*ExpandString(char *);
void		 OutputString(char *, FILE *, int);
char		*LookupSDATA(char *);
FILE		*OpenFile(char *);
char		*FilePath(char *);
char		*FindElementPath(Element_t *, char *);
char		*NearestOlderElem(Element_t *, char *);
void		PrintLocation(Element_t *, FILE *);
char		**Split(char *, int *, int);
void		DescendTree(Element_t *, void(*)(), void(*)(), void(*)(), void *);
Map_t		*NewMap(int);
Mapping_t	*FindMapping(Map_t *, const char *);
char		*FindMappingVal(Map_t *, const char *);
void		SetMapping(Map_t *, const char *);
void		SetMappingNV(Map_t *, const char *, const char *);
void		AddID(Element_t *, char *);
Element_t	*FindElemByID(char *);

/* things defined in translate.c */
void		DoTranslate(Element_t*, FILE *);
void	    	ExpandVariables(char*, char*, Element_t*);

/* things defined in traninit.c */
void		ReadTransSpec(char *);

/* things defined in tranvar.c */
char		*Get_A_C_value(const char *);

/* things defined in info.c */
void		PrintContext(Element_t *e);
void		PrintElemSummary(Element_t *);
void		PrintElemTree(Element_t *);
void		PrintStats(Element_t *);
void		PrintIDList();

/* things defined in table.c */
void	    	CALStable(Element_t *, FILE *, char **, int);

/* ----- other declarations ----- */

#ifdef ultrix
#define strdup(s)	strcpy((char *)malloc(strlen(s)+1), s)
#endif

