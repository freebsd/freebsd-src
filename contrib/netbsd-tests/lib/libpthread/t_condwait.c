/* $NetBSD: t_condwait.c,v 1.8 2019/08/11 11:42:23 martin Exp $ */

/*
 * Copyright (c) 2013 The NetBSD Foundation, Inc.
 * All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */
#include <sys/cdefs.h>
__RCSID("$NetBSD: t_condwait.c,v 1.8 2019/08/11 11:42:23 martin Exp $");

#include <sys/time.h>
#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include <atf-c.h>

#include "isqemu.h"

#include "h_common.h"

#define WAITTIME 2	/* Timeout wait secound */

static const int debug = 1;

struct run_param {
	clockid_t clck;
	bool clockwait;
	bool pshared;
};

static void *
run(void *param)
{
	struct timespec ts, to, te, twmin, twmax;
	struct run_param *rp;
	pthread_condattr_t cattr;
	pthread_cond_t cond;
	pthread_mutexattr_t mattr;
	pthread_mutex_t m;
	int ret = 0;

	rp = param;

	PTHREAD_REQUIRE(pthread_condattr_init(&cattr));
	PTHREAD_REQUIRE(pthread_mutexattr_init(&mattr));
	if (!rp->clockwait)
		PTHREAD_REQUIRE(pthread_condattr_setclock(&cattr, rp->clck));
	if (rp->pshared) {
		PTHREAD_REQUIRE(pthread_condattr_setpshared(&cattr,
		    PTHREAD_PROCESS_SHARED));
		PTHREAD_REQUIRE(pthread_mutexattr_setpshared(&mattr,
		    PTHREAD_PROCESS_SHARED));
	}
	pthread_mutex_init(&m, &mattr);
	pthread_cond_init(&cond, &cattr);

	ATF_REQUIRE_EQ((ret = pthread_mutex_lock(&m)), 0);

	ATF_REQUIRE_EQ(clock_gettime(rp->clck, &ts), 0);
	to = ts;

	if (debug)
		printf("started: %lld.%09ld sec\n", (long long)to.tv_sec,
		    to.tv_nsec);

	ts.tv_sec += WAITTIME;	/* Timeout wait */

	if (rp->clockwait)
		ret = pthread_cond_clockwait(&cond, &m, rp->clck, &ts);
	else
		ret = pthread_cond_timedwait(&cond, &m, &ts);
	switch (ret) {
	case ETIMEDOUT:
		/* Timeout */
		ATF_REQUIRE_EQ(clock_gettime(rp->clck, &te), 0);
		timespecsub(&te, &to, &to);
		if (debug) {
			printf("timeout: %lld.%09ld sec\n",
			    (long long)te.tv_sec, te.tv_nsec);
			printf("elapsed: %lld.%09ld sec\n",
			    (long long)to.tv_sec, to.tv_nsec);
		}
		twmin.tv_sec = WAITTIME;
		twmin.tv_nsec = 0;
		if (isQEMU()) {
			struct timespec td, t;
			// td.tv_sec = 0;
			// td.tv_nsec = 900000000;
			t = twmin;
			// timespecsub(&t, &td, &twmin);
			td.tv_sec = 2;
			td.tv_nsec = 500000000;
			timespecadd(&t, &td, &twmax);
		} else {
			twmax = twmin;
			twmax.tv_sec++;
		}
		ATF_REQUIRE(timespeccmp(&to, &twmin, >=));
		ATF_REQUIRE(timespeccmp(&to, &twmax, <=));
		break;
	default:
		ATF_REQUIRE_MSG(0, "pthread_cond_timedwait: %s", strerror(ret));
	}

	ATF_REQUIRE_MSG(!(ret = pthread_mutex_unlock(&m)),
	    "pthread_mutex_unlock: %s", strerror(ret));
	pthread_exit(&ret);
}

static void
cond_wait(clockid_t clck, bool clockwait, bool pshared, const char *msg)
{
	pthread_t child;
	struct run_param rp;

	rp.clck = clck;
	rp.clockwait = clockwait;
	rp.pshared = pshared;

	if (debug)
		printf( "**** %s clock wait starting\n", msg);
	ATF_REQUIRE_EQ(pthread_create(&child, NULL, run, &rp), 0);
	ATF_REQUIRE_EQ(pthread_join(child, NULL), 0); /* wait for terminate */
	if (debug)
		printf( "**** %s clock wait ended\n", msg);
}

ATF_TC(cond_wait_real);
ATF_TC_HEAD(cond_wait_real, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "Checks pthread_cond_timedwait with CLOCK_REALTIME");
}

ATF_TC_BODY(cond_wait_real, tc) {
	cond_wait(CLOCK_REALTIME, false, false, "CLOCK_REALTIME");
}

ATF_TC(cond_wait_mono);
ATF_TC_HEAD(cond_wait_mono, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "Checks pthread_cond_timedwait with CLOCK_MONOTONIC");
}

ATF_TC_BODY(cond_wait_mono, tc) {
	cond_wait(CLOCK_MONOTONIC, false, false, "CLOCK_MONOTONIC");
}

ATF_TC(cond_clockwait_real);
ATF_TC_HEAD(cond_clockwait_real, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "Checks pthread_cond_clockwait with CLOCK_REALTIME");
}

ATF_TC_BODY(cond_clockwait_real, tc) {
	cond_wait(CLOCK_REALTIME, true, false, "CLOCK_REALTIME");
}

ATF_TC(cond_clockwait_mono);
ATF_TC_HEAD(cond_clockwait_mono, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "Checks pthread_cond_clockwait with CLOCK_MONOTONIC");
}

ATF_TC_BODY(cond_clockwait_mono, tc) {
	cond_wait(CLOCK_MONOTONIC, true, false, "CLOCK_MONOTONIC");
}

ATF_TC(cond_klockwait_real);
ATF_TC_HEAD(cond_klockwait_real, tc)
{
	atf_tc_set_md_var(tc, "descr",
    "Checks pthread_cond_clockwait with CLOCK_REALTIME calling kernel");
}

ATF_TC_BODY(cond_klockwait_real, tc) {
	cond_wait(CLOCK_REALTIME, true, true, "CLOCK_REALTIME");
}

ATF_TC(cond_klockwait_mono);
ATF_TC_HEAD(cond_klockwait_mono, tc)
{
	atf_tc_set_md_var(tc, "descr",
    "Checks pthread_cond_clockwait with CLOCK_MONOTONIC calling kernel");
}

ATF_TC_BODY(cond_klockwait_mono, tc) {
	cond_wait(CLOCK_MONOTONIC, true, true, "CLOCK_MONOTONIC");
}

ATF_TP_ADD_TCS(tp)
{
	ATF_TP_ADD_TC(tp, cond_wait_real);
	ATF_TP_ADD_TC(tp, cond_wait_mono);
	ATF_TP_ADD_TC(tp, cond_clockwait_real);
	ATF_TP_ADD_TC(tp, cond_clockwait_mono);
	ATF_TP_ADD_TC(tp, cond_klockwait_real);
	ATF_TP_ADD_TC(tp, cond_klockwait_mono);
	return atf_no_error();
}
