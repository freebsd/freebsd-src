/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2024 Stormshield
 */

#include <sys/capsicum.h>
#include <sys/event.h>
#include <sys/filio.h>
#include <sys/socket.h>
#include <sys/wait.h>

#include <netinet/in.h>
#include <netinet/tcp.h>

#include <errno.h>
#include <poll.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <atf-c.h>

static void
checked_close(int fd)
{
	int error;

	error = close(fd);
	ATF_REQUIRE_MSG(error == 0, "close failed: %s", strerror(errno));
}

static int
fionread(int fd)
{
	int data, error;

	data = 0;
	error = ioctl(fd, FIONREAD, &data);
	ATF_REQUIRE_MSG(error == 0, "ioctl failed: %s", strerror(errno));
	ATF_REQUIRE(data >= 0);
	return (data);
}

static void
noblocking(int fd)
{
	int flags, error;

	flags = fcntl(fd, F_GETFL);
	ATF_REQUIRE_MSG(flags != -1, "fcntl failed: %s", strerror(errno));
	flags |= O_NONBLOCK;
	error = fcntl(fd, F_SETFL, flags);
	ATF_REQUIRE_MSG(error == 0, "fcntl failed: %s", strerror(errno));
}

/*
 * Create a pair of connected TCP sockets, returned via the "out" array.
 */
static void
tcp_socketpair(int out[2], int domain)
{
	struct sockaddr_in sin;
	struct sockaddr_in6 sin6;
	struct sockaddr *sinp;
	int error, sd[2];

	sd[0] = socket(domain, SOCK_STREAM, 0);
	ATF_REQUIRE_MSG(sd[0] >= 0, "socket failed: %s", strerror(errno));
	sd[1] = socket(domain, SOCK_STREAM, 0);
	ATF_REQUIRE_MSG(sd[1] >= 0, "socket failed: %s", strerror(errno));

	error = setsockopt(sd[0], IPPROTO_TCP, TCP_NODELAY, &(int){ 1 },
	    sizeof(int));
	ATF_REQUIRE_MSG(error == 0, "setsockopt failed: %s", strerror(errno));
	error = setsockopt(sd[1], IPPROTO_TCP, TCP_NODELAY, &(int){ 1 },
	    sizeof(int));
	ATF_REQUIRE_MSG(error == 0, "setsockopt failed: %s", strerror(errno));

	if (domain == PF_INET) {
		memset(&sin, 0, sizeof(sin));
		sin.sin_family = AF_INET;
		sin.sin_len = sizeof(sin);
		sin.sin_addr.s_addr = htonl(INADDR_ANY);
		sin.sin_port = htons(0);
		sinp = (struct sockaddr *)&sin;
	} else {
		ATF_REQUIRE(domain == PF_INET6);
		memset(&sin6, 0, sizeof(sin6));
		sin6.sin6_family = AF_INET6;
		sin6.sin6_len = sizeof(sin6);
		sin6.sin6_addr = in6addr_any;
		sin6.sin6_port = htons(0);
		sinp = (struct sockaddr *)&sin6;
	}

	error = bind(sd[0], sinp, sinp->sa_len);
	ATF_REQUIRE_MSG(error == 0, "bind failed: %s", strerror(errno));
	error = listen(sd[0], 1);
	ATF_REQUIRE_MSG(error == 0, "listen failed: %s", strerror(errno));

	error = getsockname(sd[0], sinp, &(socklen_t){ sinp->sa_len });
	ATF_REQUIRE_MSG(error == 0, "getsockname failed: %s", strerror(errno));

	error = connect(sd[1], sinp, sinp->sa_len);
	ATF_REQUIRE_MSG(error == 0, "connect failed: %s", strerror(errno));
	out[0] = accept(sd[0], NULL, NULL);
	ATF_REQUIRE_MSG(out[0] >= 0, "accept failed: %s", strerror(errno));
	checked_close(sd[0]);
	out[1] = sd[1];
}

static void
tcp4_socketpair(int out[2])
{
	tcp_socketpair(out, PF_INET);
}

static void
tcp6_socketpair(int out[2])
{
	tcp_socketpair(out, PF_INET6);
}

static off_t
nspliced(int sd)
{
	off_t n;
	socklen_t len;
	int error;

	len = sizeof(n);
	error = getsockopt(sd, SOL_SOCKET, SO_SPLICE, &n, &len);
	ATF_REQUIRE_MSG(error == 0, "getsockopt failed: %s", strerror(errno));
	ATF_REQUIRE_MSG(len == sizeof(n), "unexpected length: %d", len);
	return (n);
}

/*
 * Use a macro so that ATF_REQUIRE_MSG prints a useful line number.
 */
#define check_nspliced(sd, n) do {					\
	off_t sofar;							\
									\
	sofar = nspliced(sd);						\
	ATF_REQUIRE_MSG(sofar == (off_t)n, "spliced %jd bytes, expected %jd", \
	    (intmax_t)sofar, (intmax_t)n);				\
} while (0)

static void
splice_init(struct splice *sp, int fd, off_t max, struct timeval *tv)
{
	memset(sp, 0, sizeof(*sp));
	sp->sp_fd = fd;
	sp->sp_max = max;
	if (tv != NULL)
		sp->sp_idle = *tv;
	else
		sp->sp_idle.tv_sec = sp->sp_idle.tv_usec = 0;
}

static void
unsplice(int fd)
{
	struct splice sp;
	int error;

	splice_init(&sp, -1, 0, NULL);
	error = setsockopt(fd, SOL_SOCKET, SO_SPLICE, &sp, sizeof(sp));
	ATF_REQUIRE_MSG(error == 0, "setsockopt failed: %s", strerror(errno));
}

static void
unsplice_pair(int fd1, int fd2)
{
	unsplice(fd1);
	unsplice(fd2);
}

static void
splice_pair(int fd1, int fd2, off_t max, struct timeval *tv)
{
	struct splice sp;
	int error;

	splice_init(&sp, fd1, max, tv);
	error = setsockopt(fd2, SOL_SOCKET, SO_SPLICE, &sp, sizeof(sp));
	ATF_REQUIRE_MSG(error == 0, "setsockopt failed: %s", strerror(errno));

	splice_init(&sp, fd2, max, tv);
	error = setsockopt(fd1, SOL_SOCKET, SO_SPLICE, &sp, sizeof(sp));
	ATF_REQUIRE_MSG(error == 0, "setsockopt failed: %s", strerror(errno));
}

/*
 * A structure representing a spliced pair of connections.  left[1] is
 * bidirectionally spliced with right[0].
 */
struct splice_conn {
	int left[2];
	int right[2];
};

/*
 * Initialize a splice connection with the given maximum number of bytes to
 * splice and the given idle timeout.  For now we're forced to use TCP socket,
 * but at some point it would be nice (and simpler) to use pairs of PF_LOCAL
 * sockets.
 */
static void
splice_conn_init_limits(struct splice_conn *sc, off_t max, struct timeval *tv)
{
	memset(sc, 0, sizeof(*sc));
	tcp4_socketpair(sc->left);
	tcp4_socketpair(sc->right);
	splice_pair(sc->left[1], sc->right[0], max, tv);
}

static void
splice_conn_init(struct splice_conn *sc)
{
	splice_conn_init_limits(sc, 0, NULL);
}

static void
splice_conn_check_empty(struct splice_conn *sc)
{
	int data;

	data = fionread(sc->left[0]);
	ATF_REQUIRE_MSG(data == 0, "unexpected data on left[0]: %d", data);
	data = fionread(sc->left[1]);
	ATF_REQUIRE_MSG(data == 0, "unexpected data on left[1]: %d", data);
	data = fionread(sc->right[0]);
	ATF_REQUIRE_MSG(data == 0, "unexpected data on right[0]: %d", data);
	data = fionread(sc->right[1]);
	ATF_REQUIRE_MSG(data == 0, "unexpected data on right[1]: %d", data);
}

static void
splice_conn_fini(struct splice_conn *sc)
{
	checked_close(sc->left[0]);
	checked_close(sc->left[1]);
	checked_close(sc->right[0]);
	checked_close(sc->right[1]);
}

static void
splice_conn_noblocking(struct splice_conn *sc)
{
	noblocking(sc->left[0]);
	noblocking(sc->left[1]);
	noblocking(sc->right[0]);
	noblocking(sc->right[1]);
}

/* Pass a byte through a pair of spliced connections. */
ATF_TC_WITHOUT_HEAD(splice_basic);
ATF_TC_BODY(splice_basic, tc)
{
	struct splice_conn sc;
	ssize_t n;
	char c;

	splice_conn_init(&sc);

	check_nspliced(sc.left[1], 0);
	check_nspliced(sc.right[0], 0);

	/* Left-to-right. */
	c = 'M';
	n = write(sc.left[0], &c, 1);
	ATF_REQUIRE_MSG(n == 1, "write failed: %s", strerror(errno));
	n = read(sc.right[1], &c, 1);
	ATF_REQUIRE_MSG(n == 1, "read failed: %s", strerror(errno));
	ATF_REQUIRE_MSG(c == 'M', "unexpected character: %c", c);
	check_nspliced(sc.left[1], 1);
	check_nspliced(sc.right[0], 0);

	/* Right-to-left. */
	c = 'J';
	n = write(sc.right[1], &c, 1);
	ATF_REQUIRE_MSG(n == 1, "write failed: %s", strerror(errno));
	n = read(sc.left[0], &c, 1);
	ATF_REQUIRE_MSG(n == 1, "read failed: %s", strerror(errno));
	ATF_REQUIRE_MSG(c == 'J', "unexpected character: %c", c);
	check_nspliced(sc.left[1], 1);
	check_nspliced(sc.right[0], 1);

	/* Unsplice and verify that the byte counts haven't changed. */
	unsplice(sc.left[1]);
	unsplice(sc.right[0]);
	check_nspliced(sc.left[1], 1);
	check_nspliced(sc.right[0], 1);

	splice_conn_fini(&sc);
}

static void
remove_rights(int fd, const cap_rights_t *toremove)
{
	cap_rights_t rights;
	int error;

	error = cap_rights_get(fd, &rights);
	ATF_REQUIRE_MSG(error == 0, "cap_rights_get failed: %s",
	    strerror(errno));
	cap_rights_remove(&rights, toremove);
	error = cap_rights_limit(fd, &rights);
	ATF_REQUIRE_MSG(error == 0, "cap_rights_limit failed: %s",
	    strerror(errno));
}

/*
 * Verify that splicing fails when the socket is missing the necessary rights.
 */
ATF_TC_WITHOUT_HEAD(splice_capsicum);
ATF_TC_BODY(splice_capsicum, tc)
{
	struct splice sp;
	cap_rights_t rights;
	off_t n;
	int error, left[2], right[2];

	tcp4_socketpair(left);
	tcp4_socketpair(right);

	/*
	 * Make sure that we splice a socket that's missing recv rights.
	 */
	remove_rights(left[1], cap_rights_init(&rights, CAP_RECV));
	splice_init(&sp, right[0], 0, NULL);
	error = setsockopt(left[1], SOL_SOCKET, SO_SPLICE, &sp, sizeof(sp));
	ATF_REQUIRE_ERRNO(ENOTCAPABLE, error == -1);

	/* Make sure we can still splice left[1] in the other direction. */
	splice_init(&sp, left[1], 0, NULL);
	error = setsockopt(right[0], SOL_SOCKET, SO_SPLICE, &sp, sizeof(sp));
	ATF_REQUIRE_MSG(error == 0, "setsockopt failed: %s", strerror(errno));
	splice_init(&sp, -1, 0, NULL);
	error = setsockopt(right[0], SOL_SOCKET, SO_SPLICE, &sp, sizeof(sp));
	ATF_REQUIRE_MSG(error == 0, "setsockopt failed: %s", strerror(errno));

	/*
	 * Now remove send rights from left[1] and verify that splicing is no
	 * longer possible.
	 */
	remove_rights(left[1], cap_rights_init(&rights, CAP_SEND));
	splice_init(&sp, left[1], 0, NULL);
	error = setsockopt(right[0], SOL_SOCKET, SO_SPLICE, &sp, sizeof(sp));
	ATF_REQUIRE_ERRNO(ENOTCAPABLE, error == -1);

	/*
	 * It's still ok to query the SO_SPLICE state though.
	 */
	n = -1;
	error = getsockopt(left[1], SOL_SOCKET, SO_SPLICE, &n,
	    &(socklen_t){ sizeof(n) });
	ATF_REQUIRE_MSG(error == 0, "getsockopt failed: %s", strerror(errno));
	ATF_REQUIRE(n == 0);

	/*
	 * Make sure that we can unsplice a spliced pair without any rights
	 * other than CAP_SETSOCKOPT.
	 */
	splice_pair(left[0], right[1], 0, NULL);
	error = cap_rights_limit(left[0],
	    cap_rights_init(&rights, CAP_SETSOCKOPT));
	ATF_REQUIRE_MSG(error == 0, "cap_rights_limit failed: %s",
	    strerror(errno));
	unsplice(left[0]);

	checked_close(left[0]);
	checked_close(left[1]);
	checked_close(right[0]);
	checked_close(right[1]);
}

/*
 * Check various error cases in splice configuration.
 */
ATF_TC_WITHOUT_HEAD(splice_error);
ATF_TC_BODY(splice_error, tc)
{
	struct splice_conn sc;
	struct splice sp;
	char path[PATH_MAX];
	int error, fd, sd, usd[2];

	memset(&sc, 0, sizeof(sc));
	tcp4_socketpair(sc.left);
	tcp4_socketpair(sc.right);

	/* A negative byte limit is invalid. */
	splice_init(&sp, sc.right[0], -3, NULL);
	error = setsockopt(sc.left[1], SOL_SOCKET, SO_SPLICE, &sp, sizeof(sp));
	ATF_REQUIRE_ERRNO(EINVAL, error == -1);

	/* Can't unsplice a never-spliced socket. */
	splice_init(&sp, -1, 0, NULL);
	error = setsockopt(sc.left[1], SOL_SOCKET, SO_SPLICE, &sp, sizeof(sp));
	ATF_REQUIRE_ERRNO(ENOTCONN, error == -1);

	/* Can't double-unsplice a socket. */
	splice_init(&sp, sc.right[0], 0, NULL);
	error = setsockopt(sc.left[1], SOL_SOCKET, SO_SPLICE, &sp, sizeof(sp));
	ATF_REQUIRE_MSG(error == 0, "setsockopt failed: %s", strerror(errno));
	unsplice(sc.left[1]);
	splice_init(&sp, -1, 0, NULL);
	error = setsockopt(sc.left[1], SOL_SOCKET, SO_SPLICE, &sp, sizeof(sp));
	ATF_REQUIRE_ERRNO(ENOTCONN, error == -1);

	/* Can't splice a spliced socket */
	splice_init(&sp, sc.right[0], 0, NULL);
	error = setsockopt(sc.left[1], SOL_SOCKET, SO_SPLICE, &sp, sizeof(sp));
	ATF_REQUIRE_MSG(error == 0, "setsockopt failed: %s", strerror(errno));
	splice_init(&sp, sc.right[1], 0, NULL);
	error = setsockopt(sc.left[1], SOL_SOCKET, SO_SPLICE, &sp, sizeof(sp));
	ATF_REQUIRE_ERRNO(EBUSY, error == -1);
	splice_init(&sp, sc.right[0], 0, NULL);
	error = setsockopt(sc.left[0], SOL_SOCKET, SO_SPLICE, &sp, sizeof(sp));
	ATF_REQUIRE_ERRNO(EBUSY, error == -1);
	splice_init(&sp, -1, 0, NULL);
	error = setsockopt(sc.left[1], SOL_SOCKET, SO_SPLICE, &sp, sizeof(sp));

	/* Can't splice to a non-socket. */
	snprintf(path, sizeof(path), "/tmp/splice_error.XXXXXX");
	fd = mkstemp(path);
	ATF_REQUIRE_MSG(fd >= 0, "mkstemp failed: %s", strerror(errno));
	splice_init(&sp, fd, 0, NULL);
	error = setsockopt(sc.left[1], SOL_SOCKET, SO_SPLICE, &sp, sizeof(sp));
	ATF_REQUIRE_ERRNO(ENOTSOCK, error == -1);

	/* Can't splice to an invalid fd. */
	checked_close(fd);
	splice_init(&sp, fd, 0, NULL);
	error = setsockopt(sc.left[1], SOL_SOCKET, SO_SPLICE, &sp, sizeof(sp));
	ATF_REQUIRE_ERRNO(EBADF, error == -1);

	/* Can't splice a unix stream socket. */
	error = socketpair(AF_UNIX, SOCK_STREAM, 0, usd);
	ATF_REQUIRE_MSG(error == 0, "socketpair failed: %s", strerror(errno));
	splice_init(&sp, usd[0], 0, NULL);
	error = setsockopt(sc.left[1], SOL_SOCKET, SO_SPLICE, &sp, sizeof(sp));
	ATF_REQUIRE_ERRNO(EPROTONOSUPPORT, error == -1);
	error = setsockopt(usd[1], SOL_SOCKET, SO_SPLICE, &sp, sizeof(sp));
	ATF_REQUIRE_ERRNO(EPROTONOSUPPORT, error == -1);
	checked_close(usd[0]);
	checked_close(usd[1]);

	/* Can't splice an unconnected TCP socket. */
	sd = socket(PF_INET, SOCK_STREAM, 0);
	ATF_REQUIRE_MSG(sd >= 0, "socket failed: %s", strerror(errno));
	splice_init(&sp, sd, 0, NULL);
	error = setsockopt(sc.left[1], SOL_SOCKET, SO_SPLICE, &sp, sizeof(sp));
	ATF_REQUIRE_ERRNO(ENOTCONN, error == -1);
	splice_init(&sp, sc.right[0], 0, NULL);
	error = setsockopt(sd, SOL_SOCKET, SO_SPLICE, &sp, sizeof(sp));
	ATF_REQUIRE_ERRNO(ENOTCONN, error == -1);

	splice_conn_fini(&sc);
}

/*
 * Make sure that kevent() doesn't report read I/O events on spliced sockets.
 */
ATF_TC_WITHOUT_HEAD(splice_kevent);
ATF_TC_BODY(splice_kevent, tc)
{
	struct splice_conn sc;
	struct kevent kev;
	struct timespec ts;
	ssize_t n;
	int error, nev, kq;
	uint8_t b;

	splice_conn_init(&sc);

	kq = kqueue();
	ATF_REQUIRE_MSG(kq >= 0, "kqueue failed: %s", strerror(errno));

	EV_SET(&kev, sc.left[1], EVFILT_READ, EV_ADD, 0, 0, NULL);
	error = kevent(kq, &kev, 1, NULL, 0, NULL);
	ATF_REQUIRE_MSG(error == 0, "kevent failed: %s", strerror(errno));

	memset(&ts, 0, sizeof(ts));
	nev = kevent(kq, NULL, 0, &kev, 1, &ts);
	ATF_REQUIRE_MSG(nev >= 0, "kevent failed: %s", strerror(errno));
	ATF_REQUIRE(nev == 0);

	b = 'M';
	n = write(sc.left[0], &b, 1);
	ATF_REQUIRE_MSG(n == 1, "write failed: %s", strerror(errno));
	n = read(sc.right[1], &b, 1);
	ATF_REQUIRE_MSG(n == 1, "read failed: %s", strerror(errno));
	ATF_REQUIRE(b == 'M');

	nev = kevent(kq, NULL, 0, &kev, 1, &ts);
	ATF_REQUIRE_MSG(nev >= 0, "kevent failed: %s", strerror(errno));
	ATF_REQUIRE(nev == 0);

	b = 'J';
	n = write(sc.right[1], &b, 1);
	ATF_REQUIRE_MSG(n == 1, "write failed: %s", strerror(errno));
	n = read(sc.left[0], &b, 1);
	ATF_REQUIRE_MSG(n == 1, "read failed: %s", strerror(errno));
	ATF_REQUIRE(b == 'J');

	splice_conn_fini(&sc);
	checked_close(kq);
}

/*
 * Verify that a splice byte limit is applied.
 */
ATF_TC_WITHOUT_HEAD(splice_limit_bytes);
ATF_TC_BODY(splice_limit_bytes, tc)
{
	struct splice_conn sc;
	ssize_t n;
	uint8_t b, buf[128];

	splice_conn_init_limits(&sc, sizeof(buf) + 1, NULL);

	memset(buf, 'A', sizeof(buf));
	for (size_t total = sizeof(buf); total > 0; total -= n) {
		n = write(sc.left[0], buf, total);
		ATF_REQUIRE_MSG(n > 0, "write failed: %s", strerror(errno));
	}
	for (size_t total = sizeof(buf); total > 0; total -= n) {
		n = read(sc.right[1], buf, sizeof(buf));
		ATF_REQUIRE_MSG(n > 0, "read failed: %s", strerror(errno));
	}

	check_nspliced(sc.left[1], sizeof(buf));
	check_nspliced(sc.right[0], 0);

	/* Trigger an unsplice by writing the last byte. */
	b = 'B';
	n = write(sc.left[0], &b, 1);
	ATF_REQUIRE_MSG(n == 1, "write failed: %s", strerror(errno));
	n = read(sc.right[1], &b, 1);
	ATF_REQUIRE_MSG(n == 1, "read failed: %s", strerror(errno));
	ATF_REQUIRE(b == 'B');

	/*
	 * The next byte should appear on the other side of the connection
	 * rather than the splice.
	 */
	b = 'C';
	n = write(sc.left[0], &b, 1);
	ATF_REQUIRE_MSG(n == 1, "write failed: %s", strerror(errno));
	n = read(sc.left[1], &b, 1);
	ATF_REQUIRE_MSG(n == 1, "write failed: %s", strerror(errno));
	ATF_REQUIRE(b == 'C');

	splice_conn_check_empty(&sc);

	splice_conn_fini(&sc);
}

/*
 * Verify that a splice timeout limit is applied.
 */
ATF_TC_WITHOUT_HEAD(splice_limit_timeout);
ATF_TC_BODY(splice_limit_timeout, tc)
{
	struct splice_conn sc;
	ssize_t n;
	int error;
	uint8_t b, buf[128];

	splice_conn_init_limits(&sc, 0,
	    &(struct timeval){ .tv_sec = 0, .tv_usec = 500000 /* 500ms */ });

	/* Write some data through the splice. */
	memset(buf, 'A', sizeof(buf));
	for (size_t total = sizeof(buf); total > 0; total -= n) {
		n = write(sc.left[0], buf, total);
		ATF_REQUIRE_MSG(n > 0, "write failed: %s", strerror(errno));
	}
	for (size_t total = sizeof(buf); total > 0; total -= n) {
		n = read(sc.right[1], buf, sizeof(buf));
		ATF_REQUIRE_MSG(n > 0, "read failed: %s", strerror(errno));
	}

	check_nspliced(sc.left[1], sizeof(buf));
	check_nspliced(sc.right[0], 0);

	/* Wait for the splice to time out. */
	error = usleep(550000);
	ATF_REQUIRE_MSG(error == 0, "usleep failed: %s", strerror(errno));

	/*
	 * The next byte should appear on the other side of the connection
	 * rather than the splice.
	 */
	b = 'C';
	n = write(sc.left[0], &b, 1);
	ATF_REQUIRE_MSG(n == 1, "write failed: %s", strerror(errno));
	n = read(sc.left[1], &b, 1);
	ATF_REQUIRE_MSG(n == 1, "write failed: %s", strerror(errno));
	ATF_REQUIRE(b == 'C');

	splice_conn_fini(&sc);
}

/*
 * Make sure that listen() fails on spliced sockets, and that SO_SPLICE can't be
 * used with listening sockets.
 */
ATF_TC_WITHOUT_HEAD(splice_listen);
ATF_TC_BODY(splice_listen, tc)
{
	struct splice sp;
	struct splice_conn sc;
	int error, sd[3];

	/*
	 * These should fail regardless since the sockets are connected, but it
	 * doesn't hurt to check.
	 */
	splice_conn_init(&sc);
	error = listen(sc.left[1], 1);
	ATF_REQUIRE_ERRNO(EINVAL, error == -1);
	error = listen(sc.right[0], 1);
	ATF_REQUIRE_ERRNO(EINVAL, error == -1);
	splice_conn_fini(&sc);

	tcp4_socketpair(sd);
	sd[2] = socket(PF_INET, SOCK_STREAM, 0);
	ATF_REQUIRE_MSG(sd[2] >= 0, "socket failed: %s", strerror(errno));
	error = listen(sd[2], 1);
	ATF_REQUIRE_MSG(error == 0, "listen failed: %s", strerror(errno));

	/*
	 * Make sure a listening socket can't be spliced in either direction.
	 */
	splice_init(&sp, sd[2], 0, NULL);
	error = setsockopt(sd[1], SOL_SOCKET, SO_SPLICE, &sp, sizeof(sp));
	ATF_REQUIRE_ERRNO(EINVAL, error == -1);
	splice_init(&sp, sd[1], 0, NULL);
	error = setsockopt(sd[2], SOL_SOCKET, SO_SPLICE, &sp, sizeof(sp));
	ATF_REQUIRE_ERRNO(EINVAL, error == -1);

	/*
	 * Make sure we can't try to unsplice a listening socket.
	 */
	splice_init(&sp, -1, 0, NULL);
	error = setsockopt(sd[2], SOL_SOCKET, SO_SPLICE, &sp, sizeof(sp));
	ATF_REQUIRE_ERRNO(EINVAL, error == -1);

	checked_close(sd[0]);
	checked_close(sd[1]);
	checked_close(sd[2]);
}

static void
sigalarm(int sig __unused)
{
}

/*
 * Our SO_SPLICE implementation doesn't do anything to prevent loops.  We should
 * however make sure that they are interruptible.
 */
ATF_TC_WITHOUT_HEAD(splice_loop);
ATF_TC_BODY(splice_loop, tc)
{
	ssize_t n;
	int sd[2], status;
	pid_t child;
	char c;

	tcp_socketpair(sd, PF_INET);
	splice_pair(sd[0], sd[1], 0, NULL);

	/*
	 * Let the child process trigger an infinite loop.  It should still be
	 * possible to kill the child with a signal, causing the connection to
	 * be dropped and ending the loop.
	 */
	child = fork();
	ATF_REQUIRE_MSG(child >= 0, "fork failed: %s", strerror(errno));
	if (child == 0) {
		alarm(2);
		c = 42;
		n = write(sd[0], &c, 1);
		if (n != 1)
			_exit(2);
		c = 24;
		n = write(sd[1], &c, 1);
		if (n != 1)
			_exit(3);

		for (;;) {
			/* Wait for SIGALARM. */
			sleep(100);
		}

		_exit(0);
	} else {
		checked_close(sd[0]);
		checked_close(sd[1]);

		child = waitpid(child, &status, 0);
		ATF_REQUIRE_MSG(child >= 0,
		    "waitpid failed: %s", strerror(errno));
		ATF_REQUIRE(WIFSIGNALED(status));
		ATF_REQUIRE(WTERMSIG(status) == SIGALRM);
	}
}

/*
 * Simple I/O test.
 */
ATF_TC_WITHOUT_HEAD(splice_nonblock);
ATF_TC_BODY(splice_nonblock, tc)
{
	struct splice_conn sc;
	char buf[200];
	size_t sofar;
	ssize_t n;

	splice_conn_init(&sc);
	splice_conn_noblocking(&sc);

	memset(buf, 'A', sizeof(buf));
	for (sofar = 0;;) {
		n = write(sc.left[0], buf, sizeof(buf));
		if (n < 0) {
			ATF_REQUIRE_ERRNO(EAGAIN, n == -1);
			break;
		}
		sofar += n;
	}

	while (sofar > 0) {
		n = read(sc.right[1], buf, sizeof(buf));
		if (n < 0) {
			ATF_REQUIRE_ERRNO(EAGAIN, n == -1);
			usleep(100);
		} else {
			for (size_t i = 0; i < (size_t)n; i++)
				ATF_REQUIRE(buf[i] == 'A');
			sofar -= n;
		}
	}

	splice_conn_fini(&sc);
}

ATF_TC_WITHOUT_HEAD(splice_resplice);
ATF_TC_BODY(splice_resplice, tc)
{
	struct splice_conn sc;
	ssize_t n;
	char c;

	splice_conn_init(&sc);

	/* Left-to-right. */
	c = 'M';
	n = write(sc.left[0], &c, 1);
	ATF_REQUIRE_MSG(n == 1, "write failed: %s", strerror(errno));
	n = read(sc.right[1], &c, 1);
	ATF_REQUIRE_MSG(n == 1, "read failed: %s", strerror(errno));
	ATF_REQUIRE_MSG(c == 'M', "unexpected character: %c", c);
	check_nspliced(sc.left[1], 1);
	check_nspliced(sc.right[0], 0);

	/* Right-to-left. */
	c = 'J';
	n = write(sc.right[1], &c, 1);
	ATF_REQUIRE_MSG(n == 1, "write failed: %s", strerror(errno));
	n = read(sc.left[0], &c, 1);
	ATF_REQUIRE_MSG(n == 1, "read failed: %s", strerror(errno));
	ATF_REQUIRE_MSG(c == 'J', "unexpected character: %c", c);
	check_nspliced(sc.left[1], 1);
	check_nspliced(sc.right[0], 1);

	/* Unsplice and verify that the byte counts haven't changed. */
	unsplice(sc.left[1]);
	unsplice(sc.right[0]);
	check_nspliced(sc.left[1], 1);
	check_nspliced(sc.right[0], 1);

	/* Splice again, check that byte counts are reset. */
	splice_pair(sc.left[1], sc.right[0], 0, NULL);
	check_nspliced(sc.left[1], 0);
	check_nspliced(sc.right[0], 0);

	/* Left-to-right. */
	c = 'M';
	n = write(sc.left[0], &c, 1);
	ATF_REQUIRE_MSG(n == 1, "write failed: %s", strerror(errno));
	n = read(sc.right[1], &c, 1);
	ATF_REQUIRE_MSG(n == 1, "read failed: %s", strerror(errno));
	ATF_REQUIRE_MSG(c == 'M', "unexpected character: %c", c);
	check_nspliced(sc.left[1], 1);
	check_nspliced(sc.right[0], 0);

	/* Right-to-left. */
	c = 'J';
	n = write(sc.right[1], &c, 1);
	ATF_REQUIRE_MSG(n == 1, "write failed: %s", strerror(errno));
	n = read(sc.left[0], &c, 1);
	ATF_REQUIRE_MSG(n == 1, "read failed: %s", strerror(errno));
	ATF_REQUIRE_MSG(c == 'J', "unexpected character: %c", c);
	check_nspliced(sc.left[1], 1);
	check_nspliced(sc.right[0], 1);

	splice_conn_fini(&sc);
}

struct xfer_args {
	pthread_barrier_t *barrier;
	uint32_t bytes;
	int fd;
};

static void *
xfer(void *arg)
{
	struct xfer_args *xfer;
	uint8_t *buf;
	size_t sz;
	ssize_t n;
	uint32_t resid;
	int error;

	xfer = arg;

	error = fcntl(xfer->fd, F_SETFL, O_NONBLOCK);
	ATF_REQUIRE_MSG(error == 0, "fcntl failed: %s", strerror(errno));

	sz = MIN(xfer->bytes, 1024 * 1024);
	buf = malloc(sz);
	ATF_REQUIRE(buf != NULL);
	arc4random_buf(buf, sz);

	pthread_barrier_wait(xfer->barrier);

	for (resid = xfer->bytes; xfer->bytes > 0 || resid > 0;) {
		n = write(xfer->fd, buf, MIN(sz, xfer->bytes));
		if (n < 0) {
			ATF_REQUIRE_ERRNO(EAGAIN, n == -1);
			usleep(1000);
		} else {
			ATF_REQUIRE(xfer->bytes >= (size_t)n);
			xfer->bytes -= n;
		}

		n = read(xfer->fd, buf, sz);
		if (n < 0) {
			ATF_REQUIRE_ERRNO(EAGAIN, n == -1);
			usleep(1000);
		} else {
			ATF_REQUIRE(resid >= (size_t)n);
			resid -= n;
		}
	}

	free(buf);
	return (NULL);
}

/*
 * Use two threads to transfer data between two spliced connections.
 */
ATF_TC_WITHOUT_HEAD(splice_throughput);
ATF_TC_BODY(splice_throughput, tc)
{
	struct xfer_args xfers[2];
	pthread_t thread[2];
	pthread_barrier_t barrier;
	struct splice_conn sc;
	uint32_t bytes;
	int error;

	/* Transfer an amount between 1B and 1GB. */
	bytes = arc4random_uniform(1024 * 1024 * 1024) + 1;
	splice_conn_init(&sc);

	error = pthread_barrier_init(&barrier, NULL, 2);
	ATF_REQUIRE(error == 0);
	xfers[0] = (struct xfer_args){
	    .barrier = &barrier,
	    .bytes = bytes,
	    .fd = sc.left[0]
	};
	xfers[1] = (struct xfer_args){
	    .barrier = &barrier,
	    .bytes = bytes,
	    .fd = sc.right[1]
	};

	error = pthread_create(&thread[0], NULL, xfer, &xfers[0]);
	ATF_REQUIRE_MSG(error == 0,
	    "pthread_create failed: %s", strerror(errno));
	error = pthread_create(&thread[1], NULL, xfer, &xfers[1]);
	ATF_REQUIRE_MSG(error == 0,
	    "pthread_create failed: %s", strerror(errno));

	error = pthread_join(thread[0], NULL);
	ATF_REQUIRE_MSG(error == 0,
	    "pthread_join failed: %s", strerror(errno));
	error = pthread_join(thread[1], NULL);
	ATF_REQUIRE_MSG(error == 0,
	    "pthread_join failed: %s", strerror(errno));

	error = pthread_barrier_destroy(&barrier);
	ATF_REQUIRE(error == 0);
	splice_conn_fini(&sc);
}

/*
 * Make sure it's possible to splice v4 and v6 sockets together.
 */
ATF_TC_WITHOUT_HEAD(splice_v4v6);
ATF_TC_BODY(splice_v4v6, tc)
{
	struct splice sp;
	ssize_t n;
	int sd4[2], sd6[2];
	int error;
	uint8_t b;

	tcp4_socketpair(sd4);
	tcp6_socketpair(sd6);

	splice_init(&sp, sd6[0], 0, NULL);
	error = setsockopt(sd4[1], SOL_SOCKET, SO_SPLICE, &sp, sizeof(sp));
	ATF_REQUIRE_MSG(error == 0, "setsockopt failed: %s", strerror(errno));

	splice_init(&sp, sd4[1], 0, NULL);
	error = setsockopt(sd6[0], SOL_SOCKET, SO_SPLICE, &sp, sizeof(sp));
	ATF_REQUIRE_MSG(error == 0, "setsockopt failed: %s", strerror(errno));

	b = 'M';
	n = write(sd4[0], &b, 1);
	ATF_REQUIRE_MSG(n == 1, "write failed: %s", strerror(errno));
	n = read(sd6[1], &b, 1);
	ATF_REQUIRE_MSG(n == 1, "read failed: %s", strerror(errno));
	ATF_REQUIRE(b == 'M');

	b = 'J';
	n = write(sd6[1], &b, 1);
	ATF_REQUIRE_MSG(n == 1, "write failed: %s", strerror(errno));
	n = read(sd4[0], &b, 1);
	ATF_REQUIRE_MSG(n == 1, "read failed: %s", strerror(errno));
	ATF_REQUIRE(b == 'J');

	checked_close(sd4[0]);
	checked_close(sd4[1]);
	checked_close(sd6[0]);
	checked_close(sd6[1]);
}

ATF_TP_ADD_TCS(tp)
{
	ATF_TP_ADD_TC(tp, splice_basic);
	ATF_TP_ADD_TC(tp, splice_capsicum);
	ATF_TP_ADD_TC(tp, splice_error);
	ATF_TP_ADD_TC(tp, splice_kevent);
	ATF_TP_ADD_TC(tp, splice_limit_bytes);
	ATF_TP_ADD_TC(tp, splice_limit_timeout);
	ATF_TP_ADD_TC(tp, splice_listen);
	ATF_TP_ADD_TC(tp, splice_loop);
	ATF_TP_ADD_TC(tp, splice_nonblock);
	ATF_TP_ADD_TC(tp, splice_resplice);
	ATF_TP_ADD_TC(tp, splice_throughput);
	ATF_TP_ADD_TC(tp, splice_v4v6);
	return (atf_no_error());
}
