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
 * Test privilege associated with real time process settings.  There are
 * three relevant notions of privilege:
 *
 * - Privilege to set the real-time priority of the current process.
 * - Privilege to set the real-time priority of another process.
 * - Privilege to set the idle priority of another process.
 * - No privilege to set the idle priority of the current process.
 *
 * This requires a test process and a target (dummy) process running with
 * various uids.  This test is based on the code in the setpriority() test.
 */

#include <sys/types.h>
#include <sys/rtprio.h>
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
	struct rtprio rtp;
	int error;

	/*
	 * Tests first as root.  Test that we can set normal, realtime, and
	 * idle priorities on the current thread and on the dummy thread.
	 */
	rtp.type = RTP_PRIO_REALTIME;
	rtp.prio = 0;
	if (rtprio(RTP_SET, 0, &rtp) < 0)
		err(-1, "rtprio(RTP_SET, 0, {REALTIME, 0}) as root");

	rtp.type = RTP_PRIO_IDLE;
	rtp.prio = 0;
	if (rtprio(RTP_SET, 0, &rtp) < 0)
		err(-1, "rtprio(RTP_SET, 0, {IDLE, 0}) as root");

	rtp.type = RTP_PRIO_NORMAL;
	rtp.prio = 0;
	if (rtprio(RTP_SET, 0, &rtp) < 0)
		err(-1, "rtprio(RTP_SET, 0, {NORMAL, 0) as root");

	rtp.type = RTP_PRIO_REALTIME;
	rtp.prio = 0;
	if (rtprio(RTP_SET, dummy_pid, &rtp) < 0)
		err(-1, "rtprio(RTP_SET, %d, {REALTIME, 0}) as root",
		    dummy_pid);

	rtp.type = RTP_PRIO_IDLE;
	rtp.prio = 0;
	if (rtprio(RTP_SET, dummy_pid, &rtp) < 0)
		err(-1, "rtprio(RTP_SET, %d, {IDLE, 0}) as root", dummy_pid);

	rtp.type = RTP_PRIO_NORMAL;
	rtp.prio = 0;
	if (rtprio(RTP_SET, dummy_pid, &rtp) < 0)
		err(-1, "rtprio(RTP_SET, %d, {NORMAL, 0) as root",
		    dummy_pid);

	/*
	 * Then test again as a different credential.
	 */
	if (setresuid(UID_OTHER, UID_OTHER, UID_OTHER) < 0)
		err(-1, "setresuid(%d)", UID_OTHER);

	rtp.type = RTP_PRIO_REALTIME;
	rtp.prio = 0;
	error = rtprio(RTP_SET, 0, &rtp);
	if (error == 0)
		errx(-1,
		    "rtprio(RTP_SET, 0, {REALTIME, 0}) succeeded as !root");
	if (errno != EPERM)
		err(-1, "rtprio(RTP_SET, 0, {REALTIME, 0}) wrong errno %d as"
		    " !root", errno);

	rtp.type = RTP_PRIO_IDLE;
	rtp.prio = 0;
	error = rtprio(RTP_SET, 0, &rtp);
	if (error == 0)
		errx(-1, "rtprio(RTP_SET, 0, {IDLE, 0}) succeeded as !root");
	if (errno != EPERM)
		err(-1, "rtprio(RTP_SET, 0, {IDLE, 0}) wrong errno %d as "
		    "!root", errno);

	rtp.type = RTP_PRIO_NORMAL;
	rtp.prio = 0;
	if (rtprio(RTP_SET, 0, &rtp) < 0)
		err(-1, "rtprio(RTP_SET, 0, {NORMAL, 0}) as !root");

	rtp.type = RTP_PRIO_REALTIME;
	rtp.prio = 0;
	error = rtprio(RTP_SET, dummy_pid, &rtp);
	if (error == 0)
		errx(-1,
		    "rtprio(RTP_SET, %d, {REALTIME, 0}) succeeded as !root",
		    dummy_pid);
	if (errno != EPERM)
		err(-1, "rtprio(RTP_SET, %d, {REALTIME, 0}) wrong errno %d as"
		    " !root", dummy_pid, errno);

	rtp.type = RTP_PRIO_IDLE;
	rtp.prio = 0;
	error = rtprio(RTP_SET, dummy_pid, &rtp);
	if (error == 0)
		errx(-1, "rtprio(RTP_SET, %d, {IDLE, 0}) succeeded as !root",
		    dummy_pid);
	if (errno != EPERM)
		err(-1,
		    "rtprio(RTP_SET, %d, {IDLE, 0}) wrong errno %d as !root",
		    dummy_pid, errno);

	rtp.type = RTP_PRIO_NORMAL;
	rtp.prio = 0;
	error = rtprio(RTP_SET, dummy_pid, &rtp);
	if (error == 0)
		errx(-1,
		    "rtprio(RTP_SET, %d, {NORMAL, 0) succeeded as !root",
		    dummy_pid);
	if (errno != EPERM)
		err(-1, "rtprio(RTP_SET, %d, {NORMAL, 0}) wrong errno %d as "
		    "!root", dummy_pid, errno);

	exit(0);
}

void
priv_sched_rtprio(void)
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
