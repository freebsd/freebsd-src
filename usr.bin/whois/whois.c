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
  "$FreeBSD$";
#endif /* not lint */

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <err.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sysexits.h>
#include <unistd.h>

#define	NICHOST		"whois.crsnic.net"
#define	INICHOST	"whois.internic.net"
#define	DNICHOST	"whois.nic.mil"
#define	GNICHOST	"whois.nic.gov"
#define	ANICHOST	"whois.arin.net"
#define	RNICHOST	"whois.ripe.net"
#define	PNICHOST	"whois.apnic.net"
#define	RUNICHOST	"whois.ripn.net"
#define	MNICHOST	"whois.ra.net"
#define	QNICHOST_TAIL	".whois-servers.net"
#define	WHOIS_PORT	43

#define WHOIS_RECURSE		0x01
#define WHOIS_INIC_FALLBACK	0x02
#define WHOIS_QUICK		0x04

static void usage __P((void));
static void whois __P((char *, struct sockaddr_in *, int));

int
main(argc, argv)
	int argc;
	char **argv;
{
	int ch, i, j;
	int use_qnichost, flags;
	char *host;
	char *qnichost;
	struct servent *sp;
	struct hostent *hp;
	struct sockaddr_in sin;

#ifdef	SOCKS
	SOCKSinit(argv[0]);
#endif

	host = NULL;
	qnichost = NULL;
	flags = 0;
	use_qnichost = 0;
	while ((ch = getopt(argc, argv, "adgh:impQrR")) != -1)
		switch((char)ch) {
		case 'a':
			host = ANICHOST;
			break;
		case 'd':
			host = DNICHOST;
			break;
		case 'g':
			host = GNICHOST;
			break;
		case 'h':
			host = optarg;
			break;
		case 'i':
			host = INICHOST;
			break;
		case 'm':
			host = MNICHOST;
			break;
		case 'p':
			host = PNICHOST;
			break;
		case 'Q':
			flags |= WHOIS_QUICK;
			break;
		case 'r':
			host = RNICHOST;
			break;
		case 'R':
			host = RUNICHOST;
			break;
		case '?':
		default:
			usage();
		}
	argc -= optind;
	argv += optind;

	if (!argc)
		usage();

	memset(&sin, 0, sizeof(sin));
	sin.sin_len = sizeof(sin);
	sin.sin_family = AF_INET;
	sp = getservbyname("whois", "tcp");
	if (sp == NULL)
		sin.sin_port = htons(WHOIS_PORT);
	else
		sin.sin_port = sp->s_port;

	/*
	 * If no nic host is specified, use whois-servers.net
	 * if there is a '.' in the name, else fall back to NICHOST.
	 */
	if (host == NULL) {
		use_qnichost = 1;
		host = NICHOST;
		if (!(flags & WHOIS_QUICK))
			flags |= WHOIS_INIC_FALLBACK | WHOIS_RECURSE;
	}
	while (argc--) {
		if (use_qnichost) {
			if (qnichost) {
				free(qnichost);
				qnichost = NULL;
			}
			for (i = j = 0; (*argv)[i]; i++)
				if ((*argv)[i] == '.')
					j = i;
			if (j != 0) {
				qnichost = (char *) calloc(i - j + 1 +
				    strlen(QNICHOST_TAIL), sizeof(char));
				if (!qnichost)
					err(1, "calloc");
				strcpy(qnichost, *argv + j + 1);
				strcat(qnichost, QNICHOST_TAIL);

				if (inet_aton(qnichost, &sin.sin_addr) == 0) {
					hp = gethostbyname2(qnichost, AF_INET);
					if (hp == NULL) {
						free(qnichost);
						qnichost = NULL;
					} else {
						sin.sin_addr = *(struct in_addr *)hp->h_addr_list[0];
					}
				}
			}
		}
		if (!qnichost && inet_aton(host, &sin.sin_addr) == 0) {
			hp = gethostbyname2(host, AF_INET);
			if (hp == NULL)
				errx(EX_NOHOST, "%s: %s", host,
				    hstrerror(h_errno));
			host = hp->h_name;
			sin.sin_addr = *(struct in_addr *)hp->h_addr_list[0];
		}

		whois(*argv++, &sin, flags);
	}
	exit(0);
}

static void
whois(name, sinp, flags)
	char *name;
	struct sockaddr_in *sinp;
	int flags;
{
	FILE *sfi, *sfo;
	char *buf, *p, *nhost;
	size_t len;
	int s, nomatch;

	s = socket(PF_INET, SOCK_STREAM, 0);
	if (s < 0)
		err(EX_OSERR, "socket");

	if (connect(s, (struct sockaddr *)sinp, sizeof(*sinp)) < 0)
		err(EX_OSERR, "connect");

	sfi = fdopen(s, "r");
	sfo = fdopen(s, "w");
	if (sfi == NULL || sfo == NULL)
		err(EX_OSERR, "fdopen");
	(void)fprintf(sfo, "%s\r\n", name);
	(void)fflush(sfo);
	nhost = NULL;
	nomatch = 0;
	while ((buf = fgetln(sfi, &len))) {
		if (buf[len - 2] == '\r')
			buf[len - 2] = '\0';
		else
			buf[len - 1] = '\0';

		if ((flags & WHOIS_RECURSE) && !nhost &&
		    (p = strstr(buf, "Whois Server: "))) {
			p += sizeof("Whois Server: ") - 1;
			if ((len = strcspn(p, " \t\n\r"))) {
				if ((nhost = malloc(len + 1)) == NULL)
					err(1, "malloc");
				memcpy(nhost, p, len);
				nhost[len] = '\0';
			}
		}
		if ((flags & WHOIS_INIC_FALLBACK) && !nhost && !nomatch &&
		    (p = strstr(buf, "No match for \""))) {
			p += sizeof("No match for \"") - 1;
			if ((len = strcspn(p, "\"")) &&
			    name[len] == '\0' &&
			    strncasecmp(name, p, len) == 0 &&
			    strchr(name, '.') == NULL)
				nomatch = 1;
		}
		(void)puts(buf);
	}

	/* Do second lookup as needed */
	if (nomatch && !nhost) {
		(void)printf("Looking up %s at %s.\n\n", name, INICHOST);
		nhost = INICHOST;
	}
	if (nhost) {
		if (inet_aton(nhost, &sinp->sin_addr) == 0) {
			struct hostent *hp = gethostbyname2(nhost, AF_INET);
			if (hp == NULL)
				return;
			sinp->sin_addr = *(struct in_addr *)hp->h_addr_list[0];
		}
		if (!nomatch)
			free(nhost);
		whois(name, sinp, 0);
	}
}

static void
usage()
{
	(void)fprintf(stderr,
	    "usage: whois [-adgimpQrR] [-h hostname] name ...\n");
	exit(EX_USAGE);
}
