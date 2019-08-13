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
#include <fcntl.h>
#include <unistd.h>
}

#include "mockfs.hh"
#include "utils.hh"

using namespace testing;

class Flush: public FuseTest {

public:
void
expect_flush(uint64_t ino, int times, pid_t lo, ProcessMockerT r)
{
	EXPECT_CALL(*m_mock, process(
		ResultOf([=](auto in) {
			return (in.header.opcode == FUSE_FLUSH &&
				in.header.nodeid == ino &&
				in.body.flush.lock_owner == (uint64_t)lo &&
				in.body.flush.fh == FH);
		}, Eq(true)),
		_)
	).Times(times)
	.WillRepeatedly(Invoke(r));
}

void expect_lookup(const char *relpath, uint64_t ino, int times)
{
	FuseTest::expect_lookup(relpath, ino, S_IFREG | 0644, 0, times);
}

/*
 * When testing FUSE_FLUSH, the FUSE_RELEASE calls are uninteresting.  This
 * expectation will silence googlemock warnings
 */
void expect_release()
{
	EXPECT_CALL(*m_mock, process(
		ResultOf([=](auto in) {
			return (in.header.opcode == FUSE_RELEASE);
		}, Eq(true)),
		_)
	).WillRepeatedly(Invoke(ReturnErrno(0)));
}
};

class FlushWithLocks: public Flush {
	virtual void SetUp() {
		m_init_flags = FUSE_POSIX_LOCKS;
		Flush::SetUp();
	}
};

/*
 * If multiple file descriptors refer to the same file handle, closing each
 * should send FUSE_FLUSH
 */
TEST_F(Flush, open_twice)
{
	const char FULLPATH[] = "mountpoint/some_file.txt";
	const char RELPATH[] = "some_file.txt";
	uint64_t ino = 42;
	int fd, fd2;

	expect_lookup(RELPATH, ino, 2);
	expect_open(ino, 0, 1);
	expect_flush(ino, 2, getpid(), ReturnErrno(0));
	expect_release();

	fd = open(FULLPATH, O_WRONLY);
	EXPECT_LE(0, fd) << strerror(errno);

	fd2 = open(FULLPATH, O_WRONLY);
	EXPECT_LE(0, fd2) << strerror(errno);

	EXPECT_EQ(0, close(fd2)) << strerror(errno);
	EXPECT_EQ(0, close(fd)) << strerror(errno);
}

/*
 * Some FUSE filesystem cache data internally and flush it on release.  Such
 * filesystems may generate errors during release.  On Linux, these get
 * returned by close(2).  However, POSIX does not require close(2) to return
 * this error.  FreeBSD's fuse(4) should return EIO if it returns an error at
 * all.
 */
/* http://pubs.opengroup.org/onlinepubs/9699919799/functions/close.html */
TEST_F(Flush, eio)
{
	const char FULLPATH[] = "mountpoint/some_file.txt";
	const char RELPATH[] = "some_file.txt";
	uint64_t ino = 42;
	int fd;

	expect_lookup(RELPATH, ino, 1);
	expect_open(ino, 0, 1);
	expect_flush(ino, 1, getpid(), ReturnErrno(EIO));
	expect_release();

	fd = open(FULLPATH, O_WRONLY);
	EXPECT_LE(0, fd) << strerror(errno);

	ASSERT_TRUE(0 == close(fd) || errno == EIO) << strerror(errno);
}

/*
 * If the filesystem returns ENOSYS, it will be treated as success and
 * no more FUSE_FLUSH operations will be sent to the daemon
 */
TEST_F(Flush, enosys)
{
	const char FULLPATH0[] = "mountpoint/some_file.txt";
	const char RELPATH0[] = "some_file.txt";
	const char FULLPATH1[] = "mountpoint/other_file.txt";
	const char RELPATH1[] = "other_file.txt";
	uint64_t ino0 = 42;
	uint64_t ino1 = 43;
	int fd0, fd1;

	expect_lookup(RELPATH0, ino0, 1);
	expect_open(ino0, 0, 1);
	/* On the 2nd close, FUSE_FLUSH won't be sent at all */
	expect_flush(ino0, 1, getpid(), ReturnErrno(ENOSYS));
	expect_release();

	expect_lookup(RELPATH1, ino1, 1);
	expect_open(ino1, 0, 1);
	/* On the 2nd close, FUSE_FLUSH won't be sent at all */
	expect_release();

	fd0 = open(FULLPATH0, O_WRONLY);
	ASSERT_LE(0, fd0) << strerror(errno);

	fd1 = open(FULLPATH1, O_WRONLY);
	ASSERT_LE(0, fd1) << strerror(errno);

	EXPECT_EQ(0, close(fd0)) << strerror(errno);
	EXPECT_EQ(0, close(fd1)) << strerror(errno);
}

/* A FUSE_FLUSH should be sent on close(2) */
TEST_F(Flush, flush)
{
	const char FULLPATH[] = "mountpoint/some_file.txt";
	const char RELPATH[] = "some_file.txt";
	uint64_t ino = 42;
	int fd;

	expect_lookup(RELPATH, ino, 1);
	expect_open(ino, 0, 1);
	expect_flush(ino, 1, getpid(), ReturnErrno(0));
	expect_release();

	fd = open(FULLPATH, O_WRONLY);
	EXPECT_LE(0, fd) << strerror(errno);

	ASSERT_TRUE(0 == close(fd)) << strerror(errno);
}

/*
 * When closing a file with a POSIX file lock, flush should release the lock,
 * _even_if_ it's not the process's last file descriptor for this file.
 */
TEST_F(FlushWithLocks, unlock_on_close)
{
	const char FULLPATH[] = "mountpoint/some_file.txt";
	const char RELPATH[] = "some_file.txt";
	uint64_t ino = 42;
	int fd, fd2;
	struct flock fl;
	pid_t pid = getpid();

	expect_lookup(RELPATH, ino, 2);
	expect_open(ino, 0, 1);
	EXPECT_CALL(*m_mock, process(
		ResultOf([=](auto in) {
			return (in.header.opcode == FUSE_SETLK &&
				in.header.nodeid == ino &&
				in.body.setlk.fh == FH);
		}, Eq(true)),
		_)
	).WillOnce(Invoke(ReturnErrno(0)));
	expect_flush(ino, 1, pid, ReturnErrno(0));

	fd = open(FULLPATH, O_RDWR);
	ASSERT_LE(0, fd) << strerror(errno);
	fl.l_start = 0;
	fl.l_len = 0;
	fl.l_pid = pid;
	fl.l_type = F_RDLCK;
	fl.l_whence = SEEK_SET;
	fl.l_sysid = 0;
	ASSERT_NE(-1, fcntl(fd, F_SETLKW, &fl)) << strerror(errno);

	fd2 = open(FULLPATH, O_WRONLY);
	ASSERT_LE(0, fd2) << strerror(errno);
	ASSERT_EQ(0, close(fd2)) << strerror(errno);
	leak(fd);
	leak(fd2);
}
