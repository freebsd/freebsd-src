/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2025 Gleb Smirnoff <glebius@FreeBSD.org>
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

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <assert.h>
#include <errno.h>
#include <limits.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <err.h>

static in_port_t
atop(const char *c)
{
	unsigned long ul;

	errno = 0;
	if ((ul = strtol(c, NULL, 10)) < 1 || ul > IPPORT_MAX || errno != 0)
		err(1, "can't parse %s", c);

	return ((in_port_t)ul);
}

int
main(int argc, char *argv[])
{
	struct sockaddr_in src = {
		.sin_family = AF_INET,
		.sin_len = sizeof(struct sockaddr_in),
	}, dst = {
		.sin_family = AF_INET,
		.sin_len = sizeof(struct sockaddr_in),
	};
	struct ip_mreqn mreqn;
	struct in_addr in;
	int s;
	bool index;

	if (argc < 7)
		errx(1, "Usage: %s src-IPv4 src-port dst-IPv4 dst-port "
		    "interface payload", argv[0]);

	if (inet_pton(AF_INET, argv[1], &src.sin_addr) != 1)
		err(1, "inet_pton(%s) failed", argv[1]);
	src.sin_port = htons(atop(argv[2]));
	if (inet_pton(AF_INET, argv[3], &dst.sin_addr) != 1)
		err(1, "inet_pton(%s) failed", argv[3]);
	dst.sin_port = htons(atop(argv[4]));
	if (inet_pton(AF_INET, argv[5], &in) == 1)
		index = false;
	else if ((mreqn.imr_ifindex = if_nametoindex(argv[5])) > 0)
		index = true;
	else
		err(1, "if_nametoindex(%s) failed", argv[5]);

	assert((s = socket(PF_INET, SOCK_DGRAM, 0)) > 0);
	assert(bind(s, (struct sockaddr *)&src, sizeof(src)) == 0);
	if (index)
		assert(setsockopt(s, IPPROTO_IP, IP_MULTICAST_IF, &mreqn,
		    sizeof(mreqn)) == 0);
	else
		assert(setsockopt(s, IPPROTO_IP, IP_MULTICAST_IF, &in,
		    sizeof(in)) == 0);
	if (sendto(s, argv[6], strlen(argv[6]) + 1, 0, (struct sockaddr *)&dst,
	    sizeof(dst)) != (ssize_t)strlen(argv[6]) + 1)
		err(1, "sendto failed");

	return (0);
}
