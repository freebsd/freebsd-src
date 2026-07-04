/*
 * Copyright (c) 2026 Faraz Vahedi <kfv@kfv.io>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

/*
 * Tests for strfromd(3), strfromf(3), and strfroml(3).
 *
 * C23 §7.24.1.3 states that these functions are equivalent to snprintf(3),
 * albeit with some subtle exceptions, and therefore snprintf(3) is used as
 * the correctness oracle.  Hardcoded string checks guard against both
 * producing the same wrong answer.
 */

#include <float.h>
#include <locale.h>
#include <math.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <atf-c.h>

/*
 * Buffer large enough for any finite value in %f form, including LDBL_MAX on
 * platforms with 80-bit or 128-bit long double.  Sizing for the full output
 * keeps the snprintf(3) oracle a full-length comparison rather than letting
 * both sides truncate alike.
 */
#define	FBUF	8192

static void
check_d(const char *fmt, double val)
{
	char got[FBUF], ref[FBUF];
	int rlen, glen;

	rlen = snprintf(ref, sizeof(ref), fmt, val);
	glen = strfromd(got, sizeof(got), fmt, val);
	ATF_CHECK_MSG(glen == rlen,
	    "strfromd(\"%s\", %g): return %d, want %d", fmt, val, glen, rlen);
	ATF_CHECK_STREQ_MSG(ref, got, "strfromd(\"%s\", %g)", fmt, val);
}

static void
check_f(const char *fmt, float val)
{
	char got[FBUF], ref[FBUF];
	int rlen, glen;

	rlen = snprintf(ref, sizeof(ref), fmt, (double)val);
	glen = strfromf(got, sizeof(got), fmt, val);
	ATF_CHECK_MSG(glen == rlen,
	    "strfromf(\"%s\", %g): return %d, want %d",
	    fmt, (double)val, glen, rlen);
	ATF_CHECK_STREQ_MSG(ref, got, "strfromf(\"%s\", %g)", fmt, (double)val);
}

static void
check_l(const char *fmt, long double val)
{
	char got[FBUF], ref[FBUF], lfmt[32];
	int rlen, glen;

	/*
	 * snprintf requires the 'L' length modifier for long double.
	 */
	snprintf(lfmt, sizeof(lfmt), "%.*sL%c",
	    (int)(strlen(fmt) - 1), fmt, fmt[strlen(fmt) - 1]);
	rlen = snprintf(ref, sizeof(ref), lfmt, val);
	glen = strfroml(got, sizeof(got), fmt, val);
	ATF_CHECK_MSG(glen == rlen,
	    "strfroml(\"%s\", %Lg): return %d, want %d", fmt, val, glen, rlen);
	ATF_CHECK_STREQ_MSG(ref, got, "strfroml(\"%s\", %Lg)", fmt, val);
}

ATF_TC_WITHOUT_HEAD(strfromd_f);
ATF_TC_BODY(strfromd_f, tc)
{
	char buf[FBUF];

	check_d("%f", 0.0);
	check_d("%f", 1.0);
	check_d("%f", -1.0);
	check_d("%f", 3.14159265358979);
	check_d("%f", 1.0 / 3.0);
	check_d("%f", 1e10);
	check_d("%f", 1e-10);
	check_d("%f", DBL_MAX);
	check_d("%f", DBL_MIN);
	check_d("%.0f", 3.14);
	check_d("%.0f", 3.5);
	check_d("%.0f", 4.5);
	check_d("%.2f", 3.14159);
	check_d("%.10f", 1.0 / 7.0);
	check_d("%.20f", 1.0);

	strfromd(buf, sizeof(buf), "%.6f", 0.0);
	ATF_CHECK_STREQ("0.000000", buf);
	strfromd(buf, sizeof(buf), "%.6f", -1.5);
	ATF_CHECK_STREQ("-1.500000", buf);
}

ATF_TC_WITHOUT_HEAD(strfromd_e);
ATF_TC_BODY(strfromd_e, tc)
{
	char buf[FBUF];

	check_d("%e", 0.0);
	check_d("%e", 1.0);
	check_d("%e", -1.0);
	check_d("%e", 3.14159265358979);
	check_d("%e", 1e100);
	check_d("%e", 1e-100);
	check_d("%E", 2.718281828);
	check_d("%.0e", 3.14);
	check_d("%.2e", 12345.6789);
	check_d("%.15e", 1.0 / 3.0);

	strfromd(buf, sizeof(buf), "%e", 1.0);
	ATF_CHECK_STREQ("1.000000e+00", buf);
	strfromd(buf, sizeof(buf), "%E", 1.0);
	ATF_CHECK_STREQ("1.000000E+00", buf);
}

ATF_TC_WITHOUT_HEAD(strfromd_g);
ATF_TC_BODY(strfromd_g, tc)
{
	char buf[FBUF];

	check_d("%g", 0.0);
	check_d("%g", 1.0);
	check_d("%g", 100000.0);
	check_d("%g", 1000000.0);        /* exponent >= prec:   hence %e */
	check_d("%g", 0.0001);           /* exponent == -4:     hence %f */
	check_d("%g", 0.00001);          /* exponent < -4:      hence %e */
	check_d("%g", 3.14159265358979);
	check_d("%G", 1.23e10);
	check_d("%.2g", 3.14159);
	check_d("%.0g", 3.14159);
	check_d("%.10g", 1.0 / 7.0);
	check_d("%g", DBL_MIN);
	check_d("%g", DBL_MAX);

	strfromd(buf, sizeof(buf), "%g", 1.0);
	ATF_CHECK_STREQ("1", buf);
	strfromd(buf, sizeof(buf), "%g", 100.0);
	ATF_CHECK_STREQ("100", buf);
}

ATF_TC_WITHOUT_HEAD(strfromd_a);
ATF_TC_BODY(strfromd_a, tc)
{
	char buf[FBUF];

	check_d("%a", 0.0);
	check_d("%a", 1.0);
	check_d("%a", -1.0);
	check_d("%a", 0.5);
	check_d("%a", 1.5);
	check_d("%a", DBL_MIN);
	check_d("%a", DBL_MAX);
	check_d("%A", 1.0);
	check_d("%.4a", 1.0 / 3.0);
	check_d("%.0a", 1.5);

	strfromd(buf, sizeof(buf), "%a", 1.0);
	ATF_CHECK_STREQ("0x1p+0", buf);
	strfromd(buf, sizeof(buf), "%a", 0.5);
	ATF_CHECK_STREQ("0x1p-1", buf);
	strfromd(buf, sizeof(buf), "%A", 1.0);
	ATF_CHECK_STREQ("0X1P+0", buf);
}

ATF_TC_WITHOUT_HEAD(strfromd_special);
ATF_TC_BODY(strfromd_special, tc)
{
	char buf[FBUF];

	check_d("%f", INFINITY);
	check_d("%f", -INFINITY);
	check_d("%f", NAN);
	check_d("%e", INFINITY);
	check_d("%e", -INFINITY);
	check_d("%e", NAN);
	check_d("%g", INFINITY);
	check_d("%g", -INFINITY);
	check_d("%g", NAN);
	check_d("%a", INFINITY);
	check_d("%a", -INFINITY);
	check_d("%a", NAN);
	check_d("%F", INFINITY);
	check_d("%E", INFINITY);
	check_d("%G", INFINITY);
	check_d("%A", INFINITY);
	check_d("%f", -0.0);
	check_d("%g", -0.0);
	check_d("%e", -0.0);
	check_d("%a", -0.0);

	strfromd(buf, sizeof(buf), "%f", INFINITY);
	ATF_CHECK_STREQ("inf", buf);
	strfromd(buf, sizeof(buf), "%F", INFINITY);
	ATF_CHECK_STREQ("INF", buf);
	strfromd(buf, sizeof(buf), "%f", -INFINITY);
	ATF_CHECK_STREQ("-inf", buf);
	strfromd(buf, sizeof(buf), "%f", NAN);
	ATF_CHECK_STREQ("nan", buf);
	strfromd(buf, sizeof(buf), "%F", NAN);
	ATF_CHECK_STREQ("NAN", buf);
}

ATF_TC_WITHOUT_HEAD(strfromd_truncation);
ATF_TC_BODY(strfromd_truncation, tc)
{
	char buf[FBUF];
	int ret;

	/*
	 * When n == 0 the buffer must not be touched and the return value
	 * must still reflect the full output length.
	 */
	buf[0] = 'X';
	ret = strfromd(buf, 0, "%f", 1.0);
	ATF_CHECK(ret > 0);
	ATF_CHECK_MSG(buf[0] == 'X', "buffer modified with n==0");

	/*
	 * With a short buffer, a null terminated prefix is written and the
	 * return value is the length that would have been needed.
	 */
	ret = strfromd(buf, 5, "%.6f", 1.0);
	ATF_CHECK_MSG(ret == 8, "return %d, want 8", ret);
	ATF_CHECK_MSG(buf[4] == '\0', "not null terminated at buf[4]");
	ATF_CHECK_MSG(strncmp(buf, "1.00", 4) == 0,
	    "prefix mismatch: got \"%.*s\"", 4, buf);
}

ATF_TC_WITHOUT_HEAD(strfromf_f);
ATF_TC_BODY(strfromf_f, tc)
{
	check_f("%f", 0.0f);
	check_f("%f", 1.0f);
	check_f("%f", -1.0f);
	check_f("%f", 3.14159f);
	check_f("%.0f", 3.5f);
	check_f("%.2f", 3.14159f);
	check_f("%.9f", 1.0f / 3.0f);
	check_f("%f", FLT_MAX);
	check_f("%f", FLT_MIN);
}

ATF_TC_WITHOUT_HEAD(strfromf_e);
ATF_TC_BODY(strfromf_e, tc)
{
	char buf[FBUF];

	check_f("%e", 0.0f);
	check_f("%e", 1.0f);
	check_f("%e", -1.0f);
	check_f("%e", 3.14159f);
	check_f("%E", 2.71828f);
	check_f("%.2e", 12345.6f);
	check_f("%.0e", 3.14f);

	strfromf(buf, sizeof(buf), "%e", 1.0f);
	ATF_CHECK_STREQ("1.000000e+00", buf);
}

ATF_TC_WITHOUT_HEAD(strfromf_g);
ATF_TC_BODY(strfromf_g, tc)
{
	char buf[FBUF];

	check_f("%g", 0.0f);
	check_f("%g", 1.0f);
	check_f("%g", 100000.0f);
	check_f("%g", 1000000.0f);
	check_f("%g", 0.0001f);
	check_f("%g", 0.00001f);
	check_f("%G", 1.23e10f);
	check_f("%.2g", 3.14159f);

	strfromf(buf, sizeof(buf), "%g", 1.0f);
	ATF_CHECK_STREQ("1", buf);
}

ATF_TC_WITHOUT_HEAD(strfromf_a);
ATF_TC_BODY(strfromf_a, tc)
{
	char buf[FBUF];

	check_f("%a", 0.0f);
	check_f("%a", 1.0f);
	check_f("%a", 0.5f);
	check_f("%a", -1.0f);
	check_f("%A", 1.0f);
	check_f("%.2a", 1.0f / 3.0f);

	strfromf(buf, sizeof(buf), "%a", 1.0f);
	ATF_CHECK_STREQ("0x1p+0", buf);
}

ATF_TC_WITHOUT_HEAD(strfromf_special);
ATF_TC_BODY(strfromf_special, tc)
{
	check_f("%f", INFINITY);
	check_f("%f", -INFINITY);
	check_f("%f", NAN);
	check_f("%e", INFINITY);
	check_f("%g", INFINITY);
	check_f("%a", INFINITY);
	check_f("%F", INFINITY);
	check_f("%f", -0.0f);
	check_f("%g", -0.0f);
}

ATF_TC_WITHOUT_HEAD(strfromf_truncation);
ATF_TC_BODY(strfromf_truncation, tc)
{
	char buf[FBUF];
	int ret;

	buf[0] = 'X';
	ret = strfromf(buf, 0, "%f", 1.0f);
	ATF_CHECK(ret > 0);
	ATF_CHECK_MSG(buf[0] == 'X', "buffer modified with n==0");

	ret = strfromf(buf, 5, "%.6f", 1.0f);
	ATF_CHECK_MSG(ret == 8, "return %d, want 8", ret);
	ATF_CHECK_MSG(buf[4] == '\0', "not null terminated at buf[4]");
}

ATF_TC_WITHOUT_HEAD(strfroml_f);
ATF_TC_BODY(strfroml_f, tc)
{
	check_l("%f", 0.0L);
	check_l("%f", 1.0L);
	check_l("%f", -1.0L);
	check_l("%f", 3.14159265358979323846L);
	check_l("%.0f", 3.5L);
	check_l("%.2f", 3.14159L);
	check_l("%.15f", 1.0L / 3.0L);
	check_l("%f", LDBL_MAX);
	check_l("%f", LDBL_MIN);
}

ATF_TC_WITHOUT_HEAD(strfroml_e);
ATF_TC_BODY(strfroml_e, tc)
{
	char buf[FBUF];

	check_l("%e", 0.0L);
	check_l("%e", 1.0L);
	check_l("%e", -1.0L);
	check_l("%e", 3.14159265358979323846L);
	check_l("%E", 2.71828182845904523536L);
	check_l("%.2e", 12345.6789L);
	check_l("%.0e", 3.14L);
	check_l("%e", LDBL_MAX);
	check_l("%e", LDBL_MIN);

	strfroml(buf, sizeof(buf), "%e", 1.0L);
	ATF_CHECK_STREQ("1.000000e+00", buf);
}

ATF_TC_WITHOUT_HEAD(strfroml_g);
ATF_TC_BODY(strfroml_g, tc)
{
	char buf[FBUF];

	check_l("%g", 0.0L);
	check_l("%g", 1.0L);
	check_l("%g", 100000.0L);
	check_l("%g", 1000000.0L);
	check_l("%g", 0.0001L);
	check_l("%g", 0.00001L);
	check_l("%G", 1.23e10L);
	check_l("%.2g", 3.14159L);
	check_l("%.0g", 3.14159L);

	strfroml(buf, sizeof(buf), "%g", 1.0L);
	ATF_CHECK_STREQ("1", buf);
}

ATF_TC_WITHOUT_HEAD(strfroml_a);
ATF_TC_BODY(strfroml_a, tc)
{
	char buf[FBUF];

	check_l("%a", 0.0L);
	check_l("%a", 1.0L);
	check_l("%a", 0.5L);
	check_l("%a", -1.0L);
	check_l("%A", 1.0L);
	check_l("%.4a", 1.0L / 3.0L);
	check_l("%.0a", 1.5L);
	check_l("%a", LDBL_MAX);
	check_l("%a", LDBL_MIN);

	strfroml(buf, sizeof(buf), "%a", 1.0L);
	ATF_CHECK_STREQ("0x1p+0", buf);
}

ATF_TC_WITHOUT_HEAD(strfroml_special);
ATF_TC_BODY(strfroml_special, tc)
{
	check_l("%f", INFINITY);
	check_l("%f", -INFINITY);
	check_l("%f", NAN);
	check_l("%e", INFINITY);
	check_l("%g", INFINITY);
	check_l("%a", INFINITY);
	check_l("%F", INFINITY);
	check_l("%f", -0.0L);
	check_l("%g", -0.0L);
#ifdef __HAVE_LONG_DOUBLE
	check_l("%.20f", 1.0L / 3.0L);
	check_l("%.20e", 1.0L / 7.0L);
#endif
}

ATF_TC_WITHOUT_HEAD(strfroml_truncation);
ATF_TC_BODY(strfroml_truncation, tc)
{
	char buf[FBUF];
	int ret;

	buf[0] = 'X';
	ret = strfroml(buf, 0, "%f", 1.0L);
	ATF_CHECK(ret > 0);
	ATF_CHECK_MSG(buf[0] == 'X', "buffer modified with n==0");

	ret = strfroml(buf, 5, "%.6f", 1.0L);
	ATF_CHECK_MSG(ret == 8, "return %d, want 8", ret);
	ATF_CHECK_MSG(buf[4] == '\0', "not null terminated at buf[4]");
}

ATF_TC_WITHOUT_HEAD(strfrom_locale);
ATF_TC_BODY(strfrom_locale, tc)
{
	char buf[FBUF];

	/*
	 * strfrom* is equivalent to snprintf(3) and so honours LC_NUMERIC:
	 * In a locale whose radix is not '.', the output must use that radix.
	 * The check_* helpers already compare against snprintf in this locale;
	 * the literal checks confirm the radix actually changed for both the
	 * decimal and hexadecimal paths.  Skip if the locale is unavailable.
	 */
	if (setlocale(LC_NUMERIC, "de_DE.UTF-8") == NULL)
		atf_tc_skip("de_DE.UTF-8 locale not available");

	check_d("%f", 1.5);
	check_d("%e", 1.5);
	check_d("%g", 1.5);
	check_d("%a", 1.5);
	check_f("%f", 1.5f);
	check_l("%f", 1.5L);

	strfromd(buf, sizeof(buf), "%.1f", 1.5);
	ATF_CHECK_STREQ("1,5", buf);
	strfromd(buf, sizeof(buf), "%a", 1.5);
	ATF_CHECK_STREQ("0x1,8p+0", buf);
}

ATF_TC_WITHOUT_HEAD(strfrom_empty_format);
ATF_TC_BODY(strfrom_empty_format, tc)
{
	char buf[FBUF];

	atf_tc_expect_signal(SIGABRT, "empty strfrom format aborts");
	strfromd(buf, sizeof(buf), "", 1.0);
}

ATF_TC_WITHOUT_HEAD(strfrom_bad_specifier);
ATF_TC_BODY(strfrom_bad_specifier, tc)
{
	char buf[FBUF];

	atf_tc_expect_signal(SIGABRT, "invalid strfrom specifier aborts");
	strfromd(buf, sizeof(buf), "%q", 1.0);
}

ATF_TP_ADD_TCS(tp)
{
	ATF_TP_ADD_TC(tp, strfromd_f);
	ATF_TP_ADD_TC(tp, strfromd_e);
	ATF_TP_ADD_TC(tp, strfromd_g);
	ATF_TP_ADD_TC(tp, strfromd_a);
	ATF_TP_ADD_TC(tp, strfromd_special);
	ATF_TP_ADD_TC(tp, strfromd_truncation);

	ATF_TP_ADD_TC(tp, strfromf_f);
	ATF_TP_ADD_TC(tp, strfromf_e);
	ATF_TP_ADD_TC(tp, strfromf_g);
	ATF_TP_ADD_TC(tp, strfromf_a);
	ATF_TP_ADD_TC(tp, strfromf_special);
	ATF_TP_ADD_TC(tp, strfromf_truncation);

	ATF_TP_ADD_TC(tp, strfroml_f);
	ATF_TP_ADD_TC(tp, strfroml_e);
	ATF_TP_ADD_TC(tp, strfroml_g);
	ATF_TP_ADD_TC(tp, strfroml_a);
	ATF_TP_ADD_TC(tp, strfroml_special);
	ATF_TP_ADD_TC(tp, strfroml_truncation);

	ATF_TP_ADD_TC(tp, strfrom_locale);
	ATF_TP_ADD_TC(tp, strfrom_empty_format);
	ATF_TP_ADD_TC(tp, strfrom_bad_specifier);

	return (atf_no_error());
}
