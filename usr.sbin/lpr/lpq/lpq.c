/*
 * Copyright (c) 1983 Regents of the University of California.
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
"@(#) Copyright (c) 1983 Regents of the University of California.\n\
 All rights reserved.\n";
#endif /* not lint */

#ifndef lint
static char sccsid[] = "@(#)lpq.c	5.7 (Berkeley) 3/2/91";
#endif /* not lint */

/*
 * Spool Queue examination program
 *
 * lpq [-l] [-Pprinter] [user...] [job...]
 *
 * -l long output
 * -P used to identify printer as per lpr/lprm
 */

#include "lp.h"

char	*user[MAXUSERS];	/* users to process */
int	users;			/* # of users in user array */
int	requ[MAXREQUESTS];	/* job number of spool entries */
int	requests;		/* # of spool requests */

main(argc, argv)
	register int	argc;
	register char	**argv;
{
	extern char	*optarg;
	extern int	optind;
	int	ch, lflag;		/* long output option */

	name = *argv;
	if (gethostname(host, sizeof(host))) {
		perror("lpq: gethostname");
		exit(1);
	}
	openlog("lpd", 0, LOG_LPR);

	lflag = 0;
	while ((ch = getopt(argc, argv, "lP:")) != EOF)
		switch((char)ch) {
		case 'l':			/* long output */
			++lflag;
			break;
		case 'P':		/* printer name */
			printer = optarg;
			break;
		case '?':
		default:
			usage();
		}

	if (printer == NULL && (printer = getenv("PRINTER")) == NULL)
		printer = DEFLP;

	for (argc -= optind, argv += optind; argc; --argc, ++argv)
		if (isdigit(argv[0][0])) {
			if (requests >= MAXREQUESTS)
				fatal("too many requests");
			requ[requests++] = atoi(*argv);
		}
		else {
			if (users >= MAXUSERS)
				fatal("too many users");
			user[users++] = *argv;
		}

	displayq(lflag);
	exit(0);
}

usage()
{
	puts("usage: lpq [-l] [-Pprinter] [user ...] [job ...]");
	exit(1);
}
