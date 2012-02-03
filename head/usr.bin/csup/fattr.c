/*-
 * Copyright (c) 2003-2006, Maxime Henrion <mux@FreeBSD.org>
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "fattr.h"
#include "idcache.h"
#include "misc.h"

/*
 * Include the appropriate definition for the file attributes we support.
 * There are two different files: fattr_bsd.h for BSD-like systems that
 * support the extended file flags a la chflags() and fattr_posix.h for
 * bare POSIX systems that don't.
 */
#ifdef HAVE_FFLAGS
#include "fattr_bsd.h"
#else
#include "fattr_posix.h"
#endif

#ifdef __FreeBSD__
#include <osreldate.h>
#endif

/* Define fflags_t if we're on a system that doesn't have it. */
#if !defined(__FreeBSD_version) || __FreeBSD_version < 500030
typedef uint32_t fflags_t;
#endif

#define	FA_MASKRADIX		16
#define	FA_FILETYPERADIX	10
#define	FA_MODTIMERADIX		10
#define	FA_SIZERADIX		10
#define	FA_RDEVRADIX		16
#define	FA_MODERADIX		8
#define	FA_FLAGSRADIX		16
#define	FA_LINKCOUNTRADIX	10
#define	FA_DEVRADIX		16
#define	FA_INODERADIX		10

#define	FA_PERMMASK		(S_IRWXU | S_IRWXG | S_IRWXO)
#define	FA_SETIDMASK		(S_ISUID | S_ISGID | S_ISVTX)

struct fattr {
	int		mask;
	int		type;
	time_t		modtime;
	off_t		size;
	char		*linktarget;
	dev_t		rdev;
	uid_t		uid;
	gid_t		gid;
	mode_t		mode;
	fflags_t	flags;
	nlink_t		linkcount;
	dev_t		dev;
	ino_t		inode;
};

static const struct fattr bogus = {
	FA_MODTIME | FA_SIZE | FA_MODE,
	FT_UNKNOWN,
	1,
	0,
	NULL,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0
};

static struct fattr *defaults[FT_NUMBER];

void
fattr_init(void)
{
	struct fattr *fa;
	int i;

	for (i = 0; i < FT_NUMBER; i++) {
		fa = fattr_new(i, -1);
		if (i == FT_DIRECTORY)
			fa->mode = 0777;
		else
			fa->mode = 0666;
		fa->mask |= FA_MODE;
		defaults[i] = fa;
	}
	/* Initialize the uid/gid lookup cache. */
	idcache_init();
}

void
fattr_fini(void)
{
	int i;

	idcache_fini();
	for (i = 0; i < FT_NUMBER; i++)
		fattr_free(defaults[i]);
}

const struct fattr *fattr_bogus = &bogus;

static char		*fattr_scanattr(struct fattr *, int, const char *);

int
fattr_supported(int type)
{

	return (fattr_support[type]);
}

struct fattr *
fattr_new(int type, time_t modtime)
{
	struct fattr *new;

	new = xmalloc(sizeof(struct fattr));
	memset(new, 0, sizeof(struct fattr));
	new->type = type;
	if (type != FT_UNKNOWN)
		new->mask |= FA_FILETYPE;
	if (modtime != -1) {
		new->modtime = modtime;
		new->mask |= FA_MODTIME;
	}
	if (fattr_supported(new->type) & FA_LINKCOUNT) {
		new->mask |= FA_LINKCOUNT;
		new->linkcount = 1;
	}
	return (new);
}

/* Returns a new file attribute structure based on a stat structure. */
struct fattr *
fattr_fromstat(struct stat *sb)
{
	struct fattr *fa;

	fa = fattr_new(FT_UNKNOWN, -1);
	if (S_ISREG(sb->st_mode))
		fa->type = FT_FILE;
	else if (S_ISDIR(sb->st_mode))
		fa->type = FT_DIRECTORY;
	else if (S_ISCHR(sb->st_mode))
		fa->type = FT_CDEV;
	else if (S_ISBLK(sb->st_mode))
		fa->type = FT_BDEV;
	else if (S_ISLNK(sb->st_mode))
		fa->type = FT_SYMLINK;
	else
		fa->type = FT_UNKNOWN;

	fa->mask = FA_FILETYPE | fattr_supported(fa->type);
	if (fa->mask & FA_MODTIME)
		fa->modtime = sb->st_mtime;
	if (fa->mask & FA_SIZE)
		fa->size = sb->st_size;
	if (fa->mask & FA_RDEV)
		fa->rdev = sb->st_rdev;
	if (fa->mask & FA_OWNER)
		fa->uid = sb->st_uid;
	if (fa->mask & FA_GROUP)
		fa->gid = sb->st_gid;
	if (fa->mask & FA_MODE)
		fa->mode = sb->st_mode & (FA_SETIDMASK | FA_PERMMASK);
#ifdef HAVE_FFLAGS
	if (fa->mask & FA_FLAGS)
		fa->flags = sb->st_flags;
#endif
	if (fa->mask & FA_LINKCOUNT)
		fa->linkcount = sb->st_nlink;
	if (fa->mask & FA_DEV)
		fa->dev = sb->st_dev;
	if (fa->mask & FA_INODE)
		fa->inode = sb->st_ino;
	return (fa);
}

struct fattr *
fattr_frompath(const char *path, int nofollow)
{
	struct fattr *fa;
	struct stat sb;
	int error, len;

	if (nofollow)
		error = lstat(path, &sb);
	else
		error = stat(path, &sb);
	if (error)
		return (NULL);
	fa = fattr_fromstat(&sb);
	if (fa->mask & FA_LINKTARGET) {
		char buf[1024];

		len = readlink(path, buf, sizeof(buf));
		if (len == -1) {
			fattr_free(fa);
			return (NULL);
		}
		if ((unsigned)len > sizeof(buf) - 1) {
			fattr_free(fa);
			errno = ENAMETOOLONG;
			return (NULL);
		}
		buf[len] = '\0';
		fa->linktarget = xstrdup(buf);
	}
	return (fa);
}

struct fattr *
fattr_fromfd(int fd)
{
	struct fattr *fa;
	struct stat sb;
	int error;

	error = fstat(fd, &sb);
	if (error)
		return (NULL);
	fa = fattr_fromstat(&sb);
	return (fa);
}

int
fattr_type(const struct fattr *fa)
{

	return (fa->type);
}

/* Returns a new file attribute structure from its encoded text form. */
struct fattr *
fattr_decode(char *attr)
{
	struct fattr *fa;
	char *next;

	fa = fattr_new(FT_UNKNOWN, -1);
	next = fattr_scanattr(fa, FA_MASK, attr);
	if (next == NULL || (fa->mask & ~FA_MASK) > 0)
		goto bad;
	if (fa->mask & FA_FILETYPE) {
		next = fattr_scanattr(fa, FA_FILETYPE, next);
		if (next == NULL)
			goto bad;
		if (fa->type < 0 || fa->type > FT_MAX)
			fa->type = FT_UNKNOWN;
	} else {
		/* The filetype attribute is always valid. */
		fa->mask |= FA_FILETYPE;
		fa->type = FT_UNKNOWN;
	}
	fa->mask = fa->mask & fattr_supported(fa->type);
	if (fa->mask & FA_MODTIME)
		next = fattr_scanattr(fa, FA_MODTIME, next);
	if (fa->mask & FA_SIZE)
		next = fattr_scanattr(fa, FA_SIZE, next);
	if (fa->mask & FA_LINKTARGET)
		next = fattr_scanattr(fa, FA_LINKTARGET, next);
	if (fa->mask & FA_RDEV)
		next = fattr_scanattr(fa, FA_RDEV, next);
	if (fa->mask & FA_OWNER)
		next = fattr_scanattr(fa, FA_OWNER, next);
	if (fa->mask & FA_GROUP)
		next = fattr_scanattr(fa, FA_GROUP, next);
	if (fa->mask & FA_MODE)
		next = fattr_scanattr(fa, FA_MODE, next);
	if (fa->mask & FA_FLAGS)
		next = fattr_scanattr(fa, FA_FLAGS, next);
	if (fa->mask & FA_LINKCOUNT) {
		next = fattr_scanattr(fa, FA_LINKCOUNT, next);
	} else if (fattr_supported(fa->type) & FA_LINKCOUNT) {
		/* If the link count is missing but supported, fake it as 1. */
		fa->mask |= FA_LINKCOUNT;
		fa->linkcount = 1;
	}
	if (fa->mask & FA_DEV)
		next = fattr_scanattr(fa, FA_DEV, next);
	if (fa->mask & FA_INODE)
		next = fattr_scanattr(fa, FA_INODE, next);
	if (next == NULL)
		goto bad;
	return (fa);
bad:
	fattr_free(fa);
	return (NULL);
}

char *
fattr_encode(const struct fattr *fa, fattr_support_t support, int ignore)
{
	struct {
		char val[32];
		char len[4];
		int extval;
		char *ext;
	} pieces[FA_NUMBER], *piece;
	char *cp, *s, *username, *groupname;
	size_t len, vallen;
	mode_t mode, modemask;
	int mask, n, i;

	username = NULL;
	groupname = NULL;
	if (support == NULL)
		mask = fa->mask;
	else
		mask = fa->mask & support[fa->type];
	mask &= ~ignore;
	if (fa->mask & FA_OWNER) {
		username = getuserbyid(fa->uid);
		if (username == NULL)
			mask &= ~FA_OWNER;
	}
	if (fa->mask & FA_GROUP) {
		groupname = getgroupbyid(fa->gid);
		if (groupname == NULL)
			mask &= ~FA_GROUP;
	}
	if (fa->mask & FA_LINKCOUNT && fa->linkcount == 1)
		mask &= ~FA_LINKCOUNT;

	memset(pieces, 0, FA_NUMBER * sizeof(*pieces));
	len = 0;
	piece = pieces;
	vallen = snprintf(piece->val, sizeof(piece->val), "%x", mask);
	len += snprintf(piece->len, sizeof(piece->len), "%lld",
	    (long long)vallen) + vallen + 1;
	piece++;
	if (mask & FA_FILETYPE) {
		vallen = snprintf(piece->val, sizeof(piece->val),
		    "%d", fa->type);
		len += snprintf(piece->len, sizeof(piece->len), "%lld",
		    (long long)vallen) + vallen + 1;
		piece++;
	}
	if (mask & FA_MODTIME) {
		vallen = snprintf(piece->val, sizeof(piece->val),
		    "%lld", (long long)fa->modtime);
		len += snprintf(piece->len, sizeof(piece->len), "%lld",
		    (long long)vallen) + vallen + 1;
		piece++;
	}
	if (mask & FA_SIZE) {
		vallen = snprintf(piece->val, sizeof(piece->val),
		    "%lld", (long long)fa->size);
		len += snprintf(piece->len, sizeof(piece->len), "%lld",
		    (long long)vallen) + vallen + 1;
		piece++;
	}
	if (mask & FA_LINKTARGET) {
		vallen = strlen(fa->linktarget);
		piece->extval = 1;
		piece->ext = fa->linktarget;
		len += snprintf(piece->len, sizeof(piece->len), "%lld",
		    (long long)vallen) + vallen + 1;
		piece++;
	}
	if (mask & FA_RDEV) {
		vallen = snprintf(piece->val, sizeof(piece->val),
		    "%lld", (long long)fa->rdev);
		len += snprintf(piece->len, sizeof(piece->len), "%lld",
		    (long long)vallen) + vallen + 1;
		piece++;
	}
	if (mask & FA_OWNER) {
		vallen = strlen(username);
		piece->extval = 1;
		piece->ext = username;
		len += snprintf(piece->len, sizeof(piece->len), "%lld",
		    (long long)vallen) + vallen + 1;
		piece++;
	}
	if (mask & FA_GROUP) {
		vallen = strlen(groupname);
		piece->extval = 1;
		piece->ext = groupname;
		len += snprintf(piece->len, sizeof(piece->len), "%lld",
		    (long long)vallen) + vallen + 1;
		piece++;
	}
	if (mask & FA_MODE) {
		if (mask & FA_OWNER && mask & FA_GROUP)
			modemask = FA_SETIDMASK | FA_PERMMASK;
		else
			modemask = FA_PERMMASK;
		mode = fa->mode & modemask;
		vallen = snprintf(piece->val, sizeof(piece->val),
		    "%o", mode);
		len += snprintf(piece->len, sizeof(piece->len), "%lld",
		    (long long)vallen) + vallen + 1;
		piece++;
	}
	if (mask & FA_FLAGS) {
		vallen = snprintf(piece->val, sizeof(piece->val), "%llx",
		    (long long)fa->flags);
		len += snprintf(piece->len, sizeof(piece->len), "%lld",
		    (long long)vallen) + vallen + 1;
		piece++;
	}
	if (mask & FA_LINKCOUNT) {
		vallen = snprintf(piece->val, sizeof(piece->val), "%lld",
		    (long long)fa->linkcount);
		len += snprintf(piece->len, sizeof(piece->len), "%lld",
		    (long long)vallen) + vallen + 1;
		piece++;
	}
	if (mask & FA_DEV) {
		vallen = snprintf(piece->val, sizeof(piece->val), "%llx",
		    (long long)fa->dev);
		len += snprintf(piece->len, sizeof(piece->len), "%lld",
		    (long long)vallen) + vallen + 1;
		piece++;
	}
	if (mask & FA_INODE) {
		vallen = snprintf(piece->val, sizeof(piece->val), "%lld",
		    (long long)fa->inode);
		len += snprintf(piece->len, sizeof(piece->len), "%lld",
		    (long long)vallen) + vallen + 1;
		piece++;
	}

	s = xmalloc(len + 1);

	n = piece - pieces;
	piece = pieces;
	cp = s;
	for (i = 0; i < n; i++) {
		if (piece->extval)
			len = sprintf(cp, "%s#%s", piece->len, piece->ext);
		else
			len = sprintf(cp, "%s#%s", piece->len, piece->val);
		cp += len;
	      	piece++;
	}
	return (s);
}

struct fattr *
fattr_dup(const struct fattr *from)
{
	struct fattr *fa;

	fa = fattr_new(FT_UNKNOWN, -1);
	fattr_override(fa, from, FA_MASK);
	return (fa);
}

void
fattr_free(struct fattr *fa)
{

	if (fa == NULL)
		return;
	if (fa->linktarget != NULL)
		free(fa->linktarget);
	free(fa);
}

void
fattr_umask(struct fattr *fa, mode_t newumask)
{

	if (fa->mask & FA_MODE)
		fa->mode = fa->mode & ~newumask;
}

void
fattr_maskout(struct fattr *fa, int mask)
{

	/* Don't forget to free() the linktarget attribute if we remove it. */
	if (mask & FA_LINKTARGET && fa->mask & FA_LINKTARGET) {
		free(fa->linktarget);
		fa->linktarget = NULL;
	}
	fa->mask &= ~mask;
}

int
fattr_getmask(const struct fattr *fa)
{

	return (fa->mask);
}

nlink_t
fattr_getlinkcount(const struct fattr *fa)
{

	return (fa->linkcount);
}

char *
fattr_getlinktarget(const struct fattr *fa)
{

	return (fa->linktarget);
}

/*
 * Eat the specified attribute and put it in the file attribute
 * structure.  Returns NULL on error, or a pointer to the next
 * attribute to parse.
 *
 * This would be much prettier if we had strntol() so that we're
 * not forced to write '\0' to the string before calling strtol()
 * and then put back the old value...
 *
 * We need to use (unsigned) long long types here because some
 * of the opaque types we're parsing (off_t, time_t...) may need
 * 64bits to fit.
 */
static char *
fattr_scanattr(struct fattr *fa, int type, const char *attr)
{
	char *attrend, *attrstart, *end;
	size_t len;
	unsigned long attrlen;
	int error;
	mode_t modemask;
	char tmp;

	if (attr == NULL)
		return (NULL);
	errno = 0;
	attrlen = strtoul(attr, &end, 10);
	if (errno || *end != '#')
		return (NULL);
	len = strlen(attr);
	attrstart = end + 1;
	attrend = attrstart + attrlen;
	tmp = *attrend;
	*attrend = '\0';
	switch (type) {
	/* Using FA_MASK here is a bit bogus semantically. */
	case FA_MASK:
		errno = 0;
		fa->mask = (int)strtol(attrstart, &end, FA_MASKRADIX);
		if (errno || end != attrend)
			goto bad;
		break;
	case FA_FILETYPE:
		errno = 0;
		fa->type = (int)strtol(attrstart, &end, FA_FILETYPERADIX);
		if (errno || end != attrend)
			goto bad;
		break;
	case FA_MODTIME:
		errno = 0;
		fa->modtime = (time_t)strtoll(attrstart, &end, FA_MODTIMERADIX);
		if (errno || end != attrend)
			goto bad;
		break;
	case FA_SIZE:
		errno = 0;
		fa->size = (off_t)strtoll(attrstart, &end, FA_SIZERADIX);
		if (errno || end != attrend)
			goto bad;
		break;
	case FA_LINKTARGET:
		fa->linktarget = xstrdup(attrstart);
		break;
	case FA_RDEV:
		errno = 0;
		fa->rdev = (dev_t)strtoll(attrstart, &end, FA_RDEVRADIX);
		if (errno || end != attrend)
			goto bad;
		break;
	case FA_OWNER:
		error = getuidbyname(attrstart, &fa->uid);
		if (error)
			fa->mask &= ~FA_OWNER;
		break;
	case FA_GROUP:
		error = getgidbyname(attrstart, &fa->gid);
		if (error)
			fa->mask &= ~FA_GROUP;
		break;
	case FA_MODE:
		errno = 0;
		fa->mode = (mode_t)strtol(attrstart, &end, FA_MODERADIX);
		if (errno || end != attrend)
			goto bad;
		if (fa->mask & FA_OWNER && fa->mask & FA_GROUP)
			modemask = FA_SETIDMASK | FA_PERMMASK;
		else
			modemask = FA_PERMMASK;
		fa->mode &= modemask;
		break;
	case FA_FLAGS:
		errno = 0;
		fa->flags = (fflags_t)strtoul(attrstart, &end, FA_FLAGSRADIX);
		if (errno || end != attrend)
			goto bad;
		break;
	case FA_LINKCOUNT:
		errno = 0;
		fa->linkcount = (nlink_t)strtol(attrstart, &end, FA_FLAGSRADIX);
		if (errno || end != attrend)
			goto bad;
		break;
	case FA_DEV:
		errno = 0;
		fa->dev = (dev_t)strtoll(attrstart, &end, FA_DEVRADIX);
		if (errno || end != attrend)
			goto bad;
		break;
	case FA_INODE:
		errno = 0;
		fa->inode = (ino_t)strtoll(attrstart, &end, FA_INODERADIX);
		if (errno || end != attrend)
			goto bad;
		break;
	}
	*attrend = tmp;
	return (attrend);
bad:
	*attrend = tmp;
	return (NULL);
}

/* Return a file attribute structure built from the RCS file attributes. */
struct fattr *
fattr_forcheckout(const struct fattr *rcsattr, mode_t mask)
{
	struct fattr *fa;

	fa = fattr_new(FT_FILE, -1);
	if (rcsattr->mask & FA_MODE) {
		if ((rcsattr->mode & 0111) > 0)
			fa->mode = 0777;
		else
			fa->mode = 0666;
		fa->mode &= ~mask;
		fa->mask |= FA_MODE;
	}
	return (fa);
}

/* Merge attributes from "from" that aren't present in "fa". */
void
fattr_merge(struct fattr *fa, const struct fattr *from)
{

	fattr_override(fa, from, from->mask & ~fa->mask);
}

/* Merge default attributes. */
void
fattr_mergedefault(struct fattr *fa)
{

	fattr_merge(fa, defaults[fa->type]);
}

/* Override selected attributes of "fa" with values from "from". */
void
fattr_override(struct fattr *fa, const struct fattr *from, int mask)
{

	mask &= from->mask;
	if (fa->mask & FA_LINKTARGET && mask & FA_LINKTARGET)
		free(fa->linktarget);
	fa->mask |= mask;
	if (mask & FA_FILETYPE)
		fa->type = from->type;
	if (mask & FA_MODTIME)
		fa->modtime = from->modtime;
	if (mask & FA_SIZE)
		fa->size = from->size;
	if (mask & FA_LINKTARGET)
		fa->linktarget = xstrdup(from->linktarget);
	if (mask & FA_RDEV)
		fa->rdev = from->rdev;
	if (mask & FA_OWNER)
		fa->uid = from->uid;
	if (mask & FA_GROUP)
		fa->gid = from->gid;
	if (mask & FA_MODE)
		fa->mode = from->mode;
	if (mask & FA_FLAGS)
		fa->flags = from->flags;
	if (mask & FA_LINKCOUNT)
		fa->linkcount = from->linkcount;
	if (mask & FA_DEV)
		fa->dev = from->dev;
	if (mask & FA_INODE)
		fa->inode = from->inode;
}

/* Create a node. */
int
fattr_makenode(const struct fattr *fa, const char *path)
{
	mode_t modemask, mode;
	int error;

	error = 0;

	if (fa->mask & FA_OWNER && fa->mask & FA_GROUP)
		modemask = FA_SETIDMASK | FA_PERMMASK;
	else
		modemask = FA_PERMMASK;

	/* We only implement fattr_makenode() for dirs for now. */
	if (fa->mask & FA_MODE)
		mode = fa->mode & modemask;
	else
		mode = 0700;

	if (fa->type == FT_DIRECTORY)
		error = mkdir(path, mode);
	else if (fa->type == FT_SYMLINK) {
		error = symlink(fa->linktarget, path);
	} else if (fa->type == FT_CDEV) {
		lprintf(-1, "Character devices not supported!\n");
	} else if (fa->type == FT_BDEV) {
		lprintf(-1, "Block devices not supported!\n");
	}
	return (error);
}

int
fattr_delete(const char *path)
{
	struct fattr *fa;
	int error;

	fa = fattr_frompath(path, FATTR_NOFOLLOW);
	if (fa == NULL) {
		if (errno == ENOENT)
			return (0);
		return (-1);
	}

#ifdef HAVE_FFLAGS
	/* Clear flags. */
	if (fa->mask & FA_FLAGS && fa->flags != 0) {
		fa->flags = 0;
		(void)chflags(path, fa->flags);
	}
#endif

	if (fa->type == FT_DIRECTORY)
		error = rmdir(path);
	else
		error = unlink(path);
	fattr_free(fa);
	return (error);
}

/*
 * Changes those attributes we can change.  Returns -1 on error,
 * 0 if no update was needed, and 1 if an update was needed and
 * it has been applied successfully.
 */
int
fattr_install(struct fattr *fa, const char *topath, const char *frompath)
{
	struct timeval tv[2];
	struct fattr *old;
	int error, inplace, mask;
	mode_t modemask, newmode;
	uid_t uid;
	gid_t gid;

	mask = fa->mask & fattr_supported(fa->type);
	if (mask & FA_OWNER && mask & FA_GROUP)
		modemask = FA_SETIDMASK | FA_PERMMASK;
	else
		modemask = FA_PERMMASK;

	inplace = 0;
	if (frompath == NULL) {
		/* Changing attributes in place. */
		frompath = topath;
		inplace = 1;
	}
	old = fattr_frompath(topath, FATTR_NOFOLLOW);
	if (old != NULL) {
		if (inplace && fattr_equal(fa, old)) {
			fattr_free(old);
			return (0);
		}

#ifdef HAVE_FFLAGS
		/*
		 * Determine whether we need to clear the flags of the target.
		 * This is bogus in that it assumes a value of 0 is safe and
		 * that non-zero is unsafe.  I'm not really worried by that
		 * since as far as I know that's the way things are.
		 */
		if ((old->mask & FA_FLAGS) && old->flags > 0) {
			(void)chflags(topath, 0);
			old->flags = 0;
		}
#endif

		/*
		 * If it is changed from a file to a symlink, remove the file
		 * and create the symlink.
		 */
		if (inplace && (fa->type == FT_SYMLINK) &&
		    (old->type == FT_FILE)) {
			error = unlink(topath);
			if (error)
				goto bad;
			error = symlink(fa->linktarget, topath);
			if (error)
				goto bad;
		}
		/* Determine whether we need to remove the target first. */
		if (!inplace && (fa->type == FT_DIRECTORY) !=
		    (old->type == FT_DIRECTORY)) {
			if (old->type == FT_DIRECTORY)
				error = rmdir(topath);
			else
				error = unlink(topath);
			if (error)
				goto bad;
		}
	}

	/* Change those attributes that we can before moving the file
	 * into place.  That makes installation atomic in most cases. */
	if (mask & FA_MODTIME) {
		gettimeofday(tv, NULL);		/* Access time. */
		tv[1].tv_sec = fa->modtime;	/* Modification time. */
		tv[1].tv_usec = 0;
		error = utimes(frompath, tv);
		if (error)
			goto bad;
	}
	if (mask & FA_OWNER || mask & FA_GROUP) {
		uid = -1;
		gid = -1;
		if (mask & FA_OWNER)
			uid = fa->uid;
		if (mask & FA_GROUP)
			gid = fa->gid;
		error = chown(frompath, uid, gid);
		if (error) {
			goto bad;
		}
	}
	if (mask & FA_MODE) {
		newmode = fa->mode & modemask;
		/* Merge in set*id bits from the old attribute. */
		if (old != NULL && old->mask & FA_MODE) {
			newmode |= (old->mode & ~modemask);
			newmode &= (FA_SETIDMASK | FA_PERMMASK);
		}
		error = chmod(frompath, newmode);
		if (error)
			goto bad;
	}

	if (!inplace) {
		error = rename(frompath, topath);
		if (error)
			goto bad;
	}

#ifdef HAVE_FFLAGS
	/* Set the flags. */
	if (mask & FA_FLAGS)
		(void)chflags(topath, fa->flags);
#endif
	fattr_free(old);
	return (1);
bad:
	fattr_free(old);
	return (-1);
}

/*
 * Returns 1 if both attributes are equal, 0 otherwise.
 *
 * This function only compares attributes that are valid in both
 * files.  A file of unknown type ("FT_UNKNOWN") is unequal to
 * anything, including itself.
 */
int
fattr_equal(const struct fattr *fa1, const struct fattr *fa2)
{
	int mask;

	mask = fa1->mask & fa2->mask;
	if (fa1->type == FT_UNKNOWN || fa2->type == FT_UNKNOWN)
		return (0);
	if (mask & FA_FILETYPE)
		if (fa1->type != fa2->type)
			return (0);
	if (mask & FA_MODTIME)
		if (fa1->modtime != fa2->modtime)
			return (0);
	if (mask & FA_SIZE)
		if (fa1->size != fa2->size)
			return (0);
	if (mask & FA_LINKTARGET)
		if (strcmp(fa1->linktarget, fa2->linktarget) != 0)
			return (0);
	if (mask & FA_RDEV)
		if (fa1->rdev != fa2->rdev)
			return (0);
	if (mask & FA_OWNER)
		if (fa1->uid != fa2->uid)
			return (0);
	if (mask & FA_GROUP)
		if (fa1->gid != fa2->gid)
			return (0);
	if (mask & FA_MODE)
		if (fa1->mode != fa2->mode)
			return (0);
	if (mask & FA_FLAGS)
		if (fa1->flags != fa2->flags)
			return (0);
	if (mask & FA_LINKCOUNT)
		if (fa1->linkcount != fa2->linkcount)
			return (0);
	if (mask & FA_DEV)
		if (fa1->dev != fa2->dev)
			return (0);
	if (mask & FA_INODE)
		if (fa1->inode != fa2->inode)
			return (0);
	return (1);
}

/*
 * Must have to get the correct filesize sendt by the server.
 */
off_t
fattr_filesize(const struct fattr *fa)
{
	return (fa->size);
}
