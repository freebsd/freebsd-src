/*
 * Copyright (c) 1980, 1993
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
"@(#) Copyright (c) 1980, 1993\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif /* not lint */

#ifndef lint
#if 0
static char sccsid[] = "@(#)whois.c	8.1 (Berkeley) 6/6/93";
#endif
static const char rcsid[] =
	"$Id: whois.c,v 1.5 1998/02/19 19:07:50 wollman Exp $";
#endif /* not lint */

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <err.h>
#include <netdb.h>
#include <stdio.h>
#include <string.h>
#include <sysexits.h>
#include <unistd.h>

#define	NICHOST		"whois.internic.net"
#define	DNICHOST	"nic.ddn.mil"
#define	ANICHOST	"whois.arin.net"
#define	RNICHOST	"whois.ripe.net"
#define	PNICHOST	"whois.apnic.net"
#define	WHOIS_PORT	43

static void usage __P((void));

int
main(argc, argv)
	int argc;
	char **argv;
{
	register FILE *sfi, *sfo;
	register int ch;
	struct sockaddr_in sin;
	struct hostent *hp;
	struct servent *sp;
	int s;
	char *host;

#ifdef	SOCKS
	SOCKSinit(argv[0]);
#endif

	host = NICHOST;
	while ((ch = getopt(argc, argv, "adh:pr")) != -1)
		switch((char)ch) {
		case 'a':
			host = ANICHOST;
			break;
		case 'd':
			host = DNICHOST;
			break;
		case 'h':
			host = optarg;
			break;
		case 'p':
			host = PNICHOST;
			break;
		case 'r':
			host = RNICHOST;
			break;
		case '?':
		default:
			usage();
		}
	argc -= optind;
	argv += optind;

	if (!argc)
		usage();

	s = socket(PF_INET, SOCK_STREAM, 0);
	if (s < 0)
		err(EX_OSERR, "socket");

	memset(&sin, 0, sizeof sin);
	sin.sin_len = sizeof sin;
	sin.sin_family = AF_INET;
	
	if (inet_aton(host, &sin.sin_addr) == 0) {
		hp = gethostbyname2(host, AF_INET);
		if (hp == NULL)
			errx(EX_NOHOST, "%s: %s", host, hstrerror(h_errno));
		host = hp->h_name;
		sin.sin_addr = *(struct in_addr *)hp->h_addr_list[0];
	}

	sp = getservbyname("whois", "tcp");
	if (sp == NULL)
		sin.sin_port = htons(WHOIS_PORT);
	else
		sin.sin_port = sp->s_port;

	if (connect(s, (struct sockaddr *)&sin, sizeof(sin)) < 0)
		err(EX_OSERR, "connect");

	sfi = fdopen(s, "r");
	sfo = fdopen(s, "w");
	if (sfi == NULL || sfo == NULL)
		err(EX_OSERR, "fdopen");
	while (argc-- > 1)
		(void)fprintf(sfo, "%s ", *argv++);
	(void)fprintf(sfo, "%s\r\n", *argv);
	(void)fflush(sfo);
	while ((ch = getc(sfi)) != EOF)
		putchar(ch);
	exit(0);
}

static void
usage()
{
	(void)fprintf(stderr, "usage: whois [-adpr] [-h hostname] name ...\n");
	exit(EX_USAGE);
}
