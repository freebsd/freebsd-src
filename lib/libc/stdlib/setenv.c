/*
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
static char sccsid[] = "@(#)setenv.c	8.1 (Berkeley) 6/4/93";
#endif /* LIBC_SCCS and not lint */
#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <errno.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

char **__alloced;       /* if allocated space before */

char *__findenv(const char *, int *);

/*
 * setenv --
 *	Set the value of the environmental variable "name" to be
 *	"value".  If rewrite is set, replace any current value.
 */
int
setenv(name, value, rewrite)
	const char *name;
	const char *value;
	int rewrite;
{
	extern char **environ;
	char *c;
	int l_value, offset;

	if (name == NULL || !*name || strchr(name, '=') != NULL) {
		errno = EINVAL;
		return (-1);
	}

	l_value = strlen(value);
	if ((c = __findenv(name, &offset))) {	/* find if already exists */
		if (!rewrite)
			return (0);
		if (strlen(c) >= l_value) {	/* old larger; copy over */
			while ( (*c++ = *value++) );
			return (0);
		}
	} else {					/* create new slot */
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
	if (!(environ[offset] =			/* name + `=' + value */
	    (char *)malloc((size_t)(strlen(name) + l_value + 2))))
		return (-1);
	for (c = environ[offset]; (*c = *name++) && *c != '='; ++c);
	for (*c++ = '='; (*c++ = *value++); );
	return (0);
}

/*
 * unsetenv(name) --
 *	Delete environmental variable "name".
 */
int
unsetenv(name)
	const char *name;
{
	extern char **environ;
	char **p;
	int offset;

	if (name == NULL || !*name || strchr(name, '=') != NULL) {
		errno = EINVAL;
		return (-1);
	}

	while (__findenv(name, &offset))	/* if set multiple times */
		for (p = &environ[offset];; ++p)
			if (!(*p = *(p + 1)))
				break;
	return (0);
}
