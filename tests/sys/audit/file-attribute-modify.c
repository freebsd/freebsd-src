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

#include <sys/file.h>
#include <sys/stat.h>

#include <atf-c.h>
#include <fcntl.h>
#include <unistd.h>

#include "utils.h"

static pid_t pid;
static int filedesc;
static struct pollfd fds[1];
static mode_t mode = 0777;
static char extregex[80];
static const char *auclass = "fm";
static const char *path = "fileforaudit";
static const char *errpath = "adirhasnoname/fileforaudit";
static const char *successreg = "fileforaudit.*return,success";
static const char *failurereg = "fileforaudit.*return,failure";


ATF_TC_WITH_CLEANUP(flock_success);
ATF_TC_HEAD(flock_success, tc)
{
	atf_tc_set_md_var(tc, "descr", "Tests the audit of a successful "
					"flock(2) call");
}

ATF_TC_BODY(flock_success, tc)
{
	pid = getpid();
	snprintf(extregex, sizeof(extregex), "flock.*%d.*return,success", pid);

	/* File needs to exist to call flock(2) */
	ATF_REQUIRE((filedesc = open(path, O_CREAT, mode)) != -1);
	FILE *pipefd = setup(fds, auclass);
	ATF_REQUIRE_EQ(0, flock(filedesc, LOCK_SH));
	check_audit(fds, extregex, pipefd);
	close(filedesc);
}

ATF_TC_CLEANUP(flock_success, tc)
{
	cleanup();
}


ATF_TC_WITH_CLEANUP(flock_failure);
ATF_TC_HEAD(flock_failure, tc)
{
	atf_tc_set_md_var(tc, "descr", "Tests the audit of an unsuccessful "
					"flock(2) call");
}

ATF_TC_BODY(flock_failure, tc)
{
	const char *regex = "flock.*return,failure : Bad file descriptor";
	FILE *pipefd = setup(fds, auclass);
	ATF_REQUIRE_EQ(-1, flock(-1, LOCK_SH));
	check_audit(fds, regex, pipefd);
}

ATF_TC_CLEANUP(flock_failure, tc)
{
	cleanup();
}


ATF_TC_WITH_CLEANUP(fcntl_success);
ATF_TC_HEAD(fcntl_success, tc)
{
	atf_tc_set_md_var(tc, "descr", "Tests the audit of a successful "
					"fcntl(2) call");
}

ATF_TC_BODY(fcntl_success, tc)
{
	int flagstatus;
	/* File needs to exist to call fcntl(2) */
	ATF_REQUIRE((filedesc = open(path, O_CREAT, mode)) != -1);
	FILE *pipefd = setup(fds, auclass);

	/* Retrieve the status flags of 'filedesc' and store it in flagstatus */
	ATF_REQUIRE((flagstatus = fcntl(filedesc, F_GETFL, 0)) != -1);
	snprintf(extregex, sizeof(extregex),
			"fcntl.*return,success,%d", flagstatus);
	check_audit(fds, extregex, pipefd);
	close(filedesc);
}

ATF_TC_CLEANUP(fcntl_success, tc)
{
	cleanup();
}


ATF_TC_WITH_CLEANUP(fcntl_failure);
ATF_TC_HEAD(fcntl_failure, tc)
{
	atf_tc_set_md_var(tc, "descr", "Tests the audit of an unsuccessful "
					"fcntl(2) call");
}

ATF_TC_BODY(fcntl_failure, tc)
{
	const char *regex = "fcntl.*return,failure : Bad file descriptor";
	FILE *pipefd = setup(fds, auclass);
	ATF_REQUIRE_EQ(-1, fcntl(-1, F_GETFL, 0));
	check_audit(fds, regex, pipefd);
}

ATF_TC_CLEANUP(fcntl_failure, tc)
{
	cleanup();
}


ATF_TC_WITH_CLEANUP(fsync_success);
ATF_TC_HEAD(fsync_success, tc)
{
	atf_tc_set_md_var(tc, "descr", "Tests the audit of a successful "
					"fsync(2) call");
}

ATF_TC_BODY(fsync_success, tc)
{
	pid = getpid();
	snprintf(extregex, sizeof(extregex), "fsync.*%d.*return,success", pid);

	/* File needs to exist to call fsync(2) */
	ATF_REQUIRE((filedesc = open(path, O_CREAT, mode)) != -1);
	FILE *pipefd = setup(fds, auclass);
	ATF_REQUIRE_EQ(0, fsync(filedesc));
	check_audit(fds, extregex, pipefd);
	close(filedesc);
}

ATF_TC_CLEANUP(fsync_success, tc)
{
	cleanup();
}


ATF_TC_WITH_CLEANUP(fsync_failure);
ATF_TC_HEAD(fsync_failure, tc)
{
	atf_tc_set_md_var(tc, "descr", "Tests the audit of an unsuccessful "
					"fsync(2) call");
}

ATF_TC_BODY(fsync_failure, tc)
{
	const char *regex = "fsync.*return,failure : Bad file descriptor";
	FILE *pipefd = setup(fds, auclass);
	/* Failure reason: Invalid file descriptor */
	ATF_REQUIRE_EQ(-1, fsync(-1));
	check_audit(fds, regex, pipefd);
}

ATF_TC_CLEANUP(fsync_failure, tc)
{
	cleanup();
}


ATF_TC_WITH_CLEANUP(chmod_success);
ATF_TC_HEAD(chmod_success, tc)
{
	atf_tc_set_md_var(tc, "descr", "Tests the audit of a successful "
					"chmod(2) call");
}

ATF_TC_BODY(chmod_success, tc)
{
	/* File needs to exist to call chmod(2) */
	ATF_REQUIRE((filedesc = open(path, O_CREAT, mode)) != -1);
	FILE *pipefd = setup(fds, auclass);
	ATF_REQUIRE_EQ(0, chmod(path, mode));
	check_audit(fds, successreg, pipefd);
	close(filedesc);
}

ATF_TC_CLEANUP(chmod_success, tc)
{
	cleanup();
}


ATF_TC_WITH_CLEANUP(chmod_failure);
ATF_TC_HEAD(chmod_failure, tc)
{
	atf_tc_set_md_var(tc, "descr", "Tests the audit of an unsuccessful "
					"chmod(2) call");
}

ATF_TC_BODY(chmod_failure, tc)
{
	FILE *pipefd = setup(fds, auclass);
	/* Failure reason: file does not exist */
	ATF_REQUIRE_EQ(-1, chmod(errpath, mode));
	check_audit(fds, failurereg, pipefd);
}

ATF_TC_CLEANUP(chmod_failure, tc)
{
	cleanup();
}


ATF_TC_WITH_CLEANUP(fchmod_success);
ATF_TC_HEAD(fchmod_success, tc)
{
	atf_tc_set_md_var(tc, "descr", "Tests the audit of a successful "
					"fchmod(2) call");
}

ATF_TC_BODY(fchmod_success, tc)
{
	pid = getpid();
	snprintf(extregex, sizeof(extregex), "fchmod.*%d.*return,success", pid);

	/* File needs to exist to call fchmod(2) */
	ATF_REQUIRE((filedesc = open(path, O_CREAT, mode)) != -1);
	FILE *pipefd = setup(fds, auclass);
	ATF_REQUIRE_EQ(0, fchmod(filedesc, mode));
	check_audit(fds, extregex, pipefd);
	close(filedesc);
}

ATF_TC_CLEANUP(fchmod_success, tc)
{
	cleanup();
}


ATF_TC_WITH_CLEANUP(fchmod_failure);
ATF_TC_HEAD(fchmod_failure, tc)
{
	atf_tc_set_md_var(tc, "descr", "Tests the audit of an unsuccessful "
					"fchmod(2) call");
}

ATF_TC_BODY(fchmod_failure, tc)
{
	const char *regex = "fchmod.*return,failure : Bad file descriptor";
	FILE *pipefd = setup(fds, auclass);
	/* Failure reason: Invalid file descriptor */
	ATF_REQUIRE_EQ(-1, fchmod(-1, mode));
	check_audit(fds, regex, pipefd);
}

ATF_TC_CLEANUP(fchmod_failure, tc)
{
	cleanup();
}


ATF_TC_WITH_CLEANUP(lchmod_success);
ATF_TC_HEAD(lchmod_success, tc)
{
	atf_tc_set_md_var(tc, "descr", "Tests the audit of a successful "
					"lchmod(2) call");
}

ATF_TC_BODY(lchmod_success, tc)
{
	/* Symbolic link needs to exist to call lchmod(2) */
	ATF_REQUIRE_EQ(0, symlink("symlink", path));
	FILE *pipefd = setup(fds, auclass);
	ATF_REQUIRE_EQ(0, lchmod(path, mode));
	check_audit(fds, successreg, pipefd);
}

ATF_TC_CLEANUP(lchmod_success, tc)
{
	cleanup();
}


ATF_TC_WITH_CLEANUP(lchmod_failure);
ATF_TC_HEAD(lchmod_failure, tc)
{
	atf_tc_set_md_var(tc, "descr", "Tests the audit of an unsuccessful "
					"lchmod(2) call");
}

ATF_TC_BODY(lchmod_failure, tc)
{
	FILE *pipefd = setup(fds, auclass);
	/* Failure reason: file does not exist */
	ATF_REQUIRE_EQ(-1, lchmod(errpath, mode));
	check_audit(fds, failurereg, pipefd);
}

ATF_TC_CLEANUP(lchmod_failure, tc)
{
	cleanup();
}


ATF_TC_WITH_CLEANUP(fchmodat_success);
ATF_TC_HEAD(fchmodat_success, tc)
{
	atf_tc_set_md_var(tc, "descr", "Tests the audit of a successful "
					"fchmodat(2) call");
}

ATF_TC_BODY(fchmodat_success, tc)
{
	/* File needs to exist to call fchmodat(2) */
	ATF_REQUIRE((filedesc = open(path, O_CREAT, mode)) != -1);
	FILE *pipefd = setup(fds, auclass);
	ATF_REQUIRE_EQ(0, fchmodat(AT_FDCWD, path, mode, 0));
	check_audit(fds, successreg, pipefd);
	close(filedesc);
}

ATF_TC_CLEANUP(fchmodat_success, tc)
{
	cleanup();
}


ATF_TC_WITH_CLEANUP(fchmodat_failure);
ATF_TC_HEAD(fchmodat_failure, tc)
{
	atf_tc_set_md_var(tc, "descr", "Tests the audit of an unsuccessful "
					"fchmodat(2) call");
}

ATF_TC_BODY(fchmodat_failure, tc)
{
	FILE *pipefd = setup(fds, auclass);
	/* Failure reason: file does not exist */
	ATF_REQUIRE_EQ(-1, fchmodat(AT_FDCWD, errpath, mode, 0));
	check_audit(fds, failurereg, pipefd);
}

ATF_TC_CLEANUP(fchmodat_failure, tc)
{
	cleanup();
}


ATF_TP_ADD_TCS(tp)
{
	ATF_TP_ADD_TC(tp, flock_success);
	ATF_TP_ADD_TC(tp, flock_failure);
	ATF_TP_ADD_TC(tp, fcntl_success);
	ATF_TP_ADD_TC(tp, fcntl_failure);
	ATF_TP_ADD_TC(tp, fsync_success);
	ATF_TP_ADD_TC(tp, fsync_failure);

	ATF_TP_ADD_TC(tp, chmod_success);
	ATF_TP_ADD_TC(tp, chmod_failure);
	ATF_TP_ADD_TC(tp, fchmod_success);
	ATF_TP_ADD_TC(tp, fchmod_failure);
	ATF_TP_ADD_TC(tp, lchmod_success);
	ATF_TP_ADD_TC(tp, lchmod_failure);
	ATF_TP_ADD_TC(tp, fchmodat_success);
	ATF_TP_ADD_TC(tp, fchmodat_failure);

	return (atf_no_error());
}
