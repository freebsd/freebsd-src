/*
 * Copyright (c) 1988 The Regents of the University of California.
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
static char sccsid[] = "@(#)ttyname.c	5.10 (Berkeley) 5/6/91";
#endif /* LIBC_SCCS and not lint */

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <dirent.h>
#include <sgtty.h>
#include <db.h>
#include <unistd.h>
#include <paths.h>

static char buf[sizeof(_PATH_DEV) + MAXNAMLEN] = _PATH_DEV;

char *
ttyname(fd)
	int fd;
{
	struct stat sb;
	struct sgttyb ttyb;
	DB *db;
	DBT data, key;
	struct {
		mode_t type;
		dev_t dev;
	} bkey;
	static char *__oldttyname();

	/* Must be a terminal. */
	if (ioctl(fd, TIOCGETP, &ttyb) < 0)
		return(NULL);
	/* Must be a character device. */
	if (fstat(fd, &sb) || !S_ISCHR(sb.st_mode))
		return(NULL);

	if (db = dbopen(_PATH_DEVDB, O_RDONLY, 0, DB_HASH, NULL)) {
		bkey.type = S_IFCHR;
		bkey.dev = sb.st_rdev;
		key.data = &bkey;
		key.size = sizeof(bkey);
		if (!(db->get)(db, &key, &data, 0)) {
			bcopy(data.data,
			    buf + sizeof(_PATH_DEV) - 1, data.size);
			return(buf);
		}
	}
	return(__oldttyname(fd, &sb));
}

static char *
__oldttyname(fd, sb)
	int fd;
	struct stat *sb;
{
	register struct dirent *dirp;
	register DIR *dp;
	struct stat dsb;
	char *rval, *strcpy();

	if ((dp = opendir(_PATH_DEV)) == NULL)
		return(NULL);

	for (rval = NULL; dirp = readdir(dp);) {
		if (dirp->d_fileno != sb->st_ino)
			continue;
		bcopy(dirp->d_name, buf + sizeof(_PATH_DEV) - 1,
		    dirp->d_namlen + 1);
		if (stat(buf, &dsb) || sb->st_dev != dsb.st_dev ||
		    sb->st_ino != dsb.st_ino)
			continue;
		rval = buf;
		break;
	}
	(void)closedir(dp);
	return(rval);
}
