/*-
 * Copyright (c) 2003-2004 Tim Kientzle
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer
 *    in this position and unchanged.
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
__FBSDID("$FreeBSD$");

#include <sys/stat.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "tree.h"

/*
 * TODO:
 *    1) Loop checking.
 *    3) Arbitrary logical traversals by closing/reopening intermediate fds.
 */

struct tree_entry {
	struct tree_entry *next;
	struct tree_entry *parent;
	char *name;
	size_t dirname_length;
	dev_t dev;
	ino_t ino;
	int fd;
	int flags;
};

/* Definitions for tree_entry.flags bitmap. */
#define	isDir 1 /* This entry is a regular directory. */
#define	isDirLink 2 /* This entry is a symbolic link to a directory. */
#define	needsPreVisit 4 /* This entry needs to be previsited. */
#define	needsPostVisit 8 /* This entry needs to be postvisited. */

/*
 * Local data for this package.
 */
struct tree {
	struct tree_entry	*stack;
	struct tree_entry	*current;
	DIR	*d;
	int	 initialDirFd;
	int	 flags;
	int	 visit_type;
	int	 tree_errno; /* Error code from last failed operation. */

	char	*buff;
	const char	*basename;
	size_t	 buff_length;
	size_t	 path_length;
	size_t	 dirname_length;

	int	 depth;
	int	 openCount;
	int	 maxOpenCount;

	struct stat	lst;
	struct stat	st;
};

/* Definitions for tree.flags bitmap. */
#define needsReturn 8  /* Marks first entry as not having been returned yet. */
#define hasStat 16  /* The st entry is set. */
#define hasLstat 32 /* The lst entry is set. */


#ifdef HAVE_DIRENT_D_NAMLEN
/* BSD extension; avoids need for a strlen() call. */
#define D_NAMELEN(dp)	(dp)->d_namlen
#else
#define D_NAMELEN(dp)	(strlen((dp)->d_name))
#endif

#if 0
#include <stdio.h>
void
tree_dump(struct tree *t, FILE *out)
{
	struct tree_entry *te;

	fprintf(out, "\tdepth: %d\n", t->depth);
	fprintf(out, "\tbuff: %s\n", t->buff);
	fprintf(out, "\tpwd: "); fflush(stdout); system("pwd");
	fprintf(out, "\taccess: %s\n", t->basename);
	fprintf(out, "\tstack:\n");
	for (te = t->stack; te != NULL; te = te->next) {
		fprintf(out, "\t\tte->name: %s%s%s\n", te->name,
		    te->flags & needsPreVisit ? "" : " *",
		    t->current == te ? " (current)" : "");
	}
}
#endif

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
	t->stack = te;
	te->fd = -1;
	te->name = strdup(path);
	te->flags = needsPreVisit | needsPostVisit;
	te->dirname_length = t->dirname_length;
}

/*
 * Append a name to the current path.
 */
static void
tree_append(struct tree *t, const char *name, size_t name_length)
{
	char *p;

	if (t->buff != NULL)
		t->buff[t->dirname_length] = '\0';
	/* Strip trailing '/' from name, unless entire name is "/". */
	while (name_length > 1 && name[name_length - 1] == '/')
		name_length--;

	/* Resize pathname buffer as needed. */
	while (name_length + 1 + t->dirname_length >= t->buff_length) {
		t->buff_length *= 2;
		if (t->buff_length < 1024)
			t->buff_length = 1024;
		t->buff = realloc(t->buff, t->buff_length);
	}
	p = t->buff + t->dirname_length;
	t->path_length = t->dirname_length + name_length;
	/* Add a separating '/' if it's needed. */
	if (t->dirname_length > 0 && p[-1] != '/') {
		*p++ = '/';
		t->path_length ++;
	}
	strncpy(p, name, name_length);
	p[name_length] = '\0';
	t->basename = p;
}

/*
 * Open a directory tree for traversal.
 */
struct tree *
tree_open(const char *path)
{
	struct tree *t;

	t = malloc(sizeof(*t));
	memset(t, 0, sizeof(*t));
	tree_append(t, path, strlen(path));
	t->initialDirFd = open(".", O_RDONLY);
	/*
	 * During most of the traversal, items are set up and then
	 * returned immediately from tree_next().  That doesn't work
	 * for the very first entry, so we set a flag for this special
	 * case.
	 */
	t->flags = needsReturn;
	return (t);
}

/*
 * We've finished a directory; ascend back to the parent.
 */
static void
tree_ascend(struct tree *t)
{
	struct tree_entry *te;

	te = t->stack;
	t->depth--;
	if (te->flags & isDirLink) {
		fchdir(te->fd);
		close(te->fd);
		t->openCount--;
	} else {
		chdir("..");
	}
}

/*
 * Pop the working stack.
 */
static void
tree_pop(struct tree *t)
{
	struct tree_entry *te;

	t->buff[t->dirname_length] = '\0';
	if (t->stack == t->current && t->current != NULL)
		t->current = t->current->parent;
	te = t->stack;
	t->stack = te->next;
	t->dirname_length = te->dirname_length;
	t->basename = t->buff + t->dirname_length;
	/* Special case: starting dir doesn't skip leading '/'. */
	if (t->dirname_length > 0)
		t->basename++;
	free(te->name);
	free(te);
}

/*
 * Get the next item in the tree traversal.
 */
int
tree_next(struct tree *t)
{
	struct dirent *de = NULL;

	/* Handle the startup case by returning the initial entry. */
	if (t->flags & needsReturn) {
		t->flags &= ~needsReturn;
		return (t->visit_type = TREE_REGULAR);
	}

	while (t->stack != NULL) {
		/* If there's an open dir, get the next entry from there. */
		while (t->d != NULL) {
			de = readdir(t->d);
			if (de == NULL) {
				closedir(t->d);
				t->d = NULL;
			} else if (de->d_name[0] == '.'
			    && de->d_name[1] == '\0') {
				/* Skip '.' */
			} else if (de->d_name[0] == '.'
			    && de->d_name[1] == '.'
			    && de->d_name[2] == '\0') {
				/* Skip '..' */
			} else {
				/*
				 * Append the path to the current path
				 * and return it.
				 */
				tree_append(t, de->d_name, D_NAMELEN(de));
				t->flags &= ~hasLstat;
				t->flags &= ~hasStat;
				return (t->visit_type = TREE_REGULAR);
			}
		}

		/* If the current dir needs to be visited, set it up. */
		if (t->stack->flags & needsPreVisit) {
			t->current = t->stack;
			tree_append(t, t->stack->name, strlen(t->stack->name));
			t->stack->flags &= ~needsPreVisit;
			/* If it is a link, set up fd for the ascent. */
			if (t->stack->flags & isDirLink) {
				t->stack->fd = open(".", O_RDONLY);
				t->openCount++;
				if (t->openCount > t->maxOpenCount)
					t->maxOpenCount = t->openCount;
			}
			t->dirname_length = t->path_length;
			if (chdir(t->stack->name) != 0) {
				/* chdir() failed; return error */
				tree_pop(t);
				t->tree_errno = errno;
				return (t->visit_type = TREE_ERROR_DIR);
			}
			t->depth++;
			t->d = opendir(".");
			if (t->d == NULL) {
				tree_ascend(t); /* Undo "chdir" */
				tree_pop(t);
				t->tree_errno = errno;
				return (t->visit_type = TREE_ERROR_DIR);
			}
			t->flags &= ~hasLstat;
			t->flags &= ~hasStat;
			t->basename = ".";
			return (t->visit_type = TREE_POSTDESCENT);
		}

		/* We've done everything necessary for the top stack entry. */
		if (t->stack->flags & needsPostVisit) {
			tree_ascend(t);
			tree_pop(t);
			t->flags &= ~hasLstat;
			t->flags &= ~hasStat;
			return (t->visit_type = TREE_POSTASCENT);
		}
	}
	return (t->visit_type = 0);
}

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
		if (stat(t->basename, &t->st) != 0)
			return NULL;
		t->flags |= hasStat;
	}
	return (&t->st);
}

/*
 * Get the lstat() data for the entry just returned from tree_next().
 */
const struct stat *
tree_current_lstat(struct tree *t)
{
	if (!(t->flags & hasLstat)) {
		if (lstat(t->basename, &t->lst) != 0)
			return NULL;
		t->flags |= hasLstat;
	}
	return (&t->lst);
}

/*
 * Test whether current entry is a dir or dir link.
 */
int
tree_current_is_dir(struct tree *t)
{
	/* If we've already pulled stat(), just use that. */
	if (t->flags & hasStat)
		return (S_ISDIR(tree_current_stat(t)->st_mode));

	/* If we've already pulled lstat(), we may be able to use that. */
	if (t->flags & hasLstat) {
		/* If lstat() says it's a dir, it must be a dir. */
		if (S_ISDIR(tree_current_lstat(t)->st_mode))
			return 1;
		/* If it's not a dir and not a link, we're done. */
		if (!S_ISLNK(tree_current_lstat(t)->st_mode))
			return 0;
		/*
		 * If the above two tests fail, then it's a link, but
		 * we don't know whether it's a link to a dir or a
		 * non-dir.
		 */
	}

	/* TODO: Use a more efficient mechanism when available. */
	return (S_ISDIR(tree_current_stat(t)->st_mode));
}

/*
 * Test whether current entry is a physical directory.
 */
int
tree_current_is_physical_dir(struct tree *t)
{
	/* If we've already pulled lstat(), just use that. */
	if (t->flags & hasLstat)
		return (S_ISDIR(tree_current_lstat(t)->st_mode));

	/* TODO: Use a more efficient mechanism when available. */
	return (S_ISDIR(tree_current_lstat(t)->st_mode));
}

/*
 * Test whether current entry is a symbolic link.
 */
int
tree_current_is_physical_link(struct tree *t)
{
	/* If we've already pulled lstat(), just use that. */
	if (t->flags & hasLstat)
		return (S_ISLNK(tree_current_lstat(t)->st_mode));

	/* TODO: Use a more efficient mechanism when available. */
	return (S_ISLNK(tree_current_lstat(t)->st_mode));
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
	if (t->buff)
		free(t->buff);
	/* chdir() back to where we started. */
	if (t->initialDirFd >= 0) {
		fchdir(t->initialDirFd);
		close(t->initialDirFd);
		t->initialDirFd = -1;
	}
	free(t);
}
