/*-
 * Copyright (c) 2013 Jilles Tjoelker
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

#include <sys/filio.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>

#include <atf-c.h>

/*
 * O_ACCMODE is currently defined incorrectly. This is what it should be.
 * Various code depends on the incorrect value.
 */
#define CORRECT_O_ACCMODE (O_ACCMODE | O_EXEC)

static void
basic_tests(const char *path, int omode, const char *omodetext)
{
	int fd, flags1, flags2, flags3;

	fd = open(path, omode);
	ATF_REQUIRE_MSG(fd != -1, "open(\"%s\", %s) failed: %s", path,
	    omodetext, strerror(errno));

	flags1 = fcntl(fd, F_GETFL);
	ATF_REQUIRE_MSG(flags1 != -1, "fcntl(F_GETFL) (1) failed: %s",
	    strerror(errno));
	ATF_REQUIRE_INTEQ(omode, flags1 & CORRECT_O_ACCMODE);
	ATF_REQUIRE((flags1 & O_NONBLOCK) == 0);

	ATF_REQUIRE_MSG(fcntl(fd, F_SETFL, flags1) != -1,
	    "fcntl(F_SETFL) same flags failed: %s", strerror(errno));

	flags2 = fcntl(fd, F_GETFL);
	ATF_REQUIRE_MSG(flags2 != -1, "fcntl(F_GETFL) (2) failed: %s",
	    strerror(errno));
	ATF_REQUIRE_INTEQ(flags1, flags2);

	ATF_REQUIRE_MSG(fcntl(fd, F_SETFL, flags2 | O_NONBLOCK) != -1,
	    "fcntl(F_SETFL) O_NONBLOCK failed: %s", strerror(errno));

	flags3 = fcntl(fd, F_GETFL);
	ATF_REQUIRE_MSG(flags3 != -1, "fcntl(F_GETFL) (3) failed: %s",
	    strerror(errno));
	ATF_REQUIRE_INTEQ(flags2 | O_NONBLOCK, flags3);

	(void)close(fd);
}

ATF_TC_WITHOUT_HEAD(read_only_null);
ATF_TC_BODY(read_only_null, tc)
{
	basic_tests("/dev/null", O_RDONLY, "O_RDONLY");
}

ATF_TC_WITHOUT_HEAD(write_only_null);
ATF_TC_BODY(write_only_null, tc)
{
	basic_tests("/dev/null", O_WRONLY, "O_WRONLY");
}

ATF_TC_WITHOUT_HEAD(read_write_null);
ATF_TC_BODY(read_write_null, tc)
{
	basic_tests("/dev/null", O_RDWR, "O_RDWR");
}

ATF_TC_WITHOUT_HEAD(exec_only_sh);
ATF_TC_BODY(exec_only_sh, tc)
{
	basic_tests("/bin/sh", O_EXEC, "O_EXEC");
}

ATF_TC_WITHOUT_HEAD(fioasync_dev_null);
ATF_TC_BODY(fioasync_dev_null, tc)
{
	int fd, flags1, flags2, val;

	fd = open("/dev/null", O_RDONLY);
	ATF_REQUIRE_MSG(fd != -1, "open(\"/dev/null\") failed: %s",
	    strerror(errno));

	flags1 = fcntl(fd, F_GETFL);
	ATF_REQUIRE_MSG(flags1 != -1, "fcntl(F_GETFL) (1) failed: %s",
	    strerror(errno));
	ATF_REQUIRE((flags1 & O_ASYNC) == 0);

	val = 1;
	ATF_REQUIRE_ERRNO(EINVAL, ioctl(fd, FIOASYNC, &val) == -1);

	flags2 = fcntl(fd, F_GETFL);
	ATF_REQUIRE_MSG(flags2 != -1, "fcntl(F_GETFL) (2) failed: %s",
	    strerror(errno));
	ATF_REQUIRE_INTEQ(flags1, flags2);

	(void)close(fd);
}

ATF_TP_ADD_TCS(tp)
{
	ATF_TP_ADD_TC(tp, read_only_null);
	ATF_TP_ADD_TC(tp, write_only_null);
	ATF_TP_ADD_TC(tp, read_write_null);
	ATF_TP_ADD_TC(tp, exec_only_sh);
	ATF_TP_ADD_TC(tp, fioasync_dev_null);

	return (atf_no_error());
}
