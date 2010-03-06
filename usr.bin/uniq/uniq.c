/*
 * Copyright (c) 1989, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Case Larsen.
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
static const char copyright[] =
"@(#) Copyright (c) 1989, 1993\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif /* not lint */

#ifndef lint
#if 0
static char sccsid[] = "@(#)uniq.c	8.3 (Berkeley) 5/4/95";
#endif
static const char rcsid[] =
  "$FreeBSD$";
#endif /* not lint */

#include <ctype.h>
#include <err.h>
#include <limits.h>
#include <locale.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <wchar.h>
#include <wctype.h>

int cflag, dflag, uflag, iflag;
int numchars, numfields, repeats;

FILE	*file(const char *, const char *);
wchar_t	*convert(wchar_t *, const char *);
char	*getlinemax(char *, FILE *);
void	 show(FILE *, const char *);
wchar_t	*skip(wchar_t *);
void	 obsolete(char *[]);
static void	 usage(void);

int
main (int argc, char *argv[])
{
	wchar_t *tprev, *tthis, *wprev, *wthis, *wp;
	FILE *ifp, *ofp;
	int ch, comp;
	char *prevline, *thisline, *p;
	const char *ifn;

	(void) setlocale(LC_ALL, "");

	obsolete(argv);
	while ((ch = getopt(argc, argv, "cdif:s:u")) != -1)
		switch (ch) {
		case 'c':
			cflag = 1;
			break;
		case 'd':
			dflag = 1;
			break;
		case 'i':
			iflag = 1;
			break;
		case 'f':
			numfields = strtol(optarg, &p, 10);
			if (numfields < 0 || *p)
				errx(1, "illegal field skip value: %s", optarg);
			break;
		case 's':
			numchars = strtol(optarg, &p, 10);
			if (numchars < 0 || *p)
				errx(1, "illegal character skip value: %s", optarg);
			break;
		case 'u':
			uflag = 1;
			break;
		case '?':
		default:
			usage();
		}

	argc -= optind;
	argv += optind;

	/* If no flags are set, default is -d -u. */
	if (cflag) {
		if (dflag || uflag)
			usage();
	} else if (!dflag && !uflag)
		dflag = uflag = 1;

	if (argc > 2)
		usage();

	ifp = stdin;
	ifn = "stdin";
	ofp = stdout;
	if (argc > 0 && strcmp(argv[0], "-") != 0)
		ifp = file(ifn = argv[0], "r");
	if (argc > 1)
		ofp = file(argv[1], "w");

	prevline = malloc(LINE_MAX);
	thisline = malloc(LINE_MAX);
	wprev = malloc(LINE_MAX * sizeof(*wprev));
	wthis = malloc(LINE_MAX * sizeof(*wthis));
	if (prevline == NULL || thisline == NULL ||
	    wprev == NULL || wthis == NULL)
		err(1, "malloc");

	if ((prevline = getlinemax(prevline, ifp)) == NULL) {
		if (ferror(ifp))
			err(1, "%s", ifn);
		exit(0);
	}
	tprev = convert(wprev, prevline);

	if (!cflag && uflag && dflag)
		show(ofp, prevline);

	while ((thisline = getlinemax(thisline, ifp)) != NULL) {
		tthis = convert(wthis, thisline);

		if (tthis == NULL && tprev == NULL)
			comp = strcmp(thisline, prevline);
		else if (tthis == NULL || tprev == NULL)
			comp = 1;
		else
			comp = wcscoll(tthis, tprev);

		if (comp) {
			/* If different, print; set previous to new value. */
			if (cflag || !dflag || !uflag)
				show(ofp, prevline);
			p = prevline;
			wp = wprev;
			prevline = thisline;
			wprev = wthis;
			tprev = tthis;
			if (!cflag && uflag && dflag)
				show(ofp, prevline);
			thisline = p;
			wthis = wp;
			repeats = 0;
		} else
			++repeats;
	}
	if (ferror(ifp))
		err(1, "%s", ifn);
	if (cflag || !dflag || !uflag)
		show(ofp, prevline);
	exit(0);
}

char *
getlinemax(char *buf, FILE *fp)
{
	size_t bufpos;
	int ch;

	bufpos = 0;
	while ((ch = getc(fp)) != EOF && ch != '\n') {
		buf[bufpos++] = ch;
		if (bufpos >= LINE_MAX)
			errx(1, "Maximum line length (%d) exceeded",
			     LINE_MAX);
	}
	buf[bufpos] = '\0';

	return (bufpos != 0 || ch == '\n' ? buf : NULL);
}

wchar_t *
convert(wchar_t *buf, const char *str)
{
	size_t n;
	wchar_t *p, *ret;

	if ((n = mbstowcs(buf, str, LINE_MAX)) == LINE_MAX)
		errx(1, "Maximum line length (%d) exceeded", LINE_MAX);
	else if (n != (size_t)-1) {
		/* If requested get the chosen fields + character offsets. */
		if (numfields || numchars)
			ret = skip(buf);
		else
			ret = buf;
		if (iflag) {
			for (p = ret; *p != L'\0'; p++)
				*p = towlower(*p);
		}
	} else
		ret = NULL;

	return (ret);
}

/*
 * show --
 *	Output a line depending on the flags and number of repetitions
 *	of the line.
 */
void
show(FILE *ofp, const char *str)
{

	if (cflag)
		(void)fprintf(ofp, "%4d %s\n", repeats + 1, str);
	if ((dflag && repeats) || (uflag && !repeats))
		(void)fprintf(ofp, "%s\n", str);
}

wchar_t *
skip(wchar_t *str)
{
	int nchars, nfields;

	for (nfields = 0; *str != L'\0' && nfields++ != numfields; ) {
		while (iswblank(*str))
			str++;
		while (*str != L'\0' && !iswblank(*str))
			str++;
	}
	for (nchars = numchars; nchars-- && *str != L'\0'; ++str)
		;
	return(str);
}

FILE *
file(const char *name, const char *mode)
{
	FILE *fp;

	if ((fp = fopen(name, mode)) == NULL)
		err(1, "%s", name);
	return(fp);
}

void
obsolete(char *argv[])
{
	int len;
	char *ap, *p, *start;

	while ((ap = *++argv)) {
		/* Return if "--" or not an option of any form. */
		if (ap[0] != '-') {
			if (ap[0] != '+')
				return;
		} else if (ap[1] == '-')
			return;
		if (!isdigit((unsigned char)ap[1]))
			continue;
		/*
		 * Digit signifies an old-style option.  Malloc space for dash,
		 * new option and argument.
		 */
		len = strlen(ap);
		if ((start = p = malloc(len + 3)) == NULL)
			err(1, "malloc");
		*p++ = '-';
		*p++ = ap[0] == '+' ? 's' : 'f';
		(void)strcpy(p, ap + 1);
		*argv = start;
	}
}

static void
usage(void)
{
	(void)fprintf(stderr,
"usage: uniq [-c | -d | -u] [-i] [-f fields] [-s chars] [input [output]]\n");
	exit(1);
}
