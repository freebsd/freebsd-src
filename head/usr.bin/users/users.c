/*
 * Copyright (c) 1980, 1987, 1993
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
static const char copyright[] =
"@(#) Copyright (c) 1980, 1987, 1993\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif /* not lint */

#ifndef lint
#if 0
static char sccsid[] = "@(#)users.c	8.1 (Berkeley) 6/6/93";
#endif
static const char rcsid[] =
  "$FreeBSD$";
#endif /* not lint */

#include <sys/param.h>
#include <sys/types.h>
#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <utmpx.h>

typedef char   namebuf[MAXLOGNAME];

int scmp(const void *, const void *);
static void usage(void);

int
main(int argc, char **argv)
{
	namebuf *names = NULL;
	int ncnt = 0;
	int nmax = 0;
	int cnt;
	struct utmpx *ut;
	int ch;

	while ((ch = getopt(argc, argv, "")) != -1)
		switch(ch) {
		case '?':
		default:
			usage();
		}
	argc -= optind;
	argv += optind;

	setutxent();
	while ((ut = getutxent()) != NULL) {
		if (ut->ut_type != USER_PROCESS)
			continue;
		if (ncnt >= nmax) {
			nmax += 32;
			names = realloc(names, sizeof(*names) * nmax);
			if (!names) {
				errx(1, "realloc");
				/* NOTREACHED */
			}
		}
		(void)strlcpy(names[ncnt], ut->ut_user, sizeof(*names));
		++ncnt;
	}
	endutxent();
	if (ncnt > 0) {
		qsort(names, ncnt, sizeof(namebuf), scmp);
		(void)printf("%s", names[0]);
		for (cnt = 1; cnt < ncnt; ++cnt)
			if (strcmp(names[cnt], names[cnt - 1]) != 0)
				(void)printf(" %s", names[cnt]);
		(void)printf("\n");
	}
	exit(0);
}

static void
usage(void)
{
	(void)fprintf(stderr, "usage: users\n");
	exit(1);
}
	
int
scmp(const void *p, const void *q)
{

	return (strcmp(p, q));
}
