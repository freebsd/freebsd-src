/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2019 The FreeBSD Foundation
 *
 * This software was developed by BFF Storage Systems, LLC under sponsorship
 * from the FreeBSD Foundation.
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
 *
 * $FreeBSD$
 */

extern "C" {
#include <fcntl.h>
#include <pthread.h>
#include <semaphore.h>
}

#include "mockfs.hh"
#include "utils.hh"

using namespace testing;

/* Tests for orderly unmounts */
class Destroy: public FuseTest {};

/* Tests for unexpected deaths of the server */
class Death: public FuseTest{};

static void* open_th(void* arg) {
	int fd;
	const char *path = (const char*)arg;

	fd = open(path, O_RDONLY);
	EXPECT_EQ(-1, fd);
	EXPECT_EQ(ENOTCONN, errno);
	return 0;
}

/*
 * The server dies with unsent operations still on the message queue.
 * Check for any memory leaks like this:
 * 1) kldunload fusefs, if necessary
 * 2) kldload fusefs
 * 3) ./destroy --gtest_filter=Destroy.unsent_operations
 * 4) kldunload fusefs
 * 5) check /var/log/messages for anything like this:
Freed UMA keg (fuse_ticket) was not empty (31 items).  Lost 2 pages of memory.
Warning: memory type fuse_msgbuf leaked memory on destroy (68 allocations, 428800 bytes leaked).
 */
TEST_F(Death, unsent_operations)
{
	const char FULLPATH0[] = "mountpoint/some_file.txt";
	const char FULLPATH1[] = "mountpoint/other_file.txt";
	const char RELPATH0[] = "some_file.txt";
	const char RELPATH1[] = "other_file.txt";
	pthread_t th0, th1;
	ino_t ino0 = 42, ino1 = 43;
	sem_t sem;
	mode_t mode = S_IFREG | 0644;

	sem_init(&sem, 0, 0);

	EXPECT_LOOKUP(FUSE_ROOT_ID, RELPATH0)
	.WillRepeatedly(Invoke(
		ReturnImmediate([=](auto in __unused, auto& out) {
		SET_OUT_HEADER_LEN(out, entry);
		out.body.entry.attr.mode = mode;
		out.body.entry.nodeid = ino0;
		out.body.entry.attr.nlink = 1;
	})));
	EXPECT_LOOKUP(FUSE_ROOT_ID, RELPATH1)
	.WillRepeatedly(Invoke(
		ReturnImmediate([=](auto in __unused, auto& out) {
		SET_OUT_HEADER_LEN(out, entry);
		out.body.entry.attr.mode = mode;
		out.body.entry.nodeid = ino1;
		out.body.entry.attr.nlink = 1;
	})));

	EXPECT_CALL(*m_mock, process(
		ResultOf([&](auto in) {
			return (in.header.opcode == FUSE_OPEN);
		}, Eq(true)),
		_)
	).WillOnce(Invoke([&](auto in __unused, auto &out __unused) {
		sem_post(&sem);
		pause();
	}));

	/*
	 * One thread's operation will be sent to the daemon and block, and the
	 * other's will be stuck in the message queue.
	 */
	ASSERT_EQ(0, pthread_create(&th0, NULL, open_th,
		__DECONST(void*, FULLPATH0))) << strerror(errno);
	ASSERT_EQ(0, pthread_create(&th1, NULL, open_th,
		__DECONST(void*, FULLPATH1))) << strerror(errno);

	/* Wait for the first thread to block */
	sem_wait(&sem);
	/* Give the second thread time to block */
	nap();

	m_mock->kill_daemon();

	pthread_join(th0, NULL);
	pthread_join(th1, NULL);

	sem_destroy(&sem);
}

/*
 * On unmount the kernel should send a FUSE_DESTROY operation.  It should also
 * send FUSE_FORGET operations for all inodes with lookup_count > 0.
 */
TEST_F(Destroy, ok)
{
	const char FULLPATH[] = "mountpoint/some_file.txt";
	const char RELPATH[] = "some_file.txt";
	uint64_t ino = 42;

	expect_lookup(RELPATH, ino, S_IFREG | 0644, 0, 2);
	expect_forget(ino, 2);
	expect_destroy(0);

	/*
	 * access(2) the file to force a lookup.  Access it twice to double its
	 * lookup count.
	 */
	ASSERT_EQ(0, access(FULLPATH, F_OK)) << strerror(errno);
	ASSERT_EQ(0, access(FULLPATH, F_OK)) << strerror(errno);

	/*
	 * Unmount, triggering a FUSE_DESTROY and also causing a VOP_RECLAIM
	 * for every vnode on this mp, triggering FUSE_FORGET for each of them.
	 */
	m_mock->unmount();
}
