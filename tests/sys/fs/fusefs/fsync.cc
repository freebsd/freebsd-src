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
#include <aio.h>
#include <fcntl.h>
#include <unistd.h>
}

#include "mockfs.hh"
#include "utils.hh"

using namespace testing;

/*
 * TODO: remove FUSE_FSYNC_FDATASYNC definition when upgrading to protocol 7.28.
 * This bit was actually part of kernel protocol version 5.2, but never
 * documented until after 7.28
 */
#ifndef FUSE_FSYNC_FDATASYNC
#define FUSE_FSYNC_FDATASYNC 1
#endif

class Fsync: public FuseTest {
public:
void expect_fsync(uint64_t ino, uint32_t flags, int error, int times = 1)
{
	EXPECT_CALL(*m_mock, process(
		ResultOf([=](auto in) {
			return (in.header.opcode == FUSE_FSYNC &&
				in.header.nodeid == ino &&
				/* 
				 * TODO: reenable pid check after fixing
				 * bug 236379
				 */
				//(pid_t)in.header.pid == getpid() &&
				in.body.fsync.fh == FH &&
				in.body.fsync.fsync_flags == flags);
		}, Eq(true)),
		_)
	).Times(times)
	.WillRepeatedly(Invoke(ReturnErrno(error)));
}

void expect_lookup(const char *relpath, uint64_t ino, int times = 1)
{
	FuseTest::expect_lookup(relpath, ino, S_IFREG | 0644, 0, times);
}

void expect_write(uint64_t ino, uint64_t size, const void *contents)
{
	FuseTest::expect_write(ino, 0, size, size, 0, 0, contents);
}

};

class AioFsync: public Fsync {
virtual void SetUp() {
	if (!is_unsafe_aio_enabled())
		GTEST_SKIP() <<
			"vfs.aio.enable_unsafe must be set for this test";
	FuseTest::SetUp();
}
};

/* https://bugs.freebsd.org/bugzilla/show_bug.cgi?id=236379 */
TEST_F(AioFsync, aio_fsync)
{
	const char FULLPATH[] = "mountpoint/some_file.txt";
	const char RELPATH[] = "some_file.txt";
	const char *CONTENTS = "abcdefgh";
	ssize_t bufsize = strlen(CONTENTS);
	uint64_t ino = 42;
	struct aiocb iocb, *piocb;
	int fd;

	expect_lookup(RELPATH, ino);
	expect_open(ino, 0, 1);
	expect_write(ino, bufsize, CONTENTS);
	expect_fsync(ino, 0, 0);

	fd = open(FULLPATH, O_RDWR);
	ASSERT_LE(0, fd) << strerror(errno);
	ASSERT_EQ(bufsize, write(fd, CONTENTS, bufsize)) << strerror(errno);

	bzero(&iocb, sizeof(iocb));
	iocb.aio_fildes = fd;

	ASSERT_EQ(0, aio_fsync(O_SYNC, &iocb)) << strerror(errno);
	ASSERT_EQ(0, aio_waitcomplete(&piocb, NULL)) << strerror(errno);

	leak(fd);
}

/*
 * fuse(4) should NOT fsync during VOP_RELEASE or VOP_INACTIVE
 *
 * This test only really make sense in writeback caching mode, but it should
 * still pass in any cache mode.
 */
TEST_F(Fsync, close)
{
	const char FULLPATH[] = "mountpoint/some_file.txt";
	const char RELPATH[] = "some_file.txt";
	const char *CONTENTS = "abcdefgh";
	ssize_t bufsize = strlen(CONTENTS);
	uint64_t ino = 42;
	int fd;

	expect_lookup(RELPATH, ino);
	expect_open(ino, 0, 1);
	expect_write(ino, bufsize, CONTENTS);
	EXPECT_CALL(*m_mock, process(
		ResultOf([=](auto in) {
			return (in.header.opcode == FUSE_SETATTR);
		}, Eq(true)),
		_)
	).WillRepeatedly(Invoke(ReturnImmediate([=](auto i __unused, auto& out) {
		SET_OUT_HEADER_LEN(out, attr);
		out.body.attr.attr.ino = ino;	// Must match nodeid
	})));
	EXPECT_CALL(*m_mock, process(
		ResultOf([=](auto in) {
			return (in.header.opcode == FUSE_FSYNC);
		}, Eq(true)),
		_)
	).Times(0);
	expect_flush(ino, 1, ReturnErrno(0));
	expect_release(ino, FH);

	fd = open(FULLPATH, O_RDWR);
	ASSERT_LE(0, fd) << strerror(errno);
	ASSERT_EQ(bufsize, write(fd, CONTENTS, bufsize)) << strerror(errno);
	close(fd);
}

TEST_F(Fsync, eio)
{
	const char FULLPATH[] = "mountpoint/some_file.txt";
	const char RELPATH[] = "some_file.txt";
	const char *CONTENTS = "abcdefgh";
	ssize_t bufsize = strlen(CONTENTS);
	uint64_t ino = 42;
	int fd;

	expect_lookup(RELPATH, ino);
	expect_open(ino, 0, 1);
	expect_write(ino, bufsize, CONTENTS);
	expect_fsync(ino, FUSE_FSYNC_FDATASYNC, EIO);

	fd = open(FULLPATH, O_RDWR);
	ASSERT_LE(0, fd) << strerror(errno);
	ASSERT_EQ(bufsize, write(fd, CONTENTS, bufsize)) << strerror(errno);
	ASSERT_NE(0, fdatasync(fd));
	ASSERT_EQ(EIO, errno);

	leak(fd);
}

/*
 * If the filesystem returns ENOSYS, it will be treated as success and
 * subsequent calls to VOP_FSYNC will succeed automatically without being sent
 * to the filesystem daemon
 */
TEST_F(Fsync, enosys)
{
	const char FULLPATH[] = "mountpoint/some_file.txt";
	const char RELPATH[] = "some_file.txt";
	const char *CONTENTS = "abcdefgh";
	ssize_t bufsize = strlen(CONTENTS);
	uint64_t ino = 42;
	int fd;

	expect_lookup(RELPATH, ino);
	expect_open(ino, 0, 1);
	expect_write(ino, bufsize, CONTENTS);
	expect_fsync(ino, FUSE_FSYNC_FDATASYNC, ENOSYS);

	fd = open(FULLPATH, O_RDWR);
	ASSERT_LE(0, fd) << strerror(errno);
	ASSERT_EQ(bufsize, write(fd, CONTENTS, bufsize)) << strerror(errno);
	EXPECT_EQ(0, fdatasync(fd));

	/* Subsequent calls shouldn't query the daemon*/
	EXPECT_EQ(0, fdatasync(fd));
	leak(fd);
}


TEST_F(Fsync, fdatasync)
{
	const char FULLPATH[] = "mountpoint/some_file.txt";
	const char RELPATH[] = "some_file.txt";
	const char *CONTENTS = "abcdefgh";
	ssize_t bufsize = strlen(CONTENTS);
	uint64_t ino = 42;
	int fd;

	expect_lookup(RELPATH, ino);
	expect_open(ino, 0, 1);
	expect_write(ino, bufsize, CONTENTS);
	expect_fsync(ino, FUSE_FSYNC_FDATASYNC, 0);

	fd = open(FULLPATH, O_RDWR);
	ASSERT_LE(0, fd) << strerror(errno);
	ASSERT_EQ(bufsize, write(fd, CONTENTS, bufsize)) << strerror(errno);
	ASSERT_EQ(0, fdatasync(fd)) << strerror(errno);

	leak(fd);
}

TEST_F(Fsync, fsync)
{
	const char FULLPATH[] = "mountpoint/some_file.txt";
	const char RELPATH[] = "some_file.txt";
	const char *CONTENTS = "abcdefgh";
	ssize_t bufsize = strlen(CONTENTS);
	uint64_t ino = 42;
	int fd;

	expect_lookup(RELPATH, ino);
	expect_open(ino, 0, 1);
	expect_write(ino, bufsize, CONTENTS);
	expect_fsync(ino, 0, 0);

	fd = open(FULLPATH, O_RDWR);
	ASSERT_LE(0, fd) << strerror(errno);
	ASSERT_EQ(bufsize, write(fd, CONTENTS, bufsize)) << strerror(errno);
	ASSERT_EQ(0, fsync(fd)) << strerror(errno);

	leak(fd);
}

/* If multiple FUSE file handles are active, we must fsync them all */
TEST_F(Fsync, two_handles)
{
	const char FULLPATH[] = "mountpoint/some_file.txt";
	const char RELPATH[] = "some_file.txt";
	const char *CONTENTS = "abcdefgh";
	ssize_t bufsize = strlen(CONTENTS);
	uint64_t ino = 42;
	int fd1, fd2;

	expect_lookup(RELPATH, ino, 2);
	expect_open(ino, 0, 2);
	expect_write(ino, bufsize, CONTENTS);
	expect_fsync(ino, 0, 0, 2);

	fd1 = open(FULLPATH, O_WRONLY);
	ASSERT_LE(0, fd1) << strerror(errno);
	fd2 = open(FULLPATH, O_RDONLY);
	ASSERT_LE(0, fd2) << strerror(errno);
	ASSERT_EQ(bufsize, write(fd1, CONTENTS, bufsize)) << strerror(errno);
	ASSERT_EQ(0, fsync(fd1)) << strerror(errno);

	leak(fd1);
	leak(fd2);
}
