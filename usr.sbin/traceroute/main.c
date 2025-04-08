/*
 * SPDX-License-Identifier: ISC
 *
 * Copyright (c) 2025 Lexi Winter
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <sys/types.h>
#include <sys/socket.h>

#include <netinet/in.h>
#include <netinet/ip.h>

#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <netdb.h>
#include <unistd.h>
#include <err.h>

#include "traceroute.h"

char *hostname;
int packlen;
int as_path;		/* print as numbers for each hop */
char *as_server;
char *device;
#ifdef CANT_HACK_IPCKSUM
int doipcksum = 0;	/* don't calculate ip checksums by default */
#else
int doipcksum = 1;	/* calculate ip checksums by default */
#endif
int fixedPort;		/* Use fixed destination port for TCP and UDP */
int options;		/* socket options */
int printdiff;		/* Print the difference between sent and quoted */
int ecnflag;		/* ECN bleaching detection flag */
int first_ttl = 1;
int Iflag;
int max_ttl = -1;
int Nflag;
int nflag;		/* print addresses numerically */
int nprobes = -1;
unsigned short off;
unsigned int pausemsecs;
char *protoname;
int requestPort = -1;
int Sflag;
char *source;
int sump;
int Tflag;
int tos = -1;
int Uflag;
int verbose;
int waittime = 5;	/* time to wait for response (in seconds) */

const char *gateways[MAX_GATEWAYS];
int ngateways;

int
main(int argc, char **argv)
{
	const char *progname = getprogname();
	bool is_traceroute6 =
		(progname != NULL) && !strcmp(progname, "traceroute6");
	const char *optstr = is_traceroute6
		? "aA:dEf:g:Ilm:nNp:q:rs:St:TUvw:"
		: "46aA:eEdDFInrSvxf:g:i:M:m:P:p:q:s:t:w:z:";
	int opt, ret;
	struct addrinfo hints, *ai;

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = is_traceroute6 ? AF_INET6 : AF_UNSPEC;

	while ((opt = getopt(argc, argv, optstr)) != -1) {
		switch (opt) {
		case '4':
			hints.ai_family = AF_INET;
			break;

		case '6':
			hints.ai_family = AF_INET6;
			break;

		case 'a':
			as_path = 1;
			break;

		case 'A':
			as_path = 1;
			as_server = optarg;
			break;

		case 'd':
			options |= SO_DEBUG;
			break;

		case 'D':
			printdiff = 1;
			break;

		case 'e':
			fixedPort = 1;
			break;

		case 'E':
			ecnflag = 1;
			break;

		case 'f':
		case 'M':	/* FreeBSD compat. */
			first_ttl = str2val(optarg, "first ttl", 1, 255);
			break;

		case 'F':
			off = IP_DF;
			break;

		/*
		 * XXX - This appears to have been broken for IPv6 for a long
		 * time (and is still broken).
		 */
		case 'g':
			if (ngateways == MAX_GATEWAYS)
				errx(1, "too many gateways");

			gateways[ngateways++] = optarg;
			break;

		case 'I':
			Iflag = 1;
			break;

		case 'i':
			device = optarg;
			break;

		case 'l':
			/*
			 * Ignored for backward compatibility with historical
			 * versions of traceroute6.
			 */
			break;

		case 'm':
			max_ttl = str2val(optarg, "max ttl", 1, 255);
			break;

		case 'N': /* traceroute6 only */
			Nflag = 1;
			break;

		case 'n':
			++nflag;
			break;

		case 'P':
			protoname = optarg;
			break;

		case 'p':
			requestPort = (u_short)str2val(optarg, "port",
			    1, (1 << 16) - 1);
			break;

		case 'q':
			nprobes = str2val(optarg, "nprobes", 1, -1);
			break;

		/*
		 * XXX - We accept this option for traceroute6 to match the
		 * historical behaviour, but although the kernel will allow it
		 * to be set, it doesn't appear to do anything, i.e. outgoing
		 * packets are still routed.
		 */
		case 'r':
			options |= SO_DONTROUTE;
			break;

		case 'S':
			if (is_traceroute6)
				Sflag = 1; /* use SCTP */
			else
				sump = 1; /* print loss% on each hop */
			break;

		case 's':
			source = optarg;
			break;

		case 'T': /* traceroute6 only */
			Tflag = 1;
			break;

		case 't':
			tos = str2val(optarg, "tos", 0, 255);
			break;

		case 'U': /* traceroute6 only */
			Uflag = 1;
			break;

		case 'v':
			++verbose;
			break;

		case 'w':
			waittime = str2val(optarg, "wait time",
			    1, 24 * 60 * 60);
			break;

		/*
		 * XXX - This needs to be cleaned up.  On FreeBSD,
		 * CANT_HACK_IPCKSUM is never defined, so this isn't really a
		 * toggle.  Determine the actual behaviour and correct both
		 * this code and the manpage.
		 */
		case 'x':
			doipcksum = (doipcksum == 0);
			break;

		case 'z':
			pausemsecs = str2val(optarg, "pause msecs",
			    0, 60 * 60 * 1000);
			break;
		}
	}

	argv += optind;
	argc -= optind;

	hints.ai_socktype = SOCK_DGRAM;
	hints.ai_protocol = IPPROTO_UDP;
	hints.ai_flags = AI_ADDRCONFIG | AI_CANONNAME | AI_NUMERICSERV;

	ret = getaddrinfo(argv[0], NULL, &hints, &ai);
	if (ret)
		errx(1, "%s: %s", argv[0], gai_strerror(ret));

	if (argc > 1) {
		packlen = strtoul(argv[1], NULL, 10);
	}

	for (; ai; ai = ai->ai_next) {
		if (ai->ai_family != AF_INET && ai->ai_family != AF_INET6)
			continue;

		if (ai->ai_next) {
			char shost[NI_MAXHOST];
			ret = getnameinfo(ai->ai_addr, ai->ai_addrlen, shost,
					  sizeof(shost), NULL, 0,
					  NI_NUMERICHOST);
			if (ret)
				errx(1, "getnameinfo failed");

			warnx("%s has multiple addresses; using %s",
			      argv[0], shost);
		}

		hostname = ai->ai_canonname ? ai->ai_canonname : argv[0];

		switch (ai->ai_family) {
		case AF_INET:
			return traceroute4(ai->ai_addr);

		case AF_INET6:
			/*
			 * Many of these could be supported for AF_INET6, they
			 * just aren't right now.
			 */

			if (printdiff)
				errx(1, "the -D flag is not supported for "
				     "IPv6 hosts");

			if (fixedPort)
				errx(1, "the -e flag is not supported for "
				     "IPv6 hosts");

			if (off)
				errx(1, "the -F flag is not supported for "
				     "IPv6 hosts");

			if (device)
				errx(1, "the -i flag is not supported for "
				     "IPv6 hosts");

			if (pausemsecs)
				errx(1, "the -z flag is not supported for "
				     "IPv6 hosts");

			if (sump)
				errx(1, "the -S flag is not supported for "
				     "IPv6 hosts");

			if (protoname)
				errx(1, "the -P flag is not supported for "
				     "IPv6 hosts");

			return traceroute6(ai->ai_addr);

		default:
			break;
		}
	}

	errx(1, "%s: no suitable addresses", argv[0]);
}
