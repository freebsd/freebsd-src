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

#include <sys/param.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netinet/ip.h>
#include <net/if.h>
#include <errno.h>
#include <stdlib.h>

#include <atf-c.h>

#define	PROT1		253 /* RFC3692 */
#define	PROT2		254 /* RFC3692 */
#define	ADDR1		{ htonl(0xc0000202) }	/* RFC5737 */
#define	ADDR2		{ htonl(0xc0000203) }	/* RFC5737 */
#define	WILD		{ htonl(INADDR_ANY) }
#define	LOOP(x)		{ htonl(INADDR_LOOPBACK + (x)) }
#define	MULT(x)		{ htonl(INADDR_UNSPEC_GROUP + (x)) }

static int
rawsender(bool mcast)
{
	int s;

	ATF_REQUIRE((s = socket(PF_INET, SOCK_RAW, 0)) != -1);
	ATF_REQUIRE(setsockopt(s, IPPROTO_IP, IP_HDRINCL, &(int){1},
	    sizeof(int)) == 0);
	/*
	 * Make sending socket connected.  The socket API requires connected
	 * status to use send(2), even with IP_HDRINCL.
	 */
	ATF_REQUIRE(connect(s,
	    (struct sockaddr *)&(struct sockaddr_in){
		.sin_family = AF_INET,
		.sin_len = sizeof(struct sockaddr_in),
		.sin_addr = { htonl(INADDR_ANY) },
	    }, sizeof(struct sockaddr_in)) == 0);

	if (mcast)
		ATF_REQUIRE(setsockopt(s, IPPROTO_IP, IP_MULTICAST_IF,
		    &(struct ip_mreqn){
			.imr_ifindex = if_nametoindex("lo0"),
		    }, sizeof(struct ip_mreqn)) == 0);

	return (s);
}

/*
 * The 'input' test exercises logic of rip_input().  The best documentation
 * for raw socket input behavior is collected in Stevens's UNIX Network
 * Programming, Section 28.4.  We create several sockets, with different
 * remote and local bindings, as well as a socket with multicast membership
 * and then we send different packets and see which sockets received their
 * copy.
 * The table tests[] describes our expectations.
 */
ATF_TC_WITHOUT_HEAD(input);
ATF_TC_BODY(input, tc)
{
	static const struct rcvr {
		struct in_addr laddr, faddr, maddr;
		uint8_t	proto;
	} rcvrs[] = {
		{ WILD,	   WILD,    WILD,    0 },
		{ WILD,    WILD,    WILD,    PROT1 },
		{ LOOP(0), WILD,    WILD,    0 },
		{ LOOP(0), WILD,    WILD,    PROT1 },
		{ LOOP(1), WILD,    WILD,    0 },
		{ LOOP(1), WILD,    WILD,    PROT1 },
		{ LOOP(0), LOOP(2), WILD,    0 },
		{ LOOP(0), LOOP(2), WILD,    PROT1 },
		{ LOOP(0), LOOP(3), WILD,    0 },
		{ LOOP(0), LOOP(3), WILD,    PROT1 },
		{ LOOP(1), LOOP(3), WILD,    0 },
		{ LOOP(1), LOOP(3), WILD,    PROT1 },
		{ WILD,	   WILD,    MULT(1), 0 },
	};
	static const struct test {
		struct in_addr src, dst;
		uint8_t proto;
		bool results[nitems(rcvrs)];
	} tests[] = {
#define	x true
#define	o false
		{ LOOP(2), LOOP(0), PROT1,
		  { x, x, x, x, o, o, x, x, o, o, o, o, x } },
		{ LOOP(2), LOOP(0), PROT2,
		  { x, o, x, o, o, o, x, o, o, o, o, o, x } },
		{ LOOP(3), LOOP(0), PROT1,
		  { x, x, x, x, o, o, o, o, x, x, o, o, x } },
		{ LOOP(3), LOOP(0), PROT2,
		  { x, o, x, o, o, o, o, o, x, o, o, o, x } },
		{ LOOP(2), LOOP(1), PROT1,
		  { x, x, o, o, x, x, o, o, o, o, o, o, x } },
		{ LOOP(2), LOOP(1), PROT2,
		  { x, o, o, o, x, o, o, o, o, o, o, o, x } },
		{ LOOP(3), LOOP(1), PROT1,
		  { x, x, o, o, x, x, o, o, o, o, x, x, x } },
		{ LOOP(3), LOOP(1), PROT2,
		  { x, o, o, o, x, o, o, o, o, o, x, o, x } },
		{ LOOP(3), MULT(1), PROT1,
		  { x, x, o, o, o, o, o, o, o, o, o, o, x } },
		{ LOOP(3), MULT(2), PROT1,
		  { x, x, o, o, o, o, o, o, o, o, o, o, o } },
#undef x
#undef o
	};
	struct pkt {
		struct ip ip;
		char payload[100];
	} __packed pkt = {
		.ip.ip_v = IPVERSION,
		.ip.ip_hl = sizeof(struct ip) >> 2,
		.ip.ip_len = htons(sizeof(struct pkt)),
		.ip.ip_ttl = 16,
	};
	struct sockaddr_in sin = {
		.sin_family = AF_INET,
		.sin_len = sizeof(sin),
	};
	struct ip_mreqn mreqn = {
		.imr_ifindex = if_nametoindex("lo0"),
	};
	int r[nitems(rcvrs)];
	int s;

	/*
	 * This XXX to be removed when kyua provides generic framework for
	 * constructing test jail environments.
	 */
	system("/sbin/ifconfig lo0 127.0.0.1/32");
	system("/sbin/ifconfig lo0 127.0.0.2/32 alias");

	for (u_int i = 0; i < nitems(rcvrs); i++) {
		/*
		 * To avoid a race between send(2) and packet queueing in
		 * netisr(9) and our recv(2), set the very first receiver
		 * socket to blocking mode.  Note in the above table that first
		 * receiver is supposed to receive something in every test.
		 */
		ATF_REQUIRE((r[i] = socket(PF_INET, SOCK_RAW |
		    (i != 0 ? SOCK_NONBLOCK : 0),
		    rcvrs[i].proto)) != -1);
		if (rcvrs[i].laddr.s_addr != htonl(INADDR_ANY)) {
			sin.sin_addr = rcvrs[i].laddr;
			ATF_REQUIRE(bind(r[i], (struct sockaddr *)&sin,
			    sizeof(sin)) == 0);
		}
		if (rcvrs[i].faddr.s_addr != htonl(INADDR_ANY)) {
			sin.sin_addr = rcvrs[i].faddr;
			ATF_REQUIRE(connect(r[i], (struct sockaddr *)&sin,
			    sizeof(sin)) == 0);
		}
		if (rcvrs[i].maddr.s_addr != htonl(INADDR_ANY)) {
			mreqn.imr_multiaddr = rcvrs[i].maddr;
			ATF_REQUIRE(setsockopt(r[i], IPPROTO_IP,
			    IP_ADD_MEMBERSHIP, &mreqn, sizeof(mreqn)) == 0);
		}
	}

	/*
	 * Force multicast interface for the sending socket to be able to
	 * send to MULT(x) destinations.
	 */
	s = rawsender(true);

	for (u_int i = 0; i < nitems(tests); i++) {
		arc4random_buf(&pkt.payload, sizeof(pkt.payload));
		pkt.ip.ip_src = tests[i].src;
		pkt.ip.ip_dst = tests[i].dst;
		pkt.ip.ip_p = tests[i].proto;
		ATF_REQUIRE(send(s, &pkt, sizeof(pkt), 0) == sizeof(pkt));
		for (u_int j = 0; j < nitems(rcvrs); j++) {
			char buf[sizeof(pkt)];
			char p[4][INET_ADDRSTRLEN];
			ssize_t ss;

			ss = recv(r[j], buf, sizeof(buf), 0);

			ATF_REQUIRE_MSG((tests[i].results[j] == true &&
			    ss == sizeof(buf) && memcmp(buf + sizeof(struct ip),
			    pkt.payload, sizeof(pkt.payload)) == 0) ||
			    (tests[i].results[j] == false &&
			    ss == -1 && errno == EAGAIN),
			    "test #%u %s->%s %u unexpected receive of %zd "
			     "bytes errno %d on socket #%u %s->%s %u", i,
			    inet_ntop(AF_INET, &tests[i].src, p[0],
				INET_ADDRSTRLEN),
			    inet_ntop(AF_INET, &tests[i].dst, p[1],
				INET_ADDRSTRLEN),
			    tests[i].proto, ss, errno, j,
			    inet_ntop(AF_INET, &rcvrs[j].faddr, p[2],
				INET_ADDRSTRLEN),
			    inet_ntop(AF_INET, &rcvrs[j].laddr, p[3],
				INET_ADDRSTRLEN),
			    rcvrs[j].proto);
		}
	}
}

/*
 * Test input on the same socket that changes its connection status.  We send
 * packets with different sources in each iteration and check results.
 * Check that connect(INADDR_ANY) is effectively a disconnect and turns socket
 * back to receive-all mode.
 */
ATF_TC_WITHOUT_HEAD(reconnect);
ATF_TC_BODY(reconnect, tc)
{
	static const struct in_addr srcs[] = { ADDR1, ADDR2 };
	static const struct test {
		struct in_addr faddr;
		bool results[nitems(srcs)];
	} tests[] = {
		{ ADDR1,	{ true, false } },
		{ ADDR2,	{ false, true } },
		{ {INADDR_ANY},	{ true, true } },
	};
	struct pkt {
		struct ip ip;
		char payload[100];
	} __packed pkt = {
		.ip.ip_v = IPVERSION,
		.ip.ip_hl = sizeof(struct ip) >> 2,
		.ip.ip_len = htons(sizeof(struct pkt)),
		.ip.ip_ttl = 16,
		.ip.ip_p = PROT1,
		.ip.ip_dst = LOOP(0),
	};
	int r, s;

	/* XXX */
	system("/sbin/ifconfig lo0 127.0.0.1/32");

	ATF_REQUIRE((r = socket(PF_INET, SOCK_RAW | SOCK_NONBLOCK, 0)) != -1);
	s = rawsender(false);

	for (u_int i = 0; i < nitems(tests); i++) {
		ATF_REQUIRE(connect(r,
		    (struct sockaddr *)&(struct sockaddr_in){
			.sin_family = AF_INET,
			.sin_len = sizeof(struct sockaddr_in),
			.sin_addr = tests[i].faddr,
		    }, sizeof(struct sockaddr_in)) == 0);

		for (u_int j = 0; j < nitems(srcs); j++) {
			char buf[sizeof(pkt)];
			char p[2][INET_ADDRSTRLEN];
			ssize_t ss;

			arc4random_buf(&pkt.payload, sizeof(pkt.payload));
			pkt.ip.ip_src = srcs[j];
			ATF_REQUIRE(send(s, &pkt, sizeof(pkt), 0) ==
			    sizeof(pkt));

			/*
			 * The sender is a blocking socket, so we first receive
			 * from the sender and when this read returns we are
			 * guaranteed that the test socket also received the
			 * datagram.
			 */
			ss = recv(s, buf, sizeof(buf), 0);
			ATF_REQUIRE(ss == sizeof(buf) &&
			    memcmp(buf + sizeof(struct ip),
			    pkt.payload, sizeof(pkt.payload)) == 0);

			ss = recv(r, buf, sizeof(buf), 0);

			ATF_REQUIRE_MSG((tests[i].results[j] == true &&
			    ss == sizeof(buf) && memcmp(buf + sizeof(struct ip),
			    pkt.payload, sizeof(pkt.payload)) == 0) ||
			    (tests[i].results[j] == false && ss == -1 &&
			    errno == EAGAIN),
			    "test #%u src %s connect address %s unexpected "
			    "receive of %zd bytes errno %d", i,
			    inet_ntop(AF_INET, &srcs[j], p[0],
				INET_ADDRSTRLEN),
			    inet_ntop(AF_INET, &tests[i].faddr, p[1],
				INET_ADDRSTRLEN),
			    ss, errno);
		}
	}
}

ATF_TP_ADD_TCS(tp)
{
	ATF_TP_ADD_TC(tp, input);
	ATF_TP_ADD_TC(tp, reconnect);

	return (atf_no_error());
}
