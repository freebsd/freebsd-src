/*
 * Copyright (c) 1989 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Adam S. Moskowitz of Menlo Consulting and Marciano Pitargue.
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
 *
 * PATCHES MAGIC                LEVEL   PATCH THAT GOT US HERE
 * --------------------         -----   ----------------------
 * CURRENT PATCH LEVEL:         1       00142
 * --------------------         -----   ----------------------
 *
 * 20 Apr 93	Simon J Gerraty		cut -f1 outputs a field separator
 *					before the first field.
 */

#ifndef lint
char copyright[] =
"@(#) Copyright (c) 1989 The Regents of the University of California.\n\
 All rights reserved.\n";
#endif /* not lint */

#ifndef lint
static char sccsid[] = "@(#)cut.c	5.4 (Berkeley) 10/30/90";
#endif /* not lint */

#include <limits.h>
#include <stdio.h>
#include <ctype.h>

int	cflag;
char	dchar;
int	dflag;
int	fflag;
int	sflag;

main(argc, argv)
	int argc;
	char **argv;
{
	extern char *optarg;
	extern int errno, optind;
	FILE *fp;
	int ch, (*fcn)(), c_cut(), f_cut();
	char *strerror();

	dchar = '\t';			/* default delimiter is \t */

	while ((ch = getopt(argc, argv, "c:d:f:s")) != EOF)
		switch(ch) {
		case 'c':
			fcn = c_cut;
			get_list(optarg);
			cflag = 1;
			break;
		case 'd':
			dchar = *optarg;
			dflag = 1;
			break;
		case 'f':
			get_list(optarg);
			fcn = f_cut;
			fflag = 1;
			break;
		case 's':
			sflag = 1;
			break;
		case '?':
		default:
			usage();
		}
	argc -= optind;
	argv += optind;

	if (fflag) {
		if (cflag)
			usage();
	} else if (!cflag || dflag || sflag)
		usage();

	if (*argv)
		for (; *argv; ++argv) {
			if (!(fp = fopen(*argv, "r"))) {
				(void)fprintf(stderr,
				    "cut: %s: %s\n", *argv, strerror(errno));
				exit(1);
			}
			fcn(fp, *argv);
		}
	else
		fcn(stdin, "stdin");
	exit(0);
}

int autostart, autostop, maxval;

char positions[_POSIX2_LINE_MAX + 1];

get_list(list)
	char *list;
{
	register char *pos;
	register int setautostart, start, stop;
	char *p, *strtok();

	/*
	 * set a byte in the positions array to indicate if a field or
	 * column is to be selected; use +1, it's 1-based, not 0-based.
	 * This parser is less restrictive than the Draft 9 POSIX spec.
	 * POSIX doesn't allow lists that aren't in increasing order or
	 * overlapping lists.  We also handle "-3-5" although there's no
	 * real reason too.
	 */
	for (; p = strtok(list, ", \t"); list = NULL) {
		setautostart = start = stop = 0;
		if (*p == '-') {
			++p;
			setautostart = 1;
		}
		if (isdigit(*p)) {
			start = stop = strtol(p, &p, 10);
			if (setautostart && start > autostart)
				autostart = start;
		}
		if (*p == '-') {
			if (isdigit(p[1]))
				stop = strtol(p + 1, &p, 10);
			if (*p == '-') {
				++p;
				if (!autostop || autostop > stop)
					autostop = stop;
			}
		}
		if (*p)
			badlist("illegal list value");
		if (!stop || !start)
			badlist("values may not include zero");
		if (stop > _POSIX2_LINE_MAX) {
			/* positions used rather than allocate a new buffer */
			(void)sprintf(positions, "%d too large (max %d)",
			    stop, _POSIX2_LINE_MAX);
			badlist(positions);
		}
		if (maxval < stop)
			maxval = stop;
		for (pos = positions + start; start++ <= stop; *pos++ = 1);
	}

	/* overlapping ranges */
	if (autostop && maxval > autostop)
		maxval = autostop;

	/* set autostart */
	if (autostart)
		memset(positions + 1, '1', autostart);
}

/* ARGSUSED */
c_cut(fp, fname)
	FILE *fp;
	char *fname;
{
	register int ch, col;
	register char *pos;

	for (;;) {
		pos = positions + 1;
		for (col = maxval; col; --col) {
			if ((ch = getc(fp)) == EOF)
				return;
			if (ch == '\n')
				break;
			if (*pos++)
				putchar(ch);
		}
		if (ch != '\n')
			if (autostop)
				while ((ch = getc(fp)) != EOF && ch != '\n')
					putchar(ch);
			else
				while ((ch = getc(fp)) != EOF && ch != '\n');
		putchar('\n');
	}
}

f_cut(fp, fname)
	FILE *fp;
	char *fname;
{
	register int ch, field, isdelim;
	register char *pos, *p, sep;
	int output;
	char lbuf[_POSIX2_LINE_MAX + 1];

	for (sep = dchar, output = 0; fgets(lbuf, sizeof(lbuf), fp); output = 0) {
		for (isdelim = 0, p = lbuf;; ++p) {
			if (!(ch = *p)) {
				(void)fprintf(stderr,
				    "cut: %s: line too long.\n", fname);
				exit(1);
			}
			/* this should work if newline is delimiter */
			if (ch == sep)
				isdelim = 1;
			if (ch == '\n') {
				if (!isdelim && !sflag)
					(void)printf("%s", lbuf);
				break;
			}
		}
		if (!isdelim)
			continue;

		pos = positions + 1;
		for (field = maxval, p = lbuf; field; --field, ++pos) {
			if (*pos) {
				if (output++)
					putchar(sep);
				while ((ch = *p++) != '\n' && ch != sep)
					putchar(ch);
			} else
				while ((ch = *p++) != '\n' && ch != sep);
			if (ch == '\n')
				break;
		}
		if (ch != '\n')
			if (autostop) {
				if (output)
					putchar(sep);
				for (; (ch = *p) != '\n'; ++p)
					putchar(ch);
			} else
				for (; (ch = *p) != '\n'; ++p);
		putchar('\n');
	}
}

badlist(msg)
	char *msg;
{
	(void)fprintf(stderr, "cut: [-cf] list: %s.\n", msg);
	exit(1);
}

usage()
{
	(void)fprintf(stderr,
"usage:\tcut -c list [file1 ...]\n\tcut -f list [-s] [-d delim] [file ...]\n");
	exit(1);
}
