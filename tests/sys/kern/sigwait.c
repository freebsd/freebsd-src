/*-
 * Copyright (c) 2022 Dmitry Chagin <dchagin@FreeBSD.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */
#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/limits.h>
#include <sys/time.h>
#include <sys/sysctl.h>
#include <sys/user.h>
#include <sys/wait.h>

#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <stdbool.h>
#include <unistd.h>

#include <atf-c.h>


static inline struct timespec
make_timespec(time_t s, long int ns)
{
	struct timespec rts;

	rts.tv_sec = s;
	rts.tv_nsec = ns;
	return (rts);
}

static void
dummy_sig_handler(int sig)
{

}

static void
dummy_sigchld(int signo, siginfo_t *info, void *ctx)
{

}

static void
support_signal(int sig, sig_t handler)
{

	ATF_REQUIRE(signal(sig, handler) != SIG_ERR);
}

static void
support_sysctlset(const char *name, int32_t val)
{

	ATF_REQUIRE(sysctlbyname(name, NULL, NULL, &val, sizeof(val)) == 0);
}

static timer_t
support_create_timer(uint64_t sec, long int nsec, bool repeat,
    bool callback)
{
	struct sigevent ev = {
		.sigev_notify = SIGEV_SIGNAL,
		.sigev_signo = SIGALRM
	};
	struct itimerspec its =
	{
		{ .tv_sec = repeat ? sec : 0, .tv_nsec = repeat ? nsec : 0 },
		{ .tv_sec = sec, .tv_nsec = nsec }
	};
	struct sigaction sa;
	timer_t timerid;

	if (callback) {
		sa.sa_handler = dummy_sig_handler;
		sigemptyset (&sa.sa_mask);
		sa.sa_flags = 0;
		ATF_REQUIRE(sigaction(SIGALRM, &sa, NULL) == 0);
	}
	ATF_REQUIRE(timer_create(CLOCK_REALTIME, &ev, &timerid) == 0);
	ATF_REQUIRE(timer_settime(timerid, 0, &its, NULL) == 0);
	return (timerid);
}

static void
support_delete_timer(timer_t timer)
{

	ATF_REQUIRE(timer_delete(timer) == 0);
	support_signal(SIGALRM, SIG_DFL);
}

static pid_t
support_create_sig_proc(int sig, int count, unsigned int usec)
{
	pid_t pid, cpid;

	pid = getpid();
	ATF_REQUIRE(pid > 0);
	cpid = fork();
	ATF_REQUIRE(cpid >= 0);

	if (cpid == 0) {
		while (count-- > 0) {
			usleep(usec);
			if (kill(pid, sig) == -1)
				break;
		}
		exit(0);
	}
	return (cpid);
}

#define TIMESPEC_HZ     1000000000

static void
test_sigtimedwait_timeout_eagain(time_t sec, bool zero_tmo)
{
	struct timespec ts, timeout, now;
	sigset_t ss;
	int rv;

	ATF_REQUIRE(clock_gettime(CLOCK_REALTIME, &ts) == 0);

	timeout = make_timespec(sec, zero_tmo ? 0 : TIMESPEC_HZ/2);
	timespecadd(&ts, &timeout, &ts);

	sigemptyset(&ss);
	sigaddset(&ss, SIGUSR1);
	rv = sigtimedwait(&ss, NULL, &timeout);
	ATF_REQUIRE_EQ_MSG(-1, rv,
	    "sigtimedwait () should fail: rv %d, errno %d", rv, errno);
	ATF_REQUIRE_EQ_MSG(EAGAIN, errno,
	    "sigtimedwait() should fail with EAGAIN: rv %d, errno %d",
	    rv, errno);
	/* now >= ts */
	ATF_REQUIRE(clock_gettime(CLOCK_REALTIME, &now) == 0);
	ATF_REQUIRE_MSG(timespeccmp(&now, &ts, >=) == true,
	    "timespeccmp: now { %jd.%ld } < ts { %jd.%ld }",
	    (intmax_t)now.tv_sec, now.tv_nsec,
	    (intmax_t)ts.tv_sec, ts.tv_nsec);
}

ATF_TC(test_sigtimedwait_timeout_eagain0);
ATF_TC_HEAD(test_sigtimedwait_timeout_eagain0, tc)
{

	atf_tc_set_md_var(tc, "descr", "Check if sigtimedwait exits immediately");
}

ATF_TC_BODY(test_sigtimedwait_timeout_eagain0, tc)
{

	test_sigtimedwait_timeout_eagain(0, true);
}

ATF_TC(test_sigtimedwait_timeout_eagain1);
ATF_TC_HEAD(test_sigtimedwait_timeout_eagain1, tc)
{

	atf_tc_set_md_var(tc, "descr", "Check if sigtimedwait exits immediately");
}

ATF_TC_BODY(test_sigtimedwait_timeout_eagain1, tc)
{

	test_sigtimedwait_timeout_eagain(-1, true);
}

ATF_TC(test_sigtimedwait_timeout_eagain2);
ATF_TC_HEAD(test_sigtimedwait_timeout_eagain2, tc)
{

	atf_tc_set_md_var(tc, "descr", "Check if sigtimedwait exits immediately");
}

ATF_TC_BODY(test_sigtimedwait_timeout_eagain2, tc)
{

	test_sigtimedwait_timeout_eagain(-1, false);
}

ATF_TC(test_sigtimedwait_timeout_eagain3);
ATF_TC_HEAD(test_sigtimedwait_timeout_eagain3, tc)
{

	atf_tc_set_md_var(tc, "descr", "Check if sigtimedwait exits after specified timeout");
}

ATF_TC_BODY(test_sigtimedwait_timeout_eagain3, tc)
{

	test_sigtimedwait_timeout_eagain(0, false);
}

ATF_TC(test_sigtimedwait_large_timeout_eintr);
ATF_TC_HEAD(test_sigtimedwait_large_timeout_eintr, tc)
{

	atf_tc_set_md_var(tc, "descr", "Check if sigtimedwait exits with EINTR");
}

ATF_TC_BODY(test_sigtimedwait_large_timeout_eintr, tc)
{
	struct timespec ts;
	timer_t timerid;
	sigset_t ss;
	int rv;

	ts = make_timespec(LONG_MAX, 0);
	timerid = support_create_timer(0, 100000000, false, true);

	sigemptyset(&ss);
	sigaddset(&ss, SIGUSR1);
	rv = sigtimedwait(&ss, NULL, &ts);
	ATF_REQUIRE_EQ_MSG(-1, rv,
	    "sigtimedwait () should fail: rv %d, errno %d", rv, errno);
	ATF_REQUIRE_EQ_MSG(EINTR, errno,
	    "sigtimedwait() should fail with EINTR: rv %d, errno %d",
	    rv, errno);
	support_delete_timer(timerid);
}

ATF_TC(test_sigtimedwait_infinity);
ATF_TC_HEAD(test_sigtimedwait_infinity, tc)
{

	atf_tc_set_md_var(tc, "descr", "Check if sigtimedwait exits with EINTR");
}

ATF_TC_BODY(test_sigtimedwait_infinity, tc)
{
	timer_t timerid;
	sigset_t ss;
	int rv;

	timerid = support_create_timer(0, 100000000, false, true);

	sigemptyset(&ss);
	sigaddset(&ss, SIGUSR1);
	rv = sigtimedwait(&ss, NULL, NULL);
	ATF_REQUIRE_EQ_MSG(-1, rv,
	    "sigtimedwait () should fail: rv %d, errno %d", rv, errno);
	ATF_REQUIRE_EQ_MSG(EINTR, errno,
	    "sigtimedwait() should fail with EINTR: rv %d, errno %d",
	    rv, errno);
	support_delete_timer(timerid);
}

ATF_TC(test_sigtimedwait_einval);
ATF_TC_HEAD(test_sigtimedwait_einval, tc)
{

	atf_tc_set_md_var(tc, "descr", "Check if sigtimedwait exits with EINVAL");
}

ATF_TC_BODY(test_sigtimedwait_einval, tc)
{
	struct timespec ts;
	timer_t timerid;
	sigset_t ss;
	int rv;

	ts = make_timespec(0, -1);
	timerid = support_create_timer(0, 100000000, false, true);

	sigemptyset(&ss);
	sigaddset(&ss, SIGUSR1);
	rv = sigtimedwait(&ss, NULL, &ts);
	ATF_REQUIRE_EQ_MSG(-1, rv,
	    "sigtimedwait () should fail: rv %d, errno %d", rv, errno);
	ATF_REQUIRE_EQ_MSG(EINVAL, errno,
	    "sigtimedwait() should fail with EINVAL: rv %d, errno %d",
	    rv, errno);
	support_delete_timer(timerid);
}

ATF_TC(test_sigwait_eintr);
ATF_TC_HEAD(test_sigwait_eintr, tc)
{

	atf_tc_set_md_var(tc, "descr", "Check if sigwait exits with EINTR");
}

ATF_TC_BODY(test_sigwait_eintr, tc)
{
	timer_t timerid;
	sigset_t ss;
	int rv, sig, pid;

	support_signal(SIGUSR1, dummy_sig_handler);

	pid = support_create_sig_proc(SIGUSR1, 1, 400000);
	timerid = support_create_timer(0, 200000, false, true);

	sigemptyset(&ss);
	sigaddset(&ss, SIGUSR1);
	rv = sigwait(&ss, &sig);
	ATF_REQUIRE_EQ_MSG(0, rv,
	    "sigwait() should not fail: rv %d, errno %d", rv, errno);
	ATF_REQUIRE_EQ_MSG(SIGUSR1, sig,
	    "sigwait() should return SIGUSR1: rv %d, sig %d", rv, sig);
	ATF_REQUIRE(waitid(P_PID, pid, NULL, WEXITED) == 0);
	support_delete_timer(timerid);
}

ATF_TC(test_sigwaitinfo_eintr);
ATF_TC_HEAD(test_sigwaitinfo_eintr, tc)
{

	atf_tc_set_md_var(tc, "descr", "Check if sigwaitinfo exits with EINTR");
}

ATF_TC_BODY(test_sigwaitinfo_eintr, tc)
{
	timer_t timerid;
	sigset_t ss;
	int rv;

	timerid = support_create_timer(0, 100000000, false, true);

	sigemptyset(&ss);
	sigaddset(&ss, SIGUSR1);
	rv = sigwaitinfo(&ss, NULL);
	ATF_REQUIRE_EQ_MSG(-1, rv,
	    "sigwaitinfo() should fail, rv %d != -1", rv);
	ATF_REQUIRE_EQ_MSG(EINTR, errno,
	    "sigwaitinfo() should fail errno %d != EINTR", errno);
	support_delete_timer(timerid);
}

/*
 * Test kern.sig_discard_ign knob (default true).
 * See commit bc387624
 */
static void
test_sig_discard_ign(bool ignore)
{
	struct timespec ts;
	sigset_t mask;
	pid_t pid;
	int rv;

	support_signal(SIGUSR2, SIG_IGN);

	if (ignore)
		support_sysctlset("kern.sig_discard_ign", 1);
	else
		support_sysctlset("kern.sig_discard_ign", 0);

	sigemptyset(&mask);
	sigaddset(&mask, SIGUSR2);
	ATF_REQUIRE(sigprocmask(SIG_SETMASK, &mask, NULL) == 0);

	pid = support_create_sig_proc(SIGUSR2, 1, 100000);

	ts = make_timespec(1, 0);
	rv = sigtimedwait(&mask, NULL, &ts);
	if (ignore == true) {
		ATF_REQUIRE_EQ_MSG(-1, rv,
		    "sigtimedwait() ign=on should fail, rv %d != -1", rv);
		ATF_REQUIRE_EQ_MSG(EAGAIN, errno,
		    "sigtimedwait() ign=on should fail with EAGAIN errno %d",
		    errno);
	} else
		ATF_REQUIRE_EQ_MSG(SIGUSR2, rv,
		    "sigtimedwait() ign=off should return SIGUSR2, rv %d errno %d",
		    rv, errno);
	ATF_REQUIRE(waitid(P_PID, pid, NULL, WEXITED) == 0);
}

static void
support_check_siginfo(int code, int status, pid_t pid,
    siginfo_t *si, int sig)
{

	ATF_REQUIRE_EQ_MSG(sig, si->si_signo,
	    "check_siginfo: si_signo %d != sig %d", si->si_signo, sig);
	ATF_REQUIRE_EQ_MSG(code, si->si_code,
	    "check_siginfo: si_code %d != code %d", si->si_code, code);
	ATF_REQUIRE_EQ_MSG(status, si->si_status,
	    "check_siginfo: si_status %d != status %d", si->si_status, status);
	ATF_REQUIRE_EQ_MSG(pid, si->si_pid,
	    "check_siginfo: si_pid %d != pid %d", si->si_pid, pid);
}

static void
support_check_sigchld(sigset_t *set, int code, int status, pid_t pid,
    bool dequeue)
{
	siginfo_t si;
	int sig, kpid;

	if (dequeue == true)
		kpid = support_create_sig_proc(SIGUSR2, 1, 1000000);

	sig = sigwaitinfo(set, &si);
	if (dequeue == true) {
		ATF_REQUIRE_EQ_MSG(-1, sig,
		    "sigwaitinfo() should fail, sig %d != -1", sig);
		ATF_REQUIRE_EQ_MSG(EINTR, errno,
		    "sigwaitinfo() should fail errno %d != EINTR", errno);
	} else
		ATF_REQUIRE_EQ_MSG(SIGCHLD, sig,
		    "sigwaitinfo() %d != SIGCHLD", sig);
	if (dequeue == false)
		support_check_siginfo(code, status, pid, &si, SIGCHLD);
	if (dequeue == true)
		ATF_REQUIRE(waitid(P_PID, kpid, &si, WEXITED) == 0);
}

static void
test_child(void)
{

	raise(SIGSTOP);
	while (1)
		pause();
}

/*
 * Test kern.wait_dequeue_sigchld knob.
 */
static void
test_wait_dequeue_sigchld(bool dequeue)
{
	struct sigaction sa;
	siginfo_t si;
	sigset_t set;
	pid_t pid;

	sa.sa_flags = SA_SIGINFO | SA_RESTART;
	sa.sa_sigaction = dummy_sigchld;
	sigemptyset(&sa.sa_mask);
	ATF_REQUIRE(sigaction(SIGCHLD, &sa, NULL) == 0);

	support_signal(SIGUSR2, dummy_sig_handler);

	sigemptyset(&set);
	sigaddset(&set, SIGCHLD);
	ATF_REQUIRE(sigprocmask(SIG_BLOCK, &set, NULL) == 0);

	if (dequeue)
		support_sysctlset("kern.wait_dequeue_sigchld", 1);
	else
		support_sysctlset("kern.wait_dequeue_sigchld", 0);

	pid = fork();
	ATF_REQUIRE(pid >= 0);
	if (pid == 0) {
		test_child();
		exit(0);
	}

	bzero(&si, sizeof(si));
	ATF_REQUIRE(waitid(P_PID, pid, &si, WSTOPPED) == 0);

	support_check_siginfo(CLD_STOPPED, SIGSTOP, pid, &si, SIGCHLD);
	support_check_sigchld(&set, CLD_STOPPED, SIGSTOP, pid, dequeue);

	ATF_REQUIRE(kill(pid, SIGCONT) == 0);

	bzero(&si, sizeof(si));
	ATF_REQUIRE(waitid(P_PID, pid, &si, WCONTINUED) == 0);

	support_check_siginfo(CLD_CONTINUED, SIGCONT, pid, &si, SIGCHLD);
	support_check_sigchld(&set, CLD_CONTINUED, SIGCONT, pid, dequeue);

	ATF_REQUIRE(kill(pid, SIGKILL) == 0);

	bzero(&si, sizeof(si));
	ATF_REQUIRE(waitid(P_PID, pid, &si, WEXITED) == 0);

	support_check_siginfo(CLD_KILLED, SIGKILL, pid, &si, SIGCHLD);
}

ATF_TC_WITH_CLEANUP(test_sig_discard_ign_true);
ATF_TC_HEAD(test_sig_discard_ign_true, tc)
{

	atf_tc_set_md_var(tc, "require.user", "root");
	atf_tc_set_md_var(tc, "descr", "Test kern.sig_discard_ign on");
}

ATF_TC_BODY(test_sig_discard_ign_true, tc)
{

	test_sig_discard_ign(true);
}

ATF_TC_CLEANUP(test_sig_discard_ign_true, tc)
{

	support_sysctlset("kern.sig_discard_ign", 1);
}

ATF_TC_WITH_CLEANUP(test_sig_discard_ign_false);
ATF_TC_HEAD(test_sig_discard_ign_false, tc)
{

	atf_tc_set_md_var(tc, "require.user", "root");
	atf_tc_set_md_var(tc, "descr", "Test kern.sig_discard_ign off");
}

ATF_TC_BODY(test_sig_discard_ign_false, tc)
{

	test_sig_discard_ign(false);
}

ATF_TC_CLEANUP(test_sig_discard_ign_false, tc)
{

	support_sysctlset("kern.sig_discard_ign", 1);
}

ATF_TC_WITH_CLEANUP(test_wait_dequeue_sigchld_true);
ATF_TC_HEAD(test_wait_dequeue_sigchld_true, tc)
{

	atf_tc_set_md_var(tc, "require.user", "root");
	atf_tc_set_md_var(tc, "descr", "Test kern.wait_dequeue_sigchld on");
}

ATF_TC_BODY(test_wait_dequeue_sigchld_true, tc)
{

	test_wait_dequeue_sigchld(true);
}

ATF_TC_CLEANUP(test_wait_dequeue_sigchld_true, tc)
{

	support_sysctlset("kern.wait_dequeue_sigchld", 1);
}

ATF_TC_WITH_CLEANUP(test_wait_dequeue_sigchld_false);
ATF_TC_HEAD(test_wait_dequeue_sigchld_false, tc)
{

	atf_tc_set_md_var(tc, "require.user", "root");
	atf_tc_set_md_var(tc, "descr", "Test kern.wait_dequeue_sigchld off");
}

ATF_TC_BODY(test_wait_dequeue_sigchld_false, tc)
{

	test_wait_dequeue_sigchld(false);
}

ATF_TC_CLEANUP(test_wait_dequeue_sigchld_false, tc)
{

	support_sysctlset("kern.wait_dequeue_sigchld", 1);
}

ATF_TP_ADD_TCS(tp)
{

	ATF_TP_ADD_TC(tp, test_sigtimedwait_timeout_eagain0);
	ATF_TP_ADD_TC(tp, test_sigtimedwait_timeout_eagain1);
	ATF_TP_ADD_TC(tp, test_sigtimedwait_timeout_eagain2);
	ATF_TP_ADD_TC(tp, test_sigtimedwait_timeout_eagain3);

	ATF_TP_ADD_TC(tp, test_sigtimedwait_large_timeout_eintr);
	ATF_TP_ADD_TC(tp, test_sigtimedwait_infinity);

	ATF_TP_ADD_TC(tp, test_sigtimedwait_einval);

	ATF_TP_ADD_TC(tp, test_sigwait_eintr);
	ATF_TP_ADD_TC(tp, test_sigwaitinfo_eintr);

	ATF_TP_ADD_TC(tp, test_sig_discard_ign_true);
	ATF_TP_ADD_TC(tp, test_sig_discard_ign_false);

	ATF_TP_ADD_TC(tp, test_wait_dequeue_sigchld_true);
	ATF_TP_ADD_TC(tp, test_wait_dequeue_sigchld_false);

	return (atf_no_error());
}
