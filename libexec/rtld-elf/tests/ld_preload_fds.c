/*-
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright 2021 Mariusz Zaborski <oshogbo@FreeBSD.org>
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#include <atf-c.h>
#include <fcntl.h>
#include <stdio.h>

#include "common.h"

int binaryfd;
int libraryfd;

static void
setup(const atf_tc_t *tc)
{
	int testdir;

	testdir = opendir_fd(atf_tc_get_config_var(tc, "srcdir"));
	ATF_REQUIRE(testdir >= 0);

	binaryfd = openat(testdir, TARGET_ELF_NAME, O_RDONLY);
	ATF_REQUIRE(binaryfd >= 0);
	libraryfd = openat(testdir, TARGET_LIBRARY, O_RDONLY);
	ATF_REQUIRE(libraryfd >= 0);

	close(testdir);
}

ATF_TC_WITHOUT_HEAD(missing_library);
ATF_TC_BODY(missing_library, tc)
{

	setup(tc);
	expect_missing_library(binaryfd, NULL);
}

ATF_TC_WITHOUT_HEAD(bad_librarys);
ATF_TC_BODY(bad_librarys, tc)
{
	char *senv;

	ATF_REQUIRE(asprintf(&senv, "LD_PRELOAD_FDS=::") > 0);

	setup(tc);
	expect_missing_library(binaryfd, senv);
}

ATF_TC_WITHOUT_HEAD(single_library);
ATF_TC_BODY(single_library, tc)
{
	char *senv;

	setup(tc);

	ATF_REQUIRE(
	    asprintf(&senv, "LD_PRELOAD_FDS=%d", libraryfd) > 0);

	expect_success(binaryfd, senv);
}

ATF_TC_WITHOUT_HEAD(two_librarys);
ATF_TC_BODY(two_librarys, tc)
{
	char *senv;

	setup(tc);

	ATF_REQUIRE(
	    asprintf(&senv, "LD_PRELOAD_FDS=%d:%d", libraryfd, libraryfd) > 0);

	expect_success(binaryfd, senv);
}

/* Register test cases with ATF. */
ATF_TP_ADD_TCS(tp)
{

	ATF_TP_ADD_TC(tp, missing_library);
	ATF_TP_ADD_TC(tp, bad_librarys);
	ATF_TP_ADD_TC(tp, single_library);
	ATF_TP_ADD_TC(tp, two_librarys);

	return atf_no_error();
}
