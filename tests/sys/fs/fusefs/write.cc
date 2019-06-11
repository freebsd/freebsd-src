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
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/sysctl.h>
#include <sys/time.h>
#include <sys/uio.h>

#include <aio.h>
#include <fcntl.h>
#include <signal.h>
#include <unistd.h>
}

#include "mockfs.hh"
#include "utils.hh"

using namespace testing;

class Write: public FuseTest {

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

	FuseTest::TearDown();
}

void expect_lookup(const char *relpath, uint64_t ino, uint64_t size)
{
	FuseTest::expect_lookup(relpath, ino, S_IFREG | 0644, size, 1);
}

void expect_release(uint64_t ino, ProcessMockerT r)
{
	EXPECT_CALL(*m_mock, process(
		ResultOf([=](auto in) {
			return (in.header.opcode == FUSE_RELEASE &&
				in.header.nodeid == ino);
		}, Eq(true)),
		_)
	).WillRepeatedly(Invoke(r));
}

void expect_write(uint64_t ino, uint64_t offset, uint64_t isize,
	uint64_t osize, const void *contents)
{
	FuseTest::expect_write(ino, offset, isize, osize, 0, 0, contents);
}

};

sig_atomic_t Write::s_sigxfsz = 0;

class Write_7_8: public FuseTest {

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

class AioWrite: public Write {
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

/* Tests for the write-through cache mode */
class WriteThrough: public Write {
public:
virtual void SetUp() {
	const char *cache_mode_node = "vfs.fusefs.data_cache_mode";
	int val = 0;
	size_t size = sizeof(val);

	FuseTest::SetUp();
	if (IsSkipped())
		return;

	ASSERT_EQ(0, sysctlbyname(cache_mode_node, &val, &size, NULL, 0))
		<< strerror(errno);
	if (val != 1)
		GTEST_SKIP() << "vfs.fusefs.data_cache_mode must be set to 1 "
			"(writethrough) for this test";
}

void expect_write(uint64_t ino, uint64_t offset, uint64_t isize,
	uint64_t osize, const void *contents)
{
	FuseTest::expect_write(ino, offset, isize, osize, 0, FUSE_WRITE_CACHE,
		contents);
}
};

/* Tests for the writeback cache mode */
class WriteBack: public Write {
public:
virtual void SetUp() {
	const char *node = "vfs.fusefs.data_cache_mode";
	int val = 0;
	size_t size = sizeof(val);

	FuseTest::SetUp();
	if (IsSkipped())
		return;

	ASSERT_EQ(0, sysctlbyname(node, &val, &size, NULL, 0))
		<< strerror(errno);
	if (val != 2)
		GTEST_SKIP() << "vfs.fusefs.data_cache_mode must be set to 2 "
			"(writeback) for this test";
}

void expect_write(uint64_t ino, uint64_t offset, uint64_t isize,
	uint64_t osize, const void *contents)
{
	FuseTest::expect_write(ino, offset, isize, osize, FUSE_WRITE_CACHE, 0,
		contents);
}
};

void sigxfsz_handler(int __unused sig) {
	Write::s_sigxfsz = 1;
}

/* AIO writes need to set the header's pid field correctly */
/* https://bugs.freebsd.org/bugzilla/show_bug.cgi?id=236379 */
TEST_F(AioWrite, DISABLED_aio_write)
{
	const char FULLPATH[] = "mountpoint/some_file.txt";
	const char RELPATH[] = "some_file.txt";
	const char *CONTENTS = "abcdefgh";
	uint64_t ino = 42;
	uint64_t offset = 4096;
	int fd;
	ssize_t bufsize = strlen(CONTENTS);
	struct aiocb iocb, *piocb;

	expect_lookup(RELPATH, ino, 0);
	expect_open(ino, 0, 1);
	expect_write(ino, offset, bufsize, bufsize, CONTENTS);

	fd = open(FULLPATH, O_WRONLY);
	EXPECT_LE(0, fd) << strerror(errno);

	iocb.aio_nbytes = bufsize;
	iocb.aio_fildes = fd;
	iocb.aio_buf = (void *)CONTENTS;
	iocb.aio_offset = offset;
	iocb.aio_sigevent.sigev_notify = SIGEV_NONE;
	ASSERT_EQ(0, aio_write(&iocb)) << strerror(errno);
	ASSERT_EQ(bufsize, aio_waitcomplete(&piocb, NULL)) << strerror(errno);
	/* Deliberately leak fd.  close(2) will be tested in release.cc */
}

/* 
 * When a file is opened with O_APPEND, we should forward that flag to
 * FUSE_OPEN (tested by Open.o_append) but still attempt to calculate the
 * offset internally.  That way we'll work both with filesystems that
 * understand O_APPEND (and ignore the offset) and filesystems that don't (and
 * simply use the offset).
 *
 * Note that verifying the O_APPEND flag in FUSE_OPEN is done in the
 * Open.o_append test.
 */
TEST_F(Write, append)
{
	const ssize_t BUFSIZE = 9;
	const char FULLPATH[] = "mountpoint/some_file.txt";
	const char RELPATH[] = "some_file.txt";
	const char CONTENTS[BUFSIZE] = "abcdefgh";
	uint64_t ino = 42;
	/* 
	 * Set offset to a maxbcachebuf boundary so we don't need to RMW when
	 * using writeback caching
	 */
	uint64_t initial_offset = m_maxbcachebuf;
	int fd;

	expect_lookup(RELPATH, ino, initial_offset);
	expect_open(ino, 0, 1);
	expect_write(ino, initial_offset, BUFSIZE, BUFSIZE, CONTENTS);

	/* Must open O_RDWR or fuse(4) implicitly sets direct_io */
	fd = open(FULLPATH, O_RDWR | O_APPEND);
	EXPECT_LE(0, fd) << strerror(errno);

	ASSERT_EQ(BUFSIZE, write(fd, CONTENTS, BUFSIZE)) << strerror(errno);
	/* Deliberately leak fd.  close(2) will be tested in release.cc */
}

/* If a file is cached, then appending to the end should not cause a read */
TEST_F(Write, append_to_cached)
{
	const ssize_t BUFSIZE = 9;
	const char FULLPATH[] = "mountpoint/some_file.txt";
	const char RELPATH[] = "some_file.txt";
	char *oldcontents, *oldbuf;
	const char CONTENTS[BUFSIZE] = "abcdefgh";
	uint64_t ino = 42;
	/* 
	 * Set offset in between maxbcachebuf boundary to test buffer handling
	 */
	uint64_t oldsize = m_maxbcachebuf / 2;
	int fd;

	oldcontents = (char*)calloc(1, oldsize);
	ASSERT_NE(NULL, oldcontents) << strerror(errno);
	oldbuf = (char*)malloc(oldsize);
	ASSERT_NE(NULL, oldbuf) << strerror(errno);

	expect_lookup(RELPATH, ino, oldsize);
	expect_open(ino, 0, 1);
	expect_read(ino, 0, oldsize, oldsize, oldcontents);
	expect_write(ino, oldsize, BUFSIZE, BUFSIZE, CONTENTS);

	/* Must open O_RDWR or fuse(4) implicitly sets direct_io */
	fd = open(FULLPATH, O_RDWR | O_APPEND);
	EXPECT_LE(0, fd) << strerror(errno);

	/* Read the old data into the cache */
	ASSERT_EQ((ssize_t)oldsize, read(fd, oldbuf, oldsize))
		<< strerror(errno);

	/* Write the new data.  There should be no more read operations */
	ASSERT_EQ(BUFSIZE, write(fd, CONTENTS, BUFSIZE)) << strerror(errno);
	/* Deliberately leak fd.  close(2) will be tested in release.cc */
}

TEST_F(Write, append_direct_io)
{
	const ssize_t BUFSIZE = 9;
	const char FULLPATH[] = "mountpoint/some_file.txt";
	const char RELPATH[] = "some_file.txt";
	const char CONTENTS[BUFSIZE] = "abcdefgh";
	uint64_t ino = 42;
	uint64_t initial_offset = 4096;
	int fd;

	expect_lookup(RELPATH, ino, initial_offset);
	expect_open(ino, FOPEN_DIRECT_IO, 1);
	expect_write(ino, initial_offset, BUFSIZE, BUFSIZE, CONTENTS);

	fd = open(FULLPATH, O_WRONLY | O_APPEND);
	EXPECT_LE(0, fd) << strerror(errno);

	ASSERT_EQ(BUFSIZE, write(fd, CONTENTS, BUFSIZE)) << strerror(errno);
	/* Deliberately leak fd.  close(2) will be tested in release.cc */
}

/* A direct write should evict any overlapping cached data */
TEST_F(Write, direct_io_evicts_cache)
{
	const char FULLPATH[] = "mountpoint/some_file.txt";
	const char RELPATH[] = "some_file.txt";
	const char CONTENTS0[] = "abcdefgh";
	const char CONTENTS1[] = "ijklmnop";
	uint64_t ino = 42;
	int fd;
	ssize_t bufsize = strlen(CONTENTS0) + 1;
	char readbuf[bufsize];

	expect_lookup(RELPATH, ino, bufsize);
	expect_open(ino, 0, 1);
	expect_read(ino, 0, bufsize, bufsize, CONTENTS0);
	expect_write(ino, 0, bufsize, bufsize, CONTENTS1);

	fd = open(FULLPATH, O_RDWR);
	EXPECT_LE(0, fd) << strerror(errno);

	// Prime cache
	ASSERT_EQ(bufsize, read(fd, readbuf, bufsize)) << strerror(errno);

	// Write directly, evicting cache
	ASSERT_EQ(0, fcntl(fd, F_SETFL, O_DIRECT)) << strerror(errno);
	ASSERT_EQ(0, lseek(fd, 0, SEEK_SET)) << strerror(errno);
	ASSERT_EQ(bufsize, write(fd, CONTENTS1, bufsize)) << strerror(errno);

	// Read again.  Cache should be bypassed
	expect_read(ino, 0, bufsize, bufsize, CONTENTS1);
	ASSERT_EQ(0, fcntl(fd, F_SETFL, 0)) << strerror(errno);
	ASSERT_EQ(0, lseek(fd, 0, SEEK_SET)) << strerror(errno);
	ASSERT_EQ(bufsize, read(fd, readbuf, bufsize)) << strerror(errno);
	ASSERT_STREQ(readbuf, CONTENTS1);

	/* Deliberately leak fd.  close(2) will be tested in release.cc */
}

/*
 * If the server doesn't return FOPEN_DIRECT_IO during FUSE_OPEN, then it's not
 * allowed to return a short write for that file handle.  However, if it does
 * then we should still do our darndest to handle it by resending the unwritten
 * portion.
 */
TEST_F(Write, indirect_io_short_write)
{
	const char FULLPATH[] = "mountpoint/some_file.txt";
	const char RELPATH[] = "some_file.txt";
	const char *CONTENTS = "abcdefghijklmnop";
	uint64_t ino = 42;
	int fd;
	ssize_t bufsize = strlen(CONTENTS);
	ssize_t bufsize0 = 11;
	ssize_t bufsize1 = strlen(CONTENTS) - bufsize0;
	const char *contents1 = CONTENTS + bufsize0;

	expect_lookup(RELPATH, ino, 0);
	expect_open(ino, 0, 1);
	expect_write(ino, 0, bufsize, bufsize0, CONTENTS);
	expect_write(ino, bufsize0, bufsize1, bufsize1, contents1);

	fd = open(FULLPATH, O_WRONLY);
	EXPECT_LE(0, fd) << strerror(errno);

	ASSERT_EQ(bufsize, write(fd, CONTENTS, bufsize)) << strerror(errno);
	/* Deliberately leak fd.  close(2) will be tested in release.cc */
}

/* 
 * When the direct_io option is used, filesystems are allowed to write less
 * data than requested.  We should return the short write to userland.
 */
TEST_F(Write, direct_io_short_write)
{
	const char FULLPATH[] = "mountpoint/some_file.txt";
	const char RELPATH[] = "some_file.txt";
	const char *CONTENTS = "abcdefghijklmnop";
	uint64_t ino = 42;
	int fd;
	ssize_t bufsize = strlen(CONTENTS);
	ssize_t halfbufsize = bufsize / 2;

	expect_lookup(RELPATH, ino, 0);
	expect_open(ino, FOPEN_DIRECT_IO, 1);
	expect_write(ino, 0, bufsize, halfbufsize, CONTENTS);

	fd = open(FULLPATH, O_WRONLY);
	EXPECT_LE(0, fd) << strerror(errno);

	ASSERT_EQ(halfbufsize, write(fd, CONTENTS, bufsize)) << strerror(errno);
	/* Deliberately leak fd.  close(2) will be tested in release.cc */
}

/*
 * An insidious edge case: the filesystem returns a short write, and the
 * difference between what we requested and what it actually wrote crosses an
 * iov element boundary
 */
TEST_F(Write, direct_io_short_write_iov)
{
	const char FULLPATH[] = "mountpoint/some_file.txt";
	const char RELPATH[] = "some_file.txt";
	const char *CONTENTS0 = "abcdefgh";
	const char *CONTENTS1 = "ijklmnop";
	const char *EXPECTED0 = "abcdefghijklmnop";
	uint64_t ino = 42;
	int fd;
	ssize_t size0 = strlen(CONTENTS0) - 1;
	ssize_t size1 = strlen(CONTENTS1) + 1;
	ssize_t totalsize = size0 + size1;
	struct iovec iov[2];

	expect_lookup(RELPATH, ino, 0);
	expect_open(ino, FOPEN_DIRECT_IO, 1);
	expect_write(ino, 0, totalsize, size0, EXPECTED0);

	fd = open(FULLPATH, O_WRONLY);
	EXPECT_LE(0, fd) << strerror(errno);

	iov[0].iov_base = (void*)CONTENTS0;
	iov[0].iov_len = strlen(CONTENTS0);
	iov[1].iov_base = (void*)CONTENTS1;
	iov[1].iov_len = strlen(CONTENTS1);
	ASSERT_EQ(size0, writev(fd, iov, 2)) << strerror(errno);
	/* Deliberately leak fd.  close(2) will be tested in release.cc */
}

/* fusefs should respect RLIMIT_FSIZE */
TEST_F(Write, rlimit_fsize)
{
	const char FULLPATH[] = "mountpoint/some_file.txt";
	const char RELPATH[] = "some_file.txt";
	const char *CONTENTS = "abcdefgh";
	struct rlimit rl;
	ssize_t bufsize = strlen(CONTENTS);
	off_t offset = 1'000'000'000;
	uint64_t ino = 42;
	int fd;

	expect_lookup(RELPATH, ino, 0);
	expect_open(ino, 0, 1);

	rl.rlim_cur = offset;
	rl.rlim_max = 10 * offset;
	ASSERT_EQ(0, setrlimit(RLIMIT_FSIZE, &rl)) << strerror(errno);
	ASSERT_NE(SIG_ERR, signal(SIGXFSZ, sigxfsz_handler)) << strerror(errno);

	fd = open(FULLPATH, O_WRONLY);

	EXPECT_LE(0, fd) << strerror(errno);

	ASSERT_EQ(-1, pwrite(fd, CONTENTS, bufsize, offset));
	EXPECT_EQ(EFBIG, errno);
	EXPECT_EQ(1, s_sigxfsz);
	/* Deliberately leak fd.  close(2) will be tested in release.cc */
}

/*
 * If the kernel cannot be sure which uid, gid, or pid was responsible for a
 * write, then it must set the FUSE_WRITE_CACHE bit
 */
/* https://bugs.freebsd.org/bugzilla/show_bug.cgi?id=236378 */
// TODO: check vfs.fusefs.mmap_enable
TEST_F(Write, mmap)
{
	const char FULLPATH[] = "mountpoint/some_file.txt";
	const char RELPATH[] = "some_file.txt";
	const char *CONTENTS = "abcdefgh";
	uint64_t ino = 42;
	int fd;
	ssize_t bufsize = strlen(CONTENTS);
	void *p;
	uint64_t offset = 10;
	size_t len;
	void *zeros, *expected;

	len = getpagesize();

	zeros = calloc(1, len);
	ASSERT_NE(NULL, zeros);
	expected = calloc(1, len);
	ASSERT_NE(NULL, expected);
	memmove((uint8_t*)expected + offset, CONTENTS, bufsize);

	expect_lookup(RELPATH, ino, len);
	expect_open(ino, 0, 1);
	expect_read(ino, 0, len, len, zeros);
	/* 
	 * Writes from the pager may or may not be associated with the correct
	 * pid, so they must set FUSE_WRITE_CACHE.
	 */
	FuseTest::expect_write(ino, 0, len, len, FUSE_WRITE_CACHE, 0, expected);
	expect_flush(ino, 1, ReturnErrno(0));
	expect_release(ino, ReturnErrno(0));

	fd = open(FULLPATH, O_RDWR);
	EXPECT_LE(0, fd) << strerror(errno);

	p = mmap(NULL, len, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	ASSERT_NE(MAP_FAILED, p) << strerror(errno);

	memmove((uint8_t*)p + offset, CONTENTS, bufsize);

	ASSERT_EQ(0, munmap(p, len)) << strerror(errno);
	close(fd);	// Write mmap'd data on close

	free(expected);
	free(zeros);
}

/* In WriteThrough mode, a write should evict overlapping cached data */
TEST_F(WriteThrough, evicts_read_cache)
{
	const char FULLPATH[] = "mountpoint/some_file.txt";
	const char RELPATH[] = "some_file.txt";
	ssize_t bufsize = 65536;
	/* End the write in the middle of a page */
	ssize_t wrsize = bufsize - 1000;
	char *contents0, *contents1, *readbuf, *expected;
	uint64_t ino = 42;
	int fd;

	contents0 = (char*)malloc(bufsize);
	memset(contents0, 'X', bufsize);
	contents0[bufsize - 1] = '\0';	// Null-terminate
	contents1 = (char*)malloc(wrsize);
	memset(contents1, 'Y', wrsize);
	readbuf = (char*)calloc(bufsize, 1);
	expected = (char*)malloc(bufsize);
	memset(expected, 'Y', wrsize);
	memset(expected + wrsize, 'X', bufsize - wrsize);
	expected[bufsize - 1] = '\0';	// Null-terminate

	expect_lookup(RELPATH, ino, bufsize);
	expect_open(ino, 0, 1);
	expect_read(ino, 0, bufsize, bufsize, contents0);
	expect_write(ino, 0, wrsize, wrsize, contents1);

	fd = open(FULLPATH, O_RDWR);
	EXPECT_LE(0, fd) << strerror(errno);

	// Prime cache
	ASSERT_EQ(bufsize, read(fd, readbuf, bufsize)) << strerror(errno);

	// Write directly, evicting cache
	ASSERT_EQ(0, lseek(fd, 0, SEEK_SET)) << strerror(errno);
	ASSERT_EQ(wrsize, write(fd, contents1, wrsize)) << strerror(errno);

	// Read again.  Cache should be bypassed
	expect_read(ino, 0, bufsize, bufsize, expected);
	ASSERT_EQ(0, lseek(fd, 0, SEEK_SET)) << strerror(errno);
	ASSERT_EQ(bufsize, read(fd, readbuf, bufsize)) << strerror(errno);
	ASSERT_STREQ(readbuf, expected);

	/* Deliberately leak fd.  close(2) will be tested in release.cc */
}

TEST_F(WriteThrough, pwrite)
{
	const char FULLPATH[] = "mountpoint/some_file.txt";
	const char RELPATH[] = "some_file.txt";
	const char *CONTENTS = "abcdefgh";
	uint64_t ino = 42;
	uint64_t offset = 4096;
	int fd;
	ssize_t bufsize = strlen(CONTENTS);

	expect_lookup(RELPATH, ino, 0);
	expect_open(ino, 0, 1);
	expect_write(ino, offset, bufsize, bufsize, CONTENTS);

	fd = open(FULLPATH, O_WRONLY);
	EXPECT_LE(0, fd) << strerror(errno);

	ASSERT_EQ(bufsize, pwrite(fd, CONTENTS, bufsize, offset))
		<< strerror(errno);
	/* Deliberately leak fd.  close(2) will be tested in release.cc */
}

TEST_F(Write, write)
{
	const char FULLPATH[] = "mountpoint/some_file.txt";
	const char RELPATH[] = "some_file.txt";
	const char *CONTENTS = "abcdefgh";
	uint64_t ino = 42;
	int fd;
	ssize_t bufsize = strlen(CONTENTS);

	expect_lookup(RELPATH, ino, 0);
	expect_open(ino, 0, 1);
	expect_write(ino, 0, bufsize, bufsize, CONTENTS);

	fd = open(FULLPATH, O_WRONLY);
	EXPECT_LE(0, fd) << strerror(errno);

	ASSERT_EQ(bufsize, write(fd, CONTENTS, bufsize)) << strerror(errno);
	/* Deliberately leak fd.  close(2) will be tested in release.cc */
}

/* fuse(4) should not issue writes of greater size than the daemon requests */
TEST_F(Write, write_large)
{
	const char FULLPATH[] = "mountpoint/some_file.txt";
	const char RELPATH[] = "some_file.txt";
	int *contents;
	uint64_t ino = 42;
	int fd;
	ssize_t halfbufsize, bufsize;

	halfbufsize = m_mock->m_max_write;
	bufsize = halfbufsize * 2;
	contents = (int*)malloc(bufsize);
	ASSERT_NE(NULL, contents);
	for (int i = 0; i < (int)bufsize / (int)sizeof(i); i++) {
		contents[i] = i;
	}

	expect_lookup(RELPATH, ino, 0);
	expect_open(ino, 0, 1);
	expect_write(ino, 0, halfbufsize, halfbufsize, contents);
	expect_write(ino, halfbufsize, halfbufsize, halfbufsize,
		&contents[halfbufsize / sizeof(int)]);

	fd = open(FULLPATH, O_WRONLY);
	EXPECT_LE(0, fd) << strerror(errno);

	ASSERT_EQ(bufsize, write(fd, contents, bufsize)) << strerror(errno);
	/* Deliberately leak fd.  close(2) will be tested in release.cc */

	free(contents);
}

TEST_F(Write, write_nothing)
{
	const char FULLPATH[] = "mountpoint/some_file.txt";
	const char RELPATH[] = "some_file.txt";
	const char *CONTENTS = "";
	uint64_t ino = 42;
	int fd;
	ssize_t bufsize = 0;

	expect_lookup(RELPATH, ino, 0);
	expect_open(ino, 0, 1);

	fd = open(FULLPATH, O_WRONLY);
	EXPECT_LE(0, fd) << strerror(errno);

	ASSERT_EQ(bufsize, write(fd, CONTENTS, bufsize)) << strerror(errno);
	/* Deliberately leak fd.  close(2) will be tested in release.cc */
}

TEST_F(Write_7_8, write)
{
	const char FULLPATH[] = "mountpoint/some_file.txt";
	const char RELPATH[] = "some_file.txt";
	const char *CONTENTS = "abcdefgh";
	uint64_t ino = 42;
	int fd;
	ssize_t bufsize = strlen(CONTENTS);

	expect_lookup(RELPATH, ino, 0);
	expect_open(ino, 0, 1);
	expect_write_7_8(ino, 0, bufsize, bufsize, CONTENTS);

	fd = open(FULLPATH, O_WRONLY);
	EXPECT_LE(0, fd) << strerror(errno);

	ASSERT_EQ(bufsize, write(fd, CONTENTS, bufsize)) << strerror(errno);
	/* Deliberately leak fd.  close(2) will be tested in release.cc */
}

/* In writeback mode, dirty data should be written on close */
TEST_F(WriteBack, close)
{
	const char FULLPATH[] = "mountpoint/some_file.txt";
	const char RELPATH[] = "some_file.txt";
	const char *CONTENTS = "abcdefgh";
	uint64_t ino = 42;
	int fd;
	ssize_t bufsize = strlen(CONTENTS);

	expect_lookup(RELPATH, ino, 0);
	expect_open(ino, 0, 1);
	expect_write(ino, 0, bufsize, bufsize, CONTENTS);
	EXPECT_CALL(*m_mock, process(
		ResultOf([=](auto in) {
			return (in.header.opcode == FUSE_SETATTR);
		}, Eq(true)),
		_)
	).WillRepeatedly(Invoke(ReturnImmediate([=](auto i __unused, auto& out) {
		SET_OUT_HEADER_LEN(out, attr);
		out.body.attr.attr.ino = ino;	// Must match nodeid
	})));
	expect_flush(ino, 1, ReturnErrno(0));
	expect_release(ino, ReturnErrno(0));

	fd = open(FULLPATH, O_RDWR);
	ASSERT_LE(0, fd) << strerror(errno);

	ASSERT_EQ(bufsize, write(fd, CONTENTS, bufsize)) << strerror(errno);
	close(fd);
}

/*
 * In writeback mode, writes to an O_WRONLY file could trigger reads from the
 * server.  The FUSE protocol explicitly allows that.
 */
TEST_F(WriteBack, rmw)
{
	const char FULLPATH[] = "mountpoint/some_file.txt";
	const char RELPATH[] = "some_file.txt";
	const char *CONTENTS = "abcdefgh";
	const char *INITIAL   = "XXXXXXXXXX";
	uint64_t ino = 42;
	uint64_t offset = 1;
	off_t fsize = 10;
	int fd;
	ssize_t bufsize = strlen(CONTENTS);

	FuseTest::expect_lookup(RELPATH, ino, S_IFREG | 0644, fsize, 1);
	expect_open(ino, 0, 1);
	expect_read(ino, 0, fsize, fsize, INITIAL, O_WRONLY);
	expect_write(ino, offset, bufsize, bufsize, CONTENTS);

	fd = open(FULLPATH, O_WRONLY);
	EXPECT_LE(0, fd) << strerror(errno);

	ASSERT_EQ(bufsize, pwrite(fd, CONTENTS, bufsize, offset))
		<< strerror(errno);
	/* Deliberately leak fd.  close(2) will be tested in release.cc */
}

/*
 * Without direct_io, writes should be committed to cache
 */
TEST_F(WriteBack, writeback)
{
	const char FULLPATH[] = "mountpoint/some_file.txt";
	const char RELPATH[] = "some_file.txt";
	const char *CONTENTS = "abcdefgh";
	uint64_t ino = 42;
	int fd;
	ssize_t bufsize = strlen(CONTENTS);
	char readbuf[bufsize];

	expect_lookup(RELPATH, ino, 0);
	expect_open(ino, 0, 1);
	expect_write(ino, 0, bufsize, bufsize, CONTENTS);

	fd = open(FULLPATH, O_RDWR);
	EXPECT_LE(0, fd) << strerror(errno);

	ASSERT_EQ(bufsize, write(fd, CONTENTS, bufsize)) << strerror(errno);
	/* 
	 * A subsequent read should be serviced by cache, without querying the
	 * filesystem daemon
	 */
	ASSERT_EQ(0, lseek(fd, 0, SEEK_SET)) << strerror(errno);
	ASSERT_EQ(bufsize, read(fd, readbuf, bufsize)) << strerror(errno);
	/* Deliberately leak fd.  close(2) will be tested in release.cc */
}

/*
 * With O_DIRECT, writes should be not committed to cache.  Admittedly this is
 * an odd test, because it would be unusual to use O_DIRECT for writes but not
 * reads.
 */
TEST_F(WriteBack, o_direct)
{
	const char FULLPATH[] = "mountpoint/some_file.txt";
	const char RELPATH[] = "some_file.txt";
	const char *CONTENTS = "abcdefgh";
	uint64_t ino = 42;
	int fd;
	ssize_t bufsize = strlen(CONTENTS);
	char readbuf[bufsize];

	expect_lookup(RELPATH, ino, 0);
	expect_open(ino, 0, 1);
	FuseTest::expect_write(ino, 0, bufsize, bufsize, 0, FUSE_WRITE_CACHE,
		CONTENTS);
	expect_read(ino, 0, bufsize, bufsize, CONTENTS);

	fd = open(FULLPATH, O_RDWR | O_DIRECT);
	EXPECT_LE(0, fd) << strerror(errno);

	ASSERT_EQ(bufsize, write(fd, CONTENTS, bufsize)) << strerror(errno);
	/* A subsequent read must query the daemon because cache is empty */
	ASSERT_EQ(0, lseek(fd, 0, SEEK_SET)) << strerror(errno);
	ASSERT_EQ(0, fcntl(fd, F_SETFL, 0)) << strerror(errno);
	ASSERT_EQ(bufsize, read(fd, readbuf, bufsize)) << strerror(errno);
	/* Deliberately leak fd.  close(2) will be tested in release.cc */
}

/*
 * Without direct_io, writes should be committed to cache
 */
/* 
 * Disabled because we don't yet implement write-through caching.  No bugzilla
 * entry, because that's a feature request, not a bug.
 */
TEST_F(WriteThrough, DISABLED_writethrough)
{
	const char FULLPATH[] = "mountpoint/some_file.txt";
	const char RELPATH[] = "some_file.txt";
	const char *CONTENTS = "abcdefgh";
	uint64_t ino = 42;
	int fd;
	ssize_t bufsize = strlen(CONTENTS);
	char readbuf[bufsize];

	expect_lookup(RELPATH, ino, 0);
	expect_open(ino, 0, 1);
	expect_write(ino, 0, bufsize, bufsize, CONTENTS);

	fd = open(FULLPATH, O_RDWR);
	EXPECT_LE(0, fd) << strerror(errno);

	ASSERT_EQ(bufsize, write(fd, CONTENTS, bufsize)) << strerror(errno);
	/*
	 * A subsequent read should be serviced by cache, without querying the
	 * filesystem daemon
	 */
	ASSERT_EQ(bufsize, read(fd, readbuf, bufsize)) << strerror(errno);
	/* Deliberately leak fd.  close(2) will be tested in release.cc */
}

/* With writethrough caching, writes update the cached file size */
TEST_F(WriteThrough, update_file_size)
{
	const char FULLPATH[] = "mountpoint/some_file.txt";
	const char RELPATH[] = "some_file.txt";
	const char *CONTENTS = "abcdefgh";
	struct stat sb;
	uint64_t ino = 42;
	int fd;
	ssize_t bufsize = strlen(CONTENTS);

	expect_lookup(RELPATH, ino, 0);
	expect_open(ino, 0, 1);
	expect_write(ino, 0, bufsize, bufsize, CONTENTS);

	fd = open(FULLPATH, O_RDWR);
	EXPECT_LE(0, fd) << strerror(errno);

	ASSERT_EQ(bufsize, write(fd, CONTENTS, bufsize)) << strerror(errno);
	/* Get cached attributes */
	ASSERT_EQ(0, fstat(fd, &sb)) << strerror(errno);
	ASSERT_EQ(bufsize, sb.st_size);
	/* Deliberately leak fd.  close(2) will be tested in release.cc */
}
