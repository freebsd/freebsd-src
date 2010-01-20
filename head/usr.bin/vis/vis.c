/*-
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

#include <sys/cdefs.h>

__FBSDID("$FreeBSD$");

#ifndef lint
static const char copyright[] =
"@(#) Copyright (c) 1989, 1993\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif

#ifndef lint
static const char sccsid[] = "@(#)vis.c	8.1 (Berkeley) 6/6/93";
#endif

#include <err.h>
#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <vis.h>

#include "extern.h"

int eflags, fold, foldwidth=80, none, markeol, debug;

void process(FILE *);
static void usage(void);

int
main(int argc, char *argv[])
{
	FILE *fp;
	int ch;

	(void) setlocale(LC_CTYPE, "");

	while ((ch = getopt(argc, argv, "nwctsobfF:ld")) != -1)
		switch((char)ch) {
		case 'n':
			none++;
			break;
		case 'w':
			eflags |= VIS_WHITE;
			break;
		case 'c':
			eflags |= VIS_CSTYLE;
			break;
		case 't':
			eflags |= VIS_TAB;
			break;
		case 's':
			eflags |= VIS_SAFE;
			break;
		case 'o':
			eflags |= VIS_OCTAL;
			break;
		case 'b':
			eflags |= VIS_NOSLASH;
			break;
		case 'F':
			if ((foldwidth = atoi(optarg))<5)
				errx(1, "can't fold lines to less than 5 cols");
			/*FALLTHROUGH*/
		case 'f':
			fold++;		/* fold output lines to 80 cols */
			break;		/* using hidden newline */
		case 'l':
			markeol++;	/* mark end of line with \$ */
			break;
#ifdef DEBUG
		case 'd':
			debug++;
			break;
#endif
		case '?':
		default:
			usage();
		}
	argc -= optind;
	argv += optind;

	if (*argv)
		while (*argv) {
			if ((fp=fopen(*argv, "r")) != NULL)
				process(fp);
			else
				warn("%s", *argv);
			argv++;
		}
	else
		process(stdin);
	exit(0);
}


static void
usage(void)
{
#ifdef DEBUG
	fprintf(stderr, "usage: vis [-cbflnostwd] [-F foldwidth] [file ...]\n");
#else
	fprintf(stderr, "usage: vis [-cbflnostw] [-F foldwidth] [file ...]\n");
#endif
	exit(1);
}

void
process(FILE *fp)
{
	static int col = 0;
	static char dummy[] = "\0";
	char *cp = dummy+1; /* so *(cp-1) starts out != '\n' */
	int c, rachar;
	char buff[5];

	c = getc(fp);
	while (c != EOF) {
		rachar = getc(fp);
		if (none) {
			cp = buff;
			*cp++ = c;
			if (c == '\\')
				*cp++ = '\\';
			*cp = '\0';
		} else if (markeol && c == '\n') {
			cp = buff;
			if ((eflags & VIS_NOSLASH) == 0)
				*cp++ = '\\';
			*cp++ = '$';
			*cp++ = '\n';
			*cp = '\0';
		} else
			(void) vis(buff, (char)c, eflags, (char)rachar);

		cp = buff;
		if (fold) {
#ifdef DEBUG
			if (debug)
				printf("<%02d,", col);
#endif
			col = foldit(cp, col, foldwidth);
#ifdef DEBUG
			if (debug)
				printf("%02d>", col);
#endif
		}
		do {
			putchar(*cp);
		} while (*++cp);
		c = rachar;
	}
	/*
	 * terminate partial line with a hidden newline
	 */
	if (fold && *(cp-1) != '\n')
		printf("\\\n");
}
