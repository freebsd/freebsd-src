/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
 * Copyright (C) 2019 Jan Sucan <jansucan@FreeBSD.org>
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
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/types.h>
#include <sys/socket.h>

#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>

#include <err.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "main.h"
#include "ping.h"
#ifdef INET6
#include "ping6.h"
#endif

#ifdef INET6
#define	OPTSTR ":46"
#else
#define OPTSTR ":4"
#endif

int
main(int argc, char *argv[])
{
	struct in_addr a;
	struct addrinfo hints;
	int ch;
	bool ipv4;
#ifdef INET6
	struct in6_addr a6;
	bool ipv6;

	if (strcmp(getprogname(), "ping6") == 0)
		ipv6 = true;
	else
		ipv6 = false;
#endif
	ipv4 = false;

	while ((ch = getopt(argc, argv, OPTSTR)) != -1) {
		switch(ch) {
		case '4':
			ipv4 = true;
			break;
#ifdef INET6
		case '6':
			ipv6 = true;
			break;
#endif
		default:
			break;
		}
	}

	if (optind >= argc)
		usage();

	optreset = 1;
	optind = 1;
#ifdef INET6
	if (ipv4 && ipv6)
		errx(1, "-4 and -6 cannot be used simultaneously");
#endif

	if (inet_pton(AF_INET, argv[argc - 1], &a) == 1) {
#ifdef INET6
		if (ipv6)
			errx(1, "IPv6 requested but IPv4 target address "
			    "provided");
#endif
		hints.ai_family = AF_INET;
	}
#ifdef INET6
	else if (inet_pton(AF_INET6, argv[argc - 1], &a6) == 1) {
		if (ipv4)
			errx(1, "IPv4 requested but IPv6 target address "
			    "provided");
		hints.ai_family = AF_INET6;
	} else if (ipv6)
		hints.ai_family = AF_INET6;
#endif
	else if (ipv4)
		hints.ai_family = AF_INET;
	else {
		struct addrinfo *res;

		memset(&hints, 0, sizeof(hints));
		hints.ai_socktype = SOCK_RAW;
		hints.ai_family = AF_UNSPEC;
		getaddrinfo(argv[argc - 1], NULL, &hints, &res);
		if (res != NULL) {
			hints.ai_family = res[0].ai_family;
			freeaddrinfo(res);
		}
	}

	if (hints.ai_family == AF_INET)
		return ping(argc, argv);
#ifdef INET6
	else if (hints.ai_family == AF_INET6)
		return ping6(argc, argv);
#endif
	else
		errx(1, "Unknown host");
}

void
usage(void)
{
	(void)fprintf(stderr,
	    "usage: ping [-4AaDdfHnoQqRrv] [-C pcp] [-c count] "
	    "[-G sweepmaxsize]\n"
	    "	    [-g sweepminsize] [-h sweepincrsize] [-i wait] "
	    "[-l preload]\n"
	    "	    [-M mask | time] [-m ttl]" 
#ifdef IPSEC
	    "[-P policy] "
#endif
	    "[-p pattern] [-S src_addr] \n"
	    "	    [-s packetsize] [-t timeout] [-W waittime] [-z tos] "
	    "IPv4-host\n"
	    "       ping [-4AaDdfHLnoQqRrv] [-C pcp] [-c count] [-I iface] "
	    "[-i wait]\n"
	    "	    [-l preload] [-M mask | time] [-m ttl] "
#ifdef IPSEC
	    "[-P policy] "
#endif
	    "[-p pattern]\n"
	    "	    [-S src_addr] [-s packetsize] [-T ttl] [-t timeout] [-W waittime]\n"
	    "            [-z tos] IPv4-mcast-group\n"
#ifdef INET6
            "       ping [-6aADd"
#if defined(IPSEC) && !defined(IPSEC_POLICY_IPSEC)
            "E"
#endif
            "fHnNoOq"
#ifdef IPV6_USE_MIN_MTU
            "u"
#endif
            "vyY"
#if defined(IPSEC) && !defined(IPSEC_POLICY_IPSEC)
            "Z"
#endif
	    "] "
            "[-b bufsiz] [-c count] [-e gateway]\n"
            "            [-I interface] [-i wait] [-k addrtype] [-l preload] "
            "[-m hoplimit]\n"
            "            [-p pattern]"
#if defined(IPSEC) && defined(IPSEC_POLICY_IPSEC)
            " [-P policy]"
#endif
            " [-S sourceaddr] [-s packetsize] [-t timeout]\n"
	    "	    [-W waittime] [-z tclass] [IPv6-hops ...] IPv6-host\n"
#endif	/* INET6 */
	    );

	exit(1);
}
