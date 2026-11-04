/*-
 * Copyright (c) 2018 The FreeBSD Foundation
 *
 * This software was developed by Mark Johnston under sponsorship from
 * the FreeBSD Foundation.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
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
#include <sys/event.h>
#include <sys/filio.h>
#include <sys/ioccom.h>
#include <sys/jail.h>
#include <sys/socket.h>
#include <sys/sysctl.h>
#include <sys/uio.h>
#include <sys/wait.h>

#include <netinet/in.h>
#include <netinet/tcp.h>

#include <err.h>
#include <errno.h>
#include <poll.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#include <atf-c.h>

/* Keep in sync with INPCBLBGROUP_SIZMAX in sys/netinet/in_pcb.c. */
#define	LB_GROUP_MAX	256

/*
 * Given an array of non-blocking listening sockets configured in a LB group
 * for "addr", try connecting to "addr" in a loop and verify that connections
 * are roughly balanced across the sockets.
 */
static void
lb_simple_accept_loop(int domain, const struct sockaddr *addr, int sds[],
    size_t nsds, int nconns)
{
	struct sockaddr_in6 sin6;
	size_t i;
	int *acceptcnt;
	int csd, error, excnt, sd;
	const struct linger lopt = { 1, 0 };

	/*
	 * We expect each listening socket to accept roughly nconns/nsds
	 * connections, but allow for some error.
	 */
	excnt = nconns / nsds / 8;
	acceptcnt = calloc(nsds, sizeof(*acceptcnt));
	ATF_REQUIRE_MSG(acceptcnt != NULL, "calloc() failed: %s",
	    strerror(errno));

	while (nconns-- > 0) {
		sd = socket(domain, SOCK_STREAM, 0);
		ATF_REQUIRE_MSG(sd >= 0, "socket() failed: %s",
		    strerror(errno));

		if (domain == PF_INET6 && addr->sa_family == AF_INET6) {
			memcpy(&sin6, addr, sizeof(sin6));
		} else {
			memset(&sin6, 0, sizeof(sin6));
		}
		if (domain == PF_INET6 && addr->sa_family == AF_INET6 &&
		    IN6_IS_ADDR_V4MAPPED(&sin6.sin6_addr)) {
			error = setsockopt(sd, IPPROTO_IPV6, IPV6_V6ONLY,
			    (int[]){0}, sizeof(int));
			ATF_REQUIRE_MSG(error == 0,
			    "setsockopt(IPV6_V6ONLY) failed: %s",
			    strerror(errno));
		}
		error = connect(sd, addr, addr->sa_len);
		ATF_REQUIRE_MSG(error == 0, "connect() failed: %s",
		    strerror(errno));

		error = setsockopt(sd, SOL_SOCKET, SO_LINGER, &lopt, sizeof(lopt));
		ATF_REQUIRE_MSG(error == 0, "Setting linger failed: %s",
		    strerror(errno));

		/*
		 * Poll the listening sockets.
		 */
		do {
			for (i = 0; i < nsds; i++) {
				csd = accept(sds[i], NULL, NULL);
				if (csd < 0) {
					ATF_REQUIRE_MSG(errno == EWOULDBLOCK ||
					    errno == EAGAIN,
					    "accept() failed: %s",
					    strerror(errno));
					continue;
				}

				error = close(csd);
				ATF_REQUIRE_MSG(error == 0,
				    "close() failed: %s", strerror(errno));

				acceptcnt[i]++;
				break;
			}
		} while (i == nsds);

		error = close(sd);
		ATF_REQUIRE_MSG(error == 0, "close() failed: %s",
		    strerror(errno));
	}

	for (i = 0; i < nsds; i++)
		ATF_REQUIRE_MSG(acceptcnt[i] > excnt, "uneven balancing");
}

static int
lb_listen_socket(int domain, int flags)
{
	int one;
	int error, sd;

	sd = socket(domain, SOCK_STREAM | flags, 0);
	ATF_REQUIRE_MSG(sd >= 0, "socket() failed: %s", strerror(errno));

	one = 1;
	error = setsockopt(sd, SOL_SOCKET, SO_REUSEPORT_LB, &one, sizeof(one));
	ATF_REQUIRE_MSG(error == 0, "setsockopt(SO_REUSEPORT_LB) failed: %s",
	    strerror(errno));

	return (sd);
}

ATF_TC_WITHOUT_HEAD(basic_ipv4);
ATF_TC_BODY(basic_ipv4, tc)
{
	struct sockaddr_in addr;
	socklen_t slen;
	size_t i;
	const int nconns = 16384;
	int error, sds[16];
	uint16_t port;

	sds[0] = lb_listen_socket(PF_INET, SOCK_NONBLOCK);

	memset(&addr, 0, sizeof(addr));
	addr.sin_len = sizeof(addr);
	addr.sin_family = AF_INET;
	addr.sin_port = htons(0);
	addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	error = bind(sds[0], (const struct sockaddr *)&addr, sizeof(addr));
	ATF_REQUIRE_MSG(error == 0, "bind() failed: %s", strerror(errno));
	error = listen(sds[0], 1);
	ATF_REQUIRE_MSG(error == 0, "listen() failed: %s", strerror(errno));

	slen = sizeof(addr);
	error = getsockname(sds[0], (struct sockaddr *)&addr, &slen);
	ATF_REQUIRE_MSG(error == 0, "getsockname() failed: %s",
	    strerror(errno));
	ATF_REQUIRE_MSG(slen == sizeof(addr), "sockaddr size changed");
	port = addr.sin_port;

	memset(&addr, 0, sizeof(addr));
	addr.sin_len = sizeof(addr);
	addr.sin_family = AF_INET;
	addr.sin_port = port;
	addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	for (i = 1; i < nitems(sds); i++) {
		sds[i] = lb_listen_socket(PF_INET, SOCK_NONBLOCK);

		error = bind(sds[i], (const struct sockaddr *)&addr,
		    sizeof(addr));
		ATF_REQUIRE_MSG(error == 0, "bind() failed: %s",
		    strerror(errno));
		error = listen(sds[i], 1);
		ATF_REQUIRE_MSG(error == 0, "listen() failed: %s",
		    strerror(errno));
	}

	lb_simple_accept_loop(PF_INET, (struct sockaddr *)&addr, sds,
	    nitems(sds), nconns);
	for (i = 0; i < nitems(sds); i++) {
		error = close(sds[i]);
		ATF_REQUIRE_MSG(error == 0, "close() failed: %s",
		    strerror(errno));
	}
}

ATF_TC_WITHOUT_HEAD(basic_ipv6);
ATF_TC_BODY(basic_ipv6, tc)
{
	const struct in6_addr loopback6 = IN6ADDR_LOOPBACK_INIT;
	struct sockaddr_in6 addr;
	socklen_t slen;
	size_t i;
	const int nconns = 16384;
	int error, sds[16];
	uint16_t port;

	sds[0] = lb_listen_socket(PF_INET6, SOCK_NONBLOCK);

	memset(&addr, 0, sizeof(addr));
	addr.sin6_len = sizeof(addr);
	addr.sin6_family = AF_INET6;
	addr.sin6_port = htons(0);
	addr.sin6_addr = loopback6;
	error = bind(sds[0], (const struct sockaddr *)&addr, sizeof(addr));
	ATF_REQUIRE_MSG(error == 0, "bind() failed: %s", strerror(errno));
	error = listen(sds[0], 1);
	ATF_REQUIRE_MSG(error == 0, "listen() failed: %s", strerror(errno));

	slen = sizeof(addr);
	error = getsockname(sds[0], (struct sockaddr *)&addr, &slen);
	ATF_REQUIRE_MSG(error == 0, "getsockname() failed: %s",
	    strerror(errno));
	ATF_REQUIRE_MSG(slen == sizeof(addr), "sockaddr size changed");
	port = addr.sin6_port;

	memset(&addr, 0, sizeof(addr));
	addr.sin6_len = sizeof(addr);
	addr.sin6_family = AF_INET6;
	addr.sin6_port = port;
	addr.sin6_addr = loopback6;
	for (i = 1; i < nitems(sds); i++) {
		sds[i] = lb_listen_socket(PF_INET6, SOCK_NONBLOCK);

		error = bind(sds[i], (const struct sockaddr *)&addr,
		    sizeof(addr));
		ATF_REQUIRE_MSG(error == 0, "bind() failed: %s",
		    strerror(errno));
		error = listen(sds[i], 1);
		ATF_REQUIRE_MSG(error == 0, "listen() failed: %s",
		    strerror(errno));
	}

	lb_simple_accept_loop(PF_INET6, (struct sockaddr *)&addr, sds,
	    nitems(sds), nconns);
	for (i = 0; i < nitems(sds); i++) {
		error = close(sds[i]);
		ATF_REQUIRE_MSG(error == 0, "close() failed: %s",
		    strerror(errno));
	}
}

struct concurrent_add_softc {
	struct sockaddr_storage ss;
	int socks[128];
	int kq;
};

static void *
listener(void *arg)
{
	for (struct concurrent_add_softc *sc = arg;;) {
		struct kevent kev;
		ssize_t n;
		int error, count, cs, s;
		uint8_t b;

		count = kevent(sc->kq, NULL, 0, &kev, 1, NULL);
		ATF_REQUIRE_MSG(count == 1,
		    "kevent() failed: %s", strerror(errno));

		s = (int)kev.ident;
		cs = accept(s, NULL, NULL);
		ATF_REQUIRE_MSG(cs >= 0,
		    "accept() failed: %s", strerror(errno));

		b = 'M';
		n = write(cs, &b, sizeof(b));
		ATF_REQUIRE_MSG(n >= 0, "write() failed: %s", strerror(errno));
		ATF_REQUIRE(n == 1);

		error = close(cs);
		ATF_REQUIRE_MSG(error == 0 || errno == ECONNRESET,
		    "close() failed: %s", strerror(errno));
	}
}

static void *
connector(void *arg)
{
	for (struct concurrent_add_softc *sc = arg;;) {
		ssize_t n;
		int error, s;
		uint8_t b;

		s = socket(sc->ss.ss_family, SOCK_STREAM, 0);
		ATF_REQUIRE_MSG(s >= 0, "socket() failed: %s", strerror(errno));

		error = setsockopt(s, SOL_SOCKET, SO_REUSEADDR, (int[]){1},
		    sizeof(int));

		error = connect(s, (struct sockaddr *)&sc->ss, sc->ss.ss_len);
		ATF_REQUIRE_MSG(error == 0, "connect() failed: %s",
		    strerror(errno));

		n = read(s, &b, sizeof(b));
		ATF_REQUIRE_MSG(n >= 0, "read() failed: %s",
		    strerror(errno));
		ATF_REQUIRE(n == 1);
		ATF_REQUIRE(b == 'M');
		error = close(s);
		ATF_REQUIRE_MSG(error == 0,
		    "close() failed: %s", strerror(errno));
	}
}

/*
 * Run three threads.  One accepts connections from listening sockets on a
 * kqueue, while the other makes connections.  The third thread slowly adds
 * sockets to the LB group.  This is meant to help flush out race conditions.
 */
ATF_TC_WITHOUT_HEAD(concurrent_add);
ATF_TC_BODY(concurrent_add, tc)
{
	struct concurrent_add_softc sc;
	struct sockaddr_in *sin;
	pthread_t threads[4];
	int error;

	sc.kq = kqueue();
	ATF_REQUIRE_MSG(sc.kq >= 0, "kqueue() failed: %s", strerror(errno));

	error = pthread_create(&threads[0], NULL, listener, &sc);
	ATF_REQUIRE_MSG(error == 0, "pthread_create() failed: %s",
	    strerror(error));

	sin = (struct sockaddr_in *)&sc.ss;
	memset(sin, 0, sizeof(*sin));
	sin->sin_len = sizeof(*sin);
	sin->sin_family = AF_INET;
	sin->sin_port = htons(0);
	sin->sin_addr.s_addr = htonl(INADDR_LOOPBACK);

	for (size_t i = 0; i < nitems(sc.socks); i++) {
		struct kevent kev;
		int s;

		sc.socks[i] = s = socket(AF_INET, SOCK_STREAM, 0);
		ATF_REQUIRE_MSG(s >= 0, "socket() failed: %s", strerror(errno));

		error = setsockopt(s, SOL_SOCKET, SO_REUSEPORT_LB, (int[]){1},
		    sizeof(int));
		ATF_REQUIRE_MSG(error == 0,
		    "setsockopt(SO_REUSEPORT_LB) failed: %s", strerror(errno));

		error = bind(s, (struct sockaddr *)sin, sizeof(*sin));
		ATF_REQUIRE_MSG(error == 0, "bind() failed: %s",
		    strerror(errno));

		error = listen(s, 5);
		ATF_REQUIRE_MSG(error == 0, "listen() failed: %s",
		    strerror(errno));

		EV_SET(&kev, s, EVFILT_READ, EV_ADD | EV_ENABLE, 0, 0, 0);
		error = kevent(sc.kq, &kev, 1, NULL, 0, NULL);
		ATF_REQUIRE_MSG(error == 0, "kevent() failed: %s",
		    strerror(errno));

		if (i == 0) {
			socklen_t slen = sizeof(sc.ss);

			error = getsockname(sc.socks[i],
			    (struct sockaddr *)&sc.ss, &slen);
			ATF_REQUIRE_MSG(error == 0, "getsockname() failed: %s",
			    strerror(errno));
			ATF_REQUIRE(sc.ss.ss_family == AF_INET);

			for (size_t j = 1; j < nitems(threads); j++) {
				error = pthread_create(&threads[j], NULL,
				    connector, &sc);
				ATF_REQUIRE_MSG(error == 0,
				    "pthread_create() failed: %s",
				    strerror(error));
			}
		}

		usleep(20000);
	}

	for (size_t j = nitems(threads); j > 0; j--) {
		ATF_REQUIRE(pthread_cancel(threads[j - 1]) == 0);
		ATF_REQUIRE(pthread_join(threads[j - 1], NULL) == 0);
	}
}

/*
 * Try calling listen(2) twice on a socket with SO_REUSEPORT_LB set.
 */
ATF_TC_WITHOUT_HEAD(double_listen_ipv4);
ATF_TC_BODY(double_listen_ipv4, tc)
{
	struct sockaddr_in sin;
	int error, s;

	s = lb_listen_socket(PF_INET, 0);

	memset(&sin, 0, sizeof(sin));
	sin.sin_len = sizeof(sin);
	sin.sin_family = AF_INET;
	sin.sin_port = htons(0);
	sin.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	error = bind(s, (struct sockaddr *)&sin, sizeof(sin));
	ATF_REQUIRE_MSG(error == 0, "bind() failed: %s", strerror(errno));

	error = listen(s, 1);
	ATF_REQUIRE_MSG(error == 0, "listen() failed: %s", strerror(errno));
	error = listen(s, 2);
	ATF_REQUIRE_MSG(error == 0, "listen() failed: %s", strerror(errno));

	error = close(s);
	ATF_REQUIRE_MSG(error == 0, "close() failed: %s", strerror(errno));
}

/*
 * Try calling listen(2) twice on a socket with SO_REUSEPORT_LB set.
 */
ATF_TC_WITHOUT_HEAD(double_listen_ipv6);
ATF_TC_BODY(double_listen_ipv6, tc)
{
	struct sockaddr_in6 sin6;
	int error, s;

	s = lb_listen_socket(PF_INET6, 0);

	memset(&sin6, 0, sizeof(sin6));
	sin6.sin6_len = sizeof(sin6);
	sin6.sin6_family = AF_INET6;
	sin6.sin6_port = htons(0);
	sin6.sin6_addr = in6addr_loopback;
	error = bind(s, (struct sockaddr *)&sin6, sizeof(sin6));
	ATF_REQUIRE_MSG(error == 0, "bind() failed: %s", strerror(errno));

	error = listen(s, 1);
	ATF_REQUIRE_MSG(error == 0, "listen() failed: %s", strerror(errno));
	error = listen(s, 2);
	ATF_REQUIRE_MSG(error == 0, "listen() failed: %s", strerror(errno));

	error = close(s);
	ATF_REQUIRE_MSG(error == 0, "close() failed: %s", strerror(errno));
}

/*
 * Try binding many sockets to the same lbgroup without calling listen(2) on
 * them.
 */
ATF_TC_WITHOUT_HEAD(bind_without_listen);
ATF_TC_BODY(bind_without_listen, tc)
{
	const int nsockets = 100;
	struct sockaddr_in sin;
	socklen_t socklen;
	int error, s, s2[nsockets];

	s = lb_listen_socket(PF_INET, 0);

	memset(&sin, 0, sizeof(sin));
	sin.sin_len = sizeof(sin);
	sin.sin_family = AF_INET;
	sin.sin_port = htons(0);
	sin.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	error = bind(s, (struct sockaddr *)&sin, sizeof(sin));
	ATF_REQUIRE_MSG(error == 0, "bind() failed: %s", strerror(errno));

	socklen = sizeof(sin);
	error = getsockname(s, (struct sockaddr *)&sin, &socklen);
	ATF_REQUIRE_MSG(error == 0, "getsockname() failed: %s",
	    strerror(errno));

	for (int i = 0; i < nsockets; i++) {
		s2[i] = lb_listen_socket(PF_INET, 0);
		error = bind(s2[i], (struct sockaddr *)&sin, sizeof(sin));
		ATF_REQUIRE_MSG(error == 0, "bind() failed: %s", strerror(errno));
	}
	for (int i = 0; i < nsockets; i++) {
		error = listen(s2[i], 1);
		ATF_REQUIRE_MSG(error == 0, "listen() failed: %s", strerror(errno));
	}
	for (int i = 0; i < nsockets; i++) {
		error = close(s2[i]);
		ATF_REQUIRE_MSG(error == 0, "close() failed: %s", strerror(errno));
	}

	error = close(s);
	ATF_REQUIRE_MSG(error == 0, "close() failed: %s", strerror(errno));
}

/*
 * Check that SO_REUSEPORT_LB doesn't mess with connect(2).
 * Two sockets:
 * 1) auxiliary peer socket 'p', where we connect to
 * 2) test socket 's', that sets SO_REUSEPORT_LB and then connect(2)s to 'p'
 */
ATF_TC_WITHOUT_HEAD(connect_not_bound);
ATF_TC_BODY(connect_not_bound, tc)
{
	struct sockaddr_in sin = {
		.sin_family = AF_INET,
		.sin_len = sizeof(sin),
		.sin_addr = { htonl(INADDR_LOOPBACK) },
	};
	socklen_t slen = sizeof(struct sockaddr_in);
	int p, s, rv;

	ATF_REQUIRE((p = socket(PF_INET, SOCK_STREAM, 0)) > 0);
	ATF_REQUIRE(bind(p, (struct sockaddr *)&sin, sizeof(sin)) == 0);
	ATF_REQUIRE(listen(p, 1) == 0);
	ATF_REQUIRE(getsockname(p, (struct sockaddr *)&sin, &slen) == 0);

	s = lb_listen_socket(PF_INET, 0);
	rv = connect(s, (struct sockaddr *)&sin, sizeof(sin));
	ATF_REQUIRE_MSG(rv == -1 && errno == EOPNOTSUPP,
	    "Expected EOPNOTSUPP on connect(2) not met. Got %d, errno %d",
	    rv, errno);
	rv = sendto(s, "test", 4, 0, (struct sockaddr *)&sin,
	    sizeof(sin));
	ATF_REQUIRE_MSG(rv == -1 && errno == EOPNOTSUPP,
	    "Expected EOPNOTSUPP on sendto(2) not met. Got %d, errno %d",
	    rv, errno);

	close(p);
	close(s);
}

/*
 * Same as above, but we also bind(2) between setsockopt(2) of SO_REUSEPORT_LB
 * and the connect(2).
 */
ATF_TC_WITHOUT_HEAD(connect_bound);
ATF_TC_BODY(connect_bound, tc)
{
	struct sockaddr_in sin = {
		.sin_family = AF_INET,
		.sin_len = sizeof(sin),
		.sin_addr = { htonl(INADDR_LOOPBACK) },
	};
	socklen_t slen = sizeof(struct sockaddr_in);
	int p, s, rv;

	ATF_REQUIRE((p = socket(PF_INET, SOCK_STREAM, 0)) > 0);
	ATF_REQUIRE(bind(p, (struct sockaddr *)&sin, sizeof(sin)) == 0);
	ATF_REQUIRE(listen(p, 1) == 0);

	s = lb_listen_socket(PF_INET, 0);
	ATF_REQUIRE(bind(s, (struct sockaddr *)&sin, sizeof(sin)) == 0);
	ATF_REQUIRE(getsockname(p, (struct sockaddr *)&sin, &slen) == 0);
	rv = connect(s, (struct sockaddr *)&sin, sizeof(sin));
	ATF_REQUIRE_MSG(rv == -1 && errno == EOPNOTSUPP,
	    "Expected EOPNOTSUPP on connect(2) not met. Got %d, errno %d",
	    rv, errno);
	rv = sendto(s, "test", 4, 0, (struct sockaddr *)&sin,
	    sizeof(sin));
	ATF_REQUIRE_MSG(rv == -1 && errno == EOPNOTSUPP,
	    "Expected EOPNOTSUPP on sendto(2) not met. Got %d, errno %d",
	    rv, errno);

	close(p);
	close(s);
}

/*
 * The kernel erroneously permits calling connect() on a UDP socket with
 * SO_REUSEPORT_LB set.  Verify that packets sent to the bound address are
 * dropped unless they come from the connected address.
 */
ATF_TC_WITHOUT_HEAD(connect_udp);
ATF_TC_BODY(connect_udp, tc)
{
	struct sockaddr_in sin = {
		.sin_family = AF_INET,
		.sin_len = sizeof(sin),
		.sin_addr = { htonl(INADDR_LOOPBACK) },
	};
	ssize_t n;
	int error, len, s1, s2, s3;
	char ch;

	s1 = socket(PF_INET, SOCK_DGRAM, 0);
	ATF_REQUIRE(s1 >= 0);
	s2 = socket(PF_INET, SOCK_DGRAM, 0);
	ATF_REQUIRE(s2 >= 0);
	s3 = socket(PF_INET, SOCK_DGRAM, 0);
	ATF_REQUIRE(s3 >= 0);

	error = setsockopt(s1, SOL_SOCKET, SO_REUSEPORT_LB, (int[]){1},
	    sizeof(int));
	ATF_REQUIRE_MSG(error == 0,
	    "setsockopt(SO_REUSEPORT_LB) failed: %s", strerror(errno));
	error = bind(s1, (struct sockaddr *)&sin, sizeof(sin));
	ATF_REQUIRE_MSG(error == 0, "bind() failed: %s", strerror(errno));

	error = bind(s2, (struct sockaddr *)&sin, sizeof(sin));
	ATF_REQUIRE_MSG(error == 0, "bind() failed: %s", strerror(errno));

	error = bind(s3, (struct sockaddr *)&sin, sizeof(sin));
	ATF_REQUIRE_MSG(error == 0, "bind() failed: %s", strerror(errno));

	/* Connect to an address not owned by s2. */
	error = getsockname(s3, (struct sockaddr *)&sin,
	    (socklen_t[]){sizeof(sin)});
	ATF_REQUIRE(error == 0);
	error = connect(s1, (struct sockaddr *)&sin, sizeof(sin));
	ATF_REQUIRE_MSG(error == 0, "connect() failed: %s", strerror(errno));

	/* Try to send a packet to s1 from s2. */
	error = getsockname(s1, (struct sockaddr *)&sin,
	    (socklen_t[]){sizeof(sin)});
	ATF_REQUIRE(error == 0);

	ch = 42;
	n = sendto(s2, &ch, sizeof(ch), 0, (struct sockaddr *)&sin,
	    sizeof(sin));
	ATF_REQUIRE(n == 1);

	/* Give the packet some time to arrive. */
	usleep(100000);

	/* s1 is connected to s3 and shouldn't receive from s2. */
	error = ioctl(s1, FIONREAD, &len);
	ATF_REQUIRE(error == 0);
	ATF_REQUIRE_MSG(len == 0, "unexpected data available");

	/* ... but s3 can of course send to s1. */
	n = sendto(s3, &ch, sizeof(ch), 0, (struct sockaddr *)&sin,
	    sizeof(sin));
	ATF_REQUIRE(n == 1);
	usleep(100000);
	error = ioctl(s1, FIONREAD, &len);
	ATF_REQUIRE(error == 0);
	ATF_REQUIRE_MSG(len == 1, "expected data available");
}

/*
 * The kernel erroneously permits calling connect() on a UDP socket with
 * SO_REUSEPORT_LB set.  Verify that packets sent to the bound address are
 * dropped unless they come from the connected address.
 */
ATF_TC_WITHOUT_HEAD(connect_udp6);
ATF_TC_BODY(connect_udp6, tc)
{
	struct sockaddr_in6 sin6 = {
		.sin6_family = AF_INET6,
		.sin6_len = sizeof(sin6),
		.sin6_addr = IN6ADDR_LOOPBACK_INIT,
	};
	ssize_t n;
	int error, len, s1, s2, s3;
	char ch;

	s1 = socket(PF_INET6, SOCK_DGRAM, 0);
	ATF_REQUIRE(s1 >= 0);
	s2 = socket(PF_INET6, SOCK_DGRAM, 0);
	ATF_REQUIRE(s2 >= 0);
	s3 = socket(PF_INET6, SOCK_DGRAM, 0);
	ATF_REQUIRE(s3 >= 0);

	error = setsockopt(s1, SOL_SOCKET, SO_REUSEPORT_LB, (int[]){1},
	    sizeof(int));
	ATF_REQUIRE_MSG(error == 0,
	    "setsockopt(SO_REUSEPORT_LB) failed: %s", strerror(errno));
	error = bind(s1, (struct sockaddr *)&sin6, sizeof(sin6));
	ATF_REQUIRE_MSG(error == 0, "bind() failed: %s", strerror(errno));

	error = bind(s2, (struct sockaddr *)&sin6, sizeof(sin6));
	ATF_REQUIRE_MSG(error == 0, "bind() failed: %s", strerror(errno));

	error = bind(s3, (struct sockaddr *)&sin6, sizeof(sin6));
	ATF_REQUIRE_MSG(error == 0, "bind() failed: %s", strerror(errno));

	/* Connect to an address not owned by s2. */
	error = getsockname(s3, (struct sockaddr *)&sin6,
	    (socklen_t[]){sizeof(sin6)});
	ATF_REQUIRE(error == 0);
	error = connect(s1, (struct sockaddr *)&sin6, sizeof(sin6));
	ATF_REQUIRE_MSG(error == 0, "connect() failed: %s", strerror(errno));

	/* Try to send a packet to s1 from s2. */
	error = getsockname(s1, (struct sockaddr *)&sin6,
	    (socklen_t[]){sizeof(sin6)});
	ATF_REQUIRE(error == 0);

	ch = 42;
	n = sendto(s2, &ch, sizeof(ch), 0, (struct sockaddr *)&sin6,
	    sizeof(sin6));
	ATF_REQUIRE(n == 1);

	/* Give the packet some time to arrive. */
	usleep(100000);

	/* s1 is connected to s3 and shouldn't receive from s2. */
	error = ioctl(s1, FIONREAD, &len);
	ATF_REQUIRE(error == 0);
	ATF_REQUIRE_MSG(len == 0, "unexpected data available");

	/* ... but s3 can of course send to s1. */
	n = sendto(s3, &ch, sizeof(ch), 0, (struct sockaddr *)&sin6,
	    sizeof(sin6));
	ATF_REQUIRE(n == 1);
	usleep(100000);
	error = ioctl(s1, FIONREAD, &len);
	ATF_REQUIRE(error == 0);
	ATF_REQUIRE_MSG(len == 1, "expected data available");
}

static int
lb_dualstack_socket(int type, uint16_t *portp, int fib)
{
	struct sockaddr_in6 sin6 = {
		.sin6_family = AF_INET6,
		.sin6_len = sizeof(sin6),
		.sin6_addr = IN6ADDR_ANY_INIT,
		.sin6_port = *portp,
	};
	socklen_t slen;
	int error, sd;

	sd = socket(PF_INET6, type | SOCK_NONBLOCK, 0);
	ATF_REQUIRE_MSG(sd >= 0, "socket() failed: %s", strerror(errno));
	if (fib >= 0) {
		error = setsockopt(sd, SOL_SOCKET, SO_SETFIB, &fib,
		    sizeof(fib));
		ATF_REQUIRE_MSG(error == 0, "setsockopt(SO_SETFIB) failed: %s",
		    strerror(errno));
	}
	error = setsockopt(sd, IPPROTO_IPV6, IPV6_V6ONLY, (int[]){0},
	    sizeof(int));
	ATF_REQUIRE_MSG(error == 0, "setsockopt(IPV6_V6ONLY) failed: %s",
	    strerror(errno));
	error = setsockopt(sd, SOL_SOCKET, SO_REUSEPORT_LB, (int[]){1},
	    sizeof(int));
	ATF_REQUIRE_MSG(error == 0, "setsockopt(SO_REUSEPORT_LB) failed: %s",
	    strerror(errno));
	error = bind(sd, (struct sockaddr *)&sin6, sizeof(sin6));
	ATF_REQUIRE_MSG(error == 0, "bind() failed: %s", strerror(errno));
	if (*portp == 0) {
		slen = sizeof(sin6);
		error = getsockname(sd, (struct sockaddr *)&sin6, &slen);
		ATF_REQUIRE_MSG(error == 0, "getsockname() failed: %s",
		    strerror(errno));
		*portp = sin6.sin6_port;
	}
	return (sd);
}

static void
lb_ipv4_mapped_addr(struct in6_addr *addr)
{
	struct in_addr in;

	memset(addr, 0, sizeof(*addr));
	addr->s6_addr[10] = 0xff;
	addr->s6_addr[11] = 0xff;
	in.s_addr = htonl(INADDR_LOOPBACK);
	memcpy(&addr->s6_addr[12], &in, sizeof(in));
}

static int
lb_dualstack_socket_addr(int type, const struct in6_addr *addr,
    uint16_t *portp)
{
	struct sockaddr_in6 sin6 = {
		.sin6_family = AF_INET6,
		.sin6_len = sizeof(sin6),
		.sin6_addr = *addr,
		.sin6_port = *portp,
	};
	socklen_t slen;
	int error, sd;

	sd = socket(PF_INET6, type | SOCK_NONBLOCK, 0);
	ATF_REQUIRE_MSG(sd >= 0, "socket() failed: %s", strerror(errno));
	error = setsockopt(sd, IPPROTO_IPV6, IPV6_V6ONLY, (int[]){0},
	    sizeof(int));
	ATF_REQUIRE_MSG(error == 0, "setsockopt(IPV6_V6ONLY) failed: %s",
	    strerror(errno));
	error = setsockopt(sd, SOL_SOCKET, SO_REUSEPORT_LB, (int[]){1},
	    sizeof(int));
	ATF_REQUIRE_MSG(error == 0, "setsockopt(SO_REUSEPORT_LB) failed: %s",
	    strerror(errno));
	error = bind(sd, (struct sockaddr *)&sin6, sizeof(sin6));
	ATF_REQUIRE_MSG(error == 0, "bind() failed: %s", strerror(errno));
	if (*portp == 0) {
		slen = sizeof(sin6);
		error = getsockname(sd, (struct sockaddr *)&sin6, &slen);
		ATF_REQUIRE_MSG(error == 0, "getsockname() failed: %s",
		    strerror(errno));
		*portp = sin6.sin6_port;
	}
	return (sd);
}

static int
lb_v6only_socket(int type, uint16_t *portp)
{
	struct sockaddr_in6 sin6 = {
		.sin6_family = AF_INET6,
		.sin6_len = sizeof(sin6),
		.sin6_addr = IN6ADDR_ANY_INIT,
		.sin6_port = *portp,
	};
	socklen_t slen;
	int error, sd, one;

	sd = socket(PF_INET6, type | SOCK_NONBLOCK, 0);
	ATF_REQUIRE_MSG(sd >= 0, "socket() failed: %s", strerror(errno));
	one = 1;
	error = setsockopt(sd, IPPROTO_IPV6, IPV6_V6ONLY, &one, sizeof(one));
	ATF_REQUIRE_MSG(error == 0, "setsockopt(IPV6_V6ONLY) failed: %s",
	    strerror(errno));
	error = setsockopt(sd, SOL_SOCKET, SO_REUSEPORT_LB, (int[]){1},
	    sizeof(int));
	ATF_REQUIRE_MSG(error == 0, "setsockopt(SO_REUSEPORT_LB) failed: %s",
	    strerror(errno));
	error = bind(sd, (struct sockaddr *)&sin6, sizeof(sin6));
	ATF_REQUIRE_MSG(error == 0, "bind() failed: %s", strerror(errno));
	if (*portp == 0) {
		slen = sizeof(sin6);
		error = getsockname(sd, (struct sockaddr *)&sin6, &slen);
		ATF_REQUIRE_MSG(error == 0, "getsockname() failed: %s",
		    strerror(errno));
		*portp = sin6.sin6_port;
	}
	return (sd);
}

/*
 * Send one datagram from each of "nclients" sockets with distinct source
 * ports, and return the number of datagrams queued on each of "sds".
 */
static void
lb_udp_spray(const struct sockaddr *dst, const int sds[], int counts[],
    size_t nsds, int nclients)
{
	struct sockaddr_in6 sin6;
	struct timespec deadline, now;
	struct pollfd *pfds;
	size_t i;
	ssize_t n;
	long remaining;
	int error, sd, total;
	char ch;

	pfds = calloc(nsds, sizeof(*pfds));
	ATF_REQUIRE(pfds != NULL);
	for (i = 0; i < nsds; i++) {
		counts[i] = 0;
		pfds[i].fd = sds[i];
		pfds[i].events = POLLIN;
	}
	ch = 42;
	for (int c = 0; c < nclients; c++) {
		sd = socket(dst->sa_family, SOCK_DGRAM, 0);
		ATF_REQUIRE_MSG(sd >= 0, "socket() failed: %s",
		    strerror(errno));
		if (dst->sa_family == AF_INET6) {
			memcpy(&sin6, dst, sizeof(sin6));
		} else {
			memset(&sin6, 0, sizeof(sin6));
		}
		if (dst->sa_family == AF_INET6 &&
		    IN6_IS_ADDR_V4MAPPED(&sin6.sin6_addr)) {
			error = setsockopt(sd, IPPROTO_IPV6, IPV6_V6ONLY,
			    (int[]){0}, sizeof(int));
			ATF_REQUIRE_MSG(error == 0,
			    "setsockopt(IPV6_V6ONLY) failed: %s",
			    strerror(errno));
		}
		n = sendto(sd, &ch, sizeof(ch), 0, dst, dst->sa_len);
		ATF_REQUIRE_MSG(n == 1, "sendto() failed: %s",
		    strerror(errno));
		error = close(sd);
		ATF_REQUIRE_MSG(error == 0, "close() failed: %s",
		    strerror(errno));
	}
	ATF_REQUIRE(clock_gettime(CLOCK_MONOTONIC, &deadline) == 0);
	deadline.tv_sec += 5;
	total = 0;
	while (total < nclients) {
		ATF_REQUIRE(clock_gettime(CLOCK_MONOTONIC, &now) == 0);
		remaining = (deadline.tv_sec - now.tv_sec) * 1000 +
		    (deadline.tv_nsec - now.tv_nsec) / 1000000;
		ATF_REQUIRE_MSG(remaining > 0,
		    "received %d of %d datagrams before deadline",
		    total, nclients);
		error = poll(pfds, nsds, remaining);
		if (error == -1 && errno == EINTR)
			continue;
		ATF_REQUIRE_MSG(error >= 0, "poll() failed: %s",
		    strerror(errno));
		for (i = 0; i < nsds; i++) {
			ATF_REQUIRE_MSG((pfds[i].revents &
			    (POLLERR | POLLHUP | POLLNVAL)) == 0,
			    "unexpected poll events %#x on socket %zu",
			    pfds[i].revents, i);
			if ((pfds[i].revents & POLLIN) == 0)
				continue;
			for (;;) {
				n = recv(sds[i], &ch, sizeof(ch), MSG_DONTWAIT);
				if (n == -1 && errno == EINTR)
					continue;
				if (n == -1 && errno == EAGAIN)
					break;
				ATF_REQUIRE_MSG(n == 1,
				    "recv() returned %zd: %s", n,
				    n == -1 ? strerror(errno) :
				    "unexpected datagram length");
				ATF_REQUIRE_EQ(42, ch);
				counts[i]++;
				total++;
				ATF_REQUIRE(total <= nclients);
			}
		}
	}
	free(pfds);
}

static void
lb_tcp_accept_expected(const struct sockaddr *dst, const int sds[],
    const bool expected[], size_t nsds, int nconns, int counts[])
{
	struct pollfd *pfds;
	const struct linger lopt = { 1, 0 };
	size_t i, selected;
	int accepted, client, error, ready;

	pfds = calloc(nsds, sizeof(*pfds));
	ATF_REQUIRE_MSG(pfds != NULL, "calloc() failed: %s", strerror(errno));
	for (i = 0; i < nsds; i++) {
		pfds[i].fd = sds[i];
		pfds[i].events = POLLIN;
		counts[i] = 0;
	}
	while (nconns-- > 0) {
		client = socket(dst->sa_family, SOCK_STREAM, 0);
		ATF_REQUIRE_MSG(client >= 0, "socket() failed: %s",
		    strerror(errno));
		error = connect(client, dst, dst->sa_len);
		ATF_REQUIRE_MSG(error == 0, "connect() failed: %s",
		    strerror(errno));
		error = poll(pfds, nsds, 2000);
		ATF_REQUIRE_MSG(error > 0, "no listener became readable");
		ready = 0;
		selected = nsds;
		for (i = 0; i < nsds; i++) {
			ATF_REQUIRE_MSG((pfds[i].revents &
			    (POLLERR | POLLHUP | POLLNVAL)) == 0,
			    "unexpected poll events %#x on listener %zu",
			    pfds[i].revents, i);
			if ((pfds[i].revents & POLLIN) == 0)
				continue;
			ATF_REQUIRE_MSG(expected[i],
			    "unexpected listener %zu accepted a connection", i);
			ready++;
			selected = i;
		}
		ATF_REQUIRE_MSG(ready == 1,
		    "expected one readable listener, got %d", ready);
		accepted = accept(sds[selected], NULL, NULL);
		ATF_REQUIRE_MSG(accepted >= 0, "accept() failed: %s",
		    strerror(errno));
		counts[selected]++;
		error = close(accepted);
		ATF_REQUIRE_MSG(error == 0, "close() failed: %s",
		    strerror(errno));
		error = setsockopt(client, SOL_SOCKET, SO_LINGER, &lopt,
		    sizeof(lopt));
		ATF_REQUIRE_MSG(error == 0, "setsockopt(SO_LINGER) failed: %s",
		    strerror(errno));
		error = close(client);
		ATF_REQUIRE_MSG(error == 0, "close() failed: %s",
		    strerror(errno));
		for (i = 0; i < nsds; i++)
			pfds[i].revents = 0;
	}
	free(pfds);
}

ATF_TC_WITHOUT_HEAD(dualstack_udp_no_lb);
ATF_TC_BODY(dualstack_udp_no_lb, tc)
{
	struct sockaddr_in sin = {
		.sin_family = AF_INET,
		.sin_len = sizeof(sin),
		.sin_addr = { htonl(INADDR_LOOPBACK) },
	};
	struct sockaddr_in6 sin6 = {
		.sin6_family = AF_INET6,
		.sin6_len = sizeof(sin6),
		.sin6_addr = IN6ADDR_ANY_INIT,
	};
	socklen_t slen;
	int counts[1], sd;

	sd = socket(PF_INET6, SOCK_DGRAM | SOCK_NONBLOCK, 0);
	ATF_REQUIRE(sd >= 0);
	ATF_REQUIRE(setsockopt(sd, IPPROTO_IPV6, IPV6_V6ONLY,
	    (int[]){0}, sizeof(int)) == 0);
	ATF_REQUIRE(bind(sd, (struct sockaddr *)&sin6, sizeof(sin6)) == 0);
	slen = sizeof(sin6);
	ATF_REQUIRE(getsockname(sd, (struct sockaddr *)&sin6, &slen) == 0);
	sin.sin_port = sin6.sin6_port;
	sin6.sin6_addr = in6addr_loopback;
	lb_udp_spray((struct sockaddr *)&sin, &sd, counts, 1, 128);
	ATF_CHECK_EQ(128, counts[0]);
	lb_udp_spray((struct sockaddr *)&sin6, &sd, counts, 1, 128);
	ATF_CHECK_EQ(128, counts[0]);
	ATF_REQUIRE(close(sd) == 0);
}

/*
 * Dual-stack sockets bound to the unspecified address form a load balance
 * group for both IPv4 and IPv6 traffic.
 */
ATF_TC_WITHOUT_HEAD(dualstack_udp);
ATF_TC_BODY(dualstack_udp, tc)
{
	struct sockaddr_in sin = {
		.sin_family = AF_INET,
		.sin_len = sizeof(sin),
		.sin_addr = { htonl(INADDR_LOOPBACK) },
	};
	struct sockaddr_in6 sin6 = {
		.sin6_family = AF_INET6,
		.sin6_len = sizeof(sin6),
		.sin6_addr = IN6ADDR_LOOPBACK_INIT,
	};
	size_t i;
	int counts[16], sds[16];
	uint16_t port;

	port = 0;
	for (i = 0; i < nitems(sds); i++)
		sds[i] = lb_dualstack_socket(SOCK_DGRAM, &port, -1);
	sin.sin_port = sin6.sin6_port = port;

	lb_udp_spray((struct sockaddr *)&sin, sds, counts, nitems(sds), 512);
	for (i = 0; i < nitems(sds); i++)
		ATF_REQUIRE_MSG(counts[i] > 0,
		    "IPv4 datagrams not balanced, socket %zu got none", i);

	lb_udp_spray((struct sockaddr *)&sin6, sds, counts, nitems(sds), 512);
	for (i = 0; i < nitems(sds); i++)
		ATF_REQUIRE_MSG(counts[i] > 0,
		    "IPv6 datagrams not balanced, socket %zu got none", i);

	for (i = 0; i < nitems(sds); i++)
		ATF_REQUIRE(close(sds[i]) == 0);
}

ATF_TC_WITHOUT_HEAD(dualstack_tcp);
ATF_TC_BODY(dualstack_tcp, tc)
{
	struct sockaddr_in sin = {
		.sin_family = AF_INET,
		.sin_len = sizeof(sin),
		.sin_addr = { htonl(INADDR_LOOPBACK) },
	};
	struct sockaddr_in6 sin6 = {
		.sin6_family = AF_INET6,
		.sin6_len = sizeof(sin6),
		.sin6_addr = IN6ADDR_LOOPBACK_INIT,
	};
	size_t i;
	int error, sds[16];
	uint16_t port;

	port = 0;
	for (i = 0; i < nitems(sds); i++) {
		sds[i] = lb_dualstack_socket(SOCK_STREAM, &port, -1);
		error = listen(sds[i], 1);
		ATF_REQUIRE_MSG(error == 0, "listen() failed: %s",
		    strerror(errno));
	}
	sin.sin_port = sin6.sin6_port = port;

	lb_simple_accept_loop(PF_INET, (struct sockaddr *)&sin, sds,
	    nitems(sds), 4096);
	lb_simple_accept_loop(PF_INET6, (struct sockaddr *)&sin6, sds,
	    nitems(sds), 4096);

	for (i = 0; i < nitems(sds); i++)
		ATF_REQUIRE(close(sds[i]) == 0);
}

ATF_TC_WITHOUT_HEAD(dualstack_udp_ipv4_mapped);
ATF_TC_BODY(dualstack_udp_ipv4_mapped, tc)
{
	struct sockaddr_in6 sin6 = {
		.sin6_family = AF_INET6,
		.sin6_len = sizeof(sin6),
	};
	size_t i;
	int counts[16], sds[16];
	uint16_t port;

	lb_ipv4_mapped_addr(&sin6.sin6_addr);
	port = 0;
	for (i = 0; i < nitems(sds); i++)
		sds[i] = lb_dualstack_socket(SOCK_DGRAM, &port, -1);
	sin6.sin6_port = port;

	lb_udp_spray((struct sockaddr *)&sin6, sds, counts, nitems(sds), 512);
	for (i = 0; i < nitems(sds); i++)
		ATF_REQUIRE_MSG(counts[i] > 0,
		    "IPv4-mapped datagrams not balanced, socket %zu got none",
		    i);

	for (i = 0; i < nitems(sds); i++)
		ATF_REQUIRE(close(sds[i]) == 0);
}

ATF_TC_WITHOUT_HEAD(dualstack_tcp_ipv4_mapped);
ATF_TC_BODY(dualstack_tcp_ipv4_mapped, tc)
{
	struct sockaddr_in6 sin6 = {
		.sin6_family = AF_INET6,
		.sin6_len = sizeof(sin6),
	};
	size_t i;
	int error, sds[16];
	uint16_t port;

	lb_ipv4_mapped_addr(&sin6.sin6_addr);
	port = 0;
	for (i = 0; i < nitems(sds); i++) {
		sds[i] = lb_dualstack_socket(SOCK_STREAM, &port, -1);
		error = listen(sds[i], 1);
		ATF_REQUIRE_MSG(error == 0, "listen() failed: %s",
		    strerror(errno));
	}
	sin6.sin6_port = port;

	lb_simple_accept_loop(PF_INET6, (struct sockaddr *)&sin6, sds,
	    nitems(sds), 4096);

	for (i = 0; i < nitems(sds); i++)
		ATF_REQUIRE(close(sds[i]) == 0);
}

/*
 * An IPv4 socket takes precedence over a dual-stack load balance group for
 * IPv4 traffic, as it does over a single dual-stack socket.
 */
ATF_TC_WITHOUT_HEAD(dualstack_udp_ipv4_precedence);
ATF_TC_BODY(dualstack_udp_ipv4_precedence, tc)
{
	struct sockaddr_in sin = {
		.sin_family = AF_INET,
		.sin_len = sizeof(sin),
		.sin_addr = { htonl(INADDR_ANY) },
	};
	struct sockaddr_in6 sin6 = {
		.sin6_family = AF_INET6,
		.sin6_len = sizeof(sin6),
		.sin6_addr = IN6ADDR_LOOPBACK_INIT,
	};
	socklen_t slen;
	size_t i;
	int counts[5], error, sds[5];
	uint16_t port;

	sds[0] = socket(PF_INET, SOCK_DGRAM | SOCK_NONBLOCK, 0);
	ATF_REQUIRE_MSG(sds[0] >= 0, "socket() failed: %s", strerror(errno));
	error = bind(sds[0], (struct sockaddr *)&sin, sizeof(sin));
	ATF_REQUIRE_MSG(error == 0, "bind() failed: %s", strerror(errno));
	slen = sizeof(sin);
	error = getsockname(sds[0], (struct sockaddr *)&sin, &slen);
	ATF_REQUIRE_MSG(error == 0, "getsockname() failed: %s",
	    strerror(errno));
	port = sin.sin_port;
	for (i = 1; i < nitems(sds); i++)
		sds[i] = lb_dualstack_socket(SOCK_DGRAM, &port, -1);
	sin.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	sin6.sin6_port = port;

	lb_udp_spray((struct sockaddr *)&sin, sds, counts, nitems(sds), 64);
	ATF_REQUIRE_MSG(counts[0] == 64,
	    "IPv4 socket got %d of 64 IPv4 datagrams", counts[0]);

	lb_udp_spray((struct sockaddr *)&sin6, sds, counts, nitems(sds), 64);
	ATF_REQUIRE_MSG(counts[0] == 0, "IPv4 socket got IPv6 datagrams");

	for (i = 0; i < nitems(sds); i++)
		ATF_REQUIRE(close(sds[i]) == 0);
}

/*
 * IPv6-only load balance groups take precedence over dual-stack groups.
 */
ATF_TC_WITHOUT_HEAD(dualstack_udp_ipv6_precedence);
ATF_TC_BODY(dualstack_udp_ipv6_precedence, tc)
{
	struct sockaddr_in sin = {
		.sin_family = AF_INET,
		.sin_len = sizeof(sin),
		.sin_addr = { htonl(INADDR_LOOPBACK) },
	};
	struct sockaddr_in6 sin6 = {
		.sin6_family = AF_INET6,
		.sin6_len = sizeof(sin6),
		.sin6_addr = IN6ADDR_LOOPBACK_INIT,
	};
	size_t i;
	int counts[8], sds[8];
	uint16_t port;

	port = 0;
	for (i = 0; i < nitems(sds) / 2; i++)
		sds[i] = lb_dualstack_socket(SOCK_DGRAM, &port, -1);
	for (; i < nitems(sds); i++)
		sds[i] = lb_v6only_socket(SOCK_DGRAM, &port);
	sin6.sin6_port = port;

	lb_udp_spray((struct sockaddr *)&sin6, sds, counts, nitems(sds), 256);
	for (i = 0; i < nitems(sds) / 2; i++)
		ATF_REQUIRE_MSG(counts[i] == 0,
		    "dual-stack group socket %zu received IPv6 datagrams", i);
	for (; i < nitems(sds); i++)
		ATF_REQUIRE_MSG(counts[i] > 0,
		    "IPv6-only group socket %zu received no datagrams",
		    i - nitems(sds) / 2);

	sin.sin_port = sin6.sin6_port = port;
	lb_udp_spray((struct sockaddr *)&sin, sds, counts, nitems(sds), 256);
	for (i = 0; i < nitems(sds) / 2; i++)
		ATF_REQUIRE_MSG(counts[i] > 0,
		    "dual-stack group socket %zu received no IPv4 datagrams",
		    i);
	for (; i < nitems(sds); i++)
		ATF_REQUIRE_MSG(counts[i] == 0,
		    "IPv6-only group socket %zu received IPv4 datagrams",
		    i - nitems(sds) / 2);

	for (i = 0; i < nitems(sds); i++)
		ATF_REQUIRE(close(sds[i]) == 0);
}

static void
lb_dualstack_tcp_ipv6_precedence(bool v6only_first)
{
	struct sockaddr_in sin = {
		.sin_family = AF_INET,
		.sin_len = sizeof(sin),
		.sin_addr = { htonl(INADDR_LOOPBACK) },
	};
	struct sockaddr_in6 sin6 = {
		.sin6_family = AF_INET6,
		.sin6_len = sizeof(sin6),
		.sin6_addr = IN6ADDR_LOOPBACK_INIT,
	};
	bool expected[4], is_v6only[4];
	uint16_t port;
	int counts[4], sds[4], error;

	port = 0;
	for (size_t i = 0; i < nitems(sds); i++) {
		is_v6only[i] = (i < 2) == v6only_first;
		if (is_v6only[i])
			sds[i] = lb_v6only_socket(SOCK_STREAM, &port);
		else
			sds[i] = lb_dualstack_socket(SOCK_STREAM, &port, -1);
		error = listen(sds[i], 1);
		ATF_REQUIRE_MSG(error == 0, "listen() failed: %s",
		    strerror(errno));
	}
	sin.sin_port = sin6.sin6_port = port;

	for (size_t i = 0; i < nitems(sds); i++)
		expected[i] = is_v6only[i];
	lb_tcp_accept_expected((struct sockaddr *)&sin6, sds, expected,
	    nitems(sds), 64, counts);
	for (size_t i = 0; i < nitems(sds); i++)
		ATF_REQUIRE_MSG(counts[i] == 0 || is_v6only[i],
		    "dual-stack listener %zu accepted IPv6 traffic", i);
	ATF_REQUIRE(counts[0] + counts[1] + counts[2] + counts[3] == 64);

	for (size_t i = 0; i < nitems(sds); i++)
		expected[i] = !is_v6only[i];
	lb_tcp_accept_expected((struct sockaddr *)&sin, sds, expected,
	    nitems(sds), 64, counts);
	for (size_t i = 0; i < nitems(sds); i++)
		ATF_REQUIRE_MSG(counts[i] == 0 || !is_v6only[i],
		    "IPv6-only listener %zu accepted IPv4 traffic", i);
	ATF_REQUIRE(counts[0] + counts[1] + counts[2] + counts[3] == 64);
	for (size_t i = 0; i < nitems(sds); i++)
		if (!is_v6only[i])
			ATF_REQUIRE_MSG(counts[i] > 0,
			    "dual-stack listener %zu accepted no IPv4 traffic",
			    i);

	for (size_t i = 0; i < nitems(sds); i++)
		ATF_REQUIRE(close(sds[i]) == 0);
}

ATF_TC_WITHOUT_HEAD(dualstack_tcp_ipv6_precedence);
ATF_TC_BODY(dualstack_tcp_ipv6_precedence, tc)
{
	lb_dualstack_tcp_ipv6_precedence(true);
	lb_dualstack_tcp_ipv6_precedence(false);
}

/*
 * Dual-stack groups must receive IPv4 traffic even when bind_all_fibs is
 * disabled and the group belongs to another FIB.
 */
ATF_TC_WITHOUT_HEAD(dualstack_udp_other_fib);
ATF_TC_BODY(dualstack_udp_other_fib, tc)
{
	struct sockaddr_in sin = {
		.sin_family = AF_INET,
		.sin_len = sizeof(sin),
		.sin_addr = { htonl(INADDR_LOOPBACK) },
	};
	struct sockaddr_in6 sin6 = {
		.sin6_family = AF_INET6,
		.sin6_len = sizeof(sin6),
		.sin6_addr = IN6ADDR_LOOPBACK_INIT,
	};
	size_t i, len;
	int bind_all_fibs, counts[8], error, nfibs, sds[8];
	uint16_t port;

	len = sizeof(nfibs);
	error = sysctlbyname("net.fibs", &nfibs, &len, NULL, 0);
	ATF_REQUIRE_MSG(error == 0, "sysctlbyname(net.fibs) failed: %s",
	    strerror(errno));
	if (nfibs < 2)
		atf_tc_skip("requires at least two FIBs");
	len = sizeof(bind_all_fibs);
	error = sysctlbyname("net.inet.udp.bind_all_fibs", &bind_all_fibs,
	    &len, NULL, 0);
	ATF_REQUIRE_MSG(error == 0,
	    "sysctlbyname(net.inet.udp.bind_all_fibs) failed: %s",
	    strerror(errno));
	if (bind_all_fibs != 0)
		atf_tc_skip("requires net.inet.udp.bind_all_fibs=0");

	port = 0;
	for (i = 0; i < nitems(sds); i++)
		sds[i] = lb_dualstack_socket(SOCK_DGRAM, &port, 1);
	sin.sin_port = sin6.sin6_port = port;

	lb_udp_spray((struct sockaddr *)&sin, sds, counts, nitems(sds), 512);
	for (i = 0; i < nitems(sds); i++)
		ATF_REQUIRE_MSG(counts[i] > 0,
		    "IPv4 datagrams not balanced across FIBs, socket %zu got none",
		    i);

	lb_udp_spray((struct sockaddr *)&sin6, sds, counts, nitems(sds), 512);
	for (i = 0; i < nitems(sds); i++)
		ATF_REQUIRE_MSG(counts[i] > 0,
		    "IPv6 datagrams not balanced across FIBs, "
		    "socket %zu got none", i);

	for (i = 0; i < nitems(sds); i++)
		ATF_REQUIRE(close(sds[i]) == 0);
}

/*
 * A failed connect() must leave a dual-stack group member able to receive
 * traffic of both address families.
 */
ATF_TC_WITHOUT_HEAD(dualstack_udp_connect_fail);
ATF_TC_BODY(dualstack_udp_connect_fail, tc)
{
	struct sockaddr_in sin = {
		.sin_family = AF_INET,
		.sin_len = sizeof(sin),
		.sin_addr = { htonl(INADDR_LOOPBACK) },
	};
	struct sockaddr_in6 sin6 = {
		.sin6_family = AF_INET6,
		.sin6_len = sizeof(sin6),
		.sin6_addr = IN6ADDR_LOOPBACK_INIT,
	};
	struct sockaddr_in6 peer6 = {
		.sin6_family = AF_INET6,
		.sin6_len = sizeof(peer6),
		.sin6_port = htons(9),
	};
	size_t i;
	int counts[8], error, sds[8], tmp;
	uint16_t port;

	port = 0;
	for (i = 0; i < nitems(sds); i++)
		sds[i] = lb_dualstack_socket(SOCK_DGRAM, &port, -1);
	sin.sin_port = sin6.sin6_port = port;

	/* Leaves the group, the next attempt fails with a duplicate tuple. */
	tmp = lb_dualstack_socket(SOCK_DGRAM, &port, -1);
	peer6.sin6_addr.s6_addr[10] = peer6.sin6_addr.s6_addr[11] = 0xff;
	peer6.sin6_addr.s6_addr[12] = 127;
	peer6.sin6_addr.s6_addr[15] = 1;
	error = connect(tmp, (struct sockaddr *)&peer6, sizeof(peer6));
	ATF_REQUIRE_MSG(error == 0, "connect() failed: %s", strerror(errno));
	error = connect(sds[0], (struct sockaddr *)&peer6, sizeof(peer6));
	ATF_REQUIRE_MSG(error == -1 && errno == EADDRINUSE,
	    "connect() returned %d: %s", error, strerror(errno));

	lb_udp_spray((struct sockaddr *)&sin6, sds, counts, nitems(sds), 256);
	for (i = 0; i < nitems(sds); i++)
		ATF_REQUIRE_MSG(counts[i] > 0,
		    "IPv6 datagrams not balanced, socket %zu got none", i);
	lb_udp_spray((struct sockaddr *)&sin, sds, counts, nitems(sds), 256);
	for (i = 0; i < nitems(sds); i++)
		ATF_REQUIRE_MSG(counts[i] > 0,
		    "IPv4 datagrams not balanced, socket %zu got none", i);

	ATF_REQUIRE(close(tmp) == 0);
	for (i = 0; i < nitems(sds); i++)
		ATF_REQUIRE(close(sds[i]) == 0);
}

ATF_TC_WITHOUT_HEAD(dualstack_tcp_connect_fail);
ATF_TC_BODY(dualstack_tcp_connect_fail, tc)
{
	struct sockaddr_in sin = {
		.sin_family = AF_INET,
		.sin_len = sizeof(sin),
		.sin_addr = { htonl(INADDR_LOOPBACK) },
	};
	struct sockaddr_in6 sin6 = {
		.sin6_family = AF_INET6,
		.sin6_len = sizeof(sin6),
		.sin6_addr = IN6ADDR_LOOPBACK_INIT,
	};
	struct sockaddr_in6 peer6 = {
		.sin6_family = AF_INET6,
		.sin6_len = sizeof(peer6),
		.sin6_port = htons(9),
	};
	size_t i;
	int error, sds[8];
	uint16_t port;

	peer6.sin6_addr.s6_addr[10] = peer6.sin6_addr.s6_addr[11] = 0xff;
	peer6.sin6_addr.s6_addr[12] = 127;
	peer6.sin6_addr.s6_addr[15] = 1;
	port = 0;
	for (i = 0; i < nitems(sds); i++) {
		sds[i] = lb_dualstack_socket(SOCK_STREAM, &port, -1);
		/* SO_REUSEPORT_LB sockets can't connect. */
		error = connect(sds[i], (struct sockaddr *)&peer6,
		    sizeof(peer6));
		ATF_REQUIRE_MSG(error == -1 && errno == EOPNOTSUPP,
		    "connect() returned %d: %s", error, strerror(errno));
		error = listen(sds[i], 1);
		ATF_REQUIRE_MSG(error == 0, "listen() failed: %s",
		    strerror(errno));
	}
	sin.sin_port = sin6.sin6_port = port;

	lb_simple_accept_loop(PF_INET6, (struct sockaddr *)&sin6, sds,
	    nitems(sds), 2048);
	lb_simple_accept_loop(PF_INET, (struct sockaddr *)&sin, sds,
	    nitems(sds), 2048);

	for (i = 0; i < nitems(sds); i++)
		ATF_REQUIRE(close(sds[i]) == 0);
}

static void
lb_tcp_sendto_fail(int flags)
{
	struct sockaddr_in6 peer6 = {
		.sin6_family = AF_INET6,
		.sin6_len = sizeof(peer6),
		.sin6_port = htons(9),
	};
	struct sockaddr_in6 sin6 = {
		.sin6_family = AF_INET6,
		.sin6_len = sizeof(sin6),
		.sin6_addr = IN6ADDR_LOOPBACK_INIT,
	};
	struct sockaddr_in sin = {
		.sin_family = AF_INET,
		.sin_len = sizeof(sin),
		.sin_addr = { htonl(INADDR_LOOPBACK) },
	};
	struct pollfd pfd;
	const struct sockaddr *dst;
	uint16_t port;
	ssize_t n;
	int accepted, client, error, sd;
	char ch = 'x';

	peer6.sin6_addr.s6_addr[10] = peer6.sin6_addr.s6_addr[11] = 0xff;
	peer6.sin6_addr.s6_addr[12] = 127;
	peer6.sin6_addr.s6_addr[15] = 1;
	port = 0;
	sd = lb_dualstack_socket(SOCK_STREAM, &port, -1);
	n = sendto(sd, &ch, sizeof(ch), flags,
	    (struct sockaddr *)&peer6, sizeof(peer6));
	ATF_REQUIRE_MSG(n == -1 && errno == EOPNOTSUPP,
	    "sendto() returned %zd: %s", n, strerror(errno));
	ATF_REQUIRE(listen(sd, 1) == 0);
	sin.sin_port = sin6.sin6_port = port;
	for (int family = 0; family < 2; family++) {
		dst = family == 0 ? (struct sockaddr *)&sin6 :
		    (struct sockaddr *)&sin;
		client = socket(dst->sa_family, SOCK_STREAM | SOCK_NONBLOCK, 0);
		ATF_REQUIRE(client >= 0);
		error = connect(client, dst, dst->sa_len);
		ATF_REQUIRE_MSG(error == 0 || errno == EINPROGRESS,
		    "connect() failed: %s", strerror(errno));
		pfd.fd = sd;
		pfd.events = POLLIN;
		pfd.revents = 0;
		ATF_REQUIRE_MSG(poll(&pfd, 1, 2000) == 1 &&
		    (pfd.revents & POLLIN) != 0,
		    "listener lost %s traffic after rejected sendto()",
		    family == 0 ? "IPv6" : "IPv4");
		accepted = accept(sd, NULL, NULL);
		ATF_REQUIRE(accepted >= 0);
		ATF_REQUIRE(close(accepted) == 0);
		ATF_REQUIRE(close(client) == 0);
	}
	ATF_REQUIRE(close(sd) == 0);
}

ATF_TC_WITHOUT_HEAD(dualstack_tcp_sendto_fail);
ATF_TC_BODY(dualstack_tcp_sendto_fail, tc)
{
	lb_tcp_sendto_fail(0);
}

ATF_TC_WITHOUT_HEAD(dualstack_tcp_sendto_oob_fail);
ATF_TC_BODY(dualstack_tcp_sendto_oob_fail, tc)
{
	lb_tcp_sendto_fail(MSG_OOB);
}

static void
lb_ipv6_specific_precedence(int type)
{
	struct sockaddr_in6 sin6 = {
		.sin6_family = AF_INET6,
		.sin6_len = sizeof(sin6),
		.sin6_addr = IN6ADDR_LOOPBACK_INIT,
	};
	struct pollfd pfd[2];
	socklen_t slen;
	uint16_t port;
	int counts[2], csd, error, sds[2];

	sds[0] = socket(PF_INET6, type | SOCK_NONBLOCK, 0);
	ATF_REQUIRE(sds[0] >= 0);
	ATF_REQUIRE(setsockopt(sds[0], IPPROTO_IPV6, IPV6_V6ONLY,
	    (int[]){1}, sizeof(int)) == 0);
	ATF_REQUIRE(setsockopt(sds[0], SOL_SOCKET, SO_REUSEPORT,
	    (int[]){1}, sizeof(int)) == 0);
	ATF_REQUIRE(bind(sds[0], (struct sockaddr *)&sin6, sizeof(sin6)) == 0);
	slen = sizeof(sin6);
	ATF_REQUIRE(getsockname(sds[0], (struct sockaddr *)&sin6, &slen) == 0);
	port = sin6.sin6_port;
	sds[1] = lb_dualstack_socket(type, &port, -1);

	if (type == SOCK_DGRAM) {
		lb_udp_spray((struct sockaddr *)&sin6, sds, counts,
		    nitems(sds), 64);
		ATF_CHECK_MSG(counts[0] == 64,
		    "specific IPv6 socket got %d of 64 datagrams", counts[0]);
		ATF_CHECK_EQ(0, counts[1]);
	} else {
		ATF_REQUIRE(listen(sds[0], 1) == 0);
		ATF_REQUIRE(listen(sds[1], 1) == 0);
		csd = socket(PF_INET6, SOCK_STREAM, 0);
		ATF_REQUIRE(csd >= 0);
		ATF_REQUIRE(connect(csd, (struct sockaddr *)&sin6,
		    sizeof(sin6)) == 0);
		for (size_t i = 0; i < nitems(pfd); i++) {
			pfd[i].fd = sds[i];
			pfd[i].events = POLLIN;
			pfd[i].revents = 0;
		}
		error = poll(pfd, nitems(pfd), 1000);
		ATF_REQUIRE_MSG(error > 0, "no listener became readable");
		ATF_CHECK_MSG((pfd[0].revents & POLLIN) != 0,
		    "specific IPv6 listener did not receive the connection");
		ATF_CHECK_EQ(0, pfd[1].revents);
		ATF_REQUIRE(close(csd) == 0);
	}
	ATF_REQUIRE(close(sds[0]) == 0);
	ATF_REQUIRE(close(sds[1]) == 0);
}

ATF_TC_WITHOUT_HEAD(dualstack_udp_ipv6_specific);
ATF_TC_BODY(dualstack_udp_ipv6_specific, tc)
{
	lb_ipv6_specific_precedence(SOCK_DGRAM);
}

ATF_TC_WITHOUT_HEAD(dualstack_tcp_ipv6_specific);
ATF_TC_BODY(dualstack_tcp_ipv6_specific, tc)
{
	lb_ipv6_specific_precedence(SOCK_STREAM);
}

static void
lb_dualstack_specific_precedence(int type, bool specific_first)
{
	struct in6_addr wildcard = IN6ADDR_ANY_INIT;
	struct in6_addr specific = IN6ADDR_LOOPBACK_INIT;
	struct sockaddr_in sin = {
		.sin_family = AF_INET,
		.sin_len = sizeof(sin),
		.sin_addr = { htonl(INADDR_LOOPBACK) },
	};
	struct sockaddr_in6 sin6 = {
		.sin6_family = AF_INET6,
		.sin6_len = sizeof(sin6),
		.sin6_addr = IN6ADDR_LOOPBACK_INIT,
	};
	bool expected[4], is_specific[4], make_specific;
	uint16_t port;
	int counts[4], sds[4], error;

	port = 0;
	for (size_t i = 0; i < nitems(sds); i++) {
		make_specific = (i < 2) == specific_first;
		is_specific[i] = make_specific;
		sds[i] = lb_dualstack_socket_addr(type,
		    make_specific ? &specific : &wildcard, &port);
		if (type == SOCK_STREAM) {
			error = listen(sds[i], 1);
			ATF_REQUIRE_MSG(error == 0, "listen() failed: %s",
			    strerror(errno));
		}
	}
	sin.sin_port = sin6.sin6_port = port;

	for (size_t i = 0; i < nitems(sds); i++)
		expected[i] = is_specific[i];
	if (type == SOCK_DGRAM) {
		lb_udp_spray((struct sockaddr *)&sin6, sds, counts,
		    nitems(sds), 64);
	} else {
		lb_tcp_accept_expected((struct sockaddr *)&sin6, sds, expected,
		    nitems(sds), 64, counts);
	}
	for (size_t i = 0; i < nitems(sds); i++)
		ATF_REQUIRE_MSG(counts[i] == 0 || is_specific[i],
		    "wildcard socket %zu received specific-address traffic", i);
	ATF_REQUIRE(counts[0] + counts[1] + counts[2] + counts[3] == 64);
	for (size_t i = 0; i < nitems(sds); i++)
		if (is_specific[i])
			ATF_REQUIRE_MSG(counts[i] > 0,
			    "specific socket %zu received no IPv6 traffic", i);

	for (size_t i = 0; i < nitems(sds); i++)
		expected[i] = !is_specific[i];
	if (type == SOCK_DGRAM) {
		lb_udp_spray((struct sockaddr *)&sin, sds, counts,
		    nitems(sds), 64);
	} else {
		lb_tcp_accept_expected((struct sockaddr *)&sin, sds, expected,
		    nitems(sds), 64, counts);
	}
	for (size_t i = 0; i < nitems(sds); i++)
		ATF_REQUIRE_MSG(counts[i] == 0 || !is_specific[i],
		    "specific socket %zu received wildcard-only traffic", i);
	ATF_REQUIRE(counts[0] + counts[1] + counts[2] + counts[3] == 64);
	for (size_t i = 0; i < nitems(sds); i++)
		if (!is_specific[i])
			ATF_REQUIRE_MSG(counts[i] > 0,
			    "wildcard socket %zu received no IPv4 traffic", i);

	for (size_t i = 0; i < nitems(sds); i++)
		ATF_REQUIRE(close(sds[i]) == 0);
}

ATF_TC_WITHOUT_HEAD(dualstack_udp_specific_precedence);
ATF_TC_BODY(dualstack_udp_specific_precedence, tc)
{
	lb_dualstack_specific_precedence(SOCK_DGRAM, true);
	lb_dualstack_specific_precedence(SOCK_DGRAM, false);
}

ATF_TC_WITHOUT_HEAD(dualstack_tcp_specific_precedence);
ATF_TC_BODY(dualstack_tcp_specific_precedence, tc)
{
	lb_dualstack_specific_precedence(SOCK_STREAM, true);
	lb_dualstack_specific_precedence(SOCK_STREAM, false);
}

ATF_TC(dualstack_udp_ipv4_jail_precedence);
ATF_TC_HEAD(dualstack_udp_ipv4_jail_precedence, tc)
{
	atf_tc_set_md_var(tc, "require.user", "root");
	atf_tc_set_md_var(tc, "timeout", "15");
}
ATF_TC_BODY(dualstack_udp_ipv4_jail_precedence, tc)
{
	struct in_addr ip4[] = {
		{ htonl(0x7f000002) }, { htonl(0x7f000003) },
	};
	struct in6_addr ip6[] = {
		IN6ADDR_LOOPBACK_INIT, IN6ADDR_LOOPBACK_INIT,
	};
	struct jail jconf = {
		.version = JAIL_API_VERSION,
		.path = __DECONST(char *, "/"),
		.hostname = __DECONST(char *, "lb-precedence"),
		.ip4s = nitems(ip4),
		.ip4 = ip4,
		.ip6s = nitems(ip6),
		.ip6 = ip6,
	};
	struct sockaddr_in sin = {
		.sin_family = AF_INET,
		.sin_len = sizeof(sin),
		.sin_addr = { htonl(INADDR_ANY) },
	};
	struct sockaddr_in6 sin6 = {
		.sin6_family = AF_INET6,
		.sin6_len = sizeof(sin6),
		.sin6_addr = IN6ADDR_LOOPBACK_INIT,
	};
	struct pollfd pfd;
	socklen_t slen;
	pid_t child;
	uint16_t port;
	int counts[5], sds[5], status, syncfd[2];
	char ch;

	ip6[1].s6_addr[15] = 2;
	sds[0] = socket(PF_INET, SOCK_DGRAM | SOCK_NONBLOCK, 0);
	ATF_REQUIRE(sds[0] >= 0);
	ATF_REQUIRE(bind(sds[0], (struct sockaddr *)&sin, sizeof(sin)) == 0);
	slen = sizeof(sin);
	ATF_REQUIRE(getsockname(sds[0], (struct sockaddr *)&sin, &slen) == 0);
	port = sin.sin_port;
	for (size_t i = 1; i < nitems(sds); i++)
		sds[i] = lb_dualstack_socket(SOCK_DGRAM, &port, -1);
	ATF_REQUIRE(socketpair(AF_UNIX, SOCK_STREAM, 0, syncfd) == 0);
	child = fork();
	ATF_REQUIRE(child >= 0);
	if (child == 0) {
		alarm(10);
		close(syncfd[0]);
		for (size_t i = 0; i < nitems(sds); i++)
			close(sds[i]);
		ATF_REQUIRE(jail(&jconf) > 0);
		sds[0] = lb_dualstack_socket(SOCK_DGRAM, &port, -1);
		slen = sizeof(sin6);
		ATF_REQUIRE(getsockname(sds[0], (struct sockaddr *)&sin6,
		    &slen) == 0);
		ATF_REQUIRE(IN6_IS_ADDR_UNSPECIFIED(&sin6.sin6_addr));
		ch = 1;
		ATF_REQUIRE(write(syncfd[1], &ch, sizeof(ch)) == sizeof(ch));
		ATF_REQUIRE(read(syncfd[1], &ch, sizeof(ch)) == sizeof(ch));
		_exit(0);
	}
	close(syncfd[1]);
	pfd.fd = syncfd[0];
	pfd.events = POLLIN;
	pfd.revents = 0;
	ATF_REQUIRE(poll(&pfd, 1, 5000) > 0);
	ATF_REQUIRE(read(syncfd[0], &ch, sizeof(ch)) == sizeof(ch));
	sin.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	lb_udp_spray((struct sockaddr *)&sin, sds, counts, nitems(sds), 64);
	ATF_REQUIRE(write(syncfd[0], &ch, sizeof(ch)) == sizeof(ch));
	ATF_REQUIRE(waitpid(child, &status, 0) == child);
	ATF_REQUIRE(WIFEXITED(status));
	ATF_REQUIRE_EQ(0, WEXITSTATUS(status));
	ATF_CHECK_MSG(counts[0] == 64,
	    "host IPv4 socket got %d of 64 datagrams", counts[0]);
	for (size_t i = 1; i < nitems(sds); i++)
		ATF_CHECK_MSG(counts[i] == 0,
		    "host dual-stack socket %zu received IPv4 traffic", i);

	sin6.sin6_port = port;
	lb_udp_spray((struct sockaddr *)&sin6, sds, counts, nitems(sds), 256);
	ATF_CHECK_EQ(0, counts[0]);
	for (size_t i = 1; i < nitems(sds); i++)
		ATF_CHECK_MSG(counts[i] > 0,
		    "host dual-stack socket %zu received no IPv6 traffic", i);
	ATF_REQUIRE(close(syncfd[0]) == 0);
	for (size_t i = 0; i < nitems(sds); i++)
		ATF_REQUIRE(close(sds[i]) == 0);
}

ATF_TC(dualstack_udp_ipv4_jail_ordinary);
ATF_TC_HEAD(dualstack_udp_ipv4_jail_ordinary, tc)
{
	atf_tc_set_md_var(tc, "require.user", "root");
	atf_tc_set_md_var(tc, "timeout", "15");
	atf_tc_set_md_var(tc, "is.exclusive", "true");
}
static void
lb_ipv4_jail_ordinary(bool host_lb)
{
	struct in_addr ip4[] = {
		{ htonl(INADDR_LOOPBACK) }, { htonl(INADDR_LOOPBACK + 1) },
	};
	struct in6_addr ip6[] = {
		IN6ADDR_LOOPBACK_INIT, IN6ADDR_LOOPBACK_INIT,
	};
	struct jail jconf = {
		.version = JAIL_API_VERSION,
		.path = __DECONST(char *, "/"),
		.hostname = __DECONST(char *, "lb-ipv4-jail-ordinary"),
		.ip4s = nitems(ip4),
		.ip4 = ip4,
		.ip6s = nitems(ip6),
		.ip6 = ip6,
	};
	struct sockaddr_in sin = {
		.sin_family = AF_INET,
		.sin_len = sizeof(sin),
		.sin_addr = { htonl(INADDR_LOOPBACK) },
	};
	struct sockaddr_in6 sin6 = {
		.sin6_family = AF_INET6,
		.sin6_len = sizeof(sin6),
	};
	socklen_t slen;
	pid_t child;
	uint16_t port = 0;
	int counts[3], host[2], sds[3], status;

	ip6[1].s6_addr[15] = 2;
	host[0] = host_lb ? lb_dualstack_socket(SOCK_DGRAM, &port, -1) : -1;
	host[1] = host_lb ? lb_dualstack_socket(SOCK_DGRAM, &port, -1) : -1;
	child = fork();
	ATF_REQUIRE(child >= 0);
	if (child == 0) {
		alarm(10);
		ATF_REQUIRE(jail(&jconf) > 0);
		sds[0] = socket(PF_INET6, SOCK_DGRAM | SOCK_NONBLOCK, 0);
		ATF_REQUIRE(sds[0] >= 0);
		ATF_REQUIRE(setsockopt(sds[0], IPPROTO_IPV6, IPV6_V6ONLY,
		    (int[]){0}, sizeof(int)) == 0);
		sin6.sin6_port = port;
		ATF_REQUIRE(bind(sds[0], (struct sockaddr *)&sin6,
		    sizeof(sin6)) == 0);
		slen = sizeof(sin6);
		ATF_REQUIRE(getsockname(sds[0], (struct sockaddr *)&sin6,
		    &slen) == 0);
		ATF_REQUIRE(IN6_IS_ADDR_UNSPECIFIED(&sin6.sin6_addr));
		/* Keep the inherited host sockets to detect wrong delivery. */
		if (host_lb) {
			sds[1] = host[0];
			sds[2] = host[1];
		}
		sin.sin_port = sin6.sin6_port;
		lb_udp_spray((struct sockaddr *)&sin, sds, counts,
		    host_lb ? nitems(sds) : 1, 128);
		ATF_REQUIRE_MSG(counts[0] == 128,
		    "jail socket got %d of 128 datagrams", counts[0]);
		if (host_lb) {
			ATF_REQUIRE_MSG(counts[1] == 0 && counts[2] == 0,
			    "host LB got %d and %d of 128 datagrams",
			    counts[1], counts[2]);
		}
		ATF_REQUIRE(close(sds[0]) == 0);
		if (host_lb) {
			ATF_REQUIRE(close(sds[1]) == 0);
			ATF_REQUIRE(close(sds[2]) == 0);
		}
		_exit(0);
	}
	ATF_REQUIRE(waitpid(child, &status, 0) == child);
	ATF_REQUIRE_MSG(WIFEXITED(status), "child status %#x", status);
	ATF_REQUIRE_MSG(WEXITSTATUS(status) == 0,
	    "ordinary jail socket lost IPv4 traffic");
	if (host_lb) {
		int host_counts[2];

		sin.sin_port = port;
		lb_udp_spray((struct sockaddr *)&sin, host, host_counts,
		    nitems(host), 128);
		ATF_REQUIRE_MSG(host_counts[0] > 0 && host_counts[1] > 0,
		    "host dual-stack LB received %d and %d IPv4 datagrams",
		    host_counts[0], host_counts[1]);
		ATF_REQUIRE(close(host[0]) == 0);
		ATF_REQUIRE(close(host[1]) == 0);
	}
}

ATF_TC_BODY(dualstack_udp_ipv4_jail_ordinary, tc)
{
	lb_ipv4_jail_ordinary(true);
}

ATF_TC(dualstack_udp_ipv4_jail_no_lb);
ATF_TC_HEAD(dualstack_udp_ipv4_jail_no_lb, tc)
{
	atf_tc_set_md_var(tc, "require.user", "root");
	atf_tc_set_md_var(tc, "timeout", "15");
	atf_tc_set_md_var(tc, "is.exclusive", "true");
}
ATF_TC_BODY(dualstack_udp_ipv4_jail_no_lb, tc)
{
	lb_ipv4_jail_ordinary(false);
}

static void
lb_ipv4_jail_host_precedence(int type, bool lb, bool inherit6)
{
	struct in_addr ip4[] = {
		{ htonl(INADDR_LOOPBACK) }, { htonl(INADDR_LOOPBACK + 1) },
	};
	struct in6_addr ip6[] = {
		IN6ADDR_LOOPBACK_INIT, IN6ADDR_LOOPBACK_INIT,
	};
	int ip6mode = JAIL_SYS_INHERIT;
	struct iovec jiov[] = {
		{ __DECONST(char *, "path"), sizeof("path") },
		{ __DECONST(char *, "/"), sizeof("/") },
		{ __DECONST(char *, "ip4.addr"), sizeof("ip4.addr") },
		{ ip4, sizeof(ip4) },
		{ __DECONST(char *, inherit6 ? "ip6" : "ip6.addr"),
		    inherit6 ? sizeof("ip6") : sizeof("ip6.addr") },
		{ inherit6 ? (void *)&ip6mode : (void *)ip6,
		    inherit6 ? sizeof(ip6mode) : sizeof(ip6) },
	};
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
	struct pollfd client, pfds[4];
	socklen_t slen;
	pid_t child;
	uint16_t port;
	size_t n;
	int counts[4], csd, error, failed, opt, sd, sds[4], status, syncfd[2];
	char ch;

	n = lb ? 2 : 1;
	opt = lb ? SO_REUSEPORT_LB : SO_REUSEPORT;
	ip6[1].s6_addr[15] = 2;
	for (int jail_first = 0; jail_first < 2; jail_first++) {
		port = 0;
		for (size_t i = 0; i < n; i++) {
			sds[i] = socket(PF_INET, type | SOCK_NONBLOCK, 0);
			ATF_REQUIRE(sds[i] >= 0);
			ATF_REQUIRE(setsockopt(sds[i], SOL_SOCKET, opt,
			    (int[]){1}, sizeof(int)) == 0);
		}
		ATF_REQUIRE(socketpair(AF_UNIX, SOCK_STREAM, 0, syncfd) == 0);
		child = fork();
		ATF_REQUIRE(child >= 0);
		if (child == 0) {
			alarm(20);
			ATF_REQUIRE(close(syncfd[0]) == 0);
			ATF_REQUIRE_MSG(jail_set(jiov, nitems(jiov),
			    JAIL_CREATE | JAIL_ATTACH) > 0,
			    "jail_set() failed: %s", strerror(errno));
			ATF_REQUIRE(read(syncfd[1], &port, sizeof(port)) ==
			    sizeof(port));
			for (size_t i = n; i < 2 * n; i++) {
				sds[i] = socket(PF_INET6,
				    type | SOCK_NONBLOCK, 0);
				ATF_REQUIRE(sds[i] >= 0);
				ATF_REQUIRE(setsockopt(sds[i], IPPROTO_IPV6,
				    IPV6_V6ONLY, (int[]){0}, sizeof(int)) == 0);
				ATF_REQUIRE(setsockopt(sds[i], SOL_SOCKET, opt,
				    (int[]){1}, sizeof(int)) == 0);
				sin6.sin6_port = port;
				ATF_REQUIRE(bind(sds[i],
				    (struct sockaddr *)&sin6,
				    sizeof(sin6)) == 0);
				slen = sizeof(sin6);
				ATF_REQUIRE(getsockname(sds[i],
				    (struct sockaddr *)&sin6, &slen) == 0);
				ATF_REQUIRE(IN6_IS_ADDR_UNSPECIFIED(
				    &sin6.sin6_addr));
				port = sin6.sin6_port;
				if (type == SOCK_STREAM)
					ATF_REQUIRE(listen(sds[i], 64) == 0);
			}
			ATF_REQUIRE(write(syncfd[1], &port, sizeof(port)) ==
			    sizeof(port));
			ATF_REQUIRE(read(syncfd[1], &ch, sizeof(ch)) ==
			    sizeof(ch));
			sin.sin_port = port;
			sin.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
			memset(counts, 0, sizeof(counts));
			if (type == SOCK_DGRAM) {
				lb_udp_spray((struct sockaddr *)&sin, sds,
				    counts, 2 * n, 64);
			} else {
				for (size_t i = 0; i < 2 * n; i++) {
					pfds[i].fd = sds[i];
					pfds[i].events = POLLIN;
				}
				for (int c = 0; c < 64; c++) {
					sd = socket(PF_INET,
					    SOCK_STREAM | SOCK_NONBLOCK, 0);
					ATF_REQUIRE(sd >= 0);
					error = connect(sd,
					    (struct sockaddr *)&sin,
					    sizeof(sin));
					ATF_REQUIRE(error == 0 ||
					    errno == EINPROGRESS);
					client.fd = sd;
					client.events = POLLOUT;
					client.revents = 0;
					ATF_REQUIRE(poll(&client, 1, 2000) > 0);
					slen = sizeof(error);
					ATF_REQUIRE(getsockopt(sd, SOL_SOCKET,
					    SO_ERROR, &error, &slen) == 0);
					ATF_REQUIRE_EQ(0, error);
					ATF_REQUIRE(poll(pfds, 2 * n,
					    2000) > 0);
					for (size_t i = 0; i < 2 * n; i++) {
						if ((pfds[i].revents &
						    POLLIN) == 0)
							continue;
						csd = accept(sds[i], NULL,
						    NULL);
						ATF_REQUIRE(csd >= 0);
						counts[i]++;
						ATF_REQUIRE(close(csd) == 0);
					}
					ATF_REQUIRE(close(sd) == 0);
				}
			}
			failed = 0;
			for (size_t i = 0; i < 2 * n; i++) {
				if (i < n ? counts[i] != 0 : counts[i] == 0) {
					warnx("%s socket %zu received %d "
					    "messages/connections (jail_first=%d)",
					    i < n ? "host" : "jail", i,
					    counts[i], jail_first);
					failed = 1;
				}
			}
			_exit(failed);
		}
		ATF_REQUIRE(close(syncfd[1]) == 0);
		for (int stage = 0; stage < 2; stage++) {
			if (stage == jail_first) {
				for (size_t i = 0; i < n; i++) {
					sin.sin_addr.s_addr = htonl(INADDR_ANY);
					sin.sin_port = port;
					ATF_REQUIRE(bind(sds[i],
					    (struct sockaddr *)&sin,
					    sizeof(sin)) == 0);
					slen = sizeof(sin);
					ATF_REQUIRE(getsockname(sds[i],
					    (struct sockaddr *)&sin,
					    &slen) == 0);
					port = sin.sin_port;
					if (type == SOCK_STREAM)
						ATF_REQUIRE(listen(sds[i],
						    64) == 0);
				}
			}
			if (stage == 0) {
				ATF_REQUIRE(write(syncfd[0], &port,
				    sizeof(port)) == sizeof(port));
				ATF_REQUIRE(read(syncfd[0], &port,
				    sizeof(port)) == sizeof(port));
			}
		}
		ch = 1;
		ATF_REQUIRE(write(syncfd[0], &ch, sizeof(ch)) == sizeof(ch));
		ATF_REQUIRE(waitpid(child, &status, 0) == child);
		ATF_REQUIRE_MSG(WIFEXITED(status), "child status %#x", status);
		ATF_CHECK_MSG(WEXITSTATUS(status) == 0,
		    "jail precedence failed (jail_first=%d)", jail_first);
		ATF_REQUIRE(close(syncfd[0]) == 0);
		for (size_t i = 0; i < n; i++)
			ATF_REQUIRE(close(sds[i]) == 0);
	}
}

ATF_TC(dualstack_udp_ipv4_jail_host_lb);
ATF_TC_HEAD(dualstack_udp_ipv4_jail_host_lb, tc)
{
	atf_tc_set_md_var(tc, "require.user", "root");
	atf_tc_set_md_var(tc, "timeout", "45");
	atf_tc_set_md_var(tc, "is.exclusive", "true");
}
ATF_TC_BODY(dualstack_udp_ipv4_jail_host_lb, tc)
{
	lb_ipv4_jail_host_precedence(SOCK_DGRAM, true, false);
}

ATF_TC(dualstack_tcp_ipv4_jail_host_lb);
ATF_TC_HEAD(dualstack_tcp_ipv4_jail_host_lb, tc)
{
	atf_tc_set_md_var(tc, "require.user", "root");
	atf_tc_set_md_var(tc, "timeout", "45");
	atf_tc_set_md_var(tc, "is.exclusive", "true");
}
ATF_TC_BODY(dualstack_tcp_ipv4_jail_host_lb, tc)
{
	lb_ipv4_jail_host_precedence(SOCK_STREAM, true, false);
}

ATF_TC(dualstack_udp_ipv4_jail_inherit6);
ATF_TC_HEAD(dualstack_udp_ipv4_jail_inherit6, tc)
{
	atf_tc_set_md_var(tc, "require.user", "root");
	atf_tc_set_md_var(tc, "timeout", "45");
	atf_tc_set_md_var(tc, "is.exclusive", "true");
}
ATF_TC_BODY(dualstack_udp_ipv4_jail_inherit6, tc)
{
	lb_ipv4_jail_host_precedence(SOCK_DGRAM, false, true);
}

static void
lb_ipv6_jail_precedence(int type)
{
	struct in6_addr ip6[] = {
		IN6ADDR_LOOPBACK_INIT, IN6ADDR_LOOPBACK_INIT,
	};
	struct jail jconf = {
		.version = JAIL_API_VERSION,
		.path = __DECONST(char *, "/"),
		.hostname = __DECONST(char *, "lb-ipv6-precedence"),
		.ip6s = nitems(ip6),
		.ip6 = ip6,
	};
	struct sockaddr_in6 sin6 = {
		.sin6_family = AF_INET6,
		.sin6_len = sizeof(sin6),
		.sin6_addr = IN6ADDR_LOOPBACK_INIT,
	};
	struct sockaddr_in6 bound;
	struct pollfd pfd;
	socklen_t slen;
	pid_t child;
	uint16_t port = 0;
	int csd, error, host, sd, status, syncfd[2];
	char ch;

	ip6[1].s6_addr[15] = 2;
	host = lb_v6only_socket(type, &port);
	if (type == SOCK_STREAM)
		ATF_REQUIRE(listen(host, 1) == 0);
	sin6.sin6_port = port;
	ATF_REQUIRE(socketpair(AF_UNIX, SOCK_STREAM, 0, syncfd) == 0);
	child = fork();
	ATF_REQUIRE(child >= 0);
	if (child == 0) {
		alarm(10);
		close(syncfd[0]);
		close(host);
		ATF_REQUIRE(jail(&jconf) > 0);
		sd = lb_dualstack_socket(type, &port, -1);
		slen = sizeof(bound);
		ATF_REQUIRE(getsockname(sd, (struct sockaddr *)&bound,
		    &slen) == 0);
		ATF_REQUIRE(IN6_IS_ADDR_UNSPECIFIED(&bound.sin6_addr));
		if (type == SOCK_STREAM)
			ATF_REQUIRE(listen(sd, 1) == 0);
		ch = 1;
		ATF_REQUIRE(write(syncfd[1], &ch, sizeof(ch)) == sizeof(ch));
		ATF_REQUIRE(read(syncfd[1], &ch, sizeof(ch)) == sizeof(ch));
		pfd.fd = sd;
		pfd.events = POLLIN;
		pfd.revents = 0;
		error = poll(&pfd, 1, 2000);
		ATF_REQUIRE(error >= 0);
		ch = 0;
		if (error > 0 && (pfd.revents & POLLIN) != 0) {
			if (type == SOCK_DGRAM) {
				ATF_REQUIRE(recv(sd, &ch, sizeof(ch), 0) == 1);
				ATF_REQUIRE_EQ(42, ch);
			} else {
				csd = accept(sd, NULL, NULL);
				ATF_REQUIRE(csd >= 0);
				ATF_REQUIRE(close(csd) == 0);
			}
			ch = 1;
		}
		ATF_REQUIRE(write(syncfd[1], &ch, sizeof(ch)) == sizeof(ch));
		_exit(0);
	}
	close(syncfd[1]);
	pfd.fd = syncfd[0];
	pfd.events = POLLIN;
	pfd.revents = 0;
	ATF_REQUIRE(poll(&pfd, 1, 5000) > 0);
	ATF_REQUIRE(read(syncfd[0], &ch, sizeof(ch)) == sizeof(ch));
	csd = socket(PF_INET6, type | SOCK_NONBLOCK, 0);
	ATF_REQUIRE(csd >= 0);
	if (type == SOCK_DGRAM) {
		ch = 42;
		ATF_REQUIRE(sendto(csd, &ch, sizeof(ch), 0,
		    (struct sockaddr *)&sin6, sizeof(sin6)) == sizeof(ch));
	} else {
		error = connect(csd, (struct sockaddr *)&sin6, sizeof(sin6));
		ATF_REQUIRE(error == 0 || errno == EINPROGRESS);
		pfd.fd = csd;
		pfd.events = POLLOUT;
		pfd.revents = 0;
		ATF_REQUIRE(poll(&pfd, 1, 2000) > 0);
		slen = sizeof(error);
		ATF_REQUIRE(getsockopt(csd, SOL_SOCKET, SO_ERROR, &error,
		    &slen) == 0);
		ATF_REQUIRE_EQ(0, error);
	}
	ch = 1;
	ATF_REQUIRE(write(syncfd[0], &ch, sizeof(ch)) == sizeof(ch));
	pfd.fd = syncfd[0];
	pfd.events = POLLIN;
	pfd.revents = 0;
	ATF_REQUIRE(poll(&pfd, 1, 5000) > 0);
	ATF_REQUIRE(read(syncfd[0], &ch, sizeof(ch)) == sizeof(ch));
	ATF_REQUIRE(waitpid(child, &status, 0) == child);
	ATF_REQUIRE(WIFEXITED(status));
	ATF_REQUIRE_EQ(0, WEXITSTATUS(status));
	ATF_CHECK_MSG(ch == 1, "jailed dual-stack socket received no traffic");
	pfd.fd = host;
	pfd.events = POLLIN;
	pfd.revents = 0;
	ATF_CHECK_MSG(poll(&pfd, 1, 0) == 0,
	    "host IPv6-only socket received jail-owned traffic");
	ATF_REQUIRE(close(csd) == 0);
	ATF_REQUIRE(close(host) == 0);
	ATF_REQUIRE(close(syncfd[0]) == 0);
}

ATF_TC(dualstack_udp_ipv6_jail_precedence);
ATF_TC_HEAD(dualstack_udp_ipv6_jail_precedence, tc)
{
	atf_tc_set_md_var(tc, "require.user", "root");
	atf_tc_set_md_var(tc, "timeout", "15");
	atf_tc_set_md_var(tc, "is.exclusive", "true");
}
ATF_TC_BODY(dualstack_udp_ipv6_jail_precedence, tc)
{
	lb_ipv6_jail_precedence(SOCK_DGRAM);
}

ATF_TC(dualstack_udp_jail_dualstack_precedence);
ATF_TC_HEAD(dualstack_udp_jail_dualstack_precedence, tc)
{
	atf_tc_set_md_var(tc, "require.user", "root");
	atf_tc_set_md_var(tc, "timeout", "15");
	atf_tc_set_md_var(tc, "is.exclusive", "true");
}
ATF_TC_BODY(dualstack_udp_jail_dualstack_precedence, tc)
{
	struct in_addr ip4[] = {
		{ htonl(INADDR_LOOPBACK) }, { htonl(INADDR_LOOPBACK + 1) },
	};
	struct in6_addr ip6[] = {
		IN6ADDR_LOOPBACK_INIT, IN6ADDR_LOOPBACK_INIT,
	};
	struct jail jconf = {
		.version = JAIL_API_VERSION,
		.path = __DECONST(char *, "/"),
		.hostname = __DECONST(char *, "lb-dualstack-jail-precedence"),
		.ip4s = nitems(ip4),
		.ip4 = ip4,
		.ip6s = nitems(ip6),
		.ip6 = ip6,
	};
	struct sockaddr_in sin4 = {
		.sin_family = AF_INET,
		.sin_len = sizeof(sin4),
		.sin_addr = { htonl(INADDR_LOOPBACK) },
	};
	struct sockaddr_in6 sin6 = {
		.sin6_family = AF_INET6,
		.sin6_len = sizeof(sin6),
		.sin6_addr = IN6ADDR_LOOPBACK_INIT,
	};
	struct pollfd pfd, host_pfds[2];
	pid_t child;
	uint16_t port = 0;
	int host[2], sd, sender, status, syncfd[2];
	char ch;

	ip6[1].s6_addr[15] = 2;
	host[0] = lb_v6only_socket(SOCK_DGRAM, &port);
	host[1] = lb_v6only_socket(SOCK_DGRAM, &port);
	sin4.sin_port = sin6.sin6_port = port;
	ATF_REQUIRE(socketpair(AF_UNIX, SOCK_STREAM, 0, syncfd) == 0);
	child = fork();
	ATF_REQUIRE(child >= 0);
	if (child == 0) {
		alarm(10);
		close(syncfd[0]);
		ATF_REQUIRE(close(host[0]) == 0);
		ATF_REQUIRE(close(host[1]) == 0);
		ATF_REQUIRE(jail(&jconf) > 0);
		sd = socket(PF_INET6, SOCK_DGRAM | SOCK_NONBLOCK, 0);
		ATF_REQUIRE_MSG(sd >= 0, "socket() failed: %s",
		    strerror(errno));
		ATF_REQUIRE(setsockopt(sd, IPPROTO_IPV6, IPV6_V6ONLY,
		    (int[]){0}, sizeof(int)) == 0);
		sin6.sin6_addr = in6addr_any;
		ATF_REQUIRE(bind(sd, (struct sockaddr *)&sin6,
		    sizeof(sin6)) == 0);
		sender = socket(PF_INET, SOCK_DGRAM, 0);
		ATF_REQUIRE(sender >= 0);
		ch = 42;
		ATF_REQUIRE(sendto(sender, &ch, sizeof(ch), 0,
		    (struct sockaddr *)&sin4, sizeof(sin4)) == sizeof(ch));
		pfd.fd = sd;
		pfd.events = POLLIN;
		pfd.revents = 0;
		ATF_REQUIRE_MSG(poll(&pfd, 1, 2000) == 1 &&
		    (pfd.revents & POLLIN) != 0,
		    "jailed dual-stack socket received no IPv4 datagram");
		ATF_REQUIRE(recv(sd, &ch, sizeof(ch), 0) == sizeof(ch));
		ATF_REQUIRE_EQ(42, ch);
		ATF_REQUIRE(close(sender) == 0);
		ch = 1;
		ATF_REQUIRE(write(syncfd[1], &ch, sizeof(ch)) == sizeof(ch));
		ATF_REQUIRE(read(syncfd[1], &ch, sizeof(ch)) == sizeof(ch));
		pfd.fd = sd;
		pfd.events = POLLIN;
		pfd.revents = 0;
		if (poll(&pfd, 1, 2000) == 1 &&
		    (pfd.revents & POLLIN) != 0) {
			ATF_REQUIRE(recv(sd, &ch, sizeof(ch), 0) == sizeof(ch));
			ATF_REQUIRE_EQ(42, ch);
			ch = 1;
		} else
			ch = 0;
		ATF_REQUIRE(write(syncfd[1], &ch, sizeof(ch)) == sizeof(ch));
		_exit(0);
	}
	close(syncfd[1]);
	pfd.fd = syncfd[0];
	pfd.events = POLLIN;
	pfd.revents = 0;
	ATF_REQUIRE(poll(&pfd, 1, 5000) > 0);
	ATF_REQUIRE(read(syncfd[0], &ch, sizeof(ch)) == sizeof(ch));
	ATF_REQUIRE_EQ(1, ch);
	host_pfds[0].fd = host[0];
	host_pfds[1].fd = host[1];
	host_pfds[0].events = host_pfds[1].events = POLLIN;
	sender = socket(PF_INET6, SOCK_DGRAM, 0);
	ATF_REQUIRE(sender >= 0);
	ch = 42;
	ATF_REQUIRE(sendto(sender, &ch, sizeof(ch), 0,
	    (struct sockaddr *)&sin6, sizeof(sin6)) == sizeof(ch));
	ATF_REQUIRE(write(syncfd[0], &ch, sizeof(ch)) == sizeof(ch));
	pfd.revents = 0;
	ATF_REQUIRE(poll(&pfd, 1, 5000) > 0);
	ATF_REQUIRE(read(syncfd[0], &ch, sizeof(ch)) == sizeof(ch));
	ATF_CHECK_MSG(ch == 1,
	    "jailed dual-stack socket received no IPv6 datagram");
	host_pfds[0].revents = host_pfds[1].revents = 0;
	ATF_CHECK_MSG(poll(host_pfds, nitems(host_pfds), 0) == 0,
	    "host IPv6-only LB received jail-owned IPv6 traffic");
	ATF_REQUIRE(close(sender) == 0);
	ATF_REQUIRE(waitpid(child, &status, 0) == child);
	ATF_REQUIRE(WIFEXITED(status));
	ATF_REQUIRE_EQ(0, WEXITSTATUS(status));
	ATF_REQUIRE(close(host[0]) == 0);
	ATF_REQUIRE(close(host[1]) == 0);
	ATF_REQUIRE(close(syncfd[0]) == 0);
}

static void
lb_jail_udp_mixed_wildcard(bool ipv6, bool mapped)
{
	struct in_addr ip4[] = {
		{ htonl(INADDR_LOOPBACK) }, { htonl(INADDR_LOOPBACK + 1) },
	};
	struct in6_addr ip6[] = {
		IN6ADDR_LOOPBACK_INIT, IN6ADDR_LOOPBACK_INIT,
	};
	struct jail jconf = {
		.version = JAIL_API_VERSION,
		.path = __DECONST(char *, "/"),
		.hostname = __DECONST(char *, "lb-mixed-wildcard"),
		.ip4s = nitems(ip4),
		.ip4 = ip4,
		.ip6s = nitems(ip6),
		.ip6 = ip6,
	};
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
	struct sockaddr_in dst4 = {
		.sin_family = AF_INET,
		.sin_len = sizeof(dst4),
		.sin_addr = { htonl(INADDR_LOOPBACK) },
	};
	struct sockaddr_in6 dst6 = {
		.sin6_family = AF_INET6,
		.sin6_len = sizeof(dst6),
		.sin6_addr = IN6ADDR_LOOPBACK_INIT,
	};
	const struct sockaddr *dst;
	const char *name;
	socklen_t slen;
	pid_t child;
	int counts[2], domain, error, failed, sds[2], status;

	name = mapped ? "IPv4-mapped specific" :
	    (ipv6 ? "IPv6-only" : "IPv4");
	ip6[1].s6_addr[15] = 2;
	child = fork();
	ATF_REQUIRE(child >= 0);
	if (child == 0) {
		alarm(10);
		ATF_REQUIRE_MSG(jail(&jconf) > 0, "jail() failed: %s",
		    strerror(errno));
		failed = 0;
		for (int order = 0; order < 2; order++) {
			uint16_t port;

			port = 0;
			for (int j = 0; j < 2; j++) {
				int index, one;

				index = order == 0 ? j : 1 - j;
				domain = index == 0 ?
				    (ipv6 || mapped ? PF_INET6 : PF_INET) :
				    PF_INET6;
				sds[index] = socket(domain,
				    SOCK_DGRAM | SOCK_NONBLOCK, 0);
				ATF_REQUIRE_MSG(sds[index] >= 0,
				    "socket() failed: %s", strerror(errno));
				one = 1;
				error = setsockopt(sds[index], SOL_SOCKET,
				    SO_REUSEPORT, &one, sizeof(one));
				ATF_REQUIRE_MSG(error == 0,
				    "setsockopt(SO_REUSEPORT) failed: %s",
				    strerror(errno));
				if (domain == PF_INET6) {
					one = ipv6 && index == 0;
					error = setsockopt(sds[index],
					    IPPROTO_IPV6, IPV6_V6ONLY, &one,
					    sizeof(one));
					ATF_REQUIRE_MSG(error == 0,
					    "setsockopt(IPV6_V6ONLY) failed: %s",
					    strerror(errno));
				}
				if (domain == PF_INET) {
					sin.sin_port = j == 0 ? 0 : port;
					error = bind(sds[index],
					    (struct sockaddr *)&sin,
					    sizeof(sin));
					ATF_REQUIRE_MSG(error == 0,
					    "bind() failed: %s",
					    strerror(errno));
					if (j == 0) {
						slen = sizeof(sin);
						error = getsockname(sds[index],
						    (struct sockaddr *)&sin,
						    &slen);
						ATF_REQUIRE_MSG(error == 0,
						    "getsockname() failed: %s",
						    strerror(errno));
						port = sin.sin_port;
					}
				} else {
					sin6.sin6_addr = in6addr_any;
					if (mapped && index == 0)
						lb_ipv4_mapped_addr(
						    &sin6.sin6_addr);
					sin6.sin6_port = j == 0 ? 0 : port;
					error = bind(sds[index],
					    (struct sockaddr *)&sin6,
					    sizeof(sin6));
					ATF_REQUIRE_MSG(error == 0,
					    "bind() failed: %s",
					    strerror(errno));
					if (j == 0) {
						slen = sizeof(sin6);
						error = getsockname(sds[index],
						    (struct sockaddr *)&sin6,
						    &slen);
						ATF_REQUIRE_MSG(error == 0,
						    "getsockname() failed: %s",
						    strerror(errno));
						port = sin6.sin6_port;
					}
				}
			}
			dst = ipv6 ? (struct sockaddr *)&dst6 :
			    (struct sockaddr *)&dst4;
			dst6.sin6_port = dst4.sin_port = port;
			lb_udp_spray(dst, sds, counts, nitems(sds), 64);
			if (counts[0] != 64 || counts[1] != 0) {
				warnx(
				    "%s socket got %d, dual-stack got %d "
				    "(order=%d)\n", name,
				    counts[0], counts[1], order);
				failed = 1;
			}
			for (size_t j = 0; j < nitems(sds); j++)
				ATF_REQUIRE(close(sds[j]) == 0);
		}
		_exit(failed);
	}
	ATF_REQUIRE(waitpid(child, &status, 0) == child);
	ATF_REQUIRE_MSG(WIFEXITED(status), "child status %#x", status);
	ATF_REQUIRE_MSG(WEXITSTATUS(status) == 0,
	    "%s precedence changed with bind order", name);
}

ATF_TC(dualstack_udp_ipv4_jail_mixed_wildcard);
ATF_TC_HEAD(dualstack_udp_ipv4_jail_mixed_wildcard, tc)
{
	atf_tc_set_md_var(tc, "require.user", "root");
	atf_tc_set_md_var(tc, "timeout", "15");
	atf_tc_set_md_var(tc, "is.exclusive", "true");
}
ATF_TC_BODY(dualstack_udp_ipv4_jail_mixed_wildcard, tc)
{
	lb_jail_udp_mixed_wildcard(false, false);
}

ATF_TC(dualstack_udp_ipv6_jail_mixed_wildcard);
ATF_TC_HEAD(dualstack_udp_ipv6_jail_mixed_wildcard, tc)
{
	atf_tc_set_md_var(tc, "require.user", "root");
	atf_tc_set_md_var(tc, "timeout", "15");
	atf_tc_set_md_var(tc, "is.exclusive", "true");
}
ATF_TC_BODY(dualstack_udp_ipv6_jail_mixed_wildcard, tc)
{
	lb_jail_udp_mixed_wildcard(true, false);
}

ATF_TC(dualstack_udp_mapped_jail_precedence);
ATF_TC_HEAD(dualstack_udp_mapped_jail_precedence, tc)
{
	atf_tc_set_md_var(tc, "require.user", "root");
	atf_tc_set_md_var(tc, "timeout", "15");
	atf_tc_set_md_var(tc, "is.exclusive", "true");
}
ATF_TC_BODY(dualstack_udp_mapped_jail_precedence, tc)
{
	lb_jail_udp_mixed_wildcard(false, true);
}

ATF_TC(dualstack_tcp_ipv6_jail_precedence);
ATF_TC_HEAD(dualstack_tcp_ipv6_jail_precedence, tc)
{
	atf_tc_set_md_var(tc, "require.user", "root");
	atf_tc_set_md_var(tc, "timeout", "15");
	atf_tc_set_md_var(tc, "is.exclusive", "true");
}
ATF_TC_BODY(dualstack_tcp_ipv6_jail_precedence, tc)
{
	lb_ipv6_jail_precedence(SOCK_STREAM);
}

static void
lb_dualstack_overflow(int type, int domain)
{
	struct sockaddr_in sin = {
		.sin_family = AF_INET,
		.sin_len = sizeof(sin),
		.sin_addr = { htonl(INADDR_LOOPBACK) },
	};
	struct sockaddr_in6 sin6 = {
		.sin6_family = AF_INET6,
		.sin6_len = sizeof(sin6),
		.sin6_addr = IN6ADDR_LOOPBACK_INIT,
	};
	const struct sockaddr *dst;
	struct pollfd client, pfd[LB_GROUP_MAX + 1];
	socklen_t slen;
	uint16_t port = 0;
	int active, counts[LB_GROUP_MAX + 1] = { 0 }, csd, error, sd;
	int sds[LB_GROUP_MAX + 1], total;

	for (size_t i = 0; i < nitems(sds); i++) {
		sds[i] = lb_dualstack_socket(type, &port, -1);
		if (type == SOCK_STREAM)
			ATF_REQUIRE(listen(sds[i], 64) == 0);
		pfd[i].fd = sds[i];
		pfd[i].events = POLLIN;
		pfd[i].revents = 0;
	}
	sin.sin_port = sin6.sin6_port = port;
	dst = domain == PF_INET ? (struct sockaddr *)&sin :
	    (struct sockaddr *)&sin6;
	if (type == SOCK_DGRAM) {
		lb_udp_spray(dst, sds, counts, nitems(sds), 64);
	} else {
		for (int c = 0; c < 64; c++) {
			sd = socket(domain, SOCK_STREAM | SOCK_NONBLOCK, 0);
			ATF_REQUIRE(sd >= 0);
			error = connect(sd, dst, dst->sa_len);
			ATF_REQUIRE(error == 0 || errno == EINPROGRESS);
			client.fd = sd;
			client.events = POLLOUT;
			client.revents = 0;
			ATF_REQUIRE(poll(&client, 1, 2000) > 0);
			slen = sizeof(error);
			ATF_REQUIRE(getsockopt(sd, SOL_SOCKET, SO_ERROR,
			    &error, &slen) == 0);
			ATF_REQUIRE_EQ(0, error);
			ATF_REQUIRE(poll(pfd, nitems(pfd), 2000) > 0);
			for (size_t i = 0; i < nitems(sds); i++) {
				if ((pfd[i].revents & POLLIN) == 0)
					continue;
				csd = accept(sds[i], NULL, NULL);
				ATF_REQUIRE(csd >= 0);
				counts[i]++;
				ATF_REQUIRE(close(csd) == 0);
			}
			ATF_REQUIRE(close(sd) == 0);
		}
	}
	active = total = 0;
	for (size_t i = 0; i < LB_GROUP_MAX; i++) {
		total += counts[i];
		if (counts[i] != 0)
			active++;
	}
	ATF_CHECK_MSG(total == 64,
	    "LB group received %d of 64 messages/connections", total);
	ATF_CHECK_MSG(active > 1, "traffic reached only %d group members",
	    active);
	ATF_CHECK_MSG(counts[LB_GROUP_MAX] == 0,
	    "overflow socket received %d messages/connections",
	    counts[LB_GROUP_MAX]);
	for (size_t i = 0; i < nitems(sds); i++)
		ATF_REQUIRE(close(sds[i]) == 0);
}

ATF_TC_WITHOUT_HEAD(dualstack_udp_ipv4_overflow);
ATF_TC_BODY(dualstack_udp_ipv4_overflow, tc)
{
	lb_dualstack_overflow(SOCK_DGRAM, PF_INET);
}

ATF_TC_WITHOUT_HEAD(dualstack_udp_ipv6_overflow);
ATF_TC_BODY(dualstack_udp_ipv6_overflow, tc)
{
	lb_dualstack_overflow(SOCK_DGRAM, PF_INET6);
}

ATF_TC_WITHOUT_HEAD(dualstack_tcp_ipv4_overflow);
ATF_TC_BODY(dualstack_tcp_ipv4_overflow, tc)
{
	lb_dualstack_overflow(SOCK_STREAM, PF_INET);
}

ATF_TC_WITHOUT_HEAD(dualstack_tcp_ipv6_overflow);
ATF_TC_BODY(dualstack_tcp_ipv6_overflow, tc)
{
	lb_dualstack_overflow(SOCK_STREAM, PF_INET6);
}

static void
lb_udp_mixed_options(int domain)
{
	struct sockaddr_in sin = {
		.sin_family = AF_INET,
		.sin_len = sizeof(sin),
		.sin_addr = { htonl(INADDR_LOOPBACK) },
	};
	struct sockaddr_in6 sin6 = {
		.sin6_family = AF_INET6,
		.sin6_len = sizeof(sin6),
		.sin6_addr = IN6ADDR_ANY_INIT,
	};
	const struct sockaddr *dst;
	socklen_t slen;
	int counts[3], sds[3];

	for (int ordinary_first = 0; ordinary_first < 2; ordinary_first++) {
		sin6.sin6_addr = in6addr_any;
		sin6.sin6_port = 0;
		for (int i = 0; i < 3; i++) {
			int index;

			index = ordinary_first ? i : (i + 1) % 3;
			sds[index] = socket(PF_INET6,
			    SOCK_DGRAM | SOCK_NONBLOCK, 0);
			ATF_REQUIRE(sds[index] >= 0);
			ATF_REQUIRE(setsockopt(sds[index], IPPROTO_IPV6,
			    IPV6_V6ONLY, (int[]){0}, sizeof(int)) == 0);
			ATF_REQUIRE(setsockopt(sds[index], SOL_SOCKET,
			    SO_REUSEPORT, (int[]){1}, sizeof(int)) == 0);
			if (index != 0) {
				ATF_REQUIRE(setsockopt(sds[index], SOL_SOCKET,
				    SO_REUSEPORT_LB, (int[]){1},
				    sizeof(int)) == 0);
			}
			ATF_REQUIRE(bind(sds[index],
			    (struct sockaddr *)&sin6, sizeof(sin6)) == 0);
			if (i == 0) {
				slen = sizeof(sin6);
				ATF_REQUIRE(getsockname(sds[index],
				    (struct sockaddr *)&sin6, &slen) == 0);
			}
		}
		sin.sin_port = sin6.sin6_port;
		sin6.sin6_addr = in6addr_loopback;
		dst = domain == PF_INET ? (struct sockaddr *)&sin :
		    (struct sockaddr *)&sin6;
		lb_udp_spray(dst, sds, counts, nitems(sds), 128);
		ATF_CHECK_MSG(counts[0] == 0,
		    "ordinary socket received %d datagrams (ordinary_first=%d)",
		    counts[0], ordinary_first);
		ATF_CHECK_MSG(counts[1] > 0 && counts[2] > 0,
		    "LB members received %d and %d (ordinary_first=%d)",
		    counts[1], counts[2], ordinary_first);
		ATF_CHECK_MSG(counts[1] + counts[2] == 128,
		    "LB group received %d of 128 datagrams (ordinary_first=%d)",
		    counts[1] + counts[2], ordinary_first);
		for (size_t i = 0; i < nitems(sds); i++)
			ATF_REQUIRE(close(sds[i]) == 0);
	}
}

ATF_TC_WITHOUT_HEAD(dualstack_udp_ipv4_mixed_options);
ATF_TC_BODY(dualstack_udp_ipv4_mixed_options, tc)
{
	lb_udp_mixed_options(PF_INET);
}

ATF_TC_WITHOUT_HEAD(dualstack_udp_ipv6_mixed_options);
ATF_TC_BODY(dualstack_udp_ipv6_mixed_options, tc)
{
	lb_udp_mixed_options(PF_INET6);
}

ATF_TC(dualstack_udp_ipv6_mixed_wildcard);
ATF_TC_HEAD(dualstack_udp_ipv6_mixed_wildcard, tc)
{
	atf_tc_set_md_var(tc, "require.user", "root");
	atf_tc_set_md_var(tc, "is.exclusive", "true");
}
ATF_TC_BODY(dualstack_udp_ipv6_mixed_wildcard, tc)
{
	struct in6_addr ip6[] = {
		IN6ADDR_LOOPBACK_INIT, IN6ADDR_LOOPBACK_INIT,
	};
	struct jail jconf = {
		.version = JAIL_API_VERSION,
		.path = __DECONST(char *, "/"),
		.hostname = __DECONST(char *, "lb-ipv6-mixed-wildcard"),
		.ip6s = nitems(ip6),
		.ip6 = ip6,
	};
	struct sockaddr_in6 sin6 = {
		.sin6_family = AF_INET6,
		.sin6_len = sizeof(sin6),
	};
	socklen_t slen;
	pid_t child;
	int counts[3], failed, sds[3], status;

	/* Two jail addresses keep wildcard binds from becoming specific. */
	ip6[1].s6_addr[15] = 2;
	child = fork();
	ATF_REQUIRE(child >= 0);
	if (child == 0) {
		alarm(10);
		ATF_REQUIRE(jail(&jconf) > 0);
		failed = 0;
		for (int ordinary_first = 0; ordinary_first < 2;
		    ordinary_first++) {
			sin6.sin6_addr = in6addr_any;
			sin6.sin6_port = 0;
			for (int i = 0; i < 3; i++) {
				int index;

				index = ordinary_first ? i : (i + 1) % 3;
				sds[index] = socket(PF_INET6,
				    SOCK_DGRAM | SOCK_NONBLOCK, 0);
				ATF_REQUIRE(sds[index] >= 0);
				ATF_REQUIRE(setsockopt(sds[index], IPPROTO_IPV6,
				    IPV6_V6ONLY, (int[]){index == 0},
				    sizeof(int)) == 0);
				ATF_REQUIRE(setsockopt(sds[index], SOL_SOCKET,
				    SO_REUSEPORT, (int[]){1},
				    sizeof(int)) == 0);
				if (index != 0) {
					ATF_REQUIRE(setsockopt(sds[index],
					    SOL_SOCKET, SO_REUSEPORT_LB,
					    (int[]){1}, sizeof(int)) == 0);
				}
				ATF_REQUIRE(bind(sds[index],
				    (struct sockaddr *)&sin6,
				    sizeof(sin6)) == 0);
				slen = sizeof(sin6);
				ATF_REQUIRE(getsockname(sds[index],
				    (struct sockaddr *)&sin6, &slen) == 0);
				ATF_REQUIRE(IN6_IS_ADDR_UNSPECIFIED(
				    &sin6.sin6_addr));
			}
			/* The jailed dual-stack LB forces the locked path. */
			sin6.sin6_addr = in6addr_loopback;
			lb_udp_spray((struct sockaddr *)&sin6, sds, counts,
			    nitems(sds), 128);
			ATF_CHECK_MSG(counts[0] == 128,
			    "IPv6-only socket got %d of 128 datagrams "
			    "(ordinary_first=%d)", counts[0], ordinary_first);
			ATF_CHECK_MSG(counts[1] == 0 && counts[2] == 0,
			    "dual-stack LB members got %d and %d datagrams "
			    "(ordinary_first=%d)", counts[1], counts[2],
			    ordinary_first);
			if (counts[0] != 128 || counts[1] != 0 ||
			    counts[2] != 0)
				failed = 1;
			for (size_t i = 0; i < nitems(sds); i++)
				ATF_REQUIRE(close(sds[i]) == 0);
		}
		_exit(failed);
	}
	ATF_REQUIRE(waitpid(child, &status, 0) == child);
	ATF_REQUIRE_MSG(WIFEXITED(status), "child status %#x", status);
	ATF_REQUIRE_MSG(WEXITSTATUS(status) == 0,
	    "mixed wildcard precedence failed in jail");
}

ATF_TP_ADD_TCS(tp)
{
	ATF_TP_ADD_TC(tp, basic_ipv4);
	ATF_TP_ADD_TC(tp, basic_ipv6);
	ATF_TP_ADD_TC(tp, concurrent_add);
	ATF_TP_ADD_TC(tp, double_listen_ipv4);
	ATF_TP_ADD_TC(tp, double_listen_ipv6);
	ATF_TP_ADD_TC(tp, bind_without_listen);
	ATF_TP_ADD_TC(tp, connect_not_bound);
	ATF_TP_ADD_TC(tp, connect_bound);
	ATF_TP_ADD_TC(tp, connect_udp);
	ATF_TP_ADD_TC(tp, connect_udp6);
	ATF_TP_ADD_TC(tp, dualstack_udp);
	ATF_TP_ADD_TC(tp, dualstack_udp_no_lb);
	ATF_TP_ADD_TC(tp, dualstack_tcp);
	ATF_TP_ADD_TC(tp, dualstack_udp_ipv4_mapped);
	ATF_TP_ADD_TC(tp, dualstack_tcp_ipv4_mapped);
	ATF_TP_ADD_TC(tp, dualstack_udp_ipv4_precedence);
	ATF_TP_ADD_TC(tp, dualstack_udp_ipv6_precedence);
	ATF_TP_ADD_TC(tp, dualstack_tcp_ipv6_precedence);
	ATF_TP_ADD_TC(tp, dualstack_udp_ipv6_specific);
	ATF_TP_ADD_TC(tp, dualstack_tcp_ipv6_specific);
	ATF_TP_ADD_TC(tp, dualstack_udp_specific_precedence);
	ATF_TP_ADD_TC(tp, dualstack_tcp_specific_precedence);
	ATF_TP_ADD_TC(tp, dualstack_udp_ipv4_jail_precedence);
	ATF_TP_ADD_TC(tp, dualstack_udp_ipv4_jail_ordinary);
	ATF_TP_ADD_TC(tp, dualstack_udp_ipv4_jail_no_lb);
	ATF_TP_ADD_TC(tp, dualstack_udp_ipv4_jail_host_lb);
	ATF_TP_ADD_TC(tp, dualstack_tcp_ipv4_jail_host_lb);
	ATF_TP_ADD_TC(tp, dualstack_udp_ipv4_jail_inherit6);
	ATF_TP_ADD_TC(tp, dualstack_udp_jail_dualstack_precedence);
	ATF_TP_ADD_TC(tp, dualstack_udp_ipv4_jail_mixed_wildcard);
	ATF_TP_ADD_TC(tp, dualstack_udp_ipv6_jail_precedence);
	ATF_TP_ADD_TC(tp, dualstack_udp_ipv6_jail_mixed_wildcard);
	ATF_TP_ADD_TC(tp, dualstack_udp_mapped_jail_precedence);
	ATF_TP_ADD_TC(tp, dualstack_tcp_ipv6_jail_precedence);
	ATF_TP_ADD_TC(tp, dualstack_udp_ipv4_overflow);
	ATF_TP_ADD_TC(tp, dualstack_udp_ipv6_overflow);
	ATF_TP_ADD_TC(tp, dualstack_tcp_ipv4_overflow);
	ATF_TP_ADD_TC(tp, dualstack_tcp_ipv6_overflow);
	ATF_TP_ADD_TC(tp, dualstack_udp_ipv4_mixed_options);
	ATF_TP_ADD_TC(tp, dualstack_udp_ipv6_mixed_options);
	ATF_TP_ADD_TC(tp, dualstack_udp_ipv6_mixed_wildcard);
	ATF_TP_ADD_TC(tp, dualstack_udp_other_fib);
	ATF_TP_ADD_TC(tp, dualstack_udp_connect_fail);
	ATF_TP_ADD_TC(tp, dualstack_tcp_connect_fail);
	ATF_TP_ADD_TC(tp, dualstack_tcp_sendto_fail);
	ATF_TP_ADD_TC(tp, dualstack_tcp_sendto_oob_fail);

	return (atf_no_error());
}
