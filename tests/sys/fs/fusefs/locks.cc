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
}

#include "mockfs.hh"
#include "utils.hh"

/* This flag value should probably be defined in fuse_kernel.h */
#define OFFSET_MAX 0x7fffffffffffffffLL

using namespace testing;

/* For testing filesystems without posix locking support */
class Fallback: public FuseTest {
public:

void expect_lookup(const char *relpath, uint64_t ino)
{
	FuseTest::expect_lookup(relpath, ino, S_IFREG | 0644, 0, 1);
}

};

/* For testing filesystems with posix locking support */
class Locks: public Fallback {
	virtual void SetUp() {
		m_init_flags = FUSE_POSIX_LOCKS;
		Fallback::SetUp();
	}
};

class GetlkFallback: public Fallback {};
class Getlk: public Locks {};
class SetlkFallback: public Fallback {};
class Setlk: public Locks {};
class SetlkwFallback: public Fallback {};
class Setlkw: public Locks {};

/*
 * If the fuse filesystem does not support posix file locks, then the kernel
 * should fall back to local locks.
 */
TEST_F(GetlkFallback, local)
{
	const char FULLPATH[] = "mountpoint/some_file.txt";
	const char RELPATH[] = "some_file.txt";
	uint64_t ino = 42;
	struct flock fl;
	int fd;

	expect_lookup(RELPATH, ino);
	expect_open(ino, 0, 1);

	fd = open(FULLPATH, O_RDWR);
	ASSERT_LE(0, fd) << strerror(errno);
	fl.l_start = 10;
	fl.l_len = 1000;
	fl.l_pid = getpid();
	fl.l_type = F_RDLCK;
	fl.l_whence = SEEK_SET;
	fl.l_sysid = 0;
	ASSERT_NE(-1, fcntl(fd, F_GETLK, &fl)) << strerror(errno);
	/* Deliberately leak fd.  close(2) will be tested in release.cc */
}

/* 
 * If the filesystem has no locks that fit the description, the filesystem
 * should return F_UNLCK
 */
TEST_F(Getlk, no_locks)
{
	const char FULLPATH[] = "mountpoint/some_file.txt";
	const char RELPATH[] = "some_file.txt";
	uint64_t ino = 42;
	struct flock fl;
	int fd;
	pid_t pid = 1234;

	expect_lookup(RELPATH, ino);
	expect_open(ino, 0, 1);
	EXPECT_CALL(*m_mock, process(
		ResultOf([=](auto in) {
			return (in.header.opcode == FUSE_GETLK &&
				in.header.nodeid == ino &&
				in.body.getlk.fh == FH &&
				in.body.getlk.owner == (uint32_t)pid &&
				in.body.getlk.lk.start == 10 &&
				in.body.getlk.lk.end == 1009 &&
				in.body.getlk.lk.type == F_RDLCK &&
				in.body.getlk.lk.pid == (uint64_t)pid);
		}, Eq(true)),
		_)
	).WillOnce(Invoke(ReturnImmediate([=](auto in, auto& out) {
		SET_OUT_HEADER_LEN(out, getlk);
		out.body.getlk.lk = in.body.getlk.lk;
		out.body.getlk.lk.type = F_UNLCK;
	})));

	fd = open(FULLPATH, O_RDWR);
	ASSERT_LE(0, fd) << strerror(errno);
	fl.l_start = 10;
	fl.l_len = 1000;
	fl.l_pid = pid;
	fl.l_type = F_RDLCK;
	fl.l_whence = SEEK_SET;
	fl.l_sysid = 0;
	ASSERT_NE(-1, fcntl(fd, F_GETLK, &fl)) << strerror(errno);
	ASSERT_EQ(F_UNLCK, fl.l_type);
	/* Deliberately leak fd.  close(2) will be tested in release.cc */
}

/* A different pid does have a lock */
TEST_F(Getlk, lock_exists)
{
	const char FULLPATH[] = "mountpoint/some_file.txt";
	const char RELPATH[] = "some_file.txt";
	uint64_t ino = 42;
	struct flock fl;
	int fd;
	pid_t pid = 1234;
	pid_t pid2 = 1235;

	expect_lookup(RELPATH, ino);
	expect_open(ino, 0, 1);
	EXPECT_CALL(*m_mock, process(
		ResultOf([=](auto in) {
			return (in.header.opcode == FUSE_GETLK &&
				in.header.nodeid == ino &&
				in.body.getlk.fh == FH &&
				in.body.getlk.owner == (uint32_t)pid &&
				in.body.getlk.lk.start == 10 &&
				in.body.getlk.lk.end == 1009 &&
				in.body.getlk.lk.type == F_RDLCK &&
				in.body.getlk.lk.pid == (uint64_t)pid);
		}, Eq(true)),
		_)
	).WillOnce(Invoke(ReturnImmediate([=](auto in __unused, auto& out) {
		SET_OUT_HEADER_LEN(out, getlk);
		out.body.getlk.lk.start = 100;
		out.body.getlk.lk.end = 199;
		out.body.getlk.lk.type = F_WRLCK;
		out.body.getlk.lk.pid = (uint32_t)pid2;;
	})));

	fd = open(FULLPATH, O_RDWR);
	ASSERT_LE(0, fd) << strerror(errno);
	fl.l_start = 10;
	fl.l_len = 1000;
	fl.l_pid = pid;
	fl.l_type = F_RDLCK;
	fl.l_whence = SEEK_SET;
	fl.l_sysid = 0;
	ASSERT_NE(-1, fcntl(fd, F_GETLK, &fl)) << strerror(errno);
	EXPECT_EQ(100, fl.l_start);
	EXPECT_EQ(100, fl.l_len);
	EXPECT_EQ(pid2, fl.l_pid);
	EXPECT_EQ(F_WRLCK, fl.l_type);
	EXPECT_EQ(SEEK_SET, fl.l_whence);
	EXPECT_EQ(0, fl.l_sysid);
	/* Deliberately leak fd.  close(2) will be tested in release.cc */
}

/*
 * If the fuse filesystem does not support posix file locks, then the kernel
 * should fall back to local locks.
 */
TEST_F(SetlkFallback, local)
{
	const char FULLPATH[] = "mountpoint/some_file.txt";
	const char RELPATH[] = "some_file.txt";
	uint64_t ino = 42;
	struct flock fl;
	int fd;

	expect_lookup(RELPATH, ino);
	expect_open(ino, 0, 1);

	fd = open(FULLPATH, O_RDWR);
	ASSERT_LE(0, fd) << strerror(errno);
	fl.l_start = 10;
	fl.l_len = 1000;
	fl.l_pid = getpid();
	fl.l_type = F_RDLCK;
	fl.l_whence = SEEK_SET;
	fl.l_sysid = 0;
	ASSERT_NE(-1, fcntl(fd, F_SETLK, &fl)) << strerror(errno);
	/* Deliberately leak fd.  close(2) will be tested in release.cc */
}

/* Set a new lock with FUSE_SETLK */
TEST_F(Setlk, set)
{
	const char FULLPATH[] = "mountpoint/some_file.txt";
	const char RELPATH[] = "some_file.txt";
	uint64_t ino = 42;
	struct flock fl;
	int fd;
	pid_t pid = 1234;

	expect_lookup(RELPATH, ino);
	expect_open(ino, 0, 1);
	EXPECT_CALL(*m_mock, process(
		ResultOf([=](auto in) {
			return (in.header.opcode == FUSE_SETLK &&
				in.header.nodeid == ino &&
				in.body.setlk.fh == FH &&
				in.body.setlk.owner == (uint32_t)pid &&
				in.body.setlk.lk.start == 10 &&
				in.body.setlk.lk.end == 1009 &&
				in.body.setlk.lk.type == F_RDLCK &&
				in.body.setlk.lk.pid == (uint64_t)pid);
		}, Eq(true)),
		_)
	).WillOnce(Invoke(ReturnErrno(0)));

	fd = open(FULLPATH, O_RDWR);
	ASSERT_LE(0, fd) << strerror(errno);
	fl.l_start = 10;
	fl.l_len = 1000;
	fl.l_pid = pid;
	fl.l_type = F_RDLCK;
	fl.l_whence = SEEK_SET;
	fl.l_sysid = 0;
	ASSERT_NE(-1, fcntl(fd, F_SETLK, &fl)) << strerror(errno);
	/* Deliberately leak fd.  close(2) will be tested in release.cc */
}

/* l_len = 0 is a flag value that means to lock until EOF */
TEST_F(Setlk, set_eof)
{
	const char FULLPATH[] = "mountpoint/some_file.txt";
	const char RELPATH[] = "some_file.txt";
	uint64_t ino = 42;
	struct flock fl;
	int fd;
	pid_t pid = 1234;

	expect_lookup(RELPATH, ino);
	expect_open(ino, 0, 1);
	EXPECT_CALL(*m_mock, process(
		ResultOf([=](auto in) {
			return (in.header.opcode == FUSE_SETLK &&
				in.header.nodeid == ino &&
				in.body.setlk.fh == FH &&
				in.body.setlk.owner == (uint32_t)pid &&
				in.body.setlk.lk.start == 10 &&
				in.body.setlk.lk.end == OFFSET_MAX &&
				in.body.setlk.lk.type == F_RDLCK &&
				in.body.setlk.lk.pid == (uint64_t)pid);
		}, Eq(true)),
		_)
	).WillOnce(Invoke(ReturnErrno(0)));

	fd = open(FULLPATH, O_RDWR);
	ASSERT_LE(0, fd) << strerror(errno);
	fl.l_start = 10;
	fl.l_len = 0;
	fl.l_pid = pid;
	fl.l_type = F_RDLCK;
	fl.l_whence = SEEK_SET;
	fl.l_sysid = 0;
	ASSERT_NE(-1, fcntl(fd, F_SETLK, &fl)) << strerror(errno);
	/* Deliberately leak fd.  close(2) will be tested in release.cc */
}

/* Fail to set a new lock with FUSE_SETLK due to a conflict */
TEST_F(Setlk, eagain)
{
	const char FULLPATH[] = "mountpoint/some_file.txt";
	const char RELPATH[] = "some_file.txt";
	uint64_t ino = 42;
	struct flock fl;
	int fd;
	pid_t pid = 1234;

	expect_lookup(RELPATH, ino);
	expect_open(ino, 0, 1);
	EXPECT_CALL(*m_mock, process(
		ResultOf([=](auto in) {
			return (in.header.opcode == FUSE_SETLK &&
				in.header.nodeid == ino &&
				in.body.setlk.fh == FH &&
				in.body.setlk.owner == (uint32_t)pid &&
				in.body.setlk.lk.start == 10 &&
				in.body.setlk.lk.end == 1009 &&
				in.body.setlk.lk.type == F_RDLCK &&
				in.body.setlk.lk.pid == (uint64_t)pid);
		}, Eq(true)),
		_)
	).WillOnce(Invoke(ReturnErrno(EAGAIN)));

	fd = open(FULLPATH, O_RDWR);
	ASSERT_LE(0, fd) << strerror(errno);
	fl.l_start = 10;
	fl.l_len = 1000;
	fl.l_pid = pid;
	fl.l_type = F_RDLCK;
	fl.l_whence = SEEK_SET;
	fl.l_sysid = 0;
	ASSERT_EQ(-1, fcntl(fd, F_SETLK, &fl));
	ASSERT_EQ(EAGAIN, errno);
	/* Deliberately leak fd.  close(2) will be tested in release.cc */
}

/*
 * If the fuse filesystem does not support posix file locks, then the kernel
 * should fall back to local locks.
 */
TEST_F(SetlkwFallback, local)
{
	const char FULLPATH[] = "mountpoint/some_file.txt";
	const char RELPATH[] = "some_file.txt";
	uint64_t ino = 42;
	struct flock fl;
	int fd;

	expect_lookup(RELPATH, ino);
	expect_open(ino, 0, 1);

	fd = open(FULLPATH, O_RDWR);
	ASSERT_LE(0, fd) << strerror(errno);
	fl.l_start = 10;
	fl.l_len = 1000;
	fl.l_pid = getpid();
	fl.l_type = F_RDLCK;
	fl.l_whence = SEEK_SET;
	fl.l_sysid = 0;
	ASSERT_NE(-1, fcntl(fd, F_SETLKW, &fl)) << strerror(errno);
	/* Deliberately leak fd.  close(2) will be tested in release.cc */
}

/*
 * Set a new lock with FUSE_SETLK.  If the lock is not available, then the
 * command should block.  But to the kernel, that's the same as just being
 * slow, so we don't need a separate test method
 */
TEST_F(Setlkw, set)
{
	const char FULLPATH[] = "mountpoint/some_file.txt";
	const char RELPATH[] = "some_file.txt";
	uint64_t ino = 42;
	struct flock fl;
	int fd;
	pid_t pid = 1234;

	expect_lookup(RELPATH, ino);
	expect_open(ino, 0, 1);
	EXPECT_CALL(*m_mock, process(
		ResultOf([=](auto in) {
			return (in.header.opcode == FUSE_SETLK &&
				in.header.nodeid == ino &&
				in.body.setlkw.fh == FH &&
				in.body.setlkw.owner == (uint32_t)pid &&
				in.body.setlkw.lk.start == 10 &&
				in.body.setlkw.lk.end == 1009 &&
				in.body.setlkw.lk.type == F_RDLCK &&
				in.body.setlkw.lk.pid == (uint64_t)pid);
		}, Eq(true)),
		_)
	).WillOnce(Invoke(ReturnErrno(0)));

	fd = open(FULLPATH, O_RDWR);
	ASSERT_LE(0, fd) << strerror(errno);
	fl.l_start = 10;
	fl.l_len = 1000;
	fl.l_pid = pid;
	fl.l_type = F_RDLCK;
	fl.l_whence = SEEK_SET;
	fl.l_sysid = 0;
	ASSERT_NE(-1, fcntl(fd, F_SETLKW, &fl)) << strerror(errno);
	/* Deliberately leak fd.  close(2) will be tested in release.cc */
}
