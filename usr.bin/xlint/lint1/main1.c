/*	$NetBSD: main1.c,v 1.11 2002/01/29 02:43:38 tv Exp $	*/

/*
 * Copyright (c) 1994, 1995 Jochen Pohl
 * All Rights Reserved.
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
 *      This product includes software developed by Jochen Pohl for
 *	The NetBSD Project.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
#if defined(__RCSID) && !defined(lint)
__RCSID("$NetBSD: main1.c,v 1.11 2002/01/29 02:43:38 tv Exp $");
#endif

#include <sys/types.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <limits.h>

#include "lint1.h"

/* set yydebug to 1*/
int	yflag;

/*
 * Print warnings if an assignment of an integertype to another integertype
 * causes an implizit narrowing conversion. If aflag is 1, these warnings
 * are printed only if the source type is at least as wide as long. If aflag
 * is greater than 1, they are always printed.
 */
int	aflag;

/* Print a warning if a break statement cannot be reached. */
int	bflag;

/* Print warnings for pointer casts. */
int	cflag;

/* Print various debug information. */
int	dflag;

/* Perform stricter checking of enum types and operations on enum types. */
int	eflag;

/* Print complete pathnames, not only the basename. */
int	Fflag;

/* Enable some extensions of gcc */
int	gflag;

/* Treat warnings as errors */
int	wflag;

/*
 * Apply a number of heuristic tests to attempt to intuit bugs, improve
 * style, and reduce waste.
 */
int	hflag;

/* Attempt to check portability to other dialects of C. */
int	pflag;

/*
 * In case of redeclarations/redefinitions print the location of the
 * previous declaration/definition.
 */
int	rflag;

/* Strict ANSI C mode. */
int	sflag;

/* Traditional C mode. */
int	tflag;

/*
 * Complain about functions and external variables used and not defined,
 * or defined and not used.
 */
int	uflag = 1;

/* Complain about unused function arguments. */
int	vflag = 1;

/* Complain about structures which are never defined. */
int	zflag = 1;

err_set	msgset;

static	void	usage(void);

int main(int, char *[]);

int
main(int argc, char *argv[])
{
	int	c;
	char	*ptr;

	setprogname(argv[0]);

	ERR_ZERO(&msgset);
	while ((c = getopt(argc, argv, "abcdeghmprstuvwyzFX:")) != -1) {
		switch (c) {
		case 'a':	aflag++;	break;
		case 'b':	bflag = 1;	break;
		case 'c':	cflag = 1;	break;
		case 'd':	dflag = 1;	break;
		case 'e':	eflag = 1;	break;
		case 'F':	Fflag = 1;	break;
		case 'g':	gflag = 1;	break;
		case 'h':	hflag = 1;	break;
		case 'p':	pflag = 1;	break;
		case 'r':	rflag = 1;	break;
		case 's':	sflag = 1;	break;
		case 't':	tflag = 1;	break;
		case 'u':	uflag = 0;	break;
		case 'w':	wflag = 1;	break;
		case 'v':	vflag = 0;	break;
		case 'y':	yflag = 1;	break;
		case 'z':	zflag = 0;	break;

		case 'm':
			msglist();
			return(0);

		case 'X':
			for (ptr = strtok(optarg, ","); ptr;
			    ptr = strtok(NULL, ",")) {
				char *eptr;
				long msg = strtol(ptr, &eptr, 0);
				if ((msg == LONG_MIN || msg == LONG_MAX) &&
				    errno == ERANGE)
				    err(1, "invalid error message id '%s'",
					ptr);
				if (*eptr || ptr == eptr || msg < 0 ||
				    msg >= ERR_SETSIZE)
					errx(1, "invalid error message id '%s'",
					    ptr);
				ERR_SET(msg, &msgset);
			}
			break;
		case '?':
		default:
			usage();
			break;
		}
	}
	argc -= optind;
	argv += optind;

	if (argc != 2)
		usage();

	/* open the input file */
	if ((yyin = fopen(argv[0], "r")) == NULL)
		err(1, "cannot open '%s'", argv[0]);

	/* initialize output */
	outopen(argv[1]);

	if (yflag)
		yydebug = 1;

	initmem();
	initdecl();
	initscan();
	initmtab();

	yyparse();

	/* Following warnings cannot be suppressed by LINTED */
	nowarn = 0;

	chkglsyms();

	outclose();

	return (nerr != 0);
}

static void
usage(void)
{
	(void)fprintf(stderr,
	    "Usage: %s [-abcdeghmprstuvwyzF] [-X <id>[,<id>]... src dest\n",
	    getprogname());
	exit(1);
}

void
norecover(void)
{
	/* cannot recover from previous errors */
	error(224);
	exit(1);
}
