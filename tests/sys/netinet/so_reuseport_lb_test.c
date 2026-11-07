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
#include <sys/socket.h>
#include <sys/sysctl.h>

#include <netinet/in.h>
#include <netinet/tcp.h>

#include <err.h>
#include <errno.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <unistd.h>

#include <atf-c.h>

/*
 * Given an array of non-blocking listening sockets configured in a LB group
 * for "addr", try connecting to "addr" in a loop and verify that connections
 * are roughly balanced across the sockets.
 */
static void
lb_simple_accept_loop(int domain, const struct sockaddr *addr, int sds[],
    size_t nsds, int nconns)
{
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
	size_t i;
	ssize_t n;
	int error, len, sd;
	char ch;

	for (i = 0; i < nsds; i++)
		counts[i] = 0;
	ch = 42;
	for (int c = 0; c < nclients; c++) {
		sd = socket(dst->sa_family, SOCK_DGRAM, 0);
		ATF_REQUIRE_MSG(sd >= 0, "socket() failed: %s",
		    strerror(errno));
		n = sendto(sd, &ch, sizeof(ch), 0, dst, dst->sa_len);
		ATF_REQUIRE_MSG(n == 1, "sendto() failed: %s",
		    strerror(errno));
		error = close(sd);
		ATF_REQUIRE_MSG(error == 0, "close() failed: %s",
		    strerror(errno));
	}
	usleep(100000);
	for (i = 0; i < nsds; i++) {
		while (recv(sds[i], &ch, sizeof(ch), 0) == 1)
			counts[i]++;
		error = ioctl(sds[i], FIONREAD, &len);
		ATF_REQUIRE(error == 0);
		ATF_REQUIRE(len == 0);
	}
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

	for (i = 0; i < nitems(sds); i++)
		ATF_REQUIRE(close(sds[i]) == 0);
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
	sin.sin_port = port;

	lb_udp_spray((struct sockaddr *)&sin, sds, counts, nitems(sds), 512);
	for (i = 0; i < nitems(sds); i++)
		ATF_REQUIRE_MSG(counts[i] > 0,
		    "IPv4 datagrams not balanced across FIBs, socket %zu got none",
		    i);

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
	ATF_TP_ADD_TC(tp, dualstack_tcp);
	ATF_TP_ADD_TC(tp, dualstack_udp_ipv4_precedence);
	ATF_TP_ADD_TC(tp, dualstack_udp_ipv6_precedence);
	ATF_TP_ADD_TC(tp, dualstack_udp_other_fib);
	ATF_TP_ADD_TC(tp, dualstack_udp_connect_fail);
	ATF_TP_ADD_TC(tp, dualstack_tcp_connect_fail);

	return (atf_no_error());
}
