/* OPENBSD ORIGINAL: lib/libc/stdlib/realpath.c */

/*
 * Copyright (c) 1994
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Jan-Simon Pendry.
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

#include "includes.h"

#if !defined(HAVE_REALPATH) || defined(BROKEN_REALPATH)

#if defined(LIBC_SCCS) && !defined(lint)
static char *rcsid = "$OpenBSD: realpath.c,v 1.10 2003/08/01 21:04:59 millert Exp $";
#endif /* LIBC_SCCS and not lint */

#include <sys/param.h>
#include <sys/stat.h>

#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/*
 * MAXSYMLINKS
 */
#ifndef MAXSYMLINKS
#define MAXSYMLINKS 5
#endif

/*
 * char *realpath(const char *path, char resolved_path[MAXPATHLEN]);
 *
 * Find the real name of path, by removing all ".", ".." and symlink
 * components.  Returns (resolved) on success, or (NULL) on failure,
 * in which case the path which caused trouble is left in (resolved).
 */
char *
realpath(const char *path, char *resolved)
{
	struct stat sb;
	int fd, n, needslash, serrno = 0;
	char *p, *q, wbuf[MAXPATHLEN], start[MAXPATHLEN];
	int symlinks = 0;

	/* Save the starting point. */
	getcwd(start,MAXPATHLEN);	
	if ((fd = open(".", O_RDONLY)) < 0) {
		(void)strlcpy(resolved, ".", MAXPATHLEN);
		return (NULL);
	}
	close(fd);

	/* Convert "." -> "" to optimize away a needless lstat() and chdir() */
	if (path[0] == '.' && path[1] == '\0')
		path = "";

	/*
	 * Find the dirname and basename from the path to be resolved.
	 * Change directory to the dirname component.
	 * lstat the basename part.
	 *     if it is a symlink, read in the value and loop.
	 *     if it is a directory, then change to that directory.
	 * get the current directory name and append the basename.
	 */
	strlcpy(resolved, path, MAXPATHLEN);
loop:
	q = strrchr(resolved, '/');
	if (q != NULL) {
		p = q + 1;
		if (q == resolved)
			q = "/";
		else {
			do {
				--q;
			} while (q > resolved && *q == '/');
			q[1] = '\0';
			q = resolved;
		}
		if (chdir(q) < 0)
			goto err1;
	} else
		p = resolved;

	/* Deal with the last component. */
	if (*p != '\0' && lstat(p, &sb) == 0) {
		if (S_ISLNK(sb.st_mode)) {
			if (++symlinks > MAXSYMLINKS) {
				serrno = ELOOP;
				goto err1;
			}
			n = readlink(p, resolved, MAXPATHLEN-1);
			if (n < 0)
				goto err1;
			resolved[n] = '\0';
			goto loop;
		}
		if (S_ISDIR(sb.st_mode)) {
			if (chdir(p) < 0)
				goto err1;
			p = "";
		}
	}

	/*
	 * Save the last component name and get the full pathname of
	 * the current directory.
	 */
	(void)strlcpy(wbuf, p, sizeof wbuf);
	if (getcwd(resolved, MAXPATHLEN) == 0)
		goto err1;

	/*
	 * Join the two strings together, ensuring that the right thing
	 * happens if the last component is empty, or the dirname is root.
	 */
	if (resolved[0] == '/' && resolved[1] == '\0')
		needslash = 0;
	else
		needslash = 1;

	if (*wbuf) {
		if (strlen(resolved) + strlen(wbuf) + needslash >= MAXPATHLEN) {
			serrno = ENAMETOOLONG;
			goto err1;
		}
		if (needslash)
			strlcat(resolved, "/", MAXPATHLEN);
		strlcat(resolved, wbuf, MAXPATHLEN);
	}

	/* Go back to where we came from. */
	if (chdir(start) < 0) {
		serrno = errno;
		goto err2;
	}
	return (resolved);

err1:	chdir(start);
err2:	errno = serrno;
	return (NULL);
}
#endif /* !defined(HAVE_REALPATH) || defined(BROKEN_REALPATH) */
