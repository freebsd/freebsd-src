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
#ifndef _FATTR_H_
#define _FATTR_H_

#include <sys/types.h>

#include <fcntl.h>
#include <time.h>

/*
 * File types.
 */
#define	FT_UNKNOWN	0			/* Unknown file type. */
#define	FT_FILE		1			/* Regular file. */
#define	FT_DIRECTORY	2			/* Directory. */
#define	FT_CDEV		3			/* Character device. */
#define	FT_BDEV		4			/* Block device. */
#define	FT_SYMLINK	5			/* Symbolic link. */
#define	FT_MAX		FT_SYMLINK		/* Maximum file type number. */
#define	FT_NUMBER	(FT_MAX + 1)		/* Number of file types. */

/*
 * File attributes.
 */
#define	FA_FILETYPE	0x0001		/* True for all supported file types. */
#define	FA_MODTIME	0x0002		/* Last file modification time. */
#define	FA_SIZE		0x0004		/* Size of the file. */
#define	FA_LINKTARGET	0x0008		/* Target of a symbolic link. */
#define	FA_RDEV		0x0010		/* Device for a device node. */
#define	FA_OWNER	0x0020		/* Owner of the file. */
#define	FA_GROUP	0x0040		/* Group of the file. */
#define	FA_MODE		0x0080		/* File permissions. */
#define	FA_FLAGS	0x0100		/* 4.4BSD flags, a la chflags(2). */
#define	FA_LINKCOUNT	0x0200		/* Hard link count. */
#define	FA_DEV		0x0400		/* Device holding the inode. */
#define	FA_INODE	0x0800		/* Inode number. */

#define	FA_MASK		0x0fff

#define	FA_NUMBER	12		/* Number of file attributes. */

/* Attributes that we might be able to change. */
#define	FA_CHANGEABLE	(FA_MODTIME | FA_OWNER | FA_GROUP | FA_MODE | FA_FLAGS)

/*
 * Attributes that we don't want to save in the "checkouts" file
 * when in checkout mode.
 */
#define	FA_COIGNORE	(FA_MASK & ~(FA_FILETYPE|FA_MODTIME|FA_SIZE|FA_MODE))

/* These are for fattr_frompath(). */
#define	FATTR_FOLLOW	0
#define	FATTR_NOFOLLOW	1

struct stat;
struct fattr;

typedef int	fattr_support_t[FT_NUMBER];

extern const struct fattr *fattr_bogus;

void		 fattr_init(void);
void		 fattr_fini(void);

struct fattr	*fattr_new(int, time_t);
struct fattr	*fattr_default(int);
struct fattr	*fattr_fromstat(struct stat *);
struct fattr	*fattr_frompath(const char *, int);
struct fattr	*fattr_fromfd(int);
struct fattr	*fattr_decode(char *);
struct fattr	*fattr_forcheckout(const struct fattr *, mode_t);
struct fattr	*fattr_dup(const struct fattr *);
char		*fattr_encode(const struct fattr *, fattr_support_t, int);
int		 fattr_type(const struct fattr *);
void		 fattr_maskout(struct fattr *, int);
int		 fattr_getmask(const struct fattr *);
nlink_t		 fattr_getlinkcount(const struct fattr *);
void		 fattr_umask(struct fattr *, mode_t);
void		 fattr_merge(struct fattr *, const struct fattr *);
void		 fattr_mergedefault(struct fattr *);
void		 fattr_override(struct fattr *, const struct fattr *, int);
int		 fattr_makenode(const struct fattr *, const char *);
int		 fattr_delete(const char *path);
int		 fattr_install(struct fattr *, const char *, const char *);
int		 fattr_equal(const struct fattr *, const struct fattr *);
void		 fattr_free(struct fattr *);
int		 fattr_supported(int);

#endif /* !_FATTR_H_ */
