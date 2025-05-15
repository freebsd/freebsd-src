/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2025 Ricardo Branco <rbranco@suse.de>
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

#include <ctype.h>
#include <errno.h>
#include <signal.h>
#include <stdio.h>

#include <atf-c.h>

static void
test_roundtrip(int signum)
{
	char str[SIG2STR_MAX];
	int sig;

	ATF_REQUIRE(sig2str(signum, str) == 0);
	ATF_REQUIRE(str2sig(str, &sig) == 0);
	ATF_REQUIRE_EQ(sig, signum);
}

ATF_TC_WITHOUT_HEAD(sig2str_valid);
ATF_TC_BODY(sig2str_valid, tc)
{
	int sig;

	for (sig = 1; sig < sys_nsig; sig++) {
		test_roundtrip(sig);
	}
}

ATF_TC_WITHOUT_HEAD(sig2str_invalid);
ATF_TC_BODY(sig2str_invalid, tc)
{
	char buf[SIG2STR_MAX];

	ATF_REQUIRE(sig2str(0, buf) != 0);
	ATF_REQUIRE(sig2str(-1, buf) != 0);
	ATF_REQUIRE(sig2str(SIGRTMAX + 1, buf) != 0);
}

ATF_TC_WITHOUT_HEAD(str2sig_rtmin_rtmax);
ATF_TC_BODY(str2sig_rtmin_rtmax, tc)
{
	int sig;

	ATF_REQUIRE(str2sig("RTMIN", &sig) == 0);
	ATF_REQUIRE_EQ(sig, SIGRTMIN);

	ATF_REQUIRE(str2sig("RTMAX", &sig) == 0);
	ATF_REQUIRE_EQ(sig, SIGRTMAX);

	ATF_REQUIRE(str2sig("RTMIN+1", &sig) == 0);
	ATF_REQUIRE_EQ(sig, SIGRTMIN + 1);

	ATF_REQUIRE(str2sig("RTMAX-1", &sig) == 0);
	ATF_REQUIRE_EQ(sig, SIGRTMAX - 1);
}

ATF_TC_WITHOUT_HEAD(str2sig_invalid_rt);
ATF_TC_BODY(str2sig_invalid_rt, tc)
{
	int sig;

	ATF_REQUIRE(str2sig("RTMIN+0", &sig) != 0);
	ATF_REQUIRE(str2sig("RTMAX-0", &sig) != 0);
	ATF_REQUIRE(str2sig("RTMIN-777", &sig) != 0);
	ATF_REQUIRE(str2sig("RTMIN+777", &sig) != 0);
	ATF_REQUIRE(str2sig("RTMAX-777", &sig) != 0);
	ATF_REQUIRE(str2sig("RTMAX+777", &sig) != 0);
	ATF_REQUIRE(str2sig("RTMIN-", &sig) != 0);
	ATF_REQUIRE(str2sig("RTMAX-", &sig) != 0);
	ATF_REQUIRE(str2sig("RTMIN0", &sig) != 0);
	ATF_REQUIRE(str2sig("RTMAX1", &sig) != 0);
	ATF_REQUIRE(str2sig("RTMIN+abc", &sig) != 0);
	ATF_REQUIRE(str2sig("RTMIN-abc", &sig) != 0);
}

ATF_TC_WITHOUT_HEAD(str2sig_fullname);
ATF_TC_BODY(str2sig_fullname, tc)
{
	char fullname[SIG2STR_MAX + 3];
	int n, sig;

	for (sig = 1; sig < sys_nsig; sig++) {
		snprintf(fullname, sizeof(fullname), "SIG%s", sys_signame[sig]);

		ATF_REQUIRE_MSG(str2sig(fullname, &n) == 0,
		    "str2sig(\"%s\") failed", fullname);

		ATF_REQUIRE_MSG(n == sig,
		    "Mismatch: %s = %d, %s = %d",
		    sys_signame[sig], sig, fullname, n);
	}
}

ATF_TC_WITHOUT_HEAD(str2sig_lowercase);
ATF_TC_BODY(str2sig_lowercase, tc)
{
	char fullname[SIG2STR_MAX + 3];
	int n, sig;

	for (sig = 1; sig < sys_nsig; sig++) {
		snprintf(fullname, sizeof(fullname), "sig%s", sys_signame[sig]);
		for (size_t i = 3; i < strlen(fullname); i++)
			fullname[i] = toupper(fullname[i]);

		ATF_REQUIRE_MSG(str2sig(fullname, &n) == 0,
		    "str2sig(\"%s\") failed", fullname);

		ATF_REQUIRE_MSG(n == sig,
		    "Mismatch: %s = %d, %s = %d",
		    sys_signame[sig], sig, fullname, n);
	}
}

ATF_TC_WITHOUT_HEAD(str2sig_numeric);
ATF_TC_BODY(str2sig_numeric, tc)
{
	char buf[16];
	int sig, n;

	for (sig = NSIG; sig < SIGRTMIN; sig++) {
		snprintf(buf, sizeof(buf), "%d", sig);
		ATF_REQUIRE_MSG(str2sig(buf, &n) == 0,
		    "str2sig(\"%s\") failed", buf);
		ATF_REQUIRE_EQ(n, sig);
	}
}

ATF_TC_WITHOUT_HEAD(str2sig_invalid);
ATF_TC_BODY(str2sig_invalid, tc)
{
	int sig;

	const char *invalid[] = {
		"SIGDOESNOTEXIST",
		"DOESNOTEXIST",
		"INTERRUPT",
		"",
		"SIG",
		"123abc",
		"sig1extra",
		NULL
	};

	for (int i = 0; invalid[i] != NULL; i++) {
		ATF_REQUIRE_MSG(str2sig(invalid[i], &sig) != 0,
		    "str2sig(\"%s\") unexpectedly succeeded", invalid[i]);
	}
}

ATF_TP_ADD_TCS(tp)
{
	ATF_TP_ADD_TC(tp, sig2str_valid);
	ATF_TP_ADD_TC(tp, sig2str_invalid);
	ATF_TP_ADD_TC(tp, str2sig_rtmin_rtmax);
	ATF_TP_ADD_TC(tp, str2sig_invalid_rt);
	ATF_TP_ADD_TC(tp, str2sig_fullname);
	ATF_TP_ADD_TC(tp, str2sig_lowercase);
	ATF_TP_ADD_TC(tp, str2sig_numeric);
	ATF_TP_ADD_TC(tp, str2sig_invalid);
	return (atf_no_error());
}
