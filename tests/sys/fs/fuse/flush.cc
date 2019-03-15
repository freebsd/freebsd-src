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
void expect_flush(uint64_t ino, int times, pid_t lo, ProcessMockerT r)
{
	EXPECT_CALL(*m_mock, process(
		ResultOf([=](auto in) {
			return (in->header.opcode == FUSE_FLUSH &&
				in->header.nodeid == ino &&
				in->body.flush.lock_owner == (uint64_t)lo &&
				in->body.flush.fh == FH);
		}, Eq(true)),
		_)
	).Times(times)
	.WillRepeatedly(Invoke(r));
}

void expect_lookup(const char *relpath, uint64_t ino)
{
	FuseTest::expect_lookup(relpath, ino, S_IFREG | 0644, 1);
}

/*
 * When testing FUSE_FLUSH, the FUSE_RELEASE calls are uninteresting.  This
 * expectation will silence googlemock warnings
 */
void expect_release()
{
	EXPECT_CALL(*m_mock, process(
		ResultOf([=](auto in) {
			return (in->header.opcode == FUSE_RELEASE);
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

/* If a file descriptor is duplicated, every close causes FLUSH */
/* https://bugs.freebsd.org/bugzilla/show_bug.cgi?id=236405 */
TEST_F(Flush, DISABLED_dup)
{
	const char FULLPATH[] = "mountpoint/some_file.txt";
	const char RELPATH[] = "some_file.txt";
	uint64_t ino = 42;
	int fd, fd2;

	expect_lookup(RELPATH, ino);
	expect_open(ino, 0, 1);
	expect_getattr(ino, 0);
	expect_flush(ino, 2, 0, ReturnErrno(0));
	expect_release();

	fd = open(FULLPATH, O_WRONLY);
	EXPECT_LE(0, fd) << strerror(errno);

	fd2 = dup(fd);

	ASSERT_EQ(0, close(fd2)) << strerror(errno);
	ASSERT_EQ(0, close(fd)) << strerror(errno);
}

/*
 * Some FUSE filesystem cache data internally and flush it on release.  Such
 * filesystems may generate errors during release.  On Linux, these get
 * returned by close(2).  However, POSIX does not require close(2) to return
 * this error.  FreeBSD's fuse(4) should return EIO if it returns an error at
 * all.
 */
/* http://pubs.opengroup.org/onlinepubs/9699919799/functions/close.html */
/* https://bugs.freebsd.org/bugzilla/show_bug.cgi?id=236405 */
TEST_F(Flush, DISABLED_eio)
{
	const char FULLPATH[] = "mountpoint/some_file.txt";
	const char RELPATH[] = "some_file.txt";
	uint64_t ino = 42;
	int fd;

	expect_lookup(RELPATH, ino);
	expect_open(ino, 0, 1);
	expect_getattr(ino, 0);
	expect_flush(ino, 1, 0, ReturnErrno(EIO));
	expect_release();

	fd = open(FULLPATH, O_WRONLY);
	EXPECT_LE(0, fd) << strerror(errno);

	ASSERT_TRUE(0 == close(fd) || errno == EIO) << strerror(errno);
}

/* A FUSE_FLUSH should be sent on close(2) */
/* https://bugs.freebsd.org/bugzilla/show_bug.cgi?id=236405 */
TEST_F(Flush, DISABLED_flush)
{
	const char FULLPATH[] = "mountpoint/some_file.txt";
	const char RELPATH[] = "some_file.txt";
	uint64_t ino = 42;
	int fd;

	expect_lookup(RELPATH, ino);
	expect_open(ino, 0, 1);
	expect_getattr(ino, 0);
	expect_flush(ino, 1, 0, ReturnErrno(0));
	expect_release();

	fd = open(FULLPATH, O_WRONLY);
	EXPECT_LE(0, fd) << strerror(errno);

	ASSERT_TRUE(0 == close(fd)) << strerror(errno);
}

/*
 * When closing a file with a POSIX file lock, flush should release the lock,
 * _even_if_ it's not the process's last file descriptor for this file.
 */
/* https://bugs.freebsd.org/bugzilla/show_bug.cgi?id=236405 */
/* https://bugs.freebsd.org/bugzilla/show_bug.cgi?id=234581 */
TEST_F(FlushWithLocks, DISABLED_unlock_on_close)
{
	const char FULLPATH[] = "mountpoint/some_file.txt";
	const char RELPATH[] = "some_file.txt";
	uint64_t ino = 42;
	int fd, fd2;
	struct flock fl;
	pid_t pid = getpid();

	expect_lookup(RELPATH, ino);
	expect_open(ino, 0, 1);
	expect_getattr(ino, 0);
	EXPECT_CALL(*m_mock, process(
		ResultOf([=](auto in) {
			return (in->header.opcode == FUSE_SETLK &&
				in->header.nodeid == ino &&
				in->body.setlk.fh == FH);
		}, Eq(true)),
		_)
	).WillOnce(Invoke(ReturnImmediate([=](auto in, auto out) {
		SET_OUT_HEADER_LEN(out, setlk);
		out->body.setlk.lk = in->body.setlk.lk;
	})));
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

	fd2 = dup(fd);
	ASSERT_EQ(0, close(fd2)) << strerror(errno);
	/* Deliberately leak fd */
}
