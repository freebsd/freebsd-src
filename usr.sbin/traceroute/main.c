/*
 * Copyright (c) 1988, 1989, 1991, 1994, 1995, 1996, 1997, 1998, 1999, 2000
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that: (1) source code distributions
 * retain the above copyright notice and this paragraph in its entirety, (2)
 * distributions including binary code include the above copyright notice and
 * this paragraph in its entirety in the documentation or other materials
 * provided with the distribution, and (3) all advertising materials mentioning
 * features or use of this software display the following acknowledgement:
 * ``This product includes software developed by the University of California,
 * Lawrence Berkeley Laboratory and its contributors.'' Neither the name of
 * the University nor the names of its contributors may be used to endorse
 * or promote products derived from this software without specific prior
 * written permission.
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */

#include <sys/types.h>
#include <sys/socket.h>

#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <netdb.h>
#include <unistd.h>
#include <err.h>

#include "traceroute.h"

char *hostname;
int packlen;

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
	hints.ai_family = AF_UNSPEC;

	while ((opt = getopt(argc, argv, optstr)) != -1) {
		switch (opt) {
		case '4':
			hints.ai_family = AF_INET;
			break;

		case '6':
			hints.ai_family = AF_INET6;
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
		hostname = ai->ai_canonname ? ai->ai_canonname : argv[0];

		switch (ai->ai_family) {
		case AF_INET:
			return traceroute4(ai->ai_addr);
		case AF_INET6:
			return traceroute6(ai->ai_addr);
		default:
			break;
		}
	}

	errx(1, "%s: no suitable addresses", argv[0]);
}
