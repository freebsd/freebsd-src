/*-
* SPDX-License-Identifier: BSD-2-Clause
*
* Copyright (c) 2022 Obiwac,
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
* THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
* ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
* IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
* ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
* FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
* DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
* OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
* HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
* LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
* OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
* SUCH DAMAGE.
*/

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <string.h>
#include <atf-c.h>

static void check_all(int len, const char* ordered[len]) {
	for (size_t i = 0; i < len; i++) {
		for (size_t j = 0; j < len; j++) {
			const char* a = ordered[i];
			const char* b = ordered[j];

			if (i == j) {
				ATF_CHECK_MSG(
					strverscmp(a, b) == 0,
					"strverscmp(\"%s\", \"%s\") == 0",
					a, b);
			}

			else if (i < j) {
				ATF_CHECK_MSG(
					strverscmp(a, b) < 0,
					"strverscmp(\"%s\", \"%s\") < 0",
					a, b);
			}

			else if (i > j) {
				ATF_CHECK_MSG(
					strverscmp(a, b) > 0,
					"strverscmp(\"%s\", \"%s\") > 0",
					a, b);
			}
		}
	}
}

#define CHECK_ALL(...) \
	const char* ordered[] = { __VA_ARGS__ }; \
	check_all(sizeof(ordered) / sizeof(*ordered), ordered);

ATF_TC_WITHOUT_HEAD(strcmp_functionality);
ATF_TC_BODY(strcmp_functionality, tc) {
	CHECK_ALL("", "a", "b")
}

// from Linux man page

ATF_TC_WITHOUT_HEAD(vers_ordering);
ATF_TC_BODY(vers_ordering, tc) {
	CHECK_ALL("000", "00", "01", "010", "09", "0", "1", "9", "10")
}

ATF_TC_WITHOUT_HEAD(natural_ordering);
ATF_TC_BODY(natural_ordering, tc) {
	CHECK_ALL("jan1", "jan2", "jan9", "jan10", "jan11", "jan19", "jan20")
}

// https://sourceware.org/bugzilla/show_bug.cgi?id=9913

ATF_TC_WITHOUT_HEAD(glibc_bug_9913);
ATF_TC_BODY(glibc_bug_9913, tc) {
	CHECK_ALL(
		"B0075022800016.gbp.corp.com",
		"B007502280067.gbp.corp.com",
		"B007502357019.GBP.CORP.COM"
	)
}

ATF_TC_WITHOUT_HEAD(semver_ordering);
ATF_TC_BODY(semver_ordering, tc) {
	CHECK_ALL("2.6.20", "2.6.21")
}

ATF_TP_ADD_TCS(tp) {
	ATF_TP_ADD_TC(tp, strcmp_functionality);
	ATF_TP_ADD_TC(tp, vers_ordering);
	ATF_TP_ADD_TC(tp, natural_ordering);
	ATF_TP_ADD_TC(tp, glibc_bug_9913);
	ATF_TP_ADD_TC(tp, semver_ordering);

	return atf_no_error();
}
