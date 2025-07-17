/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2024 Gleb Smirnoff <glebius@FreeBSD.org>
 * Copyright (c) 2018 Rozhuk Ivan <rozhuk.im@gmail.com>
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
#include <sys/un.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/select.h>
#include <sys/event.h>
#include <poll.h>

#include <stdbool.h>
#include <unistd.h>
#include <time.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <pthread_np.h>

#include <atf-c.h>

/*
 * This test runs several scenarios when sleep(9) on a listen(2)ing socket is
 * interrupted by shutdown(2) or by close(2).  What should happen in that case
 * is not specified, neither is documented.  However, there is certain behavior
 * that we have and this test makes sure it is preserved.  The known software
 * to rely on the behavior is FreeSWITCH telephony software (see bug 227259).
 * There might be more. This test is based on submission with the bug, bugzilla
 * attachment 192260.
 */

static const struct test {
	enum {
		SLEEP_ACCEPT = 0,
		SLEEP_SELECT,
		SLEEP_POLL,
		SLEEP_KQUEUE,
		NSLEEP
	}		sleep;
	enum {
		WAKEUP_SHUTDOWN,
		WAKEUP_CLOSE,
	}		wakeup;
	enum {
		AFTER,
		BEFORE,
	}		when;
	bool		nonblock;
	int		result;
} tests[] = {
	{ SLEEP_ACCEPT,	WAKEUP_SHUTDOWN, AFTER, false,	ECONNABORTED },
	{ SLEEP_SELECT,	WAKEUP_SHUTDOWN, AFTER, false,	0 },
	{ SLEEP_POLL,	WAKEUP_SHUTDOWN, AFTER, false,	0 },
	{ SLEEP_KQUEUE,	WAKEUP_SHUTDOWN, AFTER, false,	0 },
	{ SLEEP_ACCEPT,	WAKEUP_CLOSE,	 AFTER, false,	ETIMEDOUT },
	{ SLEEP_SELECT,	WAKEUP_CLOSE,	 AFTER, false,	EBADF },
	{ SLEEP_POLL,	WAKEUP_CLOSE,	 AFTER, false,	0 },
	{ SLEEP_KQUEUE,	WAKEUP_CLOSE,	 AFTER, false,	0 },
	{ SLEEP_ACCEPT,	WAKEUP_SHUTDOWN, BEFORE, false,	ECONNABORTED },
	{ SLEEP_SELECT,	WAKEUP_SHUTDOWN, BEFORE, false,	0 },
	{ SLEEP_POLL,	WAKEUP_SHUTDOWN, BEFORE, false,	0 },
	{ SLEEP_KQUEUE,	WAKEUP_SHUTDOWN, BEFORE, false,	0 },
	{ SLEEP_SELECT,	WAKEUP_SHUTDOWN, AFTER, true,	0 },
	{ SLEEP_POLL,	WAKEUP_SHUTDOWN, AFTER, true,	0 },
	{ SLEEP_KQUEUE,	WAKEUP_SHUTDOWN, AFTER, true,	0 },
	{ SLEEP_SELECT,	WAKEUP_SHUTDOWN, BEFORE, true,	0 },
	{ SLEEP_POLL,	WAKEUP_SHUTDOWN, BEFORE, true,	0 },
	{ SLEEP_KQUEUE,	WAKEUP_SHUTDOWN, BEFORE, true,	0 },
};

static int
tcp_listen(void)
{
	struct sockaddr_in sin = {
		.sin_family = PF_INET,
		.sin_len = sizeof(sin),
	};
	int s;

	ATF_REQUIRE((s = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP)) != -1);
	ATF_REQUIRE(bind(s, (struct sockaddr *)&sin, sizeof(sin)) == 0);
	ATF_REQUIRE(listen(s, -1) == 0);

	return (s);
}

static int
unix_listen(void)
{
	struct sockaddr_un sun = {
		.sun_family = AF_UNIX,
		.sun_len = sizeof(sun),
		.sun_path = "listen-shutdown-test.sock",
	};
	int s;

	ATF_REQUIRE((s = socket(PF_UNIX, SOCK_STREAM, 0)) != -1);
	(void)unlink(sun.sun_path);
	ATF_REQUIRE(bind(s, (struct sockaddr *)&sun, sizeof(sun)) == 0);
	ATF_REQUIRE(listen(s, -1) == 0);

	return (s);
}

static const struct proto {
	const char *name;
	int (*listen)(void);
} protos[] = {
	{ "PF_INET", tcp_listen },
	{ "PF_UNIX", unix_listen },
};

static int
sleep_accept(int s)
{
	int rv;

	rv = accept(s, NULL, NULL);

	return (rv == -1 ? errno : 0);
}

static int
sleep_select(int s)
{
	fd_set fds;
	int rv;

	FD_ZERO(&fds);
	FD_SET(s, &fds);
	rv = select(s + 1, &fds, &fds, &fds, NULL);

	return (rv == -1 ? errno : 0);
}

static int
sleep_poll(int s)
{
	struct pollfd fds = {
		.fd = s,
		.events = (POLLIN | POLLPRI | POLLRDNORM | POLLWRNORM |
			   POLLRDBAND | POLLWRBAND),
		.revents = 0,
	};
	int rv;

	rv = poll(&fds, 1, INFTIM);

	return (rv == -1 ? errno : 0);
}

static int
sleep_kqueue(int s)
{
	struct kevent kev;
	int kq, error;

	ATF_REQUIRE((kq = kqueue()) != -1);
	EV_SET(&kev, s, EVFILT_READ, EV_ADD, 0, 0, NULL);
	if (kevent(kq, &kev, 1, NULL, 0, NULL) == -1) {
		error = errno;
	} else {
		if (kev.flags & EV_ERROR)
			error = (int)kev.data;
		else
			error = 0;
	}
	ATF_REQUIRE(close(kq) == 0);

	return (error);
}

typedef int sleep_syscall_t(int);
static sleep_syscall_t *sleep_syscalls[NSLEEP] = {
	[SLEEP_ACCEPT] = sleep_accept,
	[SLEEP_SELECT] = sleep_select,
	[SLEEP_POLL] = sleep_poll,
	[SLEEP_KQUEUE] = sleep_kqueue,
};

struct test_ctx {
	struct test const *test;
	int s;
	int result;
};

static void *
sleep_syscall_thread(void *data) {
	struct test_ctx *ctx = data;

	ctx->result = sleep_syscalls[ctx->test->sleep](ctx->s);

	return (NULL);
}

static void
run_tests(const struct proto *pr)
{
	pthread_t tid;
	struct timespec ts;
	int error;

	for (u_int i = 0; i < nitems(tests); i ++) {
		struct test const *t = &tests[i];
		struct test_ctx ctx = {
			.test = t,
			/* Note: tested syscalls don't return this. */
			.result = ETIMEDOUT,
		};

		ctx.s = pr->listen();
		if (t->nonblock)
			ATF_REQUIRE(fcntl(ctx.s, F_SETFL, O_NONBLOCK) != -1);

		if (t->when == AFTER) {
			ATF_REQUIRE(pthread_create(&tid, NULL,
			    sleep_syscall_thread, &ctx) == 0);
			usleep(100000);
		}

		switch (t->wakeup) {
		case WAKEUP_SHUTDOWN:
			ATF_REQUIRE(shutdown(ctx.s, SHUT_RDWR) == -1);
			ATF_REQUIRE(errno == ENOTCONN);
			break;
		case WAKEUP_CLOSE:
			ATF_REQUIRE(close(ctx.s) == 0);
			break;
		}

		if (t->when == BEFORE) {
			ATF_REQUIRE(pthread_create(&tid, NULL,
			    sleep_syscall_thread, &ctx) == 0);
			usleep(100000);
		}

		clock_gettime(CLOCK_REALTIME, &ts);
		ts.tv_sec++;
		if ((error = pthread_timedjoin_np(tid, NULL, &ts)) != 0) {
			ATF_REQUIRE(pthread_cancel(tid) == 0);
			ATF_REQUIRE(error == ETIMEDOUT);
			ATF_REQUIRE(ctx.result == ETIMEDOUT);
		}

		ATF_REQUIRE_MSG(ctx.result == t->result,
		    "proto %s sleeping syscall #%d wakeup #%d nb %d, "
		    "expected %d, got %d", pr->name, t->sleep, t->wakeup,
		    t->nonblock, t->result, ctx.result);

		if (t->wakeup == WAKEUP_SHUTDOWN)
			ATF_REQUIRE(close(ctx.s) == 0);
	}
}

ATF_TC_WITHOUT_HEAD(all);
ATF_TC_BODY(all, tc)
{
	for (u_int f = 0; f < nitems(protos); f++)
		run_tests(&protos[f]);
}

ATF_TP_ADD_TCS(tp)
{
	ATF_TP_ADD_TC(tp, all);

	return (atf_no_error());
}
