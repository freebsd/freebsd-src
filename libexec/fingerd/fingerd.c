/*
 * Copyright (c) 1983 The Regents of the University of California.
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
"@(#) Copyright (c) 1983 The Regents of the University of California.\n\
 All rights reserved.\n";
#endif /* not lint */

#ifndef lint
static char sccsid[] = "@(#)fingerd.c	5.6 (Berkeley) 6/1/90";
#endif /* not lint */

#include <stdio.h>
#include "pathnames.h"

main()
{
	register FILE *fp;
	register int ch;
	register char *lp;
	int p[2];
#define	ENTRIES	50
	char **ap, *av[ENTRIES + 1], line[1024], *strtok();

#ifdef LOGGING					/* unused for now */
#include <netinet/in.h>
	struct sockaddr_in sin;
	int sval;

	sval = sizeof(sin);
	if (getpeername(0, &sin, &sval) < 0)
		fatal("getpeername");
#endif

	if (!fgets(line, sizeof(line), stdin))
		exit(1);

	av[0] = "finger";
	for (lp = line, ap = &av[1];;) {
		*ap = strtok(lp, " \t\r\n");
		if (!*ap)
			break;
		/* RFC742: "/[Ww]" == "-l" */
		if ((*ap)[0] == '/' && ((*ap)[1] == 'W' || (*ap)[1] == 'w'))
			*ap = "-l";
		if (++ap == av + ENTRIES)
			break;
		lp = NULL;
	}

	if (pipe(p) < 0)
		fatal("pipe");

	switch(fork()) {
	case 0:
		(void)close(p[0]);
		if (p[1] != 1) {
			(void)dup2(p[1], 1);
			(void)close(p[1]);
		}
		execv(_PATH_FINGER, av);
		_exit(1);
	case -1:
		fatal("fork");
	}
	(void)close(p[1]);
	if (!(fp = fdopen(p[0], "r")))
		fatal("fdopen");
	while ((ch = getc(fp)) != EOF) {
		if (ch == '\n')
			putchar('\r');
		putchar(ch);
	}
	exit(0);
}

fatal(msg)
	char *msg;
{
	extern int errno;
	char *strerror();

	fprintf(stderr, "fingerd: %s: %s\r\n", msg, strerror(errno));
	exit(1);
}
