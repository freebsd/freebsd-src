/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2019 John Baldwin <jhb@FreeBSD.org>
 * Copyright (c) 2025 Dag-Erling Smørgrav <des@FreeBSD.org>
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

#include <atf-c.h>
#include <errno.h>
#include <libutil.h>
#include <stdint.h>

static void
require_success(const char *str, int64_t exp_val)
{
	int64_t val;

	ATF_REQUIRE_MSG(expand_number(str, &val) == 0,
	    "Failed to parse '%s': %m", str);
	ATF_REQUIRE_MSG(val == exp_val,
	    "String '%s' parsed as %jd instead of expected %jd", str,
	    (intmax_t)val, (intmax_t)exp_val);
}

static void
require_error(const char *str, int exp_errno)
{
	int64_t val;

	ATF_REQUIRE_MSG(expand_number(str, &val) == -1,
	    "String '%s' parsed as %jd instead of error", str, (intmax_t)val);
	ATF_REQUIRE_MSG(errno == exp_errno,
	    "String '%s' failed with %d instead of expected %d", str, errno,
	    exp_errno);
}

ATF_TC_WITHOUT_HEAD(expand_number__ok);
ATF_TC_BODY(expand_number__ok, tp)
{
	/* Bare numbers. */
	require_success("-0", 0);
	require_success(" 0", 0);
	require_success("+0", 0);
	require_success("-1", -1);
	require_success(" 1", 1);
	require_success("+1", 1);
	require_success("-10", -10);
	require_success(" 10", 10);
	require_success("+10", 10);

	/* Uppercase suffixes. */
	require_success("1B", 1);
	require_success("1K", 1LL << 10);
	require_success("1M", 1LL << 20);
	require_success("1G", 1LL << 30);
	require_success("1T", 1LL << 40);
	require_success("1P", 1LL << 50);
	require_success("1E", 1LL << 60);

	/* Lowercase suffixes. */
	require_success("2b", 2);
	require_success("2k", 2LL << 10);
	require_success("2m", 2LL << 20);
	require_success("2g", 2LL << 30);
	require_success("2t", 2LL << 40);
	require_success("2p", 2LL << 50);
	require_success("2e", 2LL << 60);

	/* Suffixes with a trailing 'b'. */
	require_success("3KB", 3LL << 10);
	require_success("3MB", 3LL << 20);
	require_success("3GB", 3LL << 30);
	require_success("3TB", 3LL << 40);
	require_success("3PB", 3LL << 50);
	require_success("3EB", 3LL << 60);

	/* Negative numbers. */
	require_success("-1", -1);
	require_success("-10", -10);
	require_success("-1B", -1);
	require_success("-1K", -(1LL << 10));
	require_success("-1M", -(1LL << 20));
	require_success("-1G", -(1LL << 30));
	require_success("-1T", -(1LL << 40));
	require_success("-1P", -(1LL << 50));
	require_success("-1E", -(1LL << 60));
	require_success("-2b", -2);
	require_success("-2k", -(2LL << 10));
	require_success("-2m", -(2LL << 20));
	require_success("-2g", -(2LL << 30));
	require_success("-2t", -(2LL << 40));
	require_success("-2p", -(2LL << 50));
	require_success("-2e", -(2LL << 60));
	require_success("-3KB", -(3LL << 10));
	require_success("-3MB", -(3LL << 20));
	require_success("-3GB", -(3LL << 30));
	require_success("-3TB", -(3LL << 40));
	require_success("-3PB", -(3LL << 50));
	require_success("-3EB", -(3LL << 60));

	/* Maximum values. */
	require_success("7E", 7LL << 60);
	require_success("8191P", 8191LL << 50);
	require_success("8388607T", 8388607LL << 40);
	require_success("8589934591G", 8589934591LL << 30);
	require_success("8796093022207M", 8796093022207LL << 20);
	require_success("9007199254740991K", 9007199254740991LL << 10);
	require_success("9223372036854775807", INT64_MAX);

	/* Minimum values. */
	require_success("-7E", -(7LL << 60));
	require_success("-8191P", -(8191LL << 50));
	require_success("-8388607T", -(8388607LL << 40));
	require_success("-8589934591G", -(8589934591LL << 30));
	require_success("-8796093022207M", -(8796093022207LL << 20));
	require_success("-9007199254740991K", -(9007199254740991LL << 10));
	require_success("-9223372036854775808", INT64_MIN);
}

ATF_TC_WITHOUT_HEAD(expand_number__bad);
ATF_TC_BODY(expand_number__bad, tp)
{
	/* No digits. */
	require_error("", EINVAL);
	require_error("b", EINVAL);
	require_error("k", EINVAL);
	require_error("m", EINVAL);
	require_error("g", EINVAL);
	require_error("t", EINVAL);
	require_error("p", EINVAL);
	require_error("e", EINVAL);
	require_error("-", EINVAL);
	require_error("-b", EINVAL);
	require_error("-k", EINVAL);
	require_error("-m", EINVAL);
	require_error("-g", EINVAL);
	require_error("-t", EINVAL);
	require_error("-p", EINVAL);
	require_error("-e", EINVAL);

	require_error("not_a_number", EINVAL);

	/* Invalid suffixes. */
	require_error("1a", EINVAL);
	require_error("1c", EINVAL);
	require_error("1d", EINVAL);
	require_error("1f", EINVAL);
	require_error("1h", EINVAL);
	require_error("1i", EINVAL);
	require_error("1j", EINVAL);
	require_error("1l", EINVAL);
	require_error("1n", EINVAL);
	require_error("1o", EINVAL);
	require_error("1q", EINVAL);
	require_error("1r", EINVAL);
	require_error("1s", EINVAL);
	require_error("1u", EINVAL);
	require_error("1v", EINVAL);
	require_error("1w", EINVAL);
	require_error("1x", EINVAL);
	require_error("1y", EINVAL);
	require_error("1z", EINVAL);

	/* Trailing garbage. */
	require_error("1K foo", EINVAL);
	require_error("1Mfoo", EINVAL);

	/* Overflow. */
	require_error("8E", ERANGE);
	require_error("8192P", ERANGE);
	require_error("8388608T", ERANGE);
	require_error("8589934592G", ERANGE);
	require_error("8796093022208M", ERANGE);
	require_error("9007199254740992K", ERANGE);
	require_error("9223372036854775808", ERANGE);

	/* Multiple signs */
	require_error("--1", EINVAL);
	require_error("-+1", EINVAL);
	require_error("+-1", EINVAL);
	require_error("++1", EINVAL);

	/* Whitespace after the sign */
	require_error(" - 1", EINVAL);
	require_error(" + 1", EINVAL);
}

ATF_TC_WITHOUT_HEAD(expand_unsigned);
ATF_TC_BODY(expand_unsigned, tp)
{
	static struct tc {
		const char *str;
		uint64_t num;
		int error;
	} tcs[] = {
		{ "0", 0, 0 },
		{ "+0", 0, 0 },
		{ "-0", 0, 0 },
		{ "1", 1, 0 },
		{ "+1", 1, 0 },
		{ "-1", 0, ERANGE },
		{ "18446744073709551615", UINT64_MAX, 0 },
		{ "+18446744073709551615", UINT64_MAX, 0 },
		{ "-18446744073709551615", 0, ERANGE },
		{ 0 },
	};
	struct tc *tc;
	uint64_t num;
	int error, ret;

	for (tc = tcs; tc->str != NULL; tc++) {
		ret = expand_number(tc->str, &num);
		error = errno;
		if (tc->error == 0) {
			ATF_REQUIRE_EQ_MSG(0, ret,
			    "%s ret = %d", tc->str, ret);
			ATF_REQUIRE_EQ_MSG(tc->num, num,
			    "%s num = %ju", tc->str, (uintmax_t)num);
		} else {
			ATF_REQUIRE_EQ_MSG(-1, ret,
			    "%s ret = %d", tc->str, ret);
			ATF_REQUIRE_EQ_MSG(tc->error, error,
			    "%s errno = %d", tc->str, error);
		}
	}
}

ATF_TC_WITHOUT_HEAD(expand_generic);
ATF_TC_BODY(expand_generic, tp)
{
	uint64_t uint64;
	int64_t int64;
#ifdef __LP64__
	size_t size;
#endif
	off_t off;

	ATF_REQUIRE_EQ(0, expand_number("18446744073709551615", &uint64));
	ATF_REQUIRE_EQ(UINT64_MAX, uint64);
	ATF_REQUIRE_EQ(-1, expand_number("-1", &uint64));
	ATF_REQUIRE_EQ(ERANGE, errno);

	ATF_REQUIRE_EQ(0, expand_number("9223372036854775807", &int64));
	ATF_REQUIRE_EQ(INT64_MAX, int64);
	ATF_REQUIRE_EQ(-1, expand_number("9223372036854775808", &int64));
	ATF_REQUIRE_EQ(ERANGE, errno);
	ATF_REQUIRE_EQ(0, expand_number("-9223372036854775808", &int64));
	ATF_REQUIRE_EQ(INT64_MIN, int64);

#ifdef __LP64__
	ATF_REQUIRE_EQ(0, expand_number("18446744073709551615", &size));
	ATF_REQUIRE_EQ(UINT64_MAX, size);
	ATF_REQUIRE_EQ(-1, expand_number("-1", &size));
	ATF_REQUIRE_EQ(ERANGE, errno);
#endif

	ATF_REQUIRE_EQ(0, expand_number("9223372036854775807", &off));
	ATF_REQUIRE_EQ(INT64_MAX, off);
	ATF_REQUIRE_EQ(-1, expand_number("9223372036854775808", &off));
	ATF_REQUIRE_EQ(ERANGE, errno);
	ATF_REQUIRE_EQ(0, expand_number("-9223372036854775808", &off));
	ATF_REQUIRE_EQ(INT64_MIN, off);
}

ATF_TP_ADD_TCS(tp)
{
	ATF_TP_ADD_TC(tp, expand_number__ok);
	ATF_TP_ADD_TC(tp, expand_number__bad);
	ATF_TP_ADD_TC(tp, expand_unsigned);
	ATF_TP_ADD_TC(tp, expand_generic);

	return (atf_no_error());
}
