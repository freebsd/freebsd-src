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
#include <ctype.h>
#include <err.h>
#include <netdb.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sysexits.h>
#include <unistd.h>

#define	NICHOST		"whois.crsnic.net"
#define	INICHOST	"whois.networksolutions.com"
#define	DNICHOST	"whois.nic.mil"
#define	GNICHOST	"whois.nic.gov"
#define	ANICHOST	"whois.arin.net"
#define	RNICHOST	"whois.ripe.net"
#define	PNICHOST	"whois.apnic.net"
#define	RUNICHOST	"whois.ripn.net"
#define	MNICHOST	"whois.ra.net"
#define	QNICHOST_TAIL	".whois-servers.net"
#define	SNICHOST	"whois.6bone.net"
#define	WHOIS_PORT	43
#define	WHOIS_SERVER_ID	"Whois Server: "
#define	NO_MATCH_ID	"No match for \""

#define WHOIS_RECURSE		0x01
#define WHOIS_INIC_FALLBACK	0x02
#define WHOIS_QUICK		0x04

const char *ip_whois[] = { RNICHOST, PNICHOST, NULL };

static char *choose_server(char *);
static struct addrinfo *gethostinfo(char const *host, int exit_on_error);
static void s_asprintf(char **ret, const char *format, ...);
static void usage(void);
static void whois(char *, struct addrinfo *, int);

int
main(int argc, char *argv[])
{
	struct addrinfo *res;
	const char *host;
	char *qnichost;
	int ch, flags, use_qnichost;

#ifdef	SOCKS
	SOCKSinit(argv[0]);
#endif

	host = NULL;
	qnichost = NULL;
	flags = 0;
	use_qnichost = 0;
	while ((ch = getopt(argc, argv, "adgh:impQrR6")) != -1) {
		switch (ch) {
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
		case '6':
			host = SNICHOST;
			break;
		case '?':
		default:
			usage();
			/* NOTREACHED */
		}
	}
	argc -= optind;
	argv += optind;

	if (!argc)
		usage();

	/*
	 * If no nic host is specified determine the top level domain from
	 * the query.  If the TLD is a number, query ARIN.  Otherwise, use
	 * TLD.whois-server.net.  If the domain does not contain '.', fall
	 * back to NICHOST.
	 */
	if (host == NULL) {
		use_qnichost = 1;
		host = NICHOST;
		if (!(flags & WHOIS_QUICK))
			flags |= WHOIS_INIC_FALLBACK | WHOIS_RECURSE;
	}
	while (argc--) {
		if (use_qnichost)
			if ((qnichost = choose_server(*argv)) != NULL)
				res = gethostinfo(qnichost, 1);
		if (qnichost == NULL)
			res = gethostinfo(host, 1);

		free(qnichost);
		qnichost = NULL;
		whois(*argv++, res, flags);
		freeaddrinfo(res);
	}
	exit(0);
}

/*
 * This function will remove any trailing periods from domain, after which it
 * returns a pointer to newly allocated memory containing the whois server to
 * be queried, or a NULL if the correct server couldn't be determined.  The
 * caller must remember to free(3) the allocated memory.
 */
static char *
choose_server(char *domain)
{
	char *pos, *retval;

	for (pos = strchr(domain, '\0'); pos > domain && *--pos == '.';)
		*pos = '\0';
	if (*domain == '\0')
		errx(EX_USAGE, "can't search for a null string");
	while (pos > domain && *pos != '.')
		--pos;
	if (pos <= domain)
		return (NULL);
	if (isdigit((unsigned char)*++pos))
		s_asprintf(&retval, "%s", ANICHOST);
	else
		s_asprintf(&retval, "%s%s", pos, QNICHOST_TAIL);
	return (retval);
}

static struct addrinfo *
gethostinfo(char const *host, int exit_on_error)
{
	struct addrinfo hints, *res;
	int error;

	memset(&hints, 0, sizeof(hints));
	hints.ai_flags = 0;
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	error = getaddrinfo(host, "whois", &hints, &res);
	if (error) {
		warnx("%s: %s", host, gai_strerror(error));
		if (exit_on_error)
			exit(EX_NOHOST);
		return (NULL);
	}
	return (res);
}

/*
 * Wrapper for asprintf(3) that exits on error.
 */
static void
s_asprintf(char **ret, const char *format, ...)
{
	va_list ap;

	va_start(ap, format);
	if (vasprintf(ret, format, ap) == -1) {
		va_end(ap);
		err(EX_OSERR, "vasprintf()");
	}
	va_end(ap);
}

static void
whois(char *name, struct addrinfo *res, int flags)
{
	FILE *sfi, *sfo;
	struct addrinfo *res2;
	char *buf, *nhost, *p;
	int i, nomatch, s;
	size_t len;

	for (; res; res = res->ai_next) {
		s = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
		if (s < 0)
			continue;
		if (connect(s, res->ai_addr, res->ai_addrlen) == 0)
			break;
		close(s);
	}
	if (res == NULL)
		err(EX_OSERR, "connect()");

	sfi = fdopen(s, "r");
	sfo = fdopen(s, "w");
	if (sfi == NULL || sfo == NULL)
		err(EX_OSERR, "fdopen()");
	fprintf(sfo, "%s\r\n", name);
	fflush(sfo);
	nhost = NULL;
	nomatch = 0;
	while ((buf = fgetln(sfi, &len)) != NULL) {
		while (len && isspace(buf[len - 1]))
			buf[--len] = '\0';

		if ((flags & WHOIS_RECURSE) && nhost == NULL) {
			p = strstr(buf, WHOIS_SERVER_ID);
			if (p != NULL) {
				p += sizeof(WHOIS_SERVER_ID) - 1;
				if ((len = strcspn(p, " \t\n\r")) != 0) {
					s_asprintf(&nhost, "%s", p);
				}
			} else {
				for (i = 0; ip_whois[i] != NULL; i++) {
					if (strstr(buf, ip_whois[i]) == NULL)
						continue;
					s_asprintf(&nhost, "%s", ip_whois[i]);
				}
			}
		}

		if ((flags & WHOIS_INIC_FALLBACK) && nhost == NULL &&
		    !nomatch && (p = strstr(buf, NO_MATCH_ID)) != NULL) {
			p += sizeof(NO_MATCH_ID) - 1;
			if ((len = strcspn(p, "\"")) &&
			    strncasecmp(name, p, len) == 0 &&
			    name[len] == '\0' &&
			    strchr(name, '.') == NULL)
				nomatch = 1;
		}
		printf("%s\n", buf);
	}

	/* Do second lookup as needed. */
	if (nomatch && nhost == NULL) {
		printf("Looking up %s at %s.\n\n", name, INICHOST);
		s_asprintf(&nhost, "%s", INICHOST);
	}

	if (nhost != NULL) {
		if ((res2 = gethostinfo(nhost, 0)) == NULL) {
			free(nhost);
			return;
		}
		free(nhost);
		whois(name, res2, 0);
		freeaddrinfo(res2);
	}
}

static void
usage(void)
{
	fprintf(stderr,
	    "usage: whois [-adgimpQrR6] [-h hostname] name ...\n");
	exit(EX_USAGE);
}
