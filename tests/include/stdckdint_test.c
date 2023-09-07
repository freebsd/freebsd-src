/*-
 * Copyright (c) 2023 Dag-Erling Smørgrav
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <limits.h>
#include <stdckdint.h>

#include <atf-c.h>

ATF_TC_WITHOUT_HEAD(ckd_add);
ATF_TC_BODY(ckd_add, tc)
{
	int result;

	ATF_CHECK(!ckd_add(&result, INT_MAX, 0));
	ATF_CHECK_EQ(INT_MAX, result);
	ATF_CHECK(ckd_add(&result, INT_MAX, 1));
	ATF_CHECK_EQ(INT_MIN, result);
}

ATF_TC_WITHOUT_HEAD(ckd_sub);
ATF_TC_BODY(ckd_sub, tc)
{
	int result;

	ATF_CHECK(!ckd_sub(&result, INT_MIN, 0));
	ATF_CHECK_EQ(INT_MIN, result);
	ATF_CHECK(ckd_sub(&result, INT_MIN, 1));
	ATF_CHECK_EQ(INT_MAX, result);
}

ATF_TC_WITHOUT_HEAD(ckd_mul);
ATF_TC_BODY(ckd_mul, tc)
{
	int result;

	ATF_CHECK(!ckd_mul(&result, INT_MAX / 2, 2));
	ATF_CHECK_EQ(INT_MAX - 1, result);
	ATF_CHECK(ckd_mul(&result, INT_MAX / 2 + 1, 2));
	ATF_CHECK_EQ(INT_MIN, result);
}

ATF_TP_ADD_TCS(tp)
{
	ATF_TP_ADD_TC(tp, ckd_add);
	ATF_TP_ADD_TC(tp, ckd_sub);
	ATF_TP_ADD_TC(tp, ckd_mul);
	return (atf_no_error());

}
