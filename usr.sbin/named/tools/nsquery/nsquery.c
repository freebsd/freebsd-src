/*
 * Copyright (c) 1986 Regents of the University of California.
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
"@(#) Copyright (c) 1986 Regents of the University of California.\n\
 All rights reserved.\n";
#endif /* not lint */

#ifndef lint
static char sccsid[] = "@(#)nsquery.c	4.8 (Berkeley) 6/1/90";
#endif /* not lint */

#include <sys/param.h>
#include <arpa/nameser.h>
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <resolv.h>
#include <stdio.h>

main(argc, argv)
	int argc;
	char **argv;
{
	extern struct state _res;
	register struct hostent *hp;
	register char *s;

	if (argc >= 2 && strcmp(argv[1], "-d") == 0) {
		_res.options |= RES_DEBUG;
		argc--;
		argv++;
	}
	if (argc < 2) {
		fprintf(stderr, "usage: nsquery [-d] host [server]\n");
		exit(1);
	}
	if (argc == 3) {
		hp = gethostbyname(argv[2]);
		if (hp == NULL) {
			fprintf(stderr, "nsquery:");
			herror(argv[2]);
			exit(1);
		}
		printf("\nServer:\n");
		printanswer(hp);
		_res.nsaddr.sin_addr = *(struct in_addr *)hp->h_addr;
	}

	hp = gethostbyname(argv[1]);
	if (hp == NULL) {
		fprintf(stderr, "nsquery: %s: ", argv[1]);
		herror((char *)NULL);
		exit(1);
	}
	printanswer(hp);
	exit(0);
}

printanswer(hp)
	register struct hostent *hp;
{
	register char **cp;
	extern char *inet_ntoa();

	printf("Name: %s\n", hp->h_name);
#if BSD >= 43 || defined(h_addr)
	printf("Addresses:");
	for (cp = hp->h_addr_list; cp && *cp; cp++)
		printf(" %s", inet_ntoa(*(struct in_addr *)(*cp)));
	printf("\n");
#else
	printf("Address: %s\n", inet_ntoa(*(struct in_addr *)hp->h_addr));
#endif
	printf("Aliases:");
	for (cp = hp->h_aliases; cp && *cp && **cp; cp++)
		printf(" %s", *cp);
	printf("\n\n");
}
