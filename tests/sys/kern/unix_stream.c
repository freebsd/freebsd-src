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
#include <sys/select.h>
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

struct check_ctx;
typedef void check_func_t(struct check_ctx *);
struct check_ctx {
	check_func_t	*method;
	int		sv[2];
	bool		timeout;
	union {
		enum { SELECT_RD, SELECT_WR } select_what;
		short	poll_events;
		short	kev_filter;
	};
	int		nfds;
	union {
		short	poll_revents;
		unsigned short	kev_flags;
	};
};

static void
check_select(struct check_ctx *ctx)
{
	fd_set fds;
	int nfds;

	FD_ZERO(&fds);
	FD_SET(ctx->sv[0], &fds);
	nfds = select(ctx->sv[0] + 1,
	    ctx->select_what == SELECT_RD ? &fds : NULL,
	    ctx->select_what == SELECT_WR ? &fds : NULL,
	    NULL,
	    ctx->timeout ?  &(struct timeval){.tv_usec = 1000} : NULL);
	ATF_REQUIRE_MSG(nfds == ctx->nfds,
	    "select() returns %d errno %d", nfds, errno);
}

static void
check_poll(struct check_ctx *ctx)
{
	struct pollfd pfd[1];
	int nfds;

	pfd[0] = (struct pollfd){
		.fd = ctx->sv[0],
		.events = ctx->poll_events,
	};
	nfds = poll(pfd, 1, ctx->timeout ? 1 : INFTIM);
	ATF_REQUIRE_MSG(nfds == ctx->nfds,
	    "poll() returns %d errno %d", nfds, errno);
	ATF_REQUIRE((pfd[0].revents & ctx->poll_revents) == ctx->poll_revents);
}

static void
check_kevent(struct check_ctx *ctx)
{
	struct kevent kev;
	int nfds, kq;

	ATF_REQUIRE(kq = kqueue());
	EV_SET(&kev, ctx->sv[0], ctx->kev_filter, EV_ADD, 0, 0, NULL);
	nfds = kevent(kq, &kev, 1, NULL, 0, NULL);
	ATF_REQUIRE_MSG(nfds == 0,
	    "kevent() returns %d errno %d", nfds, errno);
	nfds = kevent(kq, NULL, 0, &kev, 1, ctx->timeout ?
	    &(struct timespec){.tv_nsec = 1000000} : NULL);
	ATF_REQUIRE_MSG(nfds == ctx->nfds,
	    "kevent() returns %d errno %d", nfds, errno);
	ATF_REQUIRE(kev.ident == (uintptr_t)ctx->sv[0] &&
	    kev.filter == ctx->kev_filter &&
	    (kev.flags & ctx->kev_flags) == ctx->kev_flags);
	close(kq);
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

static void *
pthread_wrap(void *arg)
{
	struct check_ctx *ctx = arg;

	ctx->method(ctx);

	return (NULL);
}

/*
 * Launch a thread that would block in event mech and return it.
 */
static pthread_t
pthread_create_blocked(struct check_ctx *ctx)
{
	pthread_t thr;

	ctx->timeout = false;
	ctx->nfds = 1;
	ATF_REQUIRE(pthread_create(&thr, NULL, pthread_wrap, ctx) == 0);

	/* Sleep a bit to make sure that thread is put to sleep. */
	usleep(10000);
	ATF_REQUIRE(pthread_peekjoin_np(thr, NULL) == EBUSY);

	return (thr);
}

static void
full_writability_check(struct check_ctx *ctx)
{
	pthread_t thr;
	void *buf;
	u_long space;

	space = getsendspace() / 2;
	ATF_REQUIRE((buf = malloc(space)) != NULL);

	/* First check with timeout, expecting 0 fds returned. */
	ctx->timeout = true;
	ctx->nfds = 0;
	ctx->method(ctx);

	thr = pthread_create_blocked(ctx);

	/* Read some data and re-check, the fd is expected to be returned. */
	ATF_REQUIRE(read(ctx->sv[1], buf, space) == (ssize_t)space);

	/* Now check that thread was successfully woken up and exited. */
	ATF_REQUIRE(pthread_join(thr, NULL) == 0);

	/* Extra check repeating what joined thread already did. */
	ctx->method(ctx);

	close(ctx->sv[0]);
	close(ctx->sv[1]);
	free(buf);
}

/*
 * Make sure that a full socket is not reported as writable by event APIs.
 */
ATF_TC_WITHOUT_HEAD(full_writability_select);
ATF_TC_BODY(full_writability_select, tc)
{
	struct check_ctx ctx = {
		.method = check_select,
		.select_what = SELECT_WR,
	};

	full_socketpair(ctx.sv);
	full_writability_check(&ctx);
	close(ctx.sv[0]);
	close(ctx.sv[1]);
}

ATF_TC_WITHOUT_HEAD(full_writability_poll);
ATF_TC_BODY(full_writability_poll, tc)
{
	struct check_ctx ctx = {
		.method = check_poll,
		.poll_events = POLLOUT | POLLWRNORM,
	};

	full_socketpair(ctx.sv);
	full_writability_check(&ctx);
	close(ctx.sv[0]);
	close(ctx.sv[1]);
}

ATF_TC_WITHOUT_HEAD(full_writability_kevent);
ATF_TC_BODY(full_writability_kevent, tc)
{
	struct check_ctx ctx = {
		.method = check_kevent,
		.kev_filter = EVFILT_WRITE,
	};

	full_socketpair(ctx.sv);
	full_writability_check(&ctx);
	close(ctx.sv[0]);
	close(ctx.sv[1]);
}

ATF_TC_WITHOUT_HEAD(connected_writability);
ATF_TC_BODY(connected_writability, tc)
{
	struct check_ctx ctx = {
		.timeout = true,
		.nfds = 1,
	};

	do_socketpair(ctx.sv);

	ctx.select_what = SELECT_WR;
	check_select(&ctx);
	ctx.poll_events = POLLOUT | POLLWRNORM;
	check_poll(&ctx);
	ctx.kev_filter = EVFILT_WRITE;
	check_kevent(&ctx);

	close(ctx.sv[0]);
	close(ctx.sv[1]);
}

ATF_TC_WITHOUT_HEAD(unconnected_writability);
ATF_TC_BODY(unconnected_writability, tc)
{
	struct check_ctx ctx = {
		.timeout = true,
		.nfds = 0,
	};

	ATF_REQUIRE((ctx.sv[0] = socket(PF_LOCAL, SOCK_STREAM, 0)) > 0);

	ctx.select_what = SELECT_WR;
	check_select(&ctx);
	ctx.poll_events = POLLOUT | POLLWRNORM;
	check_poll(&ctx);
	ctx.kev_filter = EVFILT_WRITE;
	check_kevent(&ctx);

	close(ctx.sv[0]);
}

ATF_TC_WITHOUT_HEAD(peerclosed_writability);
ATF_TC_BODY(peerclosed_writability, tc)
{
	struct check_ctx ctx = {
		.timeout = false,
		.nfds = 1,
	};

	do_socketpair(ctx.sv);
	close(ctx.sv[1]);

	ctx.select_what = SELECT_WR;
	check_select(&ctx);
	ctx.poll_events = POLLOUT | POLLWRNORM;
	check_poll(&ctx);
	ctx.kev_filter = EVFILT_WRITE;
	ctx.kev_flags = EV_EOF;
	check_kevent(&ctx);

	close(ctx.sv[0]);
}

ATF_TC_WITHOUT_HEAD(peershutdown_writability);
ATF_TC_BODY(peershutdown_writability, tc)
{
	struct check_ctx ctx = {
		.timeout = false,
		.nfds = 1,
	};

	do_socketpair(ctx.sv);
	shutdown(ctx.sv[1], SHUT_RD);

	ctx.select_what = SELECT_WR;
	check_select(&ctx);
	ctx.poll_events = POLLOUT | POLLWRNORM;
	check_poll(&ctx);
	/*
	 * XXXGL: historically unix(4) sockets were not reporting peer's
	 * shutdown(SHUT_RD) as our EV_EOF.  The kevent(2) manual page says
	 * "filter will set EV_EOF when the reader disconnects", which is hard
	 * to interpret unambigously.  For now leave the historic behavior,
	 * but we may want to change that in uipc_usrreq.c:uipc_filt_sowrite(),
	 * and then this test will also expect EV_EOF in returned flags.
	 */
	ctx.kev_filter = EVFILT_WRITE;
	check_kevent(&ctx);

	close(ctx.sv[0]);
	close(ctx.sv[1]);
}

ATF_TC_WITHOUT_HEAD(peershutdown_readability);
ATF_TC_BODY(peershutdown_readability, tc)
{
	struct check_ctx ctx = {
		.timeout = false,
		.nfds = 1,
	};
	ssize_t readsz;
	char c;

	do_socketpair(ctx.sv);
	shutdown(ctx.sv[1], SHUT_WR);

	/*
	 * The other side should flag as readable in select(2) to allow it to
	 * read(2) and observe EOF.  Ensure that both poll(2) and select(2)
	 * are consistent here.
	 */
	ctx.select_what = SELECT_RD;
	check_select(&ctx);
	ctx.poll_events = POLLIN | POLLRDNORM;
	check_poll(&ctx);

	/*
	 * Also check that read doesn't block.
	 */
	readsz = read(ctx.sv[0], &c, sizeof(c));
	ATF_REQUIRE_INTEQ(0, readsz);

	close(ctx.sv[0]);
	close(ctx.sv[1]);
}

static void
peershutdown_wakeup(struct check_ctx *ctx)
{
	pthread_t thr;

	ctx->timeout = false;
	ctx->nfds = 1;

	do_socketpair(ctx->sv);
	thr = pthread_create_blocked(ctx);
	shutdown(ctx->sv[1], SHUT_WR);
	ATF_REQUIRE(pthread_join(thr, NULL) == 0);

	close(ctx->sv[0]);
	close(ctx->sv[1]);
}

ATF_TC_WITHOUT_HEAD(peershutdown_wakeup_select);
ATF_TC_BODY(peershutdown_wakeup_select, tc)
{
	peershutdown_wakeup(&(struct check_ctx){
		.method = check_select,
		.select_what = SELECT_RD,
	});
}

ATF_TC_WITHOUT_HEAD(peershutdown_wakeup_poll);
ATF_TC_BODY(peershutdown_wakeup_poll, tc)
{
	peershutdown_wakeup(&(struct check_ctx){
		.method = check_poll,
		.poll_events = POLLIN | POLLRDNORM | POLLRDHUP,
		.poll_revents = POLLRDHUP,
	});
}

ATF_TC_WITHOUT_HEAD(peershutdown_wakeup_kevent);
ATF_TC_BODY(peershutdown_wakeup_kevent, tc)
{
	peershutdown_wakeup(&(struct check_ctx){
		.method = check_kevent,
		.kev_filter = EVFILT_READ,
		.kev_flags = EV_EOF,
	});
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
	ATF_TP_ADD_TC(tp, peerclosed_writability);
	ATF_TP_ADD_TC(tp, peershutdown_writability);
	ATF_TP_ADD_TC(tp, peershutdown_readability);
	ATF_TP_ADD_TC(tp, peershutdown_wakeup_select);
	ATF_TP_ADD_TC(tp, peershutdown_wakeup_poll);
	ATF_TP_ADD_TC(tp, peershutdown_wakeup_kevent);

	return atf_no_error();
}
