/*-
 * Copyright (c) 1992 Diomidis Spinellis.
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Diomidis Spinellis of Imperial College, University of London.
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
static char sccsid[] = "@(#)misc.c	8.1 (Berkeley) 6/6/93";
#endif /* not lint */

#include <sys/types.h>

#include <errno.h>
#include <regex.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "defs.h"
#include "extern.h"

/*
 * malloc with result test
 */
void *
xmalloc(size)
	u_int size;
{
	void *p;

	if ((p = malloc(size)) == NULL)
		err(FATAL, "%s", strerror(errno));
	return (p);
}

/*
 * realloc with result test
 */
void *
xrealloc(p, size)
	void *p;
	u_int size;
{
	if (p == NULL)			/* Compatibility hack. */
		return (xmalloc(size));

	if ((p = realloc(p, size)) == NULL)
		err(FATAL, "%s", strerror(errno));
	return (p);
}

/*
 * Return a string for a regular expression error passed.  This is a overkill,
 * because of the silly semantics of regerror (we can never know the size of
 * the buffer).
 */
char *
strregerror(errcode, preg)
	int errcode;
	regex_t *preg;
{
	static char *oe;
	size_t s;

	if (oe != NULL)
		free(oe);
	s = regerror(errcode, preg, "", 0);
	oe = xmalloc(s);
	(void)regerror(errcode, preg, oe, s);
	return (oe);
}

#if __STDC__
#include <stdarg.h>
#else
#include <varargs.h>
#endif
/*
 * Error reporting function
 */
void
#if __STDC__
err(int severity, const char *fmt, ...)
#else
err(severity, fmt, va_alist)
	int severity;
	char *fmt;
        va_dcl
#endif
{
	va_list ap;
#if __STDC__
	va_start(ap, fmt);
#else
	va_start(ap);
#endif
	(void)fprintf(stderr, "sed: ");
	switch (severity) {
	case WARNING:
	case COMPILE:
		(void)fprintf(stderr, "%lu: %s: ", linenum, fname);
	}
	(void)vfprintf(stderr, fmt, ap);
	va_end(ap);
	(void)fprintf(stderr, "\n");
	if (severity == WARNING)
		return;
	exit(1);
	/* NOTREACHED */
}
