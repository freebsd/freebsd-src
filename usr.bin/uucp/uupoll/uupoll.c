/*-
 * Copyright (c) 1986, 1991, 1993
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
"@(#) Copyright (c) 1986, 1991, 1993\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif /* not lint */

#ifndef lint
static char sccsid[] = "@(#)uupoll.c	8.1 (Berkeley) 6/6/93";
#endif /* not lint */

/*
 * Poll named system(s).
 *
 * The poll occurs even if recent attempts have failed,
 * but not if L.sys prohibits the call (e.g. wrong time of day).
 *
 * Original Author: Tom Truscott (rti!trt)
 */

#include "uucp.h"

int TransferSucceeded = 1;
struct timeb Now;

main(argc, argv)
int argc;
char **argv;
{
	char wrkpre[MAXFULLNAME];
	char file[MAXFULLNAME];
	char grade = 'A';
	int nocall = 0;
	int c;
	char *sysname;
	extern char *optarg;
	extern int optind;

	if (argc < 2) {
		fprintf(stderr, "usage: uupoll [-gX] [-n] system ...\n");
		cleanup(1);
	}

	if (chdir(Spool) < 0) {
		syslog(LOG_WARNING, "chdir(%s) failed: %m", Spool);
		cleanup(1);
	}
	strcpy(Progname, "uupoll");
	uucpname(Myname);

	while ((c = getopt(argc, argv, "g:n")) != EOF)
		switch(c) {
			case 'g':
				grade = *optarg;
				break;
			case 'n':
				nocall++;
				break;
			case '?':
			default:
				fprintf(stderr, "unknown option %s\n",
					argv[optind-1]);
		}

	while(optind < argc) {
		sysname = argv[optind++];
		if (strcmp(sysname, Myname) == SAME) {
			fprintf(stderr, "This *is* %s!\n", Myname);
			continue;
		}

		if (versys(&sysname)) {
			fprintf(stderr, "%s: unknown system.\n", sysname);
			continue;
		}
		/* Remove any STST file that might stop the poll */
		sprintf(wrkpre, "%s/LCK..%.*s", LOCKDIR, MAXBASENAME, sysname);
		if (access(wrkpre, 0) < 0)
			rmstat(sysname);
		sprintf(wrkpre, "%c.%.*s", CMDPRE, SYSNSIZE, sysname);
		if (!iswrk(file, "chk", Spool, wrkpre)) {
			sprintf(file, "%s/%c.%.*s%cPOLL", subdir(Spool, CMDPRE),
				CMDPRE, SYSNSIZE, sysname, grade);
			close(creat(file, 0666));
		}
		/* Attempt the call */
		if (!nocall)
			xuucico(sysname);
	}
	cleanup(0);
}

cleanup(code)
int code;
{
	exit(code);
}
