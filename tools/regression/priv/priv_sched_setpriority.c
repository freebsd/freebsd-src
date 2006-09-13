/*-
 * Copyright (c) 2006 nCircle Network Security, Inc.
 * All rights reserved.
 *
 * This software was developed by Robert N. M. Watson for the TrustedBSD
 * Project under contract to nCircle Network Security, Inc.
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
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR, NCIRCLE NETWORK SECURITY,
 * INC., OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED
 * TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD$
 */

/*
 * Test that privilege is required to lower nice value; first test with, then
 * without.  There are two failure modes associated with privilege: the right
 * to renice a process with a different uid, and the right to renice to a
 * lower priority.  Because both the real and effective uid are part of the
 * permissions test, we have to create two children processes with different
 * uids.
 */

#include <sys/types.h>
#include <sys/resource.h>
#include <sys/wait.h>

#include <err.h>
#include <errno.h>
#include <signal.h>
#include <stdlib.h>
#include <unistd.h>

#include "main.h"

static void
dummy(void)
{

	while (1)
		sleep(1);
}

static void
collect(pid_t test_pid, pid_t dummy_pid)
{
	pid_t pid;

	/*
	 * First, collect the main test process.  When it has exited, then
	 * kill off the dummy process.
	 */
	if (test_pid > 0) {
		while (1) {
			pid = waitpid(test_pid, NULL, 0);
			if (pid == -1)
				warn("waitpid(%d (test), NULL, 0)", test_pid);
			if (pid == test_pid)
				break;
		}
	}

	if (kill(dummy_pid, SIGKILL) < 0)
		err(-1, "kill(%d, SIGKILL)", dummy_pid);

	while (1) {
		pid = waitpid(dummy_pid, NULL, 0);
		if (pid == -1)
			warn("waitpid(%d, NULL, 0)", dummy_pid);
		if (pid == dummy_pid)
			return;
	}
}

static void
test(pid_t dummy_pid)
{
	int error;

	/*
	 * Tests first as root.
	 */
	if (setpriority(PRIO_PROCESS, 0, -1) < 0)
		err(-1, "setpriority(PRIO_PROCESS, 0, -1) as root");

	if (setpriority(PRIO_PROCESS, dummy_pid, -1) < 0)
		err(-1, "setpriority(PRIO_PROCESS, %d, -1) as root",
		    dummy_pid);

	/*
	 * Then test again as a different credential.
	 */
	if (setresuid(UID_OTHER, UID_OTHER, UID_OTHER) < 0)
		err(-1, "setresuid(%d)", UID_OTHER);

	error = setpriority(PRIO_PROCESS, 0, -2);
	if (error == 0)
		errx(-1,
		    "setpriority(PRIO_PROCESS, 0, -2) succeeded as !root");
	if (errno != EACCES)
		err(-1, "setpriority(PRIO_PROCESS, 0, 2) wrong errno %d as "
		    "!root", errno);

	error = setpriority(PRIO_PROCESS, dummy_pid, -2);
	if (error == 0)
		errx(-1,
		    "setpriority(PRIO_PROCESS, %d, -2) succeeded as !root",
		    dummy_pid);
	if (errno != EPERM)
		err(-1, "setpriority(PRIO_PROCESS, %d, 2) wrong errno %d as "
		    "!root", dummy_pid, errno);

	exit(0);
}

void
priv_sched_setpriority(void)
{
	pid_t dummy_pid, test_pid;

	assert_root();

	/*
	 * Set up dummy process, which we will kill before exiting.
	 */
	dummy_pid = fork();
	if (dummy_pid < 0)
		err(-1, "fork - dummy");
	if (dummy_pid == 0) {
		if (setresuid(UID_THIRD, UID_THIRD, UID_THIRD) < 0)
			err(-1, "setresuid(%d)", UID_THIRD);
		dummy();
	}
	sleep(1);	/* Allow dummy thread to change uids. */

	test_pid = fork();
	if (test_pid < 0) {
		warn("fork - test");
		collect(-1, dummy_pid);
		return;
	}
	if (test_pid == 0)
		test(dummy_pid);

	collect(test_pid, dummy_pid);
}
