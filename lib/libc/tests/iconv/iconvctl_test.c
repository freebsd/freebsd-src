/*-
 * Copyright (c) 2016 Eric van Gyzen
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

#include <iconv.h>

#include <atf-c.h>

static void
test_trivialp(const char *src, const char *dst, int expected)
{
	iconv_t ic;
	int actual, status;

	ic = iconv_open(dst, src);
	ATF_REQUIRE(ic != (iconv_t)-1);

	status = iconvctl(ic, ICONV_TRIVIALP, &actual);
	ATF_REQUIRE(status == 0);

	ATF_REQUIRE(actual == expected);

	status = iconv_close(ic);
	ATF_REQUIRE(status == 0);
}

ATF_TC_WITHOUT_HEAD(iconvctl_trivialp_test);
ATF_TC_BODY(iconvctl_trivialp_test, tc)
{

	test_trivialp("ISO-8859-1",  "ISO-8859-1",  1);
	test_trivialp("ISO-8859-1",  "ISO-8859-15", 0);
	test_trivialp("ISO-8859-15", "ISO-8859-1",  0);
	test_trivialp("ISO-8859-15", "UTF-8",       0);
	test_trivialp("UTF-8",       "ASCII",       0);
}

ATF_TP_ADD_TCS(tp)
{

	ATF_TP_ADD_TC(tp, iconvctl_trivialp_test);

	return (atf_no_error());
}
