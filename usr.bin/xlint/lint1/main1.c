/*	$NetBSD: main1.c,v 1.3 1995/10/02 17:29:56 jpo Exp $	*/

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

#ifndef lint
static char rcsid[] = "$NetBSD: main1.c,v 1.3 1995/10/02 17:29:56 jpo Exp $";
#endif

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <err.h>

#include "lint1.h"

/* set yydebug to 1*/
int	yflag;

/*
 * Print warnings if an assignment of an integertype to another integertype
 * causes an implizit narrowing conversion. If aflag is 1, these warnings
 * are printed only if the source type is at least as wide as long. If aflag
 * is greather then 1, they are always printed.
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

static	void	usage __P((void));

int
main(argc, argv)
	int	argc;
	char	*argv[];
{
	int	c;

	while ((c = getopt(argc, argv, "abcdeghprstuvyzF")) != -1) {
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
		case 'v':	vflag = 0;	break;
		case 'y':	yflag = 1;	break;
		case 'z':	zflag = 0;	break;
		case '?':	usage();
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
usage()
{
	(void)fprintf(stderr, "usage: lint1 [-abcdeghprstuvyzF] src dest\n");
	exit(1);
}
	
void
norecover()
{
	/* cannot recover from previous errors */
	error(224);
	exit(1);
}
