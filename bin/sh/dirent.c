/*-
 * Copyright (c) 1991 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Kenneth Almquist.
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

#ifndef lint
/*static char sccsid[] = "from: @(#)dirent.c	5.1 (Berkeley) 3/7/91";*/
static char rcsid[] = "dirent.c,v 1.4 1993/08/01 18:58:21 mycroft Exp";
#endif /* not lint */

#include "shell.h"	/* definitions for pointer, NULL, DIRENT, and BSD */

#if ! DIRENT

#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <dirent.h>

#ifndef S_ISDIR				/* macro to test for directory file */
#define	S_ISDIR(mode)		(((mode) & S_IFMT) == S_IFDIR)
#endif

#ifdef BSD

#ifdef __STDC__
int stat(char *, struct stat *);
#else
int stat();
#endif


/*
 * The BSD opendir routine doesn't check that what is being opened is a
 * directory, so we have to include the check in a wrapper routine.
 */

#undef opendir

DIR *
myopendir(dirname)
	char *dirname;			/* name of directory */
	{
	struct stat statb;

	if (stat(dirname, &statb) != 0 || ! S_ISDIR(statb.st_mode)) {
		errno = ENOTDIR;
		return NULL;		/* not a directory */
	}
	return opendir(dirname);
}

#else /* not BSD */

/*
 * Dirent routines for old style file systems.
 */

#ifdef __STDC__
pointer malloc(unsigned);
void free(pointer);
int open(char *, int, ...);
int close(int);
int fstat(int, struct stat *);
#else
pointer malloc();
void free();
int open();
int close();
int fstat();
#endif


DIR *
opendir(dirname)
	char		*dirname;	/* name of directory */
	{
	register DIR	*dirp;		/* -> malloc'ed storage */
	register int	fd;		/* file descriptor for read */
	struct stat	statb;		/* result of fstat() */

#ifdef O_NDELAY
	fd = open(dirname, O_RDONLY|O_NDELAY);
#else
	fd = open(dirname, O_RDONLY);
#endif
	if (fd < 0)
		return NULL;		/* errno set by open() */

	if (fstat(fd, &statb) != 0 || !S_ISDIR(statb.st_mode)) {
		(void)close(fd);
		errno = ENOTDIR;
		return NULL;		/* not a directory */
	}

	if ((dirp = (DIR *)malloc(sizeof(DIR))) == NULL) {
		(void)close(fd);
		errno = ENOMEM;
		return NULL;		/* not enough memory */
	}

	dirp->dd_fd = fd;
	dirp->dd_nleft = 0;		/* refill needed */

	return dirp;
}



int
closedir(dirp)
	register DIR *dirp;		/* stream from opendir() */
	{
	register int fd;

	if (dirp == NULL) {
		errno = EFAULT;
		return -1;			/* invalid pointer */
	}

	fd = dirp->dd_fd;
	free((pointer)dirp);
	return close(fd);
}



struct dirent *
readdir(dirp)
	register DIR *dirp;		/* stream from opendir() */
	{
	register struct direct *dp;
	register char *p, *q;
	register int i;

	do {
		if ((dirp->dd_nleft -= sizeof (struct direct)) < 0) {
			if ((i = read(dirp->dd_fd,
					   (char *)dirp->dd_buf,
					   DIRBUFENT*sizeof(struct direct))) <= 0) {
				if (i == 0)
					errno = 0;	/* unnecessary */
				return NULL;		/* EOF or error */
			}
			dirp->dd_loc = dirp->dd_buf;
			dirp->dd_nleft = i - sizeof (struct direct);
		}
		dp = dirp->dd_loc++;
	} while (dp->d_ino == 0);
	dirp->dd_entry.d_ino = dp->d_ino;

	/* now copy the name, nul terminating it */
	p = dp->d_name;
	q = dirp->dd_entry.d_name;
	i = DIRSIZ;
	while (--i >= 0 && *p != '\0')
		*q++ = *p++;
	*q = '\0';
	return &dirp->dd_entry;
}

#endif /* BSD */
#endif /* DIRENT */
