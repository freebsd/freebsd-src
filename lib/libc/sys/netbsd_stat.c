/* $Id$
/*	From: NetBSD: stat.c,v 1.2 1997/10/22 00:56:39 fvdl Exp */

/*
 * Copyright (c) 1997 Frank van der Linden
 * All rights reserved.
 * Copyright (c) 1998 John Birrell <jb@cimlogic.com.au>
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed for the NetBSD Project
 *      by Frank van der Linden
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * These are FreeBSD header files. They do not provide NetBSD structure
 * definitions or variable types.
 */
#include <sys/types.h>
#include <sys/stat.h>

/*
 * On systems with 8 byte longs and 4 byte time_ts, padding the time_ts
 * is required in order to have a consistent ABI.  This is because the
 * stat structure used to contain timespecs, which had different
 * alignment constraints than a time_t and a long alone.  The padding
 * should be removed the next time the stat structure ABI is changed.
 * (This will happen whever we change to 8 byte time_t.)
 */
#if defined(__alpha__)			/* XXX XXX XXX */
#define	__STATPAD(x)	int x;
#else
#define	__STATPAD(x)	/* nothing */
#endif

/*
 * This is the new stat structure that NetBSD added for 1.3.
 * The NetBSD syscalls __stat13, __lstat13 and __fstat13 return
 * information in this format. The functions in this file convert
 * that format to the current FreeBSD stat structure. Yuk.
 */
struct stat13 {
	u_int32_t  st_dev;		/* inode's device */
	u_int32_t  st_ino;		/* inode's number */
	u_int32_t  st_mode;		/* inode protection mode */
	u_int32_t  st_nlink;		/* number of hard links */
	u_int32_t  st_uid;		/* user ID of the file's owner */
	u_int32_t  st_gid;		/* group ID of the file's group */
	u_int32_t  st_rdev;		/* device type */
#ifndef _POSIX_SOURCE
	struct	  timespec st_atimespec;/* time of last access */
	struct	  timespec st_mtimespec;/* time of last data modification */
	struct	  timespec st_ctimespec;/* time of last file status change */
#else
	__STATPAD(__pad0)
	time_t	  st_atime;		/* time of last access */
	__STATPAD(__pad1)
	long	  st_atimensec;		/* nsec of last access */
	time_t	  st_mtime;		/* time of last data modification */
	__STATPAD(__pad2)
	long	  st_mtimensec;		/* nsec of last data modification */
	time_t	  st_ctime;		/* time of last file status change */
	__STATPAD(__pad3)
	long	  st_ctimensec;		/* nsec of last file status change */
#endif
	u_int64_t st_size;		/* file size, in bytes */
	int64_t	  st_blocks;		/* blocks allocated for file */
	u_int32_t st_blksize;		/* optimal blocksize for I/O */
	u_int32_t st_flags;		/* user defined flags for file */
	u_int32_t st_gen;		/* file generation number */
	int64_t	  st_qspare[2];
};

int     __stat13 __P((const char *, struct stat13 *));
int     __fstat13 __P((int, struct stat13 *));
int     __lstat13 __P((const char *, struct stat13 *));

/*
 * Convert from a NetBSD 1.3 stat to a FreeBSD stat structure.
 */

static void cvtstat __P((struct stat13 *, struct stat *));

static void
cvtstat(st, ost)
	struct stat13 *st;
	struct stat *ost;
{

	ost->st_dev = st->st_dev;
	ost->st_ino = st->st_ino;
	ost->st_mode = st->st_mode;
	if (st->st_nlink >= (1 << 15))
		ost->st_nlink = (1 << 15) - 1;
	else
		ost->st_nlink = st->st_nlink;
	ost->st_uid = st->st_uid;
	ost->st_gid = st->st_gid;
	ost->st_rdev = st->st_rdev;
	ost->st_atimespec = st->st_atimespec;
	ost->st_mtimespec = st->st_mtimespec;
	ost->st_ctimespec = st->st_ctimespec;
	ost->st_size = st->st_size;
	ost->st_blocks = st->st_blocks;
	ost->st_blksize = st->st_blksize;
	ost->st_flags = st->st_flags;
	ost->st_gen = st->st_gen;
}

int
stat(file, ost)
	const char *file;
	struct stat *ost;
{
	struct stat13 nst;
	int ret;

	if ((ret = __stat13(file, &nst)) < 0)
		return ret;
	cvtstat(&nst, ost);
	return ret;
}

int
fstat(f, ost)
	int f;
	struct stat *ost;
{
	struct stat13 nst;
	int ret;

	if ((ret = __fstat13(f, &nst)) < 0)
		return ret;
	cvtstat(&nst, ost);
	return ret;
}

int
lstat(file, ost)
	const char *file;
	struct stat *ost;
{
	struct stat13 nst;
	int ret;

	if ((ret = __lstat13(file, &nst)) < 0)
		return ret;
	cvtstat(&nst, ost);
	return ret;
}
