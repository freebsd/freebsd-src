/*-
* SPDX-License-Identifier: BSD-2-Clause
* Copyright (c) 2022 Aymeric Wibo <obiwac@gmail.com>
*/

#include <atf-c.h>
#include <string.h>

static void
check_all(size_t len, const char *ordered[len])
{
	const char *a, *b;

	for (size_t i = 0; i < len; i++) {
		for (size_t j = 0; j < len; j++) {
			a = ordered[i];
			b = ordered[j];

			if (i == j)
				ATF_CHECK_MSG(
				    strverscmp(a, b) == 0,
				    "strverscmp(\"%s\", \"%s\") == 0",
				    a, b
				);
			else if (i < j)
				ATF_CHECK_MSG(
				    strverscmp(a, b) < 0,
				    "strverscmp(\"%s\", \"%s\") < 0",
				    a, b
				);
			else if (i > j)
				ATF_CHECK_MSG(
				    strverscmp(a, b) > 0,
				    "strverscmp(\"%s\", \"%s\") > 0",
				    a, b
				);
		}
	}
}

#define	CHECK_ALL(...) do {                                     \
	const char *ordered[] = { __VA_ARGS__ };                \
	check_all(sizeof(ordered) / sizeof(*ordered), ordered); \
} while (0)

ATF_TC_WITHOUT_HEAD(strcmp_functionality);
ATF_TC_BODY(strcmp_functionality, tc)
{
	CHECK_ALL("", "a", "b");
}

/* from Linux man page strverscmp(3) */

ATF_TC_WITHOUT_HEAD(vers_ordering);
ATF_TC_BODY(vers_ordering, tc)
{
	CHECK_ALL("000", "00", "01", "010", "09", "0", "1", "9", "10");
}

ATF_TC_WITHOUT_HEAD(natural_ordering);
ATF_TC_BODY(natural_ordering, tc)
{
	CHECK_ALL("jan1", "jan2", "jan9", "jan10", "jan11", "jan19", "jan20");
}

/* https://sourceware.org/bugzilla/show_bug.cgi?id=9913 */

ATF_TC_WITHOUT_HEAD(glibc_bug_9913);
ATF_TC_BODY(glibc_bug_9913, tc)
{
	CHECK_ALL(
	    "B0075022800016.gbp.corp.com",
	    "B007502280067.gbp.corp.com",
	    "B007502357019.GBP.CORP.COM"
	);
}

ATF_TC_WITHOUT_HEAD(semver_ordering);
ATF_TC_BODY(semver_ordering, tc)
{
	CHECK_ALL("2.6.20", "2.6.21");
}

ATF_TP_ADD_TCS(tp)
{
	ATF_TP_ADD_TC(tp, strcmp_functionality);
	ATF_TP_ADD_TC(tp, vers_ordering);
	ATF_TP_ADD_TC(tp, natural_ordering);
	ATF_TP_ADD_TC(tp, glibc_bug_9913);
	ATF_TP_ADD_TC(tp, semver_ordering);

	return (atf_no_error());
}
