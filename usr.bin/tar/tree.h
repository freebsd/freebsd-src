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
 *
 * $FreeBSD$
 */

/*-
 * A set of routines for traversing directory trees.
 * Similar in concept to the fts library, but with a few
 * important differences:
 *    * Uses less memory.  In particular, fts stores an entire directory
 *      in memory at a time.  This package only keeps enough subdirectory
 *      information in memory to track the traversal.  Information
 *      about non-directories is discarded as soon as possible.
 *    * Supports very deep logical traversals.  The fts package
 *      uses "non-chdir" approach for logical traversals.  This
 *      package does use a chdir approach for logical traversals
 *      and can therefore handle pathnames much longer than
 *      PATH_MAX.
 *    * Supports deep physical traversals "out of the box."
 *      Due to the memory optimizations above, there's no need to
 *      limit dir names to 32k.
 */

#include <sys/stat.h>
#include <stdio.h>

struct tree;

/* Initiate/terminate a tree traversal. */
struct tree *tree_open(const char * /* pathname */);
void tree_close(struct tree *);

/*
 * tree_next() returns Zero if there is no next entry, non-zero if there is.
 * Note that directories are potentially visited three times.  The first
 * time as "regular" file.  If tree_descend() is invoked at that time,
 * the directory is added to a work list and will be visited two more
 * times:  once just after descending into the directory and again
 * just after ascending back to the parent.
 *
 * TREE_ERROR is returned if the descent failed (because the
 * directory couldn't be opened, for instance).  This is returned
 * instead of TREE_PREVISIT/TREE_POSTVISIT.
 */
#define	TREE_REGULAR	1
#define	TREE_POSTDESCENT	2
#define	TREE_POSTASCENT	3
#define	TREE_ERROR_DIR	-1
int tree_next(struct tree *);

int tree_errno(struct tree *);

/*
 * Request that current entry be visited.  If you invoke it on every
 * directory, you'll get a physical traversal.  This is ignored if the
 * current entry isn't a directory or a link to a directory.  So, if
 * you invoke this on every returned path, you'll get a full logical
 * traversal.
 */
void tree_descend(struct tree *);

/*
 * Return information about the current entry.
 */

int tree_current_depth(struct tree *);
/*
 * The current full pathname, length of the full pathname,
 * and a name that can be used to access the file.
 * Because tree does use chdir extensively, the access path is
 * almost never the same as the full current path.
 */
const char *tree_current_path(struct tree *);
size_t tree_current_pathlen(struct tree *);
const char *tree_current_access_path(struct tree *);
/*
 * Request the lstat() or stat() data for the current path.  Since the
 * tree package needs to do some of this anyway, and caches the
 * results, you should take advantage of it here if you need it rather
 * than make a redundant stat() or lstat() call of your own.
 */
const struct stat *tree_current_stat(struct tree *);
const struct stat *tree_current_lstat(struct tree *);
/* The following tests may use mechanisms much faster than stat()/lstat(). */
/* "is_physical_dir" is equivalent to S_ISDIR(tree_current_lstat()->st_mode) */
int tree_current_is_physical_dir(struct tree *);
/* "is_physical_link" is equivalent to S_ISLNK(tree_current_lstat()->st_mode) */
int tree_current_is_physical_link(struct tree *);
/* "is_dir" is equivalent to S_ISDIR(tree_current_stat()->st_mode) */
int tree_current_is_dir(struct tree *);

/* For testing/debugging: Dump the internal status to the given filehandle. */
void tree_dump(struct tree *, FILE *);
