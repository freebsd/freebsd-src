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
#include <sys/file.h>
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

void expect_lookup(const char *relpath, uint64_t ino, uint64_t size = 0)
{
	FuseTest::expect_lookup(relpath, ino, S_IFREG | 0644, size, 1);
}

};

/* For testing filesystems with posix locking support */
class Locks: public Fallback {
	virtual void SetUp() {
		m_init_flags = FUSE_POSIX_LOCKS;
		Fallback::SetUp();
	}
};

class Fcntl: public Locks {
public:
void expect_setlk(uint64_t ino, pid_t pid, uint64_t start, uint64_t end,
	uint32_t type, int err)
{
	EXPECT_CALL(*m_mock, process(
		ResultOf([=](auto in) {
			return (in.header.opcode == FUSE_SETLK &&
				in.header.nodeid == ino &&
				in.body.setlk.fh == FH &&
				in.body.setlk.owner == (uint32_t)pid &&
				in.body.setlk.lk.start == start &&
				in.body.setlk.lk.end == end &&
				in.body.setlk.lk.type == type &&
				in.body.setlk.lk.pid == (uint64_t)pid);
		}, Eq(true)),
		_)
	).WillOnce(Invoke(ReturnErrno(err)));
}
void expect_setlkw(uint64_t ino, pid_t pid, uint64_t start, uint64_t end,
	uint32_t type, int err)
{
	EXPECT_CALL(*m_mock, process(
		ResultOf([=](auto in) {
			return (in.header.opcode == FUSE_SETLKW &&
				in.header.nodeid == ino &&
				in.body.setlkw.fh == FH &&
				in.body.setlkw.owner == (uint32_t)pid &&
				in.body.setlkw.lk.start == start &&
				in.body.setlkw.lk.end == end &&
				in.body.setlkw.lk.type == type &&
				in.body.setlkw.lk.pid == (uint64_t)pid);
		}, Eq(true)),
		_)
	).WillOnce(Invoke(ReturnErrno(err)));
}
};

class Flock: public Locks {
public:
void expect_setlk(uint64_t ino, uint32_t type, int err)
{
	EXPECT_CALL(*m_mock, process(
		ResultOf([=](auto in) {
			return (in.header.opcode == FUSE_SETLK &&
				in.header.nodeid == ino &&
				in.body.setlk.fh == FH &&
				/* 
				 * The owner should be set to the address of
				 * the vnode.  That's hard to verify.
				 */
				/* in.body.setlk.owner == ??? && */
				in.body.setlk.lk.type == type);
		}, Eq(true)),
		_)
	).WillOnce(Invoke(ReturnErrno(err)));
}
};

class FlockFallback: public Fallback {};
class GetlkFallback: public Fallback {};
class Getlk: public Fcntl {};
class SetlkFallback: public Fallback {};
class Setlk: public Fcntl {};
class SetlkwFallback: public Fallback {};
class Setlkw: public Fcntl {};

/*
 * If the fuse filesystem does not support flock locks, then the kernel should
 * fall back to local locks.
 */
TEST_F(FlockFallback, local)
{
	const char FULLPATH[] = "mountpoint/some_file.txt";
	const char RELPATH[] = "some_file.txt";
	uint64_t ino = 42;
	int fd;

	expect_lookup(RELPATH, ino);
	expect_open(ino, 0, 1);

	fd = open(FULLPATH, O_RDWR);
	ASSERT_LE(0, fd) << strerror(errno);
	ASSERT_EQ(0, flock(fd, LOCK_EX)) << strerror(errno);
	leak(fd);
}

/*
 * Even if the fuse file system supports POSIX locks, we must implement flock
 * locks locally until protocol 7.17.  Protocol 7.9 added partial buggy support
 * but we won't implement that.
 */
TEST_F(Flock, local)
{
	const char FULLPATH[] = "mountpoint/some_file.txt";
	const char RELPATH[] = "some_file.txt";
	uint64_t ino = 42;
	int fd;

	expect_lookup(RELPATH, ino);
	expect_open(ino, 0, 1);

	fd = open(FULLPATH, O_RDWR);
	ASSERT_LE(0, fd) << strerror(errno);
	ASSERT_EQ(0, flock(fd, LOCK_EX)) << strerror(errno);
	leak(fd);
}

/* Set a new flock lock with FUSE_SETLK */
/* TODO: enable after upgrading to protocol 7.17 */
TEST_F(Flock, DISABLED_set)
{
	const char FULLPATH[] = "mountpoint/some_file.txt";
	const char RELPATH[] = "some_file.txt";
	uint64_t ino = 42;
	int fd;

	expect_lookup(RELPATH, ino);
	expect_open(ino, 0, 1);
	expect_setlk(ino, F_WRLCK, 0);

	fd = open(FULLPATH, O_RDWR);
	ASSERT_LE(0, fd) << strerror(errno);
	ASSERT_EQ(0, flock(fd, LOCK_EX)) << strerror(errno);
	leak(fd);
}

/* Fail to set a flock lock in non-blocking mode */
/* TODO: enable after upgrading to protocol 7.17 */
TEST_F(Flock, DISABLED_eagain)
{
	const char FULLPATH[] = "mountpoint/some_file.txt";
	const char RELPATH[] = "some_file.txt";
	uint64_t ino = 42;
	int fd;

	expect_lookup(RELPATH, ino);
	expect_open(ino, 0, 1);
	expect_setlk(ino, F_WRLCK, EAGAIN);

	fd = open(FULLPATH, O_RDWR);
	ASSERT_LE(0, fd) << strerror(errno);
	ASSERT_NE(0, flock(fd, LOCK_EX | LOCK_NB));
	ASSERT_EQ(EAGAIN, errno);
	leak(fd);
}

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
	fl.l_pid = 0;
	fl.l_type = F_RDLCK;
	fl.l_whence = SEEK_SET;
	fl.l_sysid = 0;
	ASSERT_NE(-1, fcntl(fd, F_GETLK, &fl)) << strerror(errno);
	leak(fd);
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
	pid_t pid = getpid();

	expect_lookup(RELPATH, ino);
	expect_open(ino, 0, 1);
	EXPECT_CALL(*m_mock, process(
		ResultOf([=](auto in) {
			return (in.header.opcode == FUSE_GETLK &&
				in.header.nodeid == ino &&
				in.body.getlk.fh == FH &&
				/*
				 * Though it seems useless, libfuse expects the
				 * owner and pid fields to be set during
				 * FUSE_GETLK.
				 */
				in.body.getlk.owner == (uint32_t)pid &&
				in.body.getlk.lk.pid == (uint64_t)pid &&
				in.body.getlk.lk.start == 10 &&
				in.body.getlk.lk.end == 1009 &&
				in.body.getlk.lk.type == F_RDLCK);
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
	fl.l_pid = 42;
	fl.l_type = F_RDLCK;
	fl.l_whence = SEEK_SET;
	fl.l_sysid = 42;
	ASSERT_NE(-1, fcntl(fd, F_GETLK, &fl)) << strerror(errno);

	/*
	 * If no lock is found that would prevent this lock from being created,
	 * the structure is left unchanged by this system call except for the
	 * lock type which is set to F_UNLCK.
	 */
	ASSERT_EQ(F_UNLCK, fl.l_type);
	ASSERT_EQ(fl.l_pid, 42);
	ASSERT_EQ(fl.l_start, 10);
	ASSERT_EQ(fl.l_len, 1000);
	ASSERT_EQ(fl.l_whence, SEEK_SET);
	ASSERT_EQ(fl.l_sysid, 42);

	leak(fd);
}

/* A different pid does have a lock */
TEST_F(Getlk, lock_exists)
{
	const char FULLPATH[] = "mountpoint/some_file.txt";
	const char RELPATH[] = "some_file.txt";
	uint64_t ino = 42;
	struct flock fl;
	int fd;
	pid_t pid = getpid();
	pid_t pid2 = 1235;

	expect_lookup(RELPATH, ino);
	expect_open(ino, 0, 1);
	EXPECT_CALL(*m_mock, process(
		ResultOf([=](auto in) {
			return (in.header.opcode == FUSE_GETLK &&
				in.header.nodeid == ino &&
				in.body.getlk.fh == FH &&
				/*
				 * Though it seems useless, libfuse expects the
				 * owner and pid fields to be set during
				 * FUSE_GETLK.
				 */
				in.body.getlk.owner == (uint32_t)pid &&
				in.body.getlk.lk.pid == (uint64_t)pid &&
				in.body.getlk.lk.start == 10 &&
				in.body.getlk.lk.end == 1009 &&
				in.body.getlk.lk.type == F_RDLCK);
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
	fl.l_pid = 0;
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
	leak(fd);
}

/*
 * F_GETLK with SEEK_CUR
 */
TEST_F(Getlk, seek_cur)
{
	const char FULLPATH[] = "mountpoint/some_file.txt";
	const char RELPATH[] = "some_file.txt";
	uint64_t ino = 42;
	struct flock fl;
	int fd;
	pid_t pid = getpid();

	expect_lookup(RELPATH, ino, 1024);
	expect_open(ino, 0, 1);
	EXPECT_CALL(*m_mock, process(
		ResultOf([=](auto in) {
			return (in.header.opcode == FUSE_GETLK &&
				in.header.nodeid == ino &&
				in.body.getlk.fh == FH &&
				/*
				 * Though it seems useless, libfuse expects the
				 * owner and pid fields to be set during
				 * FUSE_GETLK.
				 */
				in.body.getlk.owner == (uint32_t)pid &&
				in.body.getlk.lk.pid == (uint64_t)pid &&
				in.body.getlk.lk.start == 500 &&
				in.body.getlk.lk.end == 509 &&
				in.body.getlk.lk.type == F_RDLCK);
		}, Eq(true)),
		_)
	).WillOnce(Invoke(ReturnImmediate([=](auto in __unused, auto& out) {
		SET_OUT_HEADER_LEN(out, getlk);
		out.body.getlk.lk.start = 400;
		out.body.getlk.lk.end = 499;
		out.body.getlk.lk.type = F_WRLCK;
		out.body.getlk.lk.pid = (uint32_t)pid + 1;
	})));

	fd = open(FULLPATH, O_RDWR);
	ASSERT_LE(0, fd) << strerror(errno);
	ASSERT_NE(-1, lseek(fd, 500, SEEK_SET));

	fl.l_start = 0;
	fl.l_len = 10;
	fl.l_pid = 42;
	fl.l_type = F_RDLCK;
	fl.l_whence = SEEK_CUR;
	fl.l_sysid = 0;
	ASSERT_NE(-1, fcntl(fd, F_GETLK, &fl)) << strerror(errno);

	/*
	 * After a successful F_GETLK request, the value of l_whence is
	 * SEEK_SET.
	 */
	EXPECT_EQ(F_WRLCK, fl.l_type);
	EXPECT_EQ(fl.l_pid, pid + 1);
	EXPECT_EQ(fl.l_start, 400);
	EXPECT_EQ(fl.l_len, 100);
	EXPECT_EQ(fl.l_whence, SEEK_SET);
	ASSERT_EQ(fl.l_sysid, 0);

	leak(fd);
}

/*
 * F_GETLK with SEEK_END
 */
TEST_F(Getlk, seek_end)
{
	const char FULLPATH[] = "mountpoint/some_file.txt";
	const char RELPATH[] = "some_file.txt";
	uint64_t ino = 42;
	struct flock fl;
	int fd;
	pid_t pid = getpid();

	expect_lookup(RELPATH, ino, 1024);
	expect_open(ino, 0, 1);
	EXPECT_CALL(*m_mock, process(
		ResultOf([=](auto in) {
			return (in.header.opcode == FUSE_GETLK &&
				in.header.nodeid == ino &&
				in.body.getlk.fh == FH &&
				/*
				 * Though it seems useless, libfuse expects the
				 * owner and pid fields to be set during
				 * FUSE_GETLK.
				 */
				in.body.getlk.owner == (uint32_t)pid &&
				in.body.getlk.lk.pid == (uint64_t)pid &&
				in.body.getlk.lk.start == 512 &&
				in.body.getlk.lk.end == 1023 &&
				in.body.getlk.lk.type == F_RDLCK);
		}, Eq(true)),
		_)
	).WillOnce(Invoke(ReturnImmediate([=](auto in __unused, auto& out) {
		SET_OUT_HEADER_LEN(out, getlk);
		out.body.getlk.lk.start = 400;
		out.body.getlk.lk.end = 499;
		out.body.getlk.lk.type = F_WRLCK;
		out.body.getlk.lk.pid = (uint32_t)pid + 1;
	})));

	fd = open(FULLPATH, O_RDWR);
	ASSERT_LE(0, fd) << strerror(errno);
	ASSERT_NE(-1, lseek(fd, 500, SEEK_SET));

	fl.l_start = -512;
	fl.l_len = 512;
	fl.l_pid = 42;
	fl.l_type = F_RDLCK;
	fl.l_whence = SEEK_END;
	fl.l_sysid = 0;
	ASSERT_NE(-1, fcntl(fd, F_GETLK, &fl)) << strerror(errno);

	/*
	 * After a successful F_GETLK request, the value of l_whence is
	 * SEEK_SET.
	 */
	EXPECT_EQ(F_WRLCK, fl.l_type);
	EXPECT_EQ(fl.l_pid, pid + 1);
	EXPECT_EQ(fl.l_start, 400);
	EXPECT_EQ(fl.l_len, 100);
	EXPECT_EQ(fl.l_whence, SEEK_SET);
	ASSERT_EQ(fl.l_sysid, 0);

	leak(fd);
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
	leak(fd);
}

/* Clear a lock with FUSE_SETLK */
TEST_F(Setlk, clear)
{
	const char FULLPATH[] = "mountpoint/some_file.txt";
	const char RELPATH[] = "some_file.txt";
	uint64_t ino = 42;
	struct flock fl;
	int fd;
	pid_t pid = getpid();

	expect_lookup(RELPATH, ino);
	expect_open(ino, 0, 1);
	expect_setlk(ino, pid, 10, 1009, F_UNLCK, 0);

	fd = open(FULLPATH, O_RDWR);
	ASSERT_LE(0, fd) << strerror(errno);
	fl.l_start = 10;
	fl.l_len = 1000;
	fl.l_pid = 0;
	fl.l_type = F_UNLCK;
	fl.l_whence = SEEK_SET;
	fl.l_sysid = 0;
	ASSERT_NE(-1, fcntl(fd, F_SETLK, &fl)) << strerror(errno);
	leak(fd);
}

/* Set a new lock with FUSE_SETLK */
TEST_F(Setlk, set)
{
	const char FULLPATH[] = "mountpoint/some_file.txt";
	const char RELPATH[] = "some_file.txt";
	uint64_t ino = 42;
	struct flock fl;
	int fd;
	pid_t pid = getpid();

	expect_lookup(RELPATH, ino);
	expect_open(ino, 0, 1);
	expect_setlk(ino, pid, 10, 1009, F_RDLCK, 0);

	fd = open(FULLPATH, O_RDWR);
	ASSERT_LE(0, fd) << strerror(errno);
	fl.l_start = 10;
	fl.l_len = 1000;
	fl.l_pid = 0;
	fl.l_type = F_RDLCK;
	fl.l_whence = SEEK_SET;
	fl.l_sysid = 0;
	ASSERT_NE(-1, fcntl(fd, F_SETLK, &fl)) << strerror(errno);
	leak(fd);
}

/* l_len = 0 is a flag value that means to lock until EOF */
TEST_F(Setlk, set_eof)
{
	const char FULLPATH[] = "mountpoint/some_file.txt";
	const char RELPATH[] = "some_file.txt";
	uint64_t ino = 42;
	struct flock fl;
	int fd;
	pid_t pid = getpid();

	expect_lookup(RELPATH, ino);
	expect_open(ino, 0, 1);
	expect_setlk(ino, pid, 10, OFFSET_MAX, F_RDLCK, 0);

	fd = open(FULLPATH, O_RDWR);
	ASSERT_LE(0, fd) << strerror(errno);
	fl.l_start = 10;
	fl.l_len = 0;
	fl.l_pid = 0;
	fl.l_type = F_RDLCK;
	fl.l_whence = SEEK_SET;
	fl.l_sysid = 0;
	ASSERT_NE(-1, fcntl(fd, F_SETLK, &fl)) << strerror(errno);
	leak(fd);
}

/* Set a new lock with FUSE_SETLK, using SEEK_CUR for l_whence */
TEST_F(Setlk, set_seek_cur)
{
	const char FULLPATH[] = "mountpoint/some_file.txt";
	const char RELPATH[] = "some_file.txt";
	uint64_t ino = 42;
	struct flock fl;
	int fd;
	pid_t pid = getpid();

	expect_lookup(RELPATH, ino, 1024);
	expect_open(ino, 0, 1);
	expect_setlk(ino, pid, 500, 509, F_RDLCK, 0);

	fd = open(FULLPATH, O_RDWR);
	ASSERT_LE(0, fd) << strerror(errno);
	ASSERT_NE(-1, lseek(fd, 500, SEEK_SET));

	fl.l_start = 0;
	fl.l_len = 10;
	fl.l_pid = 0;
	fl.l_type = F_RDLCK;
	fl.l_whence = SEEK_CUR;
	fl.l_sysid = 0;
	ASSERT_NE(-1, fcntl(fd, F_SETLK, &fl)) << strerror(errno);

	leak(fd);
}

/* Set a new lock with FUSE_SETLK, using SEEK_END for l_whence */
TEST_F(Setlk, set_seek_end)
{
	const char FULLPATH[] = "mountpoint/some_file.txt";
	const char RELPATH[] = "some_file.txt";
	uint64_t ino = 42;
	struct flock fl;
	int fd;
	pid_t pid = getpid();

	expect_lookup(RELPATH, ino, 1024);
	expect_open(ino, 0, 1);
	expect_setlk(ino, pid, 1000, 1009, F_RDLCK, 0);

	fd = open(FULLPATH, O_RDWR);
	ASSERT_LE(0, fd) << strerror(errno);

	fl.l_start = -24;
	fl.l_len = 10;
	fl.l_pid = 0;
	fl.l_type = F_RDLCK;
	fl.l_whence = SEEK_END;
	fl.l_sysid = 0;
	ASSERT_NE(-1, fcntl(fd, F_SETLK, &fl)) << strerror(errno);

	leak(fd);
}

/* Fail to set a new lock with FUSE_SETLK due to a conflict */
TEST_F(Setlk, eagain)
{
	const char FULLPATH[] = "mountpoint/some_file.txt";
	const char RELPATH[] = "some_file.txt";
	uint64_t ino = 42;
	struct flock fl;
	int fd;
	pid_t pid = getpid();

	expect_lookup(RELPATH, ino);
	expect_open(ino, 0, 1);
	expect_setlk(ino, pid, 10, 1009, F_RDLCK, EAGAIN);

	fd = open(FULLPATH, O_RDWR);
	ASSERT_LE(0, fd) << strerror(errno);
	fl.l_start = 10;
	fl.l_len = 1000;
	fl.l_pid = 0;
	fl.l_type = F_RDLCK;
	fl.l_whence = SEEK_SET;
	fl.l_sysid = 0;
	ASSERT_EQ(-1, fcntl(fd, F_SETLK, &fl));
	ASSERT_EQ(EAGAIN, errno);
	leak(fd);
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
	fl.l_pid = 0;
	fl.l_type = F_RDLCK;
	fl.l_whence = SEEK_SET;
	fl.l_sysid = 0;
	ASSERT_NE(-1, fcntl(fd, F_SETLKW, &fl)) << strerror(errno);
	leak(fd);
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
	pid_t pid = getpid();

	expect_lookup(RELPATH, ino);
	expect_open(ino, 0, 1);
	expect_setlkw(ino, pid, 10, 1009, F_RDLCK, 0);

	fd = open(FULLPATH, O_RDWR);
	ASSERT_LE(0, fd) << strerror(errno);
	fl.l_start = 10;
	fl.l_len = 1000;
	fl.l_pid = 0;
	fl.l_type = F_RDLCK;
	fl.l_whence = SEEK_SET;
	fl.l_sysid = 0;
	ASSERT_NE(-1, fcntl(fd, F_SETLKW, &fl)) << strerror(errno);
	leak(fd);
}
