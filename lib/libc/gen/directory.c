/*
 * Copyright (c) 1983 Regents of the University of California.
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

#if defined(LIBC_SCCS) && !defined(lint)
static char sccsid[] = "@(#)closedir.c	5.9 (Berkeley) 2/23/91";
#endif /* LIBC_SCCS and not lint */

/*
 * One of these structures is malloced to describe the current directory
 * position each time telldir is called. It records the current magic 
 * cookie returned by getdirentries and the offset within the buffer
 * associated with that return value.
 */
struct ddloc {
	struct	ddloc *loc_next;/* next structure in list */
	long	loc_index;	/* key associated with structure */
	long	loc_seek;	/* magic cookie returned by getdirentries */
	long	loc_loc;	/* offset of entry in buffer */
};

static long	dd_loccnt = 0;	/* Index of entry for sequential telldir's */

#include <sys/types.h>
#include <sys/param.h>
#include <dirent.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>

/*
 * close a directory.
 */
int
closedir(dirp)
	register DIR *dirp;
{
	struct ddloc *ptr, *nextptr;
	int fd;

	fd = dirp->dd_fd;
	dirp->dd_fd = -1;
	dirp->dd_loc = 0;
	for (ptr = (struct ddloc*)dirp->dd_ddloc; ptr; ptr = nextptr) {
		nextptr = ptr->loc_next;
		free((void *)ptr);
	}
	free((void *)dirp->dd_buf);
	free((void *)dirp);
	return(close(fd));
}

/*
 * open a directory.
 */
DIR *
opendir(name)
	const char *name;
{
	register DIR *dirp;
	register int fd;

	if ((fd = open(name, 0)) == -1)
		return NULL;
	if (fcntl(fd, F_SETFD, 1) == -1 ||
	    (dirp = (DIR *)malloc(sizeof(DIR))) == NULL) {
		close (fd);
		return NULL;
	}
	/*
	 * If CLSIZE is an exact multiple of DIRBLKSIZ, use a CLSIZE
	 * buffer that it cluster boundary aligned.
	 * Hopefully this can be a big win someday by allowing page trades
	 * to user space to be done by getdirentries()
	 */
	if ((CLSIZE % DIRBLKSIZ) == 0) {
		dirp->dd_buf = malloc(CLSIZE);
		dirp->dd_len = CLSIZE;
	} else {
		dirp->dd_buf = malloc(DIRBLKSIZ);
		dirp->dd_len = DIRBLKSIZ;
	}
	if (dirp->dd_buf == NULL) {
		close (fd);
		free(dirp);
		return NULL;
	}
	dirp->dd_fd = fd;
	dirp->dd_loc = 0;
	dirp->dd_seek = 0;
	dirp->dd_ddloc = NULL;

	return dirp;
}

/*
 * get next entry in a directory.
 */
struct dirent *
readdir(dirp)
	register DIR *dirp;
{
	register struct dirent *dp;

	for (;;) {
		if (dirp->dd_loc == 0) {
			dirp->dd_size = getdirentries(dirp->dd_fd,
			    dirp->dd_buf, dirp->dd_len, &dirp->dd_seek);
			if (dirp->dd_size <= 0)
				return NULL;
		}
		if (dirp->dd_loc >= dirp->dd_size) {
			dirp->dd_loc = 0;
			continue;
		}
		dp = (struct dirent *)(dirp->dd_buf + dirp->dd_loc);
		if ((int)dp & 03)	/* bogus pointer check */
			return NULL;
		if (dp->d_reclen <= 0 ||
		    dp->d_reclen > dirp->dd_len + 1 - dirp->dd_loc)
			return NULL;
		dirp->dd_loc += dp->d_reclen;
		if (dp->d_ino == 0)
			continue;
		return (dp);
	}
}

void
rewinddir(dirp)
	DIR *dirp;
{
	(void)lseek(dirp->dd_fd, 0, 0);
	dirp->dd_seek = 0;
	dirp->dd_loc = 0;
}

/*
 * Seek to an entry in a directory.
 * _seekdir is in telldir.c so that it can share opaque data structures.
 */
void
seekdir(dirp, loc)
	DIR *dirp;
	long loc;
{
	register struct ddloc **prevlp;
	register struct ddloc *lp;
	struct dirent *dp;
	extern long lseek();

	prevlp = (struct ddloc **)&(dirp->dd_ddloc);
	lp = *prevlp;
	while (lp != NULL) {
		if (lp->loc_index == loc)
			break;
		prevlp = &lp->loc_next;
		lp = lp->loc_next;
	}
	if (lp) {
		if (lp->loc_seek != dirp->dd_seek) {
			if (lseek(dirp->dd_fd, lp->loc_seek, 0) == -1) {
				*prevlp = lp->loc_next;
				free(lp);
				return;
			}
			dirp->dd_seek = lp->loc_seek;
			dirp->dd_loc = 0;
			while (dirp->dd_loc < lp->loc_loc) {
				if (!(dp = readdir(dirp))) {
					*prevlp = lp->loc_next;
					free(lp);
					return;
				}
			}
		}
	}
}

/*
 * return a pointer into a directory
 */
long
telldir(dirp)
	const DIR *dirp;
{
	register struct ddloc *lp, **fakeout;

	if (lp = (struct ddloc *)malloc(sizeof(struct ddloc))) {
		lp->loc_index = dd_loccnt++;
		lp->loc_seek = dirp->dd_seek;
		lp->loc_loc = dirp->dd_loc;
		lp->loc_next = dirp->dd_ddloc;

		/* Compiler won't let us change anything pointed to by db directly */
		/* So we fake to the left and do it anyway */
		/* Wonder if the compile optomizes it to the correct solution */
		fakeout = (struct ddloc **)&(dirp->dd_ddloc);
		*fakeout = lp;

		return (lp->loc_index);
	}
	return (-1);
}

