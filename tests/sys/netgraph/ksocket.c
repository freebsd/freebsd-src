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
#include <netgraph.h>
#include <netgraph/ng_socket.h>
#include <netgraph/ng_ksocket.h>

#include <errno.h>

#include <atf-c.h>

static void
hellocheck(int wr, int rd)
{
	char sbuf[] = "Hello, peer!", rbuf[sizeof(sbuf)];

	ATF_REQUIRE(send(wr, sbuf, sizeof(sbuf), 0) == sizeof(sbuf));
	ATF_REQUIRE(recv(rd, rbuf, sizeof(rbuf), 0) == sizeof(sbuf));
	ATF_REQUIRE(strcmp(sbuf, rbuf) == 0);
}

#define	OURHOOK	"ks"

ATF_TC_WITHOUT_HEAD(udp_connect);
ATF_TC_BODY(udp_connect, tc)
{
	struct ngm_mkpeer mkp = {
		.type = NG_KSOCKET_NODE_TYPE,
		.ourhook = OURHOOK,
		.peerhook = "inet/dgram/udp",
	};
	struct sockaddr_in sin = {
		.sin_family = AF_INET,
		.sin_addr.s_addr = htonl(INADDR_LOOPBACK),
		.sin_len = sizeof(sin),
	};
	socklen_t slen = sizeof(sin);
	int cs, ds, us;

	ATF_REQUIRE((us = socket(PF_INET, SOCK_DGRAM, 0)) > 0);
	ATF_REQUIRE(bind(us, (struct sockaddr *)&sin, sizeof(sin)) == 0);
	ATF_REQUIRE(getsockname(us, (struct sockaddr *)&sin, &slen) == 0);

	ATF_REQUIRE(NgMkSockNode(NULL, &cs, &ds) == 0);
	ATF_REQUIRE(NgSendMsg(cs, ".", NGM_GENERIC_COOKIE, NGM_MKPEER, &mkp,
	    sizeof(mkp)) >= 0);
	ATF_REQUIRE(NgSendMsg(cs, ".:" OURHOOK, NGM_KSOCKET_COOKIE,
	    NGM_KSOCKET_CONNECT, &sin, sizeof(sin)) >= 0);

	hellocheck(ds, us);
}

ATF_TC_WITHOUT_HEAD(udp_bind);
ATF_TC_BODY(udp_bind, tc)
{
	struct ngm_mkpeer mkp = {
		.type = NG_KSOCKET_NODE_TYPE,
		.ourhook = OURHOOK,
		.peerhook = "inet/dgram/udp",
	};
	struct sockaddr_in sin = {
		.sin_family = AF_INET,
		.sin_addr.s_addr = htonl(INADDR_LOOPBACK),
		.sin_len = sizeof(sin),
	};
	struct ng_mesg *rep;
	int cs, ds, us;

	ATF_REQUIRE(NgMkSockNode(NULL, &cs, &ds) == 0);
	ATF_REQUIRE(NgSendMsg(cs, ".", NGM_GENERIC_COOKIE, NGM_MKPEER, &mkp,
	    sizeof(mkp)) >= 0);
	ATF_REQUIRE(NgSendMsg(cs, ".:" OURHOOK, NGM_KSOCKET_COOKIE,
	    NGM_KSOCKET_BIND, &sin, sizeof(sin)) >= 0);
	ATF_REQUIRE(NgSendMsg(cs, ".:" OURHOOK, NGM_KSOCKET_COOKIE,
	    NGM_KSOCKET_GETNAME, NULL, 0) >= 0);
	ATF_REQUIRE(NgAllocRecvMsg(cs, &rep, NULL) == sizeof(struct ng_mesg) +
	    sizeof(struct sockaddr_in));

	ATF_REQUIRE((us = socket(PF_INET, SOCK_DGRAM, 0)) > 0);
	ATF_REQUIRE(connect(us, (struct sockaddr *)rep->data,
	    sizeof(struct sockaddr_in)) == 0);

	hellocheck(us, ds);
}

ATF_TC_WITHOUT_HEAD(udp6_connect);
ATF_TC_BODY(udp6_connect, tc)
{
	struct ngm_mkpeer mkp = {
		.type = NG_KSOCKET_NODE_TYPE,
		.ourhook = OURHOOK,
		.peerhook = "inet6/dgram/udp6",
	};
	struct sockaddr_in6 sin6 = {
		.sin6_family = AF_INET6,
	};
	socklen_t slen = sizeof(sin6);
	int cs, ds, us;

	ATF_REQUIRE((us = socket(PF_INET6, SOCK_DGRAM, 0)) > 0);
	ATF_REQUIRE(bind(us, (struct sockaddr *)&sin6, sizeof(sin6)) == 0);
	ATF_REQUIRE(getsockname(us, (struct sockaddr *)&sin6, &slen) == 0);

	ATF_REQUIRE(NgMkSockNode(NULL, &cs, &ds) == 0);
	ATF_REQUIRE(NgSendMsg(cs, ".", NGM_GENERIC_COOKIE, NGM_MKPEER, &mkp,
	    sizeof(mkp)) >= 0);
	ATF_REQUIRE(NgSendMsg(cs, ".:" OURHOOK, NGM_KSOCKET_COOKIE,
	    NGM_KSOCKET_CONNECT, &sin6, sizeof(sin6)) >= 0);

	hellocheck(ds, us);
}


ATF_TC_WITHOUT_HEAD(udp6_bind);
ATF_TC_BODY(udp6_bind, tc)
{
	struct ngm_mkpeer mkp = {
		.type = NG_KSOCKET_NODE_TYPE,
		.ourhook = OURHOOK,
		.peerhook = "inet6/dgram/udp6",
	};
	struct sockaddr_in6 sin6 = {
		.sin6_family = AF_INET6,
		.sin6_len = sizeof(sin6),
	};
	struct ng_mesg *rep;
	int cs, ds, us;

	ATF_REQUIRE(NgMkSockNode(NULL, &cs, &ds) == 0);
	ATF_REQUIRE(NgSendMsg(cs, ".", NGM_GENERIC_COOKIE, NGM_MKPEER, &mkp,
	    sizeof(mkp)) >= 0);
	ATF_REQUIRE(NgSendMsg(cs, ".:" OURHOOK, NGM_KSOCKET_COOKIE,
	    NGM_KSOCKET_BIND, &sin6, sizeof(sin6)) >= 0);
	ATF_REQUIRE(NgSendMsg(cs, ".:" OURHOOK, NGM_KSOCKET_COOKIE,
	    NGM_KSOCKET_GETNAME, NULL, 0) >= 0);
	ATF_REQUIRE(NgAllocRecvMsg(cs, &rep, NULL) == sizeof(struct ng_mesg) +
	    sizeof(struct sockaddr_in6));

	ATF_REQUIRE((us = socket(PF_INET6, SOCK_DGRAM, 0)) > 0);
	ATF_REQUIRE(connect(us, (struct sockaddr *)rep->data,
			sizeof(struct sockaddr_in6)) == 0);

	hellocheck(us, ds);
}

ATF_TP_ADD_TCS(tp)
{
	ATF_TP_ADD_TC(tp, udp_connect);
	ATF_TP_ADD_TC(tp, udp_bind);
	ATF_TP_ADD_TC(tp, udp6_connect);
	ATF_TP_ADD_TC(tp, udp6_bind);

	return (atf_no_error());
}
