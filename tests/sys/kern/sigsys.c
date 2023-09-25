/*-
 * Copyright (c) 2023 The FreeBSD Foundation
 *
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * This software were developed by Konstantin Belousov <kib@FreeBSD.org>
 * under sponsorship from the FreeBSD Foundation.
 */

#include <sys/syscall.h>

#include <atf-c.h>
#include <errno.h>
#include <signal.h>
#include <stdatomic.h>
#include <stdbool.h>

static sig_atomic_t sigsys_cnt;

static void
sigsys_handler(int signo, siginfo_t *si, void *ucp)
{
	sigsys_cnt++;
}

ATF_TC(sigsys_test);

ATF_TC_HEAD(sigsys_test, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "Testing delivery of SIGSYS on invalid syscalls");
}

ATF_TC_BODY(sigsys_test, tc)
{
	struct sigaction sa;

	memset(&sa, 0, sizeof(sa));
	sa.sa_sigaction = sigsys_handler;
	sa.sa_flags = SA_SIGINFO;
	ATF_REQUIRE(sigaction(SIGSYS, &sa, NULL) == 0);

	ATF_REQUIRE(syscall(273) == -1);	/* reserved */
	ATF_CHECK_ERRNO(ENOSYS, true);
	atomic_signal_fence(memory_order_seq_cst);
	ATF_CHECK_EQ(1, sigsys_cnt);

	ATF_REQUIRE(syscall(440) == -1);	/* SYS_kse_switchin */
	ATF_CHECK_ERRNO(ENOSYS, true);
	atomic_signal_fence(memory_order_seq_cst);
	ATF_CHECK_EQ(2, sigsys_cnt);

	/* Hope this is enough for say next two months */
	ATF_REQUIRE(syscall(3000000) == -1);
	ATF_CHECK_ERRNO(ENOSYS, true);
	atomic_signal_fence(memory_order_seq_cst);
	ATF_CHECK_EQ(3, sigsys_cnt);

	ATF_REQUIRE(syscall(SYS_afs3_syscall) == -1);
	ATF_CHECK_ERRNO(ENOSYS, true);
	atomic_signal_fence(memory_order_seq_cst);
	ATF_CHECK_EQ(4, sigsys_cnt);
}

ATF_TP_ADD_TCS(tp)
{
	ATF_TP_ADD_TC(tp, sigsys_test);
	return (atf_no_error());
}
