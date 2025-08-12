/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2025 ConnectWise
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer
 *    in this position and unchanged.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/param.h>
#include <sys/errno.h>

#include <atf-c.h>

#include "../sockstat.h"

ATF_TC_WITHOUT_HEAD(backwards_range);
ATF_TC_BODY(backwards_range, tc)
{
	const char portspec[] = "22-21";

	ATF_CHECK_EQ(ERANGE, parse_ports(portspec));
}

ATF_TC_WITHOUT_HEAD(erange_low);
ATF_TC_BODY(erange_low, tc)
{
	const char portspec[] = "-1";

	ATF_CHECK_EQ(ERANGE, parse_ports(portspec));
}

ATF_TC_WITHOUT_HEAD(erange_high);
ATF_TC_BODY(erange_high, tc)
{
	const char portspec[] = "65536";

	ATF_CHECK_EQ(ERANGE, parse_ports(portspec));
}

ATF_TC_WITHOUT_HEAD(erange_on_range_end);
ATF_TC_BODY(erange_on_range_end, tc)
{
	const char portspec[] = "22-65536";

	ATF_CHECK_EQ(ERANGE, parse_ports(portspec));
}

ATF_TC_WITHOUT_HEAD(multiple);
ATF_TC_BODY(multiple, tc)
{
	const char portspec[] = "80,443";

	ATF_REQUIRE_EQ(0, parse_ports(portspec));

	ATF_CHECK(!CHK_PORT(0));

	ATF_CHECK(!CHK_PORT(79));
	ATF_CHECK(CHK_PORT(80));
	ATF_CHECK(!CHK_PORT(81));

	ATF_CHECK(!CHK_PORT(442));
	ATF_CHECK(CHK_PORT(443));
	ATF_CHECK(!CHK_PORT(444));
}

ATF_TC_WITHOUT_HEAD(multiple_plus_ranges);
ATF_TC_BODY(multiple_plus_ranges, tc)
{
	const char portspec[] = "80,443,500-501,510,520,40000-40002";

	ATF_REQUIRE_EQ(0, parse_ports(portspec));

	ATF_CHECK(!CHK_PORT(0));

	ATF_CHECK(!CHK_PORT(79));
	ATF_CHECK(CHK_PORT(80));
	ATF_CHECK(!CHK_PORT(81));

	ATF_CHECK(!CHK_PORT(442));
	ATF_CHECK(CHK_PORT(443));
	ATF_CHECK(!CHK_PORT(444));

	ATF_CHECK(!CHK_PORT(499));
	ATF_CHECK(CHK_PORT(500));
	ATF_CHECK(CHK_PORT(501));
	ATF_CHECK(!CHK_PORT(502));

	ATF_CHECK(!CHK_PORT(519));
	ATF_CHECK(CHK_PORT(520));
	ATF_CHECK(!CHK_PORT(521));

	ATF_CHECK(!CHK_PORT(39999));
	ATF_CHECK(CHK_PORT(40000));
	ATF_CHECK(CHK_PORT(40001));
	ATF_CHECK(CHK_PORT(40002));
	ATF_CHECK(!CHK_PORT(40003));
}

ATF_TC_WITHOUT_HEAD(nonnumeric);
ATF_TC_BODY(nonnumeric, tc)
{
	const char portspec[] = "foo";

	ATF_CHECK_EQ(EINVAL, parse_ports(portspec));
}

ATF_TC_WITHOUT_HEAD(null_range);
ATF_TC_BODY(null_range, tc)
{
	const char portspec[] = "22-22";

	ATF_REQUIRE_EQ(0, parse_ports(portspec));

	ATF_CHECK(!CHK_PORT(0));
	ATF_CHECK(CHK_PORT(22));
	ATF_CHECK(!CHK_PORT(23));
}

ATF_TC_WITHOUT_HEAD(range);
ATF_TC_BODY(range, tc)
{
	const char portspec[] = "22-25";

	ATF_REQUIRE_EQ(0, parse_ports(portspec));

	ATF_CHECK(!CHK_PORT(0));
	ATF_CHECK(CHK_PORT(22));
	ATF_CHECK(CHK_PORT(23));
	ATF_CHECK(CHK_PORT(24));
	ATF_CHECK(CHK_PORT(25));
	ATF_CHECK(!CHK_PORT(26));
}

ATF_TC_WITHOUT_HEAD(single);
ATF_TC_BODY(single, tc)
{
	const char portspec[] = "22";

	ATF_REQUIRE_EQ(0, parse_ports(portspec));

	ATF_CHECK(!CHK_PORT(0));
	ATF_CHECK(CHK_PORT(22));
}

ATF_TC_WITHOUT_HEAD(zero);
ATF_TC_BODY(zero, tc)
{
	const char portspec[] = "0";

	ATF_REQUIRE_EQ(0, parse_ports(portspec));

	ATF_CHECK(CHK_PORT(0));
}

ATF_TP_ADD_TCS(tp)
{
	ATF_TP_ADD_TC(tp, backwards_range);
	ATF_TP_ADD_TC(tp, erange_low);
	ATF_TP_ADD_TC(tp, erange_high);
	ATF_TP_ADD_TC(tp, erange_on_range_end);
	ATF_TP_ADD_TC(tp, multiple);
	ATF_TP_ADD_TC(tp, multiple_plus_ranges);
	ATF_TP_ADD_TC(tp, nonnumeric);
	ATF_TP_ADD_TC(tp, null_range);
	ATF_TP_ADD_TC(tp, range);
	ATF_TP_ADD_TC(tp, single);
	ATF_TP_ADD_TC(tp, zero);

	return (atf_no_error());
}
