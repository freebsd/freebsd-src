/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2025 ConnectWise
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
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

extern "C" {
#include <sys/param.h>
#include <sys/mount.h>
#include <sys/signal.h>
#include <sys/wait.h>

#include <fcntl.h>
#include <pthread.h>
#include <semaphore.h>
#include <signal.h>
}

#include "mockfs.hh"
#include "utils.hh"

using namespace testing;

/* Tests for behavior that happens before the server responds to FUSE_INIT */
class PreInit: public FuseTest {
void SetUp() {
	m_no_auto_init = true;
	FuseTest::SetUp();
}
};

static void* unmount1(void* arg __unused) {
	ssize_t r;

	r = unmount("mountpoint", 0);
	if (r >= 0)
		return 0;
	else
		return (void*)(intptr_t)errno;
}

/*
 * Attempting to unmount the file system before it fully initializes should
 * work fine.  The unmount will complete after initialization does.
 */
TEST_F(PreInit, unmount_before_init)
{
	sem_t sem0;
	pthread_t th1;

	ASSERT_EQ(0, sem_init(&sem0, 0, 0)) << strerror(errno);

	EXPECT_CALL(*m_mock, process(
		ResultOf([](auto in) {
			return (in.header.opcode == FUSE_INIT);
		}, Eq(true)),
		_)
	).WillOnce(Invoke(ReturnImmediate([&](auto in, auto& out) {
		SET_OUT_HEADER_LEN(out, init);
		out.body.init.major = FUSE_KERNEL_VERSION;
		out.body.init.minor = FUSE_KERNEL_MINOR_VERSION;
		out.body.init.flags = in.body.init.flags & m_init_flags;
		out.body.init.max_write = m_maxwrite;
		out.body.init.max_readahead = m_maxreadahead;
		out.body.init.time_gran = m_time_gran;
		sem_wait(&sem0);
	})));
	expect_destroy(0);

	ASSERT_EQ(0, pthread_create(&th1, NULL, unmount1, NULL));
	nap();	/* Wait for th1 to block in unmount() */
	sem_post(&sem0);
	/* The daemon will quit after receiving FUSE_DESTROY */
	m_mock->join_daemon();
}

/*
 * Don't panic in this very specific scenario:
 *
 * The server does not respond to FUSE_INIT in timely fashion.
 * Some other process tries to do unmount.
 * That other process gets killed by a signal.
 * The server finally responds to FUSE_INIT.
 *
 * Regression test for bug 287438
 * https://bugs.freebsd.org/bugzilla/show_bug.cgi?id=287438
 */
TEST_F(PreInit, signal_during_unmount_before_init)
{
	sem_t sem0;
	pid_t child;

	ASSERT_EQ(0, sem_init(&sem0, 0, 0)) << strerror(errno);

	EXPECT_CALL(*m_mock, process(
		ResultOf([](auto in) {
			return (in.header.opcode == FUSE_INIT);
		}, Eq(true)),
		_)
	).WillOnce(Invoke(ReturnImmediate([&](auto in, auto& out) {
		SET_OUT_HEADER_LEN(out, init);
		out.body.init.major = FUSE_KERNEL_VERSION;
		/*
		 * Use protocol 7.19, like libfuse2 does.  The server must use
		 * protocol 7.27 or older to trigger the bug.
		 */
		out.body.init.minor = 19;
		out.body.init.flags = in.body.init.flags & m_init_flags;
		out.body.init.max_write = m_maxwrite;
		out.body.init.max_readahead = m_maxreadahead;
		out.body.init.time_gran = m_time_gran;
		sem_wait(&sem0);
	})));

	if ((child = ::fork()) == 0) {
		/*
		 * In child.  This will block waiting for FUSE_INIT to complete
		 * or the receipt of an asynchronous signal.
		 */
		(void) unmount("mountpoint", 0);
		_exit(0);	/* Unreachable, unless parent dies after fork */
	} else if (child > 0) {
		/* In parent.  Wait for child process to start, then kill it */
		nap();
		kill(child, SIGINT);
		waitpid(child, NULL, WEXITED);
	} else {
		FAIL() << strerror(errno);
	}
	m_mock->m_quit = true;	/* Since we are by now unmounted. */
	sem_post(&sem0);
	m_mock->join_daemon();
}
