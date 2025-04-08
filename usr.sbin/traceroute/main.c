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
