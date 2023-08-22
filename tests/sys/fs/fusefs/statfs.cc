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
 */

extern "C" {
#include <sys/param.h>
#include <sys/mount.h>
#include <semaphore.h>
}

#include "mockfs.hh"
#include "utils.hh"

using namespace testing;

class Statfs: public FuseTest {};

TEST_F(Statfs, eio)
{
	struct statfs statbuf;

	EXPECT_CALL(*m_mock, process(
		ResultOf([](auto in) {
			return (in.header.opcode == FUSE_STATFS);
		}, Eq(true)),
		_)
	).WillOnce(Invoke(ReturnErrno(EIO)));

	ASSERT_NE(0, statfs("mountpoint", &statbuf));
	ASSERT_EQ(EIO, errno);
}

/*
 * When the daemon is dead but the filesystem is still mounted, fuse(4) fakes
 * the statfs(2) response, which is necessary for unmounting.
 */
TEST_F(Statfs, enotconn)
{
	struct statfs statbuf;
	char mp[PATH_MAX];

	m_mock->kill_daemon();

	ASSERT_NE(nullptr, getcwd(mp, PATH_MAX)) << strerror(errno);
	strlcat(mp, "/mountpoint", PATH_MAX);
	ASSERT_EQ(0, statfs("mountpoint", &statbuf)) << strerror(errno);

	EXPECT_EQ(getuid(), statbuf.f_owner);
	EXPECT_EQ(0, strcmp("fusefs", statbuf.f_fstypename));
	EXPECT_EQ(0, strcmp("/dev/fuse", statbuf.f_mntfromname));
	EXPECT_EQ(0, strcmp(mp, statbuf.f_mntonname));
}

static void* statfs_th(void* arg) {
	ssize_t r;
	struct statfs *sb = (struct statfs*)arg;

	r = statfs("mountpoint", sb);
	if (r >= 0)
		return 0;
	else
		return (void*)(intptr_t)errno;
}

/*
 * Like the enotconn test, but in this case the daemon dies after we send the
 * FUSE_STATFS operation but before we get a response.
 */
TEST_F(Statfs, enotconn_while_blocked)
{
	struct statfs statbuf;
	void *thr0_value;
	pthread_t th0;
	char mp[PATH_MAX];
	sem_t sem;

	ASSERT_EQ(0, sem_init(&sem, 0, 0)) << strerror(errno);

	EXPECT_CALL(*m_mock, process(
		ResultOf([](auto in) {
			return (in.header.opcode == FUSE_STATFS);
		}, Eq(true)),
		_)
	).WillOnce(Invoke([&](auto in __unused, auto &out __unused) {
		sem_post(&sem);
		/* Just block until the daemon dies */
	}));

	ASSERT_NE(nullptr, getcwd(mp, PATH_MAX)) << strerror(errno);
	strlcat(mp, "/mountpoint", PATH_MAX);
	ASSERT_EQ(0, pthread_create(&th0, NULL, statfs_th, (void*)&statbuf))
		<< strerror(errno);

	ASSERT_EQ(0, sem_wait(&sem)) << strerror(errno);
	m_mock->kill_daemon();

	pthread_join(th0, &thr0_value);
	ASSERT_EQ(0, (intptr_t)thr0_value);

	EXPECT_EQ(getuid(), statbuf.f_owner);
	EXPECT_EQ(0, strcmp("fusefs", statbuf.f_fstypename));
	EXPECT_EQ(0, strcmp("/dev/fuse", statbuf.f_mntfromname));
	EXPECT_EQ(0, strcmp(mp, statbuf.f_mntonname));
}

TEST_F(Statfs, ok)
{
	struct statfs statbuf;
	char mp[PATH_MAX];

	EXPECT_CALL(*m_mock, process(
		ResultOf([](auto in) {
			return (in.header.opcode == FUSE_STATFS);
		}, Eq(true)),
		_)
	).WillOnce(Invoke(ReturnImmediate([=](auto in __unused, auto& out) {
		SET_OUT_HEADER_LEN(out, statfs);
		out.body.statfs.st.blocks = 1000;
		out.body.statfs.st.bfree = 100;
		out.body.statfs.st.bavail = 200;
		out.body.statfs.st.files = 5;
		out.body.statfs.st.ffree = 6;
		out.body.statfs.st.namelen = 128;
		out.body.statfs.st.frsize = 1024;
	})));

	ASSERT_NE(nullptr, getcwd(mp, PATH_MAX)) << strerror(errno);
	strlcat(mp, "/mountpoint", PATH_MAX);
	ASSERT_EQ(0, statfs("mountpoint", &statbuf)) << strerror(errno);
	EXPECT_EQ(1024ul, statbuf.f_bsize);
	/* 
	 * fuse(4) ignores the filesystem's reported optimal transfer size, and
	 * chooses a size that works well with the rest of the system instead
	 */
	EXPECT_EQ(1000ul, statbuf.f_blocks);
	EXPECT_EQ(100ul, statbuf.f_bfree);
	EXPECT_EQ(200l, statbuf.f_bavail);
	EXPECT_EQ(5ul, statbuf.f_files);
	EXPECT_EQ(6l, statbuf.f_ffree);
	EXPECT_EQ(128u, statbuf.f_namemax);
	EXPECT_EQ(getuid(), statbuf.f_owner);
	EXPECT_EQ(0, strcmp("fusefs", statbuf.f_fstypename));
	EXPECT_EQ(0, strcmp("/dev/fuse", statbuf.f_mntfromname));
	EXPECT_EQ(0, strcmp(mp, statbuf.f_mntonname));
}
