/*-
 * Copyright (c) 1988, 1989, 1990, 1993
 *	The Regents of the University of California.  All rights reserved.
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
 *
 * @(#)suff.c	8.4 (Berkeley) 3/21/94
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

/*-
 * suff.c --
 *	Functions to maintain suffix lists and find implicit dependents
 *	using suffix transformation rules
 *
 * Interface:
 *	Suff_Init 	    	Initialize all things to do with suffixes.
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
 */

#include <string.h>
#include <stdlib.h>

#include "arch.h"
#include "buf.h"
#include "config.h"
#include "dir.h"
#include "globals.h"
#include "GNode.h"
#include "lst.h"
#include "make.h"
#include "parse.h"
#include "str.h"
#include "suff.h"
#include "targ.h"
#include "util.h"
#include "var.h"

/* Lst of suffixes */
static Lst sufflist = Lst_Initializer(sufflist);

/* Lst of suffixes to be cleaned */
static Lst suffClean = Lst_Initializer(suffClean);

/* Lst of sources */
static Lst srclist = Lst_Initializer(srclist);

/* Lst of transformation rules */
static Lst transforms = Lst_Initializer(transforms);

static int        sNum = 0;	/* Counter for assigning suffix numbers */

/*
 * Structure describing an individual suffix.
 */
typedef struct _Suff {
    char         *name;	    	/* The suffix itself */
    int		 nameLen;	/* Length of the suffix */
    short	 flags;      	/* Type of suffix */
#define	SUFF_INCLUDE	  0x01	    /* One which is #include'd */
#define	SUFF_LIBRARY	  0x02	    /* One which contains a library */
#define	SUFF_NULL 	  0x04	    /* The empty suffix */
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
    Lst	    	    cp;		/* Debug; children list */
#endif
} Src;

/*
 * A structure for passing more than one argument to the Lst-library-invoked
 * function...
 */
typedef struct {
    Lst            *l;
    Src            *s;
} LstSrc;

static Suff 	    *suffNull;	/* The NULL suffix for this run */
static Suff 	    *emptySuff;	/* The empty suffix required for POSIX
				 * single-suffix transformation rules */


static void SuffInsert(Lst *, Suff *);
static void SuffRemove(Lst *, Suff *);
static Boolean SuffParseTransform(char *, Suff **, Suff **);
static int SuffRebuildGraph(void *, void *);
static int SuffAddSrc(void *, void *);
static int SuffRemoveSrc(Lst *);
static void SuffAddLevel(Lst *, Src *);
static Src *SuffFindThem(Lst *, Lst *);
static Src *SuffFindCmds(Src *, Lst *);
static int SuffExpandChildren(void *, void *);
static Boolean SuffApplyTransform(GNode *, GNode *, Suff *, Suff *);
static void SuffFindDeps(GNode *, Lst *);
static void SuffFindArchiveDeps(GNode *, Lst *);
static void SuffFindNormalDeps(GNode *, Lst *);
static int SuffPrintName(void *, void *);
static int SuffPrintSuff(void *, void *);
static int SuffPrintTrans(void *, void *);
#ifdef DEBUG_SRC
static int PrintAddr(void *, void *);
#endif /* DEBUG_SRC */


	/*************** Lst Predicates ****************/
/*-
 *-----------------------------------------------------------------------
 * SuffStrIsPrefix  --
 *	See if pref is a prefix of str.
 *
 * Results:
 *	NULL if it ain't, pointer to character in str after prefix if so
 *
 * Side Effects:
 *	None
 *-----------------------------------------------------------------------
 */
static char *
SuffStrIsPrefix(const char *pref, char *str)
{

    while (*str && *pref == *str) {
	pref++;
	str++;
    }

    return (*pref ? NULL : str);
}

/*-
 *-----------------------------------------------------------------------
 * SuffSuffIsSuffix  --
 *	See if suff is a suffix of str. Str should point to THE END of the
 *	string to check. (THE END == the null byte)
 *
 * Results:
 *	NULL if it ain't, pointer to character in str before suffix if
 *	it is.
 *
 * Side Effects:
 *	None
 *-----------------------------------------------------------------------
 */
static char *
SuffSuffIsSuffix(const Suff *s, char *str)
{
    const char	   *p1;	    	/* Pointer into suffix name */
    char	   *p2;	    	/* Pointer into string being examined */

    p1 = s->name + s->nameLen;
    p2 = str;

    while (p1 >= s->name && *p1 == *p2) {
	p1--;
	p2--;
    }

    return (p1 == s->name - 1 ? p2 : NULL);
}

/*-
 *-----------------------------------------------------------------------
 * SuffSuffIsSuffixP --
 *	Predicate form of SuffSuffIsSuffix. Passed as the callback function
 *	to Lst_Find.
 *
 * Results:
 *	0 if the suffix is the one desired, non-zero if not.
 *
 * Side Effects:
 *	None.
 *
 * XXX use the function above once constification is complete.
 *-----------------------------------------------------------------------
 */
static int
SuffSuffIsSuffixP(const void *is, const void *str)
{
    const Suff *s = is;
    const char *p1;	    	/* Pointer into suffix name */
    const char *p2 = str;    	/* Pointer into string being examined */

    p1 = s->name + s->nameLen;

    while (p1 >= s->name && *p1 == *p2) {
	p1--;
	p2--;
    }

    return (p1 != s->name - 1);
}

/*-
 *-----------------------------------------------------------------------
 * SuffSuffHasNameP --
 *	Callback procedure for finding a suffix based on its name. Used by
 *	Suff_GetPath.
 *
 * Results:
 *	0 if the suffix is of the given name. non-zero otherwise.
 *
 * Side Effects:
 *	None
 *-----------------------------------------------------------------------
 */
static int
SuffSuffHasNameP(const void *s, const void *sname)
{

    return (strcmp(sname, ((const Suff *)s)->name));
}

/*-
 *-----------------------------------------------------------------------
 * SuffSuffIsPrefix  --
 *	See if the suffix described by s is a prefix of the string. Care
 *	must be taken when using this to search for transformations and
 *	what-not, since there could well be two suffixes, one of which
 *	is a prefix of the other...
 *
 * Results:
 *	0 if s is a prefix of str. non-zero otherwise
 *
 * Side Effects:
 *	None
 *
 * XXX use the function above once constification is complete.
 *-----------------------------------------------------------------------
 */
static int
SuffSuffIsPrefix(const void *s, const void *istr)
{
	const char *pref = ((const Suff *)s)->name;
	const char *str = istr;

	while (*str != '\0' && *pref == *str) {
		pref++;
		str++;
	}

	return (*pref != '\0');
}

/*-
 *-----------------------------------------------------------------------
 * SuffGNHasNameP  --
 *	See if the graph node has the desired name
 *
 * Results:
 *	0 if it does. non-zero if it doesn't
 *
 * Side Effects:
 *	None
 *-----------------------------------------------------------------------
 */
static int
SuffGNHasNameP(const void *gn, const void *name)
{

    return (strcmp(name, ((const GNode *)gn)->name));
}

 	    /*********** Maintenance Functions ************/

#if 0
/*
 * Keep this function for now until it is clear why a .SUFFIXES: doesn't
 * actually delete the suffixes but just puts them on the suffClean list.
 */
/*-
 *-----------------------------------------------------------------------
 * SuffFree  --
 *	Free up all memory associated with the given suffix structure.
 *
 * Results:
 *	none
 *
 * Side Effects:
 *	the suffix entry is detroyed
 *-----------------------------------------------------------------------
 */
static void
SuffFree(void *sp)
{
    Suff *s = sp;

    if (s == suffNull)
	suffNull = NULL;

    if (s == emptySuff)
	emptySuff = NULL;

    Lst_Destroy(&s->ref, NOFREE);
    Lst_Destroy(&s->children, NOFREE);
    Lst_Destroy(&s->parents, NOFREE);
    Lst_Destroy(&s->searchPath, Dir_Destroy);

    free(s->name);
    free(s);
}
#endif

/*-
 *-----------------------------------------------------------------------
 * SuffRemove  --
 *	Remove the suffix into the list
 *
 * Results:
 *	None
 *
 * Side Effects:
 *	The reference count for the suffix is decremented
 *-----------------------------------------------------------------------
 */
static void
SuffRemove(Lst *l, Suff *s)
{
    LstNode *ln = Lst_Member(l, s);

    if (ln != NULL) {
	Lst_Remove(l, ln);
	s->refCount--;
    }
}

/*-
 *-----------------------------------------------------------------------
 * SuffInsert  --
 *	Insert the suffix into the list keeping the list ordered by suffix
 *	numbers.
 *
 * Results:
 *	None
 *
 * Side Effects:
 *	The reference count of the suffix is incremented
 *-----------------------------------------------------------------------
 */
static void
SuffInsert(Lst *l, Suff *s)
{
    LstNode *ln;	/* current element in l we're examining */
    Suff *s2;		/* the suffix descriptor in this element */

    s2 = NULL;
    for (ln = Lst_First(l); ln != NULL; ln = Lst_Succ(ln)) {
	s2 = Lst_Datum(ln);
	if (s2->sNum >= s->sNum)
	    break;
    }
    if (s2 == NULL) {
	    DEBUGF(SUFF, ("inserting an empty list?..."));
    }

    DEBUGF(SUFF, ("inserting %s(%d)...", s->name, s->sNum));
    if (ln == NULL) {
	DEBUGF(SUFF, ("at end of list\n"));
	Lst_AtEnd(l, s);
	s->refCount++;
	Lst_AtEnd(&s->ref, l);
    } else if (s2->sNum != s->sNum) {
	DEBUGF(SUFF, ("before %s(%d)\n", s2->name, s2->sNum));
	Lst_Insert(l, ln, s);
	s->refCount++;
	Lst_AtEnd(&s->ref, l);
    } else {
	DEBUGF(SUFF, ("already there\n"));
    }
}

/*-
 *-----------------------------------------------------------------------
 * Suff_ClearSuffixes --
 *	This is gross. Nuke the list of suffixes but keep all transformation
 *	rules around. The transformation graph is destroyed in this process,
 *	but we leave the list of rules so when a new graph is formed the rules
 *	will remain.
 *	This function is called from the parse module when a
 *	.SUFFIXES:\n line is encountered.
 *
 * Results:
 *	none
 *
 * Side Effects:
 *	the sufflist and its graph nodes are destroyed
 *-----------------------------------------------------------------------
 */
void
Suff_ClearSuffixes(void)
{

    Lst_Concat(&suffClean, &sufflist, LST_CONCLINK);

    sNum = 1;
    suffNull = emptySuff;
    /*
     * Clear suffNull's children list (the other suffixes are built new, but
     * suffNull is used as is).
     * NOFREE is used because all suffixes are are on the suffClean list.
     * suffNull should not have parents.
     */
    Lst_Destroy(&suffNull->children, NOFREE);
}

/*-
 *-----------------------------------------------------------------------
 * SuffParseTransform --
 *	Parse a transformation string to find its two component suffixes.
 *
 * Results:
 *	TRUE if the string is a valid transformation and FALSE otherwise.
 *
 * Side Effects:
 *	The passed pointers are overwritten.
 *
 *-----------------------------------------------------------------------
 */
static Boolean
SuffParseTransform(char *str, Suff **srcPtr, Suff **targPtr)
{
    LstNode		*srcLn;	    /* element in suffix list of trans source*/
    Suff	    	*src;	    /* Source of transformation */
    LstNode		*targLn;    /* element in suffix list of trans target*/
    char	    	*str2;	    /* Extra pointer (maybe target suffix) */
    LstNode 	    	*singleLn;  /* element in suffix list of any suffix
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
	    srcLn = Lst_Find(&sufflist, str, SuffSuffIsPrefix);
	} else {
	    srcLn = Lst_FindFrom(&sufflist, Lst_Succ(srcLn), str,
				  SuffSuffIsPrefix);
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
		*srcPtr = single;
		*targPtr = suffNull;
		return (TRUE);
	    }
	    return (FALSE);
	}
	src = Lst_Datum(srcLn);
	str2 = str + src->nameLen;
	if (*str2 == '\0') {
	    single = src;
	    singleLn = srcLn;
	} else {
	    targLn = Lst_Find(&sufflist, str2, SuffSuffHasNameP);
	    if (targLn != NULL) {
		*srcPtr = src;
		*targPtr = Lst_Datum(targLn);
		return (TRUE);
	    }
	}
    }
}

/*-
 *-----------------------------------------------------------------------
 * Suff_IsTransform  --
 *	Return TRUE if the given string is a transformation rule
 *
 *
 * Results:
 *	TRUE if the string is a concatenation of two known suffixes.
 *	FALSE otherwise
 *
 * Side Effects:
 *	None
 *-----------------------------------------------------------------------
 */
Boolean
Suff_IsTransform(char *str)
{
    Suff    	  *src, *targ;

    return (SuffParseTransform(str, &src, &targ));
}

/*-
 *-----------------------------------------------------------------------
 * Suff_AddTransform --
 *	Add the transformation rule described by the line to the
 *	list of rules and place the transformation itself in the graph
 *
 * Results:
 *	The node created for the transformation in the transforms list
 *
 * Side Effects:
 *	The node is placed on the end of the transforms Lst and links are
 *	made between the two suffixes mentioned in the target name
 *-----------------------------------------------------------------------
 */
GNode *
Suff_AddTransform(char *line)
{
    GNode         *gn;		/* GNode of transformation rule */
    Suff          *s,		/* source suffix */
                  *t;		/* target suffix */
    LstNode 	  *ln;	    	/* Node for existing transformation */

    ln = Lst_Find(&transforms, line, SuffGNHasNameP);
    if (ln == NULL) {
	/*
	 * Make a new graph node for the transformation. It will be filled in
	 * by the Parse module.
	 */
	gn = Targ_NewGN(line);
	Lst_AtEnd(&transforms, gn);
    } else {
	/*
	 * New specification for transformation rule. Just nuke the old list
	 * of commands so they can be filled in again... We don't actually
	 * free the commands themselves, because a given command can be
	 * attached to several different transformations.
	 */
	gn = Lst_Datum(ln);
	Lst_Destroy(&gn->commands, NOFREE);
	Lst_Destroy(&gn->children, NOFREE);
    }

    gn->type = OP_TRANSFORM;

    SuffParseTransform(line, &s, &t);

    /*
     * link the two together in the proper relationship and order
     */
    DEBUGF(SUFF, ("defining transformation from `%s' to `%s'\n",
	   s->name, t->name));
    SuffInsert(&t->children, s);
    SuffInsert(&s->parents, t);

    return (gn);
}

/*-
 *-----------------------------------------------------------------------
 * Suff_EndTransform --
 *	Handle the finish of a transformation definition, removing the
 *	transformation from the graph if it has neither commands nor
 *	sources. This is a callback procedure for the Parse module via
 *	Lst_ForEach
 *
 * Results:
 *	=== 0
 *
 * Side Effects:
 *	If the node has no commands or children, the children and parents
 *	lists of the affected suffices are altered.
 *
 *-----------------------------------------------------------------------
 */
int
Suff_EndTransform(void *gnp, void *dummy __unused)
{
    GNode *gn = (GNode *)gnp;

    if ((gn->type & OP_TRANSFORM) && Lst_IsEmpty(&gn->commands) &&
	Lst_IsEmpty(&gn->children))
    {
	Suff	*s, *t;

	/*
	 * SuffParseTransform() may fail for special rules which are not
	 * actual transformation rules (e.g., .DEFAULT).
	 */
	if (!SuffParseTransform(gn->name, &s, &t))
	    return (0);

	DEBUGF(SUFF, ("deleting transformation from `%s' to `%s'\n",
	       s->name, t->name));

	/*
	 * Remove the source from the target's children list. We check for a
	 * NULL return to handle a beanhead saying something like
	 *  .c.o .c.o:
	 *
	 * We'll be called twice when the next target is seen, but .c and .o
	 * are only linked once...
	 */
	SuffRemove(&t->children, s);

	/*
	 * Remove the target from the source's parents list
	 */
	SuffRemove(&s->parents, t);
    } else if (gn->type & OP_TRANSFORM) {
	DEBUGF(SUFF, ("transformation %s complete\n", gn->name));
    }

    return (0);
}

/*-
 *-----------------------------------------------------------------------
 * SuffRebuildGraph --
 *	Called from Suff_AddSuffix via Lst_ForEach to search through the
 *	list of existing transformation rules and rebuild the transformation
 *	graph when it has been destroyed by Suff_ClearSuffixes. If the
 *	given rule is a transformation involving this suffix and another,
 *	existing suffix, the proper relationship is established between
 *	the two.
 *
 * Results:
 *	Always 0.
 *
 * Side Effects:
 *	The appropriate links will be made between this suffix and
 *	others if transformation rules exist for it.
 *
 *-----------------------------------------------------------------------
 */
static int
SuffRebuildGraph(void *transformp, void *sp)
{
    GNode   	*transform = transformp;
    Suff    	*s = sp;
    char 	*cp;
    LstNode	*ln;
    Suff  	*s2 = NULL;

    /*
     * First see if it is a transformation from this suffix.
     */
    cp = SuffStrIsPrefix(s->name, transform->name);
    if (cp != (char *)NULL) {
        if (cp[0] == '\0')  /* null rule */
	    s2 = suffNull;
	else {
	    ln = Lst_Find(&sufflist, cp, SuffSuffHasNameP);
	    if (ln != NULL)
	        s2 = Lst_Datum(ln);
	}
	if (s2 != NULL) {
	    /*
	     * Found target. Link in and return, since it can't be anything
	     * else.
	     */
	    SuffInsert(&s2->children, s);
	    SuffInsert(&s->parents, s2);
	    return (0);
	}
    }

    /*
     * Not from, maybe to?
     */
    cp = SuffSuffIsSuffix(s, transform->name + strlen(transform->name));
    if (cp != NULL) {
	/*
	 * Null-terminate the source suffix in order to find it.
	 */
	cp[1] = '\0';
	ln = Lst_Find(&sufflist, transform->name, SuffSuffHasNameP);
	/*
	 * Replace the start of the target suffix
	 */
	cp[1] = s->name[0];
	if (ln != NULL) {
	    /*
	     * Found it -- establish the proper relationship
	     */
	    s2 = Lst_Datum(ln);
	    SuffInsert(&s->children, s2);
	    SuffInsert(&s2->parents, s);
	}
    }
    return (0);
}

/*-
 *-----------------------------------------------------------------------
 * Suff_AddSuffix --
 *	Add the suffix in string to the end of the list of known suffixes.
 *	Should we restructure the suffix graph? Make doesn't...
 *
 * Results:
 *	None
 *
 * Side Effects:
 *	A GNode is created for the suffix and a Suff structure is created and
 *	added to the suffixes list unless the suffix was already known.
 *-----------------------------------------------------------------------
 */
void
Suff_AddSuffix(char *str)
{
    Suff          *s;	    /* new suffix descriptor */
    LstNode 	  *ln;

    ln = Lst_Find(&sufflist, str, SuffSuffHasNameP);
    if (ln == NULL) {
	s = emalloc(sizeof(Suff));

	s->name = estrdup(str);
	s->nameLen = strlen(s->name);
	Lst_Init(&s->searchPath);
	Lst_Init(&s->children);
	Lst_Init(&s->parents);
	Lst_Init(&s->ref);
	s->sNum = sNum++;
	s->flags = 0;
	s->refCount = 0;

	Lst_AtEnd(&sufflist, s);

	/*
	 * Look for any existing transformations from or to this suffix.
	 * XXX: Only do this after a Suff_ClearSuffixes?
	 */
	Lst_ForEach(&transforms, SuffRebuildGraph, s);
    }
}

/*-
 *-----------------------------------------------------------------------
 * Suff_GetPath --
 *	Return the search path for the given suffix, if it's defined.
 *
 * Results:
 *	The searchPath for the desired suffix or NULL if the suffix isn't
 *	defined.
 *
 * Side Effects:
 *	None
 *-----------------------------------------------------------------------
 */
Lst *
Suff_GetPath(char *sname)
{
    LstNode   	  *ln;
    Suff    	  *s;

    ln = Lst_Find(&sufflist, sname, SuffSuffHasNameP);
    if (ln == NULL) {
	return (NULL);
    } else {
	s = Lst_Datum(ln);
	return (&s->searchPath);
    }
}

/*-
 *-----------------------------------------------------------------------
 * Suff_DoPaths --
 *	Extend the search paths for all suffixes to include the default
 *	search path.
 *
 * Results:
 *	None.
 *
 * Side Effects:
 *	The searchPath field of all the suffixes is extended by the
 *	directories in dirSearchPath. If paths were specified for the
 *	".h" suffix, the directories are stuffed into a global variable
 *	called ".INCLUDES" with each directory preceded by a -I. The same
 *	is done for the ".a" suffix, except the variable is called
 *	".LIBS" and the flag is -L.
 *-----------------------------------------------------------------------
 */
void
Suff_DoPaths(void)
{
    Suff	*s;
    LstNode	*ln;
    char	*ptr;
    Lst	    	inIncludes;	/* Cumulative .INCLUDES path */
    Lst	    	inLibs;		/* Cumulative .LIBS path */

    Lst_Init(&inIncludes);
    Lst_Init(&inLibs);

    for (ln = Lst_First(&sufflist); ln != NULL; ln = Lst_Succ(ln)) {
	s = Lst_Datum(ln);
	if (!Lst_IsEmpty(&s->searchPath)) {
#ifdef INCLUDES
	    if (s->flags & SUFF_INCLUDE) {
		Dir_Concat(&inIncludes, &s->searchPath);
	    }
#endif /* INCLUDES */
#ifdef LIBRARIES
	    if (s->flags & SUFF_LIBRARY) {
		Dir_Concat(&inLibs, &s->searchPath);
	    }
#endif /* LIBRARIES */
	    Dir_Concat(&s->searchPath, &dirSearchPath);
	} else {
	    Lst_Destroy(&s->searchPath, Dir_Destroy);
	    Lst_Duplicate(&s->searchPath, &dirSearchPath, Dir_CopyDir);
	}
    }

    Var_Set(".INCLUDES", ptr = Dir_MakeFlags("-I", &inIncludes), VAR_GLOBAL);
    free(ptr);
    Var_Set(".LIBS", ptr = Dir_MakeFlags("-L", &inLibs), VAR_GLOBAL);
    free(ptr);

    Lst_Destroy(&inIncludes, Dir_Destroy);
    Lst_Destroy(&inLibs, Dir_Destroy);
}

/*-
 *-----------------------------------------------------------------------
 * Suff_AddInclude --
 *	Add the given suffix as a type of file which gets included.
 *	Called from the parse module when a .INCLUDES line is parsed.
 *	The suffix must have already been defined.
 *
 * Results:
 *	None.
 *
 * Side Effects:
 *	The SUFF_INCLUDE bit is set in the suffix's flags field
 *
 *-----------------------------------------------------------------------
 */
void
Suff_AddInclude(char *sname)
{
    LstNode	  *ln;
    Suff	  *s;

    ln = Lst_Find(&sufflist, sname, SuffSuffHasNameP);
    if (ln != NULL) {
	s = Lst_Datum(ln);
	s->flags |= SUFF_INCLUDE;
    }
}

/*-
 *-----------------------------------------------------------------------
 * Suff_AddLib --
 *	Add the given suffix as a type of file which is a library.
 *	Called from the parse module when parsing a .LIBS line. The
 *	suffix must have been defined via .SUFFIXES before this is
 *	called.
 *
 * Results:
 *	None.
 *
 * Side Effects:
 *	The SUFF_LIBRARY bit is set in the suffix's flags field
 *
 *-----------------------------------------------------------------------
 */
void
Suff_AddLib(char *sname)
{
    LstNode	  *ln;
    Suff	  *s;

    ln = Lst_Find(&sufflist, sname, SuffSuffHasNameP);
    if (ln != NULL) {
	s = Lst_Datum(ln);
	s->flags |= SUFF_LIBRARY;
    }
}

 	  /********** Implicit Source Search Functions *********/

/*-
 *-----------------------------------------------------------------------
 * SuffAddSrc  --
 *	Add a suffix as a Src structure to the given list with its parent
 *	being the given Src structure. If the suffix is the null suffix,
 *	the prefix is used unaltered as the file name in the Src structure.
 *
 * Results:
 *	always returns 0
 *
 * Side Effects:
 *	A Src structure is created and tacked onto the end of the list
 *-----------------------------------------------------------------------
 */
static int
SuffAddSrc(void *sp, void *lsp)
{
    Suff	*s = sp;
    LstSrc      *ls = lsp;
    Src         *s2;	    /* new Src structure */
    Src    	*targ; 	    /* Target structure */

    targ = ls->s;

    if ((s->flags & SUFF_NULL) && (*s->name != '\0')) {
	/*
	 * If the suffix has been marked as the NULL suffix, also create a Src
	 * structure for a file with no suffix attached. Two birds, and all
	 * that...
	 */
	s2 = emalloc(sizeof(Src));
	s2->file = estrdup(targ->pref);
	s2->pref = targ->pref;
	s2->parent = targ;
	s2->node = NULL;
	s2->suff = s;
	s->refCount++;
	s2->children =	0;
	targ->children += 1;
	Lst_AtEnd(ls->l, s2);
#ifdef DEBUG_SRC
	Lst_Init(&s2->cp);
	Lst_AtEnd(&targ->cp, s2);
	printf("1 add %p %p to %p:", targ, s2, ls->l);
	Lst_ForEach(ls->l, PrintAddr, (void *)NULL);
	printf("\n");
#endif
    }
    s2 = emalloc(sizeof(Src));
    s2->file = str_concat(targ->pref, s->name, 0);
    s2->pref = targ->pref;
    s2->parent = targ;
    s2->node = NULL;
    s2->suff = s;
    s->refCount++;
    s2->children = 0;
    targ->children += 1;
    Lst_AtEnd(ls->l, s2);
#ifdef DEBUG_SRC
    Lst_Init(&s2->cp);
    Lst_AtEnd(&targ->cp, s2);
    printf("2 add %p %p to %p:", targ, s2, ls->l);
    Lst_ForEach(ls->l, PrintAddr, (void *)NULL);
    printf("\n");
#endif

    return (0);
}

/*-
 *-----------------------------------------------------------------------
 * SuffAddLevel  --
 *	Add all the children of targ as Src structures to the given list
 *
 * Results:
 *	None
 *
 * Side Effects:
 * 	Lots of structures are created and added to the list
 *-----------------------------------------------------------------------
 */
static void
SuffAddLevel(Lst *l, Src *targ)
{
    LstSrc         ls;

    ls.s = targ;
    ls.l = l;

    Lst_ForEach(&targ->suff->children, SuffAddSrc, &ls);
}

/*-
 *----------------------------------------------------------------------
 * SuffRemoveSrc --
 *	Free all src structures in list that don't have a reference count
 *	XXX this actually frees only the first of these.
 *
 * Results:
 *	True if a src was removed
 *
 * Side Effects:
 *	The memory is free'd.
 *----------------------------------------------------------------------
 */
static int
SuffRemoveSrc(Lst *l)
{
    LstNode *ln, *ln1;
    Src *s;
    int t = 0;

#ifdef DEBUG_SRC
    printf("cleaning %lx: ", (unsigned long) l);
    Lst_ForEach(l, PrintAddr, (void *)NULL);
    printf("\n");
#endif

    for (ln = Lst_First(l); ln != NULL; ln = ln1) {
	ln1 = Lst_Succ(ln);

	s = (Src *)Lst_Datum(ln);
	if (s->children == 0) {
	    free(s->file);
	    if (!s->parent)
		free(s->pref);
	    else {
#ifdef DEBUG_SRC
		LstNode *ln = Lst_Member(&s->parent->cp, s);
		if (ln != NULL)
		    Lst_Remove(&s->parent->cp, ln);
#endif
		--s->parent->children;
	    }
#ifdef DEBUG_SRC
	    printf("free: [l=%p] p=%p %d\n", l, s, s->children);
	    Lst_Destroy(&s->cp, NOFREE);
#endif
	    Lst_Remove(l, ln);
	    free(s);
	    t |= 1;
	    return (TRUE);
	}
#ifdef DEBUG_SRC
	else {
	    printf("keep: [l=%p] p=%p %d: ", l, s, s->children);
	    Lst_ForEach(&s->cp, PrintAddr, (void *)NULL);
	    printf("\n");
	}
#endif
    }

    return (t);
}

/*-
 *-----------------------------------------------------------------------
 * SuffFindThem --
 *	Find the first existing file/target in the list srcs
 *
 * Results:
 *	The lowest structure in the chain of transformations
 *
 * Side Effects:
 *	None
 *-----------------------------------------------------------------------
 */
static Src *
SuffFindThem(Lst *srcs, Lst *slst)
{
    Src            *s;		/* current Src */
    Src		   *rs;		/* returned Src */
    char	   *ptr;

    rs = NULL;

    while (!Lst_IsEmpty (srcs)) {
	s = Lst_DeQueue(srcs);

	DEBUGF(SUFF, ("\ttrying %s...", s->file));

	/*
	 * A file is considered to exist if either a node exists in the
	 * graph for it or the file actually exists.
	 */
	if (Targ_FindNode(s->file, TARG_NOCREATE) != NULL) {
#ifdef DEBUG_SRC
	    printf("remove %p from %p\n", s, srcs);
#endif
	    rs = s;
	    break;
	}

	if ((ptr = Dir_FindFile(s->file, &s->suff->searchPath)) != NULL) {
	    rs = s;
#ifdef DEBUG_SRC
	    printf("remove %p from %p\n", s, srcs);
#endif
	    free(ptr);
	    break;
	}

	DEBUGF(SUFF, ("not there\n"));

	SuffAddLevel(srcs, s);
	Lst_AtEnd(slst, s);
    }

    if (rs) {
	DEBUGF(SUFF, ("got it\n"));
    }
    return (rs);
}

/*-
 *-----------------------------------------------------------------------
 * SuffFindCmds --
 *	See if any of the children of the target in the Src structure is
 *	one from which the target can be transformed. If there is one,
 *	a Src structure is put together for it and returned.
 *
 * Results:
 *	The Src structure of the "winning" child, or NULL if no such beast.
 *
 * Side Effects:
 *	A Src structure may be allocated.
 *
 *-----------------------------------------------------------------------
 */
static Src *
SuffFindCmds(Src *targ, Lst *slst)
{
    LstNode 	  	*ln; 	/* General-purpose list node */
    GNode		*t, 	/* Target GNode */
	    	  	*s; 	/* Source GNode */
    int	    	  	prefLen;/* The length of the defined prefix */
    Suff    	  	*suff;	/* Suffix on matching beastie */
    Src	    	  	*ret;	/* Return value */
    char    	  	*cp;

    t = targ->node;
    prefLen = strlen(targ->pref);

    for (ln = Lst_First(&t->children); ln != NULL; ln = Lst_Succ(ln)) {
	s = Lst_Datum(ln);

	cp = strrchr(s->name, '/');
	if (cp == NULL) {
	    cp = s->name;
	} else {
	    cp++;
	}
	if (strncmp(cp, targ->pref, prefLen) == 0) {
	    /*
	     * The node matches the prefix ok, see if it has a known
	     * suffix.
	     */
	    ln = Lst_Find(&sufflist, &cp[prefLen], SuffSuffHasNameP);
	    if (ln != NULL) {
		/*
		 * It even has a known suffix, see if there's a transformation
		 * defined between the node's suffix and the target's suffix.
		 *
		 * XXX: Handle multi-stage transformations here, too.
		 */
		suff = Lst_Datum(ln);

		if (Lst_Member(&suff->parents, targ->suff) != NULL) {
		    /*
		     * Hot Damn! Create a new Src structure to describe
		     * this transformation (making sure to duplicate the
		     * source node's name so Suff_FindDeps can free it
		     * again (ick)), and return the new structure.
		     */
		    ret = emalloc(sizeof(Src));
		    ret->file = estrdup(s->name);
		    ret->pref = targ->pref;
		    ret->suff = suff;
		    suff->refCount++;
		    ret->parent = targ;
		    ret->node = s;
		    ret->children = 0;
		    targ->children += 1;
#ifdef DEBUG_SRC
		    Lst_Init(&ret->cp);
		    printf("3 add %p %p\n", &targ, ret);
		    Lst_AtEnd(&targ->cp, ret);
#endif
		    Lst_AtEnd(slst, ret);
		    DEBUGF(SUFF, ("\tusing existing source %s\n", s->name));
		    return (ret);
		}
	    }
	}
    }
    return (NULL);
}

/*-
 *-----------------------------------------------------------------------
 * SuffExpandChildren --
 *	Expand the names of any children of a given node that contain
 *	variable invocations or file wildcards into actual targets.
 *
 * Results:
 *	== 0 (continue)
 *
 * Side Effects:
 *	The expanded node is removed from the parent's list of children,
 *	and the parent's unmade counter is decremented, but other nodes
 * 	may be added.
 *
 *-----------------------------------------------------------------------
 */
static int
SuffExpandChildren(void *cgnp, void *pgnp)
{
    GNode   	*cgn = cgnp;
    GNode   	*pgn = pgnp;
    GNode	*gn;	    /* New source 8) */
    LstNode   	*prevLN;    /* Node after which new source should be put */
    LstNode	*ln; 	    /* List element for old source */
    char	*cp;	    /* Expanded value */
    Buffer	*buf;

    /*
     * New nodes effectively take the place of the child, so place them
     * after the child
     */
    prevLN = Lst_Member(&pgn->children, cgn);

    /*
     * First do variable expansion -- this takes precedence over
     * wildcard expansion. If the result contains wildcards, they'll be gotten
     * to later since the resulting words are tacked on to the end of
     * the children list.
     */
    if (strchr(cgn->name, '$') != NULL) {
	DEBUGF(SUFF, ("Expanding \"%s\"...", cgn->name));
	buf = Var_Subst(NULL, cgn->name, pgn, TRUE);
	cp = Buf_GetAll(buf, NULL);
	Buf_Destroy(buf, FALSE);

	if (cp != NULL) {
	    Lst members = Lst_Initializer(members);

	    if (cgn->type & OP_ARCHV) {
		/*
		 * Node was an archive(member) target, so we want to call
		 * on the Arch module to find the nodes for us, expanding
		 * variables in the parent's context.
		 */
		char	*sacrifice = cp;

		Arch_ParseArchive(&sacrifice, &members, pgn);
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
			Lst_AtEnd(&members, gn);
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
			char	*junk;
			size_t	len = 0;
			Boolean	doFree;

			junk = Var_Parse(cp, pgn, TRUE, &len, &doFree);
			if (junk != var_Error) {
			    cp += len - 1;
			}

			if (doFree) {
			    free(junk);
			}
		    } else if (*cp == '\\' && *cp != '\0') {
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
		    Lst_AtEnd(&members, gn);
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
	    while(!Lst_IsEmpty(&members)) {
		gn = Lst_DeQueue(&members);

		DEBUGF(SUFF, ("%s...", gn->name));
		if (Lst_Member(&pgn->children, gn) == NULL) {
		    Lst_Append(&pgn->children, prevLN, gn);
		    prevLN = Lst_Succ(prevLN);
		    Lst_AtEnd(&gn->parents, pgn);
		    pgn->unmade++;
		}
	    }

	    /*
	     * Free the result
	     */
	    free(cp);
	}
	/*
	 * Now the source is expanded, remove it from the list of children to
	 * keep it from being processed.
	 */
	ln = Lst_Member(&pgn->children, cgn);
	pgn->unmade--;
	Lst_Remove(&pgn->children, ln);
	DEBUGF(SUFF, ("\n"));
    } else if (Dir_HasWildcards(cgn->name)) {
	Lst exp;	    /* List of expansions */
	Lst *path;	    /* Search path along which to expand */

	/*
	 * Find a path along which to expand the word.
	 *
	 * If the word has a known suffix, use that path.
	 * If it has no known suffix and we're allowed to use the null
	 *   suffix, use its path.
	 * Else use the default system search path.
	 */
	cp = cgn->name + strlen(cgn->name);
	ln = Lst_Find(&sufflist, cp, SuffSuffIsSuffixP);

	DEBUGF(SUFF, ("Wildcard expanding \"%s\"...", cgn->name));

	if (ln != NULL) {
	    Suff    *s = Lst_Datum(ln);

	    DEBUGF(SUFF, ("suffix is \"%s\"...", s->name));
	    path = &s->searchPath;
	} else {
	    /*
	     * Use default search path
	     */
	    path = &dirSearchPath;
	}

	/*
	 * Expand the word along the chosen path
	 */
	Lst_Init(&exp);
	Dir_Expand(cgn->name, path, &exp);

	while (!Lst_IsEmpty(&exp)) {
	    /*
	     * Fetch next expansion off the list and find its GNode
	     */
	    cp = Lst_DeQueue(&exp);

	    DEBUGF(SUFF, ("%s...", cp));
	    gn = Targ_FindNode(cp, TARG_CREATE);

	    /*
	     * If gn isn't already a child of the parent, make it so and
	     * up the parent's count of unmade children.
	     */
	    if (Lst_Member(&pgn->children, gn) == NULL) {
		Lst_Append(&pgn->children, prevLN, gn);
		prevLN = Lst_Succ(prevLN);
		Lst_AtEnd(&gn->parents, pgn);
		pgn->unmade++;
	    }
	}

	/*
	 * Now the source is expanded, remove it from the list of children to
	 * keep it from being processed.
	 */
	ln = Lst_Member(&pgn->children, cgn);
	pgn->unmade--;
	Lst_Remove(&pgn->children, ln);
	DEBUGF(SUFF, ("\n"));
    }

    return (0);
}

/*-
 *-----------------------------------------------------------------------
 * SuffApplyTransform --
 *	Apply a transformation rule, given the source and target nodes
 *	and suffixes.
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
 *
 *-----------------------------------------------------------------------
 */
static Boolean
SuffApplyTransform(GNode *tGn, GNode *sGn, Suff *t, Suff *s)
{
    LstNode 	*ln; 	    /* General node */
    char    	*tname;	    /* Name of transformation rule */
    GNode   	*gn;	    /* Node for same */

    if (Lst_Member(&tGn->children, sGn) == NULL) {
	/*
	 * Not already linked, so form the proper links between the
	 * target and source.
	 */
	Lst_AtEnd(&tGn->children, sGn);
	Lst_AtEnd(&sGn->parents, tGn);
	tGn->unmade += 1;
    }

    if ((sGn->type & OP_OPMASK) == OP_DOUBLEDEP) {
	/*
	 * When a :: node is used as the implied source of a node, we have
	 * to link all its cohorts in as sources as well. Only the initial
	 * sGn gets the target in its iParents list, however, as that
	 * will be sufficient to get the .IMPSRC variable set for tGn
	 */
	for (ln = Lst_First(&sGn->cohorts); ln != NULL; ln = Lst_Succ(ln)) {
	    gn = Lst_Datum(ln);

	    if (Lst_Member(&tGn->children, gn) == NULL) {
		/*
		 * Not already linked, so form the proper links between the
		 * target and source.
		 */
		Lst_AtEnd(&tGn->children, gn);
		Lst_AtEnd(&gn->parents, tGn);
		tGn->unmade += 1;
	    }
	}
    }
    /*
     * Locate the transformation rule itself
     */
    tname = str_concat(s->name, t->name, 0);
    ln = Lst_Find(&transforms, tname, SuffGNHasNameP);
    free(tname);

    if (ln == NULL) {
	/*
	 * Not really such a transformation rule (can happen when we're
	 * called to link an OP_MEMBER and OP_ARCHV node), so return
	 * FALSE.
	 */
	return (FALSE);
    }

    gn = Lst_Datum(ln);

    DEBUGF(SUFF, ("\tapplying %s -> %s to \"%s\"\n", s->name, t->name, tGn->name));

    /*
     * Record last child for expansion purposes
     */
    ln = Lst_Last(&tGn->children);

    /*
     * Pass the buck to Make_HandleUse to apply the rule
     */
    Make_HandleUse(gn, tGn);

    /*
     * Deal with wildcards and variables in any acquired sources
     */
    ln = Lst_Succ(ln);
    if (ln != NULL) {
	Lst_ForEachFrom(&tGn->children, ln, SuffExpandChildren, tGn);
    }

    /*
     * Keep track of another parent to which this beast is transformed so
     * the .IMPSRC variable can be set correctly for the parent.
     */
    Lst_AtEnd(&sGn->iParents, tGn);

    return (TRUE);
}


/*-
 *-----------------------------------------------------------------------
 * SuffFindArchiveDeps --
 *	Locate dependencies for an OP_ARCHV node.
 *
 * Results:
 *	None
 *
 * Side Effects:
 *	Same as Suff_FindDeps
 *
 *-----------------------------------------------------------------------
 */
static void
SuffFindArchiveDeps(GNode *gn, Lst *slst)
{
    char    	*eoarch;    /* End of archive portion */
    char    	*eoname;    /* End of member portion */
    GNode   	*mem;	    /* Node for member */
    /* Variables to be copied from the member node */
    static char *const copy[] = {
	TARGET,	    	    /* Must be first */
	PREFIX,	    	    /* Must be second */
    };
    int	    	i;  	    /* Index into copy and vals */
    Suff    	*ms;	    /* Suffix descriptor for member */
    char    	*name;	    /* Start of member's name */

    /*
     * The node is an archive(member) pair. so we must find a
     * suffix for both of them.
     */
    eoarch = strchr(gn->name, '(');
    eoname = strchr(eoarch, ')');

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
    if (Lst_Member(&gn->children, mem) == NULL) {
	Lst_AtEnd(&gn->children, mem);
	Lst_AtEnd(&mem->parents, gn);
	gn->unmade += 1;
    }

    /*
     * Copy in the variables from the member node to this one.
     */
    for (i = (sizeof(copy) / sizeof(copy[0]))-1; i >= 0; i--) {
	char *p1;
	Var_Set(copy[i], Var_Value(copy[i], mem, &p1), gn);
	free(p1);

    }

    ms = mem->suffix;
    if (ms == NULL) {
	/*
	 * Didn't know what it was -- use .NULL suffix if not in make mode
	 */
	DEBUGF(SUFF, ("using null suffix\n"));
	ms = suffNull;
    }


    /*
     * Set the other two local variables required for this target.
     */
    Var_Set(MEMBER, name, gn);
    Var_Set(ARCHIVE, gn->name, gn);

    if (ms != NULL) {
	/*
	 * Member has a known suffix, so look for a transformation rule from
	 * it to a possible suffix of the archive. Rather than searching
	 * through the entire list, we just look at suffixes to which the
	 * member's suffix may be transformed...
	 */
	LstNode *ln;

	/*
	 * Use first matching suffix...
	 */
	ln = Lst_Find(&ms->parents, eoarch, SuffSuffIsSuffixP);

	if (ln != NULL) {
	    /*
	     * Got one -- apply it
	     */
	    if (!SuffApplyTransform(gn, mem, Lst_Datum(ln), ms)) {
		DEBUGF(SUFF, ("\tNo transformation from %s -> %s\n",
		       ms->name, ((Suff *)Lst_Datum(ln))->name));
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
     * its modification time.
     */
    mem->type |= OP_MEMBER;
}

/*-
 *-----------------------------------------------------------------------
 * SuffFindNormalDeps --
 *	Locate implicit dependencies for regular targets.
 *
 * Results:
 *	None.
 *
 * Side Effects:
 *	Same as Suff_FindDeps...
 *
 *-----------------------------------------------------------------------
 */
static void
SuffFindNormalDeps(GNode *gn, Lst *slst)
{
    char    	*eoname;    /* End of name */
    char    	*sopref;    /* Start of prefix */
    LstNode 	*ln; 	    /* Next suffix node to check */
    Lst		srcs;	    /* List of sources at which to look */
    Lst		targs;	    /* List of targets to which things can be
			     * transformed. They all have the same file,
			     * but different suff and pref fields */
    Src	    	*bottom;    /* Start of found transformation path */
    Src 	*src;	    /* General Src pointer */
    char    	*pref;	    /* Prefix to use */
    Src	    	*targ;	    /* General Src target pointer */


    eoname = gn->name + strlen(gn->name);

    sopref = gn->name;

    /*
     * Begin at the beginning...
     */
    ln = Lst_First(&sufflist);
    Lst_Init(&srcs);
    Lst_Init(&targs);

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

    while (ln != NULL) {
	/*
	 * Look for next possible suffix...
	 */
	ln = Lst_FindFrom(&sufflist, ln, eoname, SuffSuffIsSuffixP);

	if (ln != NULL) {
	    int	    prefLen;	    /* Length of the prefix */
	    Src	    *target;

	    /*
	     * Allocate a Src structure to which things can be transformed
	     */
	    target = emalloc(sizeof(Src));
	    target->file = estrdup(gn->name);
	    target->suff = Lst_Datum(ln);
	    target->suff->refCount++;
	    target->node = gn;
	    target->parent = NULL;
	    target->children = 0;
#ifdef DEBUG_SRC
	    Lst_Init(&target->cp);
#endif

	    /*
	     * Allocate room for the prefix, whose end is found by subtracting
	     * the length of the suffix from the end of the name.
	     */
	    prefLen = (eoname - target->suff->nameLen) - sopref;
	    target->pref = emalloc(prefLen + 1);
	    memcpy(target->pref, sopref, prefLen);
	    target->pref[prefLen] = '\0';

	    /*
	     * Add nodes from which the target can be made
	     */
	    SuffAddLevel(&srcs, target);

	    /*
	     * Record the target so we can nuke it
	     */
	    Lst_AtEnd(&targs, target);

	    /*
	     * Search from this suffix's successor...
	     */
	    ln = Lst_Succ(ln);
	}
    }

    /*
     * Handle target of unknown suffix...
     */
    if (Lst_IsEmpty(&targs) && suffNull != NULL) {
	DEBUGF(SUFF, ("\tNo known suffix on %s. Using .NULL suffix\n", gn->name));

	targ = emalloc(sizeof(Src));
	targ->file = estrdup(gn->name);
	targ->suff = suffNull;
	targ->suff->refCount++;
	targ->node = gn;
	targ->parent = NULL;
	targ->children = 0;
	targ->pref = estrdup(sopref);
#ifdef DEBUG_SRC
	Lst_Init(&targ->cp);
#endif

	/*
	 * Only use the default suffix rules if we don't have commands
	 * or dependencies defined for this gnode
	 */
	if (Lst_IsEmpty(&gn->commands) && Lst_IsEmpty(&gn->children))
	    SuffAddLevel(&srcs, targ);
	else {
	    DEBUGF(SUFF, ("not "));
	}

	DEBUGF(SUFF, ("adding suffix rules\n"));

	Lst_AtEnd(&targs, targ);
    }

    /*
     * Using the list of possible sources built up from the target suffix(es),
     * try and find an existing file/target that matches.
     */
    bottom = SuffFindThem(&srcs, slst);

    if (bottom == NULL) {
	/*
	 * No known transformations -- use the first suffix found for setting
	 * the local variables.
	 */
	if (!Lst_IsEmpty(&targs)) {
	    targ = Lst_Datum(Lst_First(&targs));
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

    /*
     * The .TARGET variable we always set to be the name at this point,
     * since it's only set to the path if the thing is only a source and
     * if it's only a source, it doesn't matter what we put here as far
     * as expanding sources is concerned, since it has none...
     */
    Var_Set(TARGET, gn->name, gn);

    pref = (targ != NULL) ? targ->pref : gn->name;
    Var_Set(PREFIX, pref, gn);

    /*
     * Now we've got the important local variables set, expand any sources
     * that still contain variables or wildcards in their names.
     */
    Lst_ForEach(&gn->children, SuffExpandChildren, (void *)gn);

    if (targ == NULL) {
	DEBUGF(SUFF, ("\tNo valid suffix on %s\n", gn->name));

sfnd_abort:
	/*
	 * Deal with finding the thing on the default search path if the
	 * node is only a source (not on the lhs of a dependency operator
	 * or [XXX] it has neither children or commands).
	 */
	if (OP_NOP(gn->type) ||
	    (Lst_IsEmpty(&gn->children) && Lst_IsEmpty(&gn->commands)))
	{
	    gn->path = Dir_FindFile(gn->name,
				    (targ == NULL ? &dirSearchPath :
				     &targ->suff->searchPath));
	    if (gn->path != NULL) {
		char *ptr;
		Var_Set(TARGET, gn->path, gn);

		if (targ != NULL) {
		    /*
		     * Suffix known for the thing -- trim the suffix off
		     * the path to form the proper .PREFIX variable.
		     */
		    int		savep = strlen(gn->path) - targ->suff->nameLen;
		    char	savec;

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
	} else {
	    /*
	     * Not appropriate to search for the thing -- set the
	     * path to be the name so Dir_MTime won't go grovelling for
	     * it.
	     */
	    if (gn->suffix)
		gn->suffix->refCount--;
	    gn->suffix = (targ == NULL) ? NULL : targ->suff;
	    if (gn->suffix)
		gn->suffix->refCount++;
	    free(gn->path);
	    gn->path = estrdup(gn->name);
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
    if (!Lst_IsEmpty(&gn->children)) {
	src = SuffFindCmds(targ, slst);

	if (src != NULL) {
	    /*
	     * Free up all the Src structures in the transformation path
	     * up to, but not including, the parent node.
	     */
	    while (bottom && bottom->parent != NULL) {
		if (Lst_Member(slst, bottom) == NULL) {
		    Lst_AtEnd(slst, bottom);
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
     * So Dir_MTime doesn't go questing for it...
     */
    free(gn->path);
    gn->path = estrdup(gn->name);

    /*
     * Nuke the transformation path and the Src structures left over in the
     * two lists.
     */
sfnd_return:
    if (bottom)
	if (Lst_Member(slst, bottom) == NULL)
	    Lst_AtEnd(slst, bottom);

    while (SuffRemoveSrc(&srcs) || SuffRemoveSrc(&targs))
	continue;

    Lst_Concat(slst, &srcs, LST_CONCLINK);
    Lst_Concat(slst, &targs, LST_CONCLINK);
}

/*-
 *-----------------------------------------------------------------------
 * Suff_FindDeps  --
 *	Find implicit sources for the target described by the graph node
 *	gn
 *
 * Results:
 *	Nothing.
 *
 * Side Effects:
 *	Nodes are added to the graph below the passed-in node. The nodes
 *	are marked to have their IMPSRC variable filled in. The
 *	PREFIX variable is set for the given node and all its
 *	implied children.
 *
 * Notes:
 *	The path found by this target is the shortest path in the
 *	transformation graph, which may pass through non-existent targets,
 *	to an existing target. The search continues on all paths from the
 *	root suffix until a file is found. I.e. if there's a path
 *	.o -> .c -> .l -> .l,v from the root and the .l,v file exists but
 *	the .c and .l files don't, the search will branch out in
 *	all directions from .o and again from all the nodes on the
 *	next level until the .l,v node is encountered.
 *
 *-----------------------------------------------------------------------
 */
void
Suff_FindDeps(GNode *gn)
{

    SuffFindDeps(gn, &srclist);
    while (SuffRemoveSrc(&srclist))
	continue;
}


static void
SuffFindDeps(GNode *gn, Lst *slst)
{

    if (gn->type & OP_DEPS_FOUND) {
	/*
	 * If dependencies already found, no need to do it again...
	 */
	return;
    } else {
	gn->type |= OP_DEPS_FOUND;
    }

    DEBUGF(SUFF, ("SuffFindDeps (%s)\n", gn->name));

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
	LstNode *ln;
	Suff	*s;

	ln = Lst_Find(&sufflist, LIBSUFF, SuffSuffHasNameP);
	if (gn->suffix)
	    gn->suffix->refCount--;
	if (ln != NULL) {
	    gn->suffix = s = Lst_Datum(ln);
	    gn->suffix->refCount++;
	    Arch_FindLib(gn, &s->searchPath);
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

/*-
 *-----------------------------------------------------------------------
 * Suff_SetNull --
 *	Define which suffix is the null suffix.
 *
 * Results:
 *	None.
 *
 * Side Effects:
 *	'suffNull' is altered.
 *
 * Notes:
 *	Need to handle the changing of the null suffix gracefully so the
 *	old transformation rules don't just go away.
 *
 *-----------------------------------------------------------------------
 */
void
Suff_SetNull(char *name)
{
    Suff    *s;
    LstNode *ln;

    ln = Lst_Find(&sufflist, name, SuffSuffHasNameP);
    if (ln != NULL) {
	s = Lst_Datum(ln);
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

/*-
 *-----------------------------------------------------------------------
 * Suff_Init --
 *	Initialize suffixes module
 *
 * Results:
 *	None
 *
 * Side Effects:
 *	Many
 *-----------------------------------------------------------------------
 */
void
Suff_Init(void)
{

    sNum = 0;
    /*
     * Create null suffix for single-suffix rules (POSIX). The thing doesn't
     * actually go on the suffix list or everyone will think that's its
     * suffix.
     */
    emptySuff = suffNull = emalloc(sizeof(Suff));

    suffNull->name = estrdup("");
    suffNull->nameLen = 0;
    Lst_Init(&suffNull->searchPath);
    Dir_Concat(&suffNull->searchPath, &dirSearchPath);
    Lst_Init(&suffNull->children);
    Lst_Init(&suffNull->parents);
    Lst_Init(&suffNull->ref);
    suffNull->sNum = sNum++;
    suffNull->flags = SUFF_NULL;
    suffNull->refCount = 1;
}

/********************* DEBUGGING FUNCTIONS **********************/

static int
SuffPrintName(void *s, void *dummy __unused)
{

    printf("`%s' ", ((Suff *)s)->name);
    return (0);
}

static int
SuffPrintSuff(void *sp, void *dummy __unused)
{
    Suff    *s = sp;
    int	    flags;
    int	    flag;

    printf("# `%s' [%d] ", s->name, s->refCount);

    flags = s->flags;
    if (flags) {
	fputs(" (", stdout);
	while (flags) {
	    flag = 1 << (ffs(flags) - 1);
	    flags &= ~flag;
	    switch (flag) {
		case SUFF_NULL:
		    printf("NULL");
		    break;
		case SUFF_INCLUDE:
		    printf("INCLUDE");
		    break;
		case SUFF_LIBRARY:
		    printf("LIBRARY");
		    break;
		default:
		    break;
	    }
	    fputc(flags ? '|' : ')', stdout);
	}
    }
    fputc('\n', stdout);
    printf("#\tTo: ");
    Lst_ForEach(&s->parents, SuffPrintName, (void *)NULL);
    fputc('\n', stdout);
    printf("#\tFrom: ");
    Lst_ForEach(&s->children, SuffPrintName, (void *)NULL);
    fputc('\n', stdout);
    printf("#\tSearch Path: ");
    Dir_PrintPath(&s->searchPath);
    fputc('\n', stdout);
    return (0);
}

static int
SuffPrintTrans(void *tp, void *dummy __unused)
{
    GNode   *t = tp;

    printf("%-16s: ", t->name);
    Targ_PrintType(t->type);
    fputc('\n', stdout);
    Lst_ForEach(&t->commands, Targ_PrintCmd, (void *)NULL);
    fputc('\n', stdout);
    return (0);
}

void
Suff_PrintAll(void)
{

    printf("#*** Suffixes:\n");
    Lst_ForEach(&sufflist, SuffPrintSuff, (void *)NULL);

    printf("#*** Transformations:\n");
    Lst_ForEach(&transforms, SuffPrintTrans, (void *)NULL);
}

#ifdef DEBUG_SRC
/*
 * Printaddr --
 * 	Print the address of a node.
 */
static int
PrintAddr(void *a, void *b __unused)
{
    printf("%p ", a);
    return (0);
}
#endif /* DEBUG_SRC */
