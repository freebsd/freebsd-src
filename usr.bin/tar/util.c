/*-
 * Copyright (c) 2003-2004 Tim Kientzle
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer
 *    in this position and unchanged.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR(S) ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR(S) BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "bsdtar_platform.h"
__FBSDID("$FreeBSD$");

#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bsdtar.h"

static void	bsdtar_vwarnc(int code, const char *fmt, va_list ap);

/*
 * Print a string, taking care with any non-printable characters.
 */

void
safe_fprintf(FILE *f, const char *fmt, ...)
{
	char *buff;
	int bufflength;
	int length;
	va_list ap;
	char *p;

	bufflength = 512;
	buff = malloc(bufflength);

	va_start(ap, fmt);
	length = vsnprintf(buff, bufflength, fmt, ap);
	if (length >= bufflength) {
		bufflength = length+1;
		buff = realloc(buff, bufflength);
		length = vsnprintf(buff, bufflength, fmt, ap);
	}
	va_end(ap);

	for (p=buff; *p != '\0'; p++) {
		unsigned char c = *p;
		if (isprint(c) && c != '\\')
			putc(c, f);
		else
			switch (c) {
			case '\a': putc('\\', f); putc('a', f); break;
			case '\b': putc('\\', f); putc('b', f); break;
			case '\f': putc('\\', f); putc('f', f); break;
			case '\n': putc('\\', f); putc('n', f);	break;
#if '\r' != '\n'
			/* On some platforms, \n and \r are the same. */
			case '\r': putc('\\', f); putc('r', f);	break;
#endif
			case '\t': putc('\\', f); putc('t', f);	break;
			case '\v': putc('\\', f); putc('v', f);	break;
			case '\\': putc('\\', f); putc('\\', f); break;
			default:
				fprintf(f, "\\%03o", c);
			}
	}
	free(buff);
}

static void
bsdtar_vwarnc(int code, const char *fmt, va_list ap)
{
	const char	*p;

	p = strrchr(bsdtar_progname(), '/');
	if (p != NULL)
	    p++;
	else
	    p = bsdtar_progname();
	fprintf(stderr, "%s: ", p);

	vfprintf(stderr, fmt, ap);
	if (code != 0)
		fprintf(stderr, ": %s", strerror(code));

	fprintf(stderr, "\n");
}

void
bsdtar_warnc(int code, const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	bsdtar_vwarnc(code, fmt, ap);
	va_end(ap);
}

void
bsdtar_errc(int eval, int code, const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	bsdtar_vwarnc(code, fmt, ap);
	va_end(ap);
	exit(eval);
}

int
yes(const char *fmt, ...)
{
	char buff[32];
	char *p;
	ssize_t l;

	va_list ap;
	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
	fprintf(stderr, " (y/N)? ");
	fflush(stderr);

	l = read(2, buff, sizeof(buff));
	buff[l] = 0;

	for (p = buff; *p != '\0'; p++) {
		if (isspace(*p))
			continue;
		switch(*p) {
		case 'y': case 'Y':
			return (1);
		case 'n': case 'N':
			return (0);
		default:
			return (0);
		}
	}

	return (0);
}
