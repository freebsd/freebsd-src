/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 Vladimir Grebenshchikov <vova@zote.me>
 */

#include <atf-c.h>
#include "linux_test_support.h"

ATF_TC(partial_result);
ATF_TC_HEAD(partial_result, tc)
{

	atf_tc_set_md_var(tc, "descr",
	    "Return datagrams received before EAGAIN");
	atf_tc_set_md_var(tc, "require.kmods", "linux64");
}
ATF_TC_BODY(partial_result, tc)
{
	linux_test_run(tc, "recvmmsg_linux");
}

ATF_TP_ADD_TCS(tp)
{

	ATF_TP_ADD_TC(tp, partial_result);
	return (atf_no_error());
}
