/*
 * Copyright (c) 1989, 1993
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

#if !defined(BUILTIN) && !defined(SHELL)
#ifndef lint
static char const copyright[] =
"@(#) Copyright (c) 1989, 1993\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif /* not lint */
#endif

#ifndef lint
#if 0
static char const sccsid[] = "@(#)printf.c	8.1 (Berkeley) 7/20/93";
#endif
static const char rcsid[] =
  "$FreeBSD$";
#endif /* not lint */

#include <sys/types.h>

#include <err.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#ifdef SHELL
#define main printfcmd
#include "bltin/bltin.h"
#include "memalloc.h"
#else
#define	warnx1(a, b, c)		warnx(a)
#define	warnx2(a, b, c)		warnx(a, b)
#define	warnx3(a, b, c)		warnx(a, b, c)
#endif

#ifndef BUILTIN
#include <locale.h>
#endif

#define PF(f, func) { \
	char *b = NULL; \
	if (fieldwidth) \
		if (precision) \
			(void)asprintf(&b, f, fieldwidth, precision, func); \
		else \
			(void)asprintf(&b, f, fieldwidth, func); \
	else if (precision) \
		(void)asprintf(&b, f, precision, func); \
	else \
		(void)asprintf(&b, f, func); \
	if (b) { \
		(void)fputs(b, stdout); \
		free(b); \
	} \
}

static int	 asciicode __P((void));
static void	 escape __P((char *));
static int	 getchr __P((void));
static double	 getdouble __P((void));
static int	 getint __P((int *));
static int	 getquad __P((quad_t *));
static char	*getstr __P((void));
static char	*mklong __P((char *, int));
static void	 usage __P((void));

static char **gargv;

int
#ifdef BUILTIN
progprintf(argc, argv)
#else
main(argc, argv)
#endif
	int argc;
	char *argv[];
{
	static char *skip1, *skip2;
	int ch, end, fieldwidth, precision;
	char convch, nextch, *format, *fmt, *start;

#ifndef BUILTIN
	(void) setlocale(LC_NUMERIC, "");
#endif
	while ((ch = getopt(argc, argv, "")) != -1)
		switch (ch) {
		case '?':
		default:
			usage();
			return (1);
		}
	argc -= optind;
	argv += optind;

	if (argc < 1) {
		usage();
		return (1);
	}

	/*
	 * Basic algorithm is to scan the format string for conversion
	 * specifications -- once one is found, find out if the field
	 * width or precision is a '*'; if it is, gather up value.  Note,
	 * format strings are reused as necessary to use up the provided
	 * arguments, arguments of zero/null string are provided to use
	 * up the format string.
	 */
	skip1 = "#-+ 0";
	skip2 = "0123456789";

	escape(fmt = format = *argv);		/* backslash interpretation */
	gargv = ++argv;
	for (;;) {
		end = 0;
		/* find next format specification */
next:		for (start = fmt;; ++fmt) {
			if (!*fmt) {
				/* avoid infinite loop */
				if (end == 1) {
					warnx1("missing format character",
					    NULL, NULL);
					return (1);
				}
				end = 1;
				if (fmt > start)
					(void)printf("%s", start);
				if (!*gargv)
					return (0);
				fmt = format;
				goto next;
			}
			/* %% prints a % */
			if (*fmt == '%') {
				if (*++fmt != '%')
					break;
				*fmt++ = '\0';
				(void)printf("%s", start);
				goto next;
			}
		}

		/* skip to field width */
		for (; strchr(skip1, *fmt); ++fmt);
		if (*fmt == '*') {
			if (getint(&fieldwidth))
				return (1);
			++fmt;
		} else {
			fieldwidth = 0;

			/* skip to possible '.', get following precision */
			for (; strchr(skip2, *fmt); ++fmt);
		}
		if (*fmt == '.') {
			/* precision present? */
			++fmt;
			if (*fmt == '*') {
				if (getint(&precision))
					return (1);
				++fmt;
			} else {
				precision = 0;

				/* skip to conversion char */
				for (; strchr(skip2, *fmt); ++fmt);
			}
		} else
			precision = 0;
		if (!*fmt) {
			warnx1("missing format character", NULL, NULL);
			return (1);
		}

		convch = *fmt;
		nextch = *++fmt;
		*fmt = '\0';
		switch(convch) {
		case 'c': {
			char p;

			p = getchr();
			PF(start, p);
			break;
		}
		case 's': {
			char *p;

			p = getstr();
			PF(start, p);
			break;
		}
		case 'd': case 'i': case 'o': case 'u': case 'x': case 'X': {
			quad_t p;
			char *f;

			if ((f = mklong(start, convch)) == NULL)
				return (1);
			if (getquad(&p))
				return (1);
			PF(f, p);
			break;
		}
		case 'e': case 'E': case 'f': case 'g': case 'G': {
			double p;

			p = getdouble();
			PF(start, p);
			break;
		}
		default:
			warnx2("illegal format character %c", convch, NULL);
			return (1);
		}
		*fmt = nextch;
	}
	/* NOTREACHED */
}

static char *
mklong(str, ch)
	char *str;
	int ch;
{
	static char *copy;
	static size_t copy_size;
	size_t len, newlen;
	char *newcopy;

	len = strlen(str) + 2;
	if (len > copy_size) {
		newlen = ((len + 1023) >> 10) << 10;
#ifdef SHELL
		if ((newcopy = ckrealloc(copy, newlen)) == NULL)
#else
		if ((newcopy = realloc(copy, newlen)) == NULL)
#endif
			return (NULL);
		copy = newcopy;
		copy_size = newlen;
	}

	memmove(copy, str, len - 3);
	copy[len - 3] = 'q';
	copy[len - 2] = ch;
	copy[len - 1] = '\0';
	return (copy);
}

static void
escape(fmt)
	register char *fmt;
{
	register char *store;
	register int value, c;

	for (store = fmt; (c = *fmt); ++fmt, ++store) {
		if (c != '\\') {
			*store = c;
			continue;
		}
		switch (*++fmt) {
		case '\0':		/* EOS, user error */
			*store = '\\';
			*++store = '\0';
			return;
		case '\\':		/* backslash */
		case '\'':		/* single quote */
			*store = *fmt;
			break;
		case 'a':		/* bell/alert */
			*store = '\7';
			break;
		case 'b':		/* backspace */
			*store = '\b';
			break;
		case 'f':		/* form-feed */
			*store = '\f';
			break;
		case 'n':		/* newline */
			*store = '\n';
			break;
		case 'r':		/* carriage-return */
			*store = '\r';
			break;
		case 't':		/* horizontal tab */
			*store = '\t';
			break;
		case 'v':		/* vertical tab */
			*store = '\13';
			break;
					/* octal constant */
		case '0': case '1': case '2': case '3':
		case '4': case '5': case '6': case '7':
			for (c = 3, value = 0;
			    c-- && *fmt >= '0' && *fmt <= '7'; ++fmt) {
				value <<= 3;
				value += *fmt - '0';
			}
			--fmt;
			*store = value;
			break;
		default:
			*store = *fmt;
			break;
		}
	}
	*store = '\0';
}

static int
getchr()
{
	if (!*gargv)
		return ('\0');
	return ((int)**gargv++);
}

static char *
getstr()
{
	if (!*gargv)
		return ("");
	return (*gargv++);
}

static char *Number = "+-.0123456789";
static int
getint(ip)
	int *ip;
{
	quad_t val;

	if (getquad(&val))
		return (1);
	if (val < INT_MIN || val > INT_MAX) {
		warnx3("%s: %s", *gargv, strerror(ERANGE));
		return (1);
	}
	*ip = (int)val;
	return (0);
}

static int
getquad(lp)
	quad_t *lp;
{
	quad_t val;
	char *ep;

	if (!*gargv) {
		*lp = 0;
		return (0);
	}
	if (strchr(Number, **gargv)) {
		errno = 0;
		val = strtoq(*gargv, &ep, 0);
		if (*ep != '\0') {
			warnx2("%s: illegal number", *gargv, NULL);
			return (1);
		}
		if (errno == ERANGE)
			if (val == QUAD_MAX) {
				warnx3("%s: %s", *gargv, strerror(ERANGE));
				return (1);
			}
			if (val == QUAD_MIN) {
				warnx3("%s: %s", *gargv, strerror(ERANGE));
				return (1);
			}

		*lp = val;
		++gargv;
		return (0);
	}
	*lp =  (long)asciicode();
	return (0);
}

static double
getdouble()
{
	if (!*gargv)
		return ((double)0);
	if (strchr(Number, **gargv))
		return (atof(*gargv++));
	return ((double)asciicode());
}

static int
asciicode()
{
	register int ch;

	ch = **gargv;
	if (ch == '\'' || ch == '"')
		ch = (*gargv)[1];
	++gargv;
	return (ch);
}

static void
usage()
{
	(void)fprintf(stderr, "usage: printf format [arg ...]\n");
}
