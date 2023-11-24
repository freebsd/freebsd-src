/*-
 * Copyright (C) 2018 Conrad Meyer <cem@FreeBSD.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/param.h>

#include <locale.h>
#include <monetary.h>
#include <stdio.h>

#include <atf-c.h>

ATF_TC_WITHOUT_HEAD(strfmon_locale_thousands);
ATF_TC_BODY(strfmon_locale_thousands, tc)
{
	char actual[40], expected[40];
	struct lconv *lc;
	const char *ts;
	double n;

	setlocale(LC_MONETARY, "sv_SE.UTF-8");

	lc = localeconv();

	ts = lc->mon_thousands_sep;
	if (strlen(ts) == 0)
		ts = lc->thousands_sep;

	if (strlen(ts) < 2)
		atf_tc_skip("multi-byte thousands-separator not found");

	n = 1234.56;
	strfmon(actual, sizeof(actual) - 1, "%i", n);

	strcpy(expected, "1");
	strlcat(expected, ts, sizeof(expected));
	strlcat(expected, "234", sizeof(expected));

	/* We're just testing the thousands separator, not all of strfmon. */
	actual[strlen(expected)] = '\0';
	ATF_CHECK_STREQ(expected, actual);
}

ATF_TC_WITHOUT_HEAD(strfmon_examples);
ATF_TC_BODY(strfmon_examples, tc)
{
	const struct {
		const char *format;
		const char *expected;
	} tests[] = {
	    { "%n", "[$123.45] [-$123.45] [$3,456.78]" },
	    { "%11n", "[    $123.45] [   -$123.45] [  $3,456.78]" },
	    { "%#5n", "[ $   123.45] [-$   123.45] [ $ 3,456.78]" },
	    { "%=*#5n", "[ $***123.45] [-$***123.45] [ $*3,456.78]" },
	    { "%=0#5n", "[ $000123.45] [-$000123.45] [ $03,456.78]" },
	    { "%^#5n", "[ $  123.45] [-$  123.45] [ $ 3456.78]" },
	    { "%^#5.0n", "[ $  123] [-$  123] [ $ 3457]" },
	    { "%^#5.4n", "[ $  123.4500] [-$  123.4500] [ $ 3456.7810]" },
	    { "%(#5n", "[ $   123.45 ] [($   123.45)] [ $ 3,456.78 ]" },
	    { "%!(#5n", "[    123.45 ] [(   123.45)] [  3,456.78 ]" },
	    { "%-14#5.4n", "[ $   123.4500 ] [-$   123.4500 ] [ $ 3,456.7810 ]" },
	    { "%14#5.4n", "[  $   123.4500] [ -$   123.4500] [  $ 3,456.7810]" },
	};
	size_t i;
	char actual[100], format[50];

	if (setlocale(LC_MONETARY, "en_US.UTF-8") == NULL)
		atf_tc_skip("unable to setlocale()");

	for (i = 0; i < nitems(tests); ++i) {
		snprintf(format, sizeof(format), "[%s] [%s] [%s]",
		    tests[i].format, tests[i].format, tests[i].format);
		strfmon(actual, sizeof(actual) - 1, format,
		    123.45, -123.45, 3456.781);
		ATF_CHECK_STREQ_MSG(tests[i].expected, actual,
		    "[%s]", tests[i].format);
	}
}

ATF_TC(strfmon_cs_precedes_0);
ATF_TC_HEAD(strfmon_cs_precedes_0, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "sep_by_space x sign_posn when cs_precedes = 0");
}
ATF_TC_BODY(strfmon_cs_precedes_0, tc)
{
	const struct {
		const char *expected;
	} tests[] = {
	    /* sep_by_space x sign_posn */
	    { "[(123.00$)] [-123.00$] [123.00$-] [123.00-$] [123.00$-]" },
	    { "[(123.00 $)] [-123.00 $] [123.00 $-] [123.00 -$] [123.00 $-]" },
	    { "[(123.00$)] [- 123.00$] [123.00$ -] [123.00- $] [123.00$ -]" },
	};
	size_t i, j;
	struct lconv *lc;
	char actual[100], buf[100];

	if (setlocale(LC_MONETARY, "en_US.UTF-8") == NULL)
		atf_tc_skip("unable to setlocale()");

	lc = localeconv();
	lc->n_cs_precedes = 0;

	for (i = 0; i < nitems(tests); ++i) {
		actual[0] = '\0';
		lc->n_sep_by_space = i;

		for (j = 0; j < 5; ++j) {
			lc->n_sign_posn = j;

			strfmon(buf, sizeof(buf) - 1, "[%n] ", -123.0);
			strlcat(actual, buf, sizeof(actual));
		}

		actual[strlen(actual) - 1] = '\0';
		ATF_CHECK_STREQ_MSG(tests[i].expected, actual,
		    "sep_by_space = %zu", i);
	}
}

ATF_TC(strfmon_cs_precedes_1);
ATF_TC_HEAD(strfmon_cs_precedes_1, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "sep_by_space x sign_posn when cs_precedes = 1");
}
ATF_TC_BODY(strfmon_cs_precedes_1, tc)
{
	const struct {
		const char *expected;
	} tests[] = {
	    /* sep_by_space x sign_posn */
	    { "[($123.00)] [-$123.00] [$123.00-] [-$123.00] [$-123.00]" },
	    { "[($ 123.00)] [-$ 123.00] [$ 123.00-] [-$ 123.00] [$- 123.00]" },
	    { "[($123.00)] [- $123.00] [$123.00 -] [- $123.00] [$ -123.00]" },
	};
	size_t i, j;
	struct lconv *lc;
	char actual[100], buf[100];

	if (setlocale(LC_MONETARY, "en_US.UTF-8") == NULL)
		atf_tc_skip("unable to setlocale()");

	lc = localeconv();
	lc->n_cs_precedes = 1;

	for (i = 0; i < nitems(tests); ++i) {
		actual[0] = '\0';
		lc->n_sep_by_space = i;

		for (j = 0; j < 5; ++j) {
			lc->n_sign_posn = j;

			strfmon(buf, sizeof(buf) - 1, "[%n] ", -123.0);
			strlcat(actual, buf, sizeof(actual));
		}

		actual[strlen(actual) - 1] = '\0';
		ATF_CHECK_STREQ_MSG(tests[i].expected, actual,
		    "sep_by_space = %zu", i);
	}
}

ATF_TC_WITHOUT_HEAD(strfmon_international_currency_code);
ATF_TC_BODY(strfmon_international_currency_code, tc)
{
	const struct {
		const char *locale;
		const char *expected;
	} tests[] = {
	    { "en_US.UTF-8", "[USD123.45]" },
	    { "de_DE.UTF-8", "[123,45 EUR]" },
	    { "C", "[123.45]" },
	};
	size_t i;
	char actual[100];

	for (i = 0; i < nitems(tests); ++i) {
		if (setlocale(LC_MONETARY, tests[i].locale) == NULL)
			atf_tc_skip("unable to setlocale()");

		strfmon(actual, sizeof(actual) - 1, "[%i]", 123.45);
		ATF_CHECK_STREQ(tests[i].expected, actual);
	}
}

ATF_TC(strfmon_l);
ATF_TC_HEAD(strfmon_l, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "checks strfmon_l under different locales");
}
ATF_TC_BODY(strfmon_l, tc)
{
	const struct {
		const char *locale;
		const char *expected;
	} tests[] = {
	    { "C", "[ **1234.57 ] [ **1234.57 ]" },
	    { "de_DE.UTF-8", "[ **1234,57 €] [ **1.234,57 EUR]" },
	    { "en_GB.UTF-8", "[ £**1234.57] [ GBP**1,234.57]" },
	};
	locale_t loc;
	size_t i;
	char buf[100];

	for (i = 0; i < nitems(tests); ++i) {
		loc = newlocale(LC_MONETARY_MASK, tests[i].locale, NULL);
		ATF_REQUIRE(loc != NULL);

		strfmon_l(buf, sizeof(buf) - 1, loc, "[%^=*#6n] [%=*#6i]",
		    1234.567, 1234.567);
		ATF_REQUIRE_STREQ(tests[i].expected, buf);

		freelocale(loc);
	}
}

ATF_TP_ADD_TCS(tp)
{
	ATF_TP_ADD_TC(tp, strfmon_locale_thousands);
	ATF_TP_ADD_TC(tp, strfmon_examples);
	ATF_TP_ADD_TC(tp, strfmon_cs_precedes_0);
	ATF_TP_ADD_TC(tp, strfmon_cs_precedes_1);
	ATF_TP_ADD_TC(tp, strfmon_international_currency_code);
	ATF_TP_ADD_TC(tp, strfmon_l);
	return (atf_no_error());
}
