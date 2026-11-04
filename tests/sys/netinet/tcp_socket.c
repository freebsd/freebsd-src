/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2024 Gleb Smirnoff <glebius@FreeBSD.org>
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
#include <errno.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>

#include <atf-c.h>

ATF_TC_WITHOUT_HEAD(implied_connect);
ATF_TC_BODY(implied_connect, tc)
{
	struct sockaddr_in sin = {
		.sin_family = AF_INET,
		.sin_len = sizeof(sin),
	};
	const char buf[] = "hello";
	char repl[sizeof(buf)];
	socklen_t len;
	int s, c, a;

	s = socket(PF_INET, SOCK_STREAM, 0);
	ATF_REQUIRE(s != -1);
	c = socket(PF_INET, SOCK_STREAM, 0);
	ATF_REQUIRE(c != -1);

	ATF_REQUIRE(bind(s, (struct sockaddr *)&sin, sizeof(sin)) == 0);
	len = sizeof(sin);
	ATF_REQUIRE(getsockname(s, (struct sockaddr *)&sin, &len) == 0);
	sin.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	ATF_REQUIRE(listen(s, -1) == 0);
#if 0
	/*
	 * The disabled code is that you would normally do.
	 */
	ATF_REQUIRE(connect(c, (struct sockaddr *)&sin, sizeof(sin)) == 0);
	ATF_REQUIRE(send(c, &buf, sizeof(buf), 0) == sizeof(buf));
#else
	/*
	 * And this is implied connect.
	 */
	ATF_REQUIRE(sendto(c, &buf, sizeof(buf), 0, (struct sockaddr *)&sin,
	    sizeof(sin)) == sizeof(buf));
#endif

	ATF_REQUIRE((a = accept(s, NULL, NULL)) != 1);
	ATF_REQUIRE(recv(a, &repl, sizeof(repl), 0) == sizeof(buf));
	ATF_REQUIRE(strcmp(buf, repl) == 0);
}

/*
 * A disconnected TCP socket shall return the local address it used before
 * it was disconnected.
 */
ATF_TC_WITHOUT_HEAD(getsockname_disconnected);
ATF_TC_BODY(getsockname_disconnected, tc)
{
	struct sockaddr_in sin = {
		.sin_family = AF_INET,
		.sin_len = sizeof(sin),
	};
	socklen_t len;
	int s, c, a;

	s = socket(PF_INET, SOCK_STREAM, 0);
	ATF_REQUIRE(s != -1);
	c = socket(PF_INET, SOCK_STREAM, 0);
	ATF_REQUIRE(c != -1);

	ATF_REQUIRE(bind(s, (struct sockaddr *)&sin, sizeof(sin)) == 0);
	len = sizeof(sin);
	ATF_REQUIRE(getsockname(s, (struct sockaddr *)&sin, &len) == 0);
	sin.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	ATF_REQUIRE(listen(s, -1) == 0);
	ATF_REQUIRE(connect(c, (struct sockaddr *)&sin, sizeof(sin)) == 0);
	ATF_REQUIRE((a = accept(s, NULL, NULL)) != 1);
	ATF_REQUIRE(close(a) == 0);
	ATF_REQUIRE(getsockname(c, (struct sockaddr *)&sin, &len) == 0);
	ATF_REQUIRE(sin.sin_addr.s_addr == htonl(INADDR_LOOPBACK));

	close(c);
	close(s);
}

/*
 * A failed connect to an in-use v4-mapped tuple must leave a bound,
 * dual-stack TCP socket able to connect to an IPv6 listener.
 */
ATF_TC_WITHOUT_HEAD(dualstack_connect_fail);
ATF_TC_BODY(dualstack_connect_fail, tc)
{
	struct sockaddr_in6 addr6 = {
		.sin6_family = AF_INET6,
		.sin6_len = sizeof(addr6),
		.sin6_addr = IN6ADDR_LOOPBACK_INIT,
	};
	struct sockaddr_in sin = {
		.sin_family = AF_INET,
		.sin_len = sizeof(sin),
		.sin_addr.s_addr = htonl(INADDR_LOOPBACK),
	};
	struct sockaddr_in6 peer4 = {
		.sin6_family = AF_INET6,
		.sin6_len = sizeof(peer4),
	};
	struct sockaddr_in6 local6 = {
		.sin6_family = AF_INET6,
		.sin6_len = sizeof(local6),
		.sin6_addr = IN6ADDR_ANY_INIT,
	};
	int error, listener4, listener6, one, s1, s2, accepted;
	socklen_t len;

	listener4 = socket(PF_INET, SOCK_STREAM, 0);
	ATF_REQUIRE_MSG(listener4 >= 0, "socket(AF_INET listener) failed: %s",
	    strerror(errno));
	ATF_REQUIRE_MSG(bind(listener4, (struct sockaddr *)&sin,
	    sizeof(sin)) == 0, "bind(AF_INET listener) failed: %s",
	    strerror(errno));
	ATF_REQUIRE_MSG(listen(listener4, 1) == 0,
	    "listen(AF_INET) failed: %s", strerror(errno));
	len = sizeof(sin);
	ATF_REQUIRE_MSG(getsockname(listener4, (struct sockaddr *)&sin,
	    &len) == 0, "getsockname(AF_INET) failed: %s", strerror(errno));

	listener6 = socket(PF_INET6, SOCK_STREAM, 0);
	ATF_REQUIRE_MSG(listener6 >= 0, "socket(AF_INET6 listener) failed: %s",
	    strerror(errno));
	ATF_REQUIRE_MSG(bind(listener6, (struct sockaddr *)&addr6,
	    sizeof(addr6)) == 0, "bind(listener) failed: %s", strerror(errno));
	ATF_REQUIRE_MSG(listen(listener6, 1) == 0, "listen(IPv6) failed: %s",
	    strerror(errno));
	len = sizeof(addr6);
	ATF_REQUIRE_MSG(getsockname(listener6, (struct sockaddr *)&addr6,
	    &len) == 0, "getsockname(listener) failed: %s", strerror(errno));

	error = inet_pton(AF_INET6, "::ffff:127.0.0.1",
	    &peer4.sin6_addr);
	ATF_REQUIRE_MSG(error == 1, "inet_pton() failed");
	peer4.sin6_port = sin.sin_port;

	one = 0;
	s1 = socket(PF_INET6, SOCK_STREAM, 0);
	ATF_REQUIRE_MSG(s1 >= 0, "socket(first client) failed: %s",
	    strerror(errno));
	error = setsockopt(s1, SOL_SOCKET, SO_REUSEPORT, &(int){1},
	    sizeof(int));
	ATF_REQUIRE_MSG(error == 0, "setsockopt(SO_REUSEPORT) failed: %s",
	    strerror(errno));
	error = setsockopt(s1, IPPROTO_IPV6, IPV6_V6ONLY, &one, sizeof(one));
	ATF_REQUIRE_MSG(error == 0, "setsockopt(IPV6_V6ONLY) failed: %s",
	    strerror(errno));
	error = bind(s1, (struct sockaddr *)&local6, sizeof(local6));
	ATF_REQUIRE_MSG(error == 0, "bind(first client) failed: %s",
	    strerror(errno));
	len = sizeof(local6);
	ATF_REQUIRE_MSG(getsockname(s1, (struct sockaddr *)&local6, &len) == 0,
	    "getsockname(first client) failed: %s", strerror(errno));

	s2 = socket(PF_INET6, SOCK_STREAM, 0);
	ATF_REQUIRE_MSG(s2 >= 0, "socket(second client) failed: %s",
	    strerror(errno));
	error = setsockopt(s2, SOL_SOCKET, SO_REUSEPORT, &(int){1},
	    sizeof(int));
	ATF_REQUIRE_MSG(error == 0, "setsockopt(SO_REUSEPORT) failed: %s",
	    strerror(errno));
	error = setsockopt(s2, IPPROTO_IPV6, IPV6_V6ONLY, &one, sizeof(one));
	ATF_REQUIRE_MSG(error == 0, "setsockopt(IPV6_V6ONLY) failed: %s",
	    strerror(errno));
	error = bind(s2, (struct sockaddr *)&local6, sizeof(local6));
	ATF_REQUIRE_MSG(error == 0, "bind(second client) failed: %s",
	    strerror(errno));

	error = connect(s1, (struct sockaddr *)&peer4, sizeof(peer4));
	ATF_REQUIRE_MSG(error == 0, "first connect(v4-mapped) failed: %s",
	    strerror(errno));
	accepted = accept(listener4, NULL, NULL);
	ATF_REQUIRE_MSG(accepted >= 0, "accept(AF_INET) failed: %s",
	    strerror(errno));
	ATF_REQUIRE(close(accepted) == 0);

	error = connect(s2, (struct sockaddr *)&peer4, sizeof(peer4));
	ATF_REQUIRE_MSG(error == -1 && errno == EADDRINUSE,
	    "second connect(v4-mapped) returned %d: %s",
	    error, strerror(errno));

	error = connect(s2, (struct sockaddr *)&addr6, sizeof(addr6));
	ATF_REQUIRE_MSG(error == 0, "connect(IPv6) after failure failed: %s",
	    strerror(errno));
	accepted = accept(listener6, NULL, NULL);
	ATF_REQUIRE_MSG(accepted >= 0, "accept(IPv6) failed: %s",
	    strerror(errno));

	ATF_REQUIRE(close(accepted) == 0);
	ATF_REQUIRE(close(s2) == 0);
	ATF_REQUIRE(close(s1) == 0);
	ATF_REQUIRE(close(listener6) == 0);
	ATF_REQUIRE(close(listener4) == 0);
}

ATF_TP_ADD_TCS(tp)
{
	ATF_TP_ADD_TC(tp, implied_connect);
	ATF_TP_ADD_TC(tp, getsockname_disconnected);
	ATF_TP_ADD_TC(tp, dualstack_connect_fail);

	return (atf_no_error());
}
