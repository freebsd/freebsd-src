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

class FsyncDir: public FuseTest {
public:
void expect_fsyncdir(uint64_t ino, uint32_t flags, int error)
{
	EXPECT_CALL(*m_mock, process(
		ResultOf([=](auto in) {
			return (in.header.opcode == FUSE_FSYNCDIR &&
				in.header.nodeid == ino &&
				/* 
				 * TODO: reenable pid check after fixing
				 * bug 236379
				 */
				//(pid_t)in.header.pid == getpid() &&
				in.body.fsyncdir.fh == FH &&
				in.body.fsyncdir.fsync_flags == flags);
		}, Eq(true)),
		_)
	).WillOnce(Invoke(ReturnErrno(error)));
}

void expect_lookup(const char *relpath, uint64_t ino)
{
	FuseTest::expect_lookup(relpath, ino, S_IFDIR | 0755, 0, 1);
}

};

class AioFsyncDir: public FsyncDir {
virtual void SetUp() {
	if (!is_unsafe_aio_enabled())
		GTEST_SKIP() <<
			"vfs.aio.enable_unsafe must be set for this test";
	FuseTest::SetUp();
}
};

/* https://bugs.freebsd.org/bugzilla/show_bug.cgi?id=236379 */
TEST_F(AioFsyncDir, aio_fsync)
{
	const char FULLPATH[] = "mountpoint/some_file.txt";
	const char RELPATH[] = "some_file.txt";
	uint64_t ino = 42;
	struct aiocb iocb, *piocb;
	int fd;

	expect_lookup(RELPATH, ino);
	expect_opendir(ino);
	expect_fsyncdir(ino, 0, 0);

	fd = open(FULLPATH, O_DIRECTORY);
	ASSERT_LE(0, fd) << strerror(errno);

	bzero(&iocb, sizeof(iocb));
	iocb.aio_fildes = fd;

	ASSERT_EQ(0, aio_fsync(O_SYNC, &iocb)) << strerror(errno);
	ASSERT_EQ(0, aio_waitcomplete(&piocb, NULL)) << strerror(errno);

	leak(fd);
}

TEST_F(FsyncDir, eio)
{
	const char FULLPATH[] = "mountpoint/some_dir";
	const char RELPATH[] = "some_dir";
	uint64_t ino = 42;
	int fd;

	expect_lookup(RELPATH, ino);
	expect_opendir(ino);
	expect_fsyncdir(ino, 0, EIO);

	fd = open(FULLPATH, O_DIRECTORY);
	ASSERT_LE(0, fd) << strerror(errno);
	ASSERT_NE(0, fsync(fd));
	ASSERT_EQ(EIO, errno);

	leak(fd);
}

/*
 * If the filesystem returns ENOSYS, it will be treated as success and
 * subsequent calls to VOP_FSYNC will succeed automatically without being sent
 * to the filesystem daemon
 */
TEST_F(FsyncDir, enosys)
{
	const char FULLPATH[] = "mountpoint/some_dir";
	const char RELPATH[] = "some_dir";
	uint64_t ino = 42;
	int fd;

	expect_lookup(RELPATH, ino);
	expect_opendir(ino);
	expect_fsyncdir(ino, 0, ENOSYS);

	fd = open(FULLPATH, O_DIRECTORY);
	ASSERT_LE(0, fd) << strerror(errno);
	EXPECT_EQ(0, fsync(fd)) << strerror(errno);

	/* Subsequent calls shouldn't query the daemon*/
	EXPECT_EQ(0, fsync(fd)) << strerror(errno);

	leak(fd);
}

TEST_F(FsyncDir, fsyncdata)
{
	const char FULLPATH[] = "mountpoint/some_dir";
	const char RELPATH[] = "some_dir";
	uint64_t ino = 42;
	int fd;

	expect_lookup(RELPATH, ino);
	expect_opendir(ino);
	expect_fsyncdir(ino, FUSE_FSYNC_FDATASYNC, 0);

	fd = open(FULLPATH, O_DIRECTORY);
	ASSERT_LE(0, fd) << strerror(errno);
	ASSERT_EQ(0, fdatasync(fd)) << strerror(errno);

	leak(fd);
}

/* 
 * Unlike regular files, the kernel doesn't know whether a directory is or
 * isn't dirty, so fuse(4) should always send FUSE_FSYNCDIR on fsync(2)
 */
TEST_F(FsyncDir, fsync)
{
	const char FULLPATH[] = "mountpoint/some_dir";
	const char RELPATH[] = "some_dir";
	uint64_t ino = 42;
	int fd;

	expect_lookup(RELPATH, ino);
	expect_opendir(ino);
	expect_fsyncdir(ino, 0, 0);

	fd = open(FULLPATH, O_DIRECTORY);
	ASSERT_LE(0, fd) << strerror(errno);
	ASSERT_EQ(0, fsync(fd)) << strerror(errno);

	leak(fd);
}
