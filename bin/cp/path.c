/*-
 * Copyright (c) 1991 The Regents of the University of California.
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

#ifndef lint
static char sccsid[] = "@(#)path.c	5.2 (Berkeley) 10/27/91";
#endif /* not lint */

#include <sys/param.h>
#include <string.h>
#include "extern.h"

/*
 * These functions manipulate paths in PATH_T structures.
 * 
 * They eliminate multiple slashes in paths when they notice them,
 * and keep the path non-slash terminated.
 *
 * Both path_set() and path_append() return 0 if the requested name
 * would be too long.
 */

#define	STRIP_TRAILING_SLASH(p) { \
	while ((p)->p_end > ((p)->p_path + 1) && (p)->p_end[-1] == '/') \
		*--(p)->p_end = 0; \
}

/*
 * Move specified string into path.  Convert "" to "." to handle BSD
 * semantics for a null path.  Strip trailing slashes.
 */
int
path_set(p, string)
	register PATH_T *p;
	char *string;
{
	if (strlen(string) > MAXPATHLEN) {
		err("%s: name too long", string);
		return(0);
	}

	(void)strcpy(p->p_path, string);
	p->p_end = p->p_path + strlen(p->p_path);

	if (p->p_path == p->p_end) {
		*p->p_end++ = '.';
		*p->p_end = 0;
	}

	STRIP_TRAILING_SLASH(p);
	return(1);
}

/*
 * Append specified string to path, inserting '/' if necessary.  Return a
 * pointer to the old end of path for restoration.
 */
char *
path_append(p, name, len)
	register PATH_T *p;
	char *name;
	int len;
{
	char *old;

	old = p->p_end;
	if (len == -1)
		len = strlen(name);

	/* The "+ 1" accounts for the '/' between old path and name. */
	if ((len + p->p_end - p->p_path + 1) > MAXPATHLEN) {
		err("%s/%s: name too long", p->p_path, name);
		return(0);
	}

	/*
	 * This code should always be executed, since paths shouldn't
	 * end in '/'.
	 */
	if (p->p_end[-1] != '/') {
		*p->p_end++ = '/';
		*p->p_end = 0;
	}

	(void)strncat(p->p_end, name, len);
	p->p_end += len;
	*p->p_end = 0;

	STRIP_TRAILING_SLASH(p);
	return(old);
}

/*
 * Restore path to previous value.  (As returned by path_append.)
 */
void
path_restore(p, old)
	PATH_T *p;
	char *old;
{
	p->p_end = old;
	*p->p_end = 0;
}

/*
 * Return basename of path.
 */
char *
path_basename(p)
	PATH_T *p;
{
	char *basename;

	basename = rindex(p->p_path, '/');
	return(basename ? basename + 1 : p->p_path);
}
