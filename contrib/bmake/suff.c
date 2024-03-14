/*	$NetBSD: suff.c,v 1.378 2024/02/07 06:43:02 rillig Exp $	*/

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

/*
 * Maintain suffix lists and find implicit dependents using suffix
 * transformation rules such as ".c.o".
 *
 * Interface:
 *	Suff_Init	Initialize the module.
 *
 *	Suff_End	Clean up the module.
 *
 *	Suff_ExtendPaths
 *			Extend the search path of each suffix to include the
 *			default search path.
 *
 *	Suff_ClearSuffixes
 *			Clear out all the suffixes and transformations.
 *
 *	Suff_IsTransform
 *			See if the passed string is a transformation rule.
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
 *			Add another transformation to the suffix graph.
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
MAKE_RCSID("$NetBSD: suff.c,v 1.378 2024/02/07 06:43:02 rillig Exp $");

typedef List SuffixList;
typedef ListNode SuffixListNode;

typedef List CandidateList;
typedef ListNode CandidateListNode;

/* The defined suffixes, such as '.c', '.o', '.l'. */
static SuffixList sufflist = LST_INIT;
#ifdef CLEANUP
/* The suffixes to be cleaned up at the end. */
static SuffixList suffClean = LST_INIT;
#endif

/*
 * The transformation rules, such as '.c.o' to transform '.c' into '.o',
 * or simply '.c' to transform 'file.c' into 'file'.
 */
static GNodeList transforms = LST_INIT;

/*
 * Counter for assigning suffix numbers.
 * TODO: What are these suffix numbers used for?
 */
static int sNum = 0;

/*
 * A suffix such as ".c" or ".o" that may be used in suffix transformation
 * rules such as ".c.o:".
 */
typedef struct Suffix {
	/* The suffix itself, such as ".c" */
	char *name;
	/* Length of the name, to avoid strlen calls */
	size_t nameLen;
	/*
	 * This suffix marks include files.  Their search path ends up in the
	 * undocumented special variable '.INCLUDES'.
	 */
	bool include:1;
	/*
	 * This suffix marks library files.  Their search path ends up in the
	 * undocumented special variable '.LIBS'.
	 */
	bool library:1;
	/*
	 * The empty suffix.
	 *
	 * XXX: What is the difference between the empty suffix and the null
	 * suffix?
	 *
	 * XXX: Why is SUFF_NULL needed at all? Wouldn't nameLen == 0 mean
	 * the same?
	 */
	bool isNull:1;
	/* The path along which files of this suffix may be found */
	SearchPath *searchPath;

	/* The suffix number; TODO: document the purpose of this number */
	int sNum;
	/* Reference count of list membership and several other places */
	int refCount;

	/* Suffixes we have a transformation to */
	SuffixList parents;
	/* Suffixes we have a transformation from */
	SuffixList children;
} Suffix;

/*
 * A candidate when searching for implied sources.
 *
 * For example, when "src.o" is to be made, a typical candidate is "src.c"
 * via the transformation rule ".c.o".  If that doesn't exist, maybe there is
 * another transformation rule ".pas.c" that would make "src.pas" an indirect
 * candidate as well.  The first such chain that leads to an existing file or
 * node is finally chosen to be made.
 */
typedef struct Candidate {
	/* The file or node to look for. */
	char *file;
	/*
	 * The prefix from which file was formed. Its memory is shared among
	 * all candidates.
	 */
	char *prefix;
	/* The suffix on the file. */
	Suffix *suff;

	/*
	 * The candidate that can be made from this, or NULL for the
	 * top-level candidate.
	 */
	struct Candidate *parent;
	/* The node describing the file. */
	GNode *node;

	/*
	 * Count of existing children, only used for memory management, so we
	 * don't free this candidate too early or too late.
	 */
	int numChildren;
#ifdef DEBUG_SRC
	CandidateList childrenList;
#endif
} Candidate;

typedef struct CandidateSearcher {

	CandidateList list;

	/*
	 * TODO: Add HashSet for seen entries, to avoid endless loops such as
	 * in suff-transform-endless.mk.
	 */

} CandidateSearcher;


/* TODO: Document the difference between nullSuff and emptySuff. */
/* The NULL suffix is used when a file has no known suffix */
static Suffix *nullSuff;
/* The empty suffix required for POSIX single-suffix transformation rules */
static Suffix *emptySuff;


static Suffix *
Suffix_Ref(Suffix *suff)
{
	suff->refCount++;
	return suff;
}

/* Change the value of a Suffix variable, adjusting the reference counts. */
static void
Suffix_Reassign(Suffix **var, Suffix *suff)
{
	if (*var != NULL)
		(*var)->refCount--;
	*var = suff;
	suff->refCount++;
}

/* Set a Suffix variable to NULL, adjusting the reference count. */
static void
Suffix_Unassign(Suffix **var)
{
	if (*var != NULL)
		(*var)->refCount--;
	*var = NULL;
}

/*
 * See if pref is a prefix of str.
 * Return NULL if it ain't, pointer to character in str after prefix if so.
 */
static const char *
StrTrimPrefix(const char *pref, const char *str)
{
	while (*str != '\0' && *pref == *str) {
		pref++;
		str++;
	}

	return *pref != '\0' ? NULL : str;
}

/*
 * See if suff is a suffix of str, and if so, return the pointer to the suffix
 * in str, which at the same time marks the end of the prefix.
 */
static const char *
StrTrimSuffix(const char *str, size_t strLen, const char *suff, size_t suffLen)
{
	const char *suffInStr;
	size_t i;

	if (strLen < suffLen)
		return NULL;

	suffInStr = str + strLen - suffLen;
	for (i = 0; i < suffLen; i++)
		if (suff[i] != suffInStr[i])
			return NULL;

	return suffInStr;
}

/*
 * See if suff is a suffix of name, and if so, return the end of the prefix
 * in name.
 */
static const char *
Suffix_TrimSuffix(const Suffix *suff, size_t nameLen, const char *nameEnd)
{
	return StrTrimSuffix(nameEnd - nameLen, nameLen,
	    suff->name, suff->nameLen);
}

static bool
Suffix_IsSuffix(const Suffix *suff, size_t nameLen, const char *nameEnd)
{
	return Suffix_TrimSuffix(suff, nameLen, nameEnd) != NULL;
}

static Suffix *
FindSuffixByNameLen(const char *name, size_t nameLen)
{
	SuffixListNode *ln;

	for (ln = sufflist.first; ln != NULL; ln = ln->next) {
		Suffix *suff = ln->datum;
		if (suff->nameLen == nameLen &&
		    memcmp(suff->name, name, nameLen) == 0)
			return suff;
	}
	return NULL;
}

static Suffix *
FindSuffixByName(const char *name)
{
	return FindSuffixByNameLen(name, strlen(name));
}

static GNode *
FindTransformByName(const char *name)
{
	GNodeListNode *ln;

	for (ln = transforms.first; ln != NULL; ln = ln->next) {
		GNode *gn = ln->datum;
		if (strcmp(gn->name, name) == 0)
			return gn;
	}
	return NULL;
}

static void
SuffixList_Unref(SuffixList *list, Suffix *suff)
{
	SuffixListNode *ln = Lst_FindDatum(list, suff);
	if (ln != NULL) {
		Lst_Remove(list, ln);
		suff->refCount--;
	}
}

static void
Suffix_Free(Suffix *suff)
{

	if (suff == nullSuff)
		nullSuff = NULL;

	if (suff == emptySuff)
		emptySuff = NULL;

#if 0
	/* We don't delete suffixes in order, so we cannot use this */
	if (suff->refCount != 0)
		Punt("Internal error deleting suffix `%s' with refcount = %d",
		    suff->name, suff->refCount);
#endif

	Lst_Done(&suff->children);
	Lst_Done(&suff->parents);
	SearchPath_Free(suff->searchPath);

	free(suff->name);
	free(suff);
}

/* Remove the suffix from the list, and free if it is otherwise unused. */
static void
SuffixList_Remove(SuffixList *list, Suffix *suff)
{
	SuffixList_Unref(list, suff);
	if (suff->refCount == 0) {
		/* XXX: can lead to suff->refCount == -1 */
		SuffixList_Unref(&sufflist, suff);
		DEBUG1(SUFF, "Removing suffix \"%s\"\n", suff->name);
		Suffix_Free(suff);
	}
}

/*
 * Insert the suffix into the list, keeping the list ordered by suffix
 * number.
 */
static void
SuffixList_Insert(SuffixList *list, Suffix *suff)
{
	SuffixListNode *ln;
	Suffix *listSuff = NULL;

	for (ln = list->first; ln != NULL; ln = ln->next) {
		listSuff = ln->datum;
		if (listSuff->sNum >= suff->sNum)
			break;
	}

	if (ln == NULL) {
		DEBUG2(SUFF, "inserting \"%s\" (%d) at end of list\n",
		    suff->name, suff->sNum);
		Lst_Append(list, Suffix_Ref(suff));
	} else if (listSuff->sNum != suff->sNum) {
		DEBUG4(SUFF, "inserting \"%s\" (%d) before \"%s\" (%d)\n",
		    suff->name, suff->sNum, listSuff->name, listSuff->sNum);
		Lst_InsertBefore(list, ln, Suffix_Ref(suff));
	} else {
		DEBUG2(SUFF, "\"%s\" (%d) is already there\n",
		    suff->name, suff->sNum);
	}
}

static void
Relate(Suffix *srcSuff, Suffix *targSuff)
{
	SuffixList_Insert(&targSuff->children, srcSuff);
	SuffixList_Insert(&srcSuff->parents, targSuff);
}

static Suffix *
Suffix_New(const char *name)
{
	Suffix *suff = bmake_malloc(sizeof *suff);

	suff->name = bmake_strdup(name);
	suff->nameLen = strlen(suff->name);
	suff->searchPath = SearchPath_New();
	Lst_Init(&suff->children);
	Lst_Init(&suff->parents);
	suff->sNum = sNum++;
	suff->include = false;
	suff->library = false;
	suff->isNull = false;
	suff->refCount = 1; /* XXX: why 1? It's not assigned anywhere yet. */

	return suff;
}

/*
 * Nuke the list of suffixes but keep all transformation rules around. The
 * transformation graph is destroyed in this process, but we leave the list
 * of rules so when a new graph is formed, the rules will remain. This
 * function is called when a line '.SUFFIXES:' with an empty suffixes list is
 * encountered in a makefile.
 */
void
Suff_ClearSuffixes(void)
{
#ifdef CLEANUP
	Lst_MoveAll(&suffClean, &sufflist);
#endif
	DEBUG0(SUFF, "Clearing all suffixes\n");
	Lst_Init(&sufflist);
	sNum = 0;
	if (nullSuff != NULL)
		Suffix_Free(nullSuff);
	emptySuff = nullSuff = Suffix_New("");

	SearchPath_AddAll(nullSuff->searchPath, &dirSearchPath);
	nullSuff->include = false;
	nullSuff->library = false;
	nullSuff->isNull = true;
}

/*
 * Parse a transformation string such as ".c.o" to find its two component
 * suffixes (the source ".c" and the target ".o").  If there are no such
 * suffixes, try a single-suffix transformation as well.
 *
 * Return true if the string is a valid transformation.
 */
static bool
ParseTransform(const char *str, Suffix **out_src, Suffix **out_targ)
{
	SuffixListNode *ln;
	Suffix *single = NULL;

	/*
	 * Loop looking first for a suffix that matches the start of the
	 * string and then for one that exactly matches the rest of it. If
	 * we can find two that meet these criteria, we've successfully
	 * parsed the string.
	 */
	for (ln = sufflist.first; ln != NULL; ln = ln->next) {
		Suffix *src = ln->datum;

		if (StrTrimPrefix(src->name, str) == NULL)
			continue;

		if (str[src->nameLen] == '\0') {
			single = src;
		} else {
			Suffix *targ = FindSuffixByName(str + src->nameLen);
			if (targ != NULL) {
				*out_src = src;
				*out_targ = targ;
				return true;
			}
		}
	}

	if (single != NULL) {
		/*
		 * There was a suffix that encompassed the entire string, so we
		 * assume it was a transformation to the null suffix (thank you
		 * POSIX; search for "single suffix" or "single-suffix").
		 *
		 * We still prefer to find a double rule over a singleton,
		 * hence we leave this check until the end.
		 *
		 * XXX: Use emptySuff over nullSuff?
		 */
		*out_src = single;
		*out_targ = nullSuff;
		return true;
	}
	return false;
}

/*
 * Return true if the given string is a transformation rule, that is, a
 * concatenation of two known suffixes such as ".c.o" or a single suffix
 * such as ".o".
 */
bool
Suff_IsTransform(const char *str)
{
	Suffix *src, *targ;

	return ParseTransform(str, &src, &targ);
}

/*
 * Add the transformation rule to the list of rules and place the
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
	Suffix *srcSuff;
	Suffix *targSuff;

	GNode *gn = FindTransformByName(name);
	if (gn == NULL) {
		/*
		 * Make a new graph node for the transformation. It will be
		 * filled in by the Parse module.
		 */
		gn = GNode_New(name);
		Lst_Append(&transforms, gn);
	} else {
		/*
		 * New specification for transformation rule. Just nuke the
		 * old list of commands so they can be filled in again. We
		 * don't actually free the commands themselves, because a
		 * given command can be attached to several different
		 * transformations.
		 */
		Lst_Done(&gn->commands);
		Lst_Init(&gn->commands);
		Lst_Done(&gn->children);
		Lst_Init(&gn->children);
	}

	gn->type = OP_TRANSFORM;

	{
		/* TODO: Avoid the redundant parsing here. */
		bool ok = ParseTransform(name, &srcSuff, &targSuff);
		assert(ok);
		/* LINTED 129 *//* expression has null effect */
		(void)ok;
	}

	/* Link the two together in the proper relationship and order. */
	DEBUG2(SUFF, "defining transformation from `%s' to `%s'\n",
	    srcSuff->name, targSuff->name);
	Relate(srcSuff, targSuff);

	return gn;
}

/*
 * Handle the finish of a transformation definition, removing the
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
	Suffix *srcSuff, *targSuff;
	SuffixList *srcSuffParents;

	if ((gn->type & OP_DOUBLEDEP) && !Lst_IsEmpty(&gn->cohorts))
		gn = gn->cohorts.last->datum;

	if (!(gn->type & OP_TRANSFORM))
		return;

	if (!Lst_IsEmpty(&gn->commands) || !Lst_IsEmpty(&gn->children)) {
		DEBUG1(SUFF, "transformation %s complete\n", gn->name);
		return;
	}

	/*
	 * SuffParseTransform() may fail for special rules which are not
	 * actual transformation rules. (e.g. .DEFAULT)
	 */
	if (!ParseTransform(gn->name, &srcSuff, &targSuff))
		return;

	DEBUG2(SUFF, "deleting incomplete transformation from `%s' to `%s'\n",
	    srcSuff->name, targSuff->name);

	/*
	 * Remember the parents since srcSuff could be deleted in
	 * SuffixList_Remove.
	 */
	srcSuffParents = &srcSuff->parents;
	SuffixList_Remove(&targSuff->children, srcSuff);
	SuffixList_Remove(srcSuffParents, targSuff);
}

/*
 * Called from Suff_AddSuffix to search through the list of
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
RebuildGraph(GNode *transform, Suffix *suff)
{
	const char *name = transform->name;
	size_t nameLen = strlen(name);
	const char *toName;

	/*
	 * See if it is a transformation from this suffix to another suffix.
	 */
	toName = StrTrimPrefix(suff->name, name);
	if (toName != NULL) {
		Suffix *to = FindSuffixByName(toName);
		if (to != NULL) {
			Relate(suff, to);
			return;
		}
	}

	/*
	 * See if it is a transformation from another suffix to this suffix.
	 */
	toName = Suffix_TrimSuffix(suff, nameLen, name + nameLen);
	if (toName != NULL) {
		Suffix *from = FindSuffixByNameLen(name,
		    (size_t)(toName - name));
		if (from != NULL)
			Relate(from, suff);
	}
}

/*
 * During Suff_AddSuffix, search through the list of existing targets and find
 * if any of the existing targets can be turned into a transformation rule.
 *
 * If such a target is found and the target is the current main target, the
 * main target is set to NULL and the next target examined (if that exists)
 * becomes the main target.
 *
 * Results:
 *	true iff a new main target has been selected.
 */
static bool
UpdateTarget(GNode *target, Suffix *suff, bool *inout_removedMain)
{
	Suffix *srcSuff, *targSuff;
	char *ptr;

	if (mainNode == NULL && *inout_removedMain &&
	    GNode_IsMainCandidate(target)) {
		DEBUG1(MAKE, "Setting main node to \"%s\"\n", target->name);
		mainNode = target;
		/*
		 * XXX: Why could it be a good idea to return true here?
		 * The main task of this function is to turn ordinary nodes
		 * into transformations, no matter whether or not a new .MAIN
		 * node has been found.
		 */
		/*
		 * XXX: Even when changing this to false, none of the existing
		 * unit tests fails.
		 */
		return true;
	}

	if (target->type == OP_TRANSFORM)
		return false;

	/*
	 * XXX: What about a transformation ".cpp.c"?  If ".c" is added as
	 * a new suffix, it seems wrong that this transformation would be
	 * skipped just because ".c" happens to be a prefix of ".cpp".
	 */
	ptr = strstr(target->name, suff->name);
	if (ptr == NULL)
		return false;

	/*
	 * XXX: In suff-rebuild.mk, in the line '.SUFFIXES: .c .b .a', this
	 * condition prevents the rule '.b.c' from being added again during
	 * Suff_AddSuffix(".b").
	 *
	 * XXX: Removing this paragraph makes suff-add-later.mk use massive
	 * amounts of memory.
	 */
	if (ptr == target->name)
		return false;

	if (ParseTransform(target->name, &srcSuff, &targSuff)) {
		if (mainNode == target) {
			DEBUG1(MAKE,
			    "Setting main node from \"%s\" back to null\n",
			    target->name);
			*inout_removedMain = true;
			mainNode = NULL;
		}
		Lst_Done(&target->children);
		Lst_Init(&target->children);
		target->type = OP_TRANSFORM;

		/*
		 * Link the two together in the proper relationship and order.
		 */
		DEBUG2(SUFF, "defining transformation from `%s' to `%s'\n",
		    srcSuff->name, targSuff->name);
		Relate(srcSuff, targSuff);
	}
	return false;
}

/*
 * Look at all existing targets to see if adding this suffix will make one
 * of the current targets mutate into a suffix rule.
 *
 * This is ugly, but other makes treat all targets that start with a '.' as
 * suffix rules.
 */
static void
UpdateTargets(Suffix *suff)
{
	bool removedMain = false;
	GNodeListNode *ln;

	for (ln = Targ_List()->first; ln != NULL; ln = ln->next) {
		GNode *gn = ln->datum;
		if (UpdateTarget(gn, suff, &removedMain))
			break;
	}
}

/* Add the suffix to the end of the list of known suffixes. */
void
Suff_AddSuffix(const char *name)
{
	GNodeListNode *ln;

	Suffix *suff = FindSuffixByName(name);
	if (suff != NULL)
		return;

	suff = Suffix_New(name);
	Lst_Append(&sufflist, suff);
	DEBUG1(SUFF, "Adding suffix \"%s\"\n", suff->name);

	UpdateTargets(suff);

	/*
	 * Look for any existing transformations from or to this suffix.
	 * XXX: Only do this after a Suff_ClearSuffixes?
	 */
	for (ln = transforms.first; ln != NULL; ln = ln->next)
		RebuildGraph(ln->datum, suff);
}

/* Return the search path for the given suffix, or NULL. */
SearchPath *
Suff_GetPath(const char *name)
{
	Suffix *suff = FindSuffixByName(name);
	return suff != NULL ? suff->searchPath : NULL;
}

/*
 * Extend the search paths for all suffixes to include the default search
 * path (dirSearchPath).
 *
 * The default search path can be defined using the special target '.PATH'.
 * The search path of each suffix can be defined using the special target
 * '.PATH<suffix>'.
 *
 * If paths were specified for the ".h" suffix, the directories are stuffed
 * into a global variable called ".INCLUDES" with each directory preceded by
 * '-I'. The same is done for the ".a" suffix, except the variable is called
 * ".LIBS" and the flag is '-L'.
 */
void
Suff_ExtendPaths(void)
{
	SuffixListNode *ln;
	char *flags;
	SearchPath *includesPath = SearchPath_New();
	SearchPath *libsPath = SearchPath_New();

	for (ln = sufflist.first; ln != NULL; ln = ln->next) {
		Suffix *suff = ln->datum;
		if (!Lst_IsEmpty(&suff->searchPath->dirs)) {
			if (suff->include)
				SearchPath_AddAll(includesPath,
				    suff->searchPath);
			if (suff->library)
				SearchPath_AddAll(libsPath, suff->searchPath);
			SearchPath_AddAll(suff->searchPath, &dirSearchPath);
		} else {
			SearchPath_Free(suff->searchPath);
			suff->searchPath = Dir_CopyDirSearchPath();
		}
	}

	flags = SearchPath_ToFlags(includesPath, "-I");
	Global_Set(".INCLUDES", flags);
	free(flags);

	flags = SearchPath_ToFlags(libsPath, "-L");
	Global_Set(".LIBS", flags);
	free(flags);

	SearchPath_Free(includesPath);
	SearchPath_Free(libsPath);
}

/*
 * Add the given suffix as a type of file which gets included.
 * Called when a '.INCLUDES: .h' line is parsed.
 * To have an effect, the suffix must already exist.
 * This affects the magic variable '.INCLUDES'.
 */
void
Suff_AddInclude(const char *suffName)
{
	Suffix *suff = FindSuffixByName(suffName);
	if (suff != NULL)
		suff->include = true;
}

/*
 * Add the given suffix as a type of file which is a library.
 * Called when a '.LIBS: .a' line is parsed.
 * To have an effect, the suffix must already exist.
 * This affects the magic variable '.LIBS'.
 */
void
Suff_AddLib(const char *suffName)
{
	Suffix *suff = FindSuffixByName(suffName);
	if (suff != NULL)
		suff->library = true;
}

/********** Implicit Source Search Functions *********/

static void
CandidateSearcher_Init(CandidateSearcher *cs)
{
	Lst_Init(&cs->list);
}

static void
CandidateSearcher_Done(CandidateSearcher *cs)
{
	Lst_Done(&cs->list);
}

static void
CandidateSearcher_Add(CandidateSearcher *cs, Candidate *cand)
{
	/* TODO: filter duplicates */
	Lst_Append(&cs->list, cand);
}

static void
CandidateSearcher_AddIfNew(CandidateSearcher *cs, Candidate *cand)
{
	/* TODO: filter duplicates */
	if (Lst_FindDatum(&cs->list, cand) == NULL)
		Lst_Append(&cs->list, cand);
}

static void
CandidateSearcher_MoveAll(CandidateSearcher *cs, CandidateList *list)
{
	/* TODO: filter duplicates */
	Lst_MoveAll(&cs->list, list);
}


#ifdef DEBUG_SRC
static void
CandidateList_PrintAddrs(CandidateList *list)
{
	CandidateListNode *ln;

	for (ln = list->first; ln != NULL; ln = ln->next) {
		Candidate *cand = ln->datum;
		debug_printf(" %p:%s", cand, cand->file);
	}
	debug_printf("\n");
}
#endif

static Candidate *
Candidate_New(char *name, char *prefix, Suffix *suff, Candidate *parent,
	      GNode *gn)
{
	Candidate *cand = bmake_malloc(sizeof *cand);

	cand->file = name;
	cand->prefix = prefix;
	cand->suff = Suffix_Ref(suff);
	cand->parent = parent;
	cand->node = gn;
	cand->numChildren = 0;
#ifdef DEBUG_SRC
	Lst_Init(&cand->childrenList);
#endif

	return cand;
}

/* Add a new candidate to the list. */
/*ARGSUSED*/
static void
CandidateList_Add(CandidateList *list, char *srcName, Candidate *targ,
		  Suffix *suff, const char *debug_tag MAKE_ATTR_UNUSED)
{
	Candidate *cand = Candidate_New(srcName, targ->prefix, suff, targ,
	    NULL);
	targ->numChildren++;
	Lst_Append(list, cand);

#ifdef DEBUG_SRC
	Lst_Append(&targ->childrenList, cand);
	debug_printf("%s add suff %p:%s candidate %p:%s to list %p:",
	    debug_tag, targ, targ->file, cand, cand->file, list);
	CandidateList_PrintAddrs(list);
#endif
}

/*
 * Add all candidates to the list that can be formed by applying a suffix to
 * the candidate.
 */
static void
CandidateList_AddCandidatesFor(CandidateList *list, Candidate *cand)
{
	SuffixListNode *ln;
	for (ln = cand->suff->children.first; ln != NULL; ln = ln->next) {
		Suffix *suff = ln->datum;

		if (suff->isNull && suff->name[0] != '\0') {
			/*
			 * If the suffix has been marked as the NULL suffix,
			 * also create a candidate for a file with no suffix
			 * attached.
			 */
			CandidateList_Add(list, bmake_strdup(cand->prefix),
			    cand, suff, "1");
		}

		CandidateList_Add(list, str_concat2(cand->prefix, suff->name),
		    cand, suff, "2");
	}
}

/*
 * Free the first candidate in the list that is not referenced anymore.
 * Return whether a candidate was removed.
 */
static bool
RemoveCandidate(CandidateList *srcs)
{
	CandidateListNode *ln;

#ifdef DEBUG_SRC
	debug_printf("cleaning list %p:", srcs);
	CandidateList_PrintAddrs(srcs);
#endif

	for (ln = srcs->first; ln != NULL; ln = ln->next) {
		Candidate *src = ln->datum;

		if (src->numChildren == 0) {
			if (src->parent == NULL)
				free(src->prefix);
			else {
#ifdef DEBUG_SRC
				/* XXX: Lst_RemoveDatum */
				CandidateListNode *ln2;
				ln2 = Lst_FindDatum(&src->parent->childrenList,
				    src);
				if (ln2 != NULL)
					Lst_Remove(&src->parent->childrenList,
					    ln2);
#endif
				src->parent->numChildren--;
			}
#ifdef DEBUG_SRC
			debug_printf("free: list %p src %p:%s children %d\n",
			    srcs, src, src->file, src->numChildren);
			Lst_Done(&src->childrenList);
#endif
			Lst_Remove(srcs, ln);
			free(src->file);
			free(src);
			return true;
		}
#ifdef DEBUG_SRC
		else {
			debug_printf("keep: list %p src %p:%s children %d:",
			    srcs, src, src->file, src->numChildren);
			CandidateList_PrintAddrs(&src->childrenList);
		}
#endif
	}

	return false;
}

/* Find the first existing file/target in srcs. */
static Candidate *
FindThem(CandidateList *srcs, CandidateSearcher *cs)
{
	HashSet seen;

	HashSet_Init(&seen);

	while (!Lst_IsEmpty(srcs)) {
		Candidate *src = Lst_Dequeue(srcs);

#ifdef DEBUG_SRC
		debug_printf("remove from list %p src %p:%s\n",
		    srcs, src, src->file);
#endif
		DEBUG1(SUFF, "\ttrying %s...", src->file);

		/*
		 * A file is considered to exist if either a node exists in the
		 * graph for it or the file actually exists.
		 */
		if (Targ_FindNode(src->file) != NULL) {
		found:
			HashSet_Done(&seen);
			DEBUG0(SUFF, "got it\n");
			return src;
		}

		{
			char *file = Dir_FindFile(src->file,
			    src->suff->searchPath);
			if (file != NULL) {
				free(file);
				goto found;
			}
		}

		DEBUG0(SUFF, "not there\n");

		if (HashSet_Add(&seen, src->file))
			CandidateList_AddCandidatesFor(srcs, src);
		else {
			DEBUG1(SUFF, "FindThem: skipping duplicate \"%s\"\n",
			    src->file);
		}

		CandidateSearcher_Add(cs, src);
	}

	HashSet_Done(&seen);
	return NULL;
}

/*
 * See if any of the children of the candidate's GNode is one from which the
 * target can be transformed. If there is one, a candidate is put together
 * for it and returned.
 */
static Candidate *
FindCmds(Candidate *targ, CandidateSearcher *cs)
{
	GNodeListNode *gln;
	GNode *tgn;		/* Target GNode */
	GNode *sgn;		/* Source GNode */
	size_t prefLen;		/* The length of the defined prefix */
	Suffix *suff;		/* Suffix of the matching candidate */
	Candidate *ret;		/* Return value */

	tgn = targ->node;
	prefLen = strlen(targ->prefix);

	for (gln = tgn->children.first; gln != NULL; gln = gln->next) {
		const char *base;

		sgn = gln->datum;

		if (sgn->type & OP_OPTIONAL && Lst_IsEmpty(&tgn->commands)) {
			/*
			 * We haven't looked to see if .OPTIONAL files exist
			 * yet, so don't use one as the implicit source.
			 * This allows us to use .OPTIONAL in .depend files so
			 * make won't complain "don't know how to make xxx.h"
			 * when a dependent file has been moved/deleted.
			 */
			continue;
		}

		base = str_basename(sgn->name);
		if (strncmp(base, targ->prefix, prefLen) != 0)
			continue;
		/*
		 * The node matches the prefix, see if it has a known suffix.
		 */
		suff = FindSuffixByName(base + prefLen);
		if (suff == NULL)
			continue;

		/*
		 * It even has a known suffix, see if there's a transformation
		 * defined between the node's suffix and the target's suffix.
		 *
		 * XXX: Handle multi-stage transformations here, too.
		 */

		if (Lst_FindDatum(&suff->parents, targ->suff) != NULL)
			break;
	}

	if (gln == NULL)
		return NULL;

	ret = Candidate_New(bmake_strdup(sgn->name), targ->prefix, suff, targ,
	    sgn);
	targ->numChildren++;
#ifdef DEBUG_SRC
	debug_printf("3 add targ %p:%s ret %p:%s\n",
	    targ, targ->file, ret, ret->file);
	Lst_Append(&targ->childrenList, ret);
#endif
	CandidateSearcher_Add(cs, ret);
	DEBUG1(SUFF, "\tusing existing source %s\n", sgn->name);
	return ret;
}

static void
ExpandWildcards(GNodeListNode *cln, GNode *pgn)
{
	GNode *cgn = cln->datum;
	StringList expansions;

	if (!Dir_HasWildcards(cgn->name))
		return;

	/* Expand the word along the chosen path. */
	Lst_Init(&expansions);
	SearchPath_Expand(Suff_FindPath(cgn), cgn->name, &expansions);

	while (!Lst_IsEmpty(&expansions)) {
		GNode *gn;
		/*
		 * Fetch next expansion off the list and find its GNode
		 */
		char *name = Lst_Dequeue(&expansions);

		DEBUG1(SUFF, "%s...", name);
		gn = Targ_GetNode(name);

		/* Insert gn before the original child. */
		Lst_InsertBefore(&pgn->children, cln, gn);
		Lst_Append(&gn->parents, pgn);
		pgn->unmade++;
	}

	Lst_Done(&expansions);

	DEBUG0(SUFF, "\n");

	/*
	 * Now that the source is expanded, remove it from the list of
	 * children, to keep it from being processed.
	 */
	pgn->unmade--;
	Lst_Remove(&pgn->children, cln);
	Lst_Remove(&cgn->parents, Lst_FindDatum(&cgn->parents, pgn));
}

/*
 * Break the result into a vector of strings whose nodes we can find, then
 * add those nodes to the members list.
 *
 * Unfortunately, we can't use Str_Words because it doesn't understand about
 * expressions with spaces in them.
 */
static void
ExpandChildrenRegular(char *p, GNode *pgn, GNodeList *members)
{
	char *start;

	pp_skip_hspace(&p);
	start = p;
	while (*p != '\0') {
		if (*p == ' ' || *p == '\t') {
			GNode *gn;
			/*
			 * White-space -- terminate element, find the node,
			 * add it, skip any further spaces.
			 */
			*p++ = '\0';
			gn = Targ_GetNode(start);
			Lst_Append(members, gn);
			pp_skip_hspace(&p);
			/* Continue at the next non-space. */
			start = p;
		} else if (*p == '$') {
			/* Skip over the expression. */
			const char *nested_p = p;
			FStr junk = Var_Parse(&nested_p, pgn, VARE_PARSE_ONLY);
			/* TODO: handle errors */
			if (junk.str == var_Error) {
				Parse_Error(PARSE_FATAL,
				    "Malformed expression at \"%s\"", p);
				p++;
			} else {
				p += nested_p - p;
			}

			FStr_Done(&junk);
		} else if (p[0] == '\\' && p[1] != '\0') {
			/* Escaped something -- skip over it. */
			/*
			 * XXX: In other places, escaping at this syntactical
			 * position is done by a '$', not a '\'.  The '\' is
			 * only used in variable modifiers.
			 */
			p += 2;
		} else {
			p++;
		}
	}

	if (p != start) {
		/*
		 * Stuff left over -- add it to the list too
		 */
		GNode *gn = Targ_GetNode(start);
		Lst_Append(members, gn);
	}
}

/*
 * Expand the names of any children of a given node that contain
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
ExpandChildren(GNodeListNode *cln, GNode *pgn)
{
	GNode *cgn = cln->datum;
	char *expanded;

	if (!Lst_IsEmpty(&cgn->order_pred) || !Lst_IsEmpty(&cgn->order_succ))
		/* It is all too hard to process the result of .ORDER */
		return;

	if (cgn->type & OP_WAIT)
		/* Ignore these (& OP_PHONY ?) */
		return;

	/*
	 * First do variable expansion -- this takes precedence over wildcard
	 * expansion. If the result contains wildcards, they'll be gotten to
	 * later since the resulting words are tacked on to the end of the
	 * children list.
	 */
	if (strchr(cgn->name, '$') == NULL) {
		ExpandWildcards(cln, pgn);
		return;
	}

	DEBUG1(SUFF, "Expanding \"%s\"...", cgn->name);
	expanded = Var_Subst(cgn->name, pgn, VARE_UNDEFERR);
	/* TODO: handle errors */

	{
		GNodeList members = LST_INIT;

		if (cgn->type & OP_ARCHV) {
			/*
			 * Node was an 'archive(member)' target, so
			 * call on the Arch module to find the nodes for us,
			 * expanding variables in the parent's scope.
			 */
			char *ap = expanded;
			(void)Arch_ParseArchive(&ap, &members, pgn);
		} else {
			ExpandChildrenRegular(expanded, pgn, &members);
		}

		/* Add all members to the parent node. */
		while (!Lst_IsEmpty(&members)) {
			GNode *gn = Lst_Dequeue(&members);

			DEBUG1(SUFF, "%s...", gn->name);
			Lst_InsertBefore(&pgn->children, cln, gn);
			Lst_Append(&gn->parents, pgn);
			pgn->unmade++;
			ExpandWildcards(cln->prev, pgn);
		}
		Lst_Done(&members);

		free(expanded);
	}

	DEBUG0(SUFF, "\n");

	/*
	 * The source is expanded now, so remove it from the list of children,
	 * to keep it from being processed.
	 */
	pgn->unmade--;
	Lst_Remove(&pgn->children, cln);
	Lst_Remove(&cgn->parents, Lst_FindDatum(&cgn->parents, pgn));
}

static void
ExpandAllChildren(GNode *gn)
{
	GNodeListNode *ln, *nln;

	for (ln = gn->children.first; ln != NULL; ln = nln) {
		nln = ln->next;
		ExpandChildren(ln, gn);
	}
}

/*
 * Find a path along which to search or expand the node.
 *
 * If the node has a known suffix, use that path,
 * otherwise use the default system search path.
 */
SearchPath *
Suff_FindPath(GNode *gn)
{
	Suffix *suff = gn->suffix;

	if (suff == NULL) {
		char *name = gn->name;
		size_t nameLen = strlen(gn->name);
		SuffixListNode *ln;
		for (ln = sufflist.first; ln != NULL; ln = ln->next)
			if (Suffix_IsSuffix(ln->datum, nameLen, name + nameLen))
				break;

		DEBUG1(SUFF, "Wildcard expanding \"%s\"...", gn->name);
		if (ln != NULL)
			suff = ln->datum;
		/*
		 * XXX: Here we can save the suffix so we don't have to do
		 * this again.
		 */
	}

	if (suff != NULL) {
		DEBUG1(SUFF, "suffix is \"%s\"...\n", suff->name);
		return suff->searchPath;
	} else {
		DEBUG0(SUFF, "\n");
		return &dirSearchPath;	/* Use default search path */
	}
}

/*
 * Apply a transformation rule, given the source and target nodes and
 * suffixes.
 *
 * The source and target are linked and the commands from the transformation
 * are added to the target node's commands list. The target also inherits all
 * the sources for the transformation rule.
 *
 * Results:
 *	true if successful, false if not.
 */
static bool
ApplyTransform(GNode *tgn, GNode *sgn, Suffix *tsuff, Suffix *ssuff)
{
	GNodeListNode *ln;
	char *tname;		/* Name of transformation rule */
	GNode *gn;		/* Node for the transformation rule */

	/* Form the proper links between the target and source. */
	Lst_Append(&tgn->children, sgn);
	Lst_Append(&sgn->parents, tgn);
	tgn->unmade++;

	/* Locate the transformation rule itself. */
	tname = str_concat2(ssuff->name, tsuff->name);
	gn = FindTransformByName(tname);
	free(tname);

	/* This can happen when linking an OP_MEMBER and OP_ARCHV node. */
	if (gn == NULL)
		return false;

	DEBUG3(SUFF, "\tapplying %s -> %s to \"%s\"\n",
	    ssuff->name, tsuff->name, tgn->name);

	/* Record last child; Make_HandleUse may add child nodes. */
	ln = tgn->children.last;

	/* Apply the rule. */
	Make_HandleUse(gn, tgn);

	/* Deal with wildcards and expressions in any acquired sources. */
	ln = ln != NULL ? ln->next : NULL;
	while (ln != NULL) {
		GNodeListNode *nln = ln->next;
		ExpandChildren(ln, tgn);
		ln = nln;
	}

	/*
	 * Keep track of another parent to which this node is transformed so
	 * the .IMPSRC variable can be set correctly for the parent.
	 */
	Lst_Append(&sgn->implicitParents, tgn);

	return true;
}

/*
 * Member has a known suffix, so look for a transformation rule from
 * it to a possible suffix of the archive.
 *
 * Rather than searching through the entire list, we just look at
 * suffixes to which the member's suffix may be transformed.
 */
static void
ExpandMember(GNode *gn, const char *eoarch, GNode *mem, Suffix *memSuff)
{
	SuffixListNode *ln;
	size_t nameLen = (size_t)(eoarch - gn->name);

	/* Use first matching suffix... */
	for (ln = memSuff->parents.first; ln != NULL; ln = ln->next)
		if (Suffix_IsSuffix(ln->datum, nameLen, eoarch))
			break;

	if (ln != NULL) {
		Suffix *suff = ln->datum;
		if (!ApplyTransform(gn, mem, suff, memSuff)) {
			DEBUG2(SUFF, "\tNo transformation from %s -> %s\n",
			    memSuff->name, suff->name);
		}
	}
}

static void FindDeps(GNode *, CandidateSearcher *);

/*
 * Locate dependencies for an OP_ARCHV node.
 *
 * Side Effects:
 *	Same as Suff_FindDeps
 */
static void
FindDepsArchive(GNode *gn, CandidateSearcher *cs)
{
	char *eoarch;		/* End of archive portion */
	char *eoname;		/* End of member portion */
	GNode *mem;		/* Node for member */
	Suffix *memSuff;
	const char *name;	/* Start of member's name */

	/*
	 * The node is an 'archive(member)' pair, so we must find a
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

	*eoname = '\0';		/* Nuke parentheses during suffix search */
	*eoarch = '\0';		/* So a suffix can be found */

	name = eoarch + 1;

	/*
	 * To simplify things, call Suff_FindDeps recursively on the member
	 * now, so we can simply compare the member's .PREFIX and .TARGET
	 * variables to locate its suffix. This allows us to figure out the
	 * suffix to use for the archive without having to do a quadratic
	 * search over the suffix list, backtracking for each one.
	 */
	mem = Targ_GetNode(name);
	FindDeps(mem, cs);

	/* Create the link between the two nodes right off. */
	Lst_Append(&gn->children, mem);
	Lst_Append(&mem->parents, gn);
	gn->unmade++;

	/* Copy in the variables from the member node to this one. */
	Var_Set(gn, PREFIX, GNode_VarPrefix(mem));
	Var_Set(gn, TARGET, GNode_VarTarget(mem));

	memSuff = mem->suffix;
	if (memSuff == NULL) {	/* Didn't know what it was. */
		DEBUG0(SUFF, "using null suffix\n");
		memSuff = nullSuff;
	}


	/* Set the other two local variables required for this target. */
	Var_Set(gn, MEMBER, name);
	Var_Set(gn, ARCHIVE, gn->name);
	/* Set $@ for compatibility with other makes. */
	Var_Set(gn, TARGET, gn->name);

	/*
	 * Now we've got the important local variables set, expand any sources
	 * that still contain variables or wildcards in their names.
	 */
	ExpandAllChildren(gn);

	if (memSuff != NULL)
		ExpandMember(gn, eoarch, mem, memSuff);

	/*
	 * Replace the opening and closing parens now we've no need of the
	 * separate pieces.
	 */
	*eoarch = '(';
	*eoname = ')';

	/*
	 * Pretend gn appeared to the left of a dependency operator so the
	 * user needn't provide a transformation from the member to the
	 * archive.
	 */
	if (!GNode_IsTarget(gn))
		gn->type |= OP_DEPENDS;

	/*
	 * Flag the member as such so we remember to look in the archive for
	 * its modification time. The OP_JOIN | OP_MADE is needed because
	 * this target should never get made.
	 */
	mem->type |= OP_MEMBER | OP_JOIN | OP_MADE;
}

/*
 * If the node is a library, it is the arch module's job to find it
 * and set the TARGET variable accordingly. We merely provide the
 * search path, assuming all libraries end in ".a" (if the suffix
 * hasn't been defined, there's nothing we can do for it, so we just
 * set the TARGET variable to the node's name in order to give it a
 * value).
 */
static void
FindDepsLib(GNode *gn)
{
	Suffix *suff = FindSuffixByName(LIBSUFF);
	if (suff != NULL) {
		Suffix_Reassign(&gn->suffix, suff);
		Arch_FindLib(gn, suff->searchPath);
	} else {
		Suffix_Unassign(&gn->suffix);
		Var_Set(gn, TARGET, gn->name);
	}

	/*
	 * Because a library (-lfoo) target doesn't follow the standard
	 * filesystem conventions, we don't set the regular variables for
	 * the thing. .PREFIX is simply made empty.
	 */
	Var_Set(gn, PREFIX, "");
}

static void
FindDepsRegularKnown(const char *name, size_t nameLen, GNode *gn,
		     CandidateList *srcs, CandidateList *targs)
{
	SuffixListNode *ln;
	Candidate *targ;
	char *pref;

	for (ln = sufflist.first; ln != NULL; ln = ln->next) {
		Suffix *suff = ln->datum;
		if (!Suffix_IsSuffix(suff, nameLen, name + nameLen))
			continue;

		pref = bmake_strldup(name, (size_t)(nameLen - suff->nameLen));
		targ = Candidate_New(bmake_strdup(gn->name), pref, suff, NULL,
		    gn);

		CandidateList_AddCandidatesFor(srcs, targ);

		/* Record the target so we can nuke it. */
		Lst_Append(targs, targ);
	}
}

static void
FindDepsRegularUnknown(GNode *gn, const char *sopref,
		       CandidateList *srcs, CandidateList *targs)
{
	Candidate *targ;

	if (!Lst_IsEmpty(targs) || nullSuff == NULL)
		return;

	DEBUG1(SUFF, "\tNo known suffix on %s. Using .NULL suffix\n", gn->name);

	targ = Candidate_New(bmake_strdup(gn->name), bmake_strdup(sopref),
	    nullSuff, NULL, gn);

	/*
	 * Only use the default suffix rules if we don't have commands
	 * defined for this gnode; traditional make programs used to not
	 * define suffix rules if the gnode had children but we don't do
	 * this anymore.
	 */
	if (Lst_IsEmpty(&gn->commands))
		CandidateList_AddCandidatesFor(srcs, targ);
	else {
		DEBUG0(SUFF, "not ");
	}

	DEBUG0(SUFF, "adding suffix rules\n");

	Lst_Append(targs, targ);
}

/*
 * Deal with finding the thing on the default search path. We always do
 * that, not only if the node is only a source (not on the lhs of a
 * dependency operator or [XXX] it has neither children or commands) as
 * the old pmake did.
 */
static void
FindDepsRegularPath(GNode *gn, Candidate *targ)
{
	if (gn->type & (OP_PHONY | OP_NOPATH))
		return;

	free(gn->path);
	gn->path = Dir_FindFile(gn->name,
	    targ == NULL ? &dirSearchPath : targ->suff->searchPath);
	if (gn->path == NULL)
		return;

	Var_Set(gn, TARGET, gn->path);

	if (targ != NULL) {
		/*
		 * Suffix known for the thing -- trim the suffix off
		 * the path to form the proper .PREFIX variable.
		 */
		size_t savep = strlen(gn->path) - targ->suff->nameLen;
		char savec;

		Suffix_Reassign(&gn->suffix, targ->suff);

		savec = gn->path[savep];
		gn->path[savep] = '\0';

		Var_Set(gn, PREFIX, str_basename(gn->path));

		gn->path[savep] = savec;
	} else {
		/*
		 * The .PREFIX gets the full path if the target has no
		 * known suffix.
		 */
		Suffix_Unassign(&gn->suffix);
		Var_Set(gn, PREFIX, str_basename(gn->path));
	}
}

/*
 * Locate implicit dependencies for regular targets.
 *
 * Input:
 *	gn		Node for which to find sources
 *
 * Side Effects:
 *	Same as Suff_FindDeps
 */
static void
FindDepsRegular(GNode *gn, CandidateSearcher *cs)
{
	/* List of sources at which to look */
	CandidateList srcs = LST_INIT;
	/*
	 * List of targets to which things can be transformed.
	 * They all have the same file, but different suff and prefix fields.
	 */
	CandidateList targs = LST_INIT;
	Candidate *bottom;	/* Start of found transformation path */
	Candidate *src;
	Candidate *targ;

	const char *name = gn->name;
	size_t nameLen = strlen(name);

#ifdef DEBUG_SRC
	DEBUG1(SUFF, "FindDepsRegular \"%s\"\n", gn->name);
#endif

	/*
	 * We're caught in a catch-22 here. On the one hand, we want to use
	 * any transformation implied by the target's sources, but we can't
	 * examine the sources until we've expanded any variables/wildcards
	 * they may hold, and we can't do that until we've set up the
	 * target's local variables and we can't do that until we know what
	 * the proper suffix for the target is (in case there are two
	 * suffixes one of which is a suffix of the other) and we can't know
	 * that until we've found its implied source, which we may not want
	 * to use if there's an existing source that implies a different
	 * transformation.
	 *
	 * In an attempt to get around this, which may not work all the time,
	 * but should work most of the time, we look for implied sources
	 * first, checking transformations to all possible suffixes of the
	 * target, use what we find to set the target's local variables,
	 * expand the children, then look for any overriding transformations
	 * they imply. Should we find one, we discard the one we found before.
	 */
	bottom = NULL;
	targ = NULL;

	if (!(gn->type & OP_PHONY)) {

		FindDepsRegularKnown(name, nameLen, gn, &srcs, &targs);

		/* Handle target of unknown suffix... */
		FindDepsRegularUnknown(gn, name, &srcs, &targs);

		/*
		 * Using the list of possible sources built up from the target
		 * suffix(es), try and find an existing file/target that
		 * matches.
		 */
		bottom = FindThem(&srcs, cs);

		if (bottom == NULL) {
			/*
			 * No known transformations -- use the first suffix
			 * found for setting the local variables.
			 */
			if (targs.first != NULL)
				targ = targs.first->datum;
			else
				targ = NULL;
		} else {
			/*
			 * Work up the transformation path to find the suffix
			 * of the target to which the transformation was made.
			 */
			for (targ = bottom;
			     targ->parent != NULL; targ = targ->parent)
				continue;
		}
	}

	Var_Set(gn, TARGET, GNode_Path(gn));
	Var_Set(gn, PREFIX, targ != NULL ? targ->prefix : gn->name);

	/*
	 * Now we've got the important local variables set, expand any sources
	 * that still contain variables or wildcards in their names.
	 */
	{
		GNodeListNode *ln, *nln;
		for (ln = gn->children.first; ln != NULL; ln = nln) {
			nln = ln->next;
			ExpandChildren(ln, gn);
		}
	}

	if (targ == NULL) {
		DEBUG1(SUFF, "\tNo valid suffix on %s\n", gn->name);

	sfnd_abort:
		FindDepsRegularPath(gn, targ);
		goto sfnd_return;
	}

	/*
	 * If the suffix indicates that the target is a library, mark that in
	 * the node's type field.
	 */
	if (targ->suff->library)
		gn->type |= OP_LIB;

	/*
	 * Check for overriding transformation rule implied by sources
	 */
	if (!Lst_IsEmpty(&gn->children)) {
		src = FindCmds(targ, cs);

		if (src != NULL) {
			/*
			 * Free up all the candidates in the transformation
			 * path, up to but not including the parent node.
			 */
			while (bottom != NULL && bottom->parent != NULL) {
				CandidateSearcher_AddIfNew(cs, bottom);
				bottom = bottom->parent;
			}
			bottom = src;
		}
	}

	if (bottom == NULL) {
		/* No idea from where it can come -- return now. */
		goto sfnd_abort;
	}

	/*
	 * We now have a list of candidates headed by 'bottom' and linked via
	 * their 'parent' pointers. What we do next is create links between
	 * source and target nodes (which may or may not have been created)
	 * and set the necessary local variables in each target.
	 *
	 * The commands for each target are set from the commands of the
	 * transformation rule used to get from the src suffix to the targ
	 * suffix. Note that this causes the commands list of the original
	 * node, gn, to be replaced with the commands of the final
	 * transformation rule.
	 */
	if (bottom->node == NULL)
		bottom->node = Targ_GetNode(bottom->file);

	for (src = bottom; src->parent != NULL; src = src->parent) {
		targ = src->parent;

		Suffix_Reassign(&src->node->suffix, src->suff);

		if (targ->node == NULL)
			targ->node = Targ_GetNode(targ->file);

		ApplyTransform(targ->node, src->node, targ->suff, src->suff);

		if (targ->node != gn) {
			/*
			 * Finish off the dependency-search process for any
			 * nodes between bottom and gn (no point in questing
			 * around the filesystem for their implicit source
			 * when it's already known). Note that the node
			 * can't have any sources that need expanding, since
			 * SuffFindThem will stop on an existing node, so all
			 * we need to do is set the standard variables.
			 */
			targ->node->type |= OP_DEPS_FOUND;
			Var_Set(targ->node, PREFIX, targ->prefix);
			Var_Set(targ->node, TARGET, targ->node->name);
		}
	}

	Suffix_Reassign(&gn->suffix, src->suff);

	/*
	 * Nuke the transformation path and the candidates left over in the
	 * two lists.
	 */
sfnd_return:
	if (bottom != NULL)
		CandidateSearcher_AddIfNew(cs, bottom);

	while (RemoveCandidate(&srcs) || RemoveCandidate(&targs))
		continue;

	CandidateSearcher_MoveAll(cs, &srcs);
	CandidateSearcher_MoveAll(cs, &targs);
}

static void
CandidateSearcher_CleanUp(CandidateSearcher *cs)
{
	while (RemoveCandidate(&cs->list))
		continue;
	assert(Lst_IsEmpty(&cs->list));
}


/*
 * Find implicit sources for the target.
 *
 * Nodes are added to the graph as children of the passed-in node. The nodes
 * are marked to have their IMPSRC variable filled in. The PREFIX variable
 * is set for the given node and all its implied children.
 *
 * The path found by this target is the shortest path in the transformation
 * graph, which may pass through nonexistent targets, to an existing target.
 * The search continues on all paths from the root suffix until a file is
 * found. I.e. if there's a path .o -> .c -> .l -> .l,v from the root and the
 * .l,v file exists but the .c and .l files don't, the search will branch out
 * in all directions from .o and again from all the nodes on the next level
 * until the .l,v node is encountered.
 */
void
Suff_FindDeps(GNode *gn)
{
	CandidateSearcher cs;

	CandidateSearcher_Init(&cs);

	FindDeps(gn, &cs);

	CandidateSearcher_CleanUp(&cs);
	CandidateSearcher_Done(&cs);
}

static void
FindDeps(GNode *gn, CandidateSearcher *cs)
{
	if (gn->type & OP_DEPS_FOUND)
		return;
	gn->type |= OP_DEPS_FOUND;

	/* Make sure we have these set, may get revised below. */
	Var_Set(gn, TARGET, GNode_Path(gn));
	Var_Set(gn, PREFIX, gn->name);

	DEBUG1(SUFF, "SuffFindDeps \"%s\"\n", gn->name);

	if (gn->type & OP_ARCHV)
		FindDepsArchive(gn, cs);
	else if (gn->type & OP_LIB)
		FindDepsLib(gn);
	else
		FindDepsRegular(gn, cs);
}

/*
 * Define which suffix is the null suffix.
 *
 * Need to handle the changing of the null suffix gracefully so the old
 * transformation rules don't just go away.
 */
void
Suff_SetNull(const char *name)
{
	Suffix *suff = FindSuffixByName(name);
	if (suff == NULL) {
		Parse_Error(PARSE_WARNING,
		    "Desired null suffix %s not defined",
		    name);
		return;
	}

	if (nullSuff != NULL)
		nullSuff->isNull = false;
	suff->isNull = true;
	/* XXX: Here's where the transformation mangling would take place. */
	nullSuff = suff;
}

/* Initialize the suffixes module. */
void
Suff_Init(void)
{
	/*
	 * Create null suffix for single-suffix rules (POSIX). The thing
	 * doesn't actually go on the suffix list or everyone will think
	 * that's its suffix.
	 */
	Suff_ClearSuffixes();
}

/* Clean up the suffixes module. */
void
Suff_End(void)
{
#ifdef CLEANUP
	SuffixListNode *ln;

	for (ln = sufflist.first; ln != NULL; ln = ln->next)
		Suffix_Free(ln->datum);
	Lst_Done(&sufflist);
	for (ln = suffClean.first; ln != NULL; ln = ln->next)
		Suffix_Free(ln->datum);
	Lst_Done(&suffClean);
	if (nullSuff != NULL)
		Suffix_Free(nullSuff);
	Lst_Done(&transforms);
#endif
}


static void
PrintSuffNames(const char *prefix, const SuffixList *suffs)
{
	SuffixListNode *ln;

	debug_printf("#\t%s: ", prefix);
	for (ln = suffs->first; ln != NULL; ln = ln->next) {
		const Suffix *suff = ln->datum;
		debug_printf("%s ", suff->name);
	}
	debug_printf("\n");
}

static void
Suffix_Print(const Suffix *suff)
{
	Buffer buf;

	Buf_Init(&buf);
	Buf_AddFlag(&buf, suff->include, "SUFF_INCLUDE");
	Buf_AddFlag(&buf, suff->library, "SUFF_LIBRARY");
	Buf_AddFlag(&buf, suff->isNull, "SUFF_NULL");

	debug_printf("# \"%s\" (num %d, ref %d)",
	    suff->name, suff->sNum, suff->refCount);
	if (buf.len > 0)
		debug_printf(" (%s)", buf.data);
	debug_printf("\n");

	Buf_Done(&buf);

	PrintSuffNames("To", &suff->parents);
	PrintSuffNames("From", &suff->children);

	debug_printf("#\tSearch Path: ");
	SearchPath_Print(suff->searchPath);
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
		SuffixListNode *ln;
		for (ln = sufflist.first; ln != NULL; ln = ln->next)
			Suffix_Print(ln->datum);
	}

	debug_printf("#*** Transformations:\n");
	{
		GNodeListNode *ln;
		for (ln = transforms.first; ln != NULL; ln = ln->next)
			PrintTransformation(ln->datum);
	}
}

char *
Suff_NamesStr(void)
{
	Buffer buf;
	SuffixListNode *ln;
	Suffix *suff;

	Buf_Init(&buf);
	for (ln = sufflist.first; ln != NULL; ln = ln->next) {
		suff = ln->datum;
		if (ln != sufflist.first)
			Buf_AddByte(&buf, ' ');
		Buf_AddStr(&buf, suff->name);
	}
	return Buf_DoneData(&buf);
}
