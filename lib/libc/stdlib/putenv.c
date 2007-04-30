/*-
 * Copyright (c) 1988, 1993
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
static char sccsid[] = "@(#)putenv.c	8.2 (Berkeley) 3/27/94";
#endif /* LIBC_SCCS and not lint */
#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <errno.h>
#include <stdlib.h>
#include <string.h>

extern char **__alloced;        /* if allocated space before */

char *__findenv(const char *, int *);

int
putenv(str)
	char *str;
{
	extern char **environ;
	char *eq;
	int offset;

	if (str == NULL || (eq = strchr(str, '=')) == NULL || eq == str) {
		errno = EINVAL;
		return (-1);
	}

	/* Trimmed version of setenv(3). */
	if (__findenv(str, &offset) == NULL) {
		int cnt;
		char **p;

		for (p = environ, cnt = 0; *p; ++p, ++cnt);
		if (__alloced == environ) {                       /* just increase size */
			p = (char **)realloc((char *)environ,
			    (size_t)(sizeof(char *) * (cnt + 2)));
			if (!p)
				return (-1);
		}
		else {				/* get new space */
						/* copy old entries into it */
			p = (char **)malloc((size_t)(sizeof(char *) * (cnt + 2)));
			if (!p)
				return (-1);
			bcopy(environ, p, cnt * sizeof(char *));
		}
		__alloced = environ = p;
		environ[cnt + 1] = NULL;
		offset = cnt;
	}
	environ[offset] = str;
	return (0);
}
