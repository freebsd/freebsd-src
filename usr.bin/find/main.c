/*-
 * Copyright (c) 1990, 1993
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
static char sccsid[] = "@(#)main.c	8.1 (Berkeley) 6/6/93";
#endif /* not lint */

#include <sys/types.h>
#include <sys/stat.h>

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <fts.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "find.h"

time_t now;			/* time find was run */
int dotfd;			/* starting directory */
int ftsoptions;			/* options for the ftsopen(3) call */
int isdeprecated;		/* using deprecated syntax */
int isdepth;			/* do directories on post-order visit */
int isoutput;			/* user specified output operator */
int isxargs;			/* don't permit xargs delimiting chars */

static void usage __P((void));

int
main(argc, argv)
	int argc;
	char *argv[];
{
	register char **p, **start;
	int ch;

	(void)time(&now);	/* initialize the time-of-day */

	p = start = argv;
	ftsoptions = FTS_NOSTAT|FTS_PHYSICAL;
	while ((ch = getopt(argc, argv, "Hdf:hXx")) != EOF)
		switch(ch) {
		case 'H':
			ftsoptions |= FTS_COMFOLLOW;
			break;
		case 'd':
			isdepth = 1;
			break;
		case 'f':
			*p++ = optarg;
			break;
		case 'h':
			ftsoptions &= ~FTS_PHYSICAL;
			ftsoptions |= FTS_LOGICAL;
			break;
		case 'X':
			isxargs = 1;
			break;
		case 'x':
			ftsoptions |= FTS_XDEV;
			break;
		case '?':
		default:
			break;
		}

	argc -= optind;	
	argv += optind;

	/* Find first option to delimit the file list. */
	while (*argv) {
		if (option(*argv))
			break;
		*p++ = *argv++;
	}

	if (p == start)
		usage();
	*p = NULL;

	if ((dotfd = open(".", O_RDONLY, 0)) < 0)
		err(1, ".:");

	find_execute(find_formplan(argv), start);
	exit(0);
}

static void
usage()
{
	(void)fprintf(stderr,
	    "usage: find [-HdhXx] [-f file] [file ...] expression\n");
	exit(1);
}









