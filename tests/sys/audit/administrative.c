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

#include <sys/param.h>
#include <sys/mount.h>
#include <sys/time.h>

#include <atf-c.h>
#include <fcntl.h>
#include <unistd.h>

#include "utils.h"

static pid_t pid;
static int filedesc;
static mode_t mode = 0777;
static struct pollfd fds[1];
static char adregex[60];
static const char *auclass = "ad";
static const char *path = "fileforaudit";


ATF_TC_WITH_CLEANUP(settimeofday_success);
ATF_TC_HEAD(settimeofday_success, tc)
{
	atf_tc_set_md_var(tc, "descr", "Tests the audit of a successful "
					"settimeofday(2) call");
}

ATF_TC_BODY(settimeofday_success, tc)
{
	pid = getpid();
	snprintf(adregex, sizeof(adregex), "settimeofday.*%d.*success", pid);

	struct timeval tp;
	struct timezone tzp;
	ATF_REQUIRE_EQ(0, gettimeofday(&tp, &tzp));

	FILE *pipefd = setup(fds, auclass);
	/* Setting the same time as obtained by gettimeofday(2) */
	ATF_REQUIRE_EQ(0, settimeofday(&tp, &tzp));
	check_audit(fds, adregex, pipefd);
}

ATF_TC_CLEANUP(settimeofday_success, tc)
{
	cleanup();
}


ATF_TC_WITH_CLEANUP(settimeofday_failure);
ATF_TC_HEAD(settimeofday_failure, tc)
{
	atf_tc_set_md_var(tc, "descr", "Tests the audit of an unsuccessful "
					"settimeofday(2) call");
}

ATF_TC_BODY(settimeofday_failure, tc)
{
	pid = getpid();
	snprintf(adregex, sizeof(adregex), "settimeofday.*%d.*failure", pid);

	struct timeval tp;
	struct timezone tzp;
	ATF_REQUIRE_EQ(0, gettimeofday(&tp, &tzp));

	FILE *pipefd = setup(fds, auclass);
	tp.tv_sec = -1;
	/* Failure reason: Invalid value for tp.tv_sec; */
	ATF_REQUIRE_EQ(-1, settimeofday(&tp, &tzp));
	check_audit(fds, adregex, pipefd);
}

ATF_TC_CLEANUP(settimeofday_failure, tc)
{
	cleanup();
}


ATF_TC_WITH_CLEANUP(adjtime_success);
ATF_TC_HEAD(adjtime_success, tc)
{
	atf_tc_set_md_var(tc, "descr", "Tests the audit of a successful "
					"adjtime(2) call");
}

ATF_TC_BODY(adjtime_success, tc)
{
	pid = getpid();
	snprintf(adregex, sizeof(adregex), "adjtime.*%d.*return,success", pid);

	FILE *pipefd = setup(fds, auclass);
	/* We don't want to change the system time, hence NULL */
	ATF_REQUIRE_EQ(0, adjtime(NULL,NULL));
	check_audit(fds, adregex, pipefd);
}

ATF_TC_CLEANUP(adjtime_success, tc)
{
	cleanup();
}


ATF_TC_WITH_CLEANUP(adjtime_failure);
ATF_TC_HEAD(adjtime_failure, tc)
{
	atf_tc_set_md_var(tc, "descr", "Tests the audit of an unsuccessful "
					"adjtime(2) call");
}

ATF_TC_BODY(adjtime_failure, tc)
{
	pid = getpid();
	snprintf(adregex, sizeof(adregex), "adjtime.*%d.*return,failure", pid);

	FILE *pipefd = setup(fds, auclass);
	ATF_REQUIRE_EQ(-1, adjtime((struct timeval *)(-1), NULL));
	check_audit(fds, adregex, pipefd);
}

ATF_TC_CLEANUP(adjtime_failure, tc)
{
	cleanup();
}



ATF_TC_WITH_CLEANUP(nfs_getfh_success);
ATF_TC_HEAD(nfs_getfh_success, tc)
{
	atf_tc_set_md_var(tc, "descr", "Tests the audit of a successful "
					"getfh(2) call");
}

ATF_TC_BODY(nfs_getfh_success, tc)
{
	fhandle_t fhp;
	pid = getpid();
	snprintf(adregex, sizeof(adregex), "nfs_getfh.*%d.*ret.*success", pid);

	/* File needs to exist to call getfh(2) */
	ATF_REQUIRE(filedesc = open(path, O_CREAT, mode) != -1);
	FILE *pipefd = setup(fds, auclass);
	ATF_REQUIRE_EQ(0, getfh(path, &fhp));
	check_audit(fds, adregex, pipefd);
	close(filedesc);
}

ATF_TC_CLEANUP(nfs_getfh_success, tc)
{
	cleanup();
}


ATF_TC_WITH_CLEANUP(nfs_getfh_failure);
ATF_TC_HEAD(nfs_getfh_failure, tc)
{
	atf_tc_set_md_var(tc, "descr", "Tests the audit of an unsuccessful "
					"getfh(2) call");
}

ATF_TC_BODY(nfs_getfh_failure, tc)
{
	pid = getpid();
	snprintf(adregex, sizeof(adregex), "nfs_getfh.*%d.*ret.*failure", pid);

	FILE *pipefd = setup(fds, auclass);
	/* Failure reason: file does not exist */
	ATF_REQUIRE_EQ(-1, getfh(path, NULL));
	check_audit(fds, adregex, pipefd);
}

ATF_TC_CLEANUP(nfs_getfh_failure, tc)
{
	cleanup();
}


ATF_TP_ADD_TCS(tp)
{
	ATF_TP_ADD_TC(tp, settimeofday_success);
	ATF_TP_ADD_TC(tp, settimeofday_failure);
	ATF_TP_ADD_TC(tp, adjtime_success);
	ATF_TP_ADD_TC(tp, adjtime_failure);

	ATF_TP_ADD_TC(tp, nfs_getfh_success);
	ATF_TP_ADD_TC(tp, nfs_getfh_failure);

	return (atf_no_error());
}
