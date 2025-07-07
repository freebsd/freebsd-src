/*-
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 1987, 1993
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

#include "namespace.h"
#include <sys/param.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include "un-namespace.h"

char *_mktemp(char *);

static int _gettemp(int, char *, int *, int, int, int);

static const unsigned char padchar[] =
"0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";

int
mkostempsat(int dfd, char *path, int slen, int oflags)
{
	int fd;

	return (_gettemp(dfd, path, &fd, 0, slen, oflags) ? fd : -1);
}

int
mkostemps(char *path, int slen, int oflags)
{
	int fd;

	return (_gettemp(AT_FDCWD, path, &fd, 0, slen, oflags) ? fd : -1);
}

int
mkstemps(char *path, int slen)
{
	int fd;

	return (_gettemp(AT_FDCWD, path, &fd, 0, slen, 0) ? fd : -1);
}

int
mkostemp(char *path, int oflags)
{
	int fd;

	return (_gettemp(AT_FDCWD, path, &fd, 0, 0, oflags) ? fd : -1);
}

int
mkstemp(char *path)
{
	int fd;

	return (_gettemp(AT_FDCWD, path, &fd, 0, 0, 0) ? fd : -1);
}

char *
mkdtemp(char *path)
{
	return (_gettemp(AT_FDCWD, path, (int *)NULL, 1, 0, 0) ? path : (char *)NULL);
}

char *
_mktemp(char *path)
{
	return (_gettemp(AT_FDCWD, path, (int *)NULL, 0, 0, 0) ? path : (char *)NULL);
}

__warn_references(mktemp,
    "warning: mktemp() possibly used unsafely; consider using mkstemp()");

char *
mktemp(char *path)
{
	return (_mktemp(path));
}

static int
_gettemp(int dfd, char *path, int *doopen, int domkdir, int slen, int oflags)
{
	char *start, *trv, *suffp, *carryp;
	char *pad;
	struct stat sbuf;
	uint32_t rand;
	char carrybuf[MAXPATHLEN];
	int saved;

	if ((doopen != NULL && domkdir) || slen < 0 ||
	    (oflags & ~(O_APPEND | O_DIRECT | O_SHLOCK | O_EXLOCK | O_SYNC |
	    O_CLOEXEC | O_CLOFORK)) != 0) {
		errno = EINVAL;
		return (0);
	}

	trv = path + strlen(path);
	if (trv - path >= MAXPATHLEN) {
		errno = ENAMETOOLONG;
		return (0);
	}
	trv -= slen;
	suffp = trv;
	--trv;
	if (trv < path || NULL != strchr(suffp, '/')) {
		errno = EINVAL;
		return (0);
	}

	/* Fill space with random characters */
	while (trv >= path && *trv == 'X') {
		rand = arc4random_uniform(sizeof(padchar) - 1);
		*trv-- = padchar[rand];
	}
	start = trv + 1;

	saved = 0;
	oflags |= O_CREAT | O_EXCL | O_RDWR;
	for (;;) {
		if (doopen) {
			*doopen = _openat(dfd, path, oflags, 0600);
			if (*doopen >= 0)
				return (1);
			if (errno != EEXIST)
				return (0);
		} else if (domkdir) {
			if (mkdir(path, 0700) == 0)
				return (1);
			if (errno != EEXIST)
				return (0);
		} else if (lstat(path, &sbuf))
			return (errno == ENOENT);

		/* save first combination of random characters */
		if (!saved) {
			memcpy(carrybuf, start, suffp - start);
			saved = 1;
		}

		/* If we have a collision, cycle through the space of filenames */
		for (trv = start, carryp = carrybuf;;) {
			/* have we tried all possible permutations? */
			if (trv == suffp)
				return (0); /* yes - exit with EEXIST */
			pad = strchr(padchar, *trv);
			if (pad == NULL) {
				/* this should never happen */
				errno = EIO;
				return (0);
			}
			/* increment character */
			*trv = (*++pad == '\0') ? padchar[0] : *pad;
			/* carry to next position? */
			if (*trv == *carryp) {
				/* increment position and loop */
				++trv;
				++carryp;
			} else {
				/* try with new name */
				break;
			}
		}
	}
	/*NOTREACHED*/
}
