/*-
 * Copyright (c) 2025 Klara, Inc.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include <atf-c.h>

ATF_TC_WITHOUT_HEAD(getenv_r_ok);
ATF_TC_BODY(getenv_r_ok, tc)
{
	const char *ident = atf_tc_get_ident(tc);
	char buf[256];

	ATF_REQUIRE_EQ(0, setenv("ATF_TC_IDENT", ident, 1));
	ATF_REQUIRE_EQ(0, getenv_r("ATF_TC_IDENT", buf, sizeof(buf)));
	ATF_REQUIRE_STREQ(ident, buf);
}

ATF_TC_WITHOUT_HEAD(getenv_r_einval);
ATF_TC_BODY(getenv_r_einval, tc)
{
	char buf[256];

	errno = 0;
	ATF_REQUIRE_EQ(-1, getenv_r(NULL, buf, sizeof(buf)));
	ATF_REQUIRE_EQ(EINVAL, errno);
	errno = 0;
	ATF_REQUIRE_EQ(-1, getenv_r("", buf, sizeof(buf)));
	ATF_REQUIRE_EQ(EINVAL, errno);
	errno = 0;
	ATF_REQUIRE_EQ(-1, getenv_r("A=B", buf, sizeof(buf)));
	ATF_REQUIRE_EQ(EINVAL, errno);
}

ATF_TC_WITHOUT_HEAD(getenv_r_enoent);
ATF_TC_BODY(getenv_r_enoent, tc)
{
	char buf[256];

	errno = 0;
	ATF_REQUIRE_EQ(-1, getenv_r("no such variable", buf, sizeof(buf)));
	ATF_REQUIRE_EQ(ENOENT, errno);
}

ATF_TC_WITHOUT_HEAD(getenv_r_erange);
ATF_TC_BODY(getenv_r_erange, tc)
{
	const char *ident = atf_tc_get_ident(tc);
	char buf[256];

	ATF_REQUIRE_EQ(0, setenv("ATF_TC_IDENT", ident, 1));
	errno = 0;
	ATF_REQUIRE_EQ(-1, getenv_r("ATF_TC_IDENT", buf, strlen(ident)));
	ATF_REQUIRE_EQ(ERANGE, errno);
}

ATF_TP_ADD_TCS(tp)
{
	ATF_TP_ADD_TC(tp, getenv_r_ok);
	ATF_TP_ADD_TC(tp, getenv_r_einval);
	ATF_TP_ADD_TC(tp, getenv_r_enoent);
	ATF_TP_ADD_TC(tp, getenv_r_erange);
	return (atf_no_error());
}
