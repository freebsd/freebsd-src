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

#ifndef lint
static char copyright[] =
"@(#) Copyright (c) 1989, 1993\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif /* not lint */

#ifndef lint
static char sccsid[] = "@(#)vis.c	8.1 (Berkeley) 6/6/93";
#endif /* not lint */

#include <stdio.h>
#include <vis.h>

int eflags, fold, foldwidth=80, none, markeol, debug;

main(argc, argv) 
	char *argv[];
{
	extern char *optarg;
	extern int optind;
	extern int errno;
	FILE *fp;
	int ch;

	while ((ch = getopt(argc, argv, "nwctsobfF:ld")) != EOF)
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
			if ((foldwidth = atoi(optarg))<5) {
				fprintf(stderr, 
				 "vis: can't fold lines to less than 5 cols\n");
				exit(1);
			}
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
			fprintf(stderr, 
		"usage: vis [-nwctsobf] [-F foldwidth]\n");
			exit(1);
		}
	argc -= optind;
	argv += optind;

	if (*argv)
		while (*argv) {
			if ((fp=fopen(*argv, "r")) != NULL)
				process(fp, *argv);
			else
				fprintf(stderr, "vis: %s: %s\n", *argv,
				    (char *)strerror(errno));
			argv++;
		}
	else
		process(stdin, "<stdin>");
	exit(0);
}
	
process(fp, filename)
	FILE *fp;
	char *filename;
{
	static int col = 0;
	register char *cp = "\0"+1;	/* so *(cp-1) starts out != '\n' */
	register int c, rachar; 
	register char nc;
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
