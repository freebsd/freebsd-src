/*
 * Copyright (c) 1983, 1993
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
#if 0
static char sccsid[] = "@(#)cu.c	8.1 (Berkeley) 6/6/93";
#endif
static const char rcsid[] =
  "$FreeBSD: src/usr.bin/tip/tip/cu.c,v 1.4 1999/08/28 01:06:33 peter Exp $";
#endif /* not lint */

#include "tipconf.h"
#include "tip.h"

void	cleanup();

#if INCLUDE_CU_INTERFACE

/*
 * Botch the interface to look like cu's
 */
cumain(argc, argv)
	char *argv[];
{
	register int i;
	static char sbuf[12];

	if (argc < 2) {
		printf("usage: cu telno [-t] [-s speed] [-a acu] [-l line] [-#]\n");
		exit(8);
	}
	CU = DV = NOSTR;
	BR = DEFBR;
	for (; argc > 1; argv++, argc--) {
		if (argv[1][0] != '-')
			PN = argv[1];
		else switch (argv[1][1]) {

		case 't':
			HW = 1, DU = -1;
			--argc;
			continue;

		case 'a':
			CU = argv[2]; ++argv; --argc;
			break;

		case 's':
			if (argc < 3 || speed(atoi(argv[2])) == 0) {
				fprintf(stderr, "cu: unsupported speed %s\n",
					argv[2]);
				exit(3);
			}
			BR = atoi(argv[2]); ++argv; --argc;
			break;

		case 'l':
			DV = argv[2]; ++argv; --argc;
			break;

		case '0': case '1': case '2': case '3': case '4':
		case '5': case '6': case '7': case '8': case '9':
			if (CU)
				CU[strlen(CU)-1] = argv[1][1];
			if (DV)
				DV[strlen(DV)-1] = argv[1][1];
			break;

		default:
			printf("Bad flag %s", argv[1]);
			break;
		}
	}
	signal(SIGINT, cleanup);
	signal(SIGQUIT, cleanup);
	signal(SIGHUP, cleanup);
	signal(SIGTERM, cleanup);

	/*
	 * The "cu" host name is used to define the
	 * attributes of the generic dialer.
	 */
	(void)snprintf(sbuf, sizeof(sbuf), "cu%d", BR);
	if ((i = hunt(sbuf)) == 0) {
		printf("all ports busy\n");
		exit(3);
	}
	if (i == -1) {
		printf("link down\n");
		(void)uu_unlock(uucplock);
		exit(3);
	}
	setbuf(stdout, NULL);
	loginit();
	user_uid();
	vinit();
	setparity("none");
	boolean(value(VERBOSE)) = 0;
	if (HW)
		ttysetup(speed(BR));
	if (connect()) {
		printf("Connect failed\n");
		daemon_uid();
		(void)uu_unlock(uucplock);
		exit(1);
	}
	if (!HW)
		ttysetup(speed(BR));
}
#endif /* INCLUDE_CU_INTERFACE */
