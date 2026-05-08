/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 ConnectWise
 * Copyright (c) 2026 Mark Johnston <markj@FreeBSD.org>
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

#include <sys/types.h>
#include <sys/user.h>
#include <sys/proc.h>
#include <sys/procdesc.h>
#include <sys/sysctl.h>
#include <sys/wait.h>

#include <poll.h>
#include <pthread.h>
#include <stdio.h>
#include <unistd.h>

#include <atf-c.h>

/* Tests for procdesc(4) that aren't specific to any one syscall */

static void *
poll_procdesc(void *arg)
{
	struct pollfd pfd;

	pfd.fd = *(int *)arg;
	pfd.events = POLLHUP;
	(void)poll(&pfd, 1, 5000);
	return ((void *)(uintptr_t)pfd.revents);
}

/*
 * Regression test to exercise the case where a procdesc is closed while a
 * thread is poll()ing it.
 */
ATF_TC_WITHOUT_HEAD(poll_close_race);
ATF_TC_BODY(poll_close_race, tc)
{
	pthread_t thr;
	pid_t pid;
	uintptr_t revents;
	int error, pd;

	pid = pdfork(&pd, PD_DAEMON);
	ATF_REQUIRE_MSG(pid >= 0, "pdfork: %s", strerror(errno));
	if (pid == 0) {
		pause();
		_exit(0);
	}

	error = pthread_create(&thr, NULL, poll_procdesc, &pd);
	ATF_REQUIRE_MSG(error == 0, "pthread_create: %s", strerror(error));

	/* Wait for the thread to block in poll(2). */
	usleep(250000);

	ATF_REQUIRE_MSG(close(pd) == 0, "close: %s", strerror(errno));

	error = pthread_join(thr, (void *)&revents);
	ATF_REQUIRE_MSG(error == 0, "pthread_join: %s", strerror(error));
	ATF_REQUIRE_EQ(revents, POLLNVAL);
}

/*
 * Verify that poll(2) of a procdesc returns POLLHUP when the process exits.
 */
ATF_TC_WITHOUT_HEAD(poll_exit_wakeup);
ATF_TC_BODY(poll_exit_wakeup, tc)
{
	pthread_t thr;
	uintptr_t revents;
	pid_t pid;
	int error, pd;

	pid = pdfork(&pd, PD_DAEMON);
	ATF_REQUIRE_MSG(pid >= 0, "pdfork: %s", strerror(errno));
	if (pid == 0) {
		pause();
		_exit(0);
	}

	error = pthread_create(&thr, NULL, poll_procdesc, &pd);
	ATF_REQUIRE_MSG(error == 0, "pthread_create: %s", strerror(error));

	/* Wait for the thread to block in poll(2). */
	usleep(250000);

	ATF_REQUIRE_MSG(pdkill(pd, SIGKILL) == 0,
	    "pdkill: %s", strerror(errno));

	error = pthread_join(thr, (void *)&revents);
	ATF_REQUIRE_MSG(error == 0, "pthread_join: %s", strerror(error));
	ATF_REQUIRE_EQ(revents, POLLHUP);

	ATF_REQUIRE_MSG(close(pd) == 0, "close: %s", strerror(errno));
}

ATF_TP_ADD_TCS(tp)
{
	ATF_TP_ADD_TC(tp, poll_close_race);
	ATF_TP_ADD_TC(tp, poll_exit_wakeup);

	return (atf_no_error());
}
