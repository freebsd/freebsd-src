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

#define PF(f, func) do { \
	char *b = NULL; \
	if (havewidth) \
		if (haveprec) \
			(void)asprintf(&b, f, fieldwidth, precision, func); \
		else \
			(void)asprintf(&b, f, fieldwidth, func); \
	else if (haveprec) \
		(void)asprintf(&b, f, precision, func); \
	else \
		(void)asprintf(&b, f, func); \
	if (b) { \
		(void)fputs(b, stdout); \
		free(b); \
	} \
} while (0)

static int	 asciicode(void);
static int	 escape(char *, int);
static int	 getchr(void);
static int	 getdouble(double *);
static int	 getint(int *);
static int	 getquads(quad_t *, u_quad_t *, int);
static const char
		*getstr(void);
static char	*mkquad(char *, int);
static void	 usage(void);

static char **gargv;

int
#ifdef BUILTIN
progprintf(int argc, char *argv[])
#else
main(int argc, char *argv[])
#endif
{
	static const char *skip1, *skip2;
	int ch, chopped, end, fieldwidth, haveprec, havewidth, precision, rval;
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

	chopped = escape(fmt = format = *argv, 1);/* backslash interpretation */
	rval = 0;
	gargv = ++argv;
	for (;;) {
		end = 0;
		/* find next format specification */
next:		for (start = fmt;; ++fmt) {
			if (!*fmt) {
				/* avoid infinite loop */
				if (chopped) {
					(void)printf("%s", start);
					return (rval);
				}
				if (end == 1) {
					warnx1("missing format character",
					    NULL, NULL);
					return (1);
				}
				end = 1;
				if (fmt > start)
					(void)printf("%s", start);
				if (!*gargv)
					return (rval);
				fmt = format;
				goto next;
			}
			/* %% prints a % */
			if (*fmt == '%') {
				if (*++fmt != '%')
					break;
				(void)printf("%.*s", (int)(fmt - start), start);
				fmt++;
				goto next;
			}
		}

		/* skip to field width */
		for (; strchr(skip1, *fmt); ++fmt);
		if (*fmt == '*') {
			if (getint(&fieldwidth))
				return (1);
			havewidth = 1;
			++fmt;
		} else {
			havewidth = 0;

			/* skip to possible '.', get following precision */
			for (; strchr(skip2, *fmt); ++fmt);
		}
		if (*fmt == '.') {
			/* precision present? */
			++fmt;
			if (*fmt == '*') {
				if (getint(&precision))
					return (1);
				haveprec = 1;
				++fmt;
			} else {
				haveprec = 0;

				/* skip to conversion char */
				for (; strchr(skip2, *fmt); ++fmt);
			}
		} else
			haveprec = 0;
		if (!*fmt) {
			warnx1("missing format character", NULL, NULL);
			return (1);
		}

		convch = *fmt;
		nextch = *++fmt;
		*fmt = '\0';
		switch(convch) {
		case 'b': {
			char *p;
			int getout;

#ifdef SHELL
			p = savestr(getstr());
#else
			p = strdup(getstr());
#endif
			if (p == NULL) {
				warnx2("%s", strerror(ENOMEM), NULL);
				return (1);
			}
			getout = escape(p, 0);
			*(fmt - 1) = 's';
			PF(start, p);
			*(fmt - 1) = 'b';
#ifdef SHELL
			ckfree(p);
#else
			free(p);
#endif
			if (getout)
				return (rval);
			break;
		}
		case 'c': {
			char p;

			p = getchr();
			PF(start, p);
			break;
		}
		case 's': {
			const char *p;

			p = getstr();
			PF(start, p);
			break;
		}
		case 'd': case 'i': case 'o': case 'u': case 'x': case 'X': {
			char *f;
			quad_t val;
			u_quad_t uval;
			int signedconv;

			signedconv = (convch == 'd' || convch == 'i');
			if ((f = mkquad(start, convch)) == NULL)
				return (1);
			if (getquads(&val, &uval, signedconv))
				rval = 1;
			if (signedconv)
				PF(f, val);
			else
				PF(f, uval);
			break;
		}
		case 'e': case 'E':
		case 'f': case 'F':
		case 'g': case 'G':
		case 'a': case 'A': {
			double p;

			if (getdouble(&p))
				rval = 1;
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
mkquad(char *str, int ch)
{
	static char *copy;
	static size_t copy_size;
	char *newcopy;
	size_t len, newlen;

	len = strlen(str) + 2;
	if (len > copy_size) {
		newlen = ((len + 1023) >> 10) << 10;
#ifdef SHELL
		if ((newcopy = ckrealloc(copy, newlen)) == NULL)
#else
		if ((newcopy = realloc(copy, newlen)) == NULL)
#endif
		{
			warnx2("%s", strerror(ENOMEM), NULL);
			return (NULL);
		}
		copy = newcopy;
		copy_size = newlen;
	}

	memmove(copy, str, len - 3);
	copy[len - 3] = 'q';
	copy[len - 2] = ch;
	copy[len - 1] = '\0';
	return (copy);
}

static int
escape(char *fmt, int percent)
{
	char *store;
	int value, c;

	for (store = fmt; (c = *fmt); ++fmt, ++store) {
		if (c != '\\') {
			*store = c;
			continue;
		}
		switch (*++fmt) {
		case '\0':		/* EOS, user error */
			*store = '\\';
			*++store = '\0';
			return (0);
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
		case 'c':
			*store = '\0';
			return (1);
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
			for (c = *fmt == '0' ? 4 : 3, value = 0;
			    c-- && *fmt >= '0' && *fmt <= '7'; ++fmt) {
				value <<= 3;
				value += *fmt - '0';
			}
			--fmt;
			if (percent && value == '%') {
				*store++ = '%';
				*store = '%';
			} else
				*store = value;
			break;
		default:
			*store = *fmt;
			break;
		}
	}
	*store = '\0';
	return (0);
}

static int
getchr(void)
{
	if (!*gargv)
		return ('\0');
	return ((int)**gargv++);
}

static const char *
getstr(void)
{
	if (!*gargv)
		return ("");
	return (*gargv++);
}

static int
getint(int *ip)
{
	quad_t val;
	u_quad_t uval;
	int rval;

	if (getquads(&val, &uval, 1))
		return (1);
	rval = 0;
	if (val < INT_MIN || val > INT_MAX) {
		warnx3("%s: %s", *gargv, strerror(ERANGE));
		rval = 1;
	}
	*ip = (int)val;
	return (rval);
}

static int
getquads(quad_t *qp, u_quad_t *uqp, int signedconv)
{
	char *ep;
	int rval;

	if (!*gargv) {
		*qp = 0;
		return (0);
	}
	if (**gargv == '"' || **gargv == '\'') {
		if (signedconv)
			*qp = asciicode();
		else
			*uqp = asciicode();
		return (0);
	}
	rval = 0;
	errno = 0;
	if (signedconv)
		*qp = strtoq(*gargv, &ep, 0);
	else
		*uqp = strtouq(*gargv, &ep, 0);
	if (ep == *gargv) {
		warnx2("%s: expected numeric value", *gargv, NULL);
		rval = 1;
	}
	else if (*ep != '\0') {
		warnx2("%s: not completely converted", *gargv, NULL);
		rval = 1;
	}
	if (errno == ERANGE) {
		warnx3("%s: %s", *gargv, strerror(ERANGE));
		rval = 1;
	}
	++gargv;
	return (rval);
}

static int
getdouble(double *dp)
{
	char *ep;
	int rval;

	if (!*gargv)
		return (0);
	if (**gargv == '"' || **gargv == '\'') {
		*dp = asciicode();
		return (0);
	}
	rval = 0;
	errno = 0;
	*dp = strtod(*gargv, &ep);
	if (ep == *gargv) {
		warnx2("%s: expected numeric value", *gargv, NULL);
		rval = 1;
	} else if (*ep != '\0') {
		warnx2("%s: not completely converted", *gargv, NULL);
		rval = 1;
	}
	if (errno == ERANGE) {
		warnx3("%s: %s", *gargv, strerror(ERANGE));
		rval = 1;
	}
	++gargv;
	return (rval);
}

static int
asciicode(void)
{
	int ch;

	ch = **gargv;
	if (ch == '\'' || ch == '"')
		ch = (*gargv)[1];
	++gargv;
	return (ch);
}

static void
usage(void)
{
	(void)fprintf(stderr, "usage: printf format [arg ...]\n");
}
