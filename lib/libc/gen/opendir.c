/*
 * Copyright (c) 1983, 1993
 *	The Regents of the University of California.  All rights reserved.
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
static char sccsid[] = "@(#)opendir.c	8.2 (Berkeley) 2/12/94";
#endif /* LIBC_SCCS and not lint */

#include <sys/param.h>
#include <sys/stat.h>

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>

/*
 * open a directory.
 */
DIR *
opendir(name)
	const char *name;
{
	register DIR *dirp;
	register int fd;
	int saved_errno;
	struct stat sb;

	/*
	 * stat() before open() because opening of special files may be
	 * harmful.  fstat() after open because the file may have changed.
	 */
	if (stat(name, &sb) != 0)
		return NULL;
	if (!S_ISDIR(sb.st_mode)) {
		errno = ENOTDIR;
		return NULL;
	}
	if ((fd = open(name, O_RDONLY | O_NONBLOCK)) == -1)
		return NULL;
	dirp = NULL;
	if (fstat(fd, &sb) != 0)
		goto fail;
	if (!S_ISDIR(sb.st_mode)) {
		errno = ENOTDIR;
		goto fail;
	}
	if (fcntl(fd, F_SETFD, FD_CLOEXEC) == -1 ||
	    (dirp = malloc(sizeof(DIR))) == NULL)
		goto fail;
	/*
	 * If CLBYTES is an exact multiple of DIRBLKSIZ, use a CLBYTES
	 * buffer that it cluster boundary aligned.
	 * Hopefully this can be a big win someday by allowing page trades
	 * to user space to be done by getdirentries()
	 */
	if ((CLBYTES % DIRBLKSIZ) == 0) {
		dirp->dd_buf = malloc(CLBYTES);
		dirp->dd_len = CLBYTES;
	} else {
		dirp->dd_buf = malloc(DIRBLKSIZ);
		dirp->dd_len = DIRBLKSIZ;
	}
	if (dirp->dd_buf == NULL)
		goto fail;
	dirp->dd_fd = fd;
	dirp->dd_loc = 0;
	dirp->dd_seek = 0;
	/*
	 * Set up seek point for rewinddir.
	 */
	dirp->dd_rewind = telldir(dirp);
	return dirp;

fail:
	saved_errno = errno;
	free(dirp);
	close(fd);
	errno = saved_errno;
	return NULL;
}
