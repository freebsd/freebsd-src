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
#include <netinet/ip.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <assert.h>
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sysexits.h>
#include <limits.h>
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
	char buf[IP_MAXPACKET + 1];
	struct sockaddr_in sin = {
		.sin_family = AF_INET,
		.sin_len = sizeof(struct sockaddr_in),
	};
	socklen_t slen = sizeof(struct sockaddr_in);
	struct in_addr maddr, ifaddr;
	ssize_t len;
	int s, ifindex;
	bool index;

	if (argc < 4)
usage:
		errx(1, "Usage: %s (ip_mreq|ip_mreqn|group_req) "
		    "IPv4-group port interface", argv[0]);

	if (inet_pton(AF_INET, argv[2], &maddr) != 1)
		err(1, "inet_pton(%s) failed", argv[2]);
	sin.sin_port = htons(atop(argv[3]));
	if (inet_pton(AF_INET, argv[4], &ifaddr) == 1)
		index = false;
	else if ((ifindex = if_nametoindex(argv[4])) > 0)
		index = true;
	else if (strcmp(argv[4], "0") == 0) {
		ifindex = 0;
		index = true;
	} else
		err(1, "if_nametoindex(%s) failed", argv[4]);

	assert((s = socket(PF_INET, SOCK_DGRAM, 0)) > 0);
	assert(bind(s, (struct sockaddr *)&sin, sizeof(sin)) == 0);

	if (strcmp(argv[1], "ip_mreq") == 0) {
		if (index)
			errx(1, "ip_mreq doesn't accept index");
		struct ip_mreq mreq = {
			.imr_multiaddr = maddr,
			.imr_interface = ifaddr,
		};
		if (setsockopt(s, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq,
		    sizeof(mreq)) != 0)
			err(EX_OSERR, "setsockopt");
	} else if (strcmp(argv[1], "ip_mreqn") == 0) {
		/*
		 * ip_mreqn shall be used with index, but for testing
		 * purposes accept address too.
		 */
		struct ip_mreqn mreqn = {
			.imr_multiaddr = maddr,
			.imr_address = index ? (struct in_addr){ 0 } : ifaddr,
			.imr_ifindex = index ? ifindex : 0,
		};
		if (setsockopt(s, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreqn,
		    sizeof(mreqn)) != 0)
			err(EX_OSERR, "setsockopt");
	} else if (strcmp(argv[1], "group_req") == 0) {
		if (!index)
			errx(1, "group_req expects index");
		struct group_req greq = { .gr_interface = ifindex };
		struct sockaddr_in *gsa = (struct sockaddr_in *)&greq.gr_group;

		gsa->sin_family = AF_INET;
		gsa->sin_len = sizeof(struct sockaddr_in);
		gsa->sin_addr = maddr;
		if (setsockopt(s, IPPROTO_IP, MCAST_JOIN_GROUP, &greq,
		    sizeof(greq)) != 0)
			err(EX_OSERR, "setsockopt");
	} else
		goto usage;

	assert((len = recvfrom(s, buf, sizeof(buf) - 1, 0,
	    (struct sockaddr *)&sin, &slen)) > 0);
	buf[len] = '\0';
	printf("%s:%u %s\n", inet_ntoa(sin.sin_addr), ntohs(sin.sin_port), buf);

	return (0);
}
