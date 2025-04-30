/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2018 Alan Somers
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
 * THIS SOFTWARE IS PROVIDED BY AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
#include <sys/socket.h>
#include <sys/event.h>
#include <sys/sysctl.h>
#include <sys/un.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <poll.h>
#include <pthread.h>
#include <pthread_np.h>

#include <atf-c.h>

static void
do_socketpair(int *sv)
{
	int s;

	s = socketpair(PF_LOCAL, SOCK_STREAM, 0, sv);
	ATF_REQUIRE_EQ(0, s);
	ATF_REQUIRE(sv[0] >= 0);
	ATF_REQUIRE(sv[1] >= 0);
	ATF_REQUIRE(sv[0] != sv[1]);
}

static u_long
getsendspace(void)
{
	u_long sendspace;

	ATF_REQUIRE_MSG(sysctlbyname("net.local.stream.sendspace", &sendspace,
	    &(size_t){sizeof(u_long)}, NULL, 0) != -1,
	    "sysctl net.local.stream.sendspace failed: %s", strerror(errno));

	return (sendspace);
}

/* getpeereid(3) should work with stream sockets created via socketpair(2) */
ATF_TC_WITHOUT_HEAD(getpeereid);
ATF_TC_BODY(getpeereid, tc)
{
	int sv[2];
	uid_t real_euid, euid;
	gid_t real_egid, egid;

	real_euid = geteuid();
	real_egid = getegid();

	do_socketpair(sv);

	ATF_REQUIRE_EQ(0, getpeereid(sv[0], &euid, &egid));
	ATF_CHECK_EQ(real_euid, euid);
	ATF_CHECK_EQ(real_egid, egid);

	ATF_REQUIRE_EQ(0, getpeereid(sv[1], &euid, &egid));
	ATF_CHECK_EQ(real_euid, euid);
	ATF_CHECK_EQ(real_egid, egid);

	close(sv[0]);
	close(sv[1]);
}

/* Sending zero bytes should succeed (once regressed in aba79b0f4a3f). */
ATF_TC_WITHOUT_HEAD(send_0);
ATF_TC_BODY(send_0, tc)
{
	int sv[2];

	do_socketpair(sv);
	ATF_REQUIRE(send(sv[0], sv, 0, 0) == 0);
	close(sv[0]);
	close(sv[1]);
}

static void
check_writable_select(int fd, int expect, bool timeout)
{
	fd_set wrfds;
	int nfds;

	FD_ZERO(&wrfds);
	FD_SET(fd, &wrfds);
	nfds = select(fd + 1, NULL, &wrfds, NULL, timeout ?
	    &(struct timeval){.tv_usec = 1000} : NULL);
	ATF_REQUIRE_MSG(nfds == expect,
	    "select() returns %d errno %d", nfds, errno);
}

static void
check_writable_poll(int fd, int expect, bool timeout)
{
	struct pollfd pfd[1];
	int nfds;

	pfd[0] = (struct pollfd){
		.fd = fd,
		.events = POLLOUT | POLLWRNORM,
	};
	nfds = poll(pfd, 1, timeout ? 1 : INFTIM);
	ATF_REQUIRE_MSG(nfds == expect,
	    "poll() returns %d errno %d", nfds, errno);
}

static void
check_writable_kevent(int fd, int expect, bool timeout)
{
	struct kevent kev;
	int nfds, kq;

	ATF_REQUIRE(kq = kqueue());
	EV_SET(&kev, fd, EVFILT_WRITE, EV_ADD, 0, 0, NULL);
	nfds = kevent(kq, &kev, 1, NULL, 0, NULL);
	ATF_REQUIRE_MSG(nfds == 0,
	    "kevent() returns %d errno %d", nfds, errno);
	nfds = kevent(kq, NULL, 0, &kev, 1, timeout ?
	    &(struct timespec){.tv_nsec = 1000000} : NULL);
	ATF_REQUIRE_MSG(nfds == expect,
	    "kevent() returns %d errno %d", nfds, errno);
	close(kq);
}

typedef void check_writable_func_t(int, int, bool);
struct check_writable_ctx {
	check_writable_func_t	*method;
	int			fd;
};

static void *
check_writable_blocking_thread(void *arg)
{
	struct check_writable_ctx *ctx = arg;

	ctx->method(ctx->fd, 1, false);

	return (NULL);
}

static void
full_socketpair(int *sv)
{
	void *buf;
	u_long sendspace;

	sendspace = getsendspace();
	ATF_REQUIRE((buf = malloc(sendspace)) != NULL);
	do_socketpair(sv);
	ATF_REQUIRE(fcntl(sv[0], F_SETFL, O_NONBLOCK) != -1);
	do {} while (send(sv[0], buf, sendspace, 0) == (ssize_t)sendspace);
	ATF_REQUIRE(errno == EAGAIN);
	ATF_REQUIRE(fcntl(sv[0], F_SETFL, 0) != -1);
	free(buf);
}

static void
full_writability_check(int *sv, check_writable_func_t method)
{
	struct check_writable_ctx ctx = {
		.method = method,
		.fd = sv[0],
	};
	pthread_t thr;
	void *buf;
	u_long space;

	space = getsendspace() / 2;
	ATF_REQUIRE((buf = malloc(space)) != NULL);

	/* First check with timeout, expecting 0 fds returned. */
	method(sv[0], 0, true);

	/* Launch blocking thread. */
	ATF_REQUIRE(pthread_create(&thr, NULL, check_writable_blocking_thread,
	    &ctx) == 0);

	/* Sleep a bit to make sure that thread is put to sleep. */
	usleep(10000);
	ATF_REQUIRE(pthread_peekjoin_np(thr, NULL) == EBUSY);

	/* Read some data and re-check, the fd is expected to be returned. */
	ATF_REQUIRE(read(sv[1], buf, space) == (ssize_t)space);

	method(sv[0], 1, true);

	/* Now check that thread was successfully woken up and exited. */
	ATF_REQUIRE(pthread_join(thr, NULL) == 0);

	close(sv[0]);
	close(sv[1]);
	free(buf);
}

/*
 * Make sure that a full socket is not reported as writable by event APIs.
 */
ATF_TC_WITHOUT_HEAD(full_writability_select);
ATF_TC_BODY(full_writability_select, tc)
{
	int sv[2];

	full_socketpair(sv);
	full_writability_check(sv, check_writable_select);
	close(sv[0]);
	close(sv[1]);
}

ATF_TC_WITHOUT_HEAD(full_writability_poll);
ATF_TC_BODY(full_writability_poll, tc)
{
	int sv[2];

	full_socketpair(sv);
	full_writability_check(sv, check_writable_poll);
	close(sv[0]);
	close(sv[1]);
}

ATF_TC_WITHOUT_HEAD(full_writability_kevent);
ATF_TC_BODY(full_writability_kevent, tc)
{
	int sv[2];

	full_socketpair(sv);
	full_writability_check(sv, check_writable_kevent);
	close(sv[0]);
	close(sv[1]);
}

ATF_TC_WITHOUT_HEAD(connected_writability);
ATF_TC_BODY(connected_writability, tc)
{
	int sv[2];

	do_socketpair(sv);
	check_writable_select(sv[0], 1, true);
	check_writable_poll(sv[0], 1, true);
	check_writable_kevent(sv[0], 1, true);
	close(sv[0]);
	close(sv[1]);
}

ATF_TC_WITHOUT_HEAD(unconnected_writability);
ATF_TC_BODY(unconnected_writability, tc)
{
	int s;

	ATF_REQUIRE((s = socket(PF_LOCAL, SOCK_STREAM, 0)) > 0);
	check_writable_select(s, 0, true);
	check_writable_poll(s, 0, true);
	check_writable_kevent(s, 0, true);
	close(s);
}

ATF_TP_ADD_TCS(tp)
{
	ATF_TP_ADD_TC(tp, getpeereid);
	ATF_TP_ADD_TC(tp, send_0);
	ATF_TP_ADD_TC(tp, connected_writability);
	ATF_TP_ADD_TC(tp, unconnected_writability);
	ATF_TP_ADD_TC(tp, full_writability_select);
	ATF_TP_ADD_TC(tp, full_writability_poll);
	ATF_TP_ADD_TC(tp, full_writability_kevent);

	return atf_no_error();
}
