/*	$NetBSD: t_mkfifoat.c,v 1.5 2019/06/20 03:31:53 kamil Exp $ */

/*-
 * Copyright (c) 2012 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Emmanuel Dreyfus.
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
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */
#include <sys/cdefs.h>
__RCSID("$NetBSD: t_mkfifoat.c,v 1.5 2019/06/20 03:31:53 kamil Exp $");

#include <atf-c.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <paths.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/param.h>
#include <sys/stat.h>

#define DIR "dir"
#define FIFO "dir/openat"
#define BASEFIFO "openat"
#define FIFOERR "dir/openaterr" 

ATF_TC(mkfifoat_fd);
ATF_TC_HEAD(mkfifoat_fd, tc)
{
	atf_tc_set_md_var(tc, "descr", "See that mkfifoat works with fd");
}
ATF_TC_BODY(mkfifoat_fd, tc)
{
	int dfd;
	mode_t mode = 0600;

	ATF_REQUIRE(mkdir(DIR, 0755) == 0);
	ATF_REQUIRE((dfd = open(DIR, O_RDONLY, 0)) != -1);
	ATF_REQUIRE(mkfifoat(dfd, BASEFIFO, mode) != -1);
	ATF_REQUIRE(access(FIFO, F_OK) == 0);
	(void)close(dfd);
}

ATF_TC(mkfifoat_fdcwd);
ATF_TC_HEAD(mkfifoat_fdcwd, tc)
{
	atf_tc_set_md_var(tc, "descr", 
			  "See that mkfifoat works with fd as AT_FDCWD");
}
ATF_TC_BODY(mkfifoat_fdcwd, tc)
{
	mode_t mode = 0600;

	ATF_REQUIRE(mkdir(DIR, 0755) == 0);
	ATF_REQUIRE(mkfifoat(AT_FDCWD, FIFO, mode) != -1);
	ATF_REQUIRE(access(FIFO, F_OK) == 0);
}

ATF_TC(mkfifoat_fdcwderr);
ATF_TC_HEAD(mkfifoat_fdcwderr, tc)
{
	atf_tc_set_md_var(tc, "descr", 
		  "See that mkfifoat fails with fd as AT_FDCWD and bad path");
}
ATF_TC_BODY(mkfifoat_fdcwderr, tc)
{
	mode_t mode = 0600;

	ATF_REQUIRE(mkfifoat(AT_FDCWD, FIFOERR, mode) == -1);
}

ATF_TC(mkfifoat_fderr);
ATF_TC_HEAD(mkfifoat_fderr, tc)
{
	atf_tc_set_md_var(tc, "descr", "See that mkfifoat fails with fd as -1");
}
ATF_TC_BODY(mkfifoat_fderr, tc)
{
	int fd;
	mode_t mode = 0600;

	ATF_REQUIRE(mkdir(DIR, 0755) == 0);
	ATF_REQUIRE((fd = open(FIFO, O_CREAT|O_RDWR, 0644)) != -1);
	ATF_REQUIRE(close(fd) == 0);
	ATF_REQUIRE(mkfifoat(-1, FIFO, mode) == -1);
}

ATF_TC(mknodat_s_ififo);
ATF_TC_HEAD(mknodat_s_ififo, tc)
{
	atf_tc_set_md_var(tc, "descr", "Test mknodat(2) with S_IFIFO");
}

ATF_TC_BODY(mknodat_s_ififo, tc)
{
	struct stat st;
	int dfd;
	mode_t mode = S_IFIFO | 0600;

	(void)memset(&st, 0, sizeof(struct stat));

	ATF_REQUIRE(mkdir(DIR, 0755) == 0);
	ATF_REQUIRE((dfd = open(DIR, O_RDONLY, 0)) != -1);
	ATF_REQUIRE(mknodat(dfd, BASEFIFO, mode, 0) != -1);
	ATF_REQUIRE(access(FIFO, F_OK) == 0);
	ATF_REQUIRE(stat(FIFO, &st) == 0);

	if (S_ISFIFO(st.st_mode) == 0)
		atf_tc_fail("invalid mode from mknodat(2) with S_IFIFO");

	(void)close(dfd);
}

ATF_TP_ADD_TCS(tp)
{

	ATF_TP_ADD_TC(tp, mkfifoat_fd);
	ATF_TP_ADD_TC(tp, mkfifoat_fdcwd);
	ATF_TP_ADD_TC(tp, mkfifoat_fdcwderr);
	ATF_TP_ADD_TC(tp, mkfifoat_fderr);
	ATF_TP_ADD_TC(tp, mknodat_s_ififo);

	return atf_no_error();
}
