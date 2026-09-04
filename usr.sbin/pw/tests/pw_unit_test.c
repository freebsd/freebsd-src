/*
 * Copyright (c) 2026 Baptiste Daroussin <bapt@FreeBSD.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <sys/types.h>

#include <atf-c.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <sysexits.h>

#include "pw.h"
#include "pwupd.h"

struct pwconf conf;

/* stub */
struct userconf *
read_userconfig(char const *file __unused)
{

	return (NULL);
}

ATF_TC_WITHOUT_HEAD(checkfd_dash);
ATF_TC_BODY(checkfd_dash, tc)
{
	char dash[] = "-";

	ATF_CHECK_EQ(pw_checkfd(dash), _PWDASH);
}

ATF_TC_WITHOUT_HEAD(checkfd_zero);
ATF_TC_BODY(checkfd_zero, tc)
{
	char zero[] = "0";

	ATF_CHECK_EQ(pw_checkfd(zero), 0);
}

ATF_TC_WITHOUT_HEAD(checkfd_valid);
ATF_TC_BODY(checkfd_valid, tc)
{
	char five[] = "5";

	ATF_CHECK_EQ(pw_checkfd(five), 5);
}

ATF_TC_WITHOUT_HEAD(checkfd_max);
ATF_TC_BODY(checkfd_max, tc)
{
	char max[] = "2147483647";

	ATF_CHECK_EQ(pw_checkfd(max), INT_MAX);
}

static void
checkfd_invalid(const char *value, const char *errstr)
{
	pid_t pid;
	char buf[32];
	char experr[128];

	strlcpy(buf, value, sizeof(buf));
	snprintf(experr, sizeof(experr),
	    "pw_unit_test: Bad file descriptor '%s': %s\n", value, errstr);
	pid = atf_utils_fork();
	if (pid == 0) {
		pw_checkfd(buf);
		exit(1);
	}
	atf_utils_wait(pid, EX_USAGE, "", experr);
}

ATF_TC_WITHOUT_HEAD(checkfd_invalid);
ATF_TC_BODY(checkfd_invalid, tc)
{
	checkfd_invalid("abc", "invalid");
}

ATF_TC_WITHOUT_HEAD(checkfd_negative);
ATF_TC_BODY(checkfd_negative, tc)
{
	checkfd_invalid("-1", "too small");
}

ATF_TC_WITHOUT_HEAD(checkfd_overflow);
ATF_TC_BODY(checkfd_overflow, tc)
{
	checkfd_invalid("999999999999", "too large");
}

ATF_TP_ADD_TCS(tp)
{
	ATF_TP_ADD_TC(tp, checkfd_dash);
	ATF_TP_ADD_TC(tp, checkfd_zero);
	ATF_TP_ADD_TC(tp, checkfd_valid);
	ATF_TP_ADD_TC(tp, checkfd_max);
	ATF_TP_ADD_TC(tp, checkfd_invalid);
	ATF_TP_ADD_TC(tp, checkfd_negative);
	ATF_TP_ADD_TC(tp, checkfd_overflow);

	return (atf_no_error());
}
