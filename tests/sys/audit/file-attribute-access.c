/*-
 * Copyright (c) 2018 Aniket Pandey
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
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#include <sys/stat.h>
#include <sys/syscall.h>

#include <atf-c.h>
#include <fcntl.h>
#include <unistd.h>

#include "utils.h"

static struct pollfd fds[1];
static mode_t mode = 0777;
static int filedesc;
static char extregex[80];
static struct stat statbuff;
static const char *auclass = "fa";
static const char *path = "fileforaudit";
static const char *errpath = "dirdoesnotexist/fileforaudit";
static const char *successreg = "fileforaudit.*return,success";
static const char *failurereg = "fileforaudit.*return,failure";


ATF_TC_WITH_CLEANUP(stat_success);
ATF_TC_HEAD(stat_success, tc)
{
	atf_tc_set_md_var(tc, "descr", "Tests the audit of a successful "
					"stat(2) call");
}

ATF_TC_BODY(stat_success, tc)
{
	/* File needs to exist to call stat(2) */
	ATF_REQUIRE((filedesc = open(path, O_CREAT, mode)) != -1);
	FILE *pipefd = setup(fds, auclass);
	ATF_REQUIRE_EQ(0, stat(path, &statbuff));
	check_audit(fds, successreg, pipefd);
	close(filedesc);
}

ATF_TC_CLEANUP(stat_success, tc)
{
	cleanup();
}


ATF_TC_WITH_CLEANUP(stat_failure);
ATF_TC_HEAD(stat_failure, tc)
{
	atf_tc_set_md_var(tc, "descr", "Tests the audit of an unsuccessful "
					"stat(2) call");
}

ATF_TC_BODY(stat_failure, tc)
{
	FILE *pipefd = setup(fds, auclass);
	/* Failure reason: file does not exist */
	ATF_REQUIRE_EQ(-1, stat(errpath, &statbuff));
	check_audit(fds, failurereg, pipefd);
}

ATF_TC_CLEANUP(stat_failure, tc)
{
	cleanup();
}


ATF_TC_WITH_CLEANUP(lstat_success);
ATF_TC_HEAD(lstat_success, tc)
{
	atf_tc_set_md_var(tc, "descr", "Tests the audit of a successful "
					"lstat(2) call");
}

ATF_TC_BODY(lstat_success, tc)
{
	/* Symbolic link needs to exist to call lstat(2) */
	ATF_REQUIRE_EQ(0, symlink("symlink", path));
	FILE *pipefd = setup(fds, auclass);
	ATF_REQUIRE_EQ(0, lstat(path, &statbuff));
	check_audit(fds, successreg, pipefd);
}

ATF_TC_CLEANUP(lstat_success, tc)
{
	cleanup();
}


ATF_TC_WITH_CLEANUP(lstat_failure);
ATF_TC_HEAD(lstat_failure, tc)
{
	atf_tc_set_md_var(tc, "descr", "Tests the audit of an unsuccessful "
					"lstat(2) call");
}

ATF_TC_BODY(lstat_failure, tc)
{
	FILE *pipefd = setup(fds, auclass);
	/* Failure reason: symbolic link does not exist */
	ATF_REQUIRE_EQ(-1, lstat(errpath, &statbuff));
	check_audit(fds, failurereg, pipefd);
}

ATF_TC_CLEANUP(lstat_failure, tc)
{
	cleanup();
}


ATF_TC_WITH_CLEANUP(fstat_success);
ATF_TC_HEAD(fstat_success, tc)
{
	atf_tc_set_md_var(tc, "descr", "Tests the audit of a successful "
					"fstat(2) call");
}

ATF_TC_BODY(fstat_success, tc)
{
	/* File needs to exist to call fstat(2) */
	ATF_REQUIRE((filedesc = open(path, O_CREAT | O_RDWR, mode)) != -1);
	FILE *pipefd = setup(fds, auclass);
	ATF_REQUIRE_EQ(0, fstat(filedesc, &statbuff));

	snprintf(extregex, sizeof(extregex),
		"fstat.*%jd.*return,success", (intmax_t)statbuff.st_ino);
	check_audit(fds, extregex, pipefd);
	close(filedesc);
}

ATF_TC_CLEANUP(fstat_success, tc)
{
	cleanup();
}


ATF_TC_WITH_CLEANUP(fstat_failure);
ATF_TC_HEAD(fstat_failure, tc)
{
	atf_tc_set_md_var(tc, "descr", "Tests the audit of an unsuccessful "
					"fstat(2) call");
}

ATF_TC_BODY(fstat_failure, tc)
{
	FILE *pipefd = setup(fds, auclass);
	const char *regex = "fstat.*return,failure : Bad file descriptor";
	/* Failure reason: bad file descriptor */
	ATF_REQUIRE_EQ(-1, fstat(-1, &statbuff));
	check_audit(fds, regex, pipefd);
}

ATF_TC_CLEANUP(fstat_failure, tc)
{
	cleanup();
}


ATF_TC_WITH_CLEANUP(fstatat_success);
ATF_TC_HEAD(fstatat_success, tc)
{
	atf_tc_set_md_var(tc, "descr", "Tests the audit of a successful "
					"fstatat(2) call");
}

ATF_TC_BODY(fstatat_success, tc)
{
	/* File or Symbolic link needs to exist to call lstat(2) */
	ATF_REQUIRE_EQ(0, symlink("symlink", path));
	FILE *pipefd = setup(fds, auclass);
	ATF_REQUIRE_EQ(0, fstatat(AT_FDCWD, path, &statbuff,
		AT_SYMLINK_NOFOLLOW));
	check_audit(fds, successreg, pipefd);
}

ATF_TC_CLEANUP(fstatat_success, tc)
{
	cleanup();
}


ATF_TC_WITH_CLEANUP(fstatat_failure);
ATF_TC_HEAD(fstatat_failure, tc)
{
	atf_tc_set_md_var(tc, "descr", "Tests the audit of an unsuccessful "
					"fstatat(2) call");
}

ATF_TC_BODY(fstatat_failure, tc)
{
	FILE *pipefd = setup(fds, auclass);
	/* Failure reason: symbolic link does not exist */
	ATF_REQUIRE_EQ(-1, fstatat(AT_FDCWD, path, &statbuff,
		AT_SYMLINK_NOFOLLOW));
	check_audit(fds, failurereg, pipefd);
}

ATF_TC_CLEANUP(fstatat_failure, tc)
{
	cleanup();
}


ATF_TP_ADD_TCS(tp)
{
	ATF_TP_ADD_TC(tp, stat_success);
	ATF_TP_ADD_TC(tp, stat_failure);
	ATF_TP_ADD_TC(tp, lstat_success);
	ATF_TP_ADD_TC(tp, lstat_failure);
	ATF_TP_ADD_TC(tp, fstat_success);
	ATF_TP_ADD_TC(tp, fstat_failure);
	ATF_TP_ADD_TC(tp, fstatat_success);
	ATF_TP_ADD_TC(tp, fstatat_failure);

	return (atf_no_error());
}
