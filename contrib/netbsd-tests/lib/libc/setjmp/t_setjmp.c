/* $NetBSD: t_setjmp.c,v 1.2 2017/01/14 21:08:17 christos Exp $ */

/*-
 * Copyright (c) 2008 The NetBSD Foundation, Inc.
 * All rights reserved.
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

/*
 * Copyright (c) 1994 Christopher G. Demetriou
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *          This product includes software developed for the
 *          NetBSD Project.  See http://www.NetBSD.org/ for
 *          information about NetBSD.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 * 
 * <<Id: LICENSE,v 1.2 2000/06/14 15:57:33 cgd Exp>>
 */

#include <sys/cdefs.h>
__COPYRIGHT("@(#) Copyright (c) 2008\
 The NetBSD Foundation, inc. All rights reserved.");
__RCSID("$NetBSD: t_setjmp.c,v 1.2 2017/01/14 21:08:17 christos Exp $");

#include <sys/types.h>

#include <errno.h>
#include <setjmp.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <atf-c.h>

#define REQUIRE_ERRNO(x) ATF_REQUIRE_MSG(x, "%s", strerror(errno))

#define TEST_SETJMP 0
#define TEST_U_SETJMP 1
#define TEST_SIGSETJMP_SAVE 2
#define TEST_SIGSETJMP_NOSAVE 3
#define TEST_LONGJMP_ZERO 4
#define TEST_U_LONGJMP_ZERO 5

static int expectsignal;

static void
aborthandler(int signo __unused)
{
	ATF_REQUIRE_MSG(expectsignal, "kill(SIGABRT) succeeded");
	atf_tc_pass();
}

static void
h_check(int test)
{
	struct sigaction sa;
	jmp_buf jb;
	sigjmp_buf sjb;
	sigset_t ss;
	int i, x;
	volatile bool did_longjmp;

	i = getpid();
	did_longjmp = false;

	if (test == TEST_SETJMP || test == TEST_SIGSETJMP_SAVE ||
	    test == TEST_LONGJMP_ZERO)
		expectsignal = 0;
	else if (test == TEST_U_SETJMP || test == TEST_SIGSETJMP_NOSAVE ||
	    test == TEST_U_LONGJMP_ZERO)
		expectsignal = 1;
	else
		atf_tc_fail("unknown test");

	sa.sa_handler = aborthandler;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = 0;
	REQUIRE_ERRNO(sigaction(SIGABRT, &sa, NULL) != -1);
	REQUIRE_ERRNO(sigemptyset(&ss) != -1);
	REQUIRE_ERRNO(sigaddset(&ss, SIGABRT) != -1);
	REQUIRE_ERRNO(sigprocmask(SIG_BLOCK, &ss, NULL) != -1);

	if (test == TEST_SETJMP || test == TEST_LONGJMP_ZERO)
		x = setjmp(jb);
	else if (test == TEST_U_SETJMP || test == TEST_U_LONGJMP_ZERO)
		x = _setjmp(jb);
	else 
		x = sigsetjmp(sjb, !expectsignal);

	if (x != 0) {
		if (test == TEST_LONGJMP_ZERO || test == TEST_U_LONGJMP_ZERO)
			ATF_REQUIRE_MSG(x == 1, "setjmp returned wrong value");
		else
			ATF_REQUIRE_MSG(x == i, "setjmp returned wrong value");

		kill(i, SIGABRT);
		ATF_REQUIRE_MSG(!expectsignal, "kill(SIGABRT) failed");
		atf_tc_pass();
	} else if (did_longjmp) {
		atf_tc_fail("setjmp returned zero after longjmp");
	}

	REQUIRE_ERRNO(sigprocmask(SIG_UNBLOCK, &ss, NULL) != -1);

	did_longjmp = true;
	if (test == TEST_SETJMP)
		longjmp(jb, i);
	else if (test == TEST_LONGJMP_ZERO)
		longjmp(jb, 0);
	else if (test == TEST_U_SETJMP)
		_longjmp(jb, i);
	else if (test == TEST_U_LONGJMP_ZERO)
		_longjmp(jb, 0);
	else 
		siglongjmp(sjb, i);

	atf_tc_fail("jmp failed");
}

ATF_TC(setjmp);
ATF_TC_HEAD(setjmp, tc)
{
	atf_tc_set_md_var(tc, "descr", "Checks setjmp(3)");
}
ATF_TC_BODY(setjmp, tc)
{
	h_check(TEST_SETJMP);
}

ATF_TC(_setjmp);
ATF_TC_HEAD(_setjmp, tc)
{
	atf_tc_set_md_var(tc, "descr", "Checks _setjmp(3)");
}
ATF_TC_BODY(_setjmp, tc)
{
	h_check(TEST_U_SETJMP);
}

ATF_TC(sigsetjmp_save);
ATF_TC_HEAD(sigsetjmp_save, tc)
{
	atf_tc_set_md_var(tc, "descr", "Checks sigsetjmp(3) with savemask enabled");
}
ATF_TC_BODY(sigsetjmp_save, tc)
{
	h_check(TEST_SIGSETJMP_SAVE);
}

ATF_TC(sigsetjmp_nosave);
ATF_TC_HEAD(sigsetjmp_nosave, tc)
{
	atf_tc_set_md_var(tc, "descr", "Checks sigsetjmp(3) with savemask disabled");
}
ATF_TC_BODY(sigsetjmp_nosave, tc)
{
	h_check(TEST_SIGSETJMP_NOSAVE);
}

ATF_TC(longjmp_zero);
ATF_TC_HEAD(longjmp_zero, tc)
{
	atf_tc_set_md_var(tc, "descr", "Checks longjmp(3) with a zero value");
}
ATF_TC_BODY(longjmp_zero, tc)
{
	h_check(TEST_LONGJMP_ZERO);
}

ATF_TC(_longjmp_zero);
ATF_TC_HEAD(_longjmp_zero, tc)
{
	atf_tc_set_md_var(tc, "descr", "Checks _longjmp(3) with a zero value");
}
ATF_TC_BODY(_longjmp_zero, tc)
{
	h_check(TEST_U_LONGJMP_ZERO);
}

ATF_TP_ADD_TCS(tp)
{
	ATF_TP_ADD_TC(tp, setjmp);
	ATF_TP_ADD_TC(tp, _setjmp);
	ATF_TP_ADD_TC(tp, sigsetjmp_save);
	ATF_TP_ADD_TC(tp, sigsetjmp_nosave);
	ATF_TP_ADD_TC(tp, longjmp_zero);
	ATF_TP_ADD_TC(tp, _longjmp_zero);

	return atf_no_error();
}
