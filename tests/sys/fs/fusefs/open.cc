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
#include <sys/wait.h>

#include <fcntl.h>
#include <semaphore.h>
}

#include "mockfs.hh"
#include "utils.hh"

using namespace testing;

class Open: public FuseTest {

public:

/* Test an OK open of a file with the given flags */
void test_ok(int os_flags, int fuse_flags) {
	const char FULLPATH[] = "mountpoint/some_file.txt";
	const char RELPATH[] = "some_file.txt";
	uint64_t ino = 42;
	int fd;

	FuseTest::expect_lookup(RELPATH, ino, S_IFREG | 0644, 0, 1);
	EXPECT_CALL(*m_mock, process(
		ResultOf([=](auto in) {
			return (in.header.opcode == FUSE_OPEN &&
				in.body.open.flags == (uint32_t)fuse_flags &&
				in.header.nodeid == ino);
		}, Eq(true)),
		_)
	).WillOnce(Invoke(ReturnImmediate([](auto in __unused, auto& out) {
		out.header.len = sizeof(out.header);
		SET_OUT_HEADER_LEN(out, open);
	})));

	fd = open(FULLPATH, os_flags);
	ASSERT_LE(0, fd) << strerror(errno);
	leak(fd);
}
};


class OpenNoOpenSupport: public FuseTest {
	virtual void SetUp() {
		m_init_flags = FUSE_NO_OPEN_SUPPORT;
		FuseTest::SetUp();
	}
};

/* 
 * fusefs(5) does not support I/O on device nodes (neither does UFS).  But it
 * shouldn't crash
 */
TEST_F(Open, chr)
{
	const char FULLPATH[] = "mountpoint/zero";
	const char RELPATH[] = "zero";
	uint64_t ino = 42;

	EXPECT_LOOKUP(FUSE_ROOT_ID, RELPATH)
	.WillRepeatedly(Invoke(ReturnImmediate([=](auto in __unused, auto& out) {
		SET_OUT_HEADER_LEN(out, entry);
		out.body.entry.attr.mode = S_IFCHR | 0644;
		out.body.entry.nodeid = ino;
		out.body.entry.attr.nlink = 1;
		out.body.entry.attr_valid = UINT64_MAX;
		out.body.entry.attr.rdev = 44;	/* /dev/zero's rdev */
	})));

	ASSERT_EQ(-1, open(FULLPATH, O_RDONLY));
	EXPECT_EQ(EOPNOTSUPP, errno);
}

/* 
 * The fuse daemon fails the request with enoent.  This usually indicates a
 * race condition: some other FUSE client removed the file in between when the
 * kernel checked for it with lookup and tried to open it
 */
TEST_F(Open, enoent)
{
	const char FULLPATH[] = "mountpoint/some_file.txt";
	const char RELPATH[] = "some_file.txt";
	uint64_t ino = 42;
	sem_t sem;

	ASSERT_EQ(0, sem_init(&sem, 0, 0)) << strerror(errno);

	expect_lookup(RELPATH, ino, S_IFREG | 0644, 0, 1);
	EXPECT_CALL(*m_mock, process(
		ResultOf([=](auto in) {
			return (in.header.opcode == FUSE_OPEN &&
				in.header.nodeid == ino);
		}, Eq(true)),
		_)
	).WillOnce(Invoke(ReturnErrno(ENOENT)));
	// Since FUSE_OPEN returns ENOENT, the kernel will reclaim the vnode
	// and send a FUSE_FORGET
	expect_forget(ino, 1, &sem);

	ASSERT_EQ(-1, open(FULLPATH, O_RDONLY));
	EXPECT_EQ(ENOENT, errno);

	sem_wait(&sem);
	sem_destroy(&sem);
}

/* 
 * The daemon is responsible for checking file permissions (unless the
 * default_permissions mount option was used)
 */
TEST_F(Open, eperm)
{
	const char FULLPATH[] = "mountpoint/some_file.txt";
	const char RELPATH[] = "some_file.txt";
	uint64_t ino = 42;

	expect_lookup(RELPATH, ino, S_IFREG | 0644, 0, 1);
	EXPECT_CALL(*m_mock, process(
		ResultOf([=](auto in) {
			return (in.header.opcode == FUSE_OPEN &&
				in.header.nodeid == ino);
		}, Eq(true)),
		_)
	).WillOnce(Invoke(ReturnErrno(EPERM)));
	ASSERT_EQ(-1, open(FULLPATH, O_RDONLY));
	EXPECT_EQ(EPERM, errno);
}

/* 
 * fusefs must issue multiple FUSE_OPEN operations if clients with different
 * credentials open the same file, even if they use the same mode.  This is
 * necessary so that the daemon can validate each set of credentials.
 */
TEST_F(Open, multiple_creds)
{
	const static char FULLPATH[] = "mountpoint/some_file.txt";
	const static char RELPATH[] = "some_file.txt";
	int fd1, status;
	const static uint64_t ino = 42;
	const static uint64_t fh0 = 100, fh1 = 200;

	/* Fork a child to open the file with different credentials */
	fork(false, &status, [&] {

		expect_lookup(RELPATH, ino, S_IFREG | 0644, 0, 2);
		EXPECT_CALL(*m_mock, process(
			ResultOf([=](auto in) {
				return (in.header.opcode == FUSE_OPEN &&
					in.header.pid == (uint32_t)getpid() &&
					in.header.nodeid == ino);
			}, Eq(true)),
			_)
		).WillOnce(Invoke(
			ReturnImmediate([](auto in __unused, auto& out) {
			out.body.open.fh = fh0;
			out.header.len = sizeof(out.header);
			SET_OUT_HEADER_LEN(out, open);
		})));

		EXPECT_CALL(*m_mock, process(
			ResultOf([=](auto in) {
				return (in.header.opcode == FUSE_OPEN &&
					in.header.pid != (uint32_t)getpid() &&
					in.header.nodeid == ino);
			}, Eq(true)),
			_)
		).WillOnce(Invoke(
			ReturnImmediate([](auto in __unused, auto& out) {
			out.body.open.fh = fh1;
			out.header.len = sizeof(out.header);
			SET_OUT_HEADER_LEN(out, open);
		})));
		expect_flush(ino, 2, ReturnErrno(0));
		expect_release(ino, fh0);
		expect_release(ino, fh1);

		fd1 = open(FULLPATH, O_RDONLY);
		ASSERT_LE(0, fd1) << strerror(errno);
	}, [] {
		int fd0;

		fd0 = open(FULLPATH, O_RDONLY);
		if (fd0 < 0) {
			perror("open");
			return(1);
		}
		leak(fd0);
		return 0;
	}
	);
	ASSERT_EQ(0, WEXITSTATUS(status));

	close(fd1);
}

/* https://bugs.freebsd.org/bugzilla/show_bug.cgi?id=236340 */
TEST_F(Open, DISABLED_o_append)
{
	test_ok(O_WRONLY | O_APPEND, O_WRONLY | O_APPEND);
}

/* The kernel is supposed to filter out this flag */
TEST_F(Open, o_creat)
{
	test_ok(O_WRONLY | O_CREAT, O_WRONLY);
}

/* https://bugs.freebsd.org/bugzilla/show_bug.cgi?id=236340 */
TEST_F(Open, DISABLED_o_direct)
{
	test_ok(O_WRONLY | O_DIRECT, O_WRONLY | O_DIRECT);
}

/* The kernel is supposed to filter out this flag */
TEST_F(Open, o_excl)
{
	test_ok(O_WRONLY | O_EXCL, O_WRONLY);
}

TEST_F(Open, o_exec)
{
	test_ok(O_EXEC, O_EXEC);
}

/* The kernel is supposed to filter out this flag */
TEST_F(Open, o_noctty)
{
	test_ok(O_WRONLY | O_NOCTTY, O_WRONLY);
}

TEST_F(Open, o_rdonly)
{
	test_ok(O_RDONLY, O_RDONLY);
}

/* https://bugs.freebsd.org/bugzilla/show_bug.cgi?id=236340 */
TEST_F(Open, DISABLED_o_trunc)
{
	test_ok(O_WRONLY | O_TRUNC, O_WRONLY | O_TRUNC);
}

TEST_F(Open, o_wronly)
{
	test_ok(O_WRONLY, O_WRONLY);
}

TEST_F(Open, o_rdwr)
{
	test_ok(O_RDWR, O_RDWR);
}

/*
 * Without FUSE_NO_OPEN_SUPPORT, returning ENOSYS is an error
 */
TEST_F(Open, enosys)
{
	const char FULLPATH[] = "mountpoint/some_file.txt";
	const char RELPATH[] = "some_file.txt";
	uint64_t ino = 42;
	int fd;

	FuseTest::expect_lookup(RELPATH, ino, S_IFREG | 0644, 0, 1);
	EXPECT_CALL(*m_mock, process(
		ResultOf([=](auto in) {
			return (in.header.opcode == FUSE_OPEN &&
				in.body.open.flags == (uint32_t)O_RDONLY &&
				in.header.nodeid == ino);
		}, Eq(true)),
		_)
	).Times(1)
	.WillOnce(Invoke(ReturnErrno(ENOSYS)));

	fd = open(FULLPATH, O_RDONLY);
	ASSERT_EQ(-1, fd) << strerror(errno);
	EXPECT_EQ(ENOSYS, errno);
}

/*
 * If a fuse server sets FUSE_NO_OPEN_SUPPORT and returns ENOSYS to a
 * FUSE_OPEN, then it and subsequent FUSE_OPEN and FUSE_RELEASE operations will
 * also succeed automatically without being sent to the server.
 */
TEST_F(OpenNoOpenSupport, enosys)
{
	const char FULLPATH[] = "mountpoint/some_file.txt";
	const char RELPATH[] = "some_file.txt";
	uint64_t ino = 42;
	int fd;

	FuseTest::expect_lookup(RELPATH, ino, S_IFREG | 0644, 0, 2);
	EXPECT_CALL(*m_mock, process(
		ResultOf([=](auto in) {
			return (in.header.opcode == FUSE_OPEN &&
				in.body.open.flags == (uint32_t)O_RDONLY &&
				in.header.nodeid == ino);
		}, Eq(true)),
		_)
	).Times(1)
	.WillOnce(Invoke(ReturnErrno(ENOSYS)));
	expect_flush(ino, 1, ReturnErrno(ENOSYS));

	fd = open(FULLPATH, O_RDONLY);
	ASSERT_LE(0, fd) << strerror(errno);
	close(fd);

	fd = open(FULLPATH, O_RDONLY);
	ASSERT_LE(0, fd) << strerror(errno);

	leak(fd);
}
