/* $NetBSD: t_wait.c,v 1.4 2016/04/27 21:14:24 christos Exp $ */

/*-
 * Copyright (c) 2016 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Christos Zoulas.
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
__RCSID("$NetBSD: t_wait.c,v 1.4 2016/04/27 21:14:24 christos Exp $");

#include <sys/wait.h>
#include <sys/resource.h>

#include <stdio.h>
#include <errno.h>
#include <limits.h>
#include <pwd.h>
#include <signal.h>
#include <stdlib.h>
#include <unistd.h>

#include <atf-c.h>

#ifdef __FreeBSD__
#define	wrusage	__wrusage
#endif

ATF_TC(wait6_invalid);
ATF_TC_HEAD(wait6_invalid, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "Test that wait6(2) returns EINVAL with 0 options");
}

ATF_TC_BODY(wait6_invalid, tc)
{
	siginfo_t si;
	struct wrusage wru;
	int st;
	ATF_REQUIRE(wait6(P_ALL, 0, &st, 0, &wru, &si) == -1
	    && errno == EINVAL);
}

ATF_TC(wait6_noproc);
ATF_TC_HEAD(wait6_noproc, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "Test that wait6(2) returns ECHILD with for no processes");
}

ATF_TC_BODY(wait6_noproc, tc)
{
	siginfo_t si;
	struct wrusage wru;
	int st;
	ATF_REQUIRE(wait6(P_ALL, 0, &st, WEXITED, &wru, &si) == -1
	    && errno == ECHILD);
}

ATF_TC(wait6_exited);
ATF_TC_HEAD(wait6_exited, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "Test that wait6(2) handled exiting process and code");
}

ATF_TC_BODY(wait6_exited, tc)
{
	siginfo_t si;
	struct wrusage wru;
	int st;
	pid_t pid;

	switch (pid = fork()) {
	case -1:
		ATF_REQUIRE(pid > 0); 
	case 0:
		exit(0x5a5a5a5a);
		/*NOTREACHED*/
	default:
		ATF_REQUIRE(wait6(P_PID, pid, &st, WEXITED, &wru, &si) == pid); 
		ATF_REQUIRE(WIFEXITED(st) && WEXITSTATUS(st) == 0x5a);
		ATF_REQUIRE(si.si_status = 0x5a5a5a5a);
		ATF_REQUIRE(si.si_pid == pid);
		ATF_REQUIRE(si.si_uid == getuid());
		ATF_REQUIRE(si.si_code == CLD_EXITED);
#ifdef __NetBSD__
		printf("user: %ju system: %ju\n", (uintmax_t)si.si_utime,
		    (uintmax_t)si.si_utime);
#endif
		break;
	}
}

ATF_TC(wait6_terminated);
ATF_TC_HEAD(wait6_terminated, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "Test that wait6(2) handled terminated process and code");
}

ATF_TC_BODY(wait6_terminated, tc)
{
	siginfo_t si;
	struct wrusage wru;
	int st;
	pid_t pid;

	switch (pid = fork()) {
	case 0:
		sleep(100);
		/*FALLTHROUGH*/
	case -1:
		ATF_REQUIRE(pid > 0); 
	default:
		ATF_REQUIRE(kill(pid, SIGTERM) == 0);
		ATF_REQUIRE(wait6(P_PID, pid, &st, WEXITED, &wru, &si) == pid); 
		ATF_REQUIRE(WIFSIGNALED(st) && WTERMSIG(st) == SIGTERM);
		ATF_REQUIRE(si.si_status == SIGTERM);
		ATF_REQUIRE(si.si_pid == pid);
		ATF_REQUIRE(si.si_uid == getuid());
		ATF_REQUIRE(si.si_code == CLD_KILLED);
#ifdef __NetBSD__
		printf("user: %ju system: %ju\n", (uintmax_t)si.si_utime,
		    (uintmax_t)si.si_utime);
#endif
		break;
	}
}

ATF_TC(wait6_coredumped);
ATF_TC_HEAD(wait6_coredumped, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "Test that wait6(2) handled coredumped process and code");
}

ATF_TC_BODY(wait6_coredumped, tc)
{
	siginfo_t si;
	struct wrusage wru;
	int st;
	pid_t pid;
	static const struct rlimit rl = { RLIM_INFINITY, RLIM_INFINITY };

	switch (pid = fork()) {
	case 0:
		ATF_REQUIRE(setrlimit(RLIMIT_CORE, &rl) == 0);
		*(char *)8 = 0;
		/*FALLTHROUGH*/
	case -1:
		ATF_REQUIRE(pid > 0); 
	default:
		ATF_REQUIRE(wait6(P_PID, pid, &st, WEXITED, &wru, &si) == pid); 
		ATF_REQUIRE(WIFSIGNALED(st) && WTERMSIG(st) == SIGSEGV
		    && WCOREDUMP(st));
		ATF_REQUIRE(si.si_status == SIGSEGV);
		ATF_REQUIRE(si.si_pid == pid);
		ATF_REQUIRE(si.si_uid == getuid());
		ATF_REQUIRE(si.si_code == CLD_DUMPED);
#ifdef __NetBSD__
		printf("user: %ju system: %ju\n", (uintmax_t)si.si_utime,
		    (uintmax_t)si.si_utime);
#endif
		break;
	}
}

ATF_TC(wait6_stop_and_go);
ATF_TC_HEAD(wait6_stop_and_go, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "Test that wait6(2) handled stopped/continued process and code");
}

ATF_TC_BODY(wait6_stop_and_go, tc)
{
	siginfo_t si;
	struct wrusage wru;
	int st;
	pid_t pid;
	static const struct rlimit rl = { 0, 0 };

	ATF_REQUIRE(setrlimit(RLIMIT_CORE, &rl) == 0);
	switch (pid = fork()) {
	case 0:
		sleep(100);
		/*FALLTHROUGH*/
	case -1:
		ATF_REQUIRE(pid > 0); 
	default:
		ATF_REQUIRE(kill(pid, SIGSTOP) == 0);
		ATF_REQUIRE(wait6(P_PID, pid, &st, WSTOPPED, &wru, &si) == pid); 
		ATF_REQUIRE(WIFSTOPPED(st) && WSTOPSIG(st) == SIGSTOP);
		ATF_REQUIRE(si.si_status == SIGSTOP);
		ATF_REQUIRE(si.si_pid == pid);
		ATF_REQUIRE(si.si_uid == getuid());
		ATF_REQUIRE(si.si_code == CLD_STOPPED);
#ifdef __NetBSD__
		printf("user: %ju system: %ju\n", (uintmax_t)si.si_utime,
		    (uintmax_t)si.si_utime);
#endif

		ATF_REQUIRE(kill(pid, SIGCONT) == 0);
		ATF_REQUIRE(wait6(P_PID, pid, &st, WCONTINUED, &wru, &si) == pid); 
		ATF_REQUIRE(WIFCONTINUED(st));
		ATF_REQUIRE(si.si_status == SIGCONT);
		ATF_REQUIRE(si.si_pid == pid);
		ATF_REQUIRE(si.si_uid == getuid());
		ATF_REQUIRE(si.si_code == CLD_CONTINUED);
#ifdef __NetBSD__
		printf("user: %ju system: %ju\n", (uintmax_t)si.si_utime,
		    (uintmax_t)si.si_utime);
#endif

		ATF_REQUIRE(kill(pid, SIGQUIT) == 0);
		ATF_REQUIRE(wait6(P_PID, pid, &st, WEXITED, &wru, &si) == pid); 
		ATF_REQUIRE(WIFSIGNALED(st) && WTERMSIG(st) == SIGQUIT);
		ATF_REQUIRE(si.si_status == SIGQUIT);
		ATF_REQUIRE(si.si_pid == pid);
		ATF_REQUIRE(si.si_uid == getuid());
		ATF_REQUIRE(si.si_code == CLD_KILLED);
#ifdef __NetBSD__
		printf("user: %ju system: %ju\n", (uintmax_t)si.si_utime,
		    (uintmax_t)si.si_utime);
#endif
		break;
	}
}

ATF_TP_ADD_TCS(tp)
{

	ATF_TP_ADD_TC(tp, wait6_invalid);
	ATF_TP_ADD_TC(tp, wait6_noproc);
	ATF_TP_ADD_TC(tp, wait6_exited);
	ATF_TP_ADD_TC(tp, wait6_terminated);
	ATF_TP_ADD_TC(tp, wait6_coredumped);
	ATF_TP_ADD_TC(tp, wait6_stop_and_go);

	return atf_no_error();
}
