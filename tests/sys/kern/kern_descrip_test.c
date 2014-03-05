/*-
 * Copyright (c) 2014 EMC Corp.
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

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <sys/limits.h>
#include <sys/stat.h>
#include <unistd.h>
#include <atf-c.h>

ATF_TC_WITHOUT_HEAD(dup2_simple);
ATF_TC_BODY(dup2_simple, tc)
{
	int fd1, fd2;
	struct stat sb1, sb2;

	fd1 = open("/etc/passwd", O_RDONLY);
	ATF_REQUIRE(fd1 >= 0);
	fd2 = 27;
	ATF_REQUIRE(dup2(fd1, fd2) != -1);
	ATF_REQUIRE(fstat(fd1, &sb1) != -1);
	ATF_REQUIRE(fstat(fd2, &sb2) != -1);
	ATF_REQUIRE(bcmp(&sb1, &sb2, sizeof(sb1)) == 0);
}

ATF_TC(dup2__ebadf_when_2nd_arg_out_of_range);
ATF_TC_HEAD(dup2__ebadf_when_2nd_arg_out_of_range, tc)
{
	atf_tc_set_md_var(tc, "descr", "Regression test for r234131");
}
ATF_TC_BODY(dup2__ebadf_when_2nd_arg_out_of_range, tc)
{
	int fd1, fd2, ret;

	fd1 =  open("/etc/passwd", O_RDONLY);
	fd2 = INT_MAX;
	ret = dup2(fd1, fd2);
	ATF_CHECK_EQ(-1, ret);
	ATF_CHECK_EQ(EBADF, errno);
}

ATF_TP_ADD_TCS(tp)
{

        ATF_TP_ADD_TC(tp, dup2_simple);
        ATF_TP_ADD_TC(tp, dup2__ebadf_when_2nd_arg_out_of_range);

        return atf_no_error();
}
