/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 Gleb Smirnoff <glebius@FreeBSD.org>
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
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>

#include <atf-c.h>

static int
connectedsock(void)
{
	struct sockaddr_in6 sa = {
		.sin6_family = AF_INET6,
		.sin6_len = sizeof(sa),
		.sin6_port = htons(1638),
		.sin6_addr = IN6ADDR_LOOPBACK_INIT,
	};
	int s;

	ATF_REQUIRE((s = socket(AF_INET6, SOCK_DGRAM, 0)) >= 0);
	ATF_REQUIRE(connect(s, (struct sockaddr *)&sa, sizeof(sa)) == 0);
	return (s);
}

static void
REQUIRE_SO_ERROR(int s, int expected)
{
	socklen_t len = sizeof(int);
	int error;

	ATF_REQUIRE(getsockopt(s, SOL_SOCKET, SO_ERROR, &error, &len) == 0);
	ATF_REQUIRE_MSG(error == expected,
	    "SO_ERROR: got %d (%s), expected %d (%s)",
	    error, strerror(error), expected, strerror(expected));
}

/*
 * Two connected sockets to the same remote address and port.  Kernel will
 * assign different local ports to them.  Trigger ICMPv6 Port Unreachable for
 * each (kernel generates it).  Assert that only one of them is notified.
 * XXX: the remote is actually ::1 and the test relies on port being
 * unreachable.
 */
ATF_TC_WITHOUT_HEAD(icmp_port_unreach_notify);
ATF_TC_BODY(icmp_port_unreach_notify, tc)
{
	const char buf[] = "probe";
	int s1, s2;

	s1 = connectedsock();
	s2 = connectedsock();
	ATF_REQUIRE(send(s1, buf, sizeof(buf), 0) == sizeof(buf));
	usleep(100000);
	REQUIRE_SO_ERROR(s1, ECONNREFUSED);
	REQUIRE_SO_ERROR(s2, 0);
	ATF_REQUIRE(send(s2, buf, sizeof(buf), 0) == sizeof(buf));
	usleep(100000);
	REQUIRE_SO_ERROR(s2, ECONNREFUSED);
	REQUIRE_SO_ERROR(s1, 0);

	close(s1);
	close(s2);
}

ATF_TP_ADD_TCS(tp)
{
	ATF_TP_ADD_TC(tp, icmp_port_unreach_notify);

	return (atf_no_error());
}
