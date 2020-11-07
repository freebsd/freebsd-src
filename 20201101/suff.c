/*	$NetBSD: suff.c,v 1.230 2020/10/31 11:54:33 rillig Exp $	*/

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

/*-
 * suff.c --
 *	Functions to maintain suffix lists and find implicit dependents
 *	using suffix transformation rules
 *
 * Interface:
 *	Suff_Init	Initialize all things to do with suffixes.
 *
 *	Suff_End	Clean up the module
 *
 *	Suff_DoPaths	This function is used to make life easier
 *			when searching for a file according to its
 *			suffix. It takes the global search path,
 *			as defined using the .PATH: target, and appends
 *			its directories to the path of each of the
 *			defined suffixes, as specified using
 *			.PATH<suffix>: targets. In addition, all
 *			directories given for suffixes labeled as
 *			include files or libraries, using the .INCLUDES
 *			or .LIBS targets, are played with using
 *			Dir_MakeFlags to create the .INCLUDES and
 *			.LIBS global variables.
 *
 *	Suff_ClearSuffixes
 *			Clear out all the suffixes and defined
 *			transformations.
 *
 *	Suff_IsTransform
 *			Return TRUE if the passed string is the lhs
 *			of a transformation rule.
 *
 *	Suff_AddSuffix	Add the passed string as another known suffix.
 *
 *	Suff_GetPath	Return the search path for the given suffix.
 *
 *	Suff_AddInclude
 *			Mark the given suffix as denoting an include file.
 *
 *	Suff_AddLib	Mark the given suffix as denoting a library.
 *
 *	Suff_AddTransform
 *			Add another transformation to the suffix
 *			graph. Returns  GNode suitable for framing, I
 *			mean, tacking commands, attributes, etc. on.
 *
 *	Suff_SetNull	Define the suffix to consider the suffix of
 *			any file that doesn't have a known one.
 *
 *	Suff_FindDeps	Find implicit sources for and the location of
 *			a target based on its suffix. Returns the
 *			bottom-most node added to the graph or NULL
 *			if the target had no implicit sources.
 *
 *	Suff_FindPath	Return the appropriate path to search in order to
 *			find the node.
 */

#include "make.h"
#include "dir.h"

/*	"@(#)suff.c	8.4 (Berkeley) 3/21/94"	*/
MAKE_RCSID("$NetBSD: suff.c,v 1.230 2020/10/31 11:54:33 rillig Exp $");

#define SUFF_DEBUG0(text) DEBUG0(SUFF, text)
#define SUFF_DEBUG1(fmt, arg1) DEBUG1(SUFF, fmt, arg1)
#define SUFF_DEBUG2(fmt, arg1, arg2) DEBUG2(SUFF, fmt, arg1, arg2)
#define SUFF_DEBUG3(fmt, arg1, arg2, arg3) DEBUG3(SUFF, fmt, arg1, arg2, arg3)
#define SUFF_DEBUG4(fmt, arg1, arg2, arg3, arg4) \
	DEBUG4(SUFF, fmt, arg1, arg2, arg3, arg4)

typedef List SuffList;
typedef ListNode SuffListNode;

typedef List SrcList;
typedef ListNode SrcListNode;

static SuffList *sufflist;	/* List of suffixes */
#ifdef CLEANUP
static SuffList *suffClean;	/* List of suffixes to be cleaned */
#endif
static SrcList *srclist;	/* List of sources */
static GNodeList *transforms;	/* List of transformation rules */

static int        sNum = 0;	/* Counter for assigning suffix numbers */

typedef enum SuffFlags {
    SUFF_INCLUDE	= 0x01,	/* One which is #include'd */
    SUFF_LIBRARY	= 0x02,	/* One which contains a library */
    SUFF_NULL		= 0x04	/* The empty suffix */
    /* XXX: Why is SUFF_NULL needed? Wouldn't nameLen == 0 mean the same? */
} SuffFlags;

ENUM_FLAGS_RTTI_3(SuffFlags,
		  SUFF_INCLUDE, SUFF_LIBRARY, SUFF_NULL);

typedef List SuffListList;

/*
 * Structure describing an individual suffix.
 */
typedef struct Suff {
    char         *name;		/* The suffix itself, such as ".c" */
    size_t	 nameLen;	/* Length of the name, to avoid strlen calls */
    SuffFlags	 flags;		/* Type of suffix */
    SearchPath	 *searchPath;	/* The path along which files of this suffix
				 * may be found */
    int          sNum;		/* The suffix number */
    int		 refCount;	/* Reference count of list membership
				 * and several other places */
    SuffList	 *parents;	/* Suffixes we have a transformation to */
    SuffList	 *children;	/* Suffixes we have a transformation from */
    SuffListList *ref;		/* Lists in which this suffix is referenced */
} Suff;

/*
 * Structure used in the search for implied sources.
 */
typedef struct Src {
    char *file;			/* The file to look for */
    char *pref;			/* Prefix from which file was formed */
    Suff *suff;			/* The suffix on the file */
    struct Src *parent;		/* The Src for which this is a source */
    GNode *node;		/* The node describing the file */
    int children;		/* Count of existing children (so we don't free
				 * this thing too early or never nuke it) */
#ifdef DEBUG_SRC
    SrcList *childrenList;
#endif
} Src;

static Suff *suffNull;		/* The NULL suffix for this run */
static Suff *emptySuff;		/* The empty suffix required for POSIX
				 * single-suffix transformation rules */


static void SuffFindDeps(GNode *, SrcList *);
static void SuffExpandWildcards(GNodeListNode *, GNode *);

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

/* See if suff is a suffix of str.
 *
 * Input:
 *	s		possible suffix
 *	nameLen		length of the string to examine
 *	nameEnd		end of the string to examine
 *
 * Results:
 *	NULL if it ain't, pointer to the start of suffix in str if it is.
 */
static const char *
SuffSuffGetSuffix(const Suff *s, size_t nameLen, const char *nameEnd)
{
    const char *p1;		/* Pointer into suffix name */
    const char *p2;		/* Pointer into string being examined */

    if (nameLen < s->nameLen)
	return NULL;		/* this string is shorter than the suffix */

    p1 = s->name + s->nameLen;
    p2 = nameEnd;

    while (p1 >= s->name && *p1 == *p2) {
	p1--;
	p2--;
    }

    /* XXX: s->name - 1 invokes undefined behavior */
    return p1 == s->name - 1 ? p2 + 1 : NULL;
}

static Boolean
SuffSuffIsSuffix(const Suff *suff, size_t nameLen, const char *nameEnd)
{
    return SuffSuffGetSuffix(suff, nameLen, nameEnd) != NULL;
}

static Suff *
FindSuffByNameLen(const char *name, size_t nameLen)
{
    SuffListNode *ln;

    for (ln = sufflist->first; ln != NULL; ln = ln->next) {
	Suff *suff = ln->datum;
	if (suff->nameLen == nameLen && memcmp(suff->name, name, nameLen) == 0)
	    return suff;
    }
    return NULL;
}

static Suff *
FindSuffByName(const char *name)
{
    return FindSuffByNameLen(name, strlen(name));
}

static GNode *
FindTransformByName(const char *name)
{
    GNodeListNode *ln;
    for (ln = transforms->first; ln != NULL; ln = ln->next) {
	GNode *gn = ln->datum;
	if (strcmp(gn->name, name) == 0)
	    return gn;
    }
    return NULL;
}

	    /*********** Maintenance Functions ************/

static void
SuffUnRef(SuffList *list, Suff *suff)
{
    SuffListNode *ln = Lst_FindDatum(list, suff);
    if (ln != NULL) {
	Lst_Remove(list, ln);
	suff->refCount--;
    }
}

/* Free up all memory associated with the given suffix structure. */
static void
SuffFree(void *sp)
{
    Suff *s = sp;

    if (s == suffNull)
	suffNull = NULL;

    if (s == emptySuff)
	emptySuff = NULL;

#if 0
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
SuffRemove(SuffList *list, Suff *suff)
{
    SuffUnRef(list, suff);
    if (suff->refCount == 0) {
	SuffUnRef(sufflist, suff);
	SuffFree(suff);
    }
}

/* Insert the suffix into the list, keeping the list ordered by suffix
 * numbers. */
static void
SuffInsert(SuffList *list, Suff *suff)
{
    SuffListNode *ln;
    Suff *listSuff = NULL;

    for (ln = list->first; ln != NULL; ln = ln->next) {
	listSuff = ln->datum;
	if (listSuff->sNum >= suff->sNum)
	    break;
    }

    if (ln == NULL) {
	SUFF_DEBUG2("inserting \"%s\" (%d) at end of list\n",
		    suff->name, suff->sNum);
	Lst_Append(list, suff);
	suff->refCount++;
	Lst_Append(suff->ref, list);
    } else if (listSuff->sNum != suff->sNum) {
	SUFF_DEBUG4("inserting \"%s\" (%d) before \"%s\" (%d)\n",
		    suff->name, suff->sNum, listSuff->name, listSuff->sNum);
	Lst_InsertBefore(list, ln, suff);
	suff->refCount++;
	Lst_Append(suff->ref, list);
    } else {
	SUFF_DEBUG2("\"%s\" (%d) is already there\n", suff->name, suff->sNum);
    }
}

static Suff *
SuffNew(const char *name)
{
    Suff *s = bmake_malloc(sizeof(Suff));

    s->name = bmake_strdup(name);
    s->nameLen = strlen(s->name);
    s->searchPath = Lst_New();
    s->children = Lst_New();
    s->parents = Lst_New();
    s->ref = Lst_New();
    s->sNum = sNum++;
    s->flags = 0;
    s->refCount = 1;

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
    sufflist = Lst_New();
    sNum = 0;
    if (suffNull)
	SuffFree(suffNull);
    emptySuff = suffNull = SuffNew("");

    Dir_Concat(suffNull->searchPath, dirSearchPath);
    suffNull->flags = SUFF_NULL;
}

/* Parse a transformation string such as ".c.o" to find its two component
 * suffixes (the source ".c" and the target ".o").  If there are no such
 * suffixes, try a single-suffix transformation as well.
 *
 * Return TRUE if the string is a valid transformation.
 */
static Boolean
SuffParseTransform(const char *str, Suff **out_src, Suff **out_targ)
{
    SuffListNode *ln;
    Suff *singleSrc = NULL;

    /*
     * Loop looking first for a suffix that matches the start of the
     * string and then for one that exactly matches the rest of it. If
     * we can find two that meet these criteria, we've successfully
     * parsed the string.
     */
    for (ln = sufflist->first; ln != NULL; ln = ln->next) {
	Suff *src = ln->datum;

	if (SuffStrIsPrefix(src->name, str) == NULL)
	    continue;

	if (str[src->nameLen] == '\0') {
	    singleSrc = src;
	} else {
	    Suff *targ = FindSuffByName(str + src->nameLen);
	    if (targ != NULL) {
		*out_src = src;
		*out_targ = targ;
		return TRUE;
	    }
	}
    }

    if (singleSrc != NULL) {
	/*
	 * Not so fast Mr. Smith! There was a suffix that encompassed
	 * the entire string, so we assume it was a transformation
	 * to the null suffix (thank you POSIX). We still prefer to
	 * find a double rule over a singleton, hence we leave this
	 * check until the end.
	 *
	 * XXX: Use emptySuff over suffNull?
	 */
	*out_src = singleSrc;
	*out_targ = suffNull;
	return TRUE;
    }
    return FALSE;
}

/* Return TRUE if the given string is a transformation rule, that is, a
 * concatenation of two known suffixes. */
Boolean
Suff_IsTransform(const char *str)
{
    Suff *src, *targ;

    return SuffParseTransform(str, &src, &targ);
}

/* Add the transformation rule to the list of rules and place the
 * transformation itself in the graph.
 *
 * The transformation is linked to the two suffixes mentioned in the name.
 *
 * Input:
 *	name		must have the form ".from.to" or just ".from"
 *
 * Results:
 *	The created or existing transformation node in the transforms list
 */
GNode *
Suff_AddTransform(const char *name)
{
    GNode         *gn;		/* GNode of transformation rule */
    Suff          *s,		/* source suffix */
		  *t;		/* target suffix */
    Boolean ok;

    gn = FindTransformByName(name);
    if (gn == NULL) {
	/*
	 * Make a new graph node for the transformation. It will be filled in
	 * by the Parse module.
	 */
	gn = Targ_NewGN(name);
	Lst_Append(transforms, gn);
    } else {
	/*
	 * New specification for transformation rule. Just nuke the old list
	 * of commands so they can be filled in again... We don't actually
	 * free the commands themselves, because a given command can be
	 * attached to several different transformations.
	 */
	Lst_Free(gn->commands);
	Lst_Free(gn->children);
	gn->commands = Lst_New();
	gn->children = Lst_New();
    }

    gn->type = OP_TRANSFORM;

    ok = SuffParseTransform(name, &s, &t);
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
 *
 * If the node has no commands or children, the children and parents lists
 * of the affected suffixes are altered.
 *
 * Input:
 *	gn		Node for transformation
 */
void
Suff_EndTransform(GNode *gn)
{
    if ((gn->type & OP_DOUBLEDEP) && !Lst_IsEmpty(gn->cohorts))
	gn = gn->cohorts->last->datum;
    if ((gn->type & OP_TRANSFORM) && Lst_IsEmpty(gn->commands) &&
	Lst_IsEmpty(gn->children))
    {
	Suff	*s, *t;

	/*
	 * SuffParseTransform() may fail for special rules which are not
	 * actual transformation rules. (e.g. .DEFAULT)
	 */
	if (SuffParseTransform(gn->name, &s, &t)) {
	    SuffList *p;

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
}

/* Called from Suff_AddSuffix to search through the list of
 * existing transformation rules and rebuild the transformation graph when
 * it has been destroyed by Suff_ClearSuffixes. If the given rule is a
 * transformation involving this suffix and another, existing suffix, the
 * proper relationship is established between the two.
 *
 * The appropriate links will be made between this suffix and others if
 * transformation rules exist for it.
 *
 * Input:
 *	transform	Transformation to test
 *	suff		Suffix to rebuild
 */
static void
SuffRebuildGraph(GNode *transform, Suff *suff)
{
    const char *name = transform->name;
    size_t nameLen = strlen(name);
    const char *toName;

    /*
     * First see if it is a transformation from this suffix.
     */
    toName = SuffStrIsPrefix(suff->name, name);
    if (toName != NULL) {
	Suff *to = FindSuffByName(toName);
	if (to != NULL) {
	    /* Link in and return, since it can't be anything else. */
	    SuffInsert(to->children, suff);
	    SuffInsert(suff->parents, to);
	    return;
	}
    }

    /*
     * Not from, maybe to?
     */
    toName = SuffSuffGetSuffix(suff, nameLen, name + nameLen);
    if (toName != NULL) {
	Suff *from = FindSuffByNameLen(name, (size_t)(toName - name));

	if (from != NULL) {
	    /* establish the proper relationship */
	    SuffInsert(suff->children, from);
	    SuffInsert(from->parents, suff);
	}
    }
}

/* During Suff_AddSuffix, search through the list of existing targets and find
 * if any of the existing targets can be turned into a transformation rule.
 *
 * If such a target is found and the target is the current main target, the
 * main target is set to NULL and the next target examined (if that exists)
 * becomes the main target.
 *
 * Results:
 *	TRUE iff a new main target has been selected.
 */
static Boolean
SuffScanTargets(GNode *target, GNode **inout_main, Suff *gs_s, Boolean *gs_r)
{
    Suff *s, *t;
    char *ptr;

    if (*inout_main == NULL && *gs_r && !(target->type & OP_NOTARGET)) {
	*inout_main = target;
	Targ_SetMain(target);
	return TRUE;
    }

    if (target->type == OP_TRANSFORM)
	return FALSE;

    if ((ptr = strstr(target->name, gs_s->name)) == NULL ||
	ptr == target->name)
	return FALSE;

    if (SuffParseTransform(target->name, &s, &t)) {
	if (*inout_main == target) {
	    *gs_r = TRUE;
	    *inout_main = NULL;
	    Targ_SetMain(NULL);
	}
	Lst_Free(target->children);
	target->children = Lst_New();
	target->type = OP_TRANSFORM;
	/*
	 * link the two together in the proper relationship and order
	 */
	SUFF_DEBUG2("defining transformation from `%s' to `%s'\n",
		    s->name, t->name);
	SuffInsert(t->children, s);
	SuffInsert(s->parents, t);
    }
    return FALSE;
}

/* Look at all existing targets to see if adding this suffix will make one
 * of the current targets mutate into a suffix rule.
 *
 * This is ugly, but other makes treat all targets that start with a '.' as
 * suffix rules. */
static void
UpdateTargets(GNode **inout_main, Suff *s)
{
    Boolean r = FALSE;
    GNodeListNode *ln;
    for (ln = Targ_List()->first; ln != NULL; ln = ln->next) {
	GNode *gn = ln->datum;
	if (SuffScanTargets(gn, inout_main, s, &r))
	    break;
    }
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
Suff_AddSuffix(const char *name, GNode **inout_main)
{
    GNodeListNode *ln;

    Suff *s = FindSuffByName(name);
    if (s != NULL)
	return;

    s = SuffNew(name);
    Lst_Append(sufflist, s);

    UpdateTargets(inout_main, s);

    /*
     * Look for any existing transformations from or to this suffix.
     * XXX: Only do this after a Suff_ClearSuffixes?
     */
    for (ln = transforms->first; ln != NULL; ln = ln->next)
	SuffRebuildGraph(ln->datum, s);
}

/* Return the search path for the given suffix, or NULL. */
SearchPath *
Suff_GetPath(const char *sname)
{
    Suff *s = FindSuffByName(sname);
    return s != NULL ? s->searchPath : NULL;
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
    SuffListNode *ln;
    char *ptr;
    SearchPath *inIncludes; /* Cumulative .INCLUDES path */
    SearchPath *inLibs;	    /* Cumulative .LIBS path */

    inIncludes = Lst_New();
    inLibs = Lst_New();

    for (ln = sufflist->first; ln != NULL; ln = ln->next) {
	Suff *s = ln->datum;
	if (!Lst_IsEmpty(s->searchPath)) {
#ifdef INCLUDES
	    if (s->flags & SUFF_INCLUDE) {
		Dir_Concat(inIncludes, s->searchPath);
	    }
#endif
#ifdef LIBRARIES
	    if (s->flags & SUFF_LIBRARY) {
		Dir_Concat(inLibs, s->searchPath);
	    }
#endif
	    Dir_Concat(s->searchPath, dirSearchPath);
	} else {
	    Lst_Destroy(s->searchPath, Dir_Destroy);
	    s->searchPath = Dir_CopyDirSearchPath();
	}
    }

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
Suff_AddInclude(const char *sname)
{
    Suff *suff = FindSuffByName(sname);
    if (suff != NULL)
	suff->flags |= SUFF_INCLUDE;
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
    Suff *suff = FindSuffByName(sname);
    if (suff != NULL)
	suff->flags |= SUFF_LIBRARY;
}

	  /********** Implicit Source Search Functions *********/

#ifdef DEBUG_SRC
static void
SrcList_PrintAddrs(SrcList *srcList)
{
    SrcListNode *ln;
    for (ln = srcList->first; ln != NULL; ln = ln->next)
	debug_printf(" %p", ln->datum);
    debug_printf("\n");
}
#endif

static Src *
SrcNew(char *name, char *pref, Suff *suff, Src *parent, GNode *gn)
{
    Src *src = bmake_malloc(sizeof *src);

    src->file = name;
    src->pref = pref;
    src->suff = suff;
    src->parent = parent;
    src->node = gn;
    src->children = 0;
#ifdef DEBUG_SRC
    src->childrenList = Lst_New();
#endif

    return src;
}

static void
SuffAddSrc(Suff *suff, SrcList *srcList, Src *targ, char *srcName,
	   const char *debug_tag)
{
    Src *s2 = SrcNew(srcName, targ->pref, suff, targ, NULL);
    suff->refCount++;
    targ->children++;
    Lst_Append(srcList, s2);
#ifdef DEBUG_SRC
    Lst_Append(targ->childrenList, s2);
    debug_printf("%s add suff %p src %p to list %p:",
		 debug_tag, targ, s2, srcList);
    SrcList_PrintAddrs(srcList);
#endif
}

/* Add a suffix as a Src structure to the given list with its parent
 * being the given Src structure. If the suffix is the null suffix,
 * the prefix is used unaltered as the file name in the Src structure.
 *
 * Input:
 *	suff		suffix for which to create a Src structure
 *	srcList		list for the new Src
 *	targ		parent for the new Src
 */
static void
SuffAddSources(Suff *suff, SrcList *srcList, Src *targ)
{
    if ((suff->flags & SUFF_NULL) && suff->name[0] != '\0') {
	/*
	 * If the suffix has been marked as the NULL suffix, also create a Src
	 * structure for a file with no suffix attached. Two birds, and all
	 * that...
	 */
	SuffAddSrc(suff, srcList, targ, bmake_strdup(targ->pref), "1");
    }
    SuffAddSrc(suff, srcList, targ, str_concat2(targ->pref, suff->name), "2");
}

/* Add all the children of targ as Src structures to the given list.
 *
 * Input:
 *	l		list to which to add the new level
 *	targ		Src structure to use as the parent
 */
static void
SuffAddLevel(SrcList *l, Src *targ)
{
    SrcListNode *ln;
    for (ln = targ->suff->children->first; ln != NULL; ln = ln->next) {
	Suff *childSuff = ln->datum;
	SuffAddSources(childSuff, l, targ);
    }
}

/* Free the first Src in the list that is not referenced anymore.
 * Return whether a Src was removed. */
static Boolean
SuffRemoveSrc(SrcList *l)
{
    SrcListNode *ln;

#ifdef DEBUG_SRC
    debug_printf("cleaning list %p:", l);
    SrcList_PrintAddrs(l);
#endif

    for (ln = l->first; ln != NULL; ln = ln->next) {
	Src *s = ln->datum;

	if (s->children == 0) {
	    free(s->file);
	    if (s->parent == NULL)
		free(s->pref);
	    else {
#ifdef DEBUG_SRC
		SrcListNode *ln2 = Lst_FindDatum(s->parent->childrenList, s);
		if (ln2 != NULL)
		    Lst_Remove(s->parent->childrenList, ln2);
#endif
		s->parent->children--;
	    }
#ifdef DEBUG_SRC
	    debug_printf("free: list %p src %p children %d\n",
			 l, s, s->children);
	    Lst_Free(s->childrenList);
#endif
	    Lst_Remove(l, ln);
	    free(s);
	    return TRUE;
	}
#ifdef DEBUG_SRC
	else {
	    debug_printf("keep: list %p src %p children %d:",
			 l, s, s->children);
	    SrcList_PrintAddrs(s->childrenList);
	}
#endif
    }

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
SuffFindThem(SrcList *srcs, SrcList *slst)
{
    Src *retsrc = NULL;

    while (!Lst_IsEmpty(srcs)) {
	Src *src = Lst_Dequeue(srcs);

	SUFF_DEBUG1("\ttrying %s...", src->file);

	/*
	 * A file is considered to exist if either a node exists in the
	 * graph for it or the file actually exists.
	 */
	if (Targ_FindNode(src->file) != NULL) {
#ifdef DEBUG_SRC
	    debug_printf("remove from list %p src %p\n", srcs, src);
#endif
	    retsrc = src;
	    break;
	}

	{
	    char *file = Dir_FindFile(src->file, src->suff->searchPath);
	    if (file != NULL) {
		retsrc = src;
#ifdef DEBUG_SRC
		debug_printf("remove from list %p src %p\n", srcs, src);
#endif
		free(file);
		break;
	    }
	}

	SUFF_DEBUG0("not there\n");

	SuffAddLevel(srcs, src);
	Lst_Append(slst, src);
    }

    if (retsrc) {
	SUFF_DEBUG0("got it\n");
    }
    return retsrc;
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
SuffFindCmds(Src *targ, SrcList *slst)
{
    GNodeListNode *gln;
    GNode *tgn;			/* Target GNode */
    GNode *sgn;			/* Source GNode */
    size_t prefLen;		/* The length of the defined prefix */
    Suff *suff;			/* Suffix on matching beastie */
    Src *ret;			/* Return value */
    char *cp;

    tgn = targ->node;
    prefLen = strlen(targ->pref);

    for (gln = tgn->children->first; gln != NULL; gln = gln->next) {
	sgn = gln->datum;

	if (sgn->type & OP_OPTIONAL && Lst_IsEmpty(tgn->commands)) {
	    /*
	     * We haven't looked to see if .OPTIONAL files exist yet, so
	     * don't use one as the implicit source.
	     * This allows us to use .OPTIONAL in .depend files so make won't
	     * complain "don't know how to make xxx.h' when a dependent file
	     * has been moved/deleted.
	     */
	    continue;
	}

	cp = strrchr(sgn->name, '/');
	if (cp == NULL) {
	    cp = sgn->name;
	} else {
	    cp++;
	}
	if (strncmp(cp, targ->pref, prefLen) != 0)
	    continue;
	/*
	 * The node matches the prefix ok, see if it has a known
	 * suffix.
	 */
	suff = FindSuffByName(cp + prefLen);
	if (suff == NULL)
	    continue;

	/*
	 * It even has a known suffix, see if there's a transformation
	 * defined between the node's suffix and the target's suffix.
	 *
	 * XXX: Handle multi-stage transformations here, too.
	 */

	/* XXX: Can targ->suff be NULL here? */
	if (targ->suff != NULL &&
	    Lst_FindDatum(suff->parents, targ->suff) != NULL)
	    break;
    }

    if (gln == NULL)
	return NULL;

    /*
     * Hot Damn! Create a new Src structure to describe
     * this transformation (making sure to duplicate the
     * source node's name so Suff_FindDeps can free it
     * again (ick)), and return the new structure.
     */
    ret = SrcNew(bmake_strdup(sgn->name), targ->pref, suff, targ, sgn);
    suff->refCount++;
    targ->children++;
#ifdef DEBUG_SRC
    debug_printf("3 add targ %p ret %p\n", targ, ret);
    Lst_Append(targ->childrenList, ret);
#endif
    Lst_Append(slst, ret);
    SUFF_DEBUG1("\tusing existing source %s\n", sgn->name);
    return ret;
}

/* Expand the names of any children of a given node that contain variable
 * expressions or file wildcards into actual targets.
 *
 * The expanded node is removed from the parent's list of children, and the
 * parent's unmade counter is decremented, but other nodes may be added.
 *
 * Input:
 *	cln		Child to examine
 *	pgn		Parent node being processed
 */
static void
SuffExpandChildren(GNodeListNode *cln, GNode *pgn)
{
    GNode *cgn = cln->datum;
    GNode *gn;			/* New source 8) */
    char *cp;			/* Expanded value */

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
    (void)Var_Subst(cgn->name, pgn, VARE_UNDEFERR|VARE_WANTRES, &cp);
    /* TODO: handle errors */

    {
	GNodeList *members = Lst_New();

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
	    cp = start;
	    while (*cp != '\0') {
		if (*cp == ' ' || *cp == '\t') {
		    /*
		     * White-space -- terminate element, find the node,
		     * add it, skip any further spaces.
		     */
		    *cp++ = '\0';
		    gn = Targ_GetNode(start);
		    Lst_Append(members, gn);
		    while (*cp == ' ' || *cp == '\t') {
			cp++;
		    }
		    start = cp;		/* Continue at the next non-space. */
		} else if (*cp == '$') {
		    /*
		     * Start of a variable spec -- contact variable module
		     * to find the end so we can skip over it.
		     */
		    const char *nested_p = cp;
		    const char	*junk;
		    void	*freeIt;

		    /* XXX: Why VARE_WANTRES when the result is not used? */
		    (void)Var_Parse(&nested_p, pgn,
				    VARE_UNDEFERR|VARE_WANTRES,
				    &junk, &freeIt);
		    /* TODO: handle errors */
		    if (junk == var_Error) {
			Parse_Error(PARSE_FATAL,
				    "Malformed variable expression at \"%s\"",
				    cp);
			cp++;
		    } else {
			cp += nested_p - cp;
		    }

		    free(freeIt);
		} else if (*cp == '\\' && cp[1] != '\0') {
		    /*
		     * Escaped something -- skip over it
		     */
		    /* XXX: In other places, escaping at this syntactical
		     * position is done by a '$', not a '\'.  The '\' is only
		     * used in variable modifiers. */
		    cp += 2;
		} else {
		    cp++;
		}
	    }

	    if (cp != start) {
		/*
		 * Stuff left over -- add it to the list too
		 */
		gn = Targ_GetNode(start);
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
	    SuffExpandWildcards(cln->prev, pgn);
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
SuffExpandWildcards(GNodeListNode *cln, GNode *pgn)
{
    GNode *cgn = cln->datum;
    StringList *explist;

    if (!Dir_HasWildcards(cgn->name))
	return;

    /*
     * Expand the word along the chosen path
     */
    explist = Lst_New();
    Dir_Expand(cgn->name, Suff_FindPath(cgn), explist);

    while (!Lst_IsEmpty(explist)) {
	GNode	*gn;
	/*
	 * Fetch next expansion off the list and find its GNode
	 */
	char *cp = Lst_Dequeue(explist);

	SUFF_DEBUG1("%s...", cp);
	gn = Targ_GetNode(cp);

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
SearchPath *
Suff_FindPath(GNode* gn)
{
    Suff *suff = gn->suffix;

    if (suff == NULL) {
	char *name = gn->name;
	size_t nameLen = strlen(gn->name);
	SuffListNode *ln;
	for (ln = sufflist->first; ln != NULL; ln = ln->next)
	    if (SuffSuffIsSuffix(ln->datum, nameLen, name + nameLen))
		break;

	SUFF_DEBUG1("Wildcard expanding \"%s\"...", gn->name);
	if (ln != NULL)
	    suff = ln->datum;
	/* XXX: Here we can save the suffix so we don't have to do this again */
    }

    if (suff != NULL) {
	SUFF_DEBUG1("suffix is \"%s\"...\n", suff->name);
	return suff->searchPath;
    } else {
	SUFF_DEBUG0("\n");
	return dirSearchPath;	/* Use default search path */
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
    GNodeListNode *ln, *nln;    /* General node */
    char *tname;		/* Name of transformation rule */
    GNode *gn;			/* Node for same */

    /*
     * Form the proper links between the target and source.
     */
    Lst_Append(tGn->children, sGn);
    Lst_Append(sGn->parents, tGn);
    tGn->unmade++;

    /*
     * Locate the transformation rule itself
     */
    tname = str_concat2(s->name, t->name);
    gn = FindTransformByName(tname);
    free(tname);

    if (gn == NULL) {
	/*
	 * Not really such a transformation rule (can happen when we're
	 * called to link an OP_MEMBER and OP_ARCHV node), so return
	 * FALSE.
	 */
	return FALSE;
    }

    SUFF_DEBUG3("\tapplying %s -> %s to \"%s\"\n", s->name, t->name, tGn->name);

    /*
     * Record last child for expansion purposes
     */
    ln = tGn->children->last;

    /*
     * Pass the buck to Make_HandleUse to apply the rule
     */
    (void)Make_HandleUse(gn, tGn);

    /*
     * Deal with wildcards and variables in any acquired sources
     */
    for (ln = ln != NULL ? ln->next : NULL; ln != NULL; ln = nln) {
	nln = ln->next;
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
SuffFindArchiveDeps(GNode *gn, SrcList *slst)
{
    char *eoarch;		/* End of archive portion */
    char *eoname;		/* End of member portion */
    GNode *mem;			/* Node for member */
    SuffListNode *ln, *nln;	/* Next suffix node to check */
    Suff *ms;			/* Suffix descriptor for member */
    char *name;			/* Start of member's name */

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
    mem = Targ_GetNode(name);
    SuffFindDeps(mem, slst);

    /*
     * Create the link between the two nodes right off
     */
    Lst_Append(gn->children, mem);
    Lst_Append(mem->parents, gn);
    gn->unmade++;

    /*
     * Copy in the variables from the member node to this one.
     */
    Var_Set(PREFIX, GNode_VarPrefix(mem), gn);
    Var_Set(TARGET, GNode_VarTarget(mem), gn);

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
    for (ln = gn->children->first; ln != NULL; ln = nln) {
	nln = ln->next;
	SuffExpandChildren(ln, gn);
    }

    if (ms != NULL) {
	/*
	 * Member has a known suffix, so look for a transformation rule from
	 * it to a possible suffix of the archive. Rather than searching
	 * through the entire list, we just look at suffixes to which the
	 * member's suffix may be transformed...
	 */
	size_t nameLen = (size_t)(eoarch - gn->name);

	/* Use first matching suffix... */
	for (ln = ms->parents->first; ln != NULL; ln = ln->next)
	    if (SuffSuffIsSuffix(ln->datum, nameLen, eoarch))
		break;

	if (ln != NULL) {
	    /*
	     * Got one -- apply it
	     */
	    Suff *suff = ln->datum;
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
    if (!GNode_IsTarget(gn)) {
	gn->type |= OP_DEPENDS;
    }

    /*
     * Flag the member as such so we remember to look in the archive for
     * its modification time. The OP_JOIN | OP_MADE is needed because this
     * target should never get made.
     */
    mem->type |= OP_MEMBER | OP_JOIN | OP_MADE;
}

static void
SuffFindNormalDepsKnown(const char *name, size_t nameLen, GNode *gn,
			SrcList *srcs, SrcList *targs)
{
    SuffListNode *ln;
    Src *targ;
    char *pref;

    for (ln = sufflist->first; ln != NULL; ln = ln->next) {
	Suff *suff = ln->datum;
	if (!SuffSuffIsSuffix(suff, nameLen, name + nameLen))
	    continue;

	pref = bmake_strldup(name, (size_t)(nameLen - suff->nameLen));
	targ = SrcNew(bmake_strdup(gn->name), pref, suff, NULL, gn);
	suff->refCount++;

	/*
	 * Add nodes from which the target can be made
	 */
	SuffAddLevel(srcs, targ);

	/*
	 * Record the target so we can nuke it
	 */
	Lst_Append(targs, targ);
    }
}

static void
SuffFindNormalDepsUnknown(GNode *gn, const char *sopref,
			  SrcList *srcs, SrcList *targs)
{
    Src *targ;

    if (!Lst_IsEmpty(targs) || suffNull == NULL)
	return;

    SUFF_DEBUG1("\tNo known suffix on %s. Using .NULL suffix\n", gn->name);

    targ = SrcNew(bmake_strdup(gn->name), bmake_strdup(sopref),
		  suffNull, NULL, gn);
    targ->suff->refCount++;

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
 * Deal with finding the thing on the default search path. We
 * always do that, not only if the node is only a source (not
 * on the lhs of a dependency operator or [XXX] it has neither
 * children or commands) as the old pmake did.
 */
static void
SuffFindNormalDepsPath(GNode *gn, Src *targ)
{
    if (gn->type & (OP_PHONY | OP_NOPATH))
	return;

    free(gn->path);
    gn->path = Dir_FindFile(gn->name,
			    (targ == NULL ? dirSearchPath :
			     targ->suff->searchPath));
    if (gn->path == NULL)
	return;

    Var_Set(TARGET, gn->path, gn);

    if (targ != NULL) {
	/*
	 * Suffix known for the thing -- trim the suffix off
	 * the path to form the proper .PREFIX variable.
	 */
	size_t savep = strlen(gn->path) - targ->suff->nameLen;
	char savec;
	char *ptr;

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
	char *ptr;

	/* The .PREFIX gets the full path if the target has no known suffix. */
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

/* Locate implicit dependencies for regular targets.
 *
 * Input:
 *	gn		Node for which to find sources
 *
 * Side Effects:
 *	Same as Suff_FindDeps
 */
static void
SuffFindNormalDeps(GNode *gn, SrcList *slst)
{
    SrcList *srcs;		/* List of sources at which to look */
    SrcList *targs;		/* List of targets to which things can be
				 * transformed. They all have the same file,
				 * but different suff and pref fields */
    Src *bottom;		/* Start of found transformation path */
    Src *src;			/* General Src pointer */
    char *pref;			/* Prefix to use */
    Src *targ;			/* General Src target pointer */

    const char *name = gn->name;
    size_t nameLen = strlen(name);

    /*
     * Begin at the beginning...
     */
    srcs = Lst_New();
    targs = Lst_New();

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

	SuffFindNormalDepsKnown(name, nameLen, gn, srcs, targs);

	/* Handle target of unknown suffix... */
	SuffFindNormalDepsUnknown(gn, name, srcs, targs);

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
		targ = targs->first->datum;
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

    Var_Set(TARGET, GNode_Path(gn), gn);

    pref = (targ != NULL) ? targ->pref : gn->name;
    Var_Set(PREFIX, pref, gn);

    /*
     * Now we've got the important local variables set, expand any sources
     * that still contain variables or wildcards in their names.
     */
    {
	SuffListNode *ln, *nln;
	for (ln = gn->children->first; ln != NULL; ln = nln) {
	    nln = ln->next;
	    SuffExpandChildren(ln, gn);
	}
    }

    if (targ == NULL) {
	SUFF_DEBUG1("\tNo valid suffix on %s\n", gn->name);

sfnd_abort:
	SuffFindNormalDepsPath(gn, targ);
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
	bottom->node = Targ_GetNode(bottom->file);
    }

    for (src = bottom; src->parent != NULL; src = src->parent) {
	targ = src->parent;

	if (src->node->suffix)
	    src->node->suffix->refCount--;
	src->node->suffix = src->suff;
	src->node->suffix->refCount++;

	if (targ->node == NULL) {
	    targ->node = Targ_GetNode(targ->file);
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
SuffFindDeps(GNode *gn, SrcList *slst)
{
    if (gn->type & OP_DEPS_FOUND)
	return;
    gn->type |= OP_DEPS_FOUND;

    /*
     * Make sure we have these set, may get revised below.
     */
    Var_Set(TARGET, GNode_Path(gn), gn);
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
	Suff *s = FindSuffByName(LIBSUFF);
	if (gn->suffix)
	    gn->suffix->refCount--;
	if (s != NULL) {
	    gn->suffix = s;
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
Suff_SetNull(const char *name)
{
    Suff *s = FindSuffByName(name);
    if (s == NULL) {
	Parse_Error(PARSE_WARNING, "Desired null suffix %s not defined.",
		    name);
	return;
    }

    if (suffNull != NULL) {
	suffNull->flags &= ~(unsigned)SUFF_NULL;
    }
    s->flags |= SUFF_NULL;
    /*
     * XXX: Here's where the transformation mangling would take place
     */
    suffNull = s;
}

/* Initialize the suffixes module. */
void
Suff_Init(void)
{
#ifdef CLEANUP
    suffClean = Lst_New();
    sufflist = Lst_New();
#endif
    srclist = Lst_New();
    transforms = Lst_New();

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

static void
PrintSuffNames(const char *prefix, SuffList *suffs)
{
    SuffListNode *ln;

    debug_printf("#\t%s: ", prefix);
    for (ln = suffs->first; ln != NULL; ln = ln->next) {
	Suff *suff = ln->datum;
	debug_printf("%s ", suff->name);
    }
    debug_printf("\n");
}

static void
PrintSuff(Suff *s)
{
    debug_printf("# \"%s\" (num %d, ref %d)", s->name, s->sNum, s->refCount);
    if (s->flags != 0) {
	char flags_buf[SuffFlags_ToStringSize];

	debug_printf(" (%s)",
		     Enum_FlagsToString(flags_buf, sizeof flags_buf,
					s->flags, SuffFlags_ToStringSpecs));
    }
    debug_printf("\n");

    PrintSuffNames("To", s->parents);
    PrintSuffNames("From", s->children);

    debug_printf("#\tSearch Path: ");
    Dir_PrintPath(s->searchPath);
    debug_printf("\n");
}

static void
PrintTransformation(GNode *t)
{
    debug_printf("%-16s:", t->name);
    Targ_PrintType(t->type);
    debug_printf("\n");
    Targ_PrintCmds(t);
    debug_printf("\n");
}

void
Suff_PrintAll(void)
{
    debug_printf("#*** Suffixes:\n");
    {
	SuffListNode *ln;
	for (ln = sufflist->first; ln != NULL; ln = ln->next)
	    PrintSuff(ln->datum);
    }

    debug_printf("#*** Transformations:\n");
    {
	GNodeListNode *ln;
	for (ln = transforms->first; ln != NULL; ln = ln->next)
	    PrintTransformation(ln->datum);
    }
}
