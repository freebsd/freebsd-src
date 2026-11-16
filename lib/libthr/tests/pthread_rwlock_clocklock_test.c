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
	pthread_rwlock_t *rw;
	bool		write;
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

static int
clocklock(pthread_rwlock_t *rw, bool write, clockid_t clock,
    const struct timespec *abstime)
{
	return (write ? pthread_rwlock_clockwrlock(rw, clock, abstime) :
	    pthread_rwlock_clockrdlock(rw, clock, abstime));
}

/*
 * Lock the rwlock so that the lock for the given mode cannot be
 * granted: a reader is blocked by a writer, and a writer by a reader.
 */
static void
block_mode(pthread_rwlock_t *rw, bool write)
{
	if (write)
		ATF_REQUIRE_INTEQ(0, pthread_rwlock_rdlock(rw));
	else
		ATF_REQUIRE_INTEQ(0, pthread_rwlock_wrlock(rw));
}

static void *
locker_thread(void *arg)
{
	struct locker *l;

	l = arg;
	l->error = clocklock(l->rw, l->write, l->clock, &l->abstime);
	l->reached = clock_reached(l->clock, &l->abstime);
	if (l->error == 0)
		(void)pthread_rwlock_unlock(l->rw);
	return (NULL);
}

static void
locker_start(struct locker *l, pthread_t *thr, pthread_rwlock_t *rw,
    bool write, clockid_t clock, const struct timespec *abstime)
{
	l->rw = rw;
	l->write = write;
	l->clock = clock;
	l->abstime = *abstime;
	l->error = -1;
	l->reached = false;
	ATF_REQUIRE_INTEQ(0, pthread_create(thr, NULL, locker_thread, l));
}

/*
 * Try to lock from another thread, while the calling thread holds
 * the lock in the conflicting mode.
 */
static void
clocklock_contended(pthread_rwlock_t *rw, bool write, clockid_t clock,
    const struct timespec *abstime, struct locker *l)
{
	pthread_t thr;

	locker_start(l, &thr, rw, write, clock, abstime);
	ATF_REQUIRE_INTEQ(0, pthread_join(thr, NULL));
}

static const char *
mode_name(bool write)
{
	return (write ? "pthread_rwlock_clockwrlock" :
	    "pthread_rwlock_clockrdlock");
}

static void
check_uncontended(bool write)
{
	pthread_rwlock_t rw;
	struct timespec ts;

	ATF_REQUIRE_INTEQ(0, pthread_rwlock_init(&rw, NULL));

	deadline(CLOCK_REALTIME, TIMEOUT_MS, &ts);
	ATF_CHECK_INTEQ(0, clocklock(&rw, write, CLOCK_REALTIME, &ts));
	ATF_REQUIRE_INTEQ(0, pthread_rwlock_unlock(&rw));

	deadline(CLOCK_MONOTONIC, TIMEOUT_MS, &ts);
	ATF_CHECK_INTEQ(0, clocklock(&rw, write, CLOCK_MONOTONIC, &ts));
	ATF_REQUIRE_INTEQ(0, pthread_rwlock_unlock(&rw));

	/* Not validated when the lock is granted without blocking. */
	ATF_CHECK_INTEQ(0, clocklock(&rw, write, CLOCK_THREAD_CPUTIME_ID,
	    &ts));
	ATF_REQUIRE_INTEQ(0, pthread_rwlock_unlock(&rw));

	ts.tv_nsec = 1000000000;
	ATF_CHECK_INTEQ(0, clocklock(&rw, write, CLOCK_MONOTONIC, &ts));
	ATF_REQUIRE_INTEQ(0, pthread_rwlock_unlock(&rw));

	ATF_REQUIRE_INTEQ(0, pthread_rwlock_destroy(&rw));
}

static void
check_timeout(bool write, clockid_t clock)
{
	pthread_rwlock_t rw;
	struct timespec ts;
	struct locker l;

	ATF_REQUIRE_INTEQ(0, pthread_rwlock_init(&rw, NULL));
	block_mode(&rw, write);
	deadline(clock, TIMEOUT_MS, &ts);
	clocklock_contended(&rw, write, clock, &ts, &l);
	ATF_CHECK_INTEQ(ETIMEDOUT, l.error);
	ATF_CHECK_MSG(l.reached, "returned before the timeout expired");
	ATF_REQUIRE_INTEQ(0, pthread_rwlock_unlock(&rw));
	ATF_REQUIRE_INTEQ(0, pthread_rwlock_destroy(&rw));
}

static void
check_wakeup(bool write)
{
	pthread_rwlock_t rw;
	pthread_t thr;
	struct timespec ts;
	struct locker l;

	ATF_REQUIRE_INTEQ(0, pthread_rwlock_init(&rw, NULL));
	block_mode(&rw, write);
	deadline(CLOCK_MONOTONIC, 60 * 1000, &ts);
	locker_start(&l, &thr, &rw, write, CLOCK_MONOTONIC, &ts);
	usleep(100 * 1000);
	ATF_REQUIRE_INTEQ(0, pthread_rwlock_unlock(&rw));
	ATF_REQUIRE_INTEQ(0, pthread_join(thr, NULL));
	ATF_CHECK_INTEQ(0, l.error);
	ATF_CHECK_MSG(!l.reached, "returned after the timeout expired");
	ATF_REQUIRE_INTEQ(0, pthread_rwlock_destroy(&rw));
}

static void
check_accepted_clocks(bool write)
{
	const struct test_clock *c;
	pthread_rwlock_t rw;
	struct timespec ts;
	struct locker l;
	u_int i;

	ATF_REQUIRE_INTEQ(0, pthread_rwlock_init(&rw, NULL));
	block_mode(&rw, write);
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
		clocklock_contended(&rw, write, c->id, &ts, &l);
		ATF_CHECK_INTEQ_MSG(ETIMEDOUT, l.error, "%s %s",
		    mode_name(write), c->name);
	}
	ATF_REQUIRE_INTEQ(0, pthread_rwlock_unlock(&rw));
	ATF_REQUIRE_INTEQ(0, pthread_rwlock_destroy(&rw));
}

static void
check_rejected_clocks(bool write)
{
	pthread_rwlock_t rw;
	struct timespec ts;
	struct locker l;
	clockid_t clocks[4];
	u_int i;

	clocks[0] = CLOCK_THREAD_CPUTIME_ID;
	clocks[1] = INT_MAX;		/* not a clock */
	ATF_REQUIRE_INTEQ(0, clock_getcpuclockid(getpid(), &clocks[2]));
	ATF_REQUIRE_INTEQ(0, pthread_getcpuclockid(pthread_self(),
	    &clocks[3]));
	deadline(CLOCK_REALTIME, TIMEOUT_MS, &ts);

	ATF_REQUIRE_INTEQ(0, pthread_rwlock_init(&rw, NULL));
	block_mode(&rw, write);
	for (i = 0; i < nitems(clocks); i++) {
		clocklock_contended(&rw, write, clocks[i], &ts, &l);
		ATF_CHECK_INTEQ_MSG(EINVAL, l.error, "%s clock %d",
		    mode_name(write), clocks[i]);
	}
	ATF_REQUIRE_INTEQ(0, pthread_rwlock_unlock(&rw));
	ATF_REQUIRE_INTEQ(0, pthread_rwlock_destroy(&rw));
}

static void
check_invalid_abstime(bool write)
{
	pthread_rwlock_t rw;
	struct timespec ts;
	struct locker l;

	ATF_REQUIRE_INTEQ(0, pthread_rwlock_init(&rw, NULL));
	block_mode(&rw, write);
	deadline(CLOCK_MONOTONIC, TIMEOUT_MS, &ts);
	ts.tv_nsec = 1000000000;
	clocklock_contended(&rw, write, CLOCK_MONOTONIC, &ts, &l);
	ATF_CHECK_INTEQ_MSG(EINVAL, l.error, "%s", mode_name(write));
	ts.tv_nsec = -1;
	clocklock_contended(&rw, write, CLOCK_MONOTONIC, &ts, &l);
	ATF_CHECK_INTEQ_MSG(EINVAL, l.error, "%s", mode_name(write));
	ATF_REQUIRE_INTEQ(0, pthread_rwlock_unlock(&rw));
	ATF_REQUIRE_INTEQ(0, pthread_rwlock_destroy(&rw));
}

static void
check_zero_deadline(bool write)
{
	static const struct test_clock clocks[] = {
		{ .id = CLOCK_REALTIME, .name = "CLOCK_REALTIME" },
		{ .id = CLOCK_MONOTONIC, .name = "CLOCK_MONOTONIC" },
		{ .id = CLOCK_UPTIME, .name = "CLOCK_UPTIME" },
	};
	pthread_rwlock_t rw;
	struct timespec ts;
	struct locker l;
	u_int i;

	ATF_REQUIRE_INTEQ(0, pthread_rwlock_init(&rw, NULL));
	block_mode(&rw, write);
	ts.tv_sec = 0;
	ts.tv_nsec = 0;
	for (i = 0; i < nitems(clocks); i++) {
		clocklock_contended(&rw, write, clocks[i].id, &ts, &l);
		ATF_CHECK_INTEQ_MSG(ETIMEDOUT, l.error, "%s %s",
		    mode_name(write), clocks[i].name);
	}
	ATF_REQUIRE_INTEQ(0, pthread_rwlock_unlock(&rw));
	ATF_REQUIRE_INTEQ(0, pthread_rwlock_destroy(&rw));
}

ATF_TC(clockrdlock_uncontended);
ATF_TC_HEAD(clockrdlock_uncontended, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "Checks that a free rwlock is read-locked without validating "
	    "the clock and the timeout");
}

ATF_TC_BODY(clockrdlock_uncontended, tc)
{
	check_uncontended(false);
}

ATF_TC(clockwrlock_uncontended);
ATF_TC_HEAD(clockwrlock_uncontended, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "Checks that a free rwlock is write-locked without validating "
	    "the clock and the timeout");
}

ATF_TC_BODY(clockwrlock_uncontended, tc)
{
	check_uncontended(true);
}

ATF_TC(clockrdlock_timeout_realtime);
ATF_TC_HEAD(clockrdlock_timeout_realtime, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "Checks the CLOCK_REALTIME timeout for a read lock");
}

ATF_TC_BODY(clockrdlock_timeout_realtime, tc)
{
	check_timeout(false, CLOCK_REALTIME);
}

ATF_TC(clockrdlock_timeout_monotonic);
ATF_TC_HEAD(clockrdlock_timeout_monotonic, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "Checks the CLOCK_MONOTONIC timeout for a read lock");
}

ATF_TC_BODY(clockrdlock_timeout_monotonic, tc)
{
	check_timeout(false, CLOCK_MONOTONIC);
}

ATF_TC(clockwrlock_timeout_realtime);
ATF_TC_HEAD(clockwrlock_timeout_realtime, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "Checks the CLOCK_REALTIME timeout for a write lock");
}

ATF_TC_BODY(clockwrlock_timeout_realtime, tc)
{
	check_timeout(true, CLOCK_REALTIME);
}

ATF_TC(clockwrlock_timeout_monotonic);
ATF_TC_HEAD(clockwrlock_timeout_monotonic, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "Checks the CLOCK_MONOTONIC timeout for a write lock");
}

ATF_TC_BODY(clockwrlock_timeout_monotonic, tc)
{
	check_timeout(true, CLOCK_MONOTONIC);
}

ATF_TC(clockrdlock_wakeup);
ATF_TC_HEAD(clockrdlock_wakeup, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "Checks that a reader gets the lock when the writer unlocks "
	    "it before the timeout");
}

ATF_TC_BODY(clockrdlock_wakeup, tc)
{
	check_wakeup(false);
}

ATF_TC(clockwrlock_wakeup);
ATF_TC_HEAD(clockwrlock_wakeup, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "Checks that a writer gets the lock when the reader unlocks "
	    "it before the timeout");
}

ATF_TC_BODY(clockwrlock_wakeup, tc)
{
	check_wakeup(true);
}

ATF_TC(clocklock_accepted_clocks);
ATF_TC_HEAD(clocklock_accepted_clocks, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "Checks that all clocks accepted by _umtx_op(2) can be used");
}

ATF_TC_BODY(clocklock_accepted_clocks, tc)
{
	check_accepted_clocks(false);
	check_accepted_clocks(true);
}

ATF_TC(clocklock_rejected_clocks);
ATF_TC_HEAD(clocklock_rejected_clocks, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "Checks that EINVAL is returned for unsupported clocks when "
	    "the lock cannot be granted");
	atf_tc_set_md_var(tc, "timeout", "30");
}

ATF_TC_BODY(clocklock_rejected_clocks, tc)
{
	check_rejected_clocks(false);
	check_rejected_clocks(true);
}

ATF_TC(clocklock_invalid_abstime);
ATF_TC_HEAD(clocklock_invalid_abstime, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "Checks that EINVAL is returned for an invalid timeout when "
	    "the lock cannot be granted");
}

ATF_TC_BODY(clocklock_invalid_abstime, tc)
{
	check_invalid_abstime(false);
	check_invalid_abstime(true);
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
	check_zero_deadline(false);
	check_zero_deadline(true);
}

ATF_TC(clocklock_self_deadlock);
ATF_TC_HEAD(clocklock_self_deadlock, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "Checks that locking an rwlock write-locked by the calling "
	    "thread fails with EDEADLK without waiting");
}

ATF_TC_BODY(clocklock_self_deadlock, tc)
{
	pthread_rwlock_t rw;
	struct timespec ts;

	ATF_REQUIRE_INTEQ(0, pthread_rwlock_init(&rw, NULL));
	ATF_REQUIRE_INTEQ(0, pthread_rwlock_wrlock(&rw));
	deadline(CLOCK_MONOTONIC, 60 * 1000, &ts);
	ATF_CHECK_INTEQ(EDEADLK, pthread_rwlock_clockrdlock(&rw,
	    CLOCK_MONOTONIC, &ts));
	ATF_CHECK_INTEQ(EDEADLK, pthread_rwlock_clockwrlock(&rw,
	    CLOCK_MONOTONIC, &ts));
	ATF_CHECK_MSG(!clock_reached(CLOCK_MONOTONIC, &ts),
	    "waited for the timeout");

	/* The calls do not block, so the arguments are not validated. */
	ATF_CHECK_INTEQ(EDEADLK, pthread_rwlock_clockrdlock(&rw,
	    CLOCK_THREAD_CPUTIME_ID, &ts));
	ATF_CHECK_INTEQ(EDEADLK, pthread_rwlock_clockwrlock(&rw,
	    CLOCK_THREAD_CPUTIME_ID, &ts));
	ts.tv_nsec = 1000000000;
	ATF_CHECK_INTEQ(EDEADLK, pthread_rwlock_clockrdlock(&rw,
	    CLOCK_MONOTONIC, &ts));
	ATF_CHECK_INTEQ(EDEADLK, pthread_rwlock_clockwrlock(&rw,
	    CLOCK_MONOTONIC, &ts));

	ATF_REQUIRE_INTEQ(0, pthread_rwlock_unlock(&rw));
	ATF_REQUIRE_INTEQ(0, pthread_rwlock_destroy(&rw));
}

ATF_TP_ADD_TCS(tp)
{
	ATF_TP_ADD_TC(tp, clockrdlock_uncontended);
	ATF_TP_ADD_TC(tp, clockwrlock_uncontended);
	ATF_TP_ADD_TC(tp, clockrdlock_timeout_realtime);
	ATF_TP_ADD_TC(tp, clockrdlock_timeout_monotonic);
	ATF_TP_ADD_TC(tp, clockwrlock_timeout_realtime);
	ATF_TP_ADD_TC(tp, clockwrlock_timeout_monotonic);
	ATF_TP_ADD_TC(tp, clockrdlock_wakeup);
	ATF_TP_ADD_TC(tp, clockwrlock_wakeup);
	ATF_TP_ADD_TC(tp, clocklock_accepted_clocks);
	ATF_TP_ADD_TC(tp, clocklock_rejected_clocks);
	ATF_TP_ADD_TC(tp, clocklock_invalid_abstime);
	ATF_TP_ADD_TC(tp, clocklock_zero_deadline);
	ATF_TP_ADD_TC(tp, clocklock_self_deadlock);
	return (atf_no_error());
}
