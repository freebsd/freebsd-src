/*-
 * SPDX-License-Identifier: BSD-3-Clause
 *
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

#include <sys/capsicum.h>

#include <capsicum_helpers.h>
#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <getopt.h>
#include <limits.h>
#include <locale.h>
#include <nl_types.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>
#include <wchar.h>
#include <wctype.h>

static enum { DF_NONE, DF_NOSEP, DF_PRESEP, DF_POSTSEP } Dflag;
static bool cflag, dflag, uflag, iflag;
static long long numchars, numfields, repeats;

static const struct option long_opts[] =
{
	{"all-repeated",optional_argument,	NULL, 'D'},
	{"count",	no_argument,		NULL, 'c'},
	{"repeated",	no_argument,		NULL, 'd'},
	{"skip-fields",	required_argument,	NULL, 'f'},
	{"ignore-case",	no_argument,		NULL, 'i'},
	{"skip-chars",	required_argument,	NULL, 's'},
	{"unique",	no_argument,		NULL, 'u'},
	{NULL,		no_argument,		NULL, 0}
};

static FILE	*file(const char *, const char *);
static wchar_t	*convert(const char *);
static int	 inlcmp(const char *, const char *);
static void	 show(FILE *, const char *);
static wchar_t	*skip(wchar_t *);
static void	 obsolete(char *[]);
static void	 usage(void);

int
main (int argc, char *argv[])
{
	wchar_t *tprev, *tthis;
	FILE *ifp, *ofp;
	int ch, comp;
	size_t prevbuflen, thisbuflen, b1;
	char *prevline, *thisline, *p;
	const char *errstr, *ifn, *ofn;
	cap_rights_t rights;

	(void) setlocale(LC_ALL, "");

	obsolete(argv);
	while ((ch = getopt_long(argc, argv, "+D::cdif:s:u", long_opts,
	    NULL)) != -1)
		switch (ch) {
		case 'D':
			if (optarg == NULL || strcasecmp(optarg, "none") == 0)
				Dflag = DF_NOSEP;
			else if (strcasecmp(optarg, "prepend") == 0)
				Dflag = DF_PRESEP;
			else if (strcasecmp(optarg, "separate") == 0)
				Dflag = DF_POSTSEP;
			else
				usage();
			break;
		case 'c':
			cflag = true;
			break;
		case 'd':
			dflag = true;
			break;
		case 'i':
			iflag = true;
			break;
		case 'f':
			numfields = strtonum(optarg, 0, INT_MAX, &errstr);
			if (errstr)
				errx(1, "field skip value is %s: %s", errstr, optarg);
			break;
		case 's':
			numchars = strtonum(optarg, 0, INT_MAX, &errstr);
			if (errstr != NULL)
				errx(1, "character skip value is %s: %s", errstr, optarg);
			break;
		case 'u':
			uflag = true;
			break;
		case '?':
		default:
			usage();
		}

	argc -= optind;
	argv += optind;

	if (argc > 2)
		usage();

	if (Dflag && dflag)
		dflag = false;

	ifp = stdin;
	ifn = "stdin";
	ofp = stdout;
	ofn = "stdout";
	if (argc > 0 && strcmp(argv[0], "-") != 0)
		ifp = file(ifn = argv[0], "r");
	cap_rights_init(&rights, CAP_FSTAT, CAP_READ);
	if (caph_rights_limit(fileno(ifp), &rights) < 0)
		err(1, "unable to limit rights for %s", ifn);
	cap_rights_init(&rights, CAP_FSTAT, CAP_WRITE);
	if (argc > 1)
		ofp = file(ofn = argv[1], "w");
	else
		cap_rights_set(&rights, CAP_IOCTL);
	if (caph_rights_limit(fileno(ofp), &rights) < 0) {
		err(1, "unable to limit rights for %s",
		    argc > 1 ? argv[1] : "stdout");
	}
	if (cap_rights_is_set(&rights, CAP_IOCTL)) {
		unsigned long cmd;

		cmd = TIOCGETA; /* required by isatty(3) in printf(3) */

		if (caph_ioctls_limit(fileno(ofp), &cmd, 1) < 0) {
			err(1, "unable to limit ioctls for %s",
			    argc > 1 ? argv[1] : "stdout");
		}
	}

	caph_cache_catpages();
	if (caph_enter() < 0)
		err(1, "unable to enter capability mode");

	prevbuflen = thisbuflen = 0;
	prevline = thisline = NULL;

	if (getline(&prevline, &prevbuflen, ifp) < 0) {
		if (ferror(ifp))
			err(1, "%s", ifn);
		exit(0);
	}
	if (!cflag && !Dflag && !dflag && !uflag)
		show(ofp, prevline);
	tprev = convert(prevline);

	tthis = NULL;
	while (getline(&thisline, &thisbuflen, ifp) >= 0) {
		if (tthis != NULL)
			free(tthis);
		tthis = convert(thisline);

		if (tthis == NULL && tprev == NULL)
			comp = inlcmp(thisline, prevline);
		else if (tthis == NULL || tprev == NULL)
			comp = 1;
		else
			comp = wcscoll(tthis, tprev);

		if (comp) {
			/* If different, print; set previous to new value. */
			if (Dflag == DF_POSTSEP && repeats > 0)
				fputc('\n', ofp);
			if (!cflag && !Dflag && !dflag && !uflag)
				show(ofp, thisline);
			else if (!Dflag &&
			    (!dflag || (cflag && repeats > 0)) &&
			    (!uflag || repeats == 0))
				show(ofp, prevline);
			p = prevline;
			b1 = prevbuflen;
			prevline = thisline;
			prevbuflen = thisbuflen;
			if (tprev != NULL)
				free(tprev);
			tprev = tthis;
			thisline = p;
			thisbuflen = b1;
			tthis = NULL;
			repeats = 0;
		} else {
			if (Dflag) {
				if (repeats == 0) {
					if (Dflag == DF_PRESEP)
						fputc('\n', ofp);
					show(ofp, prevline);
				}
			} else if (dflag && !cflag) {
				if (repeats == 0)
					show(ofp, prevline);
			}
			++repeats;
			if (Dflag)
				show(ofp, thisline);
		}
	}
	if (ferror(ifp))
		err(1, "%s", ifn);
	if (!cflag && !Dflag && !dflag && !uflag)
		/* already printed */ ;
	else if (!Dflag &&
	    (!dflag || (cflag && repeats > 0)) &&
	    (!uflag || repeats == 0))
		show(ofp, prevline);
	if (fflush(ofp) != 0)
		err(1, "%s", ofn);
	exit(0);
}

static wchar_t *
convert(const char *str)
{
	size_t n;
	wchar_t *buf, *ret, *p;

	if ((n = mbstowcs(NULL, str, 0)) == (size_t)-1)
		return (NULL);
	if (SIZE_MAX / sizeof(*buf) < n + 1)
		errx(1, "conversion buffer length overflow");
	if ((buf = malloc((n + 1) * sizeof(*buf))) == NULL)
		err(1, "malloc");
	if (mbstowcs(buf, str, n + 1) != n)
		errx(1, "internal mbstowcs() error");
	/* The last line may not end with \n. */
	if (n > 0 && buf[n - 1] == L'\n')
		buf[n - 1] = L'\0';

	/* If requested get the chosen fields + character offsets. */
	if (numfields || numchars) {
		if ((ret = wcsdup(skip(buf))) == NULL)
			err(1, "wcsdup");
		free(buf);
	} else
		ret = buf;

	if (iflag) {
		for (p = ret; *p != L'\0'; p++)
			*p = towlower(*p);
	}

	return (ret);
}

static int
inlcmp(const char *s1, const char *s2)
{
	int c1, c2;

	while (*s1 == *s2++)
		if (*s1++ == '\0')
			return (0);
	c1 = (unsigned char)*s1;
	c2 = (unsigned char)*(s2 - 1);
	/* The last line may not end with \n. */
	if (c1 == '\n')
		c1 = '\0';
	if (c2 == '\n')
		c2 = '\0';
	return (c1 - c2);
}

/*
 * show --
 *	Output a line depending on the flags and number of repetitions
 *	of the line.
 */
static void
show(FILE *ofp, const char *str)
{
	if (cflag)
		(void)fprintf(ofp, "%4lld %s", repeats + 1, str);
	else
		(void)fprintf(ofp, "%s", str);
}

static wchar_t *
skip(wchar_t *str)
{
	long long nchars, nfields;

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

static FILE *
file(const char *name, const char *mode)
{
	FILE *fp;

	if ((fp = fopen(name, mode)) == NULL)
		err(1, "%s", name);
	return(fp);
}

static void
obsolete(char *argv[])
{
	char *ap, *p;

	while ((ap = *++argv)) {
		/* Return if "--" or not an option of any form. */
		if (ap[0] != '-') {
			if (ap[0] != '+')
				return;
		} else if (ap[1] == '-') {
			return;
		}
		if (!isdigit((unsigned char)ap[1]))
			continue;
		/*
		 * Digit signifies an old-style option.  Malloc space for dash,
		 * new option and argument.
		 */
		if (asprintf(&p, "-%c%s", ap[0] == '+' ? 's' : 'f', ap + 1) < 0)
			err(1, "malloc");
		*argv = p;
	}
}

static void
usage(void)
{
	(void)fprintf(stderr, "usage: uniq [-cdiu] [-D[septype]] "
	    "[-f fields] [-s chars] [input [output]]\n");
	exit(1);
}
