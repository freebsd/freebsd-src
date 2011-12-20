/*-
 * Copyright (c) 2003-2007 Tim Kientzle
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR(S) ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR(S) BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY

 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*-
 * This is a new directory-walking system that addresses a number
 * of problems I've had with fts(3).  In particular, it has no
 * pathname-length limits (other than the size of 'int'), handles
 * deep logical traversals, uses considerably less memory, and has
 * an opaque interface (easier to modify in the future).
 *
 * Internally, it keeps a single list of "tree_entry" items that
 * represent filesystem objects that require further attention.
 * Non-directories are not kept in memory: they are pulled from
 * readdir(), returned to the client, then freed as soon as possible.
 * Any directory entry to be traversed gets pushed onto the stack.
 *
 * There is surprisingly little information that needs to be kept for
 * each item on the stack.  Just the name, depth (represented here as the
 * string length of the parent directory's pathname), and some markers
 * indicating how to get back to the parent (via chdir("..") for a
 * regular dir or via fchdir(2) for a symlink).
 */
#include "bsdtar_platform.h"
__FBSDID("$FreeBSD: src/usr.bin/tar/tree.c,v 1.9 2008/11/27 05:49:52 kientzle Exp $");

#ifdef HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif
#ifdef HAVE_DIRECT_H
#include <direct.h>
#endif
#ifdef HAVE_DIRENT_H
#include <dirent.h>
#endif
#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif
#ifdef HAVE_FCNTL_H
#include <fcntl.h>
#endif
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#if defined(HAVE_WINDOWS_H) && !defined(__CYGWIN__)
#include <windows.h>
#endif

#include "tree.h"

/*
 * TODO:
 *    1) Loop checking.
 *    3) Arbitrary logical traversals by closing/reopening intermediate fds.
 */

struct tree_entry {
	int depth;
	struct tree_entry *next;
	struct tree_entry *parent;
	char *name;
	size_t dirname_length;
	dev_t dev;
	ino_t ino;
	int flags;
	/* How to return back to the parent of a symlink. */
#ifdef HAVE_FCHDIR
	int symlink_parent_fd;
#elif defined(_WIN32) && !defined(__CYGWIN__)
	char *symlink_parent_path;
#else
#error fchdir function required.
#endif
};

/* Definitions for tree_entry.flags bitmap. */
#define	isDir 1 /* This entry is a regular directory. */
#define	isDirLink 2 /* This entry is a symbolic link to a directory. */
#define needsFirstVisit 4 /* This is an initial entry. */
#define	needsDescent 8 /* This entry needs to be previsited. */
#define needsOpen 16 /* This is a directory that needs to be opened. */
#define	needsAscent 32 /* This entry needs to be postvisited. */

/*
 * On Windows, "first visit" is handled as a pattern to be handed to
 * _findfirst().  This is consistent with Windows conventions that
 * file patterns are handled within the application.  On Posix,
 * "first visit" is just returned to the client.
 */

/*
 * Local data for this package.
 */
struct tree {
	struct tree_entry	*stack;
	struct tree_entry	*current;
#if defined(HAVE_WINDOWS_H) && !defined(__CYGWIN__)
	HANDLE d;
	BY_HANDLE_FILE_INFORMATION fileInfo;
#define INVALID_DIR_HANDLE INVALID_HANDLE_VALUE
	WIN32_FIND_DATA _findData;
	WIN32_FIND_DATA *findData;
#else
	DIR	*d;
#define INVALID_DIR_HANDLE NULL
	struct dirent *de;
#endif
	int	 flags;
	int	 visit_type;
	int	 tree_errno; /* Error code from last failed operation. */

	/* Dynamically-sized buffer for holding path */
	char	*buff;
	size_t	 buff_length;

	const char *basename; /* Last path element */
	size_t	 dirname_length; /* Leading dir length */
	size_t	 path_length; /* Total path length */

	int	 depth;
	int	 openCount;
	int	 maxOpenCount;

	struct stat	lst;
	struct stat	st;
};

/* Definitions for tree.flags bitmap. */
#define hasStat 16  /* The st entry is valid. */
#define hasLstat 32 /* The lst entry is valid. */
#define	hasFileInfo 64 /* The Windows fileInfo entry is valid. */

#if defined(_WIN32) && !defined(__CYGWIN__)
static int
tree_dir_next_windows(struct tree *t, const char *pattern);
#else
static int
tree_dir_next_posix(struct tree *t);
#endif

#ifdef HAVE_DIRENT_D_NAMLEN
/* BSD extension; avoids need for a strlen() call. */
#define D_NAMELEN(dp)	(dp)->d_namlen
#else
#define D_NAMELEN(dp)	(strlen((dp)->d_name))
#endif

#include <stdio.h>
void
tree_dump(struct tree *t, FILE *out)
{
	char buff[300];
	struct tree_entry *te;

	fprintf(out, "\tdepth: %d\n", t->depth);
	fprintf(out, "\tbuff: %s\n", t->buff);
	fprintf(out, "\tpwd: %s\n", getcwd(buff, sizeof(buff)));
	fprintf(out, "\tbasename: %s\n", t->basename);
	fprintf(out, "\tstack:\n");
	for (te = t->stack; te != NULL; te = te->next) {
		fprintf(out, "\t\t%s%d:\"%s\" %s%s%s%s%s%s\n",
		    t->current == te ? "*" : " ",
		    te->depth,
		    te->name,
		    te->flags & needsFirstVisit ? "V" : "",
		    te->flags & needsDescent ? "D" : "",
		    te->flags & needsOpen ? "O" : "",
		    te->flags & needsAscent ? "A" : "",
		    te->flags & isDirLink ? "L" : "",
		    (t->current == te && t->d) ? "+" : ""
		);
	}
}

/*
 * Add a directory path to the current stack.
 */
static void
tree_push(struct tree *t, const char *path)
{
	struct tree_entry *te;

	te = malloc(sizeof(*te));
	memset(te, 0, sizeof(*te));
	te->next = t->stack;
	te->parent = t->current;
	if (te->parent)
		te->depth = te->parent->depth + 1;
	t->stack = te;
#ifdef HAVE_FCHDIR
	te->symlink_parent_fd = -1;
	te->name = strdup(path);
#elif defined(_WIN32) && !defined(__CYGWIN__)
	te->symlink_parent_path = NULL;
	te->name = strdup(path);
#endif
	te->flags = needsDescent | needsOpen | needsAscent;
	te->dirname_length = t->dirname_length;
}

/*
 * Append a name to the current dir path.
 */
static void
tree_append(struct tree *t, const char *name, size_t name_length)
{
	char *p;
	size_t size_needed;

	if (t->buff != NULL)
		t->buff[t->dirname_length] = '\0';
	/* Strip trailing '/' from name, unless entire name is "/". */
	while (name_length > 1 && name[name_length - 1] == '/')
		name_length--;

	/* Resize pathname buffer as needed. */
	size_needed = name_length + 1 + t->dirname_length;
	if (t->buff_length < size_needed) {
		if (t->buff_length < 1024)
			t->buff_length = 1024;
		while (t->buff_length < size_needed)
			t->buff_length *= 2;
		t->buff = realloc(t->buff, t->buff_length);
	}
	if (t->buff == NULL)
		abort();
	p = t->buff + t->dirname_length;
	t->path_length = t->dirname_length + name_length;
	/* Add a separating '/' if it's needed. */
	if (t->dirname_length > 0 && p[-1] != '/') {
		*p++ = '/';
		t->path_length ++;
	}
#if HAVE_STRNCPY_S
	strncpy_s(p, t->buff_length - (p - t->buff), name, name_length);
#else
	strncpy(p, name, name_length);
#endif
	p[name_length] = '\0';
	t->basename = p;
}

/*
 * Open a directory tree for traversal.
 */
struct tree *
tree_open(const char *path)
{
#ifdef HAVE_FCHDIR
	struct tree *t;

	t = malloc(sizeof(*t));
	memset(t, 0, sizeof(*t));
	/* First item is set up a lot like a symlink traversal. */
	tree_push(t, path);
	t->stack->flags = needsFirstVisit | isDirLink | needsAscent;
	t->stack->symlink_parent_fd = open(".", O_RDONLY);
	t->openCount++;
	t->d = INVALID_DIR_HANDLE;
	return (t);
#elif defined(_WIN32) && !defined(__CYGWIN__)
	struct tree *t;
	char *cwd = _getcwd(NULL, 0);
	char *pathname = strdup(path), *p, *base;

	if (pathname == NULL)
		abort();
	for (p = pathname; *p != '\0'; ++p) {
		if (*p == '\\')
			*p = '/';
	}
	base = pathname;

	t = malloc(sizeof(*t));
	memset(t, 0, sizeof(*t));
	/* First item is set up a lot like a symlink traversal. */
	/* printf("Looking for wildcard in %s\n", path); */
	/* TODO: wildcard detection here screws up on \\?\c:\ UNC names */
	if (strchr(base, '*') || strchr(base, '?')) {
		// It has a wildcard in it...
		// Separate the last element.
		p = strrchr(base, '/');
		if (p != NULL) {
			*p = '\0';
			chdir(base);
			tree_append(t, base, p - base);
			t->dirname_length = t->path_length;
			base = p + 1;
		}
	}
	tree_push(t, base);
	free(pathname);
	t->stack->flags = needsFirstVisit | isDirLink | needsAscent;
	t->stack->symlink_parent_path = cwd;
	t->d = INVALID_DIR_HANDLE;
	return (t);
#endif
}

/*
 * We've finished a directory; ascend back to the parent.
 */
static int
tree_ascend(struct tree *t)
{
	struct tree_entry *te;
	int r = 0;

	te = t->stack;
	t->depth--;
	if (te->flags & isDirLink) {
#ifdef HAVE_FCHDIR
		if (fchdir(te->symlink_parent_fd) != 0) {
			t->tree_errno = errno;
			r = TREE_ERROR_FATAL;
		}
		close(te->symlink_parent_fd);
#elif defined(_WIN32) && !defined(__CYGWIN__)
		if (SetCurrentDirectory(te->symlink_parent_path) == 0) {
			t->tree_errno = errno;
			r = TREE_ERROR_FATAL;
		}
		free(te->symlink_parent_path);
		te->symlink_parent_path = NULL;
#endif
		t->openCount--;
	} else {
#if defined(_WIN32) && !defined(__CYGWIN__)
		if (SetCurrentDirectory("..") == 0) {
#else
		if (chdir("..") != 0) {
#endif
			t->tree_errno = errno;
			r = TREE_ERROR_FATAL;
		}
	}
	return (r);
}

/*
 * Pop the working stack.
 */
static void
tree_pop(struct tree *t)
{
	struct tree_entry *te;

	if (t->buff)
		t->buff[t->dirname_length] = '\0';
	if (t->stack == t->current && t->current != NULL)
		t->current = t->current->parent;
	te = t->stack;
	t->stack = te->next;
	t->dirname_length = te->dirname_length;
	if (t->buff) {
		t->basename = t->buff + t->dirname_length;
		while (t->basename[0] == '/')
			t->basename++;
	}
	free(te->name);
	free(te);
}

/*
 * Get the next item in the tree traversal.
 */
int
tree_next(struct tree *t)
{
	int r;

	/* If we're called again after a fatal error, that's an API
	 * violation.  Just crash now. */
	if (t->visit_type == TREE_ERROR_FATAL) {
		fprintf(stderr, "Unable to continue traversing"
		    " directory heirarchy after a fatal error.");
		abort();
	}

	while (t->stack != NULL) {
		/* If there's an open dir, get the next entry from there. */
		if (t->d != INVALID_DIR_HANDLE) {
#if defined(_WIN32) && !defined(__CYGWIN__)
			r = tree_dir_next_windows(t, NULL);
#else
			r = tree_dir_next_posix(t);
#endif
			if (r == 0)
				continue;
			return (r);
		}

		if (t->stack->flags & needsFirstVisit) {
#if defined(_WIN32) && !defined(__CYGWIN__)
			char *d = t->stack->name;
			t->stack->flags &= ~needsFirstVisit;
			if (strchr(d, '*') || strchr(d, '?')) {
				r = tree_dir_next_windows(t, d);
				if (r == 0)
					continue;
				return (r);
			}
			// Not a pattern, handle it as-is...
#endif
			/* Top stack item needs a regular visit. */
			t->current = t->stack;
			tree_append(t, t->stack->name, strlen(t->stack->name));
			//t->dirname_length = t->path_length;
			//tree_pop(t);
			t->stack->flags &= ~needsFirstVisit;
			return (t->visit_type = TREE_REGULAR);
		} else if (t->stack->flags & needsDescent) {
			/* Top stack item is dir to descend into. */
			t->current = t->stack;
			tree_append(t, t->stack->name, strlen(t->stack->name));
			t->stack->flags &= ~needsDescent;
			/* If it is a link, set up fd for the ascent. */
			if (t->stack->flags & isDirLink) {
#ifdef HAVE_FCHDIR
				t->stack->symlink_parent_fd = open(".", O_RDONLY);
				t->openCount++;
				if (t->openCount > t->maxOpenCount)
					t->maxOpenCount = t->openCount;
#elif defined(_WIN32) && !defined(__CYGWIN__)
				t->stack->symlink_parent_path = _getcwd(NULL, 0);
#endif
			}
			t->dirname_length = t->path_length;
#if defined(_WIN32) && !defined(__CYGWIN__)
			if (t->path_length == 259 || !SetCurrentDirectory(t->stack->name) != 0)
#else
			if (chdir(t->stack->name) != 0)
#endif
			{
				/* chdir() failed; return error */
				tree_pop(t);
				t->tree_errno = errno;
				return (t->visit_type = TREE_ERROR_DIR);
			}
			t->depth++;
			return (t->visit_type = TREE_POSTDESCENT);
		} else if (t->stack->flags & needsOpen) {
			t->stack->flags &= ~needsOpen;
#if defined(_WIN32) && !defined(__CYGWIN__)
			r = tree_dir_next_windows(t, "*");
#else
			r = tree_dir_next_posix(t);
#endif
			if (r == 0)
				continue;
			return (r);
		} else if (t->stack->flags & needsAscent) {
		        /* Top stack item is dir and we're done with it. */
			r = tree_ascend(t);
			tree_pop(t);
			t->visit_type = r != 0 ? r : TREE_POSTASCENT;
			return (t->visit_type);
		} else {
			/* Top item on stack is dead. */
			tree_pop(t);
			t->flags &= ~hasLstat;
			t->flags &= ~hasStat;
		}
	}
	return (t->visit_type = 0);
}

#if defined(_WIN32) && !defined(__CYGWIN__)
static int
tree_dir_next_windows(struct tree *t, const char *pattern)
{
	const char *name;
	size_t namelen;
	int r;

	for (;;) {
		if (pattern != NULL) {
			t->d = FindFirstFile(pattern, &t->_findData);
			if (t->d == INVALID_DIR_HANDLE) {
				r = tree_ascend(t); /* Undo "chdir" */
				tree_pop(t);
				t->tree_errno = errno;
				t->visit_type = r != 0 ? r : TREE_ERROR_DIR;
				return (t->visit_type);
			}
			t->findData = &t->_findData;
			pattern = NULL;
		} else if (!FindNextFile(t->d, &t->_findData)) {
			FindClose(t->d);
			t->d = INVALID_DIR_HANDLE;
			t->findData = NULL;
			return (0);
		}
		name = t->findData->cFileName;
		namelen = strlen(name);
		t->flags &= ~hasLstat;
		t->flags &= ~hasStat;
		if (name[0] == '.' && name[1] == '\0')
			continue;
		if (name[0] == '.' && name[1] == '.' && name[2] == '\0')
			continue;
		tree_append(t, name, namelen);
		return (t->visit_type = TREE_REGULAR);
	}
}
#else
static int
tree_dir_next_posix(struct tree *t)
{
	int r;
	const char *name;
	size_t namelen;

	if (t->d == NULL) {
		if ((t->d = opendir(".")) == NULL) {
			r = tree_ascend(t); /* Undo "chdir" */
			tree_pop(t);
			t->tree_errno = errno;
			t->visit_type = r != 0 ? r : TREE_ERROR_DIR;
			return (t->visit_type);
		}
	}
	for (;;) {
		t->de = readdir(t->d);
		if (t->de == NULL) {
			closedir(t->d);
			t->d = INVALID_DIR_HANDLE;
			return (0);
		}
		name = t->de->d_name;
		namelen = D_NAMELEN(t->de);
		t->flags &= ~hasLstat;
		t->flags &= ~hasStat;
		if (name[0] == '.' && name[1] == '\0')
			continue;
		if (name[0] == '.' && name[1] == '.' && name[2] == '\0')
			continue;
		tree_append(t, name, namelen);
		return (t->visit_type = TREE_REGULAR);
	}
}
#endif

/*
 * Return error code.
 */
int
tree_errno(struct tree *t)
{
	return (t->tree_errno);
}

/*
 * Called by the client to mark the directory just returned from
 * tree_next() as needing to be visited.
 */
void
tree_descend(struct tree *t)
{
	if (t->visit_type != TREE_REGULAR)
		return;

	if (tree_current_is_physical_dir(t)) {
		tree_push(t, t->basename);
		t->stack->flags |= isDir;
	} else if (tree_current_is_dir(t)) {
		tree_push(t, t->basename);
		t->stack->flags |= isDirLink;
	}
}

/*
 * Get the stat() data for the entry just returned from tree_next().
 */
const struct stat *
tree_current_stat(struct tree *t)
{
	if (!(t->flags & hasStat)) {
		if (stat(tree_current_access_path(t), &t->st) != 0)
			return NULL;
		t->flags |= hasStat;
	}
	return (&t->st);
}

#if defined(HAVE_WINDOWS_H) && !defined(__CYGWIN__)
const BY_HANDLE_FILE_INFORMATION *
tree_current_file_information(struct tree *t)
{
	if (!(t->flags & hasFileInfo)) {
		HANDLE h = CreateFile(tree_current_access_path(t),
			0, 0, NULL,
			OPEN_EXISTING,
			FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT,
			NULL);
		if (h == INVALID_HANDLE_VALUE)
			return NULL;
		if (!GetFileInformationByHandle(h, &t->fileInfo)) {
			CloseHandle(h);
			return NULL;
		}
		CloseHandle(h);
		t->flags |= hasFileInfo;
	}
	return (&t->fileInfo);
}
#endif
/*
 * Get the lstat() data for the entry just returned from tree_next().
 */
const struct stat *
tree_current_lstat(struct tree *t)
{
#if defined(_WIN32) && !defined(__CYGWIN__)
	return (tree_current_stat(t));
#else
	if (!(t->flags & hasLstat)) {
		if (lstat(tree_current_access_path(t), &t->lst) != 0)
			return NULL;
		t->flags |= hasLstat;
	}
	return (&t->lst);
#endif
}

/*
 * Test whether current entry is a dir or link to a dir.
 */
int
tree_current_is_dir(struct tree *t)
{
#if defined(_WIN32) && !defined(__CYGWIN__)
	if (t->findData)
		return (t->findData->dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY);
	if (tree_current_file_information(t))
		return (t->fileInfo.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY);
	return (0);
#else
	const struct stat *st;
	/*
	 * If we already have lstat() info, then try some
	 * cheap tests to determine if this is a dir.
	 */
	if (t->flags & hasLstat) {
		/* If lstat() says it's a dir, it must be a dir. */
		if (S_ISDIR(tree_current_lstat(t)->st_mode))
			return 1;
		/* Not a dir; might be a link to a dir. */
		/* If it's not a link, then it's not a link to a dir. */
		if (!S_ISLNK(tree_current_lstat(t)->st_mode))
			return 0;
		/*
		 * It's a link, but we don't know what it's a link to,
		 * so we'll have to use stat().
		 */
	}

	st = tree_current_stat(t);
	/* If we can't stat it, it's not a dir. */
	if (st == NULL)
		return 0;
	/* Use the definitive test.  Hopefully this is cached. */
	return (S_ISDIR(st->st_mode));
#endif
}

/*
 * Test whether current entry is a physical directory.  Usually, we
 * already have at least one of stat() or lstat() in memory, so we
 * use tricks to try to avoid an extra trip to the disk.
 */
int
tree_current_is_physical_dir(struct tree *t)
{
#if defined(_WIN32) && !defined(__CYGWIN__)
	if (tree_current_is_physical_link(t))
		return (0);
	return (tree_current_is_dir(t));
#else
	const struct stat *st;

	/*
	 * If stat() says it isn't a dir, then it's not a dir.
	 * If stat() data is cached, this check is free, so do it first.
	 */
	if ((t->flags & hasStat)
	    && (!S_ISDIR(tree_current_stat(t)->st_mode)))
		return 0;

	/*
	 * Either stat() said it was a dir (in which case, we have
	 * to determine whether it's really a link to a dir) or
	 * stat() info wasn't available.  So we use lstat(), which
	 * hopefully is already cached.
	 */

	st = tree_current_lstat(t);
	/* If we can't stat it, it's not a dir. */
	if (st == NULL)
		return 0;
	/* Use the definitive test.  Hopefully this is cached. */
	return (S_ISDIR(st->st_mode));
#endif
}

/*
 * Test whether current entry is a symbolic link.
 */
int
tree_current_is_physical_link(struct tree *t)
{
#if defined(_WIN32) && !defined(__CYGWIN__)
#ifndef IO_REPARSE_TAG_SYMLINK
/* Old SDKs do not provide IO_REPARSE_TAG_SYMLINK */
#define IO_REPARSE_TAG_SYMLINK 0xA000000CL
#endif
	if (t->findData)
		return ((t->findData->dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT)
				&& (t->findData->dwReserved0 == IO_REPARSE_TAG_SYMLINK));
	return (0);
#else
	const struct stat *st = tree_current_lstat(t);
	if (st == NULL)
		return 0;
	return (S_ISLNK(st->st_mode));
#endif
}

/*
 * Return the access path for the entry just returned from tree_next().
 */
const char *
tree_current_access_path(struct tree *t)
{
	return (t->basename);
}

/*
 * Return the full path for the entry just returned from tree_next().
 */
const char *
tree_current_path(struct tree *t)
{
	return (t->buff);
}

/*
 * Return the length of the path for the entry just returned from tree_next().
 */
size_t
tree_current_pathlen(struct tree *t)
{
	return (t->path_length);
}

/*
 * Return the nesting depth of the entry just returned from tree_next().
 */
int
tree_current_depth(struct tree *t)
{
	return (t->depth);
}

/*
 * Terminate the traversal and release any resources.
 */
void
tree_close(struct tree *t)
{
	/* Release anything remaining in the stack. */
	while (t->stack != NULL)
		tree_pop(t);
	free(t->buff);
	/* TODO: Ensure that premature close() resets cwd */
#if 0
#ifdef HAVE_FCHDIR
	if (t->initialDirFd >= 0) {
		int s = fchdir(t->initialDirFd);
		(void)s; /* UNUSED */
		close(t->initialDirFd);
		t->initialDirFd = -1;
	}
#elif defined(_WIN32) && !defined(__CYGWIN__)
	if (t->initialDir != NULL) {
		SetCurrentDir(t->initialDir);
		free(t->initialDir);
		t->initialDir = NULL;
	}
#endif
#endif
	free(t);
}
