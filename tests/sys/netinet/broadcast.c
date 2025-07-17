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

#include <sys/param.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <net/if.h>
#include <arpa/inet.h>
#include <errno.h>
#include <ifaddrs.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <atf-c.h>

static const char buf[] = "Hello";

/* Create a UDP socket with SO_BROADCAST set. */
static int
bcastsock(void)
{
	int s;

	ATF_REQUIRE((s = socket(PF_INET, SOCK_DGRAM, 0)) > 0);
	ATF_REQUIRE(setsockopt(s, SOL_SOCKET, SO_BROADCAST, &(int){1},
	    sizeof(int)) == 0);
	return (s);
}

/* Send on socket 's' with address 'to', confirm receive on 'r'. */
static void
bcastecho(int s, struct sockaddr_in *to, int r)
{
	char rbuf[sizeof(buf)];

	printf("Sending to %s\n", inet_ntoa(to->sin_addr));
	ATF_REQUIRE_MSG(sendto(s, buf, sizeof(buf), 0, (struct sockaddr *)to,
	    sizeof(*to)) == sizeof(buf), "sending of broadcast failed: %d",
	    errno);
	ATF_REQUIRE(recv(r, rbuf, sizeof(rbuf), 0) == sizeof(rbuf));
	ATF_REQUIRE_MSG(memcmp(buf, rbuf, sizeof(buf)) == 0,
	    "failed to receive own broadcast");
}

/* Find a first broadcast capable interface and copy its broadcast address. */
static void
firstbcast(struct in_addr *out)
{
	struct ifaddrs *ifa0, *ifa;
	struct sockaddr_in sin;

	ATF_REQUIRE(getifaddrs(&ifa0) == 0);
	for (ifa = ifa0; ifa != NULL; ifa = ifa->ifa_next)
		if (ifa->ifa_addr->sa_family == AF_INET &&
		    (ifa->ifa_flags & IFF_BROADCAST))
			break;
	if (ifa == NULL) {
		freeifaddrs(ifa0);
		atf_tc_skip("No broadcast address found");
	}
	memcpy(&sin, ifa->ifa_broadaddr, sizeof(struct sockaddr_in));
	*out = sin.sin_addr;
	freeifaddrs(ifa0);
}

/* Application sends to INADDR_BROADCAST, and this goes on the wire. */
ATF_TC(INADDR_BROADCAST);
ATF_TC_HEAD(INADDR_BROADCAST, tc)
{
	atf_tc_set_md_var(tc, "require.config", "allow_network_access");
}
ATF_TC_BODY(INADDR_BROADCAST, tc)
{
	struct sockaddr_in sin = {
		.sin_family = AF_INET,
		.sin_len = sizeof(struct sockaddr_in),
	};
	socklen_t slen = sizeof(sin);
	int l, s;

	l = bcastsock();
	ATF_REQUIRE(bind(l, (struct sockaddr *)&sin, sizeof(sin)) == 0);
	ATF_REQUIRE(getsockname(l, (struct sockaddr *)&sin, &slen) == 0);
	sin.sin_addr.s_addr = htonl(INADDR_BROADCAST);

	s = bcastsock();
	bcastecho(s, &sin, l);

	close(s);
	close(l);
}

/*
 * Application sends on broadcast address of an interface, INADDR_BROADCAST
 * goes on the wire of the selected interface.
 */
ATF_TC_WITHOUT_HEAD(IP_ONESBCAST);
ATF_TC_BODY(IP_ONESBCAST, tc)
{
	struct ifaddrs *ifa0, *ifa;
	struct sockaddr_in sin = {
		.sin_family = AF_INET,
		.sin_len = sizeof(struct sockaddr_in),
	};
	socklen_t slen = sizeof(sin);
	int s, l;
	in_port_t port;
	bool skip = true;

	s = bcastsock();
	ATF_REQUIRE(setsockopt(s, IPPROTO_IP, IP_ONESBCAST, &(int){1},
	    sizeof(int)) == 0);

	l = bcastsock();
	ATF_REQUIRE(bind(l, (struct sockaddr *)&sin, sizeof(sin)) == 0);
	ATF_REQUIRE(getsockname(l, (struct sockaddr *)&sin, &slen) == 0);
	port = sin.sin_port;

	ATF_REQUIRE(getifaddrs(&ifa0) == 0);
	for (ifa = ifa0; ifa != NULL; ifa = ifa->ifa_next) {
		if (ifa->ifa_addr->sa_family != AF_INET)
			continue;
		if (!(ifa->ifa_flags & IFF_BROADCAST))
			continue;
		skip = false;
		memcpy(&sin, ifa->ifa_broadaddr, sizeof(struct sockaddr_in));
		sin.sin_port = port;
		bcastecho(s, &sin, l);
	}
	freeifaddrs(ifa0);
	close(s);
	close(l);
	if (skip)
		atf_tc_skip("No broadcast address found");
}

/*
 * Application sends on broadcast address of an interface, and this is what
 * goes out the wire.
 */
ATF_TC_WITHOUT_HEAD(local_broadcast);
ATF_TC_BODY(local_broadcast, tc)
{
	struct sockaddr_in sin = {
		.sin_family = AF_INET,
		.sin_len = sizeof(struct sockaddr_in),
	};
	socklen_t slen = sizeof(sin);
	int s, l;

	s = bcastsock();
	l = bcastsock();
	ATF_REQUIRE(bind(l, (struct sockaddr *)&sin, sizeof(sin)) == 0);
	ATF_REQUIRE(getsockname(l, (struct sockaddr *)&sin, &slen) == 0);
	firstbcast(&sin.sin_addr);

	bcastecho(s, &sin, l);

	close(s);
	close(l);
}

ATF_TP_ADD_TCS(tp)
{
	ATF_TP_ADD_TC(tp, INADDR_BROADCAST);
	ATF_TP_ADD_TC(tp, IP_ONESBCAST);
	ATF_TP_ADD_TC(tp, local_broadcast);

	return (atf_no_error());
}
