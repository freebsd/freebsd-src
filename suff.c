/*	$NetBSD: suff.c,v 1.142 2020/08/31 16:44:25 rillig Exp $	*/

/*
 * Copyright (c) 1988, 1989, 1990, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Adam de Boor.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
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

/*
 * Copyright (c) 1989 by Berkeley Softworks
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Adam de Boor.
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

#ifndef MAKE_NATIVE
static char rcsid[] = "$NetBSD: suff.c,v 1.142 2020/08/31 16:44:25 rillig Exp $";
#else
#include <sys/cdefs.h>
#ifndef lint
#if 0
static char sccsid[] = "@(#)suff.c	8.4 (Berkeley) 3/21/94";
#else
__RCSID("$NetBSD: suff.c,v 1.142 2020/08/31 16:44:25 rillig Exp $");
#endif
#endif /* not lint */
#endif

/*-
 * suff.c --
 *	Functions to maintain suffix lists and find implicit dependents
 *	using suffix transformation rules
 *
 * Interface:
 *	Suff_Init 	    	Initialize all things to do with suffixes.
 *
 *	Suff_End 	    	Cleanup the module
 *
 *	Suff_DoPaths	    	This function is used to make life easier
 *	    	  	    	when searching for a file according to its
 *	    	  	    	suffix. It takes the global search path,
 *	    	  	    	as defined using the .PATH: target, and appends
 *	    	  	    	its directories to the path of each of the
 *	    	  	    	defined suffixes, as specified using
 *	    	  	    	.PATH<suffix>: targets. In addition, all
 *	    	  	    	directories given for suffixes labeled as
 *	    	  	    	include files or libraries, using the .INCLUDES
 *	    	  	    	or .LIBS targets, are played with using
 *	    	  	    	Dir_MakeFlags to create the .INCLUDES and
 *	    	  	    	.LIBS global variables.
 *
 *	Suff_ClearSuffixes  	Clear out all the suffixes and defined
 *	    	  	    	transformations.
 *
 *	Suff_IsTransform    	Return TRUE if the passed string is the lhs
 *	    	  	    	of a transformation rule.
 *
 *	Suff_AddSuffix	    	Add the passed string as another known suffix.
 *
 *	Suff_GetPath	    	Return the search path for the given suffix.
 *
 *	Suff_AddInclude	    	Mark the given suffix as denoting an include
 *	    	  	    	file.
 *
 *	Suff_AddLib	    	Mark the given suffix as denoting a library.
 *
 *	Suff_AddTransform   	Add another transformation to the suffix
 *	    	  	    	graph. Returns  GNode suitable for framing, I
 *	    	  	    	mean, tacking commands, attributes, etc. on.
 *
 *	Suff_SetNull	    	Define the suffix to consider the suffix of
 *	    	  	    	any file that doesn't have a known one.
 *
 *	Suff_FindDeps	    	Find implicit sources for and the location of
 *	    	  	    	a target based on its suffix. Returns the
 *	    	  	    	bottom-most node added to the graph or NULL
 *	    	  	    	if the target had no implicit sources.
 *
 *	Suff_FindPath	    	Return the appropriate path to search in
 *				order to find the node.
 */

#include	  "make.h"
#include	  "dir.h"

#define SUFF_DEBUG0(fmt) \
    if (!DEBUG(SUFF)) (void) 0; else fprintf(debug_file, fmt)

#define SUFF_DEBUG1(fmt, arg1) \
    if (!DEBUG(SUFF)) (void) 0; else fprintf(debug_file, fmt, arg1)

#define SUFF_DEBUG2(fmt, arg1, arg2) \
    if (!DEBUG(SUFF)) (void) 0; else fprintf(debug_file, fmt, arg1, arg2)

#define SUFF_DEBUG3(fmt, arg1, arg2, arg3) \
    if (!DEBUG(SUFF)) (void) 0; else fprintf(debug_file, fmt, arg1, arg2, arg3)

static Lst       sufflist;	/* Lst of suffixes */
#ifdef CLEANUP
static Lst	 suffClean;	/* Lst of suffixes to be cleaned */
#endif
static Lst	 srclist;	/* Lst of sources */
static Lst       transforms;	/* Lst of transformation rules */

static int        sNum = 0;	/* Counter for assigning suffix numbers */

typedef enum {
    SUFF_INCLUDE	= 0x01,	/* One which is #include'd */
    SUFF_LIBRARY	= 0x02,	/* One which contains a library */
    SUFF_NULL		= 0x04	/* The empty suffix */
    /* XXX: Why is SUFF_NULL needed? Wouldn't nameLen == 0 mean the same? */
} SuffFlags;

ENUM_FLAGS_RTTI_3(SuffFlags,
		  SUFF_INCLUDE, SUFF_LIBRARY, SUFF_NULL);

/*
 * Structure describing an individual suffix.
 */
typedef struct Suff {
    char         *name;	    	/* The suffix itself, such as ".c" */
    int		 nameLen;	/* Length of the name, to avoid strlen calls */
    SuffFlags	 flags;      	/* Type of suffix */
    Lst    	 searchPath;	/* The path along which files of this suffix
				 * may be found */
    int          sNum;	      	/* The suffix number */
    int		 refCount;	/* Reference count of list membership */
    Lst          parents;	/* Suffixes we have a transformation to */
    Lst          children;	/* Suffixes we have a transformation from */
    Lst		 ref;		/* List of lists this suffix is referenced */
} Suff;

/*
 * Structure used in the search for implied sources.
 */
typedef struct _Src {
    char            *file;	/* The file to look for */
    char    	    *pref;  	/* Prefix from which file was formed */
    Suff            *suff;	/* The suffix on the file */
    struct _Src     *parent;	/* The Src for which this is a source */
    GNode           *node;	/* The node describing the file */
    int	    	    children;	/* Count of existing children (so we don't free
				 * this thing too early or never nuke it) */
#ifdef DEBUG_SRC
    Lst		    cp;		/* Debug; children list */
#endif
} Src;

/*
 * A structure for passing more than one argument to the Lst-library-invoked
 * function...
 */
typedef struct {
    Lst            l;
    Src            *s;
} LstSrc;

typedef struct {
    GNode	  **gn;
    Suff	   *s;
    Boolean	    r;
} GNodeSuff;

static Suff 	    *suffNull;	/* The NULL suffix for this run */
static Suff 	    *emptySuff;	/* The empty suffix required for POSIX
				 * single-suffix transformation rules */


static void SuffUnRef(void *, void *);
static void SuffFree(void *);
static void SuffInsert(Lst, Suff *);
static void SuffRemove(Lst, Suff *);
static Boolean SuffParseTransform(char *, Suff **, Suff **);
static int SuffRebuildGraph(void *, void *);
static int SuffScanTargets(void *, void *);
static int SuffAddSrc(void *, void *);
static void SuffAddLevel(Lst, Src *);
static void SuffExpandChildren(LstNode, GNode *);
static void SuffExpandWildcards(LstNode, GNode *);
static Boolean SuffApplyTransform(GNode *, GNode *, Suff *, Suff *);
static void SuffFindDeps(GNode *, Lst);
static void SuffFindArchiveDeps(GNode *, Lst);
static void SuffFindNormalDeps(GNode *, Lst);
static int SuffPrintName(void *, void *);
static int SuffPrintSuff(void *, void *);
static int SuffPrintTrans(void *, void *);

	/*************** Lst Predicates ****************/
/*-
 *-----------------------------------------------------------------------
 * SuffStrIsPrefix  --
 *	See if pref is a prefix of str.
 *
 * Input:
 *	pref		possible prefix
 *	str		string to check
 *
 * Results:
 *	NULL if it ain't, pointer to character in str after prefix if so
 *
 * Side Effects:
 *	None
 *-----------------------------------------------------------------------
 */
static const char *
SuffStrIsPrefix(const char *pref, const char *str)
{
    while (*str && *pref == *str) {
	pref++;
	str++;
    }

    return *pref ? NULL : str;
}

typedef struct {
    char	*ename;		/* The end of the name */
    int		 len;		/* Length of the name */
} SuffSuffGetSuffixArgs;

/* See if suff is a suffix of str. str->ename should point to THE END
 * of the string to check. (THE END == the null byte)
 *
 * Input:
 *	s		possible suffix
 *	str		string to examine
 *
 * Results:
 *	NULL if it ain't, pointer to character in str before suffix if
 *	it is.
 */
static char *
SuffSuffGetSuffix(const Suff *s, const SuffSuffGetSuffixArgs *str)
{
    char  *p1;	    	/* Pointer into suffix name */
    char  *p2;	    	/* Pointer into string being examined */

    if (str->len < s->nameLen)
	return NULL;		/* this string is shorter than the suffix */

    p1 = s->name + s->nameLen;
    p2 = str->ename;

    while (p1 >= s->name && *p1 == *p2) {
	p1--;
	p2--;
    }

    return p1 == s->name - 1 ? p2 : NULL;
}

/* Predicate form of SuffSuffGetSuffix, for Lst_Find. */
static Boolean
SuffSuffIsSuffix(const void *s, const void *sd)
{
    return SuffSuffGetSuffix(s, sd) != NULL;
}

/* See if the suffix has the desired name. */
static Boolean
SuffSuffHasName(const void *s, const void *desiredName)
{
    return strcmp(((const Suff *)s)->name, desiredName) == 0;
}

/* See if the suffix name is a prefix of the string. Care must be taken when
 * using this to search for transformations and what-not, since there could
 * well be two suffixes, one of which is a prefix of the other... */
static Boolean
SuffSuffIsPrefix(const void *s, const void *str)
{
    return SuffStrIsPrefix(((const Suff *)s)->name, str) != NULL;
}

/* See if the graph node has the desired name. */
static Boolean
SuffGNHasName(const void *gn, const void *desiredName)
{
    return strcmp(((const GNode *)gn)->name, desiredName) == 0;
}

	    /*********** Maintenance Functions ************/

static void
SuffUnRef(void *lp, void *sp)
{
    Lst l = (Lst) lp;

    LstNode ln = Lst_FindDatum(l, sp);
    if (ln != NULL) {
	Lst_Remove(l, ln);
	((Suff *)sp)->refCount--;
    }
}

/* Free up all memory associated with the given suffix structure. */
static void
SuffFree(void *sp)
{
    Suff *s = (Suff *)sp;

    if (s == suffNull)
	suffNull = NULL;

    if (s == emptySuff)
	emptySuff = NULL;

#ifdef notdef
    /* We don't delete suffixes in order, so we cannot use this */
    if (s->refCount)
	Punt("Internal error deleting suffix `%s' with refcount = %d", s->name,
	    s->refCount);
#endif

    Lst_Free(s->ref);
    Lst_Free(s->children);
    Lst_Free(s->parents);
    Lst_Destroy(s->searchPath, Dir_Destroy);

    free(s->name);
    free(s);
}

/* Remove the suffix from the list, and free if it is otherwise unused. */
static void
SuffRemove(Lst l, Suff *s)
{
    SuffUnRef(l, s);
    if (s->refCount == 0) {
	SuffUnRef(sufflist, s);
	SuffFree(s);
    }
}

/* Insert the suffix into the list keeping the list ordered by suffix numbers.
 *
 * Input:
 *	l		the list where in s should be inserted
 *	s		the suffix to insert
 */
static void
SuffInsert(Lst l, Suff *s)
{
    LstNode 	  ln;		/* current element in l we're examining */
    Suff          *s2 = NULL;	/* the suffix descriptor in this element */

    Lst_Open(l);
    while ((ln = Lst_Next(l)) != NULL) {
	s2 = LstNode_Datum(ln);
	if (s2->sNum >= s->sNum) {
	    break;
	}
    }
    Lst_Close(l);

    SUFF_DEBUG2("inserting %s(%d)...", s->name, s->sNum);

    if (ln == NULL) {
	SUFF_DEBUG0("at end of list\n");
	Lst_Append(l, s);
	s->refCount++;
	Lst_Append(s->ref, l);
    } else if (s2->sNum != s->sNum) {
	SUFF_DEBUG2("before %s(%d)\n", s2->name, s2->sNum);
	Lst_InsertBefore(l, ln, s);
	s->refCount++;
	Lst_Append(s->ref, l);
    } else {
	SUFF_DEBUG0("already there\n");
    }
}

static Suff *
SuffNew(const char *name)
{
    Suff *s = bmake_malloc(sizeof(Suff));

    s->name =   	bmake_strdup(name);
    s->nameLen = 	strlen(s->name);
    s->searchPath = Lst_Init();
    s->children = 	Lst_Init();
    s->parents = 	Lst_Init();
    s->ref = 	Lst_Init();
    s->sNum =   	sNum++;
    s->flags =  	0;
    s->refCount =	1;

    return s;
}

/* This is gross. Nuke the list of suffixes but keep all transformation
 * rules around. The transformation graph is destroyed in this process, but
 * we leave the list of rules so when a new graph is formed the rules will
 * remain. This function is called from the parse module when a .SUFFIXES:\n
 * line is encountered. */
void
Suff_ClearSuffixes(void)
{
#ifdef CLEANUP
    Lst_MoveAll(suffClean, sufflist);
#endif
    sufflist = Lst_Init();
    sNum = 0;
    if (suffNull)
	SuffFree(suffNull);
    emptySuff = suffNull = SuffNew("");

    Dir_Concat(suffNull->searchPath, dirSearchPath);
    suffNull->flags =  	    SUFF_NULL;
}

/* Parse a transformation string to find its two component suffixes.
 *
 * Input:
 *	str		String being parsed
 *	out_src		Place to store source of trans.
 *	out_targ	Place to store target of trans.
 *
 * Results:
 *	TRUE if the string is a valid transformation, FALSE otherwise.
 */
static Boolean
SuffParseTransform(char *str, Suff **out_src, Suff **out_targ)
{
    LstNode		srcLn;	    /* element in suffix list of trans source*/
    Suff		*src;	    /* Source of transformation */
    LstNode		targLn;	    /* element in suffix list of trans target*/
    char		*str2;	    /* Extra pointer (maybe target suffix) */
    LstNode 	    	singleLn;   /* element in suffix list of any suffix
				     * that exactly matches str */
    Suff    	    	*single = NULL;/* Source of possible transformation to
				     * null suffix */

    srcLn = NULL;
    singleLn = NULL;

    /*
     * Loop looking first for a suffix that matches the start of the
     * string and then for one that exactly matches the rest of it. If
     * we can find two that meet these criteria, we've successfully
     * parsed the string.
     */
    for (;;) {
	if (srcLn == NULL) {
	    srcLn = Lst_Find(sufflist, SuffSuffIsPrefix, str);
	} else {
	    srcLn = Lst_FindFrom(sufflist, LstNode_Next(srcLn),
				  SuffSuffIsPrefix, str);
	}
	if (srcLn == NULL) {
	    /*
	     * Ran out of source suffixes -- no such rule
	     */
	    if (singleLn != NULL) {
		/*
		 * Not so fast Mr. Smith! There was a suffix that encompassed
		 * the entire string, so we assume it was a transformation
		 * to the null suffix (thank you POSIX). We still prefer to
		 * find a double rule over a singleton, hence we leave this
		 * check until the end.
		 *
		 * XXX: Use emptySuff over suffNull?
		 */
		*out_src = single;
		*out_targ = suffNull;
		return TRUE;
	    }
	    return FALSE;
	}
	src = LstNode_Datum(srcLn);
	str2 = str + src->nameLen;
	if (*str2 == '\0') {
	    single = src;
	    singleLn = srcLn;
	} else {
	    targLn = Lst_Find(sufflist, SuffSuffHasName, str2);
	    if (targLn != NULL) {
		*out_src = src;
		*out_targ = LstNode_Datum(targLn);
		return TRUE;
	    }
	}
    }
}

/* Return TRUE if the given string is a transformation rule, that is, a
 * concatenation of two known suffixes. */
Boolean
Suff_IsTransform(char *str)
{
    Suff    	  *src, *targ;

    return SuffParseTransform(str, &src, &targ);
}

/* Add the transformation rule described by the line to the list of rules
 * and place the transformation itself in the graph.
 *
 * The node is placed on the end of the transforms Lst and links are made
 * between the two suffixes mentioned in the target name.

 * Input:
 *	line		name of transformation to add
 *
 * Results:
 *	The node created for the transformation in the transforms list
 */
GNode *
Suff_AddTransform(char *line)
{
    GNode         *gn;		/* GNode of transformation rule */
    Suff          *s,		/* source suffix */
		  *t;		/* target suffix */
    LstNode 	  ln;	    	/* Node for existing transformation */
    Boolean ok;

    ln = Lst_Find(transforms, SuffGNHasName, line);
    if (ln == NULL) {
	/*
	 * Make a new graph node for the transformation. It will be filled in
	 * by the Parse module.
	 */
	gn = Targ_NewGN(line);
	Lst_Append(transforms, gn);
    } else {
	/*
	 * New specification for transformation rule. Just nuke the old list
	 * of commands so they can be filled in again... We don't actually
	 * free the commands themselves, because a given command can be
	 * attached to several different transformations.
	 */
	gn = LstNode_Datum(ln);
	Lst_Free(gn->commands);
	Lst_Free(gn->children);
	gn->commands = Lst_Init();
	gn->children = Lst_Init();
    }

    gn->type = OP_TRANSFORM;

    ok = SuffParseTransform(line, &s, &t);
    assert(ok);
    (void)ok;

    /*
     * link the two together in the proper relationship and order
     */
    SUFF_DEBUG2("defining transformation from `%s' to `%s'\n",
		s->name, t->name);
    SuffInsert(t->children, s);
    SuffInsert(s->parents, t);

    return gn;
}

/* Handle the finish of a transformation definition, removing the
 * transformation from the graph if it has neither commands nor sources.
 * This is a callback procedure for the Parse module via Lst_ForEach.
 *
 * If the node has no commands or children, the children and parents lists
 * of the affected suffixes are altered.
 *
 * Input:
 *	gnp		Node for transformation
 *
 * Results:
 *	0, so that Lst_ForEach continues
 */
int
Suff_EndTransform(void *gnp, void *dummy MAKE_ATTR_UNUSED)
{
    GNode *gn = (GNode *)gnp;

    if ((gn->type & OP_DOUBLEDEP) && !Lst_IsEmpty(gn->cohorts))
	gn = LstNode_Datum(Lst_Last(gn->cohorts));
    if ((gn->type & OP_TRANSFORM) && Lst_IsEmpty(gn->commands) &&
	Lst_IsEmpty(gn->children))
    {
	Suff	*s, *t;

	/*
	 * SuffParseTransform() may fail for special rules which are not
	 * actual transformation rules. (e.g. .DEFAULT)
	 */
	if (SuffParseTransform(gn->name, &s, &t)) {
	    Lst	 p;

	    SUFF_DEBUG2("deleting transformation from `%s' to `%s'\n",
			s->name, t->name);

	    /*
	     * Store s->parents because s could be deleted in SuffRemove
	     */
	    p = s->parents;

	    /*
	     * Remove the source from the target's children list. We check for a
	     * nil return to handle a beanhead saying something like
	     *  .c.o .c.o:
	     *
	     * We'll be called twice when the next target is seen, but .c and .o
	     * are only linked once...
	     */
	    SuffRemove(t->children, s);

	    /*
	     * Remove the target from the source's parents list
	     */
	    SuffRemove(p, t);
	}
    } else if (gn->type & OP_TRANSFORM) {
        SUFF_DEBUG1("transformation %s complete\n", gn->name);
    }

    return 0;
}

/* Called from Suff_AddSuffix via Lst_ForEach to search through the list of
 * existing transformation rules and rebuild the transformation graph when
 * it has been destroyed by Suff_ClearSuffixes. If the given rule is a
 * transformation involving this suffix and another, existing suffix, the
 * proper relationship is established between the two.
 *
 * The appropriate links will be made between this suffix and others if
 * transformation rules exist for it.
 *
 * Input:
 *	transformp	Transformation to test
 *	sp		Suffix to rebuild
 *
 * Results:
 *	0, so that Lst_ForEach continues
 */
static int
SuffRebuildGraph(void *transformp, void *sp)
{
    GNode   	*transform = (GNode *)transformp;
    Suff    	*s = (Suff *)sp;
    char 	*cp;
    LstNode	ln;
    Suff  	*s2;
    SuffSuffGetSuffixArgs sd;

    /*
     * First see if it is a transformation from this suffix.
     */
    cp = UNCONST(SuffStrIsPrefix(s->name, transform->name));
    if (cp != NULL) {
	ln = Lst_Find(sufflist, SuffSuffHasName, cp);
	if (ln != NULL) {
	    /*
	     * Found target. Link in and return, since it can't be anything
	     * else.
	     */
	    s2 = LstNode_Datum(ln);
	    SuffInsert(s2->children, s);
	    SuffInsert(s->parents, s2);
	    return 0;
	}
    }

    /*
     * Not from, maybe to?
     */
    sd.len = strlen(transform->name);
    sd.ename = transform->name + sd.len;
    cp = SuffSuffGetSuffix(s, &sd);
    if (cp != NULL) {
	/*
	 * Null-terminate the source suffix in order to find it.
	 */
	cp[1] = '\0';
	ln = Lst_Find(sufflist, SuffSuffHasName, transform->name);
	/*
	 * Replace the start of the target suffix
	 */
	cp[1] = s->name[0];
	if (ln != NULL) {
	    /*
	     * Found it -- establish the proper relationship
	     */
	    s2 = LstNode_Datum(ln);
	    SuffInsert(s->children, s2);
	    SuffInsert(s2->parents, s);
	}
    }
    return 0;
}

/* Called from Suff_AddSuffix via Lst_ForEach to search through the list of
 * existing targets and find if any of the existing targets can be turned
 * into a transformation rule.
 *
 * If such a target is found and the target is the current main target, the
 * main target is set to NULL and the next target examined (if that exists)
 * becomes the main target.
 *
 * Results:
 *	1 if a new main target has been selected, 0 otherwise.
 */
static int
SuffScanTargets(void *targetp, void *gsp)
{
    GNode   	*target = (GNode *)targetp;
    GNodeSuff	*gs = (GNodeSuff *)gsp;
    Suff	*s, *t;
    char 	*ptr;

    if (*gs->gn == NULL && gs->r && (target->type & OP_NOTARGET) == 0) {
	*gs->gn = target;
	Targ_SetMain(target);
	return 1;
    }

    if (target->type == OP_TRANSFORM)
	return 0;

    if ((ptr = strstr(target->name, gs->s->name)) == NULL ||
	ptr == target->name)
	return 0;

    if (SuffParseTransform(target->name, &s, &t)) {
	if (*gs->gn == target) {
	    gs->r = TRUE;
	    *gs->gn = NULL;
	    Targ_SetMain(NULL);
	}
	Lst_Free(target->children);
	target->children = Lst_Init();
	target->type = OP_TRANSFORM;
	/*
	 * link the two together in the proper relationship and order
	 */
	SUFF_DEBUG2("defining transformation from `%s' to `%s'\n",
		    s->name, t->name);
	SuffInsert(t->children, s);
	SuffInsert(s->parents, t);
    }
    return 0;
}

/* Add the suffix to the end of the list of known suffixes.
 * Should we restructure the suffix graph? Make doesn't...
 *
 * A GNode is created for the suffix and a Suff structure is created and
 * added to the suffixes list unless the suffix was already known.
 * The mainNode passed can be modified if a target mutated into a
 * transform and that target happened to be the main target.
 *
 * Input:
 *	name		the name of the suffix to add
 */
void
Suff_AddSuffix(const char *name, GNode **gn)
{
    Suff          *s;	    /* new suffix descriptor */
    LstNode 	  ln;
    GNodeSuff	  gs;

    ln = Lst_Find(sufflist, SuffSuffHasName, name);
    if (ln == NULL) {
        s = SuffNew(name);

	Lst_Append(sufflist, s);
	/*
	 * We also look at our existing targets list to see if adding
	 * this suffix will make one of our current targets mutate into
	 * a suffix rule. This is ugly, but other makes treat all targets
	 * that start with a . as suffix rules.
	 */
	gs.gn = gn;
	gs.s  = s;
	gs.r  = FALSE;
	Lst_ForEach(Targ_List(), SuffScanTargets, &gs);
	/*
	 * Look for any existing transformations from or to this suffix.
	 * XXX: Only do this after a Suff_ClearSuffixes?
	 */
	Lst_ForEach(transforms, SuffRebuildGraph, s);
    }
}

/* Return the search path for the given suffix, or NULL. */
Lst
Suff_GetPath(char *sname)
{
    LstNode   	  ln;
    Suff    	  *s;

    ln = Lst_Find(sufflist, SuffSuffHasName, sname);
    if (ln == NULL) {
	return NULL;
    } else {
	s = LstNode_Datum(ln);
	return s->searchPath;
    }
}

/* Extend the search paths for all suffixes to include the default search
 * path.
 *
 * The searchPath field of all the suffixes is extended by the directories
 * in dirSearchPath. If paths were specified for the ".h" suffix, the
 * directories are stuffed into a global variable called ".INCLUDES" with
 * each directory preceded by a -I. The same is done for the ".a" suffix,
 * except the variable is called ".LIBS" and the flag is -L.
 */
void
Suff_DoPaths(void)
{
    Suff	   	*s;
    LstNode  		ln;
    char		*ptr;
    Lst	    	    	inIncludes; /* Cumulative .INCLUDES path */
    Lst	    	    	inLibs;	    /* Cumulative .LIBS path */


    inIncludes = Lst_Init();
    inLibs = Lst_Init();

    Lst_Open(sufflist);
    while ((ln = Lst_Next(sufflist)) != NULL) {
	s = LstNode_Datum(ln);
	if (!Lst_IsEmpty(s->searchPath)) {
#ifdef INCLUDES
	    if (s->flags & SUFF_INCLUDE) {
		Dir_Concat(inIncludes, s->searchPath);
	    }
#endif /* INCLUDES */
#ifdef LIBRARIES
	    if (s->flags & SUFF_LIBRARY) {
		Dir_Concat(inLibs, s->searchPath);
	    }
#endif /* LIBRARIES */
	    Dir_Concat(s->searchPath, dirSearchPath);
	} else {
	    Lst_Destroy(s->searchPath, Dir_Destroy);
	    s->searchPath = Lst_Copy(dirSearchPath, Dir_CopyDir);
	}
    }
    Lst_Close(sufflist);

    Var_Set(".INCLUDES", ptr = Dir_MakeFlags("-I", inIncludes), VAR_GLOBAL);
    free(ptr);
    Var_Set(".LIBS", ptr = Dir_MakeFlags("-L", inLibs), VAR_GLOBAL);
    free(ptr);

    Lst_Destroy(inIncludes, Dir_Destroy);
    Lst_Destroy(inLibs, Dir_Destroy);
}

/* Add the given suffix as a type of file which gets included.
 * Called from the parse module when a .INCLUDES line is parsed.
 * The suffix must have already been defined.
 * The SUFF_INCLUDE bit is set in the suffix's flags field.
 *
 * Input:
 *	sname		Name of the suffix to mark
 */
void
Suff_AddInclude(char *sname)
{
    LstNode	  ln;
    Suff	  *s;

    ln = Lst_Find(sufflist, SuffSuffHasName, sname);
    if (ln != NULL) {
	s = LstNode_Datum(ln);
	s->flags |= SUFF_INCLUDE;
    }
}

/* Add the given suffix as a type of file which is a library.
 * Called from the parse module when parsing a .LIBS line.
 * The suffix must have been defined via .SUFFIXES before this is called.
 * The SUFF_LIBRARY bit is set in the suffix's flags field.
 *
 * Input:
 *	sname		Name of the suffix to mark
 */
void
Suff_AddLib(const char *sname)
{
    LstNode	  ln;
    Suff	  *s;

    ln = Lst_Find(sufflist, SuffSuffHasName, sname);
    if (ln != NULL) {
	s = LstNode_Datum(ln);
	s->flags |= SUFF_LIBRARY;
    }
}

	  /********** Implicit Source Search Functions *********/

/* Add a suffix as a Src structure to the given list with its parent
 * being the given Src structure. If the suffix is the null suffix,
 * the prefix is used unaltered as the file name in the Src structure.
 *
 * Input:
 *	sp		suffix for which to create a Src structure
 *	lsp		list and parent for the new Src
 *
 * Results:
 *	0, so that Lst_ForEach continues
 */
static int
SuffAddSrc(void *sp, void *lsp)
{
    Suff	*s = (Suff *)sp;
    LstSrc      *ls = (LstSrc *)lsp;
    Src         *s2;	    /* new Src structure */
    Src    	*targ; 	    /* Target structure */

    targ = ls->s;

    if ((s->flags & SUFF_NULL) && (*s->name != '\0')) {
	/*
	 * If the suffix has been marked as the NULL suffix, also create a Src
	 * structure for a file with no suffix attached. Two birds, and all
	 * that...
	 */
	s2 = bmake_malloc(sizeof(Src));
	s2->file =  	bmake_strdup(targ->pref);
	s2->pref =  	targ->pref;
	s2->parent = 	targ;
	s2->node =  	NULL;
	s2->suff =  	s;
	s->refCount++;
	s2->children =	0;
	targ->children += 1;
	Lst_Append(ls->l, s2);
#ifdef DEBUG_SRC
	s2->cp = Lst_Init();
	Lst_Append(targ->cp, s2);
	fprintf(debug_file, "1 add %p %p to %p:", targ, s2, ls->l);
	Lst_ForEach(ls->l, PrintAddr, NULL);
	fprintf(debug_file, "\n");
#endif
    }
    s2 = bmake_malloc(sizeof(Src));
    s2->file = 	    str_concat2(targ->pref, s->name);
    s2->pref =	    targ->pref;
    s2->parent =    targ;
    s2->node = 	    NULL;
    s2->suff = 	    s;
    s->refCount++;
    s2->children =  0;
    targ->children += 1;
    Lst_Append(ls->l, s2);
#ifdef DEBUG_SRC
    s2->cp = Lst_Init();
    Lst_Append(targ->cp, s2);
    fprintf(debug_file, "2 add %p %p to %p:", targ, s2, ls->l);
    Lst_ForEach(ls->l, PrintAddr, NULL);
    fprintf(debug_file, "\n");
#endif

    return 0;
}

/* Add all the children of targ as Src structures to the given list.
 *
 * Input:
 *	l		list to which to add the new level
 *	targ		Src structure to use as the parent
 */
static void
SuffAddLevel(Lst l, Src *targ)
{
    LstSrc         ls;

    ls.s = targ;
    ls.l = l;

    Lst_ForEach(targ->suff->children, SuffAddSrc, &ls);
}

/* Free the first Src in the list that doesn't have a reference count.
 * Return whether a Src was removed. */
static Boolean
SuffRemoveSrc(Lst l)
{
    LstNode ln;
    Src *s;

    Lst_Open(l);

#ifdef DEBUG_SRC
    fprintf(debug_file, "cleaning %lx: ", (unsigned long) l);
    Lst_ForEach(l, PrintAddr, NULL);
    fprintf(debug_file, "\n");
#endif

    while ((ln = Lst_Next(l)) != NULL) {
	s = LstNode_Datum(ln);
	if (s->children == 0) {
	    free(s->file);
	    if (!s->parent)
		free(s->pref);
	    else {
#ifdef DEBUG_SRC
		LstNode ln2 = Lst_FindDatum(s->parent->cp, s);
		if (ln2 != NULL)
		    Lst_Remove(s->parent->cp, ln2);
#endif
		--s->parent->children;
	    }
#ifdef DEBUG_SRC
	    fprintf(debug_file, "free: [l=%p] p=%p %d\n", l, s, s->children);
	    Lst_Free(s->cp);
#endif
	    Lst_Remove(l, ln);
	    free(s);
	    Lst_Close(l);
	    return TRUE;
	}
#ifdef DEBUG_SRC
	else {
	    fprintf(debug_file, "keep: [l=%p] p=%p %d: ", l, s, s->children);
	    Lst_ForEach(s->cp, PrintAddr, NULL);
	    fprintf(debug_file, "\n");
	}
#endif
    }

    Lst_Close(l);

    return FALSE;
}

/* Find the first existing file/target in the list srcs.
 *
 * Input:
 *	srcs		list of Src structures to search through
 *
 * Results:
 *	The lowest structure in the chain of transformations, or NULL.
 */
static Src *
SuffFindThem(Lst srcs, Lst slst)
{
    Src            *s;		/* current Src */
    Src		   *rs;		/* returned Src */
    char	   *ptr;

    rs = NULL;

    while (!Lst_IsEmpty(srcs)) {
	s = Lst_Dequeue(srcs);

	SUFF_DEBUG1("\ttrying %s...", s->file);

	/*
	 * A file is considered to exist if either a node exists in the
	 * graph for it or the file actually exists.
	 */
	if (Targ_FindNode(s->file, TARG_NOCREATE) != NULL) {
#ifdef DEBUG_SRC
	    fprintf(debug_file, "remove %p from %p\n", s, srcs);
#endif
	    rs = s;
	    break;
	}

	if ((ptr = Dir_FindFile(s->file, s->suff->searchPath)) != NULL) {
	    rs = s;
#ifdef DEBUG_SRC
	    fprintf(debug_file, "remove %p from %p\n", s, srcs);
#endif
	    free(ptr);
	    break;
	}

	SUFF_DEBUG0("not there\n");

	SuffAddLevel(srcs, s);
	Lst_Append(slst, s);
    }

    if (rs) {
	SUFF_DEBUG0("got it\n");
    }
    return rs;
}

/* See if any of the children of the target in the Src structure is one from
 * which the target can be transformed. If there is one, a Src structure is
 * put together for it and returned.
 *
 * Input:
 *	targ		Src to play with
 *
 * Results:
 *	The Src of the "winning" child, or NULL.
 */
static Src *
SuffFindCmds(Src *targ, Lst slst)
{
    LstNode 	  	ln; 	/* General-purpose list node */
    GNode		*t, 	/* Target GNode */
			*s; 	/* Source GNode */
    int	    	  	prefLen;/* The length of the defined prefix */
    Suff    	  	*suff;	/* Suffix on matching beastie */
    Src	    	  	*ret;	/* Return value */
    char    	  	*cp;

    t = targ->node;
    Lst_Open(t->children);
    prefLen = strlen(targ->pref);

    for (;;) {
	ln = Lst_Next(t->children);
	if (ln == NULL) {
	    Lst_Close(t->children);
	    return NULL;
	}
	s = LstNode_Datum(ln);

	if (s->type & OP_OPTIONAL && Lst_IsEmpty(t->commands)) {
	    /*
	     * We haven't looked to see if .OPTIONAL files exist yet, so
	     * don't use one as the implicit source.
	     * This allows us to use .OPTIONAL in .depend files so make won't
	     * complain "don't know how to make xxx.h' when a dependent file
	     * has been moved/deleted.
	     */
	    continue;
	}

	cp = strrchr(s->name, '/');
	if (cp == NULL) {
	    cp = s->name;
	} else {
	    cp++;
	}
	if (strncmp(cp, targ->pref, prefLen) != 0)
	    continue;
	/*
	 * The node matches the prefix ok, see if it has a known
	 * suffix.
	 */
	ln = Lst_Find(sufflist, SuffSuffHasName, &cp[prefLen]);
	if (ln == NULL)
	    continue;
	/*
	 * It even has a known suffix, see if there's a transformation
	 * defined between the node's suffix and the target's suffix.
	 *
	 * XXX: Handle multi-stage transformations here, too.
	 */
	suff = LstNode_Datum(ln);

	/* XXX: Can targ->suff be NULL here? */
	if (targ->suff != NULL &&
	    Lst_FindDatum(suff->parents, targ->suff) != NULL)
	    break;
    }

    /*
     * Hot Damn! Create a new Src structure to describe
     * this transformation (making sure to duplicate the
     * source node's name so Suff_FindDeps can free it
     * again (ick)), and return the new structure.
     */
    ret = bmake_malloc(sizeof(Src));
    ret->file = bmake_strdup(s->name);
    ret->pref = targ->pref;
    ret->suff = suff;
    suff->refCount++;
    ret->parent = targ;
    ret->node = s;
    ret->children = 0;
    targ->children += 1;
#ifdef DEBUG_SRC
    ret->cp = Lst_Init();
    fprintf(debug_file, "3 add %p %p\n", targ, ret);
    Lst_Append(targ->cp, ret);
#endif
    Lst_Append(slst, ret);
    SUFF_DEBUG1("\tusing existing source %s\n", s->name);
    Lst_Close(t->children);
    return ret;
}

/* Expand the names of any children of a given node that contain variable
 * invocations or file wildcards into actual targets.
 *
 * The expanded node is removed from the parent's list of children, and the
 * parent's unmade counter is decremented, but other nodes may be added.
 *
 * Input:
 *	cln		Child to examine
 *	pgn		Parent node being processed
 */
static void
SuffExpandChildren(LstNode cln, GNode *pgn)
{
    GNode   	*cgn = LstNode_Datum(cln);
    GNode	*gn;	    /* New source 8) */
    char	*cp;	    /* Expanded value */

    if (!Lst_IsEmpty(cgn->order_pred) || !Lst_IsEmpty(cgn->order_succ))
	/* It is all too hard to process the result of .ORDER */
	return;

    if (cgn->type & OP_WAIT)
	/* Ignore these (& OP_PHONY ?) */
	return;

    /*
     * First do variable expansion -- this takes precedence over
     * wildcard expansion. If the result contains wildcards, they'll be gotten
     * to later since the resulting words are tacked on to the end of
     * the children list.
     */
    if (strchr(cgn->name, '$') == NULL) {
	SuffExpandWildcards(cln, pgn);
	return;
    }

    SUFF_DEBUG1("Expanding \"%s\"...", cgn->name);
    cp = Var_Subst(cgn->name, pgn, VARE_UNDEFERR|VARE_WANTRES);

    {
	Lst	    members = Lst_Init();

	if (cgn->type & OP_ARCHV) {
	    /*
	     * Node was an archive(member) target, so we want to call
	     * on the Arch module to find the nodes for us, expanding
	     * variables in the parent's context.
	     */
	    char	*sacrifice = cp;

	    (void)Arch_ParseArchive(&sacrifice, members, pgn);
	} else {
	    /*
	     * Break the result into a vector of strings whose nodes
	     * we can find, then add those nodes to the members list.
	     * Unfortunately, we can't use brk_string b/c it
	     * doesn't understand about variable specifications with
	     * spaces in them...
	     */
	    char	    *start;
	    char	    *initcp = cp;   /* For freeing... */

	    for (start = cp; *start == ' ' || *start == '\t'; start++)
		continue;
	    for (cp = start; *cp != '\0'; cp++) {
		if (*cp == ' ' || *cp == '\t') {
		    /*
		     * White-space -- terminate element, find the node,
		     * add it, skip any further spaces.
		     */
		    *cp++ = '\0';
		    gn = Targ_FindNode(start, TARG_CREATE);
		    Lst_Append(members, gn);
		    while (*cp == ' ' || *cp == '\t') {
			cp++;
		    }
		    /*
		     * Adjust cp for increment at start of loop, but
		     * set start to first non-space.
		     */
		    start = cp--;
		} else if (*cp == '$') {
		    /*
		     * Start of a variable spec -- contact variable module
		     * to find the end so we can skip over it.
		     */
		    const char	*junk;
		    int 	len;
		    void	*freeIt;

		    junk = Var_Parse(cp, pgn, VARE_UNDEFERR|VARE_WANTRES,
				     &len, &freeIt);
		    if (junk != var_Error) {
			cp += len - 1;
		    }

		    free(freeIt);
		} else if (*cp == '\\' && cp[1] != '\0') {
		    /*
		     * Escaped something -- skip over it
		     */
		    cp++;
		}
	    }

	    if (cp != start) {
		/*
		 * Stuff left over -- add it to the list too
		 */
		gn = Targ_FindNode(start, TARG_CREATE);
		Lst_Append(members, gn);
	    }
	    /*
	     * Point cp back at the beginning again so the variable value
	     * can be freed.
	     */
	    cp = initcp;
	}

	/*
	 * Add all elements of the members list to the parent node.
	 */
	while(!Lst_IsEmpty(members)) {
	    gn = Lst_Dequeue(members);

	    SUFF_DEBUG1("%s...", gn->name);
	    /* Add gn to the parents child list before the original child */
	    Lst_InsertBefore(pgn->children, cln, gn);
	    Lst_Append(gn->parents, pgn);
	    pgn->unmade++;
	    /* Expand wildcards on new node */
	    SuffExpandWildcards(LstNode_Prev(cln), pgn);
	}
	Lst_Free(members);

	/*
	 * Free the result
	 */
	free(cp);
    }

    SUFF_DEBUG0("\n");

    /*
     * Now the source is expanded, remove it from the list of children to
     * keep it from being processed.
     */
    pgn->unmade--;
    Lst_Remove(pgn->children, cln);
    Lst_Remove(cgn->parents, Lst_FindDatum(cgn->parents, pgn));
}

static void
SuffExpandWildcards(LstNode cln, GNode *pgn)
{
    GNode   	*cgn = LstNode_Datum(cln);
    GNode	*gn;	    /* New source 8) */
    char	*cp;	    /* Expanded value */
    Lst 	explist;    /* List of expansions */

    if (!Dir_HasWildcards(cgn->name))
	return;

    /*
     * Expand the word along the chosen path
     */
    explist = Lst_Init();
    Dir_Expand(cgn->name, Suff_FindPath(cgn), explist);

    while (!Lst_IsEmpty(explist)) {
	/*
	 * Fetch next expansion off the list and find its GNode
	 */
	cp = Lst_Dequeue(explist);

	SUFF_DEBUG1("%s...", cp);
	gn = Targ_FindNode(cp, TARG_CREATE);

	/* Add gn to the parents child list before the original child */
	Lst_InsertBefore(pgn->children, cln, gn);
	Lst_Append(gn->parents, pgn);
	pgn->unmade++;
    }

    Lst_Free(explist);

    SUFF_DEBUG0("\n");

    /*
     * Now the source is expanded, remove it from the list of children to
     * keep it from being processed.
     */
    pgn->unmade--;
    Lst_Remove(pgn->children, cln);
    Lst_Remove(cgn->parents, Lst_FindDatum(cgn->parents, pgn));
}

/* Find a path along which to expand the node.
 *
 * If the word has a known suffix, use that path.
 * If it has no known suffix, use the default system search path.
 *
 * Input:
 *	gn		Node being examined
 *
 * Results:
 *	The appropriate path to search for the GNode.
 */
Lst
Suff_FindPath(GNode* gn)
{
    Suff *suff = gn->suffix;

    if (suff == NULL) {
	SuffSuffGetSuffixArgs sd;   /* Search string data */
	LstNode ln;
	sd.len = strlen(gn->name);
	sd.ename = gn->name + sd.len;
	ln = Lst_Find(sufflist, SuffSuffIsSuffix, &sd);

	SUFF_DEBUG1("Wildcard expanding \"%s\"...", gn->name);
	if (ln != NULL)
	    suff = LstNode_Datum(ln);
	/* XXX: Here we can save the suffix so we don't have to do this again */
    }

    if (suff != NULL) {
	SUFF_DEBUG1("suffix is \"%s\"...", suff->name);
	return suff->searchPath;
    } else {
	/*
	 * Use default search path
	 */
	return dirSearchPath;
    }
}

/* Apply a transformation rule, given the source and target nodes and
 * suffixes.
 *
 * Input:
 *	tGn		Target node
 *	sGn		Source node
 *	t		Target suffix
 *	s		Source suffix
 *
 * Results:
 *	TRUE if successful, FALSE if not.
 *
 * Side Effects:
 *	The source and target are linked and the commands from the
 *	transformation are added to the target node's commands list.
 *	All attributes but OP_DEPMASK and OP_TRANSFORM are applied
 *	to the target. The target also inherits all the sources for
 *	the transformation rule.
 */
static Boolean
SuffApplyTransform(GNode *tGn, GNode *sGn, Suff *t, Suff *s)
{
    LstNode 	ln, nln;    /* General node */
    char    	*tname;	    /* Name of transformation rule */
    GNode   	*gn;	    /* Node for same */

    /*
     * Form the proper links between the target and source.
     */
    Lst_Append(tGn->children, sGn);
    Lst_Append(sGn->parents, tGn);
    tGn->unmade += 1;

    /*
     * Locate the transformation rule itself
     */
    tname = str_concat2(s->name, t->name);
    ln = Lst_Find(transforms, SuffGNHasName, tname);
    free(tname);

    if (ln == NULL) {
	/*
	 * Not really such a transformation rule (can happen when we're
	 * called to link an OP_MEMBER and OP_ARCHV node), so return
	 * FALSE.
	 */
	return FALSE;
    }

    gn = LstNode_Datum(ln);

    SUFF_DEBUG3("\tapplying %s -> %s to \"%s\"\n", s->name, t->name, tGn->name);

    /*
     * Record last child for expansion purposes
     */
    ln = Lst_Last(tGn->children);

    /*
     * Pass the buck to Make_HandleUse to apply the rule
     */
    (void)Make_HandleUse(gn, tGn);

    /*
     * Deal with wildcards and variables in any acquired sources
     */
    for (ln = ln != NULL ? LstNode_Next(ln) : NULL; ln != NULL; ln = nln) {
	nln = LstNode_Next(ln);
	SuffExpandChildren(ln, tGn);
    }

    /*
     * Keep track of another parent to which this beast is transformed so
     * the .IMPSRC variable can be set correctly for the parent.
     */
    Lst_Append(sGn->implicitParents, tGn);

    return TRUE;
}


/* Locate dependencies for an OP_ARCHV node.
 *
 * Input:
 *	gn		Node for which to locate dependencies
 *
 * Side Effects:
 *	Same as Suff_FindDeps
 */
static void
SuffFindArchiveDeps(GNode *gn, Lst slst)
{
    char    	*eoarch;    /* End of archive portion */
    char    	*eoname;    /* End of member portion */
    GNode   	*mem;	    /* Node for member */
    LstNode 	ln, nln;    /* Next suffix node to check */
    Suff    	*ms;	    /* Suffix descriptor for member */
    char    	*name;	    /* Start of member's name */

    /*
     * The node is an archive(member) pair. so we must find a
     * suffix for both of them.
     */
    eoarch = strchr(gn->name, '(');
    eoname = strchr(eoarch, ')');

    /*
     * Caller guarantees the format `libname(member)', via
     * Arch_ParseArchive.
     */
    assert(eoarch != NULL);
    assert(eoname != NULL);

    *eoname = '\0';	  /* Nuke parentheses during suffix search */
    *eoarch = '\0';	  /* So a suffix can be found */

    name = eoarch + 1;

    /*
     * To simplify things, call Suff_FindDeps recursively on the member now,
     * so we can simply compare the member's .PREFIX and .TARGET variables
     * to locate its suffix. This allows us to figure out the suffix to
     * use for the archive without having to do a quadratic search over the
     * suffix list, backtracking for each one...
     */
    mem = Targ_FindNode(name, TARG_CREATE);
    SuffFindDeps(mem, slst);

    /*
     * Create the link between the two nodes right off
     */
    Lst_Append(gn->children, mem);
    Lst_Append(mem->parents, gn);
    gn->unmade += 1;

    /*
     * Copy in the variables from the member node to this one.
     */
    {
	char *freeIt;
	Var_Set(PREFIX, Var_Value(PREFIX, mem, &freeIt), gn);
	bmake_free(freeIt);
	Var_Set(TARGET, Var_Value(TARGET, mem, &freeIt), gn);
	bmake_free(freeIt);
    }

    ms = mem->suffix;
    if (ms == NULL) {
	/*
	 * Didn't know what it was -- use .NULL suffix if not in make mode
	 */
	SUFF_DEBUG0("using null suffix\n");
	ms = suffNull;
    }


    /*
     * Set the other two local variables required for this target.
     */
    Var_Set(MEMBER, name, gn);
    Var_Set(ARCHIVE, gn->name, gn);

    /*
     * Set $@ for compatibility with other makes
     */
    Var_Set(TARGET, gn->name, gn);

    /*
     * Now we've got the important local variables set, expand any sources
     * that still contain variables or wildcards in their names.
     */
    for (ln = Lst_First(gn->children); ln != NULL; ln = nln) {
	nln = LstNode_Next(ln);
	SuffExpandChildren(ln, gn);
    }

    if (ms != NULL) {
	/*
	 * Member has a known suffix, so look for a transformation rule from
	 * it to a possible suffix of the archive. Rather than searching
	 * through the entire list, we just look at suffixes to which the
	 * member's suffix may be transformed...
	 */
	SuffSuffGetSuffixArgs sd;	/* Search string data */

	/*
	 * Use first matching suffix...
	 */
	sd.len = eoarch - gn->name;
	sd.ename = eoarch;
	ln = Lst_Find(ms->parents, SuffSuffIsSuffix, &sd);

	if (ln != NULL) {
	    /*
	     * Got one -- apply it
	     */
	    Suff *suff = LstNode_Datum(ln);
	    if (!SuffApplyTransform(gn, mem, suff, ms)) {
		SUFF_DEBUG2("\tNo transformation from %s -> %s\n",
			    ms->name, suff->name);
	    }
	}
    }

    /*
     * Replace the opening and closing parens now we've no need of the separate
     * pieces.
     */
    *eoarch = '('; *eoname = ')';

    /*
     * Pretend gn appeared to the left of a dependency operator so
     * the user needn't provide a transformation from the member to the
     * archive.
     */
    if (OP_NOP(gn->type)) {
	gn->type |= OP_DEPENDS;
    }

    /*
     * Flag the member as such so we remember to look in the archive for
     * its modification time. The OP_JOIN | OP_MADE is needed because this
     * target should never get made.
     */
    mem->type |= OP_MEMBER | OP_JOIN | OP_MADE;
}

/* Locate implicit dependencies for regular targets.
 *
 * Input:
 *	gn		Node for which to find sources
 *
 * Side Effects:
 *	Same as Suff_FindDeps
 */
static void
SuffFindNormalDeps(GNode *gn, Lst slst)
{
    char    	*eoname;    /* End of name */
    char    	*sopref;    /* Start of prefix */
    LstNode 	ln, nln;    /* Next suffix node to check */
    Lst	    	srcs;	    /* List of sources at which to look */
    Lst	    	targs;	    /* List of targets to which things can be
			     * transformed. They all have the same file,
			     * but different suff and pref fields */
    Src	    	*bottom;    /* Start of found transformation path */
    Src 	*src;	    /* General Src pointer */
    char    	*pref;	    /* Prefix to use */
    Src	    	*targ;	    /* General Src target pointer */
    SuffSuffGetSuffixArgs sd; /* Search string data */


    sd.len = strlen(gn->name);
    sd.ename = eoname = gn->name + sd.len;

    sopref = gn->name;

    /*
     * Begin at the beginning...
     */
    ln = Lst_First(sufflist);
    srcs = Lst_Init();
    targs = Lst_Init();

    /*
     * We're caught in a catch-22 here. On the one hand, we want to use any
     * transformation implied by the target's sources, but we can't examine
     * the sources until we've expanded any variables/wildcards they may hold,
     * and we can't do that until we've set up the target's local variables
     * and we can't do that until we know what the proper suffix for the
     * target is (in case there are two suffixes one of which is a suffix of
     * the other) and we can't know that until we've found its implied
     * source, which we may not want to use if there's an existing source
     * that implies a different transformation.
     *
     * In an attempt to get around this, which may not work all the time,
     * but should work most of the time, we look for implied sources first,
     * checking transformations to all possible suffixes of the target,
     * use what we find to set the target's local variables, expand the
     * children, then look for any overriding transformations they imply.
     * Should we find one, we discard the one we found before.
     */
    bottom = NULL;
    targ = NULL;

    if (!(gn->type & OP_PHONY)) {

	while (ln != NULL) {
	    /*
	     * Look for next possible suffix...
	     */
	    ln = Lst_FindFrom(sufflist, ln, SuffSuffIsSuffix, &sd);

	    if (ln != NULL) {
	        const char *eopref;

		/*
		 * Allocate a Src structure to which things can be transformed
		 */
		targ = bmake_malloc(sizeof(Src));
		targ->file = bmake_strdup(gn->name);
		targ->suff = LstNode_Datum(ln);
		targ->suff->refCount++;
		targ->node = gn;
		targ->parent = NULL;
		targ->children = 0;
#ifdef DEBUG_SRC
		targ->cp = Lst_Init();
#endif

		eopref = eoname - targ->suff->nameLen;
		targ->pref = bmake_strsedup(sopref, eopref);

		/*
		 * Add nodes from which the target can be made
		 */
		SuffAddLevel(srcs, targ);

		/*
		 * Record the target so we can nuke it
		 */
		Lst_Append(targs, targ);

		/*
		 * Search from this suffix's successor...
		 */
		ln = LstNode_Next(ln);
	    }
	}

	/*
	 * Handle target of unknown suffix...
	 */
	if (Lst_IsEmpty(targs) && suffNull != NULL) {
	    SUFF_DEBUG1("\tNo known suffix on %s. Using .NULL suffix\n",
			gn->name);

	    targ = bmake_malloc(sizeof(Src));
	    targ->file = bmake_strdup(gn->name);
	    targ->suff = suffNull;
	    targ->suff->refCount++;
	    targ->node = gn;
	    targ->parent = NULL;
	    targ->children = 0;
	    targ->pref = bmake_strdup(sopref);
#ifdef DEBUG_SRC
	    targ->cp = Lst_Init();
#endif

	    /*
	     * Only use the default suffix rules if we don't have commands
	     * defined for this gnode; traditional make programs used to
	     * not define suffix rules if the gnode had children but we
	     * don't do this anymore.
	     */
	    if (Lst_IsEmpty(gn->commands))
		SuffAddLevel(srcs, targ);
	    else {
		SUFF_DEBUG0("not ");
	    }

	    SUFF_DEBUG0("adding suffix rules\n");

	    Lst_Append(targs, targ);
	}

	/*
	 * Using the list of possible sources built up from the target
	 * suffix(es), try and find an existing file/target that matches.
	 */
	bottom = SuffFindThem(srcs, slst);

	if (bottom == NULL) {
	    /*
	     * No known transformations -- use the first suffix found
	     * for setting the local variables.
	     */
	    if (!Lst_IsEmpty(targs)) {
		targ = LstNode_Datum(Lst_First(targs));
	    } else {
		targ = NULL;
	    }
	} else {
	    /*
	     * Work up the transformation path to find the suffix of the
	     * target to which the transformation was made.
	     */
	    for (targ = bottom; targ->parent != NULL; targ = targ->parent)
		continue;
	}
    }

    Var_Set(TARGET, gn->path ? gn->path : gn->name, gn);

    pref = (targ != NULL) ? targ->pref : gn->name;
    Var_Set(PREFIX, pref, gn);

    /*
     * Now we've got the important local variables set, expand any sources
     * that still contain variables or wildcards in their names.
     */
    for (ln = Lst_First(gn->children); ln != NULL; ln = nln) {
	nln = LstNode_Next(ln);
	SuffExpandChildren(ln, gn);
    }

    if (targ == NULL) {
	SUFF_DEBUG1("\tNo valid suffix on %s\n", gn->name);

sfnd_abort:
	/*
	 * Deal with finding the thing on the default search path. We
	 * always do that, not only if the node is only a source (not
	 * on the lhs of a dependency operator or [XXX] it has neither
	 * children or commands) as the old pmake did.
	 */
	if ((gn->type & (OP_PHONY|OP_NOPATH)) == 0) {
	    free(gn->path);
	    gn->path = Dir_FindFile(gn->name,
				    (targ == NULL ? dirSearchPath :
				     targ->suff->searchPath));
	    if (gn->path != NULL) {
		char *ptr;
		Var_Set(TARGET, gn->path, gn);

		if (targ != NULL) {
		    /*
		     * Suffix known for the thing -- trim the suffix off
		     * the path to form the proper .PREFIX variable.
		     */
		    int     savep = strlen(gn->path) - targ->suff->nameLen;
		    char    savec;

		    if (gn->suffix)
			gn->suffix->refCount--;
		    gn->suffix = targ->suff;
		    gn->suffix->refCount++;

		    savec = gn->path[savep];
		    gn->path[savep] = '\0';

		    if ((ptr = strrchr(gn->path, '/')) != NULL)
			ptr++;
		    else
			ptr = gn->path;

		    Var_Set(PREFIX, ptr, gn);

		    gn->path[savep] = savec;
		} else {
		    /*
		     * The .PREFIX gets the full path if the target has
		     * no known suffix.
		     */
		    if (gn->suffix)
			gn->suffix->refCount--;
		    gn->suffix = NULL;

		    if ((ptr = strrchr(gn->path, '/')) != NULL)
			ptr++;
		    else
			ptr = gn->path;

		    Var_Set(PREFIX, ptr, gn);
		}
	    }
	}

	goto sfnd_return;
    }

    /*
     * If the suffix indicates that the target is a library, mark that in
     * the node's type field.
     */
    if (targ->suff->flags & SUFF_LIBRARY) {
	gn->type |= OP_LIB;
    }

    /*
     * Check for overriding transformation rule implied by sources
     */
    if (!Lst_IsEmpty(gn->children)) {
	src = SuffFindCmds(targ, slst);

	if (src != NULL) {
	    /*
	     * Free up all the Src structures in the transformation path
	     * up to, but not including, the parent node.
	     */
	    while (bottom && bottom->parent != NULL) {
		if (Lst_FindDatum(slst, bottom) == NULL) {
		    Lst_Append(slst, bottom);
		}
		bottom = bottom->parent;
	    }
	    bottom = src;
	}
    }

    if (bottom == NULL) {
	/*
	 * No idea from where it can come -- return now.
	 */
	goto sfnd_abort;
    }

    /*
     * We now have a list of Src structures headed by 'bottom' and linked via
     * their 'parent' pointers. What we do next is create links between
     * source and target nodes (which may or may not have been created)
     * and set the necessary local variables in each target. The
     * commands for each target are set from the commands of the
     * transformation rule used to get from the src suffix to the targ
     * suffix. Note that this causes the commands list of the original
     * node, gn, to be replaced by the commands of the final
     * transformation rule. Also, the unmade field of gn is incremented.
     * Etc.
     */
    if (bottom->node == NULL) {
	bottom->node = Targ_FindNode(bottom->file, TARG_CREATE);
    }

    for (src = bottom; src->parent != NULL; src = src->parent) {
	targ = src->parent;

	if (src->node->suffix)
	    src->node->suffix->refCount--;
	src->node->suffix = src->suff;
	src->node->suffix->refCount++;

	if (targ->node == NULL) {
	    targ->node = Targ_FindNode(targ->file, TARG_CREATE);
	}

	SuffApplyTransform(targ->node, src->node,
			   targ->suff, src->suff);

	if (targ->node != gn) {
	    /*
	     * Finish off the dependency-search process for any nodes
	     * between bottom and gn (no point in questing around the
	     * filesystem for their implicit source when it's already
	     * known). Note that the node can't have any sources that
	     * need expanding, since SuffFindThem will stop on an existing
	     * node, so all we need to do is set the standard and System V
	     * variables.
	     */
	    targ->node->type |= OP_DEPS_FOUND;

	    Var_Set(PREFIX, targ->pref, targ->node);

	    Var_Set(TARGET, targ->node->name, targ->node);
	}
    }

    if (gn->suffix)
	gn->suffix->refCount--;
    gn->suffix = src->suff;
    gn->suffix->refCount++;

    /*
     * Nuke the transformation path and the Src structures left over in the
     * two lists.
     */
sfnd_return:
    if (bottom)
	if (Lst_FindDatum(slst, bottom) == NULL)
	    Lst_Append(slst, bottom);

    while (SuffRemoveSrc(srcs) || SuffRemoveSrc(targs))
	continue;

    Lst_MoveAll(slst, srcs);
    Lst_MoveAll(slst, targs);
}


/* Find implicit sources for the target described by the graph node.
 *
 * Nodes are added to the graph below the passed-in node. The nodes are
 * marked to have their IMPSRC variable filled in. The PREFIX variable is set
 * for the given node and all its implied children.
 *
 * The path found by this target is the shortest path in the transformation
 * graph, which may pass through non-existent targets, to an existing target.
 * The search continues on all paths from the root suffix until a file is
 * found. I.e. if there's a path .o -> .c -> .l -> .l,v from the root and the
 * .l,v file exists but the .c and .l files don't, the search will branch out
 * in all directions from .o and again from all the nodes on the next level
 * until the .l,v node is encountered.
 */
void
Suff_FindDeps(GNode *gn)
{

    SuffFindDeps(gn, srclist);
    while (SuffRemoveSrc(srclist))
	continue;
}

static void
SuffFindDeps(GNode *gn, Lst slst)
{
    if (gn->type & OP_DEPS_FOUND)
	return;
    gn->type |= OP_DEPS_FOUND;

    /*
     * Make sure we have these set, may get revised below.
     */
    Var_Set(TARGET, gn->path ? gn->path : gn->name, gn);
    Var_Set(PREFIX, gn->name, gn);

    SUFF_DEBUG1("SuffFindDeps (%s)\n", gn->name);

    if (gn->type & OP_ARCHV) {
	SuffFindArchiveDeps(gn, slst);
    } else if (gn->type & OP_LIB) {
	/*
	 * If the node is a library, it is the arch module's job to find it
	 * and set the TARGET variable accordingly. We merely provide the
	 * search path, assuming all libraries end in ".a" (if the suffix
	 * hasn't been defined, there's nothing we can do for it, so we just
	 * set the TARGET variable to the node's name in order to give it a
	 * value).
	 */
	LstNode	ln;
	Suff	*s;

	ln = Lst_Find(sufflist, SuffSuffHasName, LIBSUFF);
	if (gn->suffix)
	    gn->suffix->refCount--;
	if (ln != NULL) {
	    gn->suffix = s = LstNode_Datum(ln);
	    gn->suffix->refCount++;
	    Arch_FindLib(gn, s->searchPath);
	} else {
	    gn->suffix = NULL;
	    Var_Set(TARGET, gn->name, gn);
	}
	/*
	 * Because a library (-lfoo) target doesn't follow the standard
	 * filesystem conventions, we don't set the regular variables for
	 * the thing. .PREFIX is simply made empty...
	 */
	Var_Set(PREFIX, "", gn);
    } else {
	SuffFindNormalDeps(gn, slst);
    }
}

/* Define which suffix is the null suffix.
 *
 * Need to handle the changing of the null suffix gracefully so the old
 * transformation rules don't just go away.
 *
 * Input:
 *	name		Name of null suffix
 */
void
Suff_SetNull(char *name)
{
    Suff    *s;
    LstNode ln;

    ln = Lst_Find(sufflist, SuffSuffHasName, name);
    if (ln != NULL) {
	s = LstNode_Datum(ln);
	if (suffNull != NULL) {
	    suffNull->flags &= ~SUFF_NULL;
	}
	s->flags |= SUFF_NULL;
	/*
	 * XXX: Here's where the transformation mangling would take place
	 */
	suffNull = s;
    } else {
	Parse_Error(PARSE_WARNING, "Desired null suffix %s not defined.",
		     name);
    }
}

/* Initialize the suffixes module. */
void
Suff_Init(void)
{
#ifdef CLEANUP
    suffClean = Lst_Init();
    sufflist = Lst_Init();
#endif
    srclist = Lst_Init();
    transforms = Lst_Init();

    /*
     * Create null suffix for single-suffix rules (POSIX). The thing doesn't
     * actually go on the suffix list or everyone will think that's its
     * suffix.
     */
    Suff_ClearSuffixes();
}


/* Clean up the suffixes module. */
void
Suff_End(void)
{
#ifdef CLEANUP
    Lst_Destroy(sufflist, SuffFree);
    Lst_Destroy(suffClean, SuffFree);
    if (suffNull)
	SuffFree(suffNull);
    Lst_Free(srclist);
    Lst_Free(transforms);
#endif
}


/********************* DEBUGGING FUNCTIONS **********************/

static int SuffPrintName(void *s, void *dummy MAKE_ATTR_UNUSED)
{

    fprintf(debug_file, "%s ", ((Suff *)s)->name);
    return 0;
}

static int
SuffPrintSuff(void *sp, void *dummy MAKE_ATTR_UNUSED)
{
    Suff    *s = (Suff *)sp;

    fprintf(debug_file, "# `%s' [%d] ", s->name, s->refCount);

    if (s->flags != 0) {
	char flags_buf[SuffFlags_ToStringSize];

	fprintf(debug_file, " (%s)",
		Enum_FlagsToString(flags_buf, sizeof flags_buf,
				   s->flags, SuffFlags_ToStringSpecs));
    }
    fputc('\n', debug_file);
    fprintf(debug_file, "#\tTo: ");
    Lst_ForEach(s->parents, SuffPrintName, NULL);
    fputc('\n', debug_file);
    fprintf(debug_file, "#\tFrom: ");
    Lst_ForEach(s->children, SuffPrintName, NULL);
    fputc('\n', debug_file);
    fprintf(debug_file, "#\tSearch Path: ");
    Dir_PrintPath(s->searchPath);
    fputc('\n', debug_file);
    return 0;
}

static int
SuffPrintTrans(void *tp, void *dummy MAKE_ATTR_UNUSED)
{
    GNode   *t = (GNode *)tp;

    fprintf(debug_file, "%-16s: ", t->name);
    Targ_PrintType(t->type);
    fputc('\n', debug_file);
    Lst_ForEach(t->commands, Targ_PrintCmd, NULL);
    fputc('\n', debug_file);
    return 0;
}

void
Suff_PrintAll(void)
{
    fprintf(debug_file, "#*** Suffixes:\n");
    Lst_ForEach(sufflist, SuffPrintSuff, NULL);

    fprintf(debug_file, "#*** Transformations:\n");
    Lst_ForEach(transforms, SuffPrintTrans, NULL);
}
