/*	$NetBSD: dir.c,v 1.193 2020/10/31 17:39:20 rillig Exp $	*/

/*
 * Copyright (c) 1988, 1989, 1990 The Regents of the University of California.
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
 * Copyright (c) 1988, 1989 by Adam de Boor
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

/* Directory searching using wildcards and/or normal names.
 * Used both for source wildcarding in the makefile and for finding
 * implicit sources.
 *
 * The interface for this module is:
 *	Dir_Init	Initialize the module.
 *
 *	Dir_InitCur	Set the cur CachedDir.
 *
 *	Dir_InitDot	Set the dot CachedDir.
 *
 *	Dir_End		Clean up the module.
 *
 *	Dir_SetPATH	Set ${.PATH} to reflect state of dirSearchPath.
 *
 *	Dir_HasWildcards
 *			Returns TRUE if the name given it needs to
 *			be wildcard-expanded.
 *
 *	Dir_Expand	Given a pattern and a path, return a Lst of names
 *			which match the pattern on the search path.
 *
 *	Dir_FindFile	Searches for a file on a given search path.
 *			If it exists, the entire path is returned.
 *			Otherwise NULL is returned.
 *
 *	Dir_FindHereOrAbove
 *			Search for a path in the current directory and
 *			then all the directories above it in turn until
 *			the path is found or we reach the root ("/").
 *
 *	Dir_MTime	Return the modification time of a node. The file
 *			is searched for along the default search path.
 *			The path and mtime fields of the node are filled in.
 *
 *	Dir_AddDir	Add a directory to a search path.
 *
 *	Dir_MakeFlags	Given a search path and a command flag, create
 *			a string with each of the directories in the path
 *			preceded by the command flag and all of them
 *			separated by a space.
 *
 *	Dir_Destroy	Destroy an element of a search path. Frees up all
 *			things that can be freed for the element as long
 *			as the element is no longer referenced by any other
 *			search path.
 *
 *	Dir_ClearPath	Resets a search path to the empty list.
 *
 * For debugging:
 *	Dir_PrintDirectories
 *			Print stats about the directory cache.
 */

#include <sys/types.h>
#include <sys/stat.h>

#include <dirent.h>
#include <errno.h>

#include "make.h"
#include "dir.h"
#include "job.h"

/*	"@(#)dir.c	8.2 (Berkeley) 1/2/94"	*/
MAKE_RCSID("$NetBSD: dir.c,v 1.193 2020/10/31 17:39:20 rillig Exp $");

#define DIR_DEBUG0(text) DEBUG0(DIR, text)
#define DIR_DEBUG1(fmt, arg1) DEBUG1(DIR, fmt, arg1)
#define DIR_DEBUG2(fmt, arg1, arg2) DEBUG2(DIR, fmt, arg1, arg2)

/* A search path is a list of CachedDir structures. A CachedDir has in it the
 * name of the directory and the names of all the files in the directory.
 * This is used to cut down on the number of system calls necessary to find
 * implicit dependents and their like. Since these searches are made before
 * any actions are taken, we need not worry about the directory changing due
 * to creation commands. If this hampers the style of some makefiles, they
 * must be changed.
 *
 * All previously-read directories are kept in openDirs, which is checked
 * first before a directory is opened.
 *
 * The need for the caching of whole directories is brought about by the
 * multi-level transformation code in suff.c, which tends to search for far
 * more files than regular make does. In the initial implementation, the
 * amount of time spent performing "stat" calls was truly astronomical.
 * The problem with caching at the start is, of course, that pmake doesn't
 * then detect changes to these directories during the course of the make.
 * Three possibilities suggest themselves:
 *
 * 1)	just use stat to test for a file's existence. As mentioned above,
 *	this is very inefficient due to the number of checks engendered by
 *	the multi-level transformation code.
 *
 * 2)	use readdir() and company to search the directories, keeping them
 *	open between checks. I have tried this and while it didn't slow down
 *	the process too much, it could severely affect the amount of
 *	parallelism available as each directory open would take another file
 *	descriptor out of play for handling I/O for another job. Given that
 *	it is only recently that UNIX OS's have taken to allowing more than
 *	20 or 32 file descriptors for a process, this doesn't seem acceptable
 *	to me.
 *
 * 3)	record the mtime of the directory in the CachedDir structure and
 *	verify the directory hasn't changed since the contents were cached.
 *	This will catch the creation or deletion of files, but not the
 *	updating of files. However, since it is the creation and deletion
 *	that is the problem, this could be a good thing to do. Unfortunately,
 *	if the directory (say ".") were fairly large and changed fairly
 *	frequently, the constant reloading could seriously degrade
 *	performance. It might be good in such cases to keep track of the
 *	number of reloadings and if the number goes over a (small) limit,
 *	resort to using stat in its place.
 *
 * An additional thing to consider is that pmake is used primarily to create
 * C programs and until recently pcc-based compilers refused to allow you to
 * specify where the resulting object file should be placed. This forced all
 * objects to be created in the current directory. This isn't meant as a full
 * excuse, just an explanation of some of the reasons for the caching used
 * here.
 *
 * One more note: the location of a target's file is only performed on the
 * downward traversal of the graph and then only for terminal nodes in the
 * graph. This could be construed as wrong in some cases, but prevents
 * inadvertent modification of files when the "installed" directory for a
 * file is provided in the search path.
 *
 * Another data structure maintained by this module is an mtime cache used
 * when the searching of cached directories fails to find a file. In the past,
 * Dir_FindFile would simply perform an access() call in such a case to
 * determine if the file could be found using just the name given. When this
 * hit, however, all that was gained was the knowledge that the file existed.
 * Given that an access() is essentially a stat() without the copyout() call,
 * and that the same filesystem overhead would have to be incurred in
 * Dir_MTime, it made sense to replace the access() with a stat() and record
 * the mtime in a cache for when Dir_MTime was actually called.
 */

typedef List CachedDirList;
typedef ListNode CachedDirListNode;

typedef ListNode SearchPathNode;

SearchPath *dirSearchPath;		/* main search path */

/* A list of cached directories, with fast lookup by directory name. */
typedef struct OpenDirs {
    CachedDirList *list;
    HashTable /* of CachedDirListNode */ table;
} OpenDirs;

static void
OpenDirs_Init(OpenDirs *odirs)
{
    odirs->list = Lst_New();
    HashTable_Init(&odirs->table);
}

#ifdef CLEANUP
static void
OpenDirs_Done(OpenDirs *odirs)
{
    CachedDirListNode *ln = odirs->list->first;
    while (ln != NULL) {
	CachedDirListNode *next = ln->next;
	CachedDir *dir = ln->datum;
	Dir_Destroy(dir);	/* removes the dir from odirs->list */
	ln = next;
    }
    Lst_Free(odirs->list);
    HashTable_Done(&odirs->table);
}
#endif

static CachedDir *
OpenDirs_Find(OpenDirs *odirs, const char *name)
{
    CachedDirListNode *ln = HashTable_FindValue(&odirs->table, name);
    return ln != NULL ? ln->datum : NULL;
}

static void
OpenDirs_Add(OpenDirs *odirs, CachedDir *cdir)
{
    HashEntry *he = HashTable_FindEntry(&odirs->table, cdir->name);
    if (he != NULL)
	return;
    he = HashTable_CreateEntry(&odirs->table, cdir->name, NULL);
    Lst_Append(odirs->list, cdir);
    HashEntry_Set(he, odirs->list->last);
}

static void
OpenDirs_Remove(OpenDirs *odirs, const char *name)
{
    HashEntry *he = HashTable_FindEntry(&odirs->table, name);
    CachedDirListNode *ln;
    if (he == NULL)
	return;
    ln = HashEntry_Get(he);
    HashTable_DeleteEntry(&odirs->table, he);
    Lst_Remove(odirs->list, ln);
}

static OpenDirs openDirs;	/* the list of all open directories */

/*
 * Variables for gathering statistics on the efficiency of the cashing
 * mechanism.
 */
static int hits;		/* Found in directory cache */
static int misses;		/* Sad, but not evil misses */
static int nearmisses;		/* Found under search path */
static int bigmisses;		/* Sought by itself */

static CachedDir *dot;		/* contents of current directory */
static CachedDir *cur;		/* contents of current directory, if not dot */
static CachedDir *dotLast;	/* a fake path entry indicating we need to
				 * look for . last */

/* Results of doing a last-resort stat in Dir_FindFile -- if we have to go to
 * the system to find the file, we might as well have its mtime on record.
 *
 * XXX: If this is done way early, there's a chance other rules will have
 * already updated the file, in which case we'll update it again. Generally,
 * there won't be two rules to update a single file, so this should be ok,
 * but... */
static HashTable mtimes;

static HashTable lmtimes;	/* same as mtimes but for lstat */

/*
 * We use stat(2) a lot, cache the results.
 * mtime and mode are all we care about.
 */
struct cache_st {
    time_t lmtime;		/* lstat */
    time_t mtime;		/* stat */
    mode_t mode;
};

/* minimize changes below */
typedef enum CachedStatsFlags {
    CST_LSTAT = 0x01,		/* call lstat(2) instead of stat(2) */
    CST_UPDATE = 0x02		/* ignore existing cached entry */
} CachedStatsFlags;

/* Returns 0 and the result of stat(2) or lstat(2) in *mst, or -1 on error. */
static int
cached_stats(HashTable *htp, const char *pathname, struct make_stat *mst,
	     CachedStatsFlags flags)
{
    HashEntry *entry;
    struct stat sys_st;
    struct cache_st *cst;
    int rc;

    if (!pathname || !pathname[0])
	return -1;

    entry = HashTable_FindEntry(htp, pathname);

    if (entry && !(flags & CST_UPDATE)) {
	cst = HashEntry_Get(entry);

	mst->mst_mode = cst->mode;
	mst->mst_mtime = (flags & CST_LSTAT) ? cst->lmtime : cst->mtime;
	if (mst->mst_mtime) {
	    DIR_DEBUG2("Using cached time %s for %s\n",
		       Targ_FmtTime(mst->mst_mtime), pathname);
	    return 0;
	}
    }

    rc = (flags & CST_LSTAT)
	 ? lstat(pathname, &sys_st)
	 : stat(pathname, &sys_st);
    if (rc == -1)
	return -1;

    if (sys_st.st_mtime == 0)
	sys_st.st_mtime = 1;	/* avoid confusion with missing file */

    mst->mst_mode = sys_st.st_mode;
    mst->mst_mtime = sys_st.st_mtime;

    if (entry == NULL)
	entry = HashTable_CreateEntry(htp, pathname, NULL);
    if (HashEntry_Get(entry) == NULL) {
	HashEntry_Set(entry, bmake_malloc(sizeof(*cst)));
	memset(HashEntry_Get(entry), 0, sizeof(*cst));
    }
    cst = HashEntry_Get(entry);
    if (flags & CST_LSTAT) {
	cst->lmtime = sys_st.st_mtime;
    } else {
	cst->mtime = sys_st.st_mtime;
    }
    cst->mode = sys_st.st_mode;
    DIR_DEBUG2("   Caching %s for %s\n",
	       Targ_FmtTime(sys_st.st_mtime), pathname);

    return 0;
}

int
cached_stat(const char *pathname, struct make_stat *st)
{
    return cached_stats(&mtimes, pathname, st, 0);
}

int
cached_lstat(const char *pathname, struct make_stat *st)
{
    return cached_stats(&lmtimes, pathname, st, CST_LSTAT);
}

/* Initialize the directories module. */
void
Dir_Init(void)
{
    dirSearchPath = Lst_New();
    OpenDirs_Init(&openDirs);
    HashTable_Init(&mtimes);
    HashTable_Init(&lmtimes);
}

void
Dir_InitDir(const char *cdname)
{
    Dir_InitCur(cdname);

    dotLast = bmake_malloc(sizeof(CachedDir));
    dotLast->refCount = 1;
    dotLast->hits = 0;
    dotLast->name = bmake_strdup(".DOTLAST");
    HashTable_Init(&dotLast->files);
}

/*
 * Called by Dir_InitDir and whenever .CURDIR is assigned to.
 */
void
Dir_InitCur(const char *cdname)
{
    CachedDir *dir;

    if (cdname != NULL) {
	/*
	 * Our build directory is not the same as our source directory.
	 * Keep this one around too.
	 */
	if ((dir = Dir_AddDir(NULL, cdname))) {
	    dir->refCount++;
	    if (cur && cur != dir) {
		/*
		 * We've been here before, clean up.
		 */
		cur->refCount--;
		Dir_Destroy(cur);
	    }
	    cur = dir;
	}
    }
}

/* (Re)initialize "dot" (current/object directory) path hash.
 * Some directories may be opened. */
void
Dir_InitDot(void)
{
    if (dot != NULL) {
	/* Remove old entry from openDirs, but do not destroy. */
	OpenDirs_Remove(&openDirs, dot->name);
    }

    dot = Dir_AddDir(NULL, ".");

    if (dot == NULL) {
	Error("Cannot open `.' (%s)", strerror(errno));
	exit(1);
    }

    /*
     * We always need to have dot around, so we increment its reference count
     * to make sure it's not destroyed.
     */
    dot->refCount++;
    Dir_SetPATH();		/* initialize */
}

/* Clean up the directories module. */
void
Dir_End(void)
{
#ifdef CLEANUP
    if (cur) {
	cur->refCount--;
	Dir_Destroy(cur);
    }
    dot->refCount--;
    dotLast->refCount--;
    Dir_Destroy(dotLast);
    Dir_Destroy(dot);
    Dir_ClearPath(dirSearchPath);
    Lst_Free(dirSearchPath);
    OpenDirs_Done(&openDirs);
    HashTable_Done(&mtimes);
#endif
}

/*
 * We want ${.PATH} to indicate the order in which we will actually
 * search, so we rebuild it after any .PATH: target.
 * This is the simplest way to deal with the effect of .DOTLAST.
 */
void
Dir_SetPATH(void)
{
    CachedDirListNode *ln;
    Boolean hasLastDot = FALSE;	/* true if we should search dot last */

    Var_Delete(".PATH", VAR_GLOBAL);

    if ((ln = dirSearchPath->first) != NULL) {
	CachedDir *dir = ln->datum;
	if (dir == dotLast) {
	    hasLastDot = TRUE;
	    Var_Append(".PATH", dotLast->name, VAR_GLOBAL);
	}
    }

    if (!hasLastDot) {
	if (dot)
	    Var_Append(".PATH", dot->name, VAR_GLOBAL);
	if (cur)
	    Var_Append(".PATH", cur->name, VAR_GLOBAL);
    }

    for (ln = dirSearchPath->first; ln != NULL; ln = ln->next) {
	CachedDir *dir = ln->datum;
	if (dir == dotLast)
	    continue;
	if (dir == dot && hasLastDot)
	    continue;
	Var_Append(".PATH", dir->name, VAR_GLOBAL);
    }

    if (hasLastDot) {
	if (dot)
	    Var_Append(".PATH", dot->name, VAR_GLOBAL);
	if (cur)
	    Var_Append(".PATH", cur->name, VAR_GLOBAL);
    }
}

/* See if the given name has any wildcard characters in it and all braces and
 * brackets are properly balanced.
 *
 * XXX: This code is not 100% correct ([^]] fails etc.). I really don't think
 * that make(1) should be expanding patterns, because then you have to set a
 * mechanism for escaping the expansion!
 *
 * Return TRUE if the word should be expanded, FALSE otherwise.
 */
Boolean
Dir_HasWildcards(const char *name)
{
    const char *p;
    Boolean wild = FALSE;
    int braces = 0, brackets = 0;

    for (p = name; *p != '\0'; p++) {
	switch (*p) {
	case '{':
	    braces++;
	    wild = TRUE;
	    break;
	case '}':
	    braces--;
	    break;
	case '[':
	    brackets++;
	    wild = TRUE;
	    break;
	case ']':
	    brackets--;
	    break;
	case '?':
	case '*':
	    wild = TRUE;
	    break;
	default:
	    break;
	}
    }
    return wild && brackets == 0 && braces == 0;
}

/* See if any files match the pattern and add their names to the 'expansions'
 * list if they do.
 *
 * This is incomplete -- wildcards are only expanded in the final path
 * component, but not in directories like src/lib*c/file*.c, but it
 * will do for now (now being 1993 until at least 2020). To expand these,
 * use the ':sh' variable modifier such as in ${:!echo src/lib*c/file*.c!}.
 *
 * Input:
 *	pattern		Pattern to look for
 *	dir		Directory to search
 *	expansion	Place to store the results
 */
static void
DirMatchFiles(const char *pattern, CachedDir *dir, StringList *expansions)
{
    const char *dirName = dir->name;
    Boolean isDot = dirName[0] == '.' && dirName[1] == '\0';
    HashIter hi;

    HashIter_Init(&hi, &dir->files);
    while (HashIter_Next(&hi) != NULL) {
	const char *base = hi.entry->key;

	if (!Str_Match(base, pattern))
	    continue;

	/*
	 * Follow the UNIX convention that dot files are only found if the
	 * pattern begins with a dot. The pattern '.*' does not match '.' or
	 * '..' since these are not included in the directory cache.
	 *
	 * This means that the pattern '[a-z.]*' does not find '.file', which
	 * is consistent with bash, NetBSD sh and csh.
	 */
	if (base[0] == '.' && pattern[0] != '.')
	    continue;

	{
	    char *fullName = isDot
			     ? bmake_strdup(base)
			     : str_concat3(dirName, "/", base);
	    Lst_Append(expansions, fullName);
	}
    }
}

/* Find the next closing brace in the string, taking nested braces into
 * account. */
static const char *
closing_brace(const char *p)
{
    int nest = 0;
    while (*p != '\0') {
	if (*p == '}' && nest == 0)
	    break;
	if (*p == '{')
	    nest++;
	if (*p == '}')
	    nest--;
	p++;
    }
    return p;
}

/* Find the next closing brace or comma in the string, taking nested braces
 * into account. */
static const char *
separator_comma(const char *p)
{
    int nest = 0;
    while (*p != '\0') {
	if ((*p == '}' || *p == ',') && nest == 0)
	    break;
	if (*p == '{')
	    nest++;
	if (*p == '}')
	    nest--;
	p++;
    }
    return p;
}

static Boolean
contains_wildcard(const char *p)
{
    for (; *p != '\0'; p++) {
	switch (*p) {
	case '*':
	case '?':
	case '{':
	case '[':
	    return TRUE;
	}
    }
    return FALSE;
}

static char *
concat3(const char *a, size_t a_len, const char *b, size_t b_len,
	const char *c, size_t c_len)
{
    size_t s_len = a_len + b_len + c_len;
    char *s = bmake_malloc(s_len + 1);
    memcpy(s, a, a_len);
    memcpy(s + a_len, b, b_len);
    memcpy(s + a_len + b_len, c, c_len);
    s[s_len] = '\0';
    return s;
}

/* Expand curly braces like the C shell. Brace expansion by itself is purely
 * textual, the expansions are not looked up in the file system. But if an
 * expanded word contains wildcard characters, it is expanded further,
 * matching only the actually existing files.
 *
 * Example: "{a{b,c}}" expands to "ab" and "ac".
 * Example: "{a}" expands to "a".
 * Example: "{a,*.c}" expands to "a" and all "*.c" files that exist.
 *
 * Input:
 *	word		Entire word to expand
 *	brace		First curly brace in it
 *	path		Search path to use
 *	expansions	Place to store the expansions
 */
static void
DirExpandCurly(const char *word, const char *brace, SearchPath *path,
	       StringList *expansions)
{
    const char *prefix, *middle, *piece, *middle_end, *suffix;
    size_t prefix_len, suffix_len;

    /* Split the word into prefix '{' middle '}' suffix. */

    middle = brace + 1;
    middle_end = closing_brace(middle);
    if (*middle_end == '\0') {
	Error("Unterminated {} clause \"%s\"", middle);
	return;
    }

    prefix = word;
    prefix_len = (size_t)(brace - prefix);
    suffix = middle_end + 1;
    suffix_len = strlen(suffix);

    /* Split the middle into pieces, separated by commas. */

    piece = middle;
    while (piece < middle_end + 1) {
	const char *piece_end = separator_comma(piece);
	size_t piece_len = (size_t)(piece_end - piece);

	char *file = concat3(prefix, prefix_len, piece, piece_len,
			     suffix, suffix_len);

	if (contains_wildcard(file)) {
	    Dir_Expand(file, path, expansions);
	    free(file);
	} else {
	    Lst_Append(expansions, file);
	}

	piece = piece_end + 1;	/* skip over the comma or closing brace */
    }
}


/* Expand the word in each of the directories from the path. */
static void
DirExpandPath(const char *word, SearchPath *path, StringList *expansions)
{
    SearchPathNode *ln;
    for (ln = path->first; ln != NULL; ln = ln->next) {
	CachedDir *dir = ln->datum;
	DirMatchFiles(word, dir, expansions);
    }
}

static void
PrintExpansions(StringList *expansions)
{
    const char *sep = "";
    StringListNode *ln;
    for (ln = expansions->first; ln != NULL; ln = ln->next) {
	const char *word = ln->datum;
	debug_printf("%s%s", sep, word);
	sep = " ";
    }
    debug_printf("\n");
}

/* Expand the given word into a list of words by globbing it, looking in the
 * directories on the given search path.
 *
 * Input:
 *	word		the word to expand
 *	path		the directories in which to find the files
 *	expansions	the list on which to place the results
 */
void
Dir_Expand(const char *word, SearchPath *path, StringList *expansions)
{
    const char *cp;

    assert(path != NULL);
    assert(expansions != NULL);

    DIR_DEBUG1("Expanding \"%s\"... ", word);

    cp = strchr(word, '{');
    if (cp) {
	DirExpandCurly(word, cp, path, expansions);
    } else {
	cp = strchr(word, '/');
	if (cp) {
	    /*
	     * The thing has a directory component -- find the first wildcard
	     * in the string.
	     */
	    for (cp = word; *cp; cp++) {
		if (*cp == '?' || *cp == '[' || *cp == '*') {
		    break;
		}
	    }

	    if (*cp != '\0') {
		/*
		 * Back up to the start of the component
		 */
		while (cp > word && *cp != '/') {
		    cp--;
		}
		if (cp != word) {
		    char *prefix = bmake_strsedup(word, cp + 1);
		    /*
		     * If the glob isn't in the first component, try and find
		     * all the components up to the one with a wildcard.
		     */
		    char *dirpath = Dir_FindFile(prefix, path);
		    free(prefix);
		    /*
		     * dirpath is null if can't find the leading component
		     * XXX: Dir_FindFile won't find internal components.
		     * i.e. if the path contains ../Etc/Object and we're
		     * looking for Etc, it won't be found. Ah well.
		     * Probably not important.
		     */
		    if (dirpath != NULL) {
			char *dp = &dirpath[strlen(dirpath) - 1];
			if (*dp == '/')
			    *dp = '\0';
			path = Lst_New();
			(void)Dir_AddDir(path, dirpath);
			DirExpandPath(cp + 1, path, expansions);
			Lst_Free(path);
		    }
		} else {
		    /*
		     * Start the search from the local directory
		     */
		    DirExpandPath(word, path, expansions);
		}
	    } else {
		/*
		 * Return the file -- this should never happen.
		 */
		DirExpandPath(word, path, expansions);
	    }
	} else {
	    /*
	     * First the files in dot
	     */
	    DirMatchFiles(word, dot, expansions);

	    /*
	     * Then the files in every other directory on the path.
	     */
	    DirExpandPath(word, path, expansions);
	}
    }
    if (DEBUG(DIR))
	PrintExpansions(expansions);
}

/* Find if the file with the given name exists in the given path.
 * Return the freshly allocated path to the file, or NULL. */
static char *
DirLookup(CachedDir *dir, const char *base)
{
    char *file;			/* the current filename to check */

    DIR_DEBUG1("   %s ...\n", dir->name);

    if (HashTable_FindEntry(&dir->files, base) == NULL)
	return NULL;

    file = str_concat3(dir->name, "/", base);
    DIR_DEBUG1("   returning %s\n", file);
    dir->hits++;
    hits++;
    return file;
}


/* Find if the file with the given name exists in the given directory.
 * Return the freshly allocated path to the file, or NULL. */
static char *
DirLookupSubdir(CachedDir *dir, const char *name)
{
    struct make_stat mst;
    char *file = dir == dot ? bmake_strdup(name)
			    : str_concat3(dir->name, "/", name);

    DIR_DEBUG1("checking %s ...\n", file);

    if (cached_stat(file, &mst) == 0) {
	nearmisses++;
	return file;
    }
    free(file);
    return NULL;
}

/* Find if the file with the given name exists in the given path.
 * Return the freshly allocated path to the file, the empty string, or NULL.
 * Returning the empty string means that the search should be terminated.
 */
static char *
DirLookupAbs(CachedDir *dir, const char *name, const char *cp)
{
    const char *dnp;		/* pointer into dir->name */
    const char *np;		/* pointer into name */

    DIR_DEBUG1("   %s ...\n", dir->name);

    /*
     * If the file has a leading path component and that component
     * exactly matches the entire name of the current search
     * directory, we can attempt another cache lookup. And if we don't
     * have a hit, we can safely assume the file does not exist at all.
     */
    for (dnp = dir->name, np = name; *dnp != '\0' && *dnp == *np; dnp++, np++)
	continue;
    if (*dnp != '\0' || np != cp - 1)
	return NULL;

    if (HashTable_FindEntry(&dir->files, cp) == NULL) {
	DIR_DEBUG0("   must be here but isn't -- returning\n");
	return bmake_strdup("");	/* to terminate the search */
    }

    dir->hits++;
    hits++;
    DIR_DEBUG1("   returning %s\n", name);
    return bmake_strdup(name);
}

/* Find the file given on "." or curdir.
 * Return the freshly allocated path to the file, or NULL. */
static char *
DirFindDot(const char *name, const char *base)
{

    if (HashTable_FindEntry(&dot->files, base) != NULL) {
	DIR_DEBUG0("   in '.'\n");
	hits++;
	dot->hits++;
	return bmake_strdup(name);
    }

    if (cur != NULL && HashTable_FindEntry(&cur->files, base) != NULL) {
	DIR_DEBUG1("   in ${.CURDIR} = %s\n", cur->name);
	hits++;
	cur->hits++;
	return str_concat3(cur->name, "/", base);
    }

    return NULL;
}

/* Find the file with the given name along the given search path.
 *
 * If the file is found in a directory that is not on the path
 * already (either 'name' is absolute or it is a relative path
 * [ dir1/.../dirn/file ] which exists below one of the directories
 * already on the search path), its directory is added to the end
 * of the path, on the assumption that there will be more files in
 * that directory later on. Sometimes this is true. Sometimes not.
 *
 * Input:
 *	name		the file to find
 *	path		the directories to search, or NULL
 *
 * Results:
 *	The freshly allocated path to the file, or NULL.
 */
char *
Dir_FindFile(const char *name, SearchPath *path)
{
    SearchPathNode *ln;
    char *file;			/* the current filename to check */
    const char *base;		/* Terminal name of file */
    Boolean hasLastDot = FALSE;	/* true if we should search dot last */
    Boolean hasSlash;		/* true if 'name' contains a / */
    struct make_stat mst;	/* Buffer for stat, if necessary */
    const char *trailing_dot = ".";

    /*
     * Find the final component of the name and note whether it has a
     * slash in it (the name, I mean)
     */
    base = strrchr(name, '/');
    if (base) {
	hasSlash = TRUE;
	base++;
    } else {
	hasSlash = FALSE;
	base = name;
    }

    DIR_DEBUG1("Searching for %s ...", name);

    if (path == NULL) {
	DIR_DEBUG0("couldn't open path, file not found\n");
	misses++;
	return NULL;
    }

    if ((ln = path->first) != NULL) {
	CachedDir *dir = ln->datum;
	if (dir == dotLast) {
	    hasLastDot = TRUE;
	    DIR_DEBUG0("[dot last]...");
	}
    }
    DIR_DEBUG0("\n");

    /*
     * If there's no leading directory components or if the leading
     * directory component is exactly `./', consult the cached contents
     * of each of the directories on the search path.
     */
    if (!hasSlash || (base - name == 2 && *name == '.')) {
	/*
	 * We look through all the directories on the path seeking one which
	 * contains the final component of the given name.  If such a beast
	 * is found, we concatenate the directory name and the final
	 * component and return the resulting string. If we don't find any
	 * such thing, we go on to phase two...
	 *
	 * No matter what, we always look for the file in the current
	 * directory before anywhere else (unless we found the magic
	 * DOTLAST path, in which case we search it last) and we *do not*
	 * add the ./ to it if it exists.
	 * This is so there are no conflicts between what the user
	 * specifies (fish.c) and what pmake finds (./fish.c).
	 */
	if (!hasLastDot && (file = DirFindDot(name, base)) != NULL)
	    return file;

	for (; ln != NULL; ln = ln->next) {
	    CachedDir *dir = ln->datum;
	    if (dir == dotLast)
		continue;
	    if ((file = DirLookup(dir, base)) != NULL)
		return file;
	}

	if (hasLastDot && (file = DirFindDot(name, base)) != NULL)
	    return file;
    }

    /*
     * We didn't find the file on any directory in the search path.
     * If the name doesn't contain a slash, that means it doesn't exist.
     * If it *does* contain a slash, however, there is still hope: it
     * could be in a subdirectory of one of the members of the search
     * path. (eg. /usr/include and sys/types.h. The above search would
     * fail to turn up types.h in /usr/include, but it *is* in
     * /usr/include/sys/types.h).
     * [ This no longer applies: If we find such a beast, we assume there
     * will be more (what else can we assume?) and add all but the last
     * component of the resulting name onto the search path (at the
     * end).]
     * This phase is only performed if the file is *not* absolute.
     */
    if (!hasSlash) {
	DIR_DEBUG0("   failed.\n");
	misses++;
	return NULL;
    }

    if (*base == '\0') {
	/* we were given a trailing "/" */
	base = trailing_dot;
    }

    if (name[0] != '/') {
	Boolean checkedDot = FALSE;

	DIR_DEBUG0("   Trying subdirectories...\n");

	if (!hasLastDot) {
	    if (dot) {
		checkedDot = TRUE;
		if ((file = DirLookupSubdir(dot, name)) != NULL)
		    return file;
	    }
	    if (cur && (file = DirLookupSubdir(cur, name)) != NULL)
		return file;
	}

	for (ln = path->first; ln != NULL; ln = ln->next) {
	    CachedDir *dir = ln->datum;
	    if (dir == dotLast)
		continue;
	    if (dir == dot) {
		if (checkedDot)
		    continue;
		checkedDot = TRUE;
	    }
	    if ((file = DirLookupSubdir(dir, name)) != NULL)
		return file;
	}

	if (hasLastDot) {
	    if (dot && !checkedDot) {
		checkedDot = TRUE;
		if ((file = DirLookupSubdir(dot, name)) != NULL)
		    return file;
	    }
	    if (cur && (file = DirLookupSubdir(cur, name)) != NULL)
		return file;
	}

	if (checkedDot) {
	    /*
	     * Already checked by the given name, since . was in the path,
	     * so no point in proceeding...
	     */
	    DIR_DEBUG0("   Checked . already, returning NULL\n");
	    return NULL;
	}

    } else { /* name[0] == '/' */

	/*
	 * For absolute names, compare directory path prefix against the
	 * the directory path of each member on the search path for an exact
	 * match. If we have an exact match on any member of the search path,
	 * use the cached contents of that member to lookup the final file
	 * component. If that lookup fails we can safely assume that the
	 * file does not exist at all.  This is signified by DirLookupAbs()
	 * returning an empty string.
	 */
	DIR_DEBUG0("   Trying exact path matches...\n");

	if (!hasLastDot && cur &&
	    ((file = DirLookupAbs(cur, name, base)) != NULL)) {
	    if (file[0] == '\0') {
		free(file);
		return NULL;
	    }
	    return file;
	}

	for (ln = path->first; ln != NULL; ln = ln->next) {
	    CachedDir *dir = ln->datum;
	    if (dir == dotLast)
		continue;
	    if ((file = DirLookupAbs(dir, name, base)) != NULL) {
		if (file[0] == '\0') {
		    free(file);
		    return NULL;
		}
		return file;
	    }
	}

	if (hasLastDot && cur &&
	    ((file = DirLookupAbs(cur, name, base)) != NULL)) {
	    if (file[0] == '\0') {
		free(file);
		return NULL;
	    }
	    return file;
	}
    }

    /*
     * Didn't find it that way, either. Sigh. Phase 3. Add its directory
     * onto the search path in any case, just in case, then look for the
     * thing in the hash table. If we find it, grand. We return a new
     * copy of the name. Otherwise we sadly return a NULL pointer. Sigh.
     * Note that if the directory holding the file doesn't exist, this will
     * do an extra search of the final directory on the path. Unless something
     * weird happens, this search won't succeed and life will be groovy.
     *
     * Sigh. We cannot add the directory onto the search path because
     * of this amusing case:
     * $(INSTALLDIR)/$(FILE): $(FILE)
     *
     * $(FILE) exists in $(INSTALLDIR) but not in the current one.
     * When searching for $(FILE), we will find it in $(INSTALLDIR)
     * b/c we added it here. This is not good...
     */
#ifdef notdef
    if (base == trailing_dot) {
	base = strrchr(name, '/');
	base++;
    }
    base[-1] = '\0';
    (void)Dir_AddDir(path, name);
    base[-1] = '/';

    bigmisses++;
    ln = Lst_Last(path);
    if (ln == NULL) {
	return NULL;
    } else {
	dir = LstNode_Datum(ln);
    }

    if (Hash_FindEntry(&dir->files, base) != NULL) {
	return bmake_strdup(name);
    } else {
	return NULL;
    }
#else /* !notdef */
    DIR_DEBUG1("   Looking for \"%s\" ...\n", name);

    bigmisses++;
    if (cached_stat(name, &mst) == 0) {
	return bmake_strdup(name);
    }

    DIR_DEBUG0("   failed. Returning NULL\n");
    return NULL;
#endif /* notdef */
}


/* Search for a path starting at a given directory and then working our way
 * up towards the root.
 *
 * Input:
 *	here		starting directory
 *	search_path	the relative path we are looking for
 *
 * Results:
 *	The found path, or NULL.
 */
char *
Dir_FindHereOrAbove(const char *here, const char *search_path)
{
    struct make_stat mst;
    char *dirbase, *dirbase_end;
    char *try, *try_end;

    /* copy out our starting point */
    dirbase = bmake_strdup(here);
    dirbase_end = dirbase + strlen(dirbase);

    /* loop until we determine a result */
    for (;;) {

	/* try and stat(2) it ... */
	try = str_concat3(dirbase, "/", search_path);
	if (cached_stat(try, &mst) != -1) {
	    /*
	     * success!  if we found a file, chop off
	     * the filename so we return a directory.
	     */
	    if ((mst.mst_mode & S_IFMT) != S_IFDIR) {
		try_end = try + strlen(try);
		while (try_end > try && *try_end != '/')
		    try_end--;
		if (try_end > try)
		    *try_end = '\0';	/* chop! */
	    }

	    free(dirbase);
	    return try;
	}
	free(try);

	/*
	 * nope, we didn't find it.  if we used up dirbase we've
	 * reached the root and failed.
	 */
	if (dirbase_end == dirbase)
	    break;		/* failed! */

	/*
	 * truncate dirbase from the end to move up a dir
	 */
	while (dirbase_end > dirbase && *dirbase_end != '/')
	    dirbase_end--;
	*dirbase_end = '\0';	/* chop! */
    }

    free(dirbase);
    return NULL;
}

/*-
 *-----------------------------------------------------------------------
 * Dir_MTime  --
 *	Find the modification time of the file described by gn along the
 *	search path dirSearchPath.
 *
 * Input:
 *	gn		the file whose modification time is desired
 *
 * Results:
 *	The modification time or 0 if it doesn't exist
 *
 * Side Effects:
 *	The modification time is placed in the node's mtime slot.
 *	If the node didn't have a path entry before, and Dir_FindFile
 *	found one for it, the full name is placed in the path slot.
 *-----------------------------------------------------------------------
 */
time_t
Dir_MTime(GNode *gn, Boolean recheck)
{
    char *fullName;		/* the full pathname of name */
    struct make_stat mst;	/* buffer for finding the mod time */

    if (gn->type & OP_ARCHV) {
	return Arch_MTime(gn);
    } else if (gn->type & OP_PHONY) {
	gn->mtime = 0;
	return 0;
    } else if (gn->path == NULL) {
	if (gn->type & OP_NOPATH)
	    fullName = NULL;
	else {
	    fullName = Dir_FindFile(gn->name, Suff_FindPath(gn));
	    if (fullName == NULL && gn->flags & FROM_DEPEND &&
		!Lst_IsEmpty(gn->implicitParents)) {
		char *cp;

		cp = strrchr(gn->name, '/');
		if (cp) {
		    /*
		     * This is an implied source, and it may have moved,
		     * see if we can find it via the current .PATH
		     */
		    cp++;

		    fullName = Dir_FindFile(cp, Suff_FindPath(gn));
		    if (fullName) {
			/*
			 * Put the found file in gn->path
			 * so that we give that to the compiler.
			 */
			gn->path = bmake_strdup(fullName);
			if (!Job_RunTarget(".STALE", gn->fname))
			    fprintf(stdout,
				    "%s: %s, %d: ignoring stale %s for %s, "
				    "found %s\n", progname, gn->fname,
				    gn->lineno,
				    makeDependfile, gn->name, fullName);
		    }
		}
	    }
	    DIR_DEBUG2("Found '%s' as '%s'\n",
		       gn->name, fullName ? fullName : "(not found)");
	}
    } else {
	fullName = gn->path;
    }

    if (fullName == NULL) {
	fullName = bmake_strdup(gn->name);
    }

    if (cached_stats(&mtimes, fullName, &mst, recheck ? CST_UPDATE : 0) < 0) {
	if (gn->type & OP_MEMBER) {
	    if (fullName != gn->path)
		free(fullName);
	    return Arch_MemMTime(gn);
	} else {
	    mst.mst_mtime = 0;
	}
    }

    if (fullName != NULL && gn->path == NULL)
	gn->path = fullName;

    gn->mtime = mst.mst_mtime;
    return gn->mtime;
}

/* Read the list of filenames in the directory and store the result
 * in openDirectories.
 *
 * If a path is given, append the directory to that path.
 *
 * Input:
 *	path		The path to which the directory should be
 *			added, or NULL to only add the directory to
 *			openDirectories
 *	name		The name of the directory to add.
 *			The name is not normalized in any way.
 */
CachedDir *
Dir_AddDir(SearchPath *path, const char *name)
{
    CachedDir *dir = NULL;	/* the added directory */
    DIR *d;
    struct dirent *dp;

    if (path != NULL && strcmp(name, ".DOTLAST") == 0) {
	SearchPathNode *ln;

	for (ln = path->first; ln != NULL; ln = ln->next) {
	    CachedDir *pathDir = ln->datum;
	    if (strcmp(pathDir->name, name) == 0)
		return pathDir;
	}

	dotLast->refCount++;
	Lst_Prepend(path, dotLast);
    }

    if (path != NULL)
	dir = OpenDirs_Find(&openDirs, name);
    if (dir != NULL) {
	if (Lst_FindDatum(path, dir) == NULL) {
	    dir->refCount++;
	    Lst_Append(path, dir);
	}
	return dir;
    }

    DIR_DEBUG1("Caching %s ...", name);

    if ((d = opendir(name)) != NULL) {
	dir = bmake_malloc(sizeof(CachedDir));
	dir->name = bmake_strdup(name);
	dir->hits = 0;
	dir->refCount = 1;
	HashTable_Init(&dir->files);

	while ((dp = readdir(d)) != NULL) {
#if defined(sun) && defined(d_ino) /* d_ino is a sunos4 #define for d_fileno */
	    /*
	     * The sun directory library doesn't check for a 0 inode
	     * (0-inode slots just take up space), so we have to do
	     * it ourselves.
	     */
	    if (dp->d_fileno == 0) {
		continue;
	    }
#endif /* sun && d_ino */
	    (void)HashTable_CreateEntry(&dir->files, dp->d_name, NULL);
	}
	(void)closedir(d);
	OpenDirs_Add(&openDirs, dir);
	if (path != NULL)
	    Lst_Append(path, dir);
    }
    DIR_DEBUG0("done\n");
    return dir;
}

/* Return a copy of dirSearchPath, incrementing the reference counts for
 * the contained directories. */
SearchPath *
Dir_CopyDirSearchPath(void)
{
    SearchPath *path = Lst_New();
    SearchPathNode *ln;
    for (ln = dirSearchPath->first; ln != NULL; ln = ln->next) {
	CachedDir *dir = ln->datum;
	dir->refCount++;
	Lst_Append(path, dir);
    }
    return path;
}

/*-
 *-----------------------------------------------------------------------
 * Dir_MakeFlags --
 *	Make a string by taking all the directories in the given search
 *	path and preceding them by the given flag. Used by the suffix
 *	module to create variables for compilers based on suffix search
 *	paths.
 *
 * Input:
 *	flag		flag which should precede each directory
 *	path		list of directories
 *
 * Results:
 *	The string mentioned above. Note that there is no space between
 *	the given flag and each directory. The empty string is returned if
 *	Things don't go well.
 *
 * Side Effects:
 *	None
 *-----------------------------------------------------------------------
 */
char *
Dir_MakeFlags(const char *flag, SearchPath *path)
{
    Buffer buf;
    SearchPathNode *ln;

    Buf_Init(&buf, 0);

    if (path != NULL) {
	for (ln = path->first; ln != NULL; ln = ln->next) {
	    CachedDir *dir = ln->datum;
	    Buf_AddStr(&buf, " ");
	    Buf_AddStr(&buf, flag);
	    Buf_AddStr(&buf, dir->name);
	}
    }

    return Buf_Destroy(&buf, FALSE);
}

/* Nuke a directory descriptor, if possible. Callback procedure for the
 * suffixes module when destroying a search path.
 *
 * Input:
 *	dirp		The directory descriptor to nuke
 */
void
Dir_Destroy(void *dirp)
{
    CachedDir *dir = dirp;
    dir->refCount--;

    if (dir->refCount == 0) {
	OpenDirs_Remove(&openDirs, dir->name);

	HashTable_Done(&dir->files);
	free(dir->name);
	free(dir);
    }
}

/* Clear out all elements from the given search path.
 * The path is set to the empty list but is not destroyed. */
void
Dir_ClearPath(SearchPath *path)
{
    while (!Lst_IsEmpty(path)) {
	CachedDir *dir = Lst_Dequeue(path);
	Dir_Destroy(dir);
    }
}


/* Concatenate two paths, adding the second to the end of the first,
 * skipping duplicates. */
void
Dir_Concat(SearchPath *dst, SearchPath *src)
{
    SearchPathNode *ln;

    for (ln = src->first; ln != NULL; ln = ln->next) {
	CachedDir *dir = ln->datum;
	if (Lst_FindDatum(dst, dir) == NULL) {
	    dir->refCount++;
	    Lst_Append(dst, dir);
	}
    }
}

static int
percentage(int num, int den)
{
    return den != 0 ? num * 100 / den : 0;
}

/********** DEBUG INFO **********/
void
Dir_PrintDirectories(void)
{
    CachedDirListNode *ln;

    debug_printf("#*** Directory Cache:\n");
    debug_printf("# Stats: %d hits %d misses %d near misses %d losers (%d%%)\n",
		 hits, misses, nearmisses, bigmisses,
		 percentage(hits, hits + bigmisses + nearmisses));
    debug_printf("# %-20s referenced\thits\n", "directory");

    for (ln = openDirs.list->first; ln != NULL; ln = ln->next) {
	CachedDir *dir = ln->datum;
	debug_printf("# %-20s %10d\t%4d\n", dir->name, dir->refCount,
		     dir->hits);
    }
}

void
Dir_PrintPath(SearchPath *path)
{
    SearchPathNode *node;
    for (node = path->first; node != NULL; node = node->next) {
	const CachedDir *dir = node->datum;
	debug_printf("%s ", dir->name);
    }
}
