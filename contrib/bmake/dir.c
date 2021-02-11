/*	$NetBSD: dir.c,v 1.270 2021/02/05 05:48:19 rillig Exp $	*/

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

/*
 * Directory searching using wildcards and/or normal names.
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
 *	SearchPath_Expand
 *			Expand a filename pattern to find all matching files
 *			from the search path.
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
 *	Dir_UpdateMTime
 *			Update the modification time and path of a node with
 *			data from the file corresponding to the node.
 *
 *	SearchPath_Add	Add a directory to a search path.
 *
 *	SearchPath_ToFlags
 *			Given a search path and a command flag, create
 *			a string with each of the directories in the path
 *			preceded by the command flag and all of them
 *			separated by a space.
 *
 *	Dir_Destroy	Destroy an element of a search path. Frees up all
 *			things that can be freed for the element as long
 *			as the element is no longer referenced by any other
 *			search path.
 *
 *	SearchPath_Clear
 *			Resets a search path to the empty list.
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
MAKE_RCSID("$NetBSD: dir.c,v 1.270 2021/02/05 05:48:19 rillig Exp $");

/*
 * A search path is a list of CachedDir structures. A CachedDir has in it the
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
 *	it is only recently (as of 1993 or earlier) that UNIX OS's have taken
 *	to allowing more than 20 or 32 file descriptors for a process, this
 *	doesn't seem acceptable to me.
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
 * C programs and until recently (as of 1993 or earlier) pcc-based compilers
 * refused to allow you to specify where the resulting object file should be
 * placed. This forced all objects to be created in the current directory.
 * This isn't meant as a full excuse, just an explanation of some of the
 * reasons for the caching used here.
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
 * the mtime in a cache for when Dir_UpdateMTime was actually called.
 */


/* A cache for the filenames in a directory. */
struct CachedDir {
	/*
	 * Name of directory, either absolute or relative to the current
	 * directory. The name is not normalized in any way, that is, "."
	 * and "./." are different.
	 *
	 * Not sure what happens when .CURDIR is assigned a new value; see
	 * Parse_DoVar.
	 */
	char *name;

	/*
	 * The number of SearchPaths that refer to this directory.
	 * Plus the number of global variables that refer to this directory.
	 * References from openDirs do not count though.
	 */
	int refCount;

	/* The number of times a file in this directory has been found. */
	int hits;

	/* The names of the directory entries. */
	HashSet files;
};

typedef List CachedDirList;
typedef ListNode CachedDirListNode;

typedef ListNode SearchPathNode;

/* A list of cached directories, with fast lookup by directory name. */
typedef struct OpenDirs {
	CachedDirList list;
	HashTable /* of CachedDirListNode */ table;
} OpenDirs;

typedef enum CachedStatsFlags {
	CST_NONE	= 0,
	CST_LSTAT	= 1 << 0,	/* call lstat(2) instead of stat(2) */
	CST_UPDATE	= 1 << 1	/* ignore existing cached entry */
} CachedStatsFlags;


SearchPath dirSearchPath = { LST_INIT }; /* main search path */

static OpenDirs openDirs;	/* all cached directories */

/*
 * Variables for gathering statistics on the efficiency of the caching
 * mechanism.
 */
static int hits;		/* Found in directory cache */
static int misses;		/* Sad, but not evil misses */
static int nearmisses;		/* Found under search path */
static int bigmisses;		/* Sought by itself */

/* The cached contents of ".", the relative current directory. */
static CachedDir *dot = NULL;
/* The cached contents of the absolute current directory. */
static CachedDir *cur = NULL;
/* A fake path entry indicating we need to look for '.' last. */
static CachedDir *dotLast = NULL;

/*
 * Results of doing a last-resort stat in Dir_FindFile -- if we have to go to
 * the system to find the file, we might as well have its mtime on record.
 *
 * XXX: If this is done way early, there's a chance other rules will have
 * already updated the file, in which case we'll update it again. Generally,
 * there won't be two rules to update a single file, so this should be ok,
 * but...
 */
static HashTable mtimes;

static HashTable lmtimes;	/* same as mtimes but for lstat */


static void OpenDirs_Remove(OpenDirs *, const char *);


static CachedDir *
CachedDir_New(const char *name)
{
	CachedDir *dir = bmake_malloc(sizeof *dir);

	dir->name = bmake_strdup(name);
	dir->refCount = 0;
	dir->hits = 0;
	HashSet_Init(&dir->files);

#ifdef DEBUG_REFCNT
	DEBUG2(DIR, "CachedDir %p new  for \"%s\"\n", dir, dir->name);
#endif

	return dir;
}

static CachedDir *
CachedDir_Ref(CachedDir *dir)
{
	dir->refCount++;

#ifdef DEBUG_REFCNT
	DEBUG3(DIR, "CachedDir %p ++ %d for \"%s\"\n",
	    dir, dir->refCount, dir->name);
#endif

	return dir;
}

static void
CachedDir_Unref(CachedDir *dir)
{
	dir->refCount--;

#ifdef DEBUG_REFCNT
	DEBUG3(DIR, "CachedDir %p -- %d for \"%s\"\n",
	    dir, dir->refCount, dir->name);
#endif

	if (dir->refCount > 0)
		return;

#ifdef DEBUG_REFCNT
	DEBUG2(DIR, "CachedDir %p free for \"%s\"\n", dir, dir->name);
#endif

	OpenDirs_Remove(&openDirs, dir->name);

	free(dir->name);
	HashSet_Done(&dir->files);
	free(dir);
}

/* Update the value of the CachedDir variable, updating the reference counts. */
static void
CachedDir_Assign(CachedDir **var, CachedDir *dir)
{
	CachedDir *prev;

	prev = *var;
	*var = dir;
	if (dir != NULL)
		CachedDir_Ref(dir);
	if (prev != NULL)
		CachedDir_Unref(prev);
}

static void
OpenDirs_Init(OpenDirs *odirs)
{
	Lst_Init(&odirs->list);
	HashTable_Init(&odirs->table);
}

#ifdef CLEANUP
static void
OpenDirs_Done(OpenDirs *odirs)
{
	CachedDirListNode *ln = odirs->list.first;
	DEBUG1(DIR, "OpenDirs_Done: %u entries to remove\n",
	    odirs->table.numEntries);
	while (ln != NULL) {
		CachedDirListNode *next = ln->next;
		CachedDir *dir = ln->datum;
		DEBUG2(DIR, "OpenDirs_Done: refCount %d for \"%s\"\n",
		    dir->refCount, dir->name);
		CachedDir_Unref(dir);	/* removes the dir from odirs->list */
		ln = next;
	}
	Lst_Done(&odirs->list);
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
	if (HashTable_FindEntry(&odirs->table, cdir->name) != NULL)
		return;
	Lst_Append(&odirs->list, cdir);
	HashTable_Set(&odirs->table, cdir->name, odirs->list.last);
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
	Lst_Remove(&odirs->list, ln);
}

/*
 * Returns 0 and the result of stat(2) or lstat(2) in *out_cst,
 * or -1 on error.
 */
static int
cached_stats(const char *pathname, struct cached_stat *out_cst,
	     CachedStatsFlags flags)
{
	HashTable *tbl = flags & CST_LSTAT ? &lmtimes : &mtimes;
	struct stat sys_st;
	struct cached_stat *cst;
	int rc;

	if (pathname == NULL || pathname[0] == '\0')
		return -1;	/* This can happen in meta mode. */

	cst = HashTable_FindValue(tbl, pathname);
	if (cst != NULL && !(flags & CST_UPDATE)) {
		*out_cst = *cst;
		DEBUG2(DIR, "Using cached time %s for %s\n",
		    Targ_FmtTime(cst->cst_mtime), pathname);
		return 0;
	}

	rc = (flags & CST_LSTAT ? lstat : stat)(pathname, &sys_st);
	if (rc == -1)
		return -1;	/* don't cache negative lookups */

	if (sys_st.st_mtime == 0)
		sys_st.st_mtime = 1; /* avoid confusion with missing file */

	if (cst == NULL) {
		cst = bmake_malloc(sizeof *cst);
		HashTable_Set(tbl, pathname, cst);
	}

	cst->cst_mtime = sys_st.st_mtime;
	cst->cst_mode = sys_st.st_mode;

	*out_cst = *cst;
	DEBUG2(DIR, "   Caching %s for %s\n",
	    Targ_FmtTime(sys_st.st_mtime), pathname);

	return 0;
}

int
cached_stat(const char *pathname, struct cached_stat *cst)
{
	return cached_stats(pathname, cst, CST_NONE);
}

int
cached_lstat(const char *pathname, struct cached_stat *cst)
{
	return cached_stats(pathname, cst, CST_LSTAT);
}

/* Initialize the directories module. */
void
Dir_Init(void)
{
	OpenDirs_Init(&openDirs);
	HashTable_Init(&mtimes);
	HashTable_Init(&lmtimes);
	CachedDir_Assign(&dotLast, CachedDir_New(".DOTLAST"));
}

/*
 * Called by Dir_InitDir and whenever .CURDIR is assigned to.
 */
void
Dir_InitCur(const char *newCurdir)
{
	CachedDir *dir;

	if (newCurdir == NULL)
		return;

	/*
	 * Our build directory is not the same as our source directory.
	 * Keep this one around too.
	 */
	dir = SearchPath_Add(NULL, newCurdir);
	if (dir == NULL)
		return;

	CachedDir_Assign(&cur, dir);
}

/*
 * (Re)initialize "dot" (current/object directory) path hash.
 * Some directories may be cached.
 */
void
Dir_InitDot(void)
{
	CachedDir *dir;

	dir = SearchPath_Add(NULL, ".");
	if (dir == NULL) {
		Error("Cannot open `.' (%s)", strerror(errno));
		exit(2);	/* Not 1 so -q can distinguish error */
	}

	CachedDir_Assign(&dot, dir);

	Dir_SetPATH();		/* initialize */
}

/* Clean up the directories module. */
void
Dir_End(void)
{
#ifdef CLEANUP
	CachedDir_Assign(&cur, NULL);
	CachedDir_Assign(&dot, NULL);
	CachedDir_Assign(&dotLast, NULL);
	SearchPath_Clear(&dirSearchPath);
	OpenDirs_Done(&openDirs);
	HashTable_Done(&mtimes);
	HashTable_Done(&lmtimes);
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
	Boolean seenDotLast = FALSE;	/* true if we should search '.' last */

	Global_Delete(".PATH");

	if ((ln = dirSearchPath.dirs.first) != NULL) {
		CachedDir *dir = ln->datum;
		if (dir == dotLast) {
			seenDotLast = TRUE;
			Global_Append(".PATH", dotLast->name);
		}
	}

	if (!seenDotLast) {
		if (dot != NULL)
			Global_Append(".PATH", dot->name);
		if (cur != NULL)
			Global_Append(".PATH", cur->name);
	}

	for (ln = dirSearchPath.dirs.first; ln != NULL; ln = ln->next) {
		CachedDir *dir = ln->datum;
		if (dir == dotLast)
			continue;
		if (dir == dot && seenDotLast)
			continue;
		Global_Append(".PATH", dir->name);
	}

	if (seenDotLast) {
		if (dot != NULL)
			Global_Append(".PATH", dot->name);
		if (cur != NULL)
			Global_Append(".PATH", cur->name);
	}
}

/*
 * See if the given name has any wildcard characters in it and all braces and
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

/*
 * See if any files match the pattern and add their names to the 'expansions'
 * list if they do.
 *
 * This is incomplete -- wildcards are only expanded in the final path
 * component, but not in directories like src/lib*c/file*.c, but it
 * will do for now (now being 1993 until at least 2020). To expand these,
 * delegate the work to the shell, using the '!=' variable assignment
 * operator, the ':sh' variable modifier or the ':!...!' variable modifier,
 * such as in ${:!echo src/lib*c/file*.c!}.
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

	/*
	 * XXX: Iterating over all hash entries is inefficient.  If the
	 * pattern is a plain string without any wildcards, a direct lookup
	 * is faster.
	 */

	HashIter_InitSet(&hi, &dir->files);
	while (HashIter_Next(&hi) != NULL) {
		const char *base = hi.entry->key;

		if (!Str_Match(base, pattern))
			continue;

		/*
		 * Follow the UNIX convention that dot files are only found
		 * if the pattern begins with a dot. The pattern '.*' does
		 * not match '.' or '..' since these are not included in the
		 * directory cache.
		 *
		 * This means that the pattern '[a-z.]*' does not find
		 * '.file', which is consistent with NetBSD sh, NetBSD ksh,
		 * bash, dash, csh and probably many other shells as well.
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

/*
 * Find the next closing brace in the string, taking nested braces into
 * account.
 */
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

/*
 * Find the next closing brace or comma in the string, taking nested braces
 * into account.
 */
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

/*
 * Expand curly braces like the C shell. Brace expansion by itself is purely
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
			SearchPath_Expand(path, file, expansions);
			free(file);
		} else {
			Lst_Append(expansions, file);
		}

		/* skip over the comma or closing brace */
		piece = piece_end + 1;
	}
}


/* Expand the word in each of the directories from the path. */
static void
DirExpandPath(const char *word, SearchPath *path, StringList *expansions)
{
	SearchPathNode *ln;
	for (ln = path->dirs.first; ln != NULL; ln = ln->next) {
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

/*
 * The wildcard isn't in the first component.
 * Find all the components up to the one with the wildcard.
 */
static void
SearchPath_ExpandMiddle(SearchPath *path, const char *pattern,
			const char *wildcardComponent, StringList *expansions)
{
	char *prefix, *dirpath, *end;
	SearchPath *partPath;

	prefix = bmake_strsedup(pattern, wildcardComponent + 1);
	/*
	 * XXX: Check the "the directory is added to the path" part.
	 * It is probably surprising that the directory before a
	 * wildcard gets added to the path.
	 */
	/*
	 * XXX: Only the first match of the prefix in the path is
	 * taken, any others are ignored.  The expectation may be
	 * that the pattern is expanded in the whole path.
	 */
	dirpath = Dir_FindFile(prefix, path);
	free(prefix);

	/*
	 * dirpath is null if can't find the leading component
	 *
	 * XXX: Dir_FindFile won't find internal components.  i.e. if the
	 * path contains ../Etc/Object and we're looking for Etc, it won't
	 * be found.  Ah well.  Probably not important.
	 *
	 * XXX: Check whether the above comment is still true.
	 */
	if (dirpath == NULL)
		return;

	end = &dirpath[strlen(dirpath) - 1];
	/* XXX: What about multiple trailing slashes? */
	if (*end == '/')
		*end = '\0';

	partPath = SearchPath_New();
	(void)SearchPath_Add(partPath, dirpath);
	DirExpandPath(wildcardComponent + 1, partPath, expansions);
	SearchPath_Free(partPath);
}

/*
 * Expand the given pattern into a list of existing filenames by globbing it,
 * looking in each directory from the search path.
 *
 * Input:
 *	path		the directories in which to find the files
 *	pattern		the pattern to expand
 *	expansions	the list on which to place the results
 */
void
SearchPath_Expand(SearchPath *path, const char *pattern, StringList *expansions)
{
	const char *brace, *slash, *wildcard, *wildcardComponent;

	assert(path != NULL);
	assert(expansions != NULL);

	DEBUG1(DIR, "Expanding \"%s\"... ", pattern);

	brace = strchr(pattern, '{');
	if (brace != NULL) {
		DirExpandCurly(pattern, brace, path, expansions);
		goto done;
	}

	/* At this point, the pattern does not contain '{'. */

	slash = strchr(pattern, '/');
	if (slash == NULL) {
		/* The pattern has no directory component. */

		/* First the files in dot. */
		DirMatchFiles(pattern, dot, expansions);
		/* Then the files in every other directory on the path. */
		DirExpandPath(pattern, path, expansions);
		goto done;
	}

	/* At this point, the pattern has a directory component. */

	/* Find the first wildcard in the pattern. */
	for (wildcard = pattern; *wildcard != '\0'; wildcard++)
		if (*wildcard == '?' || *wildcard == '[' || *wildcard == '*')
			break;

	if (*wildcard == '\0') {
		/*
		 * No directory component and no wildcard at all -- this
		 * should never happen as in such a simple case there is no
		 * need to expand anything.
		 */
		DirExpandPath(pattern, path, expansions);
		goto done;
	}

	/* Back up to the start of the component containing the wildcard. */
	/* XXX: This handles '///' and '/' differently. */
	wildcardComponent = wildcard;
	while (wildcardComponent > pattern && *wildcardComponent != '/')
		wildcardComponent--;

	if (wildcardComponent == pattern) {
		/* The first component contains the wildcard. */
		/* Start the search from the local directory */
		DirExpandPath(pattern, path, expansions);
	} else {
		SearchPath_ExpandMiddle(path, pattern, wildcardComponent,
		    expansions);
	}

done:
	if (DEBUG(DIR))
		PrintExpansions(expansions);
}

/*
 * Find if the file with the given name exists in the given path.
 * Return the freshly allocated path to the file, or NULL.
 */
static char *
DirLookup(CachedDir *dir, const char *base)
{
	char *file;		/* the current filename to check */

	DEBUG1(DIR, "   %s ...\n", dir->name);

	if (!HashSet_Contains(&dir->files, base))
		return NULL;

	file = str_concat3(dir->name, "/", base);
	DEBUG1(DIR, "   returning %s\n", file);
	dir->hits++;
	hits++;
	return file;
}


/*
 * Find if the file with the given name exists in the given directory.
 * Return the freshly allocated path to the file, or NULL.
 */
static char *
DirLookupSubdir(CachedDir *dir, const char *name)
{
	struct cached_stat cst;
	char *file = dir == dot ? bmake_strdup(name)
				: str_concat3(dir->name, "/", name);

	DEBUG1(DIR, "checking %s ...\n", file);

	if (cached_stat(file, &cst) == 0) {
		nearmisses++;
		return file;
	}
	free(file);
	return NULL;
}

/*
 * Find if the file with the given name exists in the given path.
 * Return the freshly allocated path to the file, the empty string, or NULL.
 * Returning the empty string means that the search should be terminated.
 */
static char *
DirLookupAbs(CachedDir *dir, const char *name, const char *cp)
{
	const char *dnp;	/* pointer into dir->name */
	const char *np;		/* pointer into name */

	DEBUG1(DIR, "   %s ...\n", dir->name);

	/*
	 * If the file has a leading path component and that component
	 * exactly matches the entire name of the current search
	 * directory, we can attempt another cache lookup. And if we don't
	 * have a hit, we can safely assume the file does not exist at all.
	 */
	for (dnp = dir->name, np = name;
	     *dnp != '\0' && *dnp == *np; dnp++, np++)
		continue;
	if (*dnp != '\0' || np != cp - 1)
		return NULL;

	if (!HashSet_Contains(&dir->files, cp)) {
		DEBUG0(DIR, "   must be here but isn't -- returning\n");
		return bmake_strdup("");	/* to terminate the search */
	}

	dir->hits++;
	hits++;
	DEBUG1(DIR, "   returning %s\n", name);
	return bmake_strdup(name);
}

/*
 * Find the file given on "." or curdir.
 * Return the freshly allocated path to the file, or NULL.
 */
static char *
DirFindDot(const char *name, const char *base)
{

	if (HashSet_Contains(&dot->files, base)) {
		DEBUG0(DIR, "   in '.'\n");
		hits++;
		dot->hits++;
		return bmake_strdup(name);
	}

	if (cur != NULL && HashSet_Contains(&cur->files, base)) {
		DEBUG1(DIR, "   in ${.CURDIR} = %s\n", cur->name);
		hits++;
		cur->hits++;
		return str_concat3(cur->name, "/", base);
	}

	return NULL;
}

static Boolean
FindFileRelative(SearchPath *path, Boolean seenDotLast,
		 const char *name, char **out_file)
{
	SearchPathNode *ln;
	Boolean checkedDot = FALSE;
	char *file;

	DEBUG0(DIR, "   Trying subdirectories...\n");

	if (!seenDotLast) {
		if (dot != NULL) {
			checkedDot = TRUE;
			if ((file = DirLookupSubdir(dot, name)) != NULL)
				goto found;
		}
		if (cur != NULL &&
		    (file = DirLookupSubdir(cur, name)) != NULL)
			goto found;
	}

	for (ln = path->dirs.first; ln != NULL; ln = ln->next) {
		CachedDir *dir = ln->datum;
		if (dir == dotLast)
			continue;
		if (dir == dot) {
			if (checkedDot)
				continue;
			checkedDot = TRUE;
		}
		if ((file = DirLookupSubdir(dir, name)) != NULL)
			goto found;
	}

	if (seenDotLast) {
		if (dot != NULL && !checkedDot) {
			checkedDot = TRUE;
			if ((file = DirLookupSubdir(dot, name)) != NULL)
				goto found;
		}
		if (cur != NULL &&
		    (file = DirLookupSubdir(cur, name)) != NULL)
			goto found;
	}

	if (checkedDot) {
		/*
		 * Already checked by the given name, since . was in
		 * the path, so no point in proceeding.
		 */
		DEBUG0(DIR, "   Checked . already, returning NULL\n");
		file = NULL;
		goto found;
	}

	return FALSE;

found:
	*out_file = file;
	return TRUE;
}

static Boolean
FindFileAbsolute(SearchPath *path, Boolean const seenDotLast,
		 const char *const name, const char *const base,
		 char **out_file)
{
	char *file;
	SearchPathNode *ln;

	/*
	 * For absolute names, compare directory path prefix against
	 * the the directory path of each member on the search path
	 * for an exact match. If we have an exact match on any member
	 * of the search path, use the cached contents of that member
	 * to lookup the final file component. If that lookup fails we
	 * can safely assume that the file does not exist at all.
	 * This is signified by DirLookupAbs() returning an empty
	 * string.
	 */
	DEBUG0(DIR, "   Trying exact path matches...\n");

	if (!seenDotLast && cur != NULL &&
	    ((file = DirLookupAbs(cur, name, base)) != NULL))
		goto found;

	for (ln = path->dirs.first; ln != NULL; ln = ln->next) {
		CachedDir *dir = ln->datum;
		if (dir == dotLast)
			continue;
		if ((file = DirLookupAbs(dir, name, base)) != NULL)
			goto found;
	}

	if (seenDotLast && cur != NULL &&
	    ((file = DirLookupAbs(cur, name, base)) != NULL))
		goto found;

	return FALSE;

found:
	if (file[0] == '\0') {
		free(file);
		file = NULL;
	}
	*out_file = file;
	return TRUE;
}

/*
 * Find the file with the given name along the given search path.
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
	char *file;		/* the current filename to check */
	Boolean seenDotLast = FALSE; /* true if we should search dot last */
	struct cached_stat cst;	/* Buffer for stat, if necessary */
	const char *trailing_dot = ".";
	const char *base = str_basename(name);

	DEBUG1(DIR, "Searching for %s ...", name);

	if (path == NULL) {
		DEBUG0(DIR, "couldn't open path, file not found\n");
		misses++;
		return NULL;
	}

	if (path->dirs.first != NULL) {
		CachedDir *dir = path->dirs.first->datum;
		if (dir == dotLast) {
			seenDotLast = TRUE;
			DEBUG0(DIR, "[dot last]...");
		}
	}
	DEBUG0(DIR, "\n");

	/*
	 * If there's no leading directory components or if the leading
	 * directory component is exactly `./', consult the cached contents
	 * of each of the directories on the search path.
	 */
	if (base == name || (base - name == 2 && *name == '.')) {
		SearchPathNode *ln;

		/*
		 * We look through all the directories on the path seeking one
		 * which contains the final component of the given name.  If
		 * such a file is found, we concatenate the directory name
		 * and the final component and return the resulting string.
		 * If we don't find any such thing, we go on to phase two.
		 *
		 * No matter what, we always look for the file in the current
		 * directory before anywhere else (unless we found the magic
		 * DOTLAST path, in which case we search it last) and we *do
		 * not* add the ./ to it if it exists.
		 * This is so there are no conflicts between what the user
		 * specifies (fish.c) and what pmake finds (./fish.c).
		 */
		if (!seenDotLast && (file = DirFindDot(name, base)) != NULL)
			return file;

		for (ln = path->dirs.first; ln != NULL; ln = ln->next) {
			CachedDir *dir = ln->datum;
			if (dir == dotLast)
				continue;
			if ((file = DirLookup(dir, base)) != NULL)
				return file;
		}

		if (seenDotLast && (file = DirFindDot(name, base)) != NULL)
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
	 * [ This no longer applies: If we find such a file, we assume there
	 * will be more (what else can we assume?) and add all but the last
	 * component of the resulting name onto the search path (at the
	 * end).]
	 * This phase is only performed if the file is *not* absolute.
	 */
	if (base == name) {
		DEBUG0(DIR, "   failed.\n");
		misses++;
		return NULL;
	}

	if (*base == '\0') {
		/* we were given a trailing "/" */
		base = trailing_dot;
	}

	if (name[0] != '/') {
		if (FindFileRelative(path, seenDotLast, name, &file))
			return file;
	} else {
		if (FindFileAbsolute(path, seenDotLast, name, base, &file))
			return file;
	}

	/*
	 * Didn't find it that way, either. Sigh. Phase 3. Add its directory
	 * onto the search path in any case, just in case, then look for the
	 * thing in the hash table. If we find it, grand. We return a new
	 * copy of the name. Otherwise we sadly return a NULL pointer. Sigh.
	 * Note that if the directory holding the file doesn't exist, this
	 * will do an extra search of the final directory on the path. Unless
	 * something weird happens, this search won't succeed and life will
	 * be groovy.
	 *
	 * Sigh. We cannot add the directory onto the search path because
	 * of this amusing case:
	 * $(INSTALLDIR)/$(FILE): $(FILE)
	 *
	 * $(FILE) exists in $(INSTALLDIR) but not in the current one.
	 * When searching for $(FILE), we will find it in $(INSTALLDIR)
	 * b/c we added it here. This is not good...
	 */
#if 0
	{
		CachedDir *dir;
		char *prefix;

		if (base == trailing_dot) {
			base = strrchr(name, '/');
			base++;
		}
		prefix = bmake_strsedup(name, base - 1);
		(void)SearchPath_Add(path, prefix);
		free(prefix);

		bigmisses++;
		if (path->last == NULL)
			return NULL;

		dir = path->last->datum;
		if (HashSet_Contains(&dir->files, base))
			return bmake_strdup(name);
		return NULL;
	}
#else
	DEBUG1(DIR, "   Looking for \"%s\" ...\n", name);

	bigmisses++;
	if (cached_stat(name, &cst) == 0) {
		return bmake_strdup(name);
	}

	DEBUG0(DIR, "   failed. Returning NULL\n");
	return NULL;
#endif
}


/*
 * Search for a path starting at a given directory and then working our way
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
	struct cached_stat cst;
	char *dirbase, *dirbase_end;
	char *try, *try_end;

	/* copy out our starting point */
	dirbase = bmake_strdup(here);
	dirbase_end = dirbase + strlen(dirbase);

	/* loop until we determine a result */
	for (;;) {

		/* try and stat(2) it ... */
		try = str_concat3(dirbase, "/", search_path);
		if (cached_stat(try, &cst) != -1) {
			/*
			 * success!  if we found a file, chop off
			 * the filename so we return a directory.
			 */
			if ((cst.cst_mode & S_IFMT) != S_IFDIR) {
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
			break;	/* failed! */

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

/*
 * This is an implied source, and it may have moved,
 * see if we can find it via the current .PATH
 */
static char *
ResolveMovedDepends(GNode *gn)
{
	char *fullName;

	const char *base = str_basename(gn->name);
	if (base == gn->name)
		return NULL;

	fullName = Dir_FindFile(base, Suff_FindPath(gn));
	if (fullName == NULL)
		return NULL;

	/*
	 * Put the found file in gn->path so that we give that to the compiler.
	 */
	/*
	 * XXX: Better just reset gn->path to NULL; updating it is already done
	 * by Dir_UpdateMTime.
	 */
	gn->path = bmake_strdup(fullName);
	if (!Job_RunTarget(".STALE", gn->fname))
		fprintf(stdout,	/* XXX: Why stdout? */
			"%s: %s, %d: ignoring stale %s for %s, found %s\n",
			progname, gn->fname, gn->lineno,
			makeDependfile, gn->name, fullName);

	return fullName;
}

static char *
ResolveFullName(GNode *gn)
{
	char *fullName;

	fullName = gn->path;
	if (fullName == NULL && !(gn->type & OP_NOPATH)) {

		fullName = Dir_FindFile(gn->name, Suff_FindPath(gn));

		if (fullName == NULL && gn->flags & FROM_DEPEND &&
		    !Lst_IsEmpty(&gn->implicitParents))
			fullName = ResolveMovedDepends(gn);

		DEBUG2(DIR, "Found '%s' as '%s'\n",
		    gn->name, fullName != NULL ? fullName : "(not found)");
	}

	if (fullName == NULL)
		fullName = bmake_strdup(gn->name);

	/* XXX: Is every piece of memory freed as it should? */

	return fullName;
}

/*
 * Search gn along dirSearchPath and store its modification time in gn->mtime.
 * If no file is found, store 0 instead.
 *
 * The found file is stored in gn->path, unless the node already had a path.
 */
void
Dir_UpdateMTime(GNode *gn, Boolean recheck)
{
	char *fullName;
	struct cached_stat cst;

	if (gn->type & OP_ARCHV) {
		Arch_UpdateMTime(gn);
		return;
	}

	if (gn->type & OP_PHONY) {
		gn->mtime = 0;
		return;
	}

	fullName = ResolveFullName(gn);

	if (cached_stats(fullName, &cst, recheck ? CST_UPDATE : CST_NONE) < 0) {
		if (gn->type & OP_MEMBER) {
			if (fullName != gn->path)
				free(fullName);
			Arch_UpdateMemberMTime(gn);
			return;
		}

		cst.cst_mtime = 0;
	}

	if (fullName != NULL && gn->path == NULL)
		gn->path = fullName;
	/* XXX: else free(fullName)? */

	gn->mtime = cst.cst_mtime;
}

/*
 * Read the directory and add it to the cache in openDirs.
 * If a path is given, add the directory to that path as well.
 */
static CachedDir *
CacheNewDir(const char *name, SearchPath *path)
{
	CachedDir *dir = NULL;
	DIR *d;
	struct dirent *dp;

	if ((d = opendir(name)) == NULL) {
		DEBUG1(DIR, "Caching %s ... not found\n", name);
		return dir;
	}

	DEBUG1(DIR, "Caching %s ...\n", name);

	dir = CachedDir_New(name);

	while ((dp = readdir(d)) != NULL) {

#if defined(sun) && defined(d_ino) /* d_ino is a sunos4 #define for d_fileno */
		/*
		 * The sun directory library doesn't check for a 0 inode
		 * (0-inode slots just take up space), so we have to do
		 * it ourselves.
		 */
		if (dp->d_fileno == 0)
			continue;
#endif /* sun && d_ino */

		(void)HashSet_Add(&dir->files, dp->d_name);
	}
	(void)closedir(d);

	OpenDirs_Add(&openDirs, dir);
	if (path != NULL)
		Lst_Append(&path->dirs, CachedDir_Ref(dir));

	DEBUG1(DIR, "Caching %s done\n", name);
	return dir;
}

/*
 * Read the list of filenames in the directory and store the result
 * in openDirs.
 *
 * If a path is given, append the directory to that path.
 *
 * Input:
 *	path		The path to which the directory should be
 *			added, or NULL to only add the directory to openDirs
 *	name		The name of the directory to add.
 *			The name is not normalized in any way.
 * Output:
 *	result		If no path is given and the directory exists, the
 *			returned CachedDir has a reference count of 0.  It
 *			must either be assigned to a variable using
 *			CachedDir_Assign or be appended to a SearchPath using
 *			Lst_Append and CachedDir_Ref.
 */
CachedDir *
SearchPath_Add(SearchPath *path, const char *name)
{

	if (path != NULL && strcmp(name, ".DOTLAST") == 0) {
		SearchPathNode *ln;

		/* XXX: Linear search gets slow with thousands of entries. */
		for (ln = path->dirs.first; ln != NULL; ln = ln->next) {
			CachedDir *pathDir = ln->datum;
			if (strcmp(pathDir->name, name) == 0)
				return pathDir;
		}

		Lst_Prepend(&path->dirs, CachedDir_Ref(dotLast));
	}

	if (path != NULL) {
		/* XXX: Why is OpenDirs only checked if path != NULL? */
		CachedDir *dir = OpenDirs_Find(&openDirs, name);
		if (dir != NULL) {
			if (Lst_FindDatum(&path->dirs, dir) == NULL)
				Lst_Append(&path->dirs, CachedDir_Ref(dir));
			return dir;
		}
	}

	return CacheNewDir(name, path);
}

/*
 * Return a copy of dirSearchPath, incrementing the reference counts for
 * the contained directories.
 */
SearchPath *
Dir_CopyDirSearchPath(void)
{
	SearchPath *path = SearchPath_New();
	SearchPathNode *ln;
	for (ln = dirSearchPath.dirs.first; ln != NULL; ln = ln->next) {
		CachedDir *dir = ln->datum;
		Lst_Append(&path->dirs, CachedDir_Ref(dir));
	}
	return path;
}

/*
 * Make a string by taking all the directories in the given search path and
 * preceding them by the given flag. Used by the suffix module to create
 * variables for compilers based on suffix search paths.
 *
 * Input:
 *	flag		flag which should precede each directory
 *	path		list of directories
 *
 * Results:
 *	The string mentioned above. Note that there is no space between the
 *	given flag and each directory. The empty string is returned if things
 *	don't go well.
 */
char *
SearchPath_ToFlags(SearchPath *path, const char *flag)
{
	Buffer buf;
	SearchPathNode *ln;

	Buf_Init(&buf);

	if (path != NULL) {
		for (ln = path->dirs.first; ln != NULL; ln = ln->next) {
			CachedDir *dir = ln->datum;
			Buf_AddStr(&buf, " ");
			Buf_AddStr(&buf, flag);
			Buf_AddStr(&buf, dir->name);
		}
	}

	return Buf_DoneData(&buf);
}

/* Free the search path and all directories mentioned in it. */
void
SearchPath_Free(SearchPath *path)
{
	SearchPathNode *ln;

	for (ln = path->dirs.first; ln != NULL; ln = ln->next) {
		CachedDir *dir = ln->datum;
		CachedDir_Unref(dir);
	}
	Lst_Done(&path->dirs);
	free(path);
}

/*
 * Clear out all elements from the given search path.
 * The path is set to the empty list but is not destroyed.
 */
void
SearchPath_Clear(SearchPath *path)
{
	while (!Lst_IsEmpty(&path->dirs)) {
		CachedDir *dir = Lst_Dequeue(&path->dirs);
		CachedDir_Unref(dir);
	}
}


/*
 * Concatenate two paths, adding the second to the end of the first,
 * skipping duplicates.
 */
void
SearchPath_AddAll(SearchPath *dst, SearchPath *src)
{
	SearchPathNode *ln;

	for (ln = src->dirs.first; ln != NULL; ln = ln->next) {
		CachedDir *dir = ln->datum;
		if (Lst_FindDatum(&dst->dirs, dir) == NULL)
			Lst_Append(&dst->dirs, CachedDir_Ref(dir));
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
	debug_printf(
	    "# Stats: %d hits %d misses %d near misses %d losers (%d%%)\n",
	    hits, misses, nearmisses, bigmisses,
	    percentage(hits, hits + bigmisses + nearmisses));
	debug_printf("#  refs  hits  directory\n");

	for (ln = openDirs.list.first; ln != NULL; ln = ln->next) {
		CachedDir *dir = ln->datum;
		debug_printf("#  %4d  %4d  %s\n",
		    dir->refCount, dir->hits, dir->name);
	}
}

void
SearchPath_Print(const SearchPath *path)
{
	SearchPathNode *ln;

	for (ln = path->dirs.first; ln != NULL; ln = ln->next) {
		const CachedDir *dir = ln->datum;
		debug_printf("%s ", dir->name);
	}
}
