/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2024-2025 Stormshield
 */

#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/sysctl.h>

#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/ip6.h>
#include <netinet/ip_icmp.h>
#include <netinet/icmp6.h>

#include <errno.h>
#include <fcntl.h>
#include <pwd.h>
#include <stdio.h>
#include <string.h>

#include <atf-c.h>

#define	MAKETEST_TCP(name)			\
ATF_TC_WITHOUT_HEAD(name ## _tcp);		\
ATF_TC_BODY(name ## _tcp, tc)			\
{						\
	name(PF_INET, SOCK_STREAM, tc);		\
}						\
ATF_TC_WITHOUT_HEAD(name ## _tcp6);		\
ATF_TC_BODY(name ## _tcp6, tc)			\
{						\
	name(PF_INET6, SOCK_STREAM, tc);	\
}
#define	MAKETEST_UDP(name)			\
ATF_TC_WITHOUT_HEAD(name ## _udp);		\
ATF_TC_BODY(name ## _udp, tc)			\
{						\
	name(PF_INET, SOCK_DGRAM, tc);		\
}						\
ATF_TC_WITHOUT_HEAD(name ## _udp6);		\
ATF_TC_BODY(name ## _udp6, tc)			\
{						\
	name(PF_INET6, SOCK_DGRAM, tc);		\
}
#define	MAKETEST_RAW(name)			\
ATF_TC(name ## _raw);				\
ATF_TC_HEAD(name ## _raw, tc)			\
{						\
	atf_tc_set_md_var(tc, "require.user",	\
	    "root");				\
}						\
ATF_TC_BODY(name ## _raw, tc)			\
{						\
	name(PF_INET, SOCK_RAW, tc);		\
}						\
ATF_TC(name ## _raw6);				\
ATF_TC_HEAD(name ## _raw6, tc)			\
{						\
	atf_tc_set_md_var(tc, "require.user",	\
	    "root");				\
}						\
ATF_TC_BODY(name ## _raw6, tc)			\
{						\
	name(PF_INET6, SOCK_RAW, tc);		\
}

#define	MAKETEST(name)				\
	MAKETEST_TCP(name)			\
	MAKETEST_UDP(name)

#define	LISTTEST_TCP(name)			\
	ATF_TP_ADD_TC(tp, name ## _tcp);	\
	ATF_TP_ADD_TC(tp, name ## _tcp6);
#define	LISTTEST_UDP(name)			\
	ATF_TP_ADD_TC(tp, name ## _udp);	\
	ATF_TP_ADD_TC(tp, name ## _udp6);
#define	LISTTEST_RAW(name)			\
	ATF_TP_ADD_TC(tp, name ## _raw);	\
	ATF_TP_ADD_TC(tp, name ## _raw6);
#define	LISTTEST(name)				\
	LISTTEST_TCP(name)			\
	LISTTEST_UDP(name)

static void
checked_close(int s)
{
	int error;

	error = close(s);
	ATF_REQUIRE_MSG(error == 0, "close failed: %s", strerror(errno));
}

static int
mksockp(int domain, int type, int fib, int proto)
{
	int error, s;

	s = socket(domain, type, proto);
	ATF_REQUIRE(s != -1);
	error = setsockopt(s, SOL_SOCKET, SO_SETFIB, &fib, sizeof(fib));
	ATF_REQUIRE_MSG(error == 0, "setsockopt failed: %s", strerror(errno));

	return (s);
}

static int
mksock(int domain, int type, int fib)
{
	return (mksockp(domain, type, fib, 0));
}

static void
require_fibs_multibind(int socktype, int minfibs)
{
	const char *sysctl;
	size_t sz;
	int error, fibs, multibind;

	fibs = 0;
	sz = sizeof(fibs);
	error = sysctlbyname("net.fibs", &fibs, &sz, NULL, 0);
	ATF_REQUIRE_MSG(error == 0, "sysctlbyname failed: %s", strerror(errno));
	ATF_REQUIRE_MSG(fibs >= 1, "strange FIB count %d", fibs);
	if (fibs == 1)
		atf_tc_skip("multiple FIBs not enabled");
	if (fibs < minfibs)
		atf_tc_skip("not enough FIBs, need %d", minfibs);

	switch (socktype) {
	case SOCK_STREAM:
		sysctl = "net.inet.tcp.bind_all_fibs";
		break;
	case SOCK_DGRAM:
		sysctl = "net.inet.udp.bind_all_fibs";
		break;
	case SOCK_RAW:
		sysctl = "net.inet.raw.bind_all_fibs";
		break;
	default:
		atf_tc_fail("unknown socket type %d", socktype);
		break;
	}

	multibind = -1;
	sz = sizeof(multibind);
	error = sysctlbyname(sysctl, &multibind, &sz, NULL, 0);
	ATF_REQUIRE_MSG(error == 0, "sysctlbyname failed: %s", strerror(errno));
	if (multibind != 0)
		atf_tc_skip("FIB multibind not configured (%s)", sysctl);
}

/*
 * Make sure that different users can't bind to the same port from different
 * FIBs.
 */
static void
multibind_different_user(int domain, int type, const atf_tc_t *tc)
{
	struct sockaddr_storage ss;
	struct passwd *passwd;
	const char *user;
	socklen_t sslen;
	int error, s[2];

	if (geteuid() != 0)
		atf_tc_skip("need root privileges");
	if (!atf_tc_has_config_var(tc, "unprivileged_user"))
		atf_tc_skip("unprivileged_user not set");

	ATF_REQUIRE(domain == PF_INET || domain == PF_INET6);
	sslen = domain == PF_INET ? sizeof(struct sockaddr_in) :
	    sizeof(struct sockaddr_in6);

	require_fibs_multibind(type, 2);

	s[0] = mksock(domain, type, 0);

	memset(&ss, 0, sizeof(ss));
	ss.ss_family = domain;
	ss.ss_len = sslen;
	error = bind(s[0], (struct sockaddr *)&ss, sslen);
	ATF_REQUIRE_MSG(error == 0, "bind failed: %s", strerror(errno));

	error = getsockname(s[0], (struct sockaddr *)&ss, &sslen);
	ATF_REQUIRE_MSG(error == 0, "getsockname failed: %s", strerror(errno));

	/*
	 * Create a second socket in a different FIB, and bind it to the same
	 * address/port tuple.  This should succeed if done as the same user as
	 * the first socket, and should fail otherwise.
	 */
	s[1] = mksock(domain, type, 1);
	error = bind(s[1], (struct sockaddr *)&ss, sslen);
	ATF_REQUIRE_MSG(error == 0, "bind failed: %s", strerror(errno));
	ATF_REQUIRE_MSG(close(s[1]) == 0, "close failed: %s", strerror(errno));

	user = atf_tc_get_config_var(tc, "unprivileged_user");
	passwd = getpwnam(user);
	ATF_REQUIRE(passwd != NULL);
	error = seteuid(passwd->pw_uid);
	ATF_REQUIRE_MSG(error == 0, "seteuid failed: %s", strerror(errno));

	/* Repeat the bind as a different user. */
	s[1] = mksock(domain, type, 1);
	error = bind(s[1], (struct sockaddr *)&ss, sslen);
	ATF_REQUIRE_ERRNO(EADDRINUSE, error == -1);
	ATF_REQUIRE_MSG(close(s[1]) == 0, "close failed: %s", strerror(errno));
}
MAKETEST(multibind_different_user);

/*
 * Verify that a listening socket only accepts connections originating from the
 * same FIB.
 */
static void
per_fib_listening_socket(int domain, int type, const atf_tc_t *tc __unused)
{
	struct sockaddr_storage ss;
	socklen_t sslen;
	int cs1, cs2, error, fib1, fib2, ls1, ls2, ns;

	ATF_REQUIRE(type == SOCK_STREAM);
	ATF_REQUIRE(domain == PF_INET || domain == PF_INET6);
	require_fibs_multibind(type, 2);

	fib1 = 0;
	fib2 = 1;

	ls1 = mksock(domain, type, fib1);
	ls2 = mksock(domain, type, fib2);

	sslen = domain == PF_INET ? sizeof(struct sockaddr_in) :
	    sizeof(struct sockaddr_in6);

	memset(&ss, 0, sizeof(ss));
	ss.ss_family = domain;
	ss.ss_len = sslen;
	error = bind(ls1, (struct sockaddr *)&ss, sslen);
	ATF_REQUIRE_MSG(error == 0, "bind failed: %s", strerror(errno));

	error = getsockname(ls1, (struct sockaddr *)&ss, &sslen);
	ATF_REQUIRE_MSG(error == 0, "getsockname failed: %s", strerror(errno));

	error = listen(ls1, 5);
	ATF_REQUIRE_MSG(error == 0, "listen failed: %s", strerror(errno));

	cs1 = mksock(domain, type, fib1);
	cs2 = mksock(domain, type, fib2);

	/*
	 * Make sure we can connect from the same FIB.
	 */
	error = connect(cs1, (struct sockaddr *)&ss, sslen);
	ATF_REQUIRE_MSG(error == 0, "connect failed: %s", strerror(errno));
	ns = accept(ls1, NULL, NULL);
	ATF_REQUIRE_MSG(ns != -1, "accept failed: %s", strerror(errno));
	checked_close(ns);
	checked_close(cs1);
	cs1 = mksock(domain, type, fib1);

	/*
	 * ... but not from a different FIB.
	 */
	error = connect(cs2, (struct sockaddr *)&ss, sslen);
	ATF_REQUIRE_MSG(error == -1, "connect succeeded unexpectedly");
	ATF_REQUIRE_MSG(errno == ECONNREFUSED, "unexpected error %d", errno);
	checked_close(cs2);
	cs2 = mksock(domain, type, fib2);

	/*
	 * ... but if there are multiple listening sockets, we always connect to
	 * the same FIB.
	 */
	error = bind(ls2, (struct sockaddr *)&ss, sslen);
	ATF_REQUIRE_MSG(error == 0, "bind failed: %s", strerror(errno));
	error = listen(ls2, 5);
	ATF_REQUIRE_MSG(error == 0, "listen failed: %s", strerror(errno));

	for (int i = 0; i < 10; i++) {
		error = connect(cs1, (struct sockaddr *)&ss, sslen);
		ATF_REQUIRE_MSG(error == 0, "connect failed: %s",
		    strerror(errno));
		ns = accept(ls1, NULL, NULL);
		ATF_REQUIRE_MSG(ns != -1, "accept failed: %s", strerror(errno));

		checked_close(ns);
		checked_close(cs1);
		cs1 = mksock(domain, type, fib1);
	}
	for (int i = 0; i < 10; i++) {
		error = connect(cs2, (struct sockaddr *)&ss, sslen);
		ATF_REQUIRE_MSG(error == 0, "connect failed: %s",
		    strerror(errno));
		ns = accept(ls2, NULL, NULL);
		ATF_REQUIRE_MSG(ns != -1, "accept failed: %s", strerror(errno));

		checked_close(ns);
		checked_close(cs2);
		cs2 = mksock(domain, type, fib2);
	}

	/*
	 * ... and if we close one of the listening sockets, we're back to only
	 * being able to connect from the same FIB.
	 */
	checked_close(ls1);
	error = connect(cs1, (struct sockaddr *)&ss, sslen);
	ATF_REQUIRE_MSG(error == -1, "connect succeeded unexpectedly");
	ATF_REQUIRE_MSG(errno == ECONNREFUSED, "unexpected error %d", errno);
	checked_close(cs1);

	error = connect(cs2, (struct sockaddr *)&ss, sslen);
	ATF_REQUIRE_MSG(error == 0, "connect failed: %s", strerror(errno));
	ns = accept(ls2, NULL, NULL);
	ATF_REQUIRE_MSG(ns != -1, "accept failed: %s", strerror(errno));
	checked_close(ns);
	checked_close(cs2);
	checked_close(ls2);
}
MAKETEST_TCP(per_fib_listening_socket);

/*
 * Verify that a bound datagram socket only accepts data from the same FIB.
 */
static void
per_fib_dgram_socket(int domain, int type, const atf_tc_t *tc __unused)
{
	struct sockaddr_storage ss;
	struct sockaddr_in6 *sin6p;
	socklen_t sslen;
	ssize_t n;
	int error, cs1, cs2, fib1, fib2, ss1, ss2;
	char b;

	ATF_REQUIRE(type == SOCK_DGRAM);
	ATF_REQUIRE(domain == PF_INET || domain == PF_INET6);
	require_fibs_multibind(type, 2);

	fib1 = 0;
	fib2 = 1;

	cs1 = mksock(domain, type, fib1);
	cs2 = mksock(domain, type, fib2);

	ss1 = mksock(domain, type, fib1);
	ss2 = mksock(domain, type, fib2);

	sslen = domain == PF_INET ? sizeof(struct sockaddr_in) :
	    sizeof(struct sockaddr_in6);

	memset(&ss, 0, sizeof(ss));
	ss.ss_family = domain;
	ss.ss_len = sslen;
	error = bind(ss1, (struct sockaddr *)&ss, sslen);
	ATF_REQUIRE_MSG(error == 0, "bind failed: %s", strerror(errno));

	error = getsockname(ss1, (struct sockaddr *)&ss, &sslen);
	ATF_REQUIRE_MSG(error == 0, "getsockname failed: %s", strerror(errno));

	if (domain == PF_INET6) {
		sin6p = (struct sockaddr_in6 *)&ss;
		sin6p->sin6_addr = in6addr_loopback;
	}

	/* If we send a byte from cs1, it should be recieved by ss1. */
	b = 42;
	n = sendto(cs1, &b, sizeof(b), 0, (struct sockaddr *)&ss, sslen);
	ATF_REQUIRE_MSG(n == 1, "sendto failed: %s", strerror(errno));
	n = recv(ss1, &b, sizeof(b), 0);
	ATF_REQUIRE(n == 1);
	ATF_REQUIRE(b == 42);

	/* If we send a byte from cs2, it should not be received by ss1. */
	b = 42;
	n = sendto(cs2, &b, sizeof(b), 0, (struct sockaddr *)&ss, sslen);
	ATF_REQUIRE_MSG(n == 1, "sendto failed: %s", strerror(errno));
	usleep(10000);
	n = recv(ss1, &b, sizeof(b), MSG_DONTWAIT);
	ATF_REQUIRE_ERRNO(EWOULDBLOCK, n == -1);

	error = bind(ss2, (struct sockaddr *)&ss, sslen);
	ATF_REQUIRE_MSG(error == 0, "bind failed: %s", strerror(errno));

	/* Repeat now that ss2 is bound. */
	b = 42;
	n = sendto(cs1, &b, sizeof(b), 0, (struct sockaddr *)&ss, sslen);
	ATF_REQUIRE_MSG(n == 1, "sendto failed: %s", strerror(errno));
	n = recv(ss1, &b, sizeof(b), 0);
	ATF_REQUIRE(n == 1);
	ATF_REQUIRE(b == 42);

	b = 42;
	n = sendto(cs2, &b, sizeof(b), 0, (struct sockaddr *)&ss, sslen);
	ATF_REQUIRE_MSG(n == 1, "sendto failed: %s", strerror(errno));
	n = recv(ss2, &b, sizeof(b), 0);
	ATF_REQUIRE(n == 1);
	ATF_REQUIRE(b == 42);

	checked_close(ss1);
	checked_close(ss2);
	checked_close(cs1);
	checked_close(cs2);
}
MAKETEST_UDP(per_fib_dgram_socket);

static size_t
ping(int s, const struct sockaddr *sa, socklen_t salen)
{
	struct {
		struct icmphdr icmp;
		char data[64];
	} icmp;
	ssize_t n;

	memset(&icmp, 0, sizeof(icmp));
	icmp.icmp.icmp_type = ICMP_ECHO;
	icmp.icmp.icmp_code = 0;
	icmp.icmp.icmp_cksum = htons((unsigned short)~(ICMP_ECHO << 8));
	n = sendto(s, &icmp, sizeof(icmp), 0, sa, salen);
	ATF_REQUIRE_MSG(n == (ssize_t)sizeof(icmp), "sendto failed: %s",
	    strerror(errno));

	return (sizeof(icmp) + sizeof(struct ip));
}

static size_t
ping6(int s, const struct sockaddr *sa, socklen_t salen)
{
	struct {
		struct icmp6_hdr icmp6;
		char data[64];
	} icmp6;
	ssize_t n;

	memset(&icmp6, 0, sizeof(icmp6));
	icmp6.icmp6.icmp6_type = ICMP6_ECHO_REQUEST;
	icmp6.icmp6.icmp6_code = 0;
	icmp6.icmp6.icmp6_cksum =
	    htons((unsigned short)~(ICMP6_ECHO_REQUEST << 8));
	n = sendto(s, &icmp6, sizeof(icmp6), 0, sa, salen);
	ATF_REQUIRE_MSG(n == (ssize_t)sizeof(icmp6), "sendto failed: %s",
	    strerror(errno));

	return (sizeof(icmp6));
}

static void
per_fib_raw_socket(int domain, int type, const atf_tc_t *tc __unused)
{
	struct sockaddr_in sin;
	struct sockaddr_in6 sin6;
	ssize_t n;
	size_t sz;
	int error, cs, s[2], proto;
	uint8_t b[256];

	ATF_REQUIRE(type == SOCK_RAW);
	ATF_REQUIRE(domain == PF_INET || domain == PF_INET6);
	require_fibs_multibind(type, 2);

	proto = domain == PF_INET ? IPPROTO_ICMP : IPPROTO_ICMPV6;
	s[0] = mksockp(domain, type, 0, proto);
	s[1] = mksockp(domain, type, 1, proto);

	if (domain == PF_INET) {
		memset(&sin, 0, sizeof(sin));
		sin.sin_family = domain;
		sin.sin_len = sizeof(sin);
		sin.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
		error = bind(s[0], (struct sockaddr *)&sin, sizeof(sin));
	} else /* if (domain == PF_INET6) */ {
		memset(&sin6, 0, sizeof(sin6));
		sin6.sin6_family = domain;
		sin6.sin6_len = sizeof(sin6);
		sin6.sin6_addr = in6addr_loopback;
		error = bind(s[0], (struct sockaddr *)&sin6, sizeof(sin6));
	}
	ATF_REQUIRE_MSG(error == 0, "bind failed: %s", strerror(errno));

	for (int i = 0; i < 2; i++) {
		cs = mksockp(domain, type, i, proto);
		if (domain == PF_INET) {
			sz = ping(cs, (struct sockaddr *)&sin, sizeof(sin));
		} else /* if (domain == PF_INET6) */ {
			sz = ping6(cs, (struct sockaddr *)&sin6, sizeof(sin6));
		}
		n = recv(s[i], b, sizeof(b), 0);
		ATF_REQUIRE_MSG(n > 0, "recv failed: %s", strerror(errno));
		ATF_REQUIRE_MSG(n == (ssize_t)sz,
		    "short packet received: %zd", n);

		if (domain == PF_INET6) {
			/* Get the echo reply as well. */
			n = recv(s[i], b, sizeof(b), 0);
			ATF_REQUIRE_MSG(n > 0,
			    "recv failed: %s", strerror(errno));
			ATF_REQUIRE_MSG(n == (ssize_t)sz,
			    "short packet received: %zd", n);
		}

		/* Make sure that the other socket didn't receive anything. */
		n = recv(s[1 - i], b, sizeof(b), MSG_DONTWAIT);
		printf("n = %zd i = %d\n", n, i);
		ATF_REQUIRE_ERRNO(EWOULDBLOCK, n == -1);

		checked_close(cs);
	}

	checked_close(s[0]);
	checked_close(s[1]);
}
MAKETEST_RAW(per_fib_raw_socket);

/*
 * Create a pair of load-balancing listening socket groups, one in each FIB, and
 * make sure that connections to the group are only load-balanced within the
 * same FIB.
 */
static void
multibind_lbgroup_stream(int domain, int type, const atf_tc_t *tc __unused)
{
	struct sockaddr_storage ss;
	socklen_t sslen;
	int error, as, cs, s[3];

	ATF_REQUIRE(type == SOCK_STREAM);
	ATF_REQUIRE(domain == PF_INET || domain == PF_INET6);
	require_fibs_multibind(type, 2);

	s[0] = mksock(domain, type, 0);
	ATF_REQUIRE(setsockopt(s[0], SOL_SOCKET, SO_REUSEPORT_LB, &(int){1},
	    sizeof(int)) == 0);
	ATF_REQUIRE(fcntl(s[0], F_SETFL, O_NONBLOCK) == 0);
	s[1] = mksock(domain, type, 0);
	ATF_REQUIRE(setsockopt(s[1], SOL_SOCKET, SO_REUSEPORT_LB, &(int){1},
	    sizeof(int)) == 0);
	ATF_REQUIRE(fcntl(s[1], F_SETFL, O_NONBLOCK) == 0);
	s[2] = mksock(domain, type, 1);
	ATF_REQUIRE(setsockopt(s[2], SOL_SOCKET, SO_REUSEPORT_LB, &(int){1},
	    sizeof(int)) == 0);

	sslen = domain == PF_INET ? sizeof(struct sockaddr_in) :
	    sizeof(struct sockaddr_in6);
	memset(&ss, 0, sizeof(ss));
	ss.ss_family = domain;
	ss.ss_len = sslen;
	error = bind(s[0], (struct sockaddr *)&ss, sslen);
	ATF_REQUIRE_MSG(error == 0, "bind failed: %s", strerror(errno));
	error = listen(s[0], 5);
	ATF_REQUIRE_MSG(error == 0, "listen failed: %s", strerror(errno));
	error = getsockname(s[0], (struct sockaddr *)&ss, &sslen);
	ATF_REQUIRE_MSG(error == 0, "getsockname failed: %s", strerror(errno));

	error = bind(s[1], (struct sockaddr *)&ss, sslen);
	ATF_REQUIRE_MSG(error == 0, "bind failed: %s", strerror(errno));
	error = listen(s[1], 5);
	ATF_REQUIRE_MSG(error == 0, "listen failed: %s", strerror(errno));

	error = bind(s[2], (struct sockaddr *)&ss, sslen);
	ATF_REQUIRE_MSG(error == 0, "bind failed: %s", strerror(errno));
	error = listen(s[2], 5);
	ATF_REQUIRE_MSG(error == 0, "listen failed: %s", strerror(errno));

	/*
	 * Initiate connections from FIB 0, make sure they go to s[0] or s[1].
	 */
	for (int count = 0; count < 100; count++) {
		cs = mksock(domain, type, 0);
		error = connect(cs, (struct sockaddr *)&ss, sslen);
		ATF_REQUIRE_MSG(error == 0, "connect failed: %s",
		    strerror(errno));

		do {
			as = accept(s[0], NULL, NULL);
			if (as == -1) {
				ATF_REQUIRE_MSG(errno == EWOULDBLOCK,
				    "accept failed: %s", strerror(errno));
				as = accept(s[1], NULL, NULL);
				if (as == -1) {
					ATF_REQUIRE_MSG(errno == EWOULDBLOCK,
					    "accept failed: %s",
					    strerror(errno));
				}
			}
		} while (as == -1);
		checked_close(as);
		checked_close(cs);
	}

	/*
	 * Initiate connections from FIB 1, make sure they go to s[2].
	 */
	for (int count = 0; count < 100; count++) {
		cs = mksock(domain, type, 1);
		error = connect(cs, (struct sockaddr *)&ss, sslen);
		ATF_REQUIRE_MSG(error == 0, "connect failed: %s",
		    strerror(errno));

		as = accept(s[2], NULL, NULL);
		ATF_REQUIRE_MSG(as != -1, "accept failed: %s", strerror(errno));
		checked_close(as);
		checked_close(cs);
	}

	checked_close(s[0]);
	checked_close(s[1]);
	checked_close(s[2]);
}
MAKETEST_TCP(multibind_lbgroup_stream);

static void
multibind_lbgroup_dgram(int domain, int type, const atf_tc_t *tc __unused)
{
	struct sockaddr_storage ss;
	struct sockaddr_in6 *sin6p;
	socklen_t sslen;
	ssize_t n;
	int error, cs, s[3];
	char b;

	ATF_REQUIRE(type == SOCK_DGRAM);
	ATF_REQUIRE(domain == PF_INET || domain == PF_INET6);
	require_fibs_multibind(type, 2);

	s[0] = mksock(domain, type, 0);
	ATF_REQUIRE(setsockopt(s[0], SOL_SOCKET, SO_REUSEPORT_LB, &(int){1},
	    sizeof(int)) == 0);
	s[1] = mksock(domain, type, 0);
	ATF_REQUIRE(setsockopt(s[1], SOL_SOCKET, SO_REUSEPORT_LB, &(int){1},
	    sizeof(int)) == 0);
	s[2] = mksock(domain, type, 1);
	ATF_REQUIRE(setsockopt(s[2], SOL_SOCKET, SO_REUSEPORT_LB, &(int){1},
	    sizeof(int)) == 0);

	sslen = domain == PF_INET ? sizeof(struct sockaddr_in) :
	    sizeof(struct sockaddr_in6);
	memset(&ss, 0, sizeof(ss));
	ss.ss_family = domain;
	ss.ss_len = sslen;
	error = bind(s[0], (struct sockaddr *)&ss, sslen);
	ATF_REQUIRE_MSG(error == 0, "bind failed: %s", strerror(errno));
	error = getsockname(s[0], (struct sockaddr *)&ss, &sslen);
	ATF_REQUIRE_MSG(error == 0, "getsockname failed: %s", strerror(errno));

	error = bind(s[1], (struct sockaddr *)&ss, sslen);
	ATF_REQUIRE_MSG(error == 0, "bind failed: %s", strerror(errno));
	error = bind(s[2], (struct sockaddr *)&ss, sslen);
	ATF_REQUIRE_MSG(error == 0, "bind failed: %s", strerror(errno));

	if (domain == PF_INET6) {
		sin6p = (struct sockaddr_in6 *)&ss;
		sin6p->sin6_addr = in6addr_loopback;
	}

	/*
	 * Send a packet from FIB 0, make sure it goes to s[0] or s[1].
	 */
	cs = mksock(domain, type, 0);
	for (int count = 0; count < 100; count++) {
		int bytes, rs;

		b = 42;
		n = sendto(cs, &b, sizeof(b), 0, (struct sockaddr *)&ss, sslen);
		ATF_REQUIRE_MSG(n == 1, "sendto failed: %s", strerror(errno));
		usleep(1000);

		error = ioctl(s[0], FIONREAD, &bytes);
		ATF_REQUIRE_MSG(error == 0, "ioctl failed: %s",
		    strerror(errno));
		if (bytes == 0) {
			error = ioctl(s[1], FIONREAD, &bytes);
			ATF_REQUIRE_MSG(error == 0, "ioctl failed: %s",
			    strerror(errno));
			rs = s[1];
		} else {
			rs = s[0];
		}
		n = recv(rs, &b, sizeof(b), 0);
		ATF_REQUIRE(n == 1);
		ATF_REQUIRE(b == 42);
		ATF_REQUIRE(bytes == 1);
	}
	checked_close(cs);

	/*
	 * Send a packet from FIB 1, make sure it goes to s[2].
	 */
	cs = mksock(domain, type, 1);
	for (int count = 0; count < 100; count++) {
		b = 42;
		n = sendto(cs, &b, sizeof(b), 0, (struct sockaddr *)&ss, sslen);
		ATF_REQUIRE_MSG(n == 1, "sendto failed: %s", strerror(errno));
		usleep(1000);

		n = recv(s[2], &b, sizeof(b), 0);
		ATF_REQUIRE(n == 1);
		ATF_REQUIRE(b == 42);
	}
	checked_close(cs);

	checked_close(s[0]);
	checked_close(s[1]);
	checked_close(s[2]);
}
MAKETEST_UDP(multibind_lbgroup_dgram);

/*
 * Make sure that we can't change the FIB of a bound socket.
 */
static void
no_setfib_after_bind(int domain, int type, const atf_tc_t *tc __unused)
{
	struct sockaddr_storage ss;
	socklen_t sslen;
	int error, s;

	ATF_REQUIRE(domain == PF_INET || domain == PF_INET6);
	require_fibs_multibind(type, 2);

	s = mksock(domain, type, 0);

	sslen = domain == PF_INET ? sizeof(struct sockaddr_in) :
	    sizeof(struct sockaddr_in6);
	memset(&ss, 0, sizeof(ss));
	ss.ss_family = domain;
	ss.ss_len = sslen;
	error = bind(s, (struct sockaddr *)&ss, sslen);
	ATF_REQUIRE_MSG(error == 0, "bind failed: %s", strerror(errno));

	error = setsockopt(s, SOL_SOCKET, SO_SETFIB, &(int){1}, sizeof(int));
	ATF_REQUIRE_ERRNO(EISCONN, error == -1);

	/* It's ok to set the FIB number to its current value. */
	error = setsockopt(s, SOL_SOCKET, SO_SETFIB, &(int){0}, sizeof(int));
	ATF_REQUIRE_MSG(error == 0, "setsockopt failed: %s", strerror(errno));

	checked_close(s);
}
MAKETEST(no_setfib_after_bind);

ATF_TP_ADD_TCS(tp)
{
	LISTTEST(multibind_different_user);
	LISTTEST_TCP(per_fib_listening_socket);
	LISTTEST_UDP(per_fib_dgram_socket);
	LISTTEST_RAW(per_fib_raw_socket);
	LISTTEST_TCP(multibind_lbgroup_stream);
	LISTTEST_UDP(multibind_lbgroup_dgram);
	LISTTEST(no_setfib_after_bind);

	return (atf_no_error());
}
