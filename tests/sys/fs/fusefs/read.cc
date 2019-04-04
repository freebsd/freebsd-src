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
#include <sys/types.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/sysctl.h>
#include <sys/uio.h>

#include <aio.h>
#include <fcntl.h>
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

class AioRead: public Read {
public:
virtual void SetUp() {
	const char *node = "vfs.aio.enable_unsafe";
	int val = 0;
	size_t size = sizeof(val);

	FuseTest::SetUp();

	ASSERT_EQ(0, sysctlbyname(node, &val, &size, NULL, 0))
		<< strerror(errno);
	if (!val)
		GTEST_SKIP() <<
			"vfs.aio.enable_unsafe must be set for this test";
}
};

class AsyncRead: public AioRead {
	virtual void SetUp() {
		m_init_flags = FUSE_ASYNC_READ;
		AioRead::SetUp();
	}
};

class ReadCacheable: public Read {
public:
virtual void SetUp() {
	const char *node = "vfs.fusefs.data_cache_mode";
	int val = 0;
	size_t size = sizeof(val);

	FuseTest::SetUp();

	ASSERT_EQ(0, sysctlbyname(node, &val, &size, NULL, 0))
		<< strerror(errno);
	if (val == 0)
		GTEST_SKIP() <<
			"fusefs data caching must be enabled for this test";
}
};

class ReadAhead: public ReadCacheable, public WithParamInterface<uint32_t> {
	virtual void SetUp() {
		m_maxreadahead = GetParam();
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
	char buf[bufsize];
	struct aiocb iocb, *piocb;

	expect_lookup(RELPATH, ino, bufsize);
	expect_open(ino, 0, 1);
	expect_getattr(ino, bufsize);
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
	/* Deliberately leak fd.  close(2) will be tested in release.cc */
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
	off_t off1 = 4096;
	struct aiocb iocb0, iocb1;

	expect_lookup(RELPATH, ino, bufsize);
	expect_open(ino, 0, 1);
	expect_getattr(ino, bufsize);
	EXPECT_CALL(*m_mock, process(
		ResultOf([=](auto in) {
			return (in->header.opcode == FUSE_READ &&
				in->header.nodeid == ino &&
				in->body.read.fh == FH &&
				in->body.read.offset == (uint64_t)off0 &&
				in->body.read.size == bufsize);
		}, Eq(true)),
		_)
	).WillOnce(Invoke([](auto in __unused, auto &out __unused) {
		/* Filesystem is slow to respond */
	}));
	EXPECT_CALL(*m_mock, process(
		ResultOf([=](auto in) {
			return (in->header.opcode == FUSE_READ &&
				in->header.nodeid == ino &&
				in->body.read.fh == FH &&
				in->body.read.offset == (uint64_t)off1 &&
				in->body.read.size == bufsize);
		}, Eq(true)),
		_)
	).Times(0);

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
	usleep(250'000);
	
	/* Deliberately leak iocbs */
	/* Deliberately leak fd.  close(2) will be tested in release.cc */
}

/* 
 * With the FUSE_ASYNC_READ mount option, fuse(4) may issue multiple
 * simultaneous read requests on the same file handle.
 */
/* 
 * Disabled because we don't yet implement FUSE_ASYNC_READ.  No bugzilla
 * entry, because that's a feature request, not a bug.
 */
TEST_F(AsyncRead, DISABLED_async_read)
{
	const char FULLPATH[] = "mountpoint/some_file.txt";
	const char RELPATH[] = "some_file.txt";
	uint64_t ino = 42;
	int fd;
	ssize_t bufsize = 50;
	char buf0[bufsize], buf1[bufsize];
	off_t off0 = 0;
	off_t off1 = 4096;
	struct aiocb iocb0, iocb1;

	expect_lookup(RELPATH, ino, bufsize);
	expect_open(ino, 0, 1);
	expect_getattr(ino, bufsize);
	EXPECT_CALL(*m_mock, process(
		ResultOf([=](auto in) {
			return (in->header.opcode == FUSE_READ &&
				in->header.nodeid == ino &&
				in->body.read.fh == FH &&
				in->body.read.offset == (uint64_t)off0 &&
				in->body.read.size == bufsize);
		}, Eq(true)),
		_)
	).WillOnce(Invoke([](auto in __unused, auto &out __unused) {
		/* Filesystem is slow to respond */
	}));
	EXPECT_CALL(*m_mock, process(
		ResultOf([=](auto in) {
			return (in->header.opcode == FUSE_READ &&
				in->header.nodeid == ino &&
				in->body.read.fh == FH &&
				in->body.read.offset == (uint64_t)off1 &&
				in->body.read.size == bufsize);
		}, Eq(true)),
		_)
	).WillOnce(Invoke([](auto in __unused, auto &out __unused) {
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

	/* 
	 * Sleep for awhile to make sure the kernel has had a chance to issue
	 * both reads.
	 */
	usleep(250'000);
	
	/* Deliberately leak iocbs */
	/* Deliberately leak fd.  close(2) will be tested in release.cc */
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
	expect_getattr(ino, offset + 1000);

	fd = open(FULLPATH, O_RDONLY);
	ASSERT_LE(0, fd) << strerror(errno);

	ASSERT_EQ(0, pread(fd, buf, 0, offset)) << strerror(errno);
	/* Deliberately leak fd.  close(2) will be tested in release.cc */
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
	char buf[bufsize];

	expect_lookup(RELPATH, ino, offset + bufsize);
	expect_open(ino, FOPEN_DIRECT_IO, 1);
	expect_getattr(ino, offset + bufsize);
	expect_read(ino, offset, bufsize, bufsize, CONTENTS);

	fd = open(FULLPATH, O_RDONLY);
	ASSERT_LE(0, fd) << strerror(errno);

	ASSERT_EQ(bufsize, pread(fd, buf, bufsize, offset)) << strerror(errno);
	ASSERT_EQ(0, memcmp(buf, CONTENTS, bufsize));
	/* Deliberately leak fd.  close(2) will be tested in release.cc */
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
	char buf[bufsize];

	expect_lookup(RELPATH, ino, offset + bufsize);
	expect_open(ino, FOPEN_DIRECT_IO, 1);
	expect_getattr(ino, offset + bufsize);
	expect_read(ino, offset, bufsize, halfbufsize, CONTENTS);

	fd = open(FULLPATH, O_RDONLY);
	ASSERT_LE(0, fd) << strerror(errno);

	ASSERT_EQ(halfbufsize, pread(fd, buf, bufsize, offset))
		<< strerror(errno);
	ASSERT_EQ(0, memcmp(buf, CONTENTS, halfbufsize));
	/* Deliberately leak fd.  close(2) will be tested in release.cc */
}

TEST_F(Read, eio)
{
	const char FULLPATH[] = "mountpoint/some_file.txt";
	const char RELPATH[] = "some_file.txt";
	const char *CONTENTS = "abcdefgh";
	uint64_t ino = 42;
	int fd;
	ssize_t bufsize = strlen(CONTENTS);
	char buf[bufsize];

	expect_lookup(RELPATH, ino, bufsize);
	expect_open(ino, 0, 1);
	expect_getattr(ino, bufsize);
	EXPECT_CALL(*m_mock, process(
		ResultOf([=](auto in) {
			return (in->header.opcode == FUSE_READ);
		}, Eq(true)),
		_)
	).WillOnce(Invoke(ReturnErrno(EIO)));

	fd = open(FULLPATH, O_RDONLY);
	ASSERT_LE(0, fd) << strerror(errno);

	ASSERT_EQ(-1, read(fd, buf, bufsize)) << strerror(errno);
	ASSERT_EQ(EIO, errno);
	/* Deliberately leak fd.  close(2) will be tested in release.cc */
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
	char buf[bufsize];

	FuseTest::expect_lookup(RELPATH, ino, S_IFREG | 0644, bufsize, 2);
	expect_open(ino, FOPEN_KEEP_CACHE, 2);
	expect_getattr(ino, bufsize);
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

	/* Deliberately leak fd0 and fd1. */
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
	char buf[bufsize];

	FuseTest::expect_lookup(RELPATH, ino, S_IFREG | 0644, bufsize, 2);
	expect_open(ino, 0, 2);
	expect_getattr(ino, bufsize);
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

	/* Deliberately leak fd0 and fd1. */
}

TEST_F(ReadCacheable, mmap)
{
	const char FULLPATH[] = "mountpoint/some_file.txt";
	const char RELPATH[] = "some_file.txt";
	const char *CONTENTS = "abcdefgh";
	uint64_t ino = 42;
	int fd;
	ssize_t len;
	ssize_t bufsize = strlen(CONTENTS);
	void *p;
	//char buf[bufsize];

	len = getpagesize();

	expect_lookup(RELPATH, ino, bufsize);
	expect_open(ino, 0, 1);
	expect_getattr(ino, bufsize);
	/* mmap may legitimately try to read more data than is available */
	EXPECT_CALL(*m_mock, process(
		ResultOf([=](auto in) {
			return (in->header.opcode == FUSE_READ &&
				in->header.nodeid == ino &&
				in->body.read.fh == Read::FH &&
				in->body.read.offset == 0 &&
				in->body.read.size >= bufsize);
		}, Eq(true)),
		_)
	).WillOnce(Invoke(ReturnImmediate([=](auto in __unused, auto out) {
		out->header.len = sizeof(struct fuse_out_header) + bufsize;
		memmove(out->body.bytes, CONTENTS, bufsize);
	})));

	fd = open(FULLPATH, O_RDONLY);
	ASSERT_LE(0, fd) << strerror(errno);

	p = mmap(NULL, len, PROT_READ, MAP_SHARED, fd, 0);
	ASSERT_NE(MAP_FAILED, p) << strerror(errno);

	ASSERT_EQ(0, memcmp(p, CONTENTS, bufsize));

	ASSERT_EQ(0, munmap(p, len)) << strerror(errno);
	/* Deliberately leak fd.  close(2) will be tested in release.cc */
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
	char buf[bufsize];

	expect_lookup(RELPATH, ino, bufsize);
	expect_open(ino, 0, 1);
	expect_getattr(ino, bufsize);
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
	
	/* Deliberately leak fd.  close(2) will be tested in release.cc */
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
	char buf[bufsize];

	expect_lookup(RELPATH, ino, offset + bufsize);
	expect_open(ino, 0, 1);
	expect_getattr(ino, offset + bufsize);
	expect_read(ino, offset, bufsize, bufsize, CONTENTS);

	fd = open(FULLPATH, O_RDONLY);
	ASSERT_LE(0, fd) << strerror(errno);

	ASSERT_EQ(bufsize, pread(fd, buf, bufsize, offset)) << strerror(errno);
	ASSERT_EQ(0, memcmp(buf, CONTENTS, bufsize));
	/* Deliberately leak fd.  close(2) will be tested in release.cc */
}

TEST_F(Read, read)
{
	const char FULLPATH[] = "mountpoint/some_file.txt";
	const char RELPATH[] = "some_file.txt";
	const char *CONTENTS = "abcdefgh";
	uint64_t ino = 42;
	int fd;
	ssize_t bufsize = strlen(CONTENTS);
	char buf[bufsize];

	expect_lookup(RELPATH, ino, bufsize);
	expect_open(ino, 0, 1);
	expect_getattr(ino, bufsize);
	expect_read(ino, 0, bufsize, bufsize, CONTENTS);

	fd = open(FULLPATH, O_RDONLY);
	ASSERT_LE(0, fd) << strerror(errno);

	ASSERT_EQ(bufsize, read(fd, buf, bufsize)) << strerror(errno);
	ASSERT_EQ(0, memcmp(buf, CONTENTS, bufsize));

	/* Deliberately leak fd.  close(2) will be tested in release.cc */
}

/* If the filesystem allows it, the kernel should try to readahead */
TEST_F(ReadCacheable, default_readahead)
{
	const char FULLPATH[] = "mountpoint/some_file.txt";
	const char RELPATH[] = "some_file.txt";
	const char *CONTENTS0 = "abcdefghijklmnop";
	uint64_t ino = 42;
	int fd;
	ssize_t bufsize = 8;
	/* hard-coded in fuse_internal.c */
	size_t default_maxreadahead = 65536;
	ssize_t filesize = default_maxreadahead * 2;
	char *contents;
	char buf[bufsize];
	const char *contents1 = CONTENTS0 + bufsize;

	contents = (char*)calloc(1, filesize);
	ASSERT_NE(NULL, contents);
	memmove(contents, CONTENTS0, strlen(CONTENTS0));

	expect_lookup(RELPATH, ino, filesize);
	expect_open(ino, 0, 1);
	expect_getattr(ino, filesize);
	expect_read(ino, 0, default_maxreadahead, default_maxreadahead,
		contents);

	fd = open(FULLPATH, O_RDONLY);
	ASSERT_LE(0, fd) << strerror(errno);

	ASSERT_EQ(bufsize, read(fd, buf, bufsize)) << strerror(errno);
	ASSERT_EQ(0, memcmp(buf, CONTENTS0, bufsize));

	/* A subsequent read should be serviced by cache */
	ASSERT_EQ(bufsize, read(fd, buf, bufsize)) << strerror(errno);
	ASSERT_EQ(0, memcmp(buf, contents1, bufsize));
	/* Deliberately leak fd.  close(2) will be tested in release.cc */
}

/* Reading with sendfile should work (though it obviously won't be 0-copy) */
TEST_F(ReadCacheable, sendfile)
{
	const char FULLPATH[] = "mountpoint/some_file.txt";
	const char RELPATH[] = "some_file.txt";
	const char *CONTENTS = "abcdefgh";
	uint64_t ino = 42;
	int fd;
	ssize_t bufsize = strlen(CONTENTS);
	char buf[bufsize];
	int sp[2];
	off_t sbytes;

	expect_lookup(RELPATH, ino, bufsize);
	expect_open(ino, 0, 1);
	expect_getattr(ino, bufsize);
	/* Like mmap, sendfile may request more data than is available */
	EXPECT_CALL(*m_mock, process(
		ResultOf([=](auto in) {
			return (in->header.opcode == FUSE_READ &&
				in->header.nodeid == ino &&
				in->body.read.fh == Read::FH &&
				in->body.read.offset == 0 &&
				in->body.read.size >= bufsize);
		}, Eq(true)),
		_)
	).WillOnce(Invoke(ReturnImmediate([=](auto in __unused, auto out) {
		out->header.len = sizeof(struct fuse_out_header) + bufsize;
		memmove(out->body.bytes, CONTENTS, bufsize);
	})));

	ASSERT_EQ(0, socketpair(PF_LOCAL, SOCK_STREAM, 0, sp))
		<< strerror(errno);
	fd = open(FULLPATH, O_RDONLY);
	ASSERT_LE(0, fd) << strerror(errno);

	ASSERT_EQ(0, sendfile(fd, sp[1], 0, bufsize, NULL, &sbytes, 0))
		<< strerror(errno);
	ASSERT_EQ(bufsize, read(sp[0], buf, bufsize)) << strerror(errno);
	ASSERT_EQ(0, memcmp(buf, CONTENTS, bufsize));

	close(sp[1]);
	close(sp[0]);
	/* Deliberately leak fd.  close(2) will be tested in release.cc */
}

/* sendfile should fail gracefully if fuse declines the read */
/* https://bugs.freebsd.org/bugzilla/show_bug.cgi?id=236466 */
TEST_F(ReadCacheable, DISABLED_sendfile_eio)
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
	expect_getattr(ino, bufsize);
	EXPECT_CALL(*m_mock, process(
		ResultOf([=](auto in) {
			return (in->header.opcode == FUSE_READ);
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
	/* Deliberately leak fd.  close(2) will be tested in release.cc */
}

/* fuse(4) should honor the filesystem's requested m_readahead parameter */
/* https://bugs.freebsd.org/bugzilla/show_bug.cgi?id=236472 */
TEST_P(ReadAhead, DISABLED_readahead) {
	const char FULLPATH[] = "mountpoint/some_file.txt";
	const char RELPATH[] = "some_file.txt";
	const char *CONTENTS0 = "abcdefghijklmnop";
	uint64_t ino = 42;
	int fd;
	ssize_t bufsize = 8;
	ssize_t filesize = m_maxbcachebuf * 2;
	char *contents;
	char buf[bufsize];

	ASSERT_TRUE(GetParam() < (uint32_t)m_maxbcachebuf)
		<< "Test assumes that max_readahead < maxbcachebuf";

	contents = (char*)calloc(1, filesize);
	ASSERT_NE(NULL, contents);
	memmove(contents, CONTENTS0, strlen(CONTENTS0));

	expect_lookup(RELPATH, ino, filesize);
	expect_open(ino, 0, 1);
	expect_getattr(ino, filesize);
	/* fuse(4) should only read ahead the allowed amount */
	expect_read(ino, 0, GetParam(), GetParam(), contents);

	fd = open(FULLPATH, O_RDONLY);
	ASSERT_LE(0, fd) << strerror(errno);

	ASSERT_EQ(bufsize, read(fd, buf, bufsize)) << strerror(errno);
	ASSERT_EQ(0, memcmp(buf, CONTENTS0, bufsize));

	/* Deliberately leak fd.  close(2) will be tested in release.cc */
}

INSTANTIATE_TEST_CASE_P(RA, ReadAhead, ::testing::Values(0u, 2048u));
