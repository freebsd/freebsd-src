/*
 * Copyright (c) 1983, 1993, 1994
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
static char orig_sccsid[] = "@(#)opendir.c	8.2 (Berkeley) 2/12/94";
static char sccsid[] = "@(#)libc.opendir.c	8.1 (Berkeley) 2/15/94";
#endif /* LIBC_SCCS and not lint */

#include <sys/param.h>
#include <sys/mount.h>

#include <dirent.h>
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
	DIR *dirp;
	int fd;
	int incr;
	struct statfs sfb;

	if ((fd = open(name, 0)) == -1)
		return (NULL);
	if (fcntl(fd, F_SETFD, FD_CLOEXEC) == -1 ||
	    (dirp = (DIR *)malloc(sizeof(DIR))) == NULL) {
		close(fd);
		return (NULL);
	}

	/*
	 * If CLBYTES is an exact multiple of DIRBLKSIZ, use a CLBYTES
	 * buffer that it cluster boundary aligned.
	 * Hopefully this can be a big win someday by allowing page
	 * trades trade to user space to be done by getdirentries()
	 */
	if ((CLBYTES % DIRBLKSIZ) == 0)
		incr = CLBYTES;
	else
		incr = DIRBLKSIZ;

#ifdef MOUNT_UNION
	/*
	 * Determine whether this directory is the top of a union stack.
	 */
	if (fstatfs(fd, &sfb) < 0) {
		free(dirp);
		close(fd);
		return (NULL);
	}

	if (sfb.f_type == MOUNT_UNION) {
		int len = 0;
		int space = 0;
		char *buf = 0;
		char *ddptr = 0;
		int n;
		struct dirent **dpv;

		/*
		 * The strategy here is to read all the directory
		 * entries into a buffer, sort the buffer, and
		 * remove duplicate entries by setting the inode
		 * number to zero.
		 */

		/*
		 * Fixup dd_loc to be non-zero to fake out readdir
		 */
		dirp->dd_loc = sizeof(void *);

		do {
			/*
			 * Always make at least DIRBLKSIZ bytes
			 * available to getdirentries
			 */
			if (space < DIRBLKSIZ) {
				space += incr;
				len += incr;
				buf = realloc(buf, len);
				if (buf == NULL) {
					free(dirp);
					close(fd);
					return (NULL);
				}
				ddptr = buf + (len - space) + dirp->dd_loc;
			}

			n = getdirentries(fd, ddptr, space, &dirp->dd_seek);
			if (n > 0) {
				ddptr += n;
				space -= n;
			}
		} while (n > 0);

		/*
		 * There is now a buffer full of (possibly) duplicate
		 * names.
		 */
		dirp->dd_buf = buf;

		/*
		 * Go round this loop twice...
		 *
		 * Scan through the buffer, counting entries.
		 * On the second pass, save pointers to each one.
		 * Then sort the pointers and remove duplicate names.
		 */
		for (dpv = 0;;) {
			n = 0;
			ddptr = buf + dirp->dd_loc;
			while (ddptr < buf + len) {
				struct dirent *dp;

				dp = (struct dirent *) ddptr;
				if ((int)dp & 03)
					break;
				if ((dp->d_reclen <= 0) ||
				    (dp->d_reclen > (buf + len + 1 - ddptr)))
					break;
				ddptr += dp->d_reclen;
				if (dp->d_fileno) {
					if (dpv)
						dpv[n] = dp;
					n++;
				}
			}

			if (dpv) {
				struct dirent *xp;

				/*
				 * If and when whiteouts happen,
				 * this sort would need to be stable.
				 */
				heapsort(dpv, n, sizeof(*dpv), alphasort);

				dpv[n] = NULL;
				xp = NULL;

				/*
				 * Scan through the buffer in sort order,
				 * zapping the inode number of any
				 * duplicate names.
				 */
				for (n = 0; dpv[n]; n++) {
					struct dirent *dp = dpv[n];

					if ((xp == NULL) ||
					    strcmp(dp->d_name, xp->d_name))
						xp = dp;
					else
						dp->d_fileno = 0;
				}

				free(dpv);
				break;
			} else {
				dpv = malloc((n+1) * sizeof(struct dirent *));
				if (dpv == NULL)
					break;
			}
		}

		dirp->dd_len = len;
		dirp->dd_size = ddptr - dirp->dd_buf;
	} else
#endif /* MOUNT_UNION */
	{
		dirp->dd_len = incr;
		dirp->dd_buf = malloc(dirp->dd_len);
		if (dirp->dd_buf == NULL) {
			free(dirp);
			close (fd);
			return (NULL);
		}
		dirp->dd_seek = 0;
		dirp->dd_loc = 0;
	}

	dirp->dd_fd = fd;

	/*
	 * Set up seek point for rewinddir.
	 */
	dirp->dd_rewind = telldir(dirp);

	return (dirp);
}
