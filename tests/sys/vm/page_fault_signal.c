/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2019 Jilles Tjoelker
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
 *
 * $FreeBSD$
 */

#include <sys/mman.h>

#include <atf-c.h>
#include <fcntl.h>
#include <setjmp.h>
#include <signal.h>
#include <stdio.h>

static sigjmp_buf sig_env;
static volatile int last_sig, last_code;

static void
sighandler(int sig, siginfo_t *info, void *context __unused)
{

	last_sig = sig;
	last_code = info->si_code;
	siglongjmp(sig_env, 1);
}

static void
setup_signals(void)
{
	struct sigaction sa;
	int r;

	sa.sa_sigaction = sighandler;
	sa.sa_flags = SA_RESTART | SA_RESETHAND | SA_SIGINFO;
	r = sigfillset(&sa.sa_mask);
	ATF_REQUIRE(r != -1);
	r = sigaction(SIGILL, &sa, NULL);
	ATF_REQUIRE(r != -1);
	r = sigaction(SIGBUS, &sa, NULL);
	ATF_REQUIRE(r != -1);
	r = sigaction(SIGSEGV, &sa, NULL);
	ATF_REQUIRE(r != -1);
}

ATF_TC_WITHOUT_HEAD(page_fault_signal__segv_maperr_1);
ATF_TC_BODY(page_fault_signal__segv_maperr_1, tc)
{
	int *p;
	int r;
	int sz;

	sz = getpagesize();
	p = mmap(NULL, sz, PROT_READ, MAP_ANON, -1, 0);
	ATF_REQUIRE(p != MAP_FAILED);
	r = munmap(p, sz);
	ATF_REQUIRE(r != -1);
	if (sigsetjmp(sig_env, 1) == 0) {
		setup_signals();
		*(volatile int *)p = 1;
	}
	ATF_CHECK_EQ(SIGSEGV, last_sig);
	ATF_CHECK_EQ(SEGV_MAPERR, last_code);
}

ATF_TC_WITHOUT_HEAD(page_fault_signal__segv_accerr_1);
ATF_TC_BODY(page_fault_signal__segv_accerr_1, tc)
{
	int *p;
	int sz;

	sz = getpagesize();
	p = mmap(NULL, sz, PROT_READ, MAP_ANON, -1, 0);
	ATF_REQUIRE(p != MAP_FAILED);
	if (sigsetjmp(sig_env, 1) == 0) {
		setup_signals();
		*(volatile int *)p = 1;
	}
	(void)munmap(p, sz);
	ATF_CHECK_EQ(SIGSEGV, last_sig);
	ATF_CHECK_EQ(SEGV_ACCERR, last_code);
}

ATF_TC_WITHOUT_HEAD(page_fault_signal__segv_accerr_2);
ATF_TC_BODY(page_fault_signal__segv_accerr_2, tc)
{
	int *p;
	int sz;

	sz = getpagesize();
	p = mmap(NULL, sz, PROT_NONE, MAP_ANON, -1, 0);
	ATF_REQUIRE(p != MAP_FAILED);
	if (sigsetjmp(sig_env, 1) == 0) {
		setup_signals();
		(void)*(volatile int *)p;
	}
	(void)munmap(p, sz);
	ATF_CHECK_EQ(SIGSEGV, last_sig);
	ATF_CHECK_EQ(SEGV_ACCERR, last_code);
}

ATF_TC_WITHOUT_HEAD(page_fault_signal__bus_objerr_1);
ATF_TC_BODY(page_fault_signal__bus_objerr_1, tc)
{
	int *p;
	int fd;
	int sz;

	sz = getpagesize();
	fd = shm_open(SHM_ANON, O_RDWR | O_CREAT, 0600);
	ATF_REQUIRE(fd != -1);
	p = mmap(NULL, sz, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	ATF_REQUIRE(p != MAP_FAILED);
	if (sigsetjmp(sig_env, 1) == 0) {
		setup_signals();
		*(volatile int *)p = 1;
	}
	(void)munmap(p, sz);
	(void)close(fd);
	ATF_CHECK_EQ(SIGBUS, last_sig);
	ATF_CHECK_EQ(BUS_OBJERR, last_code);
}

ATF_TC_WITHOUT_HEAD(page_fault_signal__bus_objerr_2);
ATF_TC_BODY(page_fault_signal__bus_objerr_2, tc)
{
	int *p;
	int fd;
	int r;
	int sz;

	sz = getpagesize();
	fd = shm_open(SHM_ANON, O_RDWR | O_CREAT, 0600);
	ATF_REQUIRE(fd != -1);
	r = ftruncate(fd, sz);
	ATF_REQUIRE(r != -1);
	p = mmap(NULL, sz * 2, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	ATF_REQUIRE(p != MAP_FAILED);
	if (sigsetjmp(sig_env, 1) == 0) {
		setup_signals();
		((volatile int *)p)[sz / sizeof(int)] = 1;
	}
	(void)munmap(p, sz * 2);
	(void)close(fd);
	ATF_CHECK_EQ(SIGBUS, last_sig);
	ATF_CHECK_EQ(BUS_OBJERR, last_code);
}

ATF_TP_ADD_TCS(tp)
{

	ATF_TP_ADD_TC(tp, page_fault_signal__segv_maperr_1);
	ATF_TP_ADD_TC(tp, page_fault_signal__segv_accerr_1);
	ATF_TP_ADD_TC(tp, page_fault_signal__segv_accerr_2);
	ATF_TP_ADD_TC(tp, page_fault_signal__bus_objerr_1);
	ATF_TP_ADD_TC(tp, page_fault_signal__bus_objerr_2);

	return (atf_no_error());
}
