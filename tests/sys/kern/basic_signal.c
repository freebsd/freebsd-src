/*-
 * Copyright (c) 2021 M. Warner Losh <imp@FreeBSD.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <atf-c.h>
#include <errno.h>
#include <signal.h>
#include <stdbool.h>
#include <stdlib.h>

#if defined(__aarch64__)
#include <machine/armreg.h>
#define	SET_TRACE_FLAG(ucp)	(ucp)->uc_mcontext.mc_gpregs.gp_spsr |= PSR_SS
#define	CLR_TRACE_FLAG(ucp)	(ucp)->uc_mcontext.mc_gpregs.gp_spsr &= ~PSR_SS
#elif defined(__amd64__)
#include <machine/psl.h>
#define	SET_TRACE_FLAG(ucp)	(ucp)->uc_mcontext.mc_rflags |= PSL_T
#define	CLR_TRACE_FLAG(ucp)	(ucp)->uc_mcontext.mc_rflags &= ~PSL_T
#elif defined(__i386__)
#include <machine/psl.h>
#define	SET_TRACE_FLAG(ucp)	(ucp)->uc_mcontext.mc_eflags |= PSL_T
#define	CLR_TRACE_FLAG(ucp)	(ucp)->uc_mcontext.mc_eflags &= ~PSL_T
#endif

static volatile sig_atomic_t signal_fired = 0;

static void
sig_handler(int signo, siginfo_t *info __unused, void *ucp __unused)
{
	signal_fired++;
}

ATF_TC(signal_test);

ATF_TC_HEAD(signal_test, tc)
{

	atf_tc_set_md_var(tc, "descr", "Testing delivery of a signal");
}

ATF_TC_BODY(signal_test, tc)
{
	/*
	 * Setup the signal handlers
	 */
	struct sigaction sa = {
		.sa_sigaction = sig_handler,
		.sa_flags = SA_SIGINFO,
	};
	ATF_REQUIRE(sigemptyset(&sa.sa_mask) == 0);
	ATF_REQUIRE(sigaction(SIGUSR1, &sa, NULL) == 0);
	ATF_REQUIRE(sigaction(SIGUSR2, &sa, NULL) == 0);
	ATF_REQUIRE(sigaction(SIGALRM, &sa, NULL) == 0);

	/*
	 * Fire SIGUSR1
	 */
	ATF_CHECK(signal_fired == 0);
	ATF_REQUIRE(raise(SIGUSR1) == 0);
	ATF_CHECK(signal_fired == 1);

	/*
	 * Fire SIGUSR2
	 */
	ATF_REQUIRE(raise(SIGUSR2) == 0);
	ATF_CHECK(signal_fired == 2);

	/*
	 * Fire SIGALRM after a timeout
	 */
	ATF_REQUIRE(alarm(1) == 0);
	ATF_REQUIRE(pause() == -1);
	ATF_REQUIRE(errno == EINTR);
	ATF_CHECK(signal_fired == 3);
}

/*
 * Check setting the machine dependent single step flag works when supported.
 */
#ifdef SET_TRACE_FLAG
static volatile sig_atomic_t trap_signal_fired = 0;

static void
trap_sig_handler(int signo, siginfo_t *info __unused, void *_ucp)
{
	ucontext_t *ucp = _ucp;

	if (trap_signal_fired < 9) {
		SET_TRACE_FLAG(ucp);
	} else {
		CLR_TRACE_FLAG(ucp);
	}
	trap_signal_fired++;
}

ATF_TC(trap_signal_test);

ATF_TC_HEAD(trap_signal_test, tc)
{

	atf_tc_set_md_var(tc, "descr",
	    "Testing signal handler setting the MD single step flag");
}

ATF_TC_BODY(trap_signal_test, tc)
{
	/*
	 * Setup the signal handlers
	 */
	struct sigaction sa = {
		.sa_sigaction = trap_sig_handler,
		.sa_flags = SA_SIGINFO,
	};
	ATF_REQUIRE(sigemptyset(&sa.sa_mask) == 0);
	ATF_REQUIRE(sigaction(SIGTRAP, &sa, NULL) == 0);

	/*
	 * Fire SIGTRAP
	 */
	ATF_CHECK(trap_signal_fired == 0);
	ATF_REQUIRE(raise(SIGTRAP) == 0);
	ATF_CHECK(trap_signal_fired == 10);
}
#endif

/*
 * Special tests for 32-bit arm. We can call thumb code (really just t32) from
 * normal (a32) mode and vice versa. Likewise, signals can interrupt a T32
 * context with A32 code and vice versa. Make sure these all work with a simple
 * test that raises the signal and ensures that it executed. No other platform
 * has these requirements. Also note: we only support thumb2, so there's no T16
 * vs T32 issues we have to test for.
 */
#ifdef __arm__

#define a32_isa __attribute__((target("arm")))
#define t32_isa __attribute__((target("thumb")))

static volatile sig_atomic_t t32_fired = 0;
static volatile sig_atomic_t a32_fired = 0;

a32_isa static void
sig_a32(int signo, siginfo_t *info __unused, void *ucp __unused)
{
	a32_fired++;
}

t32_isa static void
sig_t32(int signo, siginfo_t *info __unused, void *ucp __unused)
{
	t32_fired++;
}


ATF_TC(signal_test_T32_to_A32);

ATF_TC_HEAD(signal_test_T32_to_A32, tc)
{

	atf_tc_set_md_var(tc, "descr", "Testing delivery of a signal from T32 to A32");
}

t32_isa ATF_TC_BODY(signal_test_T32_to_A32, tc)
{
	/*
	 * Setup the signal handlers
	 */
	struct sigaction sa = {
		.sa_sigaction = sig_a32,
		.sa_flags = SA_SIGINFO,
	};
	ATF_REQUIRE(sigemptyset(&sa.sa_mask) == 0);
	ATF_REQUIRE(sigaction(SIGUSR1, &sa, NULL) == 0);

	ATF_REQUIRE((((uintptr_t)sig_a32) & 1) == 0); /* Make sure compiled as not thumb */

	ATF_CHECK(a32_fired == 0);
	ATF_REQUIRE(raise(SIGUSR1) == 0);
	ATF_CHECK(a32_fired == 1);
}

ATF_TC(signal_test_A32_to_T32);

ATF_TC_HEAD(signal_test_A32_to_T32, tc)
{

	atf_tc_set_md_var(tc, "descr", "Testing delivery of a signal from A32 to T32");
}

a32_isa ATF_TC_BODY(signal_test_A32_to_T32, tc)
{
	/*
	 * Setup the signal handlers
	 */
	struct sigaction sa = {
		.sa_sigaction = sig_t32,
		.sa_flags = SA_SIGINFO,
	};
	ATF_REQUIRE(sigemptyset(&sa.sa_mask) == 0);
	ATF_REQUIRE(sigaction(SIGUSR1, &sa, NULL) == 0);

	ATF_REQUIRE((((uintptr_t)sig_t32) & 1) == 1);	/* Make sure compiled as thumb */

	ATF_CHECK(t32_fired == 0);
	ATF_REQUIRE(raise(SIGUSR1) == 0);
	ATF_CHECK(t32_fired == 1);
}
#endif

ATF_TP_ADD_TCS(tp)
{

	ATF_TP_ADD_TC(tp, signal_test);
#ifdef SET_TRACE_FLAG
	ATF_TP_ADD_TC(tp, trap_signal_test);
#endif
#ifdef __arm__
	ATF_TP_ADD_TC(tp, signal_test_T32_to_A32);
	ATF_TP_ADD_TC(tp, signal_test_A32_to_T32);
#endif

	return (atf_no_error());
}
