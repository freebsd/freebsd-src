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
#include <sys/param.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/sysctl.h>
#include <sys/uio.h>

#include <aio.h>
#include <fcntl.h>
#include <semaphore.h>
#include <setjmp.h>
#include <signal.h>
#include <unistd.h>
}

#include "mockfs.hh"
#include "utils.hh"

using namespace testing;

class Read: public FuseTest {

public:
void expect_lookup(const char *relpath, uint64_t ino, uint64_t size)
{
	FuseTest::expect_lookup(relpath, ino, S_IFREG | 0644, size, 1);
}
};

class RofsRead: public Read {
public:
virtual void SetUp() {
	m_ro = true;
	Read::SetUp();
}
};

class Read_7_8: public FuseTest {
public:
virtual void SetUp() {
	m_kernel_minor_version = 8;
	FuseTest::SetUp();
}

void expect_lookup(const char *relpath, uint64_t ino, uint64_t size)
{
	FuseTest::expect_lookup_7_8(relpath, ino, S_IFREG | 0644, size, 1);
}
};

class AioRead: public Read {
public:
virtual void SetUp() {
	if (!is_unsafe_aio_enabled())
		GTEST_SKIP() <<
			"vfs.aio.enable_unsafe must be set for this test";
	FuseTest::SetUp();
}
};

class AsyncRead: public AioRead {
	virtual void SetUp() {
		m_init_flags = FUSE_ASYNC_READ;
		AioRead::SetUp();
	}
};

class ReadAhead: public Read,
		 public WithParamInterface<tuple<bool, int>>
{
	virtual void SetUp() {
		int val;
		const char *node = "vfs.maxbcachebuf";
		size_t size = sizeof(val);
		ASSERT_EQ(0, sysctlbyname(node, &val, &size, NULL, 0))
			<< strerror(errno);

		m_maxreadahead = val * get<1>(GetParam());
		m_noclusterr = get<0>(GetParam());
		Read::SetUp();
	}
};

class ReadNoatime: public Read {
	virtual void SetUp() {
		m_noatime = true;
		Read::SetUp();
	}
};

class ReadSigbus: public Read
{
public:
static jmp_buf s_jmpbuf;
static void *s_si_addr;

void TearDown() {
	struct sigaction sa;

	bzero(&sa, sizeof(sa));
	sa.sa_handler = SIG_DFL;
	sigaction(SIGBUS, &sa, NULL);

	FuseTest::TearDown();
}

};

static void
handle_sigbus(int signo __unused, siginfo_t *info, void *uap __unused) {
	ReadSigbus::s_si_addr = info->si_addr;
	longjmp(ReadSigbus::s_jmpbuf, 1);
}

jmp_buf ReadSigbus::s_jmpbuf;
void *ReadSigbus::s_si_addr;

class TimeGran: public Read, public WithParamInterface<unsigned> {
public:
virtual void SetUp() {
	m_time_gran = 1 << GetParam();
	Read::SetUp();
}
};

/* AIO reads need to set the header's pid field correctly */
/* https://bugs.freebsd.org/bugzilla/show_bug.cgi?id=236379 */
TEST_F(AioRead, aio_read)
{
	const char FULLPATH[] = "mountpoint/some_file.txt";
	const char RELPATH[] = "some_file.txt";
	const char *CONTENTS = "abcdefgh";
	uint64_t ino = 42;
	int fd;
	ssize_t bufsize = strlen(CONTENTS);
	uint8_t buf[bufsize];
	struct aiocb iocb, *piocb;

	expect_lookup(RELPATH, ino, bufsize);
	expect_open(ino, 0, 1);
	expect_read(ino, 0, bufsize, bufsize, CONTENTS);

	fd = open(FULLPATH, O_RDONLY);
	ASSERT_LE(0, fd) << strerror(errno);

	iocb.aio_nbytes = bufsize;
	iocb.aio_fildes = fd;
	iocb.aio_buf = buf;
	iocb.aio_offset = 0;
	iocb.aio_sigevent.sigev_notify = SIGEV_NONE;
	ASSERT_EQ(0, aio_read(&iocb)) << strerror(errno);
	ASSERT_EQ(bufsize, aio_waitcomplete(&piocb, NULL)) << strerror(errno);
	ASSERT_EQ(0, memcmp(buf, CONTENTS, bufsize));

	leak(fd);
}

/* 
 * Without the FUSE_ASYNC_READ mount option, fuse(4) should ensure that there
 * is at most one outstanding read operation per file handle
 */
TEST_F(AioRead, async_read_disabled)
{
	const char FULLPATH[] = "mountpoint/some_file.txt";
	const char RELPATH[] = "some_file.txt";
	uint64_t ino = 42;
	int fd;
	ssize_t bufsize = 50;
	char buf0[bufsize], buf1[bufsize];
	off_t off0 = 0;
	off_t off1 = m_maxbcachebuf;
	struct aiocb iocb0, iocb1;
	volatile sig_atomic_t read_count = 0;

	expect_lookup(RELPATH, ino, 131072);
	expect_open(ino, 0, 1);
	EXPECT_CALL(*m_mock, process(
		ResultOf([=](auto in) {
			return (in.header.opcode == FUSE_READ &&
				in.header.nodeid == ino &&
				in.body.read.fh == FH &&
				in.body.read.offset == (uint64_t)off0);
		}, Eq(true)),
		_)
	).WillRepeatedly(Invoke([&](auto in __unused, auto &out __unused) {
		read_count++;
		/* Filesystem is slow to respond */
	}));
	EXPECT_CALL(*m_mock, process(
		ResultOf([=](auto in) {
			return (in.header.opcode == FUSE_READ &&
				in.header.nodeid == ino &&
				in.body.read.fh == FH &&
				in.body.read.offset == (uint64_t)off1);
		}, Eq(true)),
		_)
	).WillRepeatedly(Invoke([&](auto in __unused, auto &out __unused) {
		read_count++;
		/* Filesystem is slow to respond */
	}));

	fd = open(FULLPATH, O_RDONLY);
	ASSERT_LE(0, fd) << strerror(errno);

	/* 
	 * Submit two AIO read requests, and respond to neither.  If the
	 * filesystem ever gets the second read request, then we failed to
	 * limit outstanding reads.
	 */
	iocb0.aio_nbytes = bufsize;
	iocb0.aio_fildes = fd;
	iocb0.aio_buf = buf0;
	iocb0.aio_offset = off0;
	iocb0.aio_sigevent.sigev_notify = SIGEV_NONE;
	ASSERT_EQ(0, aio_read(&iocb0)) << strerror(errno);

	iocb1.aio_nbytes = bufsize;
	iocb1.aio_fildes = fd;
	iocb1.aio_buf = buf1;
	iocb1.aio_offset = off1;
	iocb1.aio_sigevent.sigev_notify = SIGEV_NONE;
	ASSERT_EQ(0, aio_read(&iocb1)) << strerror(errno);

	/* 
	 * Sleep for awhile to make sure the kernel has had a chance to issue
	 * the second read, even though the first has not yet returned
	 */
	nap();
	EXPECT_EQ(read_count, 1);
	
	m_mock->kill_daemon();
	/* Wait for AIO activity to complete, but ignore errors */
	(void)aio_waitcomplete(NULL, NULL);

	leak(fd);
}

/* 
 * With the FUSE_ASYNC_READ mount option, fuse(4) may issue multiple
 * simultaneous read requests on the same file handle.
 */
TEST_F(AsyncRead, async_read)
{
	const char FULLPATH[] = "mountpoint/some_file.txt";
	const char RELPATH[] = "some_file.txt";
	uint64_t ino = 42;
	int fd;
	ssize_t bufsize = 50;
	char buf0[bufsize], buf1[bufsize];
	off_t off0 = 0;
	off_t off1 = m_maxbcachebuf;
	off_t fsize = 2 * m_maxbcachebuf;
	struct aiocb iocb0, iocb1;
	sem_t sem;

	ASSERT_EQ(0, sem_init(&sem, 0, 0)) << strerror(errno);

	expect_lookup(RELPATH, ino, fsize);
	expect_open(ino, 0, 1);
	EXPECT_CALL(*m_mock, process(
		ResultOf([=](auto in) {
			return (in.header.opcode == FUSE_READ &&
				in.header.nodeid == ino &&
				in.body.read.fh == FH &&
				in.body.read.offset == (uint64_t)off0);
		}, Eq(true)),
		_)
	).WillOnce(Invoke([&](auto in __unused, auto &out __unused) {
		sem_post(&sem);
		/* Filesystem is slow to respond */
	}));
	EXPECT_CALL(*m_mock, process(
		ResultOf([=](auto in) {
			return (in.header.opcode == FUSE_READ &&
				in.header.nodeid == ino &&
				in.body.read.fh == FH &&
				in.body.read.offset == (uint64_t)off1);
		}, Eq(true)),
		_)
	).WillOnce(Invoke([&](auto in __unused, auto &out __unused) {
		sem_post(&sem);
		/* Filesystem is slow to respond */
	}));

	fd = open(FULLPATH, O_RDONLY);
	ASSERT_LE(0, fd) << strerror(errno);

	/* 
	 * Submit two AIO read requests, but respond to neither.  Ensure that
	 * we received both.
	 */
	iocb0.aio_nbytes = bufsize;
	iocb0.aio_fildes = fd;
	iocb0.aio_buf = buf0;
	iocb0.aio_offset = off0;
	iocb0.aio_sigevent.sigev_notify = SIGEV_NONE;
	ASSERT_EQ(0, aio_read(&iocb0)) << strerror(errno);

	iocb1.aio_nbytes = bufsize;
	iocb1.aio_fildes = fd;
	iocb1.aio_buf = buf1;
	iocb1.aio_offset = off1;
	iocb1.aio_sigevent.sigev_notify = SIGEV_NONE;
	ASSERT_EQ(0, aio_read(&iocb1)) << strerror(errno);

	/* Wait until both reads have reached the daemon */
	ASSERT_EQ(0, sem_wait(&sem)) << strerror(errno);
	ASSERT_EQ(0, sem_wait(&sem)) << strerror(errno);

	m_mock->kill_daemon();
	/* Wait for AIO activity to complete, but ignore errors */
	(void)aio_waitcomplete(NULL, NULL);
	
	leak(fd);
}

/* The kernel should update the cached atime attribute during a read */
TEST_F(Read, atime)
{
	const char FULLPATH[] = "mountpoint/some_file.txt";
	const char RELPATH[] = "some_file.txt";
	const char *CONTENTS = "abcdefgh";
	struct stat sb1, sb2;
	uint64_t ino = 42;
	int fd;
	ssize_t bufsize = strlen(CONTENTS);
	uint8_t buf[bufsize];

	expect_lookup(RELPATH, ino, bufsize);
	expect_open(ino, 0, 1);
	expect_read(ino, 0, bufsize, bufsize, CONTENTS);

	fd = open(FULLPATH, O_RDONLY);
	ASSERT_LE(0, fd) << strerror(errno);
	ASSERT_EQ(0, fstat(fd, &sb1));

	/* Ensure atime will be different than it was during lookup */
	nap();

	ASSERT_EQ(bufsize, read(fd, buf, bufsize)) << strerror(errno);
	ASSERT_EQ(0, fstat(fd, &sb2));

	/* The kernel should automatically update atime during read */
	EXPECT_TRUE(timespeccmp(&sb1.st_atim, &sb2.st_atim, <));
	EXPECT_TRUE(timespeccmp(&sb1.st_ctim, &sb2.st_ctim, ==));
	EXPECT_TRUE(timespeccmp(&sb1.st_mtim, &sb2.st_mtim, ==));

	leak(fd);
}

/* The kernel should update the cached atime attribute during a cached read */
TEST_F(Read, atime_cached)
{
	const char FULLPATH[] = "mountpoint/some_file.txt";
	const char RELPATH[] = "some_file.txt";
	const char *CONTENTS = "abcdefgh";
	struct stat sb1, sb2;
	uint64_t ino = 42;
	int fd;
	ssize_t bufsize = strlen(CONTENTS);
	uint8_t buf[bufsize];

	expect_lookup(RELPATH, ino, bufsize);
	expect_open(ino, 0, 1);
	expect_read(ino, 0, bufsize, bufsize, CONTENTS);

	fd = open(FULLPATH, O_RDONLY);
	ASSERT_LE(0, fd) << strerror(errno);

	ASSERT_EQ(bufsize, pread(fd, buf, bufsize, 0)) << strerror(errno);
	ASSERT_EQ(0, fstat(fd, &sb1));

	/* Ensure atime will be different than it was during the first read */
	nap();

	ASSERT_EQ(bufsize, pread(fd, buf, bufsize, 0)) << strerror(errno);
	ASSERT_EQ(0, fstat(fd, &sb2));

	/* The kernel should automatically update atime during read */
	EXPECT_TRUE(timespeccmp(&sb1.st_atim, &sb2.st_atim, <));
	EXPECT_TRUE(timespeccmp(&sb1.st_ctim, &sb2.st_ctim, ==));
	EXPECT_TRUE(timespeccmp(&sb1.st_mtim, &sb2.st_mtim, ==));

	leak(fd);
}

/* dirty atime values should be flushed during close */
TEST_F(Read, atime_during_close)
{
	const char FULLPATH[] = "mountpoint/some_file.txt";
	const char RELPATH[] = "some_file.txt";
	const char *CONTENTS = "abcdefgh";
	struct stat sb;
	uint64_t ino = 42;
	const mode_t newmode = 0755;
	int fd;
	ssize_t bufsize = strlen(CONTENTS);
	uint8_t buf[bufsize];

	expect_lookup(RELPATH, ino, bufsize);
	expect_open(ino, 0, 1);
	expect_read(ino, 0, bufsize, bufsize, CONTENTS);
	EXPECT_CALL(*m_mock, process(
		ResultOf([&](auto in) {
			uint32_t valid = FATTR_ATIME;
			return (in.header.opcode == FUSE_SETATTR &&
				in.header.nodeid == ino &&
				in.body.setattr.valid == valid &&
				(time_t)in.body.setattr.atime ==
					sb.st_atim.tv_sec &&
				(long)in.body.setattr.atimensec ==
					sb.st_atim.tv_nsec);
		}, Eq(true)),
		_)
	).WillOnce(Invoke(ReturnImmediate([=](auto in __unused, auto& out) {
		SET_OUT_HEADER_LEN(out, attr);
		out.body.attr.attr.ino = ino;
		out.body.attr.attr.mode = S_IFREG | newmode;
	})));
	expect_flush(ino, 1, ReturnErrno(0));
	expect_release(ino, FuseTest::FH);

	fd = open(FULLPATH, O_RDONLY);
	ASSERT_LE(0, fd) << strerror(errno);

	/* Ensure atime will be different than during lookup */
	nap();

	ASSERT_EQ(bufsize, read(fd, buf, bufsize)) << strerror(errno);
	ASSERT_EQ(0, fstat(fd, &sb));

	close(fd);
}

/*
 * When not using -o default_permissions, the daemon may make its own decisions
 * regarding access permissions, and these may be unpredictable.  If it rejects
 * our attempt to set atime, that should not cause close(2) to fail.
 */
TEST_F(Read, atime_during_close_eacces)
{
	const char FULLPATH[] = "mountpoint/some_file.txt";
	const char RELPATH[] = "some_file.txt";
	const char *CONTENTS = "abcdefgh";
	uint64_t ino = 42;
	int fd;
	ssize_t bufsize = strlen(CONTENTS);
	uint8_t buf[bufsize];

	expect_lookup(RELPATH, ino, bufsize);
	expect_open(ino, 0, 1);
	expect_read(ino, 0, bufsize, bufsize, CONTENTS);
	EXPECT_CALL(*m_mock, process(
		ResultOf([&](auto in) {
			uint32_t valid = FATTR_ATIME;
			return (in.header.opcode == FUSE_SETATTR &&
				in.header.nodeid == ino &&
				in.body.setattr.valid == valid);
		}, Eq(true)),
		_)
	).WillOnce(Invoke(ReturnErrno(EACCES)));
	expect_flush(ino, 1, ReturnErrno(0));
	expect_release(ino, FuseTest::FH);

	fd = open(FULLPATH, O_RDONLY);
	ASSERT_LE(0, fd) << strerror(errno);

	/* Ensure atime will be different than during lookup */
	nap();

	ASSERT_EQ(bufsize, read(fd, buf, bufsize)) << strerror(errno);

	ASSERT_EQ(0, close(fd));
}

/* A cached atime should be flushed during FUSE_SETATTR */
TEST_F(Read, atime_during_setattr)
{
	const char FULLPATH[] = "mountpoint/some_file.txt";
	const char RELPATH[] = "some_file.txt";
	const char *CONTENTS = "abcdefgh";
	struct stat sb;
	uint64_t ino = 42;
	const mode_t newmode = 0755;
	int fd;
	ssize_t bufsize = strlen(CONTENTS);
	uint8_t buf[bufsize];

	expect_lookup(RELPATH, ino, bufsize);
	expect_open(ino, 0, 1);
	expect_read(ino, 0, bufsize, bufsize, CONTENTS);
	EXPECT_CALL(*m_mock, process(
		ResultOf([&](auto in) {
			uint32_t valid = FATTR_MODE | FATTR_ATIME;
			return (in.header.opcode == FUSE_SETATTR &&
				in.header.nodeid == ino &&
				in.body.setattr.valid == valid &&
				(time_t)in.body.setattr.atime ==
					sb.st_atim.tv_sec &&
				(long)in.body.setattr.atimensec ==
					sb.st_atim.tv_nsec);
		}, Eq(true)),
		_)
	).WillOnce(Invoke(ReturnImmediate([=](auto in __unused, auto& out) {
		SET_OUT_HEADER_LEN(out, attr);
		out.body.attr.attr.ino = ino;
		out.body.attr.attr.mode = S_IFREG | newmode;
	})));

	fd = open(FULLPATH, O_RDONLY);
	ASSERT_LE(0, fd) << strerror(errno);

	/* Ensure atime will be different than during lookup */
	nap();

	ASSERT_EQ(bufsize, read(fd, buf, bufsize)) << strerror(errno);
	ASSERT_EQ(0, fstat(fd, &sb));
	ASSERT_EQ(0, fchmod(fd, newmode)) << strerror(errno);

	leak(fd);
}

/* 0-length reads shouldn't cause any confusion */
TEST_F(Read, direct_io_read_nothing)
{
	const char FULLPATH[] = "mountpoint/some_file.txt";
	const char RELPATH[] = "some_file.txt";
	uint64_t ino = 42;
	int fd;
	uint64_t offset = 100;
	char buf[80];

	expect_lookup(RELPATH, ino, offset + 1000);
	expect_open(ino, FOPEN_DIRECT_IO, 1);

	fd = open(FULLPATH, O_RDONLY);
	ASSERT_LE(0, fd) << strerror(errno);

	ASSERT_EQ(0, pread(fd, buf, 0, offset)) << strerror(errno);
	leak(fd);
}

/* 
 * With direct_io, reads should not fill the cache.  They should go straight to
 * the daemon
 */
TEST_F(Read, direct_io_pread)
{
	const char FULLPATH[] = "mountpoint/some_file.txt";
	const char RELPATH[] = "some_file.txt";
	const char *CONTENTS = "abcdefgh";
	uint64_t ino = 42;
	int fd;
	uint64_t offset = 100;
	ssize_t bufsize = strlen(CONTENTS);
	uint8_t buf[bufsize];

	expect_lookup(RELPATH, ino, offset + bufsize);
	expect_open(ino, FOPEN_DIRECT_IO, 1);
	expect_read(ino, offset, bufsize, bufsize, CONTENTS);

	fd = open(FULLPATH, O_RDONLY);
	ASSERT_LE(0, fd) << strerror(errno);

	ASSERT_EQ(bufsize, pread(fd, buf, bufsize, offset)) << strerror(errno);
	ASSERT_EQ(0, memcmp(buf, CONTENTS, bufsize));

	// With FOPEN_DIRECT_IO, the cache should be bypassed.  The server will
	// get a 2nd read request.
	expect_read(ino, offset, bufsize, bufsize, CONTENTS);
	ASSERT_EQ(bufsize, pread(fd, buf, bufsize, offset)) << strerror(errno);
	ASSERT_EQ(0, memcmp(buf, CONTENTS, bufsize));
	leak(fd);
}

/* 
 * With direct_io, filesystems are allowed to return less data than is
 * requested.  fuse(4) should return a short read to userland.
 */
TEST_F(Read, direct_io_short_read)
{
	const char FULLPATH[] = "mountpoint/some_file.txt";
	const char RELPATH[] = "some_file.txt";
	const char *CONTENTS = "abcdefghijklmnop";
	uint64_t ino = 42;
	int fd;
	uint64_t offset = 100;
	ssize_t bufsize = strlen(CONTENTS);
	ssize_t halfbufsize = bufsize / 2;
	uint8_t buf[bufsize];

	expect_lookup(RELPATH, ino, offset + bufsize);
	expect_open(ino, FOPEN_DIRECT_IO, 1);
	expect_read(ino, offset, bufsize, halfbufsize, CONTENTS);

	fd = open(FULLPATH, O_RDONLY);
	ASSERT_LE(0, fd) << strerror(errno);

	ASSERT_EQ(halfbufsize, pread(fd, buf, bufsize, offset))
		<< strerror(errno);
	ASSERT_EQ(0, memcmp(buf, CONTENTS, halfbufsize));
	leak(fd);
}

TEST_F(Read, eio)
{
	const char FULLPATH[] = "mountpoint/some_file.txt";
	const char RELPATH[] = "some_file.txt";
	const char *CONTENTS = "abcdefgh";
	uint64_t ino = 42;
	int fd;
	ssize_t bufsize = strlen(CONTENTS);
	uint8_t buf[bufsize];

	expect_lookup(RELPATH, ino, bufsize);
	expect_open(ino, 0, 1);
	EXPECT_CALL(*m_mock, process(
		ResultOf([=](auto in) {
			return (in.header.opcode == FUSE_READ);
		}, Eq(true)),
		_)
	).WillOnce(Invoke(ReturnErrno(EIO)));

	fd = open(FULLPATH, O_RDONLY);
	ASSERT_LE(0, fd) << strerror(errno);

	ASSERT_EQ(-1, read(fd, buf, bufsize)) << strerror(errno);
	ASSERT_EQ(EIO, errno);
	leak(fd);
}

/* 
 * If the server returns a short read when direct io is not in use, that
 * indicates EOF, because of a server-side truncation.  We should invalidate
 * all cached attributes.  We may update the file size, 
 */
TEST_F(Read, eof)
{
	const char FULLPATH[] = "mountpoint/some_file.txt";
	const char RELPATH[] = "some_file.txt";
	const char *CONTENTS = "abcdefghijklmnop";
	uint64_t ino = 42;
	int fd;
	uint64_t offset = 100;
	ssize_t bufsize = strlen(CONTENTS);
	ssize_t partbufsize = 3 * bufsize / 4;
	ssize_t r;
	uint8_t buf[bufsize];
	struct stat sb;

	expect_lookup(RELPATH, ino, offset + bufsize);
	expect_open(ino, 0, 1);
	expect_read(ino, 0, offset + bufsize, offset + partbufsize, CONTENTS);
	expect_getattr(ino, offset + partbufsize);

	fd = open(FULLPATH, O_RDONLY);
	ASSERT_LE(0, fd) << strerror(errno);

	r = pread(fd, buf, bufsize, offset);
	ASSERT_LE(0, r) << strerror(errno);
	EXPECT_EQ(partbufsize, r) << strerror(errno);
	ASSERT_EQ(0, fstat(fd, &sb));
	EXPECT_EQ((off_t)(offset + partbufsize), sb.st_size);
	leak(fd);
}

/* Like Read.eof, but causes an entire buffer to be invalidated */
TEST_F(Read, eof_of_whole_buffer)
{
	const char FULLPATH[] = "mountpoint/some_file.txt";
	const char RELPATH[] = "some_file.txt";
	const char *CONTENTS = "abcdefghijklmnop";
	uint64_t ino = 42;
	int fd;
	ssize_t bufsize = strlen(CONTENTS);
	off_t old_filesize = m_maxbcachebuf * 2 + bufsize;
	uint8_t buf[bufsize];
	struct stat sb;

	expect_lookup(RELPATH, ino, old_filesize);
	expect_open(ino, 0, 1);
	expect_read(ino, 2 * m_maxbcachebuf, bufsize, bufsize, CONTENTS);
	expect_read(ino, m_maxbcachebuf, m_maxbcachebuf, 0, CONTENTS);
	expect_getattr(ino, m_maxbcachebuf);

	fd = open(FULLPATH, O_RDONLY);
	ASSERT_LE(0, fd) << strerror(errno);

	/* Cache the third block */
	ASSERT_EQ(bufsize, pread(fd, buf, bufsize, m_maxbcachebuf * 2))
		<< strerror(errno);
	/* Try to read the 2nd block, but it's past EOF */
	ASSERT_EQ(0, pread(fd, buf, bufsize, m_maxbcachebuf))
		<< strerror(errno);
	ASSERT_EQ(0, fstat(fd, &sb));
	EXPECT_EQ((off_t)(m_maxbcachebuf), sb.st_size);
	leak(fd);
}

/* 
 * With the keep_cache option, the kernel may keep its read cache across
 * multiple open(2)s.
 */
TEST_F(Read, keep_cache)
{
	const char FULLPATH[] = "mountpoint/some_file.txt";
	const char RELPATH[] = "some_file.txt";
	const char *CONTENTS = "abcdefgh";
	uint64_t ino = 42;
	int fd0, fd1;
	ssize_t bufsize = strlen(CONTENTS);
	uint8_t buf[bufsize];

	FuseTest::expect_lookup(RELPATH, ino, S_IFREG | 0644, bufsize, 2);
	expect_open(ino, FOPEN_KEEP_CACHE, 2);
	expect_read(ino, 0, bufsize, bufsize, CONTENTS);

	fd0 = open(FULLPATH, O_RDONLY);
	ASSERT_LE(0, fd0) << strerror(errno);
	ASSERT_EQ(bufsize, read(fd0, buf, bufsize)) << strerror(errno);

	fd1 = open(FULLPATH, O_RDWR);
	ASSERT_LE(0, fd1) << strerror(errno);

	/*
	 * This read should be serviced by cache, even though it's on the other
	 * file descriptor
	 */
	ASSERT_EQ(bufsize, read(fd1, buf, bufsize)) << strerror(errno);

	leak(fd0);
	leak(fd1);
}

/* 
 * Without the keep_cache option, the kernel should drop its read caches on
 * every open
 */
TEST_F(Read, keep_cache_disabled)
{
	const char FULLPATH[] = "mountpoint/some_file.txt";
	const char RELPATH[] = "some_file.txt";
	const char *CONTENTS = "abcdefgh";
	uint64_t ino = 42;
	int fd0, fd1;
	ssize_t bufsize = strlen(CONTENTS);
	uint8_t buf[bufsize];

	FuseTest::expect_lookup(RELPATH, ino, S_IFREG | 0644, bufsize, 2);
	expect_open(ino, 0, 2);
	expect_read(ino, 0, bufsize, bufsize, CONTENTS);

	fd0 = open(FULLPATH, O_RDONLY);
	ASSERT_LE(0, fd0) << strerror(errno);
	ASSERT_EQ(bufsize, read(fd0, buf, bufsize)) << strerror(errno);

	fd1 = open(FULLPATH, O_RDWR);
	ASSERT_LE(0, fd1) << strerror(errno);

	/*
	 * This read should not be serviced by cache, even though it's on the
	 * original file descriptor
	 */
	expect_read(ino, 0, bufsize, bufsize, CONTENTS);
	ASSERT_EQ(0, lseek(fd0, 0, SEEK_SET)) << strerror(errno);
	ASSERT_EQ(bufsize, read(fd0, buf, bufsize)) << strerror(errno);

	leak(fd0);
	leak(fd1);
}

TEST_F(Read, mmap)
{
	const char FULLPATH[] = "mountpoint/some_file.txt";
	const char RELPATH[] = "some_file.txt";
	const char *CONTENTS = "abcdefgh";
	uint64_t ino = 42;
	int fd;
	ssize_t len;
	size_t bufsize = strlen(CONTENTS);
	void *p;

	len = getpagesize();

	expect_lookup(RELPATH, ino, bufsize);
	expect_open(ino, 0, 1);
	EXPECT_CALL(*m_mock, process(
		ResultOf([=](auto in) {
			return (in.header.opcode == FUSE_READ &&
				in.header.nodeid == ino &&
				in.body.read.fh == Read::FH &&
				in.body.read.offset == 0 &&
				in.body.read.size == bufsize);
		}, Eq(true)),
		_)
	).WillOnce(Invoke(ReturnImmediate([=](auto in __unused, auto& out) {
		out.header.len = sizeof(struct fuse_out_header) + bufsize;
		memmove(out.body.bytes, CONTENTS, bufsize);
	})));

	fd = open(FULLPATH, O_RDONLY);
	ASSERT_LE(0, fd) << strerror(errno);

	p = mmap(NULL, len, PROT_READ, MAP_SHARED, fd, 0);
	ASSERT_NE(MAP_FAILED, p) << strerror(errno);

	ASSERT_EQ(0, memcmp(p, CONTENTS, bufsize));

	ASSERT_EQ(0, munmap(p, len)) << strerror(errno);
	leak(fd);
}

/*
 * The kernel should not update the cached atime attribute during a read, if
 * MNT_NOATIME is used.
 */
TEST_F(ReadNoatime, atime)
{
	const char FULLPATH[] = "mountpoint/some_file.txt";
	const char RELPATH[] = "some_file.txt";
	const char *CONTENTS = "abcdefgh";
	struct stat sb1, sb2;
	uint64_t ino = 42;
	int fd;
	ssize_t bufsize = strlen(CONTENTS);
	uint8_t buf[bufsize];

	expect_lookup(RELPATH, ino, bufsize);
	expect_open(ino, 0, 1);
	expect_read(ino, 0, bufsize, bufsize, CONTENTS);

	fd = open(FULLPATH, O_RDONLY);
	ASSERT_LE(0, fd) << strerror(errno);
	ASSERT_EQ(0, fstat(fd, &sb1));

	nap();

	ASSERT_EQ(bufsize, read(fd, buf, bufsize)) << strerror(errno);
	ASSERT_EQ(0, fstat(fd, &sb2));

	/* The kernel should not update atime during read */
	EXPECT_TRUE(timespeccmp(&sb1.st_atim, &sb2.st_atim, ==));
	EXPECT_TRUE(timespeccmp(&sb1.st_ctim, &sb2.st_ctim, ==));
	EXPECT_TRUE(timespeccmp(&sb1.st_mtim, &sb2.st_mtim, ==));

	leak(fd);
}

/*
 * The kernel should not update the cached atime attribute during a cached
 * read, if MNT_NOATIME is used.
 */
TEST_F(ReadNoatime, atime_cached)
{
	const char FULLPATH[] = "mountpoint/some_file.txt";
	const char RELPATH[] = "some_file.txt";
	const char *CONTENTS = "abcdefgh";
	struct stat sb1, sb2;
	uint64_t ino = 42;
	int fd;
	ssize_t bufsize = strlen(CONTENTS);
	uint8_t buf[bufsize];

	expect_lookup(RELPATH, ino, bufsize);
	expect_open(ino, 0, 1);
	expect_read(ino, 0, bufsize, bufsize, CONTENTS);

	fd = open(FULLPATH, O_RDONLY);
	ASSERT_LE(0, fd) << strerror(errno);

	ASSERT_EQ(bufsize, pread(fd, buf, bufsize, 0)) << strerror(errno);
	ASSERT_EQ(0, fstat(fd, &sb1));

	nap();

	ASSERT_EQ(bufsize, pread(fd, buf, bufsize, 0)) << strerror(errno);
	ASSERT_EQ(0, fstat(fd, &sb2));

	/* The kernel should automatically update atime during read */
	EXPECT_TRUE(timespeccmp(&sb1.st_atim, &sb2.st_atim, ==));
	EXPECT_TRUE(timespeccmp(&sb1.st_ctim, &sb2.st_ctim, ==));
	EXPECT_TRUE(timespeccmp(&sb1.st_mtim, &sb2.st_mtim, ==));

	leak(fd);
}

/* Read of an mmap()ed file fails */
TEST_F(ReadSigbus, mmap_eio)
{
	const char FULLPATH[] = "mountpoint/some_file.txt";
	const char RELPATH[] = "some_file.txt";
	const char *CONTENTS = "abcdefgh";
	struct sigaction sa;
	uint64_t ino = 42;
	int fd;
	ssize_t len;
	size_t bufsize = strlen(CONTENTS);
	void *p;

	len = getpagesize();

	expect_lookup(RELPATH, ino, bufsize);
	expect_open(ino, 0, 1);
	EXPECT_CALL(*m_mock, process(
		ResultOf([=](auto in) {
			return (in.header.opcode == FUSE_READ &&
				in.header.nodeid == ino &&
				in.body.read.fh == Read::FH);
		}, Eq(true)),
		_)
	).WillRepeatedly(Invoke(ReturnErrno(EIO)));

	fd = open(FULLPATH, O_RDONLY);
	ASSERT_LE(0, fd) << strerror(errno);

	p = mmap(NULL, len, PROT_READ, MAP_SHARED, fd, 0);
	ASSERT_NE(MAP_FAILED, p) << strerror(errno);

	/* Accessing the mapped page should return SIGBUS.  */

	bzero(&sa, sizeof(sa));
	sa.sa_handler = SIG_DFL;
	sa.sa_sigaction = handle_sigbus;
	sa.sa_flags = SA_RESETHAND | SA_SIGINFO;
	ASSERT_EQ(0, sigaction(SIGBUS, &sa, NULL)) << strerror(errno);
	if (setjmp(ReadSigbus::s_jmpbuf) == 0) {
		atomic_signal_fence(std::memory_order::memory_order_seq_cst);
		volatile char x __unused = *(volatile char*)p;
		FAIL() << "shouldn't get here";
	}

	ASSERT_EQ(p, ReadSigbus::s_si_addr);
	ASSERT_EQ(0, munmap(p, len)) << strerror(errno);
	leak(fd);
}

/* 
 * A read via mmap comes up short, indicating that the file was truncated
 * server-side.
 */
TEST_F(Read, mmap_eof)
{
	const char FULLPATH[] = "mountpoint/some_file.txt";
	const char RELPATH[] = "some_file.txt";
	const char *CONTENTS = "abcdefgh";
	uint64_t ino = 42;
	int fd;
	ssize_t len;
	size_t bufsize = strlen(CONTENTS);
	struct stat sb;
	void *p;

	len = getpagesize();

	expect_lookup(RELPATH, ino, m_maxbcachebuf);
	expect_open(ino, 0, 1);
	EXPECT_CALL(*m_mock, process(
		ResultOf([=](auto in) {
			return (in.header.opcode == FUSE_READ &&
				in.header.nodeid == ino &&
				in.body.read.fh == Read::FH &&
				in.body.read.offset == 0 &&
				in.body.read.size == (uint32_t)m_maxbcachebuf);
		}, Eq(true)),
		_)
	).WillOnce(Invoke(ReturnImmediate([=](auto in __unused, auto& out) {
		out.header.len = sizeof(struct fuse_out_header) + bufsize;
		memmove(out.body.bytes, CONTENTS, bufsize);
	})));
	expect_getattr(ino, bufsize);

	fd = open(FULLPATH, O_RDONLY);
	ASSERT_LE(0, fd) << strerror(errno);

	p = mmap(NULL, len, PROT_READ, MAP_SHARED, fd, 0);
	ASSERT_NE(MAP_FAILED, p) << strerror(errno);

	/* The file size should be automatically truncated */
	ASSERT_EQ(0, memcmp(p, CONTENTS, bufsize));
	ASSERT_EQ(0, fstat(fd, &sb)) << strerror(errno);
	EXPECT_EQ((off_t)bufsize, sb.st_size);

	ASSERT_EQ(0, munmap(p, len)) << strerror(errno);
	leak(fd);
}

/*
 * During VOP_GETPAGES, the FUSE server fails a FUSE_GETATTR operation.  This
 * almost certainly indicates a buggy FUSE server, and our goal should be not
 * to panic.  Instead, generate SIGBUS.
 */
TEST_F(ReadSigbus, mmap_getblksz_fail)
{
	const char FULLPATH[] = "mountpoint/some_file.txt";
	const char RELPATH[] = "some_file.txt";
	const char *CONTENTS = "abcdefgh";
	struct sigaction sa;
	Sequence seq;
	uint64_t ino = 42;
	int fd;
	ssize_t len;
	size_t bufsize = strlen(CONTENTS);
	mode_t mode = S_IFREG | 0644;
	void *p;

	len = getpagesize();

	FuseTest::expect_lookup(RELPATH, ino, mode, bufsize, 1, 0);
	/* Expect two GETATTR calls that succeed, followed by one that fail. */
	EXPECT_CALL(*m_mock, process(
		ResultOf([=](auto in) {
			return (in.header.opcode == FUSE_GETATTR &&
				in.header.nodeid == ino);
		}, Eq(true)),
		_)
	).Times(2)
	.InSequence(seq)
	.WillRepeatedly(Invoke(ReturnImmediate([=](auto i __unused, auto& out) {
		SET_OUT_HEADER_LEN(out, attr);
		out.body.attr.attr.ino = ino;
		out.body.attr.attr.mode = mode;
		out.body.attr.attr.size = bufsize;
		out.body.attr.attr_valid = 0;
	})));
	EXPECT_CALL(*m_mock, process(
		ResultOf([=](auto in) {
			return (in.header.opcode == FUSE_GETATTR &&
				in.header.nodeid == ino);
		}, Eq(true)),
		_)
	).InSequence(seq)
	.WillRepeatedly(Invoke(ReturnErrno(EIO)));
	expect_open(ino, 0, 1);
	EXPECT_CALL(*m_mock, process(
		ResultOf([=](auto in) {
			return (in.header.opcode == FUSE_READ);
		}, Eq(true)),
		_)
	).Times(0);

	fd = open(FULLPATH, O_RDONLY);
	ASSERT_LE(0, fd) << strerror(errno);

	p = mmap(NULL, len, PROT_READ, MAP_SHARED, fd, 0);
	ASSERT_NE(MAP_FAILED, p) << strerror(errno);

	/* Accessing the mapped page should return SIGBUS.  */
	bzero(&sa, sizeof(sa));
	sa.sa_handler = SIG_DFL;
	sa.sa_sigaction = handle_sigbus;
	sa.sa_flags = SA_RESETHAND | SA_SIGINFO;
	ASSERT_EQ(0, sigaction(SIGBUS, &sa, NULL)) << strerror(errno);
	if (setjmp(ReadSigbus::s_jmpbuf) == 0) {
		atomic_signal_fence(std::memory_order::memory_order_seq_cst);
		volatile char x __unused = *(volatile char*)p;
		FAIL() << "shouldn't get here";
	}

	ASSERT_EQ(p, ReadSigbus::s_si_addr);
	ASSERT_EQ(0, munmap(p, len)) << strerror(errno);
	leak(fd);
}

/*
 * Just as when FOPEN_DIRECT_IO is used, reads with O_DIRECT should bypass
 * cache and to straight to the daemon
 */
TEST_F(Read, o_direct)
{
	const char FULLPATH[] = "mountpoint/some_file.txt";
	const char RELPATH[] = "some_file.txt";
	const char *CONTENTS = "abcdefgh";
	uint64_t ino = 42;
	int fd;
	ssize_t bufsize = strlen(CONTENTS);
	uint8_t buf[bufsize];

	expect_lookup(RELPATH, ino, bufsize);
	expect_open(ino, 0, 1);
	expect_read(ino, 0, bufsize, bufsize, CONTENTS);

	fd = open(FULLPATH, O_RDONLY);
	ASSERT_LE(0, fd) << strerror(errno);

	// Fill the cache
	ASSERT_EQ(bufsize, read(fd, buf, bufsize)) << strerror(errno);
	ASSERT_EQ(0, memcmp(buf, CONTENTS, bufsize));

	// Reads with o_direct should bypass the cache
	expect_read(ino, 0, bufsize, bufsize, CONTENTS);
	ASSERT_EQ(0, fcntl(fd, F_SETFL, O_DIRECT)) << strerror(errno);
	ASSERT_EQ(0, lseek(fd, 0, SEEK_SET)) << strerror(errno);
	ASSERT_EQ(bufsize, read(fd, buf, bufsize)) << strerror(errno);
	ASSERT_EQ(0, memcmp(buf, CONTENTS, bufsize));

	leak(fd);
}

TEST_F(Read, pread)
{
	const char FULLPATH[] = "mountpoint/some_file.txt";
	const char RELPATH[] = "some_file.txt";
	const char *CONTENTS = "abcdefgh";
	uint64_t ino = 42;
	int fd;
	/* 
	 * Set offset to a maxbcachebuf boundary so we'll be sure what offset
	 * to read from.  Without this, the read might start at a lower offset.
	 */
	uint64_t offset = m_maxbcachebuf;
	ssize_t bufsize = strlen(CONTENTS);
	uint8_t buf[bufsize];

	expect_lookup(RELPATH, ino, offset + bufsize);
	expect_open(ino, 0, 1);
	expect_read(ino, offset, bufsize, bufsize, CONTENTS);

	fd = open(FULLPATH, O_RDONLY);
	ASSERT_LE(0, fd) << strerror(errno);

	ASSERT_EQ(bufsize, pread(fd, buf, bufsize, offset)) << strerror(errno);
	ASSERT_EQ(0, memcmp(buf, CONTENTS, bufsize));
	leak(fd);
}

TEST_F(Read, read)
{
	const char FULLPATH[] = "mountpoint/some_file.txt";
	const char RELPATH[] = "some_file.txt";
	const char *CONTENTS = "abcdefgh";
	uint64_t ino = 42;
	int fd;
	ssize_t bufsize = strlen(CONTENTS);
	uint8_t buf[bufsize];

	expect_lookup(RELPATH, ino, bufsize);
	expect_open(ino, 0, 1);
	expect_read(ino, 0, bufsize, bufsize, CONTENTS);

	fd = open(FULLPATH, O_RDONLY);
	ASSERT_LE(0, fd) << strerror(errno);

	ASSERT_EQ(bufsize, read(fd, buf, bufsize)) << strerror(errno);
	ASSERT_EQ(0, memcmp(buf, CONTENTS, bufsize));

	leak(fd);
}

TEST_F(Read_7_8, read)
{
	const char FULLPATH[] = "mountpoint/some_file.txt";
	const char RELPATH[] = "some_file.txt";
	const char *CONTENTS = "abcdefgh";
	uint64_t ino = 42;
	int fd;
	ssize_t bufsize = strlen(CONTENTS);
	uint8_t buf[bufsize];

	expect_lookup(RELPATH, ino, bufsize);
	expect_open(ino, 0, 1);
	expect_read(ino, 0, bufsize, bufsize, CONTENTS);

	fd = open(FULLPATH, O_RDONLY);
	ASSERT_LE(0, fd) << strerror(errno);

	ASSERT_EQ(bufsize, read(fd, buf, bufsize)) << strerror(errno);
	ASSERT_EQ(0, memcmp(buf, CONTENTS, bufsize));

	leak(fd);
}

/* 
 * If cacheing is enabled, the kernel should try to read an entire cache block
 * at a time.
 */
TEST_F(Read, cache_block)
{
	const char FULLPATH[] = "mountpoint/some_file.txt";
	const char RELPATH[] = "some_file.txt";
	const char *CONTENTS0 = "abcdefghijklmnop";
	uint64_t ino = 42;
	int fd;
	ssize_t bufsize = 8;
	ssize_t filesize = m_maxbcachebuf * 2;
	char *contents;
	char buf[bufsize];
	const char *contents1 = CONTENTS0 + bufsize;

	contents = (char*)calloc(1, filesize);
	ASSERT_NE(nullptr, contents);
	memmove(contents, CONTENTS0, strlen(CONTENTS0));

	expect_lookup(RELPATH, ino, filesize);
	expect_open(ino, 0, 1);
	expect_read(ino, 0, m_maxbcachebuf, m_maxbcachebuf,
		contents);

	fd = open(FULLPATH, O_RDONLY);
	ASSERT_LE(0, fd) << strerror(errno);

	ASSERT_EQ(bufsize, read(fd, buf, bufsize)) << strerror(errno);
	ASSERT_EQ(0, memcmp(buf, CONTENTS0, bufsize));

	/* A subsequent read should be serviced by cache */
	ASSERT_EQ(bufsize, read(fd, buf, bufsize)) << strerror(errno);
	ASSERT_EQ(0, memcmp(buf, contents1, bufsize));
	leak(fd);
	free(contents);
}

/* Reading with sendfile should work (though it obviously won't be 0-copy) */
TEST_F(Read, sendfile)
{
	const char FULLPATH[] = "mountpoint/some_file.txt";
	const char RELPATH[] = "some_file.txt";
	const char *CONTENTS = "abcdefgh";
	uint64_t ino = 42;
	int fd;
	size_t bufsize = strlen(CONTENTS);
	uint8_t buf[bufsize];
	int sp[2];
	off_t sbytes;

	expect_lookup(RELPATH, ino, bufsize);
	expect_open(ino, 0, 1);
	EXPECT_CALL(*m_mock, process(
		ResultOf([=](auto in) {
			return (in.header.opcode == FUSE_READ &&
				in.header.nodeid == ino &&
				in.body.read.fh == Read::FH &&
				in.body.read.offset == 0 &&
				in.body.read.size == bufsize);
		}, Eq(true)),
		_)
	).WillOnce(Invoke(ReturnImmediate([=](auto in __unused, auto& out) {
		out.header.len = sizeof(struct fuse_out_header) + bufsize;
		memmove(out.body.bytes, CONTENTS, bufsize);
	})));

	ASSERT_EQ(0, socketpair(PF_LOCAL, SOCK_STREAM, 0, sp))
		<< strerror(errno);
	fd = open(FULLPATH, O_RDONLY);
	ASSERT_LE(0, fd) << strerror(errno);

	ASSERT_EQ(0, sendfile(fd, sp[1], 0, bufsize, NULL, &sbytes, 0))
		<< strerror(errno);
	ASSERT_EQ(static_cast<ssize_t>(bufsize), read(sp[0], buf, bufsize))
		<< strerror(errno);
	ASSERT_EQ(0, memcmp(buf, CONTENTS, bufsize));

	close(sp[1]);
	close(sp[0]);
	leak(fd);
}

/* sendfile should fail gracefully if fuse declines the read */
/* https://bugs.freebsd.org/bugzilla/show_bug.cgi?id=236466 */
TEST_F(Read, sendfile_eio)
{
	const char FULLPATH[] = "mountpoint/some_file.txt";
	const char RELPATH[] = "some_file.txt";
	const char *CONTENTS = "abcdefgh";
	uint64_t ino = 42;
	int fd;
	ssize_t bufsize = strlen(CONTENTS);
	int sp[2];
	off_t sbytes;

	expect_lookup(RELPATH, ino, bufsize);
	expect_open(ino, 0, 1);
	EXPECT_CALL(*m_mock, process(
		ResultOf([=](auto in) {
			return (in.header.opcode == FUSE_READ);
		}, Eq(true)),
		_)
	).WillOnce(Invoke(ReturnErrno(EIO)));

	ASSERT_EQ(0, socketpair(PF_LOCAL, SOCK_STREAM, 0, sp))
		<< strerror(errno);
	fd = open(FULLPATH, O_RDONLY);
	ASSERT_LE(0, fd) << strerror(errno);

	ASSERT_NE(0, sendfile(fd, sp[1], 0, bufsize, NULL, &sbytes, 0));

	close(sp[1]);
	close(sp[0]);
	leak(fd);
}

/*
 * Sequential reads should use readahead.  And if allowed, large reads should
 * be clustered.
 */
TEST_P(ReadAhead, readahead) {
	const char FULLPATH[] = "mountpoint/some_file.txt";
	const char RELPATH[] = "some_file.txt";
	uint64_t ino = 42;
	int fd, maxcontig, clustersize;
	ssize_t bufsize = 4 * m_maxbcachebuf;
	ssize_t filesize = bufsize;
	uint64_t len;
	char *rbuf, *contents;
	off_t offs;

	contents = (char*)malloc(filesize);
	ASSERT_NE(nullptr, contents);
	memset(contents, 'X', filesize);
	rbuf = (char*)calloc(1, bufsize);

	expect_lookup(RELPATH, ino, filesize);
	expect_open(ino, 0, 1);
	maxcontig = m_noclusterr ? m_maxbcachebuf :
		m_maxbcachebuf + m_maxreadahead;
	clustersize = MIN(maxcontig, m_maxphys);
	for (offs = 0; offs < bufsize; offs += clustersize) {
		len = std::min((size_t)clustersize, (size_t)(filesize - offs));
		expect_read(ino, offs, len, len, contents + offs);
	}

	fd = open(FULLPATH, O_RDONLY);
	ASSERT_LE(0, fd) << strerror(errno);

	/* Set the internal readahead counter to a "large" value */
	ASSERT_EQ(0, fcntl(fd, F_READAHEAD, 1'000'000'000)) << strerror(errno);

	ASSERT_EQ(bufsize, read(fd, rbuf, bufsize)) << strerror(errno);
	ASSERT_EQ(0, memcmp(rbuf, contents, bufsize));

	leak(fd);
	free(rbuf);
	free(contents);
}

INSTANTIATE_TEST_SUITE_P(RA, ReadAhead,
	Values(tuple<bool, int>(false, 0),
	       tuple<bool, int>(false, 1),
	       tuple<bool, int>(false, 2),
	       tuple<bool, int>(false, 3),
	       tuple<bool, int>(true, 0),
	       tuple<bool, int>(true, 1),
	       tuple<bool, int>(true, 2)));

/* With read-only mounts, fuse should never update atime during close */
TEST_F(RofsRead, atime_during_close)
{
	const char FULLPATH[] = "mountpoint/some_file.txt";
	const char RELPATH[] = "some_file.txt";
	const char *CONTENTS = "abcdefgh";
	uint64_t ino = 42;
	int fd;
	ssize_t bufsize = strlen(CONTENTS);
	uint8_t buf[bufsize];

	expect_lookup(RELPATH, ino, bufsize);
	expect_open(ino, 0, 1);
	expect_read(ino, 0, bufsize, bufsize, CONTENTS);
	EXPECT_CALL(*m_mock, process(
		ResultOf([&](auto in) {
			return (in.header.opcode == FUSE_SETATTR);
		}, Eq(true)),
		_)
	).Times(0);
	expect_flush(ino, 1, ReturnErrno(0));
	expect_release(ino, FuseTest::FH);

	fd = open(FULLPATH, O_RDONLY);
	ASSERT_LE(0, fd) << strerror(errno);

	/* Ensure atime will be different than during lookup */
	nap();

	ASSERT_EQ(bufsize, read(fd, buf, bufsize)) << strerror(errno);

	close(fd);
}

/* fuse_init_out.time_gran controls the granularity of timestamps */
TEST_P(TimeGran, atime_during_setattr)
{
	const char FULLPATH[] = "mountpoint/some_file.txt";
	const char RELPATH[] = "some_file.txt";
	const char *CONTENTS = "abcdefgh";
	ssize_t bufsize = strlen(CONTENTS);
	uint8_t buf[bufsize];
	uint64_t ino = 42;
	const mode_t newmode = 0755;
	int fd;

	expect_lookup(RELPATH, ino, bufsize);
	expect_open(ino, 0, 1);
	expect_read(ino, 0, bufsize, bufsize, CONTENTS);
	EXPECT_CALL(*m_mock, process(
		ResultOf([=](auto in) {
			uint32_t valid = FATTR_MODE | FATTR_ATIME;
			return (in.header.opcode == FUSE_SETATTR &&
				in.header.nodeid == ino &&
				in.body.setattr.valid == valid &&
				in.body.setattr.atimensec % m_time_gran == 0);
		}, Eq(true)),
		_)
	).WillOnce(Invoke(ReturnImmediate([=](auto in __unused, auto& out) {
		SET_OUT_HEADER_LEN(out, attr);
		out.body.attr.attr.ino = ino;
		out.body.attr.attr.mode = S_IFREG | newmode;
	})));

	fd = open(FULLPATH, O_RDWR);
	ASSERT_LE(0, fd) << strerror(errno);

	ASSERT_EQ(bufsize, read(fd, buf, bufsize)) << strerror(errno);
	ASSERT_EQ(0, fchmod(fd, newmode)) << strerror(errno);

	leak(fd);
}

INSTANTIATE_TEST_SUITE_P(TG, TimeGran, Range(0u, 10u));
