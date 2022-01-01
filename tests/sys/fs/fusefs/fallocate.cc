/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
 * Copyright (c) 2021 Alan Somers
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
#include <sys/param.h>
#include <sys/mount.h>
#include <sys/resource.h>
#include <sys/time.h>

#include <fcntl.h>
#include <signal.h>
#include <unistd.h>

#include "mntopts.h"	// for build_iovec
}

#include "mockfs.hh"
#include "utils.hh"

using namespace testing;

class Fallocate: public FuseTest{};

class PosixFallocate: public Fallocate {
public:
static sig_atomic_t s_sigxfsz;

void SetUp() {
	s_sigxfsz = 0;
	FuseTest::SetUp();
}

void TearDown() {
	struct sigaction sa;

	bzero(&sa, sizeof(sa));
	sa.sa_handler = SIG_DFL;
	sigaction(SIGXFSZ, &sa, NULL);

	Fallocate::TearDown();
}

};

sig_atomic_t PosixFallocate::s_sigxfsz = 0;

void sigxfsz_handler(int __unused sig) {
	PosixFallocate::s_sigxfsz = 1;
}

class PosixFallocate_7_18: public PosixFallocate {
public:
virtual void SetUp() {
	m_kernel_minor_version = 18;
	PosixFallocate::SetUp();
}
};


/*
 * If the server returns ENOSYS, it indicates that the server does not support
 * FUSE_FALLOCATE.  This and future calls should return EINVAL.
 */
TEST_F(PosixFallocate, enosys)
{
	const char FULLPATH[] = "mountpoint/some_file.txt";
	const char RELPATH[] = "some_file.txt";
	uint64_t ino = 42;
	uint64_t offset = 0;
	uint64_t length = 1000;
	int fd;

	expect_lookup(RELPATH, ino, S_IFREG | 0644, 0, 1);
	expect_open(ino, 0, 1);
	expect_fallocate(ino, offset, length, 0, ENOSYS);

	fd = open(FULLPATH, O_RDWR);
	ASSERT_LE(0, fd) << strerror(errno);
	EXPECT_EQ(EINVAL, posix_fallocate(fd, offset, length));

	/* Subsequent calls shouldn't query the daemon*/
	EXPECT_EQ(EINVAL, posix_fallocate(fd, offset, length));

	leak(fd);
}

/*
 * EOPNOTSUPP means either "the file system does not support fallocate" or "the
 * file system does not support fallocate with the supplied mode".  fusefs
 * should conservatively assume the latter, and not issue any more fallocate
 * operations with the same mode.
 */
TEST_F(PosixFallocate, eopnotsupp)
{
	const char FULLPATH[] = "mountpoint/some_file.txt";
	const char RELPATH[] = "some_file.txt";
	uint64_t ino = 42;
	uint64_t offset = 0;
	uint64_t length = 1000;
	int fd;

	expect_lookup(RELPATH, ino, S_IFREG | 0644, 0, 1);
	expect_open(ino, 0, 1);
	expect_fallocate(ino, offset, length, 0, EOPNOTSUPP);

	fd = open(FULLPATH, O_RDWR);
	ASSERT_LE(0, fd) << strerror(errno);
	EXPECT_EQ(EINVAL, posix_fallocate(fd, offset, length));

	/* Subsequent calls shouldn't query the daemon*/
	EXPECT_EQ(EINVAL, posix_fallocate(fd, offset, length));

	leak(fd);
}

/* EIO is not a permanent error, and may be retried */
TEST_F(PosixFallocate, eio)
{
	const char FULLPATH[] = "mountpoint/some_file.txt";
	const char RELPATH[] = "some_file.txt";
	uint64_t ino = 42;
	uint64_t offset = 0;
	uint64_t length = 1000;
	int fd;

	expect_lookup(RELPATH, ino, S_IFREG | 0644, 0, 1);
	expect_open(ino, 0, 1);
	expect_fallocate(ino, offset, length, 0, EIO);

	fd = open(FULLPATH, O_RDWR);
	ASSERT_LE(0, fd) << strerror(errno);
	EXPECT_EQ(EIO, posix_fallocate(fd, offset, length));

	expect_fallocate(ino, offset, length, 0, 0);

	EXPECT_EQ(0, posix_fallocate(fd, offset, length));

	leak(fd);
}

TEST_F(PosixFallocate, erofs)
{
	const char FULLPATH[] = "mountpoint/some_file.txt";
	const char RELPATH[] = "some_file.txt";
	struct statfs statbuf;
	struct iovec *iov = NULL;
	int iovlen = 0;
	uint64_t ino = 42;
	uint64_t offset = 0;
	uint64_t length = 1000;
	int fd;
	int newflags;

	expect_lookup(RELPATH, ino, S_IFREG | 0644, 0, 1);
	expect_open(ino, 0, 1);
	EXPECT_CALL(*m_mock, process(
		ResultOf([](auto in) {
			return (in.header.opcode == FUSE_STATFS);
		}, Eq(true)),
		_)
	).WillRepeatedly(Invoke(ReturnImmediate([=](auto in __unused, auto& out)
	{
		/*
		 * All of the fields except f_flags are don't care, and f_flags
		 * is set by the VFS
		 */
		SET_OUT_HEADER_LEN(out, statfs);
	})));

	fd = open(FULLPATH, O_RDWR);
	ASSERT_LE(0, fd) << strerror(errno);

	/* Remount read-only */
	ASSERT_EQ(0, statfs("mountpoint", &statbuf)) << strerror(errno);
	newflags = statbuf.f_flags | MNT_UPDATE | MNT_RDONLY;
	build_iovec(&iov, &iovlen, "fstype", (void*)statbuf.f_fstypename, -1);
	build_iovec(&iov, &iovlen, "fspath", (void*)statbuf.f_mntonname, -1);
	build_iovec(&iov, &iovlen, "from", __DECONST(void *, "/dev/fuse"), -1);
	ASSERT_EQ(0, nmount(iov, iovlen, newflags)) << strerror(errno);

	EXPECT_EQ(EROFS, posix_fallocate(fd, offset, length));

	leak(fd);
}

TEST_F(PosixFallocate, ok)
{
	const char FULLPATH[] = "mountpoint/some_file.txt";
	const char RELPATH[] = "some_file.txt";
	struct stat sb0, sb1;
	uint64_t ino = 42;
	uint64_t offset = 0;
	uint64_t length = 1000;
	int fd;

	EXPECT_LOOKUP(FUSE_ROOT_ID, RELPATH)
	.WillOnce(Invoke(ReturnImmediate([=](auto in __unused, auto& out) {
		SET_OUT_HEADER_LEN(out, entry);
		out.body.entry.attr.mode = S_IFREG | 0644;
		out.body.entry.nodeid = ino;
		out.body.entry.entry_valid = UINT64_MAX;
		out.body.entry.attr_valid = UINT64_MAX;
	})));
	expect_open(ino, 0, 1);
	expect_fallocate(ino, offset, length, 0, 0);

	fd = open(FULLPATH, O_RDWR);
	ASSERT_LE(0, fd) << strerror(errno);
	ASSERT_EQ(0, fstat(fd, &sb0)) << strerror(errno);
	EXPECT_EQ(0, posix_fallocate(fd, offset, length));
	/*
	 * Despite the originally cached file size of zero, stat should now
	 * return either the new size or requery the daemon.
	 */
	EXPECT_EQ(0, stat(FULLPATH, &sb1));
	EXPECT_EQ(length, (uint64_t)sb1.st_size);

	/* mtime and ctime should be updated */
	EXPECT_EQ(sb0.st_atime, sb1.st_atime);
	EXPECT_NE(sb0.st_mtime, sb1.st_mtime);
	EXPECT_NE(sb0.st_ctime, sb1.st_ctime);

	leak(fd);
}

/* fusefs should respect RLIMIT_FSIZE */
TEST_F(PosixFallocate, rlimit_fsize)
{
	const char FULLPATH[] = "mountpoint/some_file.txt";
	const char RELPATH[] = "some_file.txt";
	struct rlimit rl;
	uint64_t ino = 42;
	uint64_t offset = 0;
	uint64_t length = 1'000'000;
	int fd;

	expect_lookup(RELPATH, ino, S_IFREG | 0644, 0, 1);
	expect_open(ino, 0, 1);

	rl.rlim_cur = length / 2;
	rl.rlim_max = 10 * length;
	ASSERT_EQ(0, setrlimit(RLIMIT_FSIZE, &rl)) << strerror(errno);
	ASSERT_NE(SIG_ERR, signal(SIGXFSZ, sigxfsz_handler)) << strerror(errno);

	fd = open(FULLPATH, O_RDWR);
	ASSERT_LE(0, fd) << strerror(errno);
	EXPECT_EQ(EFBIG, posix_fallocate(fd, offset, length));
	EXPECT_EQ(1, s_sigxfsz);

	leak(fd);
}

/* With older servers, no FUSE_FALLOCATE should be attempted */
TEST_F(PosixFallocate_7_18, einval)
{
	const char FULLPATH[] = "mountpoint/some_file.txt";
	const char RELPATH[] = "some_file.txt";
	uint64_t ino = 42;
	uint64_t offset = 0;
	uint64_t length = 1000;
	int fd;

	expect_lookup(RELPATH, ino, S_IFREG | 0644, 0, 1);
	expect_open(ino, 0, 1);

	fd = open(FULLPATH, O_RDWR);
	ASSERT_LE(0, fd) << strerror(errno);
	EXPECT_EQ(EINVAL, posix_fallocate(fd, offset, length));

	leak(fd);
}
