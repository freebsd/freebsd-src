/*-
 * SPDX-License-Identifier: BSD-2-Clause
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
#include <sysexits.h>
#include <unistd.h>

#include "main.h"
#ifdef INET
#include "ping.h"
#endif
#ifdef INET6
#include "ping6.h"
#endif

#if defined(INET) && defined(INET6)
#define	OPTSTR PING6OPTS PING4OPTS
#elif defined(INET)
#define	OPTSTR PING4OPTS
#elif defined(INET6)
#define	OPTSTR PING6OPTS
#else
#error At least one of INET and INET6 is required
#endif

int
main(int argc, char *argv[])
{
#if defined(INET)
	struct in_addr a;
#endif
#if defined(INET6)
	struct in6_addr a6;
#endif
#if defined(INET) && defined(INET6)
	struct addrinfo hints, *res, *ai;
	const char *target;
	int error;
#endif
	int opt;

#ifdef INET6
	if (strcmp(getprogname(), "ping6") == 0)
		return ping6(argc, argv);
#endif

	while ((opt = getopt(argc, argv, ":" OPTSTR)) != -1) {
		switch (opt) {
#ifdef INET
		case '4':
			goto ping4;
#endif
#ifdef INET6
		case '6':
			goto ping6;
#endif
		case 'S':
			/*
			 * If -S is given with a numeric parameter,
			 * force use of the corresponding version.
			 */
#ifdef INET
			if (inet_pton(AF_INET, optarg, &a) == 1)
				goto ping4;
#endif
#ifdef INET6
			if (inet_pton(AF_INET6, optarg, &a6) == 1)
				goto ping6;
#endif
			break;
		default:
			break;
		}
	}

	/*
	 * For IPv4, only one positional argument, the target, is allowed.
	 * For IPv6, multiple positional argument are allowed; the last
	 * one is the target, and preceding ones are intermediate hops.
	 * This nuance is lost here, but the only case where it matters is
	 * an error.
	 */
	if (optind >= argc)
		usage();

#if defined(INET) && defined(INET6)
	target = argv[argc - 1];
	memset(&hints, 0, sizeof(hints));
	hints.ai_socktype = SOCK_RAW;
	if (feature_present("inet") && !feature_present("inet6"))
		hints.ai_family = AF_INET;
	else if (feature_present("inet6") && !feature_present("inet"))
		hints.ai_family = AF_INET6;
	else
		hints.ai_family = AF_UNSPEC;
	error = getaddrinfo(target, NULL, &hints, &res);
	if (res == NULL)
		errx(EX_NOHOST, "cannot resolve %s: %s",
		    target, gai_strerror(error));
	for (ai = res; ai != NULL; ai = ai->ai_next) {
		if (ai->ai_family == AF_INET) {
			freeaddrinfo(res);
			goto ping4;
		}
		if (ai->ai_family == AF_INET6) {
			freeaddrinfo(res);
			goto ping6;
		}
	}
	freeaddrinfo(res);
	errx(EX_NOHOST, "cannot resolve %s", target);
#endif
#ifdef INET
ping4:
	optreset = 1;
	optind = 1;
	return ping(argc, argv);
#endif
#ifdef INET6
ping6:
	optreset = 1;
	optind = 1;
	return ping6(argc, argv);
#endif
}

void
usage(void)
{
	(void)fprintf(stderr,
	    "usage:\n"
#ifdef INET
	    "\tping [-4AaDdfHnoQqRrv] [-C pcp] [-c count] "
	    "[-G sweepmaxsize]\n"
	    "\t    [-g sweepminsize] [-h sweepincrsize] [-i wait] "
	    "[-l preload]\n"
	    "\t    [-M mask | time] [-m ttl] "
#ifdef IPSEC
	    "[-P policy] "
#endif
	    "[-p pattern] [-S src_addr] \n"
	    "\t    [-s packetsize] [-t timeout] [-W waittime] [-z tos] "
	    "IPv4-host\n"
	    "\tping [-4AaDdfHLnoQqRrv] [-C pcp] [-c count] [-I iface] "
	    "[-i wait]\n"
	    "\t    [-l preload] [-M mask | time] [-m ttl] "
#ifdef IPSEC
	    "[-P policy] "
#endif
	    "[-p pattern]\n"
	    "\t    [-S src_addr] [-s packetsize] [-T ttl] [-t timeout] [-W waittime]\n"
	    "\t    [-z tos] IPv4-mcast-group\n"
#endif /* INET */
#ifdef INET6
	    "\tping [-6AaDd"
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
	    "[-b bufsiz] [-C pcp] [-c count] [-e gateway]\n"
	    "\t    [-I interface] [-i wait] [-k addrtype] [-l preload] "
	    "[-m hoplimit]\n"
	    "\t    [-p pattern]"
#if defined(IPSEC) && defined(IPSEC_POLICY_IPSEC)
	    " [-P policy]"
#endif
	    " [-S sourceaddr] [-s packetsize] [-t timeout]\n"
	    "\t    [-W waittime] [-z tclass] [IPv6-hops ...] IPv6-host\n"
#endif	/* INET6 */
	    );

	exit(1);
}
