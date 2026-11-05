/*
 * Copyright (c) 2026 Faraz Vahedi <kfv@FreeBSD.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

/*
 * Tests for timespec_getres() as per ISO/IEC 9899:2024 §7.29.2.7.
 */

#include <sys/param.h>
#include <limits.h>
#include <time.h>

#include <atf-c.h>

static const struct {
	int		base;
	clockid_t	clock;
} bases[] = {
	{ TIME_UTC,		CLOCK_REALTIME },
	{ TIME_MONOTONIC,	CLOCK_MONOTONIC },
};

static const int bad_bases[] = {
	INT_MIN, -1, 0, INT_MAX,
};

static void
check_res(const struct timespec *ts)
{
	ATF_CHECK(ts->tv_sec >= 0);
	ATF_CHECK(ts->tv_nsec >= 0 && ts->tv_nsec < 1000000000);
	ATF_CHECK(ts->tv_sec != 0 || ts->tv_nsec != 0);
}

ATF_TC_WITHOUT_HEAD(timespec_getres_supported);
ATF_TC_BODY(timespec_getres_supported, tc)
{
	struct timespec ts;

	for (size_t i = 0; i < nitems(bases); i++) {
		ATF_CHECK_EQ(timespec_getres(&ts, bases[i].base),
		    bases[i].base);
		check_res(&ts);
	}
}

ATF_TC_WITHOUT_HEAD(timespec_getres_clock);
ATF_TC_BODY(timespec_getres_clock, tc)
{
	struct timespec ts, cts;

	for (size_t i = 0; i < nitems(bases); i++) {
		ATF_REQUIRE_EQ(timespec_getres(&ts, bases[i].base),
		    bases[i].base);
		ATF_REQUIRE_EQ(clock_getres(bases[i].clock, &cts), 0);
		ATF_CHECK_EQ(ts.tv_sec, cts.tv_sec);
		ATF_CHECK_EQ(ts.tv_nsec, cts.tv_nsec);
	}
}

ATF_TC_WITHOUT_HEAD(timespec_getres_null);
ATF_TC_BODY(timespec_getres_null, tc)
{
	for (size_t i = 0; i < nitems(bases); i++)
		ATF_CHECK_EQ(timespec_getres(NULL, bases[i].base),
		    bases[i].base);
	for (size_t i = 0; i < nitems(bad_bases); i++)
		ATF_CHECK_EQ(timespec_getres(NULL, bad_bases[i]), 0);
}

ATF_TC_WITHOUT_HEAD(timespec_getres_unsupported);
ATF_TC_BODY(timespec_getres_unsupported, tc)
{
	struct timespec ts;

	for (size_t i = 0; i < nitems(bad_bases); i++) {
		ts.tv_sec = -1;
		ts.tv_nsec = -1;
		ATF_CHECK_EQ(timespec_getres(&ts, bad_bases[i]), 0);
		ATF_CHECK_EQ(ts.tv_sec, -1);
		ATF_CHECK_EQ(ts.tv_nsec, -1);
	}
}

ATF_TC_WITHOUT_HEAD(timespec_getres_timespec_get);
ATF_TC_BODY(timespec_getres_timespec_get, tc)
{
	struct timespec ts;

	for (int base = -1; base <= TIME_MONOTONIC + 8; base++)
		ATF_CHECK_EQ_MSG(timespec_getres(&ts, base) != 0,
		    timespec_get(&ts, base) != 0, "base %d", base);
}

ATF_TC_WITHOUT_HEAD(timespec_getres_stable);
ATF_TC_BODY(timespec_getres_stable, tc)
{
	struct timespec first, ts;

	for (size_t i = 0; i < nitems(bases); i++) {
		ATF_REQUIRE_EQ(timespec_getres(&first, bases[i].base),
		    bases[i].base);
		for (int n = 0; n < 100; n++) {
			ATF_REQUIRE_EQ(timespec_getres(&ts, bases[i].base),
			    bases[i].base);
			ATF_CHECK_EQ(ts.tv_sec, first.tv_sec);
			ATF_CHECK_EQ(ts.tv_nsec, first.tv_nsec);
		}
	}
}

ATF_TP_ADD_TCS(tp)
{
	ATF_TP_ADD_TC(tp, timespec_getres_supported);
	ATF_TP_ADD_TC(tp, timespec_getres_clock);
	ATF_TP_ADD_TC(tp, timespec_getres_null);
	ATF_TP_ADD_TC(tp, timespec_getres_unsupported);
	ATF_TP_ADD_TC(tp, timespec_getres_timespec_get);
	ATF_TP_ADD_TC(tp, timespec_getres_stable);

	return (atf_no_error());
}
