/*-
 * Copyright (c) 2024 Dag-Erling Smørgrav
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <time.h>

#include <atf-c.h>

ATF_TC_WITHOUT_HEAD(dayofweek);
ATF_TC_BODY(dayofweek, tc)
{
	static const struct {
		const char *str;
		int wday;
	} cases[] = {
		{ "1582-12-20", 1 },
		{ "1700-03-01", 1 },
		{ "1752-09-14", 4 },
		{ "1800-12-31", 3 },
		{ "1801-01-01", 4 },
		{ "1900-12-31", 1 },
		{ "1901-01-01", 2 },
		{ "2000-12-31", 0 },
		{ "2001-01-01", 1 },
		{ "2100-12-31", 5 },
		{ "2101-01-01", 6 },
		{ "2200-12-31", 3 },
		{ "2201-01-01", 4 },
		{ },
	};
	struct tm tm;

	for (unsigned int i = 0; cases[i].str != NULL; i++) {
		if (strptime(cases[i].str, "%Y-%m-%d", &tm) == NULL) {
			atf_tc_fail_nonfatal("failed to parse %s",
			    cases[i].str);
		} else if (tm.tm_wday != cases[i].wday) {
			atf_tc_fail_nonfatal("expected %d for %s, got %d",
			    cases[i].wday, cases[i].str, tm.tm_wday);
		}
	}
}

ATF_TP_ADD_TCS(tp)
{
	ATF_TP_ADD_TC(tp, dayofweek);
	return (atf_no_error());
}
