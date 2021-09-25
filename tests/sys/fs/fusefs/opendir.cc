/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
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
#include <dirent.h>

#include <fcntl.h>
#include <semaphore.h>
}

#include "mockfs.hh"
#include "utils.hh"

using namespace testing;

class Opendir: public FuseTest {
public:
void expect_lookup(const char *relpath, uint64_t ino)
{
	FuseTest::expect_lookup(relpath, ino, S_IFDIR | 0755, 0, 1);
}

void expect_opendir(uint64_t ino, uint32_t flags, ProcessMockerT r)
{
	/* opendir(3) calls fstatfs */
	EXPECT_CALL(*m_mock, process(
		ResultOf([](auto in) {
			return (in.header.opcode == FUSE_STATFS);
		}, Eq(true)),
		_)
	).WillRepeatedly(Invoke(ReturnImmediate([=](auto i __unused, auto& out) {
		SET_OUT_HEADER_LEN(out, statfs);
	})));

	EXPECT_CALL(*m_mock, process(
		ResultOf([=](auto in) {
			return (in.header.opcode == FUSE_OPENDIR &&
				in.header.nodeid == ino &&
				in.body.opendir.flags == flags);
		}, Eq(true)),
		_)
	).WillOnce(Invoke(r));
}

};

class OpendirNoOpendirSupport: public Opendir {
	virtual void SetUp() {
		m_init_flags = FUSE_NO_OPENDIR_SUPPORT;
		FuseTest::SetUp();
	}
};


/* 
 * The fuse daemon fails the request with enoent.  This usually indicates a
 * race condition: some other FUSE client removed the file in between when the
 * kernel checked for it with lookup and tried to open it
 */
TEST_F(Opendir, enoent)
{
	const char FULLPATH[] = "mountpoint/some_dir";
	const char RELPATH[] = "some_dir";
	uint64_t ino = 42;
	sem_t sem;

	ASSERT_EQ(0, sem_init(&sem, 0, 0)) << strerror(errno);

	expect_lookup(RELPATH, ino);
	expect_opendir(ino, O_RDONLY, ReturnErrno(ENOENT));
	// Since FUSE_OPENDIR returns ENOENT, the kernel will reclaim the vnode
	// and send a FUSE_FORGET
	expect_forget(ino, 1, &sem);

	ASSERT_EQ(-1, open(FULLPATH, O_DIRECTORY));
	EXPECT_EQ(ENOENT, errno);

	sem_wait(&sem);
	sem_destroy(&sem);
}

/* 
 * The daemon is responsible for checking file permissions (unless the
 * default_permissions mount option was used)
 */
TEST_F(Opendir, eperm)
{
	const char FULLPATH[] = "mountpoint/some_dir";
	const char RELPATH[] = "some_dir";
	uint64_t ino = 42;

	expect_lookup(RELPATH, ino);
	expect_opendir(ino, O_RDONLY, ReturnErrno(EPERM));

	EXPECT_EQ(-1, open(FULLPATH, O_DIRECTORY));
	EXPECT_EQ(EPERM, errno);
}

TEST_F(Opendir, open)
{
	const char FULLPATH[] = "mountpoint/some_dir";
	const char RELPATH[] = "some_dir";
	uint64_t ino = 42;
	int fd;

	expect_lookup(RELPATH, ino);
	expect_opendir(ino, O_RDONLY,
	ReturnImmediate([=](auto in __unused, auto& out) {
		SET_OUT_HEADER_LEN(out, open);
	}));

	fd = open(FULLPATH, O_DIRECTORY);
	ASSERT_LE(0, fd) << strerror(errno);

	leak(fd);
}

/* Directories can be opened O_EXEC for stuff like fchdir(2) */
TEST_F(Opendir, open_exec)
{
	const char FULLPATH[] = "mountpoint/some_dir";
	const char RELPATH[] = "some_dir";
	uint64_t ino = 42;
	int fd;

	expect_lookup(RELPATH, ino);
	expect_opendir(ino, O_EXEC,
	ReturnImmediate([=](auto in __unused, auto& out) {
		SET_OUT_HEADER_LEN(out, open);
	}));

	fd = open(FULLPATH, O_EXEC | O_DIRECTORY);
	ASSERT_LE(0, fd) << strerror(errno);

	leak(fd);
}

TEST_F(Opendir, opendir)
{
	const char FULLPATH[] = "mountpoint/some_dir";
	const char RELPATH[] = "some_dir";
	uint64_t ino = 42;

	expect_lookup(RELPATH, ino);
	expect_opendir(ino, O_RDONLY,
	ReturnImmediate([=](auto in __unused, auto& out) {
		SET_OUT_HEADER_LEN(out, open);
	}));

	errno = 0;
	EXPECT_NE(nullptr, opendir(FULLPATH)) << strerror(errno);
}

/*
 * Without FUSE_NO_OPENDIR_SUPPORT, returning ENOSYS is an error
 */
TEST_F(Opendir, enosys)
{
	const char FULLPATH[] = "mountpoint/some_file.txt";
	const char RELPATH[] = "some_file.txt";
	uint64_t ino = 42;

	expect_lookup(RELPATH, ino);
	expect_opendir(ino, O_RDONLY, ReturnErrno(ENOSYS));

	EXPECT_EQ(-1, open(FULLPATH, O_DIRECTORY));
	EXPECT_EQ(ENOSYS, errno);
}

/*
 * If a fuse server sets FUSE_NO_OPENDIR_SUPPORT and returns ENOSYS to a
 * FUSE_OPENDIR, then it and subsequent FUSE_OPENDIR and FUSE_RELEASEDIR
 * operations will also succeed automatically without being sent to the server.
 */
TEST_F(OpendirNoOpendirSupport, enosys)
{
	const char FULLPATH[] = "mountpoint/some_file.txt";
	const char RELPATH[] = "some_file.txt";
	uint64_t ino = 42;
	int fd;

	FuseTest::expect_lookup(RELPATH, ino, S_IFDIR | 0755, 0, 2);
	expect_opendir(ino, O_RDONLY, ReturnErrno(ENOSYS));

	fd = open(FULLPATH, O_DIRECTORY);
	ASSERT_LE(0, fd) << strerror(errno);
	close(fd);

	fd = open(FULLPATH, O_DIRECTORY);
	ASSERT_LE(0, fd) << strerror(errno);

	leak(fd);
}
