/*
 * Copyright (c) 1980, 1987 Regents of the University of California.
 * All rights reserved.
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
char copyright[] =
"@(#) Copyright (c) 1980, 1987 Regents of the University of California.\n\
 All rights reserved.\n";
#endif /* not lint */

#ifndef lint
static char sccsid[] = "@(#)users.c	5.12 (Berkeley) 11/1/90";
#endif /* not lint */

#include <sys/types.h>
#include <utmp.h>
#include <stdio.h>

#define	MAXUSERS	200

main(argc, argv)
	int argc;
	char **argv;
{
	extern int optind;
	register int cnt, ncnt;
	struct utmp utmp;
	char names[MAXUSERS][UT_NAMESIZE];
	int ch, scmp();

	while ((ch = getopt(argc, argv, "")) != EOF)
		switch(ch) {
		case '?':
		default:
			(void)fprintf(stderr, "usage: users\n");
			exit(1);
		}
	argc -= optind;
	argv += optind;

	if (!freopen(_PATH_UTMP, "r", stdin)) {
		(void)fprintf(stderr, "users: can't open %s.\n", _PATH_UTMP);
		exit(1);
	}
	for (ncnt = 0;
	    fread((char *)&utmp, sizeof(utmp), 1, stdin) == 1;)
		if (*utmp.ut_name) {
			if (ncnt == MAXUSERS) {
				(void)fprintf(stderr,
				    "users: too many users.\n");
				break;
			}
			(void)strncpy(names[ncnt], utmp.ut_name, UT_NAMESIZE);
			++ncnt;
		}

	if (ncnt) {
		qsort(names, ncnt, UT_NAMESIZE, scmp);
		(void)printf("%.*s", UT_NAMESIZE, names[0]);
		for (cnt = 1; cnt < ncnt; ++cnt)
			if (strncmp(names[cnt], names[cnt - 1], UT_NAMESIZE))
				(void)printf(" %.*s", UT_NAMESIZE, names[cnt]);
		(void)printf("\n");
	}
	exit(0);
}

scmp(p, q)
	char *p, *q;
{
	return(strncmp(p, q, UT_NAMESIZE));
}
