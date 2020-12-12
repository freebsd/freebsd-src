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

class Release: public FuseTest {

public:
void expect_lookup(const char *relpath, uint64_t ino, int times)
{
	FuseTest::expect_lookup(relpath, ino, S_IFREG | 0644, 0, times);
}

void expect_release(uint64_t ino, uint64_t lock_owner,
	uint32_t flags, int error)
{
	EXPECT_CALL(*m_mock, process(
		ResultOf([=](auto in) {
			return (in.header.opcode == FUSE_RELEASE &&
				in.header.nodeid == ino &&
				in.body.release.lock_owner == lock_owner &&
				in.body.release.fh == FH &&
				in.body.release.flags == flags);
		}, Eq(true)),
		_)
	).WillOnce(Invoke(ReturnErrno(error)))
	.RetiresOnSaturation();
}
};

class ReleaseWithLocks: public Release {
	virtual void SetUp() {
		m_init_flags = FUSE_POSIX_LOCKS;
		Release::SetUp();
	}
};


/* If a file descriptor is duplicated, only the last close causes RELEASE */
TEST_F(Release, dup)
{
	const char FULLPATH[] = "mountpoint/some_file.txt";
	const char RELPATH[] = "some_file.txt";
	uint64_t ino = 42;
	int fd, fd2;

	expect_lookup(RELPATH, ino, 1);
	expect_open(ino, 0, 1);
	expect_flush(ino, 1, ReturnErrno(0));
	expect_release(ino, getpid(), O_RDONLY, 0);
	
	fd = open(FULLPATH, O_RDONLY);
	ASSERT_LE(0, fd) << strerror(errno);

	fd2 = dup(fd);
	ASSERT_LE(0, fd2) << strerror(errno);

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
TEST_F(Release, eio)
{
	const char FULLPATH[] = "mountpoint/some_file.txt";
	const char RELPATH[] = "some_file.txt";
	uint64_t ino = 42;
	int fd;

	expect_lookup(RELPATH, ino, 1);
	expect_open(ino, 0, 1);
	expect_flush(ino, 1, ReturnErrno(0));
	expect_release(ino, getpid(), O_WRONLY, EIO);
	
	fd = open(FULLPATH, O_WRONLY);
	ASSERT_LE(0, fd) << strerror(errno);

	ASSERT_TRUE(0 == close(fd) || errno == EIO) << strerror(errno);
}

/*
 * FUSE_RELEASE should contain the same flags used for FUSE_OPEN
 */
/* https://bugs.freebsd.org/bugzilla/show_bug.cgi?id=236340 */
TEST_F(Release, DISABLED_flags)
{
	const char FULLPATH[] = "mountpoint/some_file.txt";
	const char RELPATH[] = "some_file.txt";
	uint64_t ino = 42;
	int fd;

	expect_lookup(RELPATH, ino, 1);
	expect_open(ino, 0, 1);
	expect_flush(ino, 1, ReturnErrno(0));
	expect_release(ino, getpid(), O_RDWR | O_APPEND, 0);
	
	fd = open(FULLPATH, O_RDWR | O_APPEND);
	ASSERT_LE(0, fd) << strerror(errno);

	ASSERT_EQ(0, close(fd)) << strerror(errno);
}

/*
 * fuse(4) will issue multiple FUSE_OPEN operations for the same file if it's
 * opened with different modes.  Each FUSE_OPEN should get its own
 * FUSE_RELEASE.
 */
TEST_F(Release, multiple_opens)
{
	const char FULLPATH[] = "mountpoint/some_file.txt";
	const char RELPATH[] = "some_file.txt";
	uint64_t ino = 42;
	int fd, fd2;

	expect_lookup(RELPATH, ino, 2);
	expect_open(ino, 0, 2);
	expect_flush(ino, 2, ReturnErrno(0));
	expect_release(ino, getpid(), O_RDONLY, 0);
	
	fd = open(FULLPATH, O_RDONLY);
	ASSERT_LE(0, fd) << strerror(errno);

	expect_release(ino, getpid(), O_WRONLY, 0);
	fd2 = open(FULLPATH, O_WRONLY);
	ASSERT_LE(0, fd2) << strerror(errno);

	ASSERT_EQ(0, close(fd2)) << strerror(errno);
	ASSERT_EQ(0, close(fd)) << strerror(errno);
}

TEST_F(Release, ok)
{
	const char FULLPATH[] = "mountpoint/some_file.txt";
	const char RELPATH[] = "some_file.txt";
	uint64_t ino = 42;
	int fd;

	expect_lookup(RELPATH, ino, 1);
	expect_open(ino, 0, 1);
	expect_flush(ino, 1, ReturnErrno(0));
	expect_release(ino, getpid(), O_RDONLY, 0);
	
	fd = open(FULLPATH, O_RDONLY);
	ASSERT_LE(0, fd) << strerror(errno);

	ASSERT_EQ(0, close(fd)) << strerror(errno);
}

/* When closing a file with a POSIX file lock, release should release the lock*/
TEST_F(ReleaseWithLocks, unlock_on_close)
{
	const char FULLPATH[] = "mountpoint/some_file.txt";
	const char RELPATH[] = "some_file.txt";
	uint64_t ino = 42;
	int fd;
	struct flock fl;
	pid_t pid = getpid();

	expect_lookup(RELPATH, ino, 1);
	expect_open(ino, 0, 1);
	EXPECT_CALL(*m_mock, process(
		ResultOf([=](auto in) {
			return (in.header.opcode == FUSE_SETLK &&
				in.header.nodeid == ino &&
				in.body.setlk.fh == FH);
		}, Eq(true)),
		_)
	).WillOnce(Invoke(ReturnErrno(0)));
	expect_flush(ino, 1, ReturnErrno(0));
	expect_release(ino, static_cast<uint64_t>(pid), O_RDWR, 0);

	fd = open(FULLPATH, O_RDWR);
	ASSERT_LE(0, fd) << strerror(errno);
	fl.l_start = 0;
	fl.l_len = 0;
	fl.l_pid = pid;
	fl.l_type = F_RDLCK;
	fl.l_whence = SEEK_SET;
	fl.l_sysid = 0;
	ASSERT_NE(-1, fcntl(fd, F_SETLKW, &fl)) << strerror(errno);

	ASSERT_EQ(0, close(fd)) << strerror(errno);
}
