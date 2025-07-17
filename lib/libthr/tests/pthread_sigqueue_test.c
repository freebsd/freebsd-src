/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright 2024 The FreeBSD Foundation
 *
 * This software was developed by Konstantin Belousov <kib@FreeBSD.org>
 * under sponsorship from the FreeBSD Foundation.
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

#include <atf-c.h>
#include <err.h>
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>

#define	NTHREADS	330
static int value[NTHREADS];
static pthread_t thr[NTHREADS];
static pthread_barrier_t barrier;

static void
handler(int signo __unused, siginfo_t *info, void *data __unused)
{
	pthread_t self;
	int i;

	/*
	 * Formally this is thread-unsafe but we know context from
	 * where the signal is sent.
	 */
	self = pthread_self();
	for (i = 0; i < NTHREADS; i++) {
		if (pthread_equal(self, thr[i])) {
			    value[i] = info->si_value.sival_int;
			    pthread_exit(NULL);
		    }
	}
}

static void *
threadfunc(void *arg __unused)
{
	pthread_barrier_wait(&barrier);
	for (;;)
		pause();
}

ATF_TC(pthread_sigqueue);
ATF_TC_HEAD(pthread_sigqueue, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "Checks pthread_sigqueue(3) sigval delivery");
}

ATF_TC_BODY(pthread_sigqueue, tc)
{
	struct sigaction sa;
	union sigval sv;
	int error, i;

	error = pthread_barrier_init(&barrier, NULL, NTHREADS + 1);
	ATF_REQUIRE_EQ(0, error);

	memset(&sa, 0, sizeof(sa));
	sa.sa_sigaction = handler;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = SA_SIGINFO;

	if (sigaction(SIGUSR1, &sa, NULL) != 0)
		atf_tc_fail("sigaction failed");

	memset(&sv, 0, sizeof(sv));

	for (i = 0; i < NTHREADS; i++) {
		error = pthread_create(&thr[i], NULL, threadfunc, NULL);
		ATF_REQUIRE_EQ(0, error);
	}
	error = pthread_barrier_wait(&barrier);
	ATF_REQUIRE(error == 0 || error == PTHREAD_BARRIER_SERIAL_THREAD);

	for (i = 0; i < NTHREADS; i++) {
		sv.sival_int = i + 1000;
		error = pthread_sigqueue(thr[i], SIGUSR1, sv);
		ATF_REQUIRE_EQ(0, error);
		error = pthread_join(thr[i], NULL);
		ATF_REQUIRE_EQ(0, error);
	}
	for (i = 0; i < NTHREADS; i++)
		ATF_REQUIRE_EQ(i + 1000, value[i]);
}

ATF_TP_ADD_TCS(tp)
{
	ATF_TP_ADD_TC(tp, pthread_sigqueue);
	return atf_no_error();
}
