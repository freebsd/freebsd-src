/*
 * Copyright (c) 2017 Jan Kokemüller
 * All rights reserved.
 * Copyright (c) 2025 Klara, Inc.
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
#include <sys/stat.h>

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <atf-c.h>

ATF_TC(realpath_null);
ATF_TC_HEAD(realpath_null, tc)
{
	atf_tc_set_md_var(tc, "descr", "Test null input");
}
ATF_TC_BODY(realpath_null, tc)
{
	ATF_REQUIRE_ERRNO(EINVAL, realpath(NULL, NULL) == NULL);
}

ATF_TC(realpath_empty);
ATF_TC_HEAD(realpath_empty, tc)
{
	atf_tc_set_md_var(tc, "descr", "Test empty input");
}
ATF_TC_BODY(realpath_empty, tc)
{
	char resb[PATH_MAX] = "";

	ATF_REQUIRE_EQ(0, mkdir("foo", 0755));
	ATF_REQUIRE_EQ(0, chdir("foo"));
	ATF_REQUIRE_ERRNO(ENOENT, realpath("", resb) == NULL);
	ATF_REQUIRE_STREQ("", resb);
}

ATF_TC(realpath_buffer_overflow);
ATF_TC_HEAD(realpath_buffer_overflow, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "Test for out of bounds read from 'left' array "
	    "(compile realpath.c with '-fsanitize=address')");
}

ATF_TC_BODY(realpath_buffer_overflow, tc)
{
	char path[PATH_MAX] = "";
	char resb[PATH_MAX] = "";

	memset(path, 'a', sizeof(path) - 1);
	path[1] = '/';
	ATF_REQUIRE(realpath(path, resb) == NULL);
}

ATF_TC(realpath_empty_symlink);
ATF_TC_HEAD(realpath_empty_symlink, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "Test for correct behavior when encountering empty symlinks");
}

ATF_TC_BODY(realpath_empty_symlink, tc)
{
	char path[PATH_MAX] = "";
	char slnk[PATH_MAX] = "";
	char resb[PATH_MAX] = "";
	int fd;

	(void)strlcat(slnk, "empty_symlink", sizeof(slnk));

	ATF_REQUIRE(symlink("", slnk) == 0);

	fd = open("aaa", O_RDONLY | O_CREAT, 0600);

	ATF_REQUIRE(fd >= 0);
	ATF_REQUIRE(close(fd) == 0);

	(void)strlcat(path, "empty_symlink", sizeof(path));
	(void)strlcat(path, "/aaa", sizeof(path));

	ATF_REQUIRE_ERRNO(ENOENT, realpath(path, resb) == NULL);

	ATF_REQUIRE(unlink("aaa") == 0);
	ATF_REQUIRE(unlink(slnk) == 0);
}

ATF_TC(realpath_partial);
ATF_TC_HEAD(realpath_partial, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "Test that failure leaves a partial result");
	atf_tc_set_md_var(tc, "require.user", "unprivileged");
}

ATF_TC_BODY(realpath_partial, tc)
{
	char resb[PATH_MAX] = "";
	size_t len;

	/* scenario 1: missing directory */
	ATF_REQUIRE_EQ(0, mkdir("foo", 0755));
	ATF_REQUIRE_ERRNO(ENOENT, realpath("foo/bar/baz", resb) == NULL);
	len = strnlen(resb, sizeof(resb));
	ATF_REQUIRE(len > 8 && len < sizeof(resb));
	ATF_REQUIRE_STREQ("/foo/bar", resb + len - 8);

	/* scenario 2: dead link 1 */
	ATF_REQUIRE_EQ(0, symlink("nix", "foo/bar"));
	ATF_REQUIRE_ERRNO(ENOENT, realpath("foo/bar/baz", resb) == NULL);
	len = strnlen(resb, sizeof(resb));
	ATF_REQUIRE(len > 8 && len < sizeof(resb));
	ATF_REQUIRE_STREQ("/foo/nix", resb + len - 8);

	/* scenario 3: missing file */
	ATF_REQUIRE_EQ(0, unlink("foo/bar"));
	ATF_REQUIRE_EQ(0, mkdir("foo/bar", 0755));
	ATF_REQUIRE_ERRNO(ENOENT, realpath("foo/bar/baz", resb) == NULL);
	len = strnlen(resb, sizeof(resb));
	ATF_REQUIRE(len > 12 && len < sizeof(resb));
	ATF_REQUIRE_STREQ("/foo/bar/baz", resb + len - 12);

	/* scenario 4: dead link 2 */
	ATF_REQUIRE_EQ(0, symlink("nix", "foo/bar/baz"));
	ATF_REQUIRE_ERRNO(ENOENT, realpath("foo/bar/baz", resb) == NULL);
	len = strnlen(resb, sizeof(resb));
	ATF_REQUIRE(len > 12 && len < sizeof(resb));
	ATF_REQUIRE_STREQ("/foo/bar/nix", resb + len - 12);

	/* scenario 5: unreadable directory */
	ATF_REQUIRE_EQ(0, chmod("foo", 000));
	ATF_REQUIRE_ERRNO(EACCES, realpath("foo/bar/baz", resb) == NULL);
	len = strnlen(resb, sizeof(resb));
	ATF_REQUIRE(len > 4 && len < sizeof(resb));
	ATF_REQUIRE_STREQ("/foo", resb + len - 4);

	/* scenario 6: not a directory */
	ATF_REQUIRE_EQ(0, close(creat("bar", 0644)));
	ATF_REQUIRE_ERRNO(ENOTDIR, realpath("bar/baz", resb) == NULL);
	len = strnlen(resb, sizeof(resb));
	ATF_REQUIRE(len > 4 && len < sizeof(resb));
	ATF_REQUIRE_STREQ("/bar", resb + len - 4);
}

ATF_TP_ADD_TCS(tp)
{
	ATF_TP_ADD_TC(tp, realpath_null);
	ATF_TP_ADD_TC(tp, realpath_empty);
	ATF_TP_ADD_TC(tp, realpath_buffer_overflow);
	ATF_TP_ADD_TC(tp, realpath_empty_symlink);
	ATF_TP_ADD_TC(tp, realpath_partial);

	return atf_no_error();
}
