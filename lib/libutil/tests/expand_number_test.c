/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2023 Google LLC
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

#include <sys/cdefs.h>
#include <errno.h>
#include <libutil.h>

#include <atf-c.h>

ATF_TC_WITHOUT_HEAD(positivetests);
ATF_TC_BODY(positivetests, tc)
{
	int retval;
	uint64_t num;

#define positive_tc(string, value) 							\
	do {										\
		ATF_CHECK_ERRNO(0, (retval = expand_number((string), &num)) == 0);	\
		ATF_CHECK_EQ(retval, 0);						\
		ATF_CHECK_EQ(num, (value));						\
	} while (0)

	positive_tc("123456", 123456);
	positive_tc("123456b", 123456);
	positive_tc("1k", 1024);
	positive_tc("1kb", 1024);
	positive_tc("1K", 1024);
	positive_tc("1KB", 1024);
	positive_tc("1m", 1048576);
	positive_tc("1M", 1048576);
	positive_tc("1g", 1073741824);
	positive_tc("1G", 1073741824);
	positive_tc("1t", 1099511627776);
	positive_tc("1T", 1099511627776);
	positive_tc("1p", 1125899906842624);
	positive_tc("1P", 1125899906842624);
	positive_tc("1e", 1152921504606846976);
	positive_tc("1E", 1152921504606846976);
	positive_tc("15E", 17293822569102704640ULL);
}

ATF_TC_WITHOUT_HEAD(negativetests);
ATF_TC_BODY(negativetests, tc)
{
	uint64_t num;

	ATF_CHECK_ERRNO(EINVAL, expand_number("", &num));
	ATF_CHECK_ERRNO(EINVAL, expand_number("x", &num));
	ATF_CHECK_ERRNO(EINVAL, expand_number("1bb", &num));
	ATF_CHECK_ERRNO(EINVAL, expand_number("1x", &num));
	ATF_CHECK_ERRNO(EINVAL, expand_number("1kx", &num));
	ATF_CHECK_ERRNO(ERANGE, expand_number("16E", &num));
}

ATF_TP_ADD_TCS(tp)
{
	ATF_TP_ADD_TC(tp, positivetests);
	ATF_TP_ADD_TC(tp, negativetests);
	return (atf_no_error());
}
