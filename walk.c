/*	$NetBSD: walk.c,v 1.17 2004/06/20 22:20:18 jmc Exp $	*/

/*
 * Copyright (c) 2001 Wasabi Systems, Inc.
 * All rights reserved.
 *
 * Written by Luke Mewburn for Wasabi Systems, Inc.
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
 *      This product includes software developed for the NetBSD Project by
 *      Wasabi Systems, Inc.
 * 4. The name of Wasabi Systems, Inc. may not be used to endorse
 *    or promote products derived from this software without specific prior
 *    written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY WASABI SYSTEMS, INC. ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL WASABI SYSTEMS, INC
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * The function link_check() was inspired from NetBSD's usr.bin/du/du.c,
 * which has the following copyright notice:
 *
 *
 * Copyright (c) 1989, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Chris Newcomb.
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

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <dirent.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "makefs.h"

#include "mtree.h"
#include "extern.h"		/* NB: mtree */

static	void	 apply_specdir(const char *, NODE *, fsnode *);
static	void	 apply_specentry(const char *, NODE *, fsnode *);
static	fsnode	*create_fsnode(const char *, struct stat *);
static	fsinode	*link_check(fsinode *);


/*
 * walk_dir --
 *	build a tree of fsnodes from `dir', with a parent fsnode of `parent'
 *	(which may be NULL for the root of the tree).
 *	each "level" is a directory, with the "." entry guaranteed to be
 *	at the start of the list, and without ".." entries.
 */
fsnode *
walk_dir(const char *dir, fsnode *parent)
{
	fsnode		*first, *cur, *prev;
	DIR		*dirp;
	struct dirent	*dent;
	char		path[MAXPATHLEN + 1];
	struct stat	stbuf;

	assert(dir != NULL);

	if (debug & DEBUG_WALK_DIR)
		printf("walk_dir: %s %p\n", dir, parent);
	if ((dirp = opendir(dir)) == NULL)
		err(1, "Can't opendir `%s'", dir);
	first = prev = NULL;
	while ((dent = readdir(dirp)) != NULL) {
		if (strcmp(dent->d_name, "..") == 0)
			continue;
		if (debug & DEBUG_WALK_DIR_NODE)
			printf("scanning %s/%s\n", dir, dent->d_name);
		if (snprintf(path, sizeof(path), "%s/%s", dir, dent->d_name)
		    >= sizeof(path))
			errx(1, "Pathname too long.");
		if (lstat(path, &stbuf) == -1)
			err(1, "Can't lstat `%s'", path);
#ifdef S_ISSOCK
		if (S_ISSOCK(stbuf.st_mode & S_IFMT)) {
			if (debug & DEBUG_WALK_DIR_NODE)
				printf("  skipping socket %s\n", path);
			continue;
		}
#endif

		cur = create_fsnode(dent->d_name, &stbuf);
		cur->parent = parent;
		if (strcmp(dent->d_name, ".") == 0) {
				/* ensure "." is at the start of the list */
			cur->next = first;
			first = cur;
			if (! prev)
				prev = cur;
		} else {			/* not "." */
			if (prev)
				prev->next = cur;
			prev = cur;
			if (!first)
				first = cur;
			if (S_ISDIR(cur->type)) {
				cur->child = walk_dir(path, cur);
				continue;
			}
		}
		if (stbuf.st_nlink > 1) {
			fsinode	*curino;

			curino = link_check(cur->inode);
			if (curino != NULL) {
				free(cur->inode);
				cur->inode = curino;
				cur->inode->nlink++;
			}
		}
		if (S_ISLNK(cur->type)) {
			char	slink[PATH_MAX+1];
			int	llen;

			llen = readlink(path, slink, sizeof(slink) - 1);
			if (llen == -1)
				err(1, "Readlink `%s'", path);
			slink[llen] = '\0';
			if ((cur->symlink = strdup(slink)) == NULL)
				err(1, "Memory allocation error");
		}
	}
	for (cur = first; cur != NULL; cur = cur->next)
		cur->first = first;
	if (closedir(dirp) == -1)
		err(1, "Can't closedir `%s'", dir);
	return (first);
}

static fsnode *
create_fsnode(const char *name, struct stat *stbuf)
{
	fsnode *cur;

	if ((cur = calloc(1, sizeof(fsnode))) == NULL ||
	    (cur->name = strdup(name)) == NULL ||
	    (cur->inode = calloc(1, sizeof(fsinode))) == NULL)
		err(1, "Memory allocation error");
	cur->type = stbuf->st_mode & S_IFMT;
	cur->inode->nlink = 1;
	cur->inode->st = *stbuf;
	return (cur);
}

/*
 * apply_specfile --
 *	read in the mtree(8) specfile, and apply it to the tree
 *	at dir,parent. parameters in parent on equivalent types
 *	will be changed to those found in specfile, and missing
 *	entries will be added.
 */
void
apply_specfile(const char *specfile, const char *dir, fsnode *parent)
{
	struct timeval	 start;
	FILE	*fp;
	NODE	*root;

	assert(specfile != NULL);
	assert(parent != NULL);

	if (debug & DEBUG_APPLY_SPECFILE)
		printf("apply_specfile: %s, %s %p\n", specfile, dir, parent);

				/* read in the specfile */
	if ((fp = fopen(specfile, "r")) == NULL)
		err(1, "Can't open `%s'", specfile);
	TIMER_START(start);
	root = mtree_readspec(fp);
	TIMER_RESULTS(start, "spec");
	if (fclose(fp) == EOF)
		err(1, "Can't close `%s'", specfile);

				/* perform some sanity checks */
	if (root == NULL)
		errx(1, "Specfile `%s' did not contain a tree", specfile);
	assert(strcmp(root->name, ".") == 0);
	assert(root->type == F_DIR);

				/* merge in the changes */
	apply_specdir(dir, root, parent);
}

static u_int
nodetoino(u_int type)
{

	switch (type) {
	case F_BLOCK:
		return S_IFBLK;
	case F_CHAR:
		return S_IFCHR;
	case F_DIR:
		return S_IFDIR;
	case F_FIFO:
		return S_IFIFO;
	case F_FILE:
		return S_IFREG;
	case F_LINK:
		return S_IFLNK;
	case F_SOCK:
		return S_IFSOCK;
	default:
		printf("unknown type %d", type);
		abort();
	}
	/* NOTREACHED */
}

static void
apply_specdir(const char *dir, NODE *specnode, fsnode *dirnode)
{
	char	 path[MAXPATHLEN + 1];
	NODE	*curnode;
	fsnode	*curfsnode;

	assert(specnode != NULL);
	assert(dirnode != NULL);

	if (debug & DEBUG_APPLY_SPECFILE)
		printf("apply_specdir: %s %p %p\n", dir, specnode, dirnode);

	if (specnode->type != F_DIR)
		errx(1, "Specfile node `%s/%s' is not a directory",
		    dir, specnode->name);
	if (dirnode->type != S_IFDIR)
		errx(1, "Directory node `%s/%s' is not a directory",
		    dir, dirnode->name);

	apply_specentry(dir, specnode, dirnode);

			/* now walk specnode->child matching up with dirnode */
	for (curnode = specnode->child; curnode != NULL;
	    curnode = curnode->next) {
		if (debug & DEBUG_APPLY_SPECENTRY)
			printf("apply_specdir:  spec %s\n",
			    curnode->name);
		for (curfsnode = dirnode->next; curfsnode != NULL;
		    curfsnode = curfsnode->next) {
#if 0	/* too verbose for now */
			if (debug & DEBUG_APPLY_SPECENTRY)
				printf("apply_specdir:  dirent %s\n",
				    curfsnode->name);
#endif
			if (strcmp(curnode->name, curfsnode->name) == 0)
				break;
		}
		if (snprintf(path, sizeof(path), "%s/%s",
		    dir, curnode->name) >= sizeof(path))
			errx(1, "Pathname too long.");
		if (curfsnode == NULL) {	/* need new entry */
			struct stat	stbuf;

					    /*
					     * don't add optional spec entries
					     * that lack an existing fs entry
					     */
			if ((curnode->flags & F_OPT) &&
			    lstat(path, &stbuf) == -1)
					continue;

					/* check that enough info is provided */
#define NODETEST(t, m)							\
			if (!(t))					\
				errx(1, "`%s': %s not provided", path, m)
			NODETEST(curnode->flags & F_TYPE, "type");
			NODETEST(curnode->flags & F_MODE, "mode");
				/* XXX: require F_TIME ? */
			NODETEST(curnode->flags & F_GID ||
			    curnode->flags & F_GNAME, "group");
			NODETEST(curnode->flags & F_UID ||
			    curnode->flags & F_UNAME, "user");
#undef NODETEST

			if (debug & DEBUG_APPLY_SPECFILE)
				printf("apply_specdir: adding %s\n",
				    curnode->name);
					/* build minimal fsnode */
			memset(&stbuf, 0, sizeof(stbuf));
			stbuf.st_mode = nodetoino(curnode->type);
			stbuf.st_nlink = 1;
			stbuf.st_mtime = stbuf.st_atime =
			    stbuf.st_ctime = start_time.tv_sec;
#if HAVE_STRUCT_STAT_ST_MTIMENSEC
			stbuf.st_mtimensec = stbuf.st_atimensec =
			    stbuf.st_ctimensec = start_time.tv_nsec;
#endif
			curfsnode = create_fsnode(curnode->name, &stbuf);
			curfsnode->parent = dirnode->parent;
			curfsnode->first = dirnode;
			curfsnode->next = dirnode->next;
			dirnode->next = curfsnode;
			if (curfsnode->type == S_IFDIR) {
					/* for dirs, make "." entry as well */
				curfsnode->child = create_fsnode(".", &stbuf);
				curfsnode->child->parent = curfsnode;
				curfsnode->child->first = curfsnode->child;
			}
			if (curfsnode->type == S_IFLNK) {
				assert(curnode->slink != NULL);
					/* for symlinks, copy the target */
				if ((curfsnode->symlink =
				    strdup(curnode->slink)) == NULL)
					err(1, "Memory allocation error");
			}
		}
		apply_specentry(dir, curnode, curfsnode);
		if (curnode->type == F_DIR) {
			if (curfsnode->type != S_IFDIR)
				errx(1, "`%s' is not a directory", path);
			assert (curfsnode->child != NULL);
			apply_specdir(path, curnode, curfsnode->child);
		}
	}
}

static void
apply_specentry(const char *dir, NODE *specnode, fsnode *dirnode)
{

	assert(specnode != NULL);
	assert(dirnode != NULL);

	if (nodetoino(specnode->type) != dirnode->type)
		errx(1, "`%s/%s' type mismatch: specfile %s, tree %s",
		    dir, specnode->name, inode_type(nodetoino(specnode->type)),
		    inode_type(dirnode->type));

	if (debug & DEBUG_APPLY_SPECENTRY)
		printf("apply_specentry: %s/%s\n", dir, dirnode->name);

#define ASEPRINT(t, b, o, n) \
		if (debug & DEBUG_APPLY_SPECENTRY) \
			printf("\t\t\tchanging %s from " b " to " b "\n", \
			    t, o, n)

	if (specnode->flags & (F_GID | F_GNAME)) {
		ASEPRINT("gid", "%d",
		    dirnode->inode->st.st_gid, specnode->st_gid);
		dirnode->inode->st.st_gid = specnode->st_gid;
	}
	if (specnode->flags & F_MODE) {
		ASEPRINT("mode", "%#o",
		    dirnode->inode->st.st_mode & ALLPERMS, specnode->st_mode);
		dirnode->inode->st.st_mode &= ~ALLPERMS;
		dirnode->inode->st.st_mode |= (specnode->st_mode & ALLPERMS);
	}
		/* XXX: ignoring F_NLINK for now */
	if (specnode->flags & F_SIZE) {
		ASEPRINT("size", "%lld",
		    (long long)dirnode->inode->st.st_size,
		    (long long)specnode->st_size);
		dirnode->inode->st.st_size = specnode->st_size;
	}
	if (specnode->flags & F_SLINK) {
		assert(dirnode->symlink != NULL);
		assert(specnode->slink != NULL);
		ASEPRINT("symlink", "%s", dirnode->symlink, specnode->slink);
		free(dirnode->symlink);
		if ((dirnode->symlink = strdup(specnode->slink)) == NULL)
			err(1, "Memory allocation error");
	}
	if (specnode->flags & F_TIME) {
		ASEPRINT("time", "%ld",
		    (long)dirnode->inode->st.st_mtime,
		    (long)specnode->st_mtimespec.tv_sec);
		dirnode->inode->st.st_mtime =		specnode->st_mtimespec.tv_sec;
		dirnode->inode->st.st_atime =		specnode->st_mtimespec.tv_sec;
		dirnode->inode->st.st_ctime =		start_time.tv_sec;
#if HAVE_STRUCT_STAT_ST_MTIMENSEC
		dirnode->inode->st.st_mtimensec =	specnode->st_mtimespec.tv_nsec;
		dirnode->inode->st.st_atimensec =	specnode->st_mtimespec.tv_nsec;
		dirnode->inode->st.st_ctimensec =	start_time.tv_nsec;
#endif
	}
	if (specnode->flags & (F_UID | F_UNAME)) {
		ASEPRINT("uid", "%d",
		    dirnode->inode->st.st_uid, specnode->st_uid);
		dirnode->inode->st.st_uid = specnode->st_uid;
	}
#if HAVE_STRUCT_STAT_ST_FLAGS
	if (specnode->flags & F_FLAGS) {
		ASEPRINT("flags", "%#lX",
		    (unsigned long)dirnode->inode->st.st_flags,
		    (unsigned long)specnode->st_flags);
		dirnode->inode->st.st_flags = specnode->st_flags;
	}
#endif
#undef ASEPRINT

	dirnode->flags |= FSNODE_F_HASSPEC;
}


/*
 * dump_fsnodes --
 *	dump the fsnodes from `cur', based in the directory `dir'
 */
void
dump_fsnodes(const char *dir, fsnode *root)
{
	fsnode	*cur;
	char	path[MAXPATHLEN + 1];

	assert (dir != NULL);
	printf("dump_fsnodes: %s %p\n", dir, root);
	for (cur = root; cur != NULL; cur = cur->next) {
		if (snprintf(path, sizeof(path), "%s/%s", dir, cur->name)
		    >= sizeof(path))
			errx(1, "Pathname too long.");

		if (debug & DEBUG_DUMP_FSNODES_VERBOSE)
			printf("cur=%8p parent=%8p first=%8p ",
			    cur, cur->parent, cur->first);
		printf("%7s: %s", inode_type(cur->type), path);
		if (S_ISLNK(cur->type)) {
			assert(cur->symlink != NULL);
			printf(" -> %s", cur->symlink);
		} else {
			assert (cur->symlink == NULL);
		}
		if (cur->inode->nlink > 1)
			printf(", nlinks=%d", cur->inode->nlink);
		putchar('\n');

		if (cur->child) {
			assert (cur->type == S_IFDIR);
			dump_fsnodes(path, cur->child);
		}
	}
	printf("dump_fsnodes: finished %s\n", dir);
}


/*
 * inode_type --
 *	for a given inode type `mode', return a descriptive string.
 */
const char *
inode_type(mode_t mode)
{

	if (S_ISREG(mode))
		return ("file");
	if (S_ISLNK(mode))
		return ("symlink");
	if (S_ISDIR(mode))
		return ("dir");
	if (S_ISLNK(mode))
		return ("link");
	if (S_ISFIFO(mode))
		return ("fifo");
	if (S_ISSOCK(mode))
		return ("socket");
	/* XXX should not happen but handle them */
	if (S_ISCHR(mode))
		return ("char");
	if (S_ISBLK(mode))
		return ("block");
	return ("unknown");
}


/*
 * link_check --
 *	return pointer to fsnode matching `entry's st_ino & st_dev if it exists,
 *	otherwise add `entry' to table and return NULL
 */
static fsinode *
link_check(fsinode *entry)
{
	static	struct dupnode {
		uint32_t	dev;
		uint64_t	ino;
		fsinode		*dup;
	} *dups, *newdups;
	static	int	ndups, maxdups;

	int	i;

	assert (entry != NULL);

		/* XXX; maybe traverse in reverse for speed? */
	for (i = 0; i < ndups; i++) {
		if (dups[i].dev == entry->st.st_dev &&
		    dups[i].ino == entry->st.st_ino) {
			if (debug & DEBUG_WALK_DIR_LINKCHECK)
				printf("link_check: found [%d,%d]\n",
				    entry->st.st_dev, entry->st.st_ino);
			return (dups[i].dup);
		}
	}

	if (debug & DEBUG_WALK_DIR_LINKCHECK)
		printf("link_check: no match for [%d, %d]\n",
		    entry->st.st_dev, entry->st.st_ino);
	if (ndups == maxdups) {
		if ((newdups = realloc(dups, sizeof(struct dupnode) * (maxdups + 128)))
		    == NULL)
			err(1, "Memory allocation error");
		dups = newdups;
		maxdups += 128;
	}
	dups[ndups].dev = entry->st.st_dev;
	dups[ndups].ino = entry->st.st_ino;
	dups[ndups].dup = entry;
	ndups++;

	return (NULL);
}
