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
#include <sys/jail.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <ifaddrs.h>
#include <poll.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <atf-c.h>

static const char buf[] = "Hello";

static void
sendtolocalhost(int s)
{
	struct sockaddr_in dst = {
		.sin_family = AF_INET,
		.sin_len = sizeof(struct sockaddr_in),
		.sin_addr = { htonl(INADDR_LOOPBACK) },
		.sin_port = htons(1638),
	};

	ATF_REQUIRE(sendto(s, buf, sizeof(buf), 0, (struct sockaddr *)&dst,
	    sizeof(dst)) == sizeof(buf));
}

/*
 * Echo back to the sender its own address in payload.
 */
static void *
echo(void *arg)
{
	int s = *(int *)arg;
	struct sockaddr_in sin;
	socklen_t slen = sizeof(sin);
	char rbuf[sizeof(buf)];

	ATF_REQUIRE(recvfrom(s, &rbuf, sizeof(rbuf), 0, (struct sockaddr *)&sin,
	    &slen) == sizeof(rbuf));
	printf("Echo to %s:%u\n", inet_ntoa(sin.sin_addr), ntohs(sin.sin_port));
	ATF_REQUIRE(sendto(s, &sin, sizeof(sin), 0, (struct sockaddr *)&sin,
	    sizeof(sin)) == sizeof(sin));
	return (NULL);
}

/*
 * Cycle through local addresses (normally there should be at least two
 * different IPv4 ones), and communicate to the echo server checking both
 * IP_SENDSRCADDR and IP_RECVDSTADDR.  Use same cmsg buffer for both send
 * and receive operation, this is a suggested in manual, given that
 * IP_RECVDSTADDR == IP_SENDSRCADDR.
 * At the setup phase check that IP_SENDSRCADDR doesn't work on unbound socket.
 */
ATF_TC_WITHOUT_HEAD(IP_SENDSRCADDR);
ATF_TC_BODY(IP_SENDSRCADDR, tc)
{
	struct sockaddr_in srv = {
		.sin_family = AF_INET,
		.sin_len = sizeof(struct sockaddr_in),
	}, dst;
	char cbuf[CMSG_SPACE(sizeof(struct in_addr))];
	struct iovec iov = {
		.iov_base = __DECONST(char *, buf),
		.iov_len = sizeof(buf),
	};
	struct iovec riov = {
		.iov_base = &dst,
		.iov_len = sizeof(dst),
	};
	struct msghdr msg = {
		.msg_iov = &iov,
		.msg_iovlen = 1,
		.msg_name = &srv,
		.msg_namelen = sizeof(srv),
		.msg_control = cbuf,
		.msg_controllen = sizeof(cbuf),
	};
	struct msghdr rmsg = {
		.msg_iov = &riov,
		.msg_iovlen = 1,
		.msg_control = cbuf,
		.msg_controllen = sizeof(cbuf),
	};
	struct cmsghdr *cmsg = CMSG_FIRSTHDR(&msg);
	*cmsg = (struct cmsghdr) {
		.cmsg_level = IPPROTO_IP,
		.cmsg_type = IP_SENDSRCADDR,
		.cmsg_len = CMSG_LEN(sizeof(struct in_addr)),
	};
	socklen_t slen = sizeof(struct sockaddr_in);
	struct ifaddrs *ifa0, *ifa;
	pthread_t tid;
	int s, e;

	/* First check that IP_SENDSRCADDR doesn't work on an unbound socket. */
	ATF_REQUIRE((s = socket(PF_INET, SOCK_DGRAM, 0)) > 0);
	ATF_REQUIRE_MSG(sendmsg(s, &msg, 0) == -1 && errno == EINVAL,
	    "sendmsg(.cmsg_type = IP_SENDSRCADDR), errno %d", errno);

	/* Bind to random ports both sender and echo server. */
	ATF_REQUIRE(bind(s, (struct sockaddr *)&srv, sizeof(srv)) == 0);
	ATF_REQUIRE((e = socket(PF_INET, SOCK_DGRAM, 0)) > 0);
	ATF_REQUIRE(bind(e, (struct sockaddr *)&srv, sizeof(srv)) == 0);
	ATF_REQUIRE(getsockname(e, (struct sockaddr *)&srv, &slen) == 0);
	srv.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

	ATF_REQUIRE(getifaddrs(&ifa0) == 0);
	for (ifa = ifa0; ifa != NULL; ifa = ifa->ifa_next) {
		struct sockaddr_in src;
		struct in_addr vrf;

		if (ifa->ifa_addr->sa_family != AF_INET)
			continue;
		memcpy(&src, ifa->ifa_addr, sizeof(src));
		printf("Sending from %s\n", inet_ntoa(src.sin_addr));
		ATF_REQUIRE(pthread_create(&tid, NULL, echo, &e) == 0);
		memcpy(CMSG_DATA(cmsg), &src.sin_addr, sizeof(src.sin_addr));
		ATF_REQUIRE(sendmsg(s, &msg, 0) == sizeof(buf));
		ATF_REQUIRE(recvmsg(s, &rmsg, 0) == sizeof(struct sockaddr_in));
		memcpy(&vrf, CMSG_DATA(cmsg), sizeof(vrf));
		ATF_REQUIRE_MSG(dst.sin_addr.s_addr == src.sin_addr.s_addr,
		    "Sent from %s, but echo server reports %s",
		    inet_ntoa(src.sin_addr), inet_ntoa(dst.sin_addr));
		ATF_REQUIRE_MSG(vrf.s_addr == src.sin_addr.s_addr,
		    "Sent from %s, but IP_RECVDSTADDR reports %s",
		    inet_ntoa(src.sin_addr), inet_ntoa(vrf));
		ATF_REQUIRE(pthread_join(tid, NULL) == 0);
	}

	freeifaddrs(ifa0);
	close(s);
	close(e);
}

/*
 * Check gethostname(2) on a newborn socket, and then on an unconnected, but
 * used socket.  The first shall return all-zeroes, and second one should
 * return us our assigned port.
 */
ATF_TC_WITHOUT_HEAD(gethostname);
ATF_TC_BODY(gethostname, tc)
{
	struct sockaddr_in sin;
	socklen_t slen = sizeof(sin);
	int s;

	ATF_REQUIRE((s = socket(PF_INET, SOCK_DGRAM, 0)) > 0);
	ATF_REQUIRE(getsockname(s, (struct sockaddr *)&sin, &slen) == 0);
	ATF_REQUIRE_MSG(sin.sin_addr.s_addr == INADDR_ANY && sin.sin_port == 0,
	    "newborn socket name %s:%u", inet_ntoa(sin.sin_addr),
	    ntohs(sin.sin_port));
	sendtolocalhost(s);
	ATF_REQUIRE(getsockname(s, (struct sockaddr *)&sin, &slen) == 0);
	ATF_REQUIRE_MSG(sin.sin_addr.s_addr == INADDR_ANY && sin.sin_port != 0,
	    "used unconnected socket name %s:%u", inet_ntoa(sin.sin_addr),
	    ntohs(sin.sin_port));
	close(s);
}

ATF_TC_WITHOUT_HEAD(gethostname_jailed);
ATF_TC_BODY(gethostname_jailed, tc)
{
	struct in_addr laddr = { htonl(INADDR_LOOPBACK) };
	struct jail jconf = {
		.version = JAIL_API_VERSION,
		.path = __DECONST(char *, "/"),
		.hostname = __DECONST(char *,"test"),
		.ip4s = 1,
		.ip4 = &laddr,
	};
	struct sockaddr_in sin;
	socklen_t slen = sizeof(sin);
	int s;

	ATF_REQUIRE(jail(&jconf) > 0);
	ATF_REQUIRE((s = socket(PF_INET, SOCK_DGRAM, 0)) > 0);
	sendtolocalhost(s);
	ATF_REQUIRE(getsockname(s, (struct sockaddr *)&sin, &slen) == 0);
	ATF_REQUIRE_MSG(sin.sin_addr.s_addr == laddr.s_addr &&
	    sin.sin_port != 0,
	    "jailed unconnected socket name %s:%u", inet_ntoa(sin.sin_addr),
	    ntohs(sin.sin_port));
	close(s);
}

ATF_TC_WITHOUT_HEAD(connect_bound_socket_from_2address_jail);
ATF_TC_BODY(connect_bound_socket_from_2address_jail, tc)
{
	struct in_addr addrs[] = {
	    { htonl(0xc0000202) }, { htonl(0xc0000203) }
	};
	struct jail jconf = {
	    .version = JAIL_API_VERSION,
	    .path = __DECONST(char *, "/"),
	    .hostname = __DECONST(char *,"test"),
	    .ip4s = nitems(addrs),
	    .ip4 = addrs,
	};
	struct sockaddr_in sin = { .sin_family = AF_INET };
	socklen_t slen = sizeof(sin);
	int s;

	ATF_REQUIRE(jail(&jconf) > 0);
	ATF_REQUIRE((s = socket(PF_INET, SOCK_DGRAM, 0)) > 0);
	ATF_REQUIRE(bind(s, (struct sockaddr *)&sin, sizeof(sin)) == 0);
	ATF_REQUIRE(getsockname(s, (struct sockaddr *)&sin, &slen) == 0);
	ATF_REQUIRE_MSG(sin.sin_addr.s_addr == INADDR_ANY && sin.sin_port != 0,
	    "jailed unconnected socket name %s:%u", inet_ntoa(sin.sin_addr),
	    ntohs(sin.sin_port));
	sin.sin_addr.s_addr = htonl(0xc0000204);
	sin.sin_port = htons(6666);
	ATF_REQUIRE(connect(s, (struct sockaddr *)&sin, sizeof(sin)) == 0);
	close(s);
}

/*
 * See bug 274009.
 */
ATF_TC_WITHOUT_HEAD(v4mapped);
ATF_TC_BODY(v4mapped, tc)
{
	struct sockaddr_in6 sa6 = {
		.sin6_family = AF_INET6,
		.sin6_len = sizeof(struct sockaddr_in6),
		.sin6_port = htons(1),
	};
	int s;

	ATF_REQUIRE((s = socket(PF_INET6, SOCK_DGRAM, 0)) > 0);
	ATF_REQUIRE(setsockopt(s, IPPROTO_IPV6, IPV6_V6ONLY, &(int){0},
	    sizeof(int)) == 0);
	ATF_REQUIRE(inet_pton(AF_INET6, "::ffff:127.0.0.1", &(sa6.sin6_addr))
	    == 1);
	ATF_REQUIRE(sendto(s, buf, sizeof(buf), 0, (struct sockaddr *)&sa6,
	    sizeof(sa6)) == sizeof(buf));
	close(s);
}

/*
 * An IPv4 wildcard socket takes precedence over a later dual-stack wildcard
 * socket, even when the IPv4 PCB lookup uses the SMR path.
 */
ATF_TC_WITHOUT_HEAD(dualstack_wildcard_precedence);
ATF_TC_BODY(dualstack_wildcard_precedence, tc)
{
	struct sockaddr_in sin = {
		.sin_family = AF_INET,
		.sin_len = sizeof(sin),
		.sin_addr = { htonl(INADDR_ANY) },
	};
	struct sockaddr_in6 sin6 = {
		.sin6_family = AF_INET6,
		.sin6_len = sizeof(sin6),
		.sin6_addr = IN6ADDR_ANY_INIT,
	};
	struct pollfd pfds[2] = {0};
	socklen_t slen;
	char ch;
	int error, one, s4, s6, tx;

	one = 1;
	s4 = socket(PF_INET, SOCK_DGRAM | SOCK_NONBLOCK, 0);
	ATF_REQUIRE_MSG(s4 >= 0, "socket(AF_INET) failed: %s",
	    strerror(errno));
	error = setsockopt(s4, SOL_SOCKET, SO_REUSEPORT, &one, sizeof(one));
	ATF_REQUIRE_MSG(error == 0, "setsockopt(SO_REUSEPORT) failed: %s",
	    strerror(errno));
	error = bind(s4, (struct sockaddr *)&sin, sizeof(sin));
	ATF_REQUIRE_MSG(error == 0, "bind(AF_INET) failed: %s",
	    strerror(errno));
	slen = sizeof(sin);
	error = getsockname(s4, (struct sockaddr *)&sin, &slen);
	ATF_REQUIRE_MSG(error == 0, "getsockname(AF_INET) failed: %s",
	    strerror(errno));

	s6 = socket(PF_INET6, SOCK_DGRAM | SOCK_NONBLOCK, 0);
	ATF_REQUIRE_MSG(s6 >= 0, "socket(AF_INET6) failed: %s",
	    strerror(errno));
	error = setsockopt(s6, SOL_SOCKET, SO_REUSEPORT, &one, sizeof(one));
	ATF_REQUIRE_MSG(error == 0, "setsockopt(SO_REUSEPORT) failed: %s",
	    strerror(errno));
	error = setsockopt(s6, IPPROTO_IPV6, IPV6_V6ONLY, &(int){0},
	    sizeof(int));
	ATF_REQUIRE_MSG(error == 0, "setsockopt(IPV6_V6ONLY) failed: %s",
	    strerror(errno));
	sin6.sin6_port = sin.sin_port;
	error = bind(s6, (struct sockaddr *)&sin6, sizeof(sin6));
	ATF_REQUIRE_MSG(error == 0, "bind(AF_INET6) failed: %s",
	    strerror(errno));

	sin.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	tx = socket(PF_INET, SOCK_DGRAM, 0);
	ATF_REQUIRE_MSG(tx >= 0, "socket(sender) failed: %s", strerror(errno));
	ch = 'x';
	ATF_REQUIRE_MSG(sendto(tx, &ch, sizeof(ch), 0,
	    (struct sockaddr *)&sin, sizeof(sin)) == sizeof(ch),
	    "sendto() failed: %s", strerror(errno));
	pfds[0] = (struct pollfd){ .fd = s4, .events = POLLIN };
	pfds[1] = (struct pollfd){ .fd = s6, .events = POLLIN };
	error = poll(pfds, 2, 1000);
	ATF_REQUIRE_MSG(error == 1 && (pfds[0].revents & POLLIN) != 0 &&
	    (pfds[1].revents & POLLIN) == 0,
	    "unexpected wildcard socket selection: revents 0x%x, 0x%x",
	    pfds[0].revents, pfds[1].revents);
	ATF_REQUIRE_MSG(recv(s4, &ch, sizeof(ch), 0) == sizeof(ch),
	    "IPv4 wildcard socket did not receive the datagram: %s",
	    strerror(errno));

	ATF_REQUIRE(close(tx) == 0);
	ATF_REQUIRE(close(s6) == 0);
	ATF_REQUIRE(close(s4) == 0);
}

/*
 * A failed connect to a v4-mapped address must leave a regular dual-stack UDP
 * socket able to connect to an IPv6 peer.
 */
ATF_TC_WITHOUT_HEAD(dualstack_connect_fail);
ATF_TC_BODY(dualstack_connect_fail, tc)
{
	struct sockaddr_in6 peer4 = {
		.sin6_family = AF_INET6,
		.sin6_len = sizeof(peer4),
		.sin6_port = htons(9),
		.sin6_addr = IN6ADDR_ANY_INIT,
	};
	struct sockaddr_in6 peer6 = {
		.sin6_family = AF_INET6,
		.sin6_len = sizeof(peer6),
		.sin6_port = htons(9),
		.sin6_addr = IN6ADDR_LOOPBACK_INIT,
	};
	struct sockaddr_in6 local6 = {
		.sin6_family = AF_INET6,
		.sin6_len = sizeof(local6),
		.sin6_addr = IN6ADDR_ANY_INIT,
	};
	int error, s;

	peer4.sin6_addr.s6_addr[10] = peer4.sin6_addr.s6_addr[11] = 0xff;
	memset(&peer4.sin6_addr.s6_addr[12], 0xff, 4);
	s = socket(PF_INET6, SOCK_DGRAM, 0);
	ATF_REQUIRE_MSG(s >= 0, "socket() failed: %s", strerror(errno));
	error = setsockopt(s, IPPROTO_IPV6, IPV6_V6ONLY, &(int){0},
	    sizeof(int));
	ATF_REQUIRE_MSG(error == 0, "setsockopt(IPV6_V6ONLY) failed: %s",
	    strerror(errno));
	error = bind(s, (struct sockaddr *)&local6, sizeof(local6));
	ATF_REQUIRE_MSG(error == 0, "bind() failed: %s", strerror(errno));
	error = connect(s, (struct sockaddr *)&peer4, sizeof(peer4));
	ATF_REQUIRE_MSG(error == -1 && errno == ENETUNREACH,
	    "connect(v4-mapped broadcast) returned %d: %s",
	    error, strerror(errno));
	error = connect(s, (struct sockaddr *)&peer6, sizeof(peer6));
	ATF_REQUIRE_MSG(error == 0, "connect(IPv6) after failure failed: %s",
	    strerror(errno));
	ATF_REQUIRE(close(s) == 0);
}

ATF_TP_ADD_TCS(tp)
{
	ATF_TP_ADD_TC(tp, v4mapped);
	ATF_TP_ADD_TC(tp, dualstack_wildcard_precedence);
	ATF_TP_ADD_TC(tp, dualstack_connect_fail);
	ATF_TP_ADD_TC(tp, gethostname);
	ATF_TP_ADD_TC(tp, gethostname_jailed);
	ATF_TP_ADD_TC(tp, connect_bound_socket_from_2address_jail);
	ATF_TP_ADD_TC(tp, IP_SENDSRCADDR);

	return (atf_no_error());
}
