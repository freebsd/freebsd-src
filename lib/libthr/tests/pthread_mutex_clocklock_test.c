/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright 2026 The FreeBSD Foundation
 *
 * This software was developed by Konstantin Belousov <kib@FreeBSD.org>
 * under sponsorship from the FreeBSD Foundation.
 */

#include <sys/param.h>
#include <sys/time.h>

#include <atf-c.h>
#include <errno.h>
#include <limits.h>
#include <pthread.h>
#include <stdbool.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define	TIMEOUT_MS	250

static const struct test_clock {
	clockid_t	id;
	const char	*name;
} accepted_clocks[] = {
#define	CLK(c)	{ .id = (c), .name = #c }
	CLK(CLOCK_REALTIME),
	CLK(CLOCK_REALTIME_PRECISE),
	CLK(CLOCK_REALTIME_FAST),
	CLK(CLOCK_MONOTONIC),
	CLK(CLOCK_MONOTONIC_PRECISE),
	CLK(CLOCK_MONOTONIC_FAST),
	CLK(CLOCK_UPTIME),
	CLK(CLOCK_UPTIME_PRECISE),
	CLK(CLOCK_UPTIME_FAST),
	CLK(CLOCK_SECOND),
	CLK(CLOCK_VIRTUAL),
	CLK(CLOCK_PROF),
	CLK(CLOCK_PROCESS_CPUTIME_ID),
	CLK(CLOCK_TAI),
#undef CLK
};

struct locker {
	pthread_mutex_t	*mtx;
	clockid_t	clock;
	struct timespec	abstime;
	int		error;
	bool		reached;	/* clock reached abstime on return */
};

/*
 * Set *ts to the time msec milliseconds after the current time of
 * the clock.
 */
static void
deadline(clockid_t clock, long msec, struct timespec *ts)
{
	ATF_REQUIRE_INTEQ(0, clock_gettime(clock, ts));
	ts->tv_sec += msec / 1000;
	ts->tv_nsec += (msec % 1000) * 1000000;
	if (ts->tv_nsec >= 1000000000) {
		ts->tv_sec++;
		ts->tv_nsec -= 1000000000;
	}
}

static bool
clock_reached(clockid_t clock, const struct timespec *ts)
{
	struct timespec now;

	return (clock_gettime(clock, &now) == 0 &&
	    timespeccmp(&now, ts, >=));
}

static void
mutex_init_attr(pthread_mutex_t *mtx, int type, int protocol)
{
	pthread_mutexattr_t attr;

	ATF_REQUIRE_INTEQ(0, pthread_mutexattr_init(&attr));
	ATF_REQUIRE_INTEQ(0, pthread_mutexattr_settype(&attr, type));
	ATF_REQUIRE_INTEQ(0, pthread_mutexattr_setprotocol(&attr, protocol));
	ATF_REQUIRE_INTEQ(0, pthread_mutex_init(mtx, &attr));
	ATF_REQUIRE_INTEQ(0, pthread_mutexattr_destroy(&attr));
}

static void *
locker_thread(void *arg)
{
	struct locker *l;

	l = arg;
	l->error = pthread_mutex_clocklock(l->mtx, l->clock, &l->abstime);
	l->reached = clock_reached(l->clock, &l->abstime);
	if (l->error == 0)
		(void)pthread_mutex_unlock(l->mtx);
	return (NULL);
}

static void
locker_start(struct locker *l, pthread_t *thr, pthread_mutex_t *mtx,
    clockid_t clock, const struct timespec *abstime)
{
	l->mtx = mtx;
	l->clock = clock;
	l->abstime = *abstime;
	l->error = -1;
	l->reached = false;
	ATF_REQUIRE_INTEQ(0, pthread_create(thr, NULL, locker_thread, l));
}

/*
 * Call pthread_mutex_clocklock() from another thread, while the
 * calling thread owns the mutex.
 */
static void
clocklock_contended(pthread_mutex_t *mtx, clockid_t clock,
    const struct timespec *abstime, struct locker *l)
{
	pthread_t thr;

	locker_start(l, &thr, mtx, clock, abstime);
	ATF_REQUIRE_INTEQ(0, pthread_join(thr, NULL));
}

static void
check_timeout(int protocol, clockid_t clock)
{
	pthread_mutex_t mtx;
	struct timespec ts;
	struct locker l;

	mutex_init_attr(&mtx, PTHREAD_MUTEX_DEFAULT, protocol);
	ATF_REQUIRE_INTEQ(0, pthread_mutex_lock(&mtx));
	deadline(clock, TIMEOUT_MS, &ts);
	clocklock_contended(&mtx, clock, &ts, &l);
	ATF_CHECK_INTEQ(ETIMEDOUT, l.error);
	ATF_CHECK_MSG(l.reached, "returned before the timeout expired");
	ATF_REQUIRE_INTEQ(0, pthread_mutex_unlock(&mtx));
	ATF_REQUIRE_INTEQ(0, pthread_mutex_destroy(&mtx));
}

ATF_TC(clocklock_uncontended);
ATF_TC_HEAD(clocklock_uncontended, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "Checks that an unowned mutex is locked without validating "
	    "the clock and the timeout");
}

ATF_TC_BODY(clocklock_uncontended, tc)
{
	pthread_mutex_t mtx;
	struct timespec ts;

	ATF_REQUIRE_INTEQ(0, pthread_mutex_init(&mtx, NULL));

	deadline(CLOCK_REALTIME, TIMEOUT_MS, &ts);
	ATF_CHECK_INTEQ(0, pthread_mutex_clocklock(&mtx, CLOCK_REALTIME, &ts));
	ATF_REQUIRE_INTEQ(0, pthread_mutex_unlock(&mtx));

	deadline(CLOCK_MONOTONIC, TIMEOUT_MS, &ts);
	ATF_CHECK_INTEQ(0, pthread_mutex_clocklock(&mtx, CLOCK_MONOTONIC,
	    &ts));
	ATF_REQUIRE_INTEQ(0, pthread_mutex_unlock(&mtx));

	ATF_CHECK_INTEQ(0, pthread_mutex_clocklock(&mtx,
	    CLOCK_THREAD_CPUTIME_ID, &ts));
	ATF_REQUIRE_INTEQ(0, pthread_mutex_unlock(&mtx));

	ts.tv_nsec = 1000000000;
	ATF_CHECK_INTEQ(0, pthread_mutex_clocklock(&mtx, CLOCK_MONOTONIC,
	    &ts));
	ATF_REQUIRE_INTEQ(0, pthread_mutex_unlock(&mtx));

	ATF_REQUIRE_INTEQ(0, pthread_mutex_destroy(&mtx));
}

ATF_TC(clocklock_timeout_realtime);
ATF_TC_HEAD(clocklock_timeout_realtime, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "Checks the CLOCK_REALTIME timeout for a mutex owned by "
	    "another thread");
}

ATF_TC_BODY(clocklock_timeout_realtime, tc)
{
	check_timeout(PTHREAD_PRIO_NONE, CLOCK_REALTIME);
}

ATF_TC(clocklock_timeout_monotonic);
ATF_TC_HEAD(clocklock_timeout_monotonic, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "Checks the CLOCK_MONOTONIC timeout for a mutex owned by "
	    "another thread");
}

ATF_TC_BODY(clocklock_timeout_monotonic, tc)
{
	check_timeout(PTHREAD_PRIO_NONE, CLOCK_MONOTONIC);
}

ATF_TC(clocklock_timeout_pi);
ATF_TC_HEAD(clocklock_timeout_pi, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "Checks the CLOCK_MONOTONIC timeout for a priority-inheritance "
	    "mutex owned by another thread");
}

ATF_TC_BODY(clocklock_timeout_pi, tc)
{
	check_timeout(PTHREAD_PRIO_INHERIT, CLOCK_MONOTONIC);
}

ATF_TC(clocklock_wakeup);
ATF_TC_HEAD(clocklock_wakeup, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "Checks that a waiter gets the mutex when it is unlocked "
	    "before the timeout");
}

ATF_TC_BODY(clocklock_wakeup, tc)
{
	pthread_mutex_t mtx;
	pthread_t thr;
	struct timespec ts;
	struct locker l;

	ATF_REQUIRE_INTEQ(0, pthread_mutex_init(&mtx, NULL));
	ATF_REQUIRE_INTEQ(0, pthread_mutex_lock(&mtx));
	deadline(CLOCK_MONOTONIC, 60 * 1000, &ts);
	locker_start(&l, &thr, &mtx, CLOCK_MONOTONIC, &ts);
	usleep(100 * 1000);
	ATF_REQUIRE_INTEQ(0, pthread_mutex_unlock(&mtx));
	ATF_REQUIRE_INTEQ(0, pthread_join(thr, NULL));
	ATF_CHECK_INTEQ(0, l.error);
	ATF_CHECK_MSG(!l.reached, "returned after the timeout expired");
	ATF_REQUIRE_INTEQ(0, pthread_mutex_destroy(&mtx));
}

ATF_TC(clocklock_accepted_clocks);
ATF_TC_HEAD(clocklock_accepted_clocks, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "Checks that all clocks accepted by _umtx_op(2) can be used");
}

ATF_TC_BODY(clocklock_accepted_clocks, tc)
{
	const struct test_clock *c;
	pthread_mutex_t mtx;
	struct timespec ts;
	struct locker l;
	u_int i;

	ATF_REQUIRE_INTEQ(0, pthread_mutex_init(&mtx, NULL));
	ATF_REQUIRE_INTEQ(0, pthread_mutex_lock(&mtx));
	for (i = 0; i < nitems(accepted_clocks); i++) {
		c = &accepted_clocks[i];

		/*
		 * The deadline is the current time, so the wait times
		 * out right away.  CLOCK_TAI cannot be read until the
		 * TAI-UTC offset is known.
		 */
		if (clock_gettime(c->id, &ts) != 0) {
			ATF_REQUIRE_MSG(c->id == CLOCK_TAI,
			    "clock_gettime(%s): %s", c->name, strerror(errno));
			continue;
		}
		clocklock_contended(&mtx, c->id, &ts, &l);
		ATF_CHECK_INTEQ_MSG(ETIMEDOUT, l.error, "%s", c->name);
	}
	ATF_REQUIRE_INTEQ(0, pthread_mutex_unlock(&mtx));
	ATF_REQUIRE_INTEQ(0, pthread_mutex_destroy(&mtx));
}

ATF_TC(clocklock_rejected_clocks);
ATF_TC_HEAD(clocklock_rejected_clocks, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "Checks that EINVAL is returned for unsupported clocks when "
	    "the mutex is owned by another thread");
	atf_tc_set_md_var(tc, "timeout", "30");
}

ATF_TC_BODY(clocklock_rejected_clocks, tc)
{
	static const int protocols[] = {
		PTHREAD_PRIO_NONE,
		PTHREAD_PRIO_INHERIT,
	};
	pthread_mutex_t mtx;
	struct timespec ts;
	struct locker l;
	clockid_t clocks[4];
	u_int i, j;

	clocks[0] = CLOCK_THREAD_CPUTIME_ID;
	clocks[1] = INT_MAX;		/* not a clock */
	ATF_REQUIRE_INTEQ(0, clock_getcpuclockid(getpid(), &clocks[2]));
	ATF_REQUIRE_INTEQ(0, pthread_getcpuclockid(pthread_self(),
	    &clocks[3]));
	deadline(CLOCK_REALTIME, TIMEOUT_MS, &ts);

	for (i = 0; i < nitems(protocols); i++) {
		mutex_init_attr(&mtx, PTHREAD_MUTEX_DEFAULT, protocols[i]);
		ATF_REQUIRE_INTEQ(0, pthread_mutex_lock(&mtx));
		for (j = 0; j < nitems(clocks); j++) {
			clocklock_contended(&mtx, clocks[j], &ts, &l);
			ATF_CHECK_INTEQ_MSG(EINVAL, l.error,
			    "protocol %d clock %d", protocols[i], clocks[j]);
		}
		ATF_REQUIRE_INTEQ(0, pthread_mutex_unlock(&mtx));
		ATF_REQUIRE_INTEQ(0, pthread_mutex_destroy(&mtx));
	}
}

ATF_TC(clocklock_invalid_abstime);
ATF_TC_HEAD(clocklock_invalid_abstime, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "Checks that EINVAL is returned for an invalid timeout when "
	    "the mutex is owned by another thread");
}

ATF_TC_BODY(clocklock_invalid_abstime, tc)
{
	pthread_mutex_t mtx;
	struct timespec ts;
	struct locker l;

	ATF_REQUIRE_INTEQ(0, pthread_mutex_init(&mtx, NULL));
	ATF_REQUIRE_INTEQ(0, pthread_mutex_lock(&mtx));
	deadline(CLOCK_MONOTONIC, TIMEOUT_MS, &ts);
	ts.tv_nsec = 1000000000;
	clocklock_contended(&mtx, CLOCK_MONOTONIC, &ts, &l);
	ATF_CHECK_INTEQ(EINVAL, l.error);
	ts.tv_nsec = -1;
	clocklock_contended(&mtx, CLOCK_MONOTONIC, &ts, &l);
	ATF_CHECK_INTEQ(EINVAL, l.error);
	ATF_REQUIRE_INTEQ(0, pthread_mutex_unlock(&mtx));
	ATF_REQUIRE_INTEQ(0, pthread_mutex_destroy(&mtx));
}

ATF_TC(clocklock_zero_deadline);
ATF_TC_HEAD(clocklock_zero_deadline, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "Checks that the absolute time zero is treated as expired");
	atf_tc_set_md_var(tc, "timeout", "30");
}

ATF_TC_BODY(clocklock_zero_deadline, tc)
{
	static const struct test_clock clocks[] = {
		{ .id = CLOCK_REALTIME, .name = "CLOCK_REALTIME" },
		{ .id = CLOCK_MONOTONIC, .name = "CLOCK_MONOTONIC" },
		{ .id = CLOCK_UPTIME, .name = "CLOCK_UPTIME" },
	};
	pthread_mutex_t mtx;
	struct timespec ts;
	struct locker l;
	u_int i;

	ATF_REQUIRE_INTEQ(0, pthread_mutex_init(&mtx, NULL));
	ATF_REQUIRE_INTEQ(0, pthread_mutex_lock(&mtx));
	ts.tv_sec = 0;
	ts.tv_nsec = 0;
	for (i = 0; i < nitems(clocks); i++) {
		clocklock_contended(&mtx, clocks[i].id, &ts, &l);
		ATF_CHECK_INTEQ_MSG(ETIMEDOUT, l.error, "%s", clocks[i].name);
	}
	ATF_REQUIRE_INTEQ(0, pthread_mutex_unlock(&mtx));
	ATF_REQUIRE_INTEQ(0, pthread_mutex_destroy(&mtx));
}

ATF_TC(clocklock_errorcheck_relock);
ATF_TC_HEAD(clocklock_errorcheck_relock, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "Checks that relocking an owned error-checking mutex fails "
	    "with EDEADLK without waiting");
}

ATF_TC_BODY(clocklock_errorcheck_relock, tc)
{
	pthread_mutex_t mtx;
	struct timespec ts;

	mutex_init_attr(&mtx, PTHREAD_MUTEX_ERRORCHECK, PTHREAD_PRIO_NONE);
	ATF_REQUIRE_INTEQ(0, pthread_mutex_lock(&mtx));
	deadline(CLOCK_MONOTONIC, 60 * 1000, &ts);
	ATF_CHECK_INTEQ(EDEADLK, pthread_mutex_clocklock(&mtx,
	    CLOCK_MONOTONIC, &ts));
	ATF_CHECK_MSG(!clock_reached(CLOCK_MONOTONIC, &ts),
	    "waited for the timeout");

	/* The call does not block, so the arguments are not validated. */
	ATF_CHECK_INTEQ(EDEADLK, pthread_mutex_clocklock(&mtx,
	    CLOCK_THREAD_CPUTIME_ID, &ts));
	ts.tv_nsec = 1000000000;
	ATF_CHECK_INTEQ(EDEADLK, pthread_mutex_clocklock(&mtx,
	    CLOCK_MONOTONIC, &ts));

	ATF_REQUIRE_INTEQ(0, pthread_mutex_unlock(&mtx));
	ATF_REQUIRE_INTEQ(0, pthread_mutex_destroy(&mtx));
}

ATF_TC(clocklock_recursive_relock);
ATF_TC_HEAD(clocklock_recursive_relock, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "Checks that relocking an owned recursive mutex succeeds "
	    "without validating the clock and the timeout");
}

ATF_TC_BODY(clocklock_recursive_relock, tc)
{
	pthread_mutex_t mtx;
	struct timespec ts;

	mutex_init_attr(&mtx, PTHREAD_MUTEX_RECURSIVE, PTHREAD_PRIO_NONE);
	ATF_REQUIRE_INTEQ(0, pthread_mutex_lock(&mtx));
	ts.tv_sec = 0;
	ts.tv_nsec = 1000000000;
	ATF_CHECK_INTEQ(0, pthread_mutex_clocklock(&mtx,
	    CLOCK_THREAD_CPUTIME_ID, &ts));
	ATF_CHECK_INTEQ(0, pthread_mutex_unlock(&mtx));
	ATF_CHECK_INTEQ(0, pthread_mutex_unlock(&mtx));
	ATF_CHECK_INTEQ(EPERM, pthread_mutex_unlock(&mtx));
	ATF_REQUIRE_INTEQ(0, pthread_mutex_destroy(&mtx));
}

ATF_TC(clocklock_normal_relock);
ATF_TC_HEAD(clocklock_normal_relock, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "Checks that relocking an owned normal mutex waits for the "
	    "timeout");
}

ATF_TC_BODY(clocklock_normal_relock, tc)
{
	pthread_mutex_t mtx;
	struct timespec ts;

	mutex_init_attr(&mtx, PTHREAD_MUTEX_NORMAL, PTHREAD_PRIO_NONE);
	ATF_REQUIRE_INTEQ(0, pthread_mutex_lock(&mtx));
	deadline(CLOCK_MONOTONIC, TIMEOUT_MS, &ts);
	ATF_CHECK_INTEQ(ETIMEDOUT, pthread_mutex_clocklock(&mtx,
	    CLOCK_MONOTONIC, &ts));
	ATF_CHECK_MSG(clock_reached(CLOCK_MONOTONIC, &ts),
	    "returned before the timeout expired");

	ATF_CHECK_INTEQ(EINVAL, pthread_mutex_clocklock(&mtx,
	    CLOCK_THREAD_CPUTIME_ID, &ts));
	ts.tv_nsec = 1000000000;
	ATF_CHECK_INTEQ(EINVAL, pthread_mutex_clocklock(&mtx,
	    CLOCK_MONOTONIC, &ts));

	ATF_REQUIRE_INTEQ(0, pthread_mutex_unlock(&mtx));
	ATF_REQUIRE_INTEQ(0, pthread_mutex_destroy(&mtx));
}

ATF_TP_ADD_TCS(tp)
{
	ATF_TP_ADD_TC(tp, clocklock_uncontended);
	ATF_TP_ADD_TC(tp, clocklock_timeout_realtime);
	ATF_TP_ADD_TC(tp, clocklock_timeout_monotonic);
	ATF_TP_ADD_TC(tp, clocklock_timeout_pi);
	ATF_TP_ADD_TC(tp, clocklock_wakeup);
	ATF_TP_ADD_TC(tp, clocklock_accepted_clocks);
	ATF_TP_ADD_TC(tp, clocklock_rejected_clocks);
	ATF_TP_ADD_TC(tp, clocklock_invalid_abstime);
	ATF_TP_ADD_TC(tp, clocklock_zero_deadline);
	ATF_TP_ADD_TC(tp, clocklock_errorcheck_relock);
	ATF_TP_ADD_TC(tp, clocklock_recursive_relock);
	ATF_TP_ADD_TC(tp, clocklock_normal_relock);
	return (atf_no_error());
}
