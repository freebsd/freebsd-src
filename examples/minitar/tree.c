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
 * There is a single list of "tree_entry" items that represent
 * filesystem objects that require further attention.  Non-directories
 * are not kept in memory: they are pulled from readdir(), returned to
 * the client, then freed as soon as possible.  Any directory entry to
 * be traversed gets pushed onto the stack.
 *
 * There is surprisingly little information that needs to be kept for
 * each item on the stack.  Just the name, depth (represented here as the
 * string length of the parent directory's pathname), and some markers
 * indicating how to get back to the parent (via chdir("..") for a
 * regular dir or via fchdir(2) for a symlink).
 */

#include <sys/stat.h>
#include <dirent.h>
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
	char *name;
	size_t dirname_length;
	int fd;
	int flags;
};

/* Definitions for tree_entry.flags bitmap. */
#define isDir 1 /* This entry is a regular directory. */
#define isDirLink 2 /* This entry is a symbolic link to a directory. */
#define needsTraversal 4 /* This entry hasn't yet been traversed. */

/*
 * Local data for this package.
 */
struct tree {
	struct tree_entry	*stack;
	DIR	*d;
	int	 initialDirFd;
	int	 flags;

	char	*buff;
	char	*basename;
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


#define HAVE_DIRENT_D_NAMLEN 1
#ifdef HAVE_DIRENT_D_NAMLEN
/* BSD extension; avoids need for a strlen() call. */
#define D_NAMELEN(dp)	(dp)->d_namlen
#else
#define D_NAMELEN(dp)	(strlen((dp)->d_name))
#endif

#if 0
static void
dumpStack(struct tree *t)
{
	struct tree_entry *te;

	printf("\tbuff: %s\n", t->buff);
	printf("\tpwd: "); fflush(stdout); system("pwd");
	printf("\tstack:\n");
	for (te = t->stack; te != NULL; te = te->next) {
		printf("\t\tte->name: %s %s\n", te->name, te->flags & needsTraversal ? "" : "*");
	}
}
#endif

/*
 * Add a directory path to the current stack.
 */
static void
tree_add(struct tree *t, const char *path)
{
	struct tree_entry *te;

	te = malloc(sizeof(*te));
	memset(te, 0, sizeof(*te));
	te->next = t->stack;
	t->stack = te;
	te->fd = -1;
	te->name = strdup(path);
	te->flags = needsTraversal;
	te->dirname_length = t->dirname_length;
}

/*
 * Append a name to the current path.
 */
static void
tree_append(struct tree *t, const char *name, size_t name_length)
{
	if (t->buff != NULL)
		t->buff[t->dirname_length] = '\0';

	/* Resize pathname buffer as needed. */
	while (name_length + 1 + t->dirname_length >= t->buff_length) {
		t->buff_length *= 2;
		if (t->buff_length < 1024)
			t->buff_length = 1024;
		t->buff = realloc(t->buff, t->buff_length);
	}
	t->basename = t->buff + t->dirname_length;
	t->path_length = t->dirname_length + name_length;
	if (t->dirname_length > 0) {
		*t->basename++ = '/';
		t->path_length ++;
	}
	strcpy(t->basename, name);
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

	te = t->stack;
	t->stack = te->next;
	t->dirname_length = te->dirname_length;
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
		return (1);
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
				return (1);
			}
		}

		/* If the current dir needs to be traversed, set it up. */
		if (t->stack->flags & needsTraversal) {
			tree_append(t, t->stack->name, strlen(t->stack->name));
			t->stack->flags &= ~needsTraversal;
			/* If it is a link, set up fd for the ascent. */
			if (t->stack->flags & isDirLink) {
				t->stack->fd = open(".", O_RDONLY);
				t->openCount++;
				if (t->openCount > t->maxOpenCount)
					t->maxOpenCount = t->openCount;
			}
			if (chdir(t->stack->name) == 0) {
				t->depth++;
				t->dirname_length = t->path_length;
				t->d = opendir(".");
			} else
				tree_pop(t);
			continue;
		}

		/* We've done everything necessary for the top stack entry. */
		tree_ascend(t);
		tree_pop(t);
	}
	return (0);
}

/*
 * Called by the client to mark the directory just returned from
 * tree_next() as needing to be visited.
 */
void
tree_descend(struct tree *t)
{
	const struct stat *s = tree_current_lstat(t);

	if (S_ISDIR(s->st_mode)) {
		tree_add(t, t->basename);
		t->stack->flags |= isDir;
	}

	if (S_ISLNK(s->st_mode) && S_ISDIR(tree_current_stat(t)->st_mode)) {
		tree_add(t, t->basename);
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
		stat(t->basename, &t->st);
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
		lstat(t->basename, &t->lst);
		t->flags |= hasLstat;
	}
	return (&t->lst);
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


#if 0
/* Main function for testing. */
#include <stdio.h>

int main(int argc, char **argv)
{
	size_t max_path_len = 0;
	int max_depth = 0;

	system("pwd");
	while (*++argv) {
		struct tree *t = tree_open(*argv);
		while (tree_next(t)) {
			size_t path_len = tree_current_pathlen(t);
			int depth = tree_current_depth(t);
			if (path_len > max_path_len)
				max_path_len = path_len;
			if (depth > max_depth)
				max_depth = depth;
			printf("%s\n", tree_current_path(t));
			if (S_ISDIR(tree_current_lstat(t)->st_mode))
				tree_descend(t); /* Descend into every dir. */
		}
		tree_close(t);
		printf("Max path length: %d\n", max_path_len);
		printf("Max depth: %d\n", max_depth);
		printf("Final open count: %d\n", t->openCount);
		printf("Max open count: %d\n", t->maxOpenCount);
		fflush(stdout);
		system("pwd");
	}
	return (0);
}
#endif
