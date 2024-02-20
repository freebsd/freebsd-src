/*-
 * Copyright (c) 2023 The FreeBSD Foundation
 *
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * This software were developed by Konstantin Belousov <kib@FreeBSD.org>
 * under sponsorship from the FreeBSD Foundation.
 */

#include <sys/param.h>
#include <sys/syscall.h>
#include <sys/sysctl.h>

#include <atf-c.h>
#include <errno.h>
#include <signal.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdio.h>

static sig_atomic_t sigsys_cnt;

#define	SAVEDVALUE	"savedsignosys"

static void
sigsys_handler(int signo, siginfo_t *si, void *ucp)
{
	sigsys_cnt++;
}

static void
sigsys_test(int knob)
{
	struct sigaction sa;

	memset(&sa, 0, sizeof(sa));
	sa.sa_sigaction = sigsys_handler;
	sa.sa_flags = SA_SIGINFO;
	ATF_REQUIRE(sigaction(SIGSYS, &sa, NULL) == 0);

	ATF_REQUIRE(syscall(273) == -1);	/* reserved */
	ATF_CHECK_ERRNO(ENOSYS, true);
	atomic_signal_fence(memory_order_seq_cst);
	ATF_CHECK_EQ(1 * knob, sigsys_cnt * knob);

	ATF_REQUIRE(syscall(440) == -1);	/* SYS_kse_switchin */
	ATF_CHECK_ERRNO(ENOSYS, true);
	atomic_signal_fence(memory_order_seq_cst);
	ATF_CHECK_EQ(2 * knob, sigsys_cnt * knob);

	/* Hope this is enough for say next two months */
	ATF_REQUIRE(syscall(3000000) == -1);
	ATF_CHECK_ERRNO(ENOSYS, true);
	atomic_signal_fence(memory_order_seq_cst);
	ATF_CHECK_EQ(3 * knob, sigsys_cnt * knob);

	ATF_REQUIRE(syscall(SYS_afs3_syscall) == -1);
	ATF_CHECK_ERRNO(ENOSYS, true);
	atomic_signal_fence(memory_order_seq_cst);
	ATF_CHECK_EQ(4 * knob, sigsys_cnt * knob);
}

static void
sysctlset(const char *name, int val)
{
	size_t oldlen = sizeof(int);
	int oldval;
	char buf[80];

	ATF_REQUIRE(sysctlbyname(name, &oldval, &oldlen, NULL, 0) == 0);

	/* Store old %name in a symlink for cleanup */
	snprintf(buf, sizeof(buf), "%d", oldval);
	ATF_REQUIRE(symlink(buf, SAVEDVALUE) == 0);

	ATF_REQUIRE(sysctlbyname(name, NULL, NULL, &val, sizeof(val)) == 0);
}

static void
sysctlcleanup(const char *name)
{
	size_t oldlen;
	int n, oldval;
	char buf[80];

	if ((n = readlink(SAVEDVALUE, buf, sizeof(buf))) > 0) {
		buf[MIN((size_t)n, sizeof(buf) - 1)] = '\0';
		if (sscanf(buf, "%d", &oldval) == 1) {
			oldlen = sizeof(oldval);
			(void)sysctlbyname(name, NULL, 0,
			    &oldval, oldlen);
		}
	}
	(void)unlink(SAVEDVALUE);
}

ATF_TC_WITH_CLEANUP(sigsys_test_on);
ATF_TC_HEAD(sigsys_test_on, tc)
{
	atf_tc_set_md_var(tc, "require.user", "root");
	atf_tc_set_md_var(tc, "require.config", "allow_sysctl_side_effects");
	atf_tc_set_md_var(tc, "descr",
	    "Testing delivery of SIGSYS on invalid syscalls");
}

ATF_TC_BODY(sigsys_test_on, tc)
{
	sysctlset("kern.signosys", 1);
	sigsys_test(1);
}

ATF_TC_CLEANUP(sigsys_test_on, tc)
{
	sysctlcleanup("kern.signosys");
}

ATF_TC_WITH_CLEANUP(sigsys_test_off);
ATF_TC_HEAD(sigsys_test_off, tc)
{
	atf_tc_set_md_var(tc, "require.user", "root");
	atf_tc_set_md_var(tc, "require.config", "allow_sysctl_side_effects");
	atf_tc_set_md_var(tc, "descr",
	    "Testing SIGSYS silence on invalid syscalls");
}

ATF_TC_BODY(sigsys_test_off, tc)
{
	sysctlset("kern.signosys", 0);
	sigsys_test(0);
}

ATF_TC_CLEANUP(sigsys_test_off, tc)
{
	sysctlcleanup("kern.signosys");
}

ATF_TP_ADD_TCS(tp)
{
	ATF_TP_ADD_TC(tp, sigsys_test_on);
	ATF_TP_ADD_TC(tp, sigsys_test_off);
	return (atf_no_error());
}
