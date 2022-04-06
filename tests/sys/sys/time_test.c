/*-
 * Copyright (c) 2022 Axcient
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions, and the following disclaimer,
 *    without modification.
 * 2. Redistributions in binary form must reproduce at minimum a disclaimer
 *    substantially similar to the "NO WARRANTY" disclaimer below
 *    ("Disclaimer") and any redistribution must be conditioned upon
 *    including a substantially similar Disclaimer requirement for further
 *    binary redistribution.
 *
 * NO WARRANTY
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDERS OR CONTRIBUTORS BE LIABLE FOR SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGES.
 *
 * $FreeBSD$
 */
#include <sys/types.h>
#include <sys/time.h>

#include <inttypes.h>
#include <stdio.h>

#include <atf-c.h>


static void
atf_check_nstosbt(sbintime_t expected, int64_t ns) {
	sbintime_t actual = nstosbt(ns);

	ATF_CHECK_MSG((expected) - 1 <= (actual) && actual <= (expected) + 1,
			"%"PRId64" != nstosbt(%"PRId64") (%"PRId64")",
			expected, ns, actual);
}

ATF_TC_WITHOUT_HEAD(nstosbt);
ATF_TC_BODY(nstosbt, tc)
{
	atf_check_nstosbt(0, 0);
	atf_check_nstosbt(4, 1);
	/* 1 second */
	atf_check_nstosbt((1ll << 32) - 4, 999999999);
	atf_check_nstosbt(1ll << 32, 1000000000);
	/* 2 seconds https://bugs.freebsd.org/bugzilla/show_bug.cgi?id=263073 */
	atf_check_nstosbt((1ll << 33) - 4, 1999999999);
	atf_check_nstosbt(1ll << 33, 2000000000);
	/* 4 seconds */
	atf_check_nstosbt((1ll << 34) - 4, 3999999999);
	atf_check_nstosbt((1ll << 34), 4000000000);
	/* Max value */
	atf_check_nstosbt(((1ll << 31) - 1) << 32,
			  ((1ll << 31) - 1) * 1000000000);
}

static void
atf_check_ustosbt(sbintime_t expected, int64_t us) {
	sbintime_t actual = ustosbt(us);

	ATF_CHECK_MSG((expected) - 1 <= (actual) && actual <= (expected) + 1,
			"%"PRId64" != ustosbt(%"PRId64") (%"PRId64")",
			expected, us, actual);
}

ATF_TC_WITHOUT_HEAD(ustosbt);
ATF_TC_BODY(ustosbt, tc)
{
	atf_check_ustosbt(0, 0);
	atf_check_ustosbt(4295, 1);
	/* 1 second */
	atf_check_ustosbt((1ll << 32) - 4295, 999999);
	atf_check_ustosbt(1ll << 32, 1000000);
	/* 2 seconds https://bugs.freebsd.org/bugzilla/show_bug.cgi?id=263073 */
	atf_check_ustosbt((1ll << 33) - 4295, 1999999);
	atf_check_ustosbt(1ll << 33, 2000000);
	/* 4 seconds */
	atf_check_ustosbt((1ll << 34) - 4295, 3999999);
	atf_check_ustosbt(1ll << 34, 4000000);
	/* Max value */
	atf_check_ustosbt(((1ull << 31) - 1) << 32,
			  ((1ll << 31) - 1) * 1000000);
}

static void
atf_check_mstosbt(sbintime_t expected, int64_t ms) {
	sbintime_t actual = mstosbt(ms);

	ATF_CHECK_MSG((expected) - 1 <= (actual) && actual <= (expected) + 1,
			"%"PRId64" != mstosbt(%"PRId64") (%"PRId64")",
			expected, ms, actual);
}

ATF_TC_WITHOUT_HEAD(mstosbt);
ATF_TC_BODY(mstosbt, tc)
{
	atf_check_mstosbt(0, 0);
	atf_check_mstosbt(4294967, 1);
	/* 1 second */
	atf_check_mstosbt((1ll << 32) - 4294968, 999);
	atf_check_mstosbt(1ll << 32, 1000);
	/* 2 seconds https://bugs.freebsd.org/bugzilla/show_bug.cgi?id=263073 */
	atf_check_mstosbt((1ll << 33) - 4294968, 1999);
	atf_check_mstosbt(1ll << 33, 2000);
	/* 4 seconds */
	atf_check_mstosbt((1ll << 34) - 4294968, 3999);
	atf_check_mstosbt(1ll << 34, 4000);
	/* Max value */
	atf_check_mstosbt(((1ll << 31) - 1) << 32, ((1ll << 31) - 1) * 1000);
}

static void
atf_check_sbttons(int64_t expected, sbintime_t sbt) {
	int64_t actual = sbttons(sbt);

	ATF_CHECK_MSG((expected) - 1 <= (actual) && actual <= (expected) + 1,
			"%"PRId64" != sbttons(%"PRId64") (%"PRId64")",
			expected, sbt, actual);
}

ATF_TC_WITHOUT_HEAD(sbttons);
ATF_TC_BODY(sbttons, tc)
{
	atf_check_sbttons(0, 0);
	atf_check_sbttons(0, 1);
	atf_check_sbttons(1, (1ll << 32) / 1000000000);
	/* 1 second */
	atf_check_sbttons(1000000000, 1ll << 32);
	atf_check_sbttons(1999999999, (1ll << 33) - 1);
	/* 2 seconds */
	atf_check_sbttons(1999999999, (1ll << 33) - 1);
	atf_check_sbttons(2000000000, 1ll << 33);
	/* 4 seconds */
	atf_check_sbttons(3999999999, (1ll << 34) - 1);
	atf_check_sbttons(4000000000, 1ll << 34);
	/* edge cases */
	atf_check_sbttons(999999999, (1ll << 32) - 1);
	atf_check_sbttons((1ll << 31) * 1000000000, (1ull << 63) - 1);
}

static void
atf_check_sbttous(int64_t expected, sbintime_t sbt) {
	int64_t actual = sbttous(sbt);

	ATF_CHECK_MSG((expected) - 1 <= (actual) && actual <= (expected) + 1,
			"%"PRId64" != sbttous(%"PRId64") (%"PRId64")",
			expected, sbt, actual);
}

ATF_TC_WITHOUT_HEAD(sbttous);
ATF_TC_BODY(sbttous, tc)
{
	atf_check_sbttous(0, 0);
	atf_check_sbttous(0, 1);
	atf_check_sbttous(1, (1ll << 32) / 1000000);
	/* 1 second */
	atf_check_sbttous(1000000, 1ll << 32);
	atf_check_sbttous(1999999, (1ll << 33) - 1);
	/* 2 seconds */
	atf_check_sbttous(1999999, (1ll << 33) - 1);
	atf_check_sbttous(2000000, 1ll << 33);
	/* 4 seconds */
	atf_check_sbttous(3999999, (1ll << 34) -1);
	atf_check_sbttous(4000000, 1ll << 34);
	/* Overflows (bug 263073) */
	atf_check_sbttous(1ll << 31, (1ull << 63) / 1000000);
	atf_check_sbttous(1ll << 31, (1ull << 63) / 1000000 + 1);
	atf_check_sbttous((1ll << 31) * 1000000, (1ull << 63) - 1);
}

static void
atf_check_sbttoms(int64_t expected, sbintime_t sbt) {
	int64_t actual = sbttoms(sbt);

	ATF_CHECK_MSG((expected) - 1 <= (actual) && actual <= (expected) + 1,
			"%"PRId64" != sbttoms(%"PRId64") (%"PRId64")",
			expected, sbt, actual);
}

ATF_TC_WITHOUT_HEAD(sbttoms);
ATF_TC_BODY(sbttoms, tc)
{
	atf_check_sbttoms(0, 0);
	atf_check_sbttoms(0, 1);
	atf_check_sbttoms(1, (1ll << 32) / 1000);
	/* 1 second */
	atf_check_sbttoms(999, (1ll << 32) - 1);
	atf_check_sbttoms(1000, 1ll << 32);
	/* 2 seconds */
	atf_check_sbttoms(1999, (1ll << 33) - 1);
	atf_check_sbttoms(2000, 1ll << 33);
	/* 4 seconds */
	atf_check_sbttoms(3999, (1ll << 34) - 1);
	atf_check_sbttoms(4000, 1ll << 34);
	/* Overflows (bug 263073) */
	atf_check_sbttoms(1ll << 31, (1ull << 63) / 1000);
	atf_check_sbttoms(1ll << 31, (1ull << 63) / 1000 + 1);
	atf_check_sbttoms((1ll << 31) * 1000, (1ull << 63) - 1);
}

ATF_TP_ADD_TCS(tp)
{

	ATF_TP_ADD_TC(tp, nstosbt);
	ATF_TP_ADD_TC(tp, ustosbt);
	ATF_TP_ADD_TC(tp, mstosbt);
	ATF_TP_ADD_TC(tp, sbttons);
	ATF_TP_ADD_TC(tp, sbttous);
	ATF_TP_ADD_TC(tp, sbttoms);

	return (atf_no_error());
}
