/* $NetBSD: t_sem.c,v 1.2 2010/11/08 13:05:49 njoly Exp $ */

/*
 * Copyright (c) 2008, 2010 The NetBSD Foundation, Inc.
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
 * Copyright (C) 2000 Jason Evans <jasone@freebsd.org>.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice(s), this list of conditions and the following disclaimer as
 *    the first lines of this file unmodified other than the possible
 *    addition of one or more copyright notices.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice(s), this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDER(S) ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT HOLDER(S) BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__COPYRIGHT("@(#) Copyright (c) 2008, 2010\
 The NetBSD Foundation, inc. All rights reserved.");
__RCSID("$NetBSD: t_sem.c,v 1.2 2010/11/08 13:05:49 njoly Exp $");

#include <sys/wait.h>

#include <errno.h>
#include <fcntl.h>
#include <semaphore.h>
#include <stdio.h>
#include <unistd.h>

#include <atf-c.h>

#define NCHILDREN 10

ATF_TC(basic);
ATF_TC_HEAD(basic, tc)
{
	atf_tc_set_md_var(tc, "descr", "Checks basic functionality of POSIX "
	    "semaphores");
}
ATF_TC_BODY(basic, tc)
{
	int val;
	sem_t *sem_b;

	if (sysconf(_SC_SEMAPHORES) == -1)
		atf_tc_skip("POSIX semaphores not supported");

	sem_b = sem_open("/sem_b", O_CREAT | O_EXCL, 0644, 0);
	ATF_REQUIRE(sem_b != SEM_FAILED);

	ATF_REQUIRE_EQ(sem_getvalue(sem_b, &val), 0);
	ATF_REQUIRE_EQ(val, 0);

	ATF_REQUIRE_EQ(sem_post(sem_b), 0);
	ATF_REQUIRE_EQ(sem_getvalue(sem_b, &val), 0);
	ATF_REQUIRE_EQ(val, 1);

	ATF_REQUIRE_EQ(sem_wait(sem_b), 0);
	ATF_REQUIRE_EQ(sem_trywait(sem_b), -1);
	ATF_REQUIRE_EQ(errno, EAGAIN);
	ATF_REQUIRE_EQ(sem_post(sem_b), 0);
	ATF_REQUIRE_EQ(sem_trywait(sem_b), 0);
	ATF_REQUIRE_EQ(sem_post(sem_b), 0);
	ATF_REQUIRE_EQ(sem_wait(sem_b), 0);
	ATF_REQUIRE_EQ(sem_post(sem_b), 0);

	ATF_REQUIRE_EQ(sem_close(sem_b), 0);
	ATF_REQUIRE_EQ(sem_unlink("/sem_b"), 0);
}

ATF_TC(child);
ATF_TC_HEAD(child, tc)
{
	atf_tc_set_md_var(tc, "descr", "Checks using semaphores to synchronize "
	    "parent with multiple child processes");
}
ATF_TC_BODY(child, tc)
{
	pid_t children[NCHILDREN];
	unsigned i, j;
	sem_t *sem_a;
	int status;

	pid_t pid;

	if (sysconf(_SC_SEMAPHORES) == -1)         
		atf_tc_skip("POSIX semaphores not supported");

	sem_a = sem_open("/sem_a", O_CREAT | O_EXCL, 0644, 0);
	ATF_REQUIRE(sem_a != SEM_FAILED);

	for (j = 1; j <= 2; j++) {
		for (i = 0; i < NCHILDREN; i++) {
			switch ((pid = fork())) {
			case -1:
				atf_tc_fail("fork() returned -1");
			case 0:
				printf("PID %d waiting for semaphore...\n",
				    getpid());
				ATF_REQUIRE_MSG(sem_wait(sem_a) == 0,
				    "sem_wait failed; iteration %d", j);
				printf("PID %d got semaphore\n", getpid());
				_exit(0);
			default:
				children[i] = pid;
				break;
			}
		}

		for (i = 0; i < NCHILDREN; i++) {
			sleep(1);
			printf("main loop %d: posting...\n", j);
			ATF_REQUIRE_EQ(sem_post(sem_a), 0);
		}

		for (i = 0; i < NCHILDREN; i++) {
			ATF_REQUIRE_EQ(waitpid(children[i], &status, 0), children[i]);
			ATF_REQUIRE(WIFEXITED(status));
			ATF_REQUIRE_EQ(WEXITSTATUS(status), 0);
		}
	}

	ATF_REQUIRE_EQ(sem_close(sem_a), 0);
	ATF_REQUIRE_EQ(sem_unlink("/sem_a"), 0);
}

ATF_TP_ADD_TCS(tp)
{

	ATF_TP_ADD_TC(tp, basic);
	ATF_TP_ADD_TC(tp, child);

	return atf_no_error();
}
