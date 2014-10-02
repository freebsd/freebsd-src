/* $NetBSD: t_sigqueue.c,v 1.4 2011/07/07 16:31:11 jruoho Exp $ */

/*-
 * Copyright (c) 2011 The NetBSD Foundation, Inc.
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
__RCSID("$NetBSD: t_sigqueue.c,v 1.4 2011/07/07 16:31:11 jruoho Exp $");


#include <atf-c.h>
#include <errno.h>
#include <signal.h>
#include <stdlib.h>
#include <sched.h>
#include <unistd.h>

static void	handler(int, siginfo_t *, void *);

#define VALUE (int)0xc001dad1
static int value;

static void
handler(int signo, siginfo_t *info, void *data)
{
	value = info->si_value.sival_int;
	kill(0, SIGINFO);
}

ATF_TC(sigqueue_basic);
ATF_TC_HEAD(sigqueue_basic, tc)
{
	atf_tc_set_md_var(tc, "descr", "Checks sigqueue(3) sigval delivery");
}

ATF_TC_BODY(sigqueue_basic, tc)
{
	struct sigaction sa;
	union sigval sv;

	sa.sa_sigaction = handler;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = SA_SIGINFO;

	if (sigaction(SIGUSR1, &sa, NULL) != 0)
		atf_tc_fail("sigaction failed");

	sv.sival_int = VALUE;

	if (sigqueue(0, SIGUSR1, sv) != 0)
		atf_tc_fail("sigqueue failed");

	sched_yield();
	ATF_REQUIRE_EQ(sv.sival_int, value);
}

ATF_TC(sigqueue_err);
ATF_TC_HEAD(sigqueue_err, tc)
{
	atf_tc_set_md_var(tc, "descr", "Test errors from sigqueue(3)");
}

ATF_TC_BODY(sigqueue_err, tc)
{
	union sigval sv;

	errno = 0;
	ATF_REQUIRE_ERRNO(EINVAL, sigqueue(getpid(), -1, sv) == -1);
}

ATF_TP_ADD_TCS(tp)
{

	ATF_TP_ADD_TC(tp, sigqueue_basic);
	ATF_TP_ADD_TC(tp, sigqueue_err);

	return atf_no_error();
}
