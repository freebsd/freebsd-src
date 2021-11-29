/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
 * Copyright (c) 2020 Alan Somers
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
#include <sys/time.h>
#include <sys/resource.h>

#include <fcntl.h>
#include <signal.h>
#include <unistd.h>
}

#include "mockfs.hh"
#include "utils.hh"

using namespace testing;

class CopyFileRange: public FuseTest {
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

void expect_maybe_lseek(uint64_t ino)
{
	EXPECT_CALL(*m_mock, process(
		ResultOf([=](auto in) {
			return (in.header.opcode == FUSE_LSEEK &&
				in.header.nodeid == ino);
		}, Eq(true)),
		_)
	).Times(AtMost(1))
	.WillRepeatedly(Invoke(ReturnErrno(ENOSYS)));
}

void expect_open(uint64_t ino, uint32_t flags, int times, uint64_t fh)
{
	EXPECT_CALL(*m_mock, process(
		ResultOf([=](auto in) {
			return (in.header.opcode == FUSE_OPEN &&
				in.header.nodeid == ino);
		}, Eq(true)),
		_)
	).Times(times)
	.WillRepeatedly(Invoke(
		ReturnImmediate([=](auto in __unused, auto& out) {
		out.header.len = sizeof(out.header);
		SET_OUT_HEADER_LEN(out, open);
		out.body.open.fh = fh;
		out.body.open.open_flags = flags;
	})));
}

void expect_write(uint64_t ino, uint64_t offset, uint64_t isize,
	uint64_t osize, const void *contents)
{
	EXPECT_CALL(*m_mock, process(
		ResultOf([=](auto in) {
			const char *buf = (const char*)in.body.bytes +
				sizeof(struct fuse_write_in);

			return (in.header.opcode == FUSE_WRITE &&
				in.header.nodeid == ino &&
				in.body.write.offset == offset  &&
				in.body.write.size == isize &&
				0 == bcmp(buf, contents, isize));
		}, Eq(true)),
		_)
	).WillOnce(Invoke(ReturnImmediate([=](auto in __unused, auto& out) {
		SET_OUT_HEADER_LEN(out, write);
		out.body.write.size = osize;
	})));
}

};

sig_atomic_t CopyFileRange::s_sigxfsz = 0;

void sigxfsz_handler(int __unused sig) {
	CopyFileRange::s_sigxfsz = 1;
}


class CopyFileRange_7_27: public CopyFileRange {
public:
virtual void SetUp() {
	m_kernel_minor_version = 27;
	CopyFileRange::SetUp();
}
};

class CopyFileRangeNoAtime: public CopyFileRange {
public:
virtual void SetUp() {
	m_noatime = true;
	CopyFileRange::SetUp();
}
};

TEST_F(CopyFileRange, eio)
{
	const char FULLPATH1[] = "mountpoint/src.txt";
	const char RELPATH1[] = "src.txt";
	const char FULLPATH2[] = "mountpoint/dst.txt";
	const char RELPATH2[] = "dst.txt";
	const uint64_t ino1 = 42;
	const uint64_t ino2 = 43;
	const uint64_t fh1 = 0xdeadbeef1a7ebabe;
	const uint64_t fh2 = 0xdeadc0de88c0ffee;
	off_t fsize1 = 1 << 20;		/* 1 MiB */
	off_t fsize2 = 1 << 19;		/* 512 KiB */
	off_t start1 = 1 << 18;
	off_t start2 = 3 << 17;
	ssize_t len = 65536;
	int fd1, fd2;

	expect_lookup(RELPATH1, ino1, S_IFREG | 0644, fsize1, 1);
	expect_lookup(RELPATH2, ino2, S_IFREG | 0644, fsize2, 1);
	expect_open(ino1, 0, 1, fh1);
	expect_open(ino2, 0, 1, fh2);
	EXPECT_CALL(*m_mock, process(
		ResultOf([=](auto in) {
			return (in.header.opcode == FUSE_COPY_FILE_RANGE &&
				in.header.nodeid == ino1 &&
				in.body.copy_file_range.fh_in == fh1 &&
				(off_t)in.body.copy_file_range.off_in == start1 &&
				in.body.copy_file_range.nodeid_out == ino2 &&
				in.body.copy_file_range.fh_out == fh2 &&
				(off_t)in.body.copy_file_range.off_out == start2 &&
				in.body.copy_file_range.len == (size_t)len &&
				in.body.copy_file_range.flags == 0);
		}, Eq(true)),
		_)
	).WillOnce(Invoke(ReturnErrno(EIO)));

	fd1 = open(FULLPATH1, O_RDONLY);
	fd2 = open(FULLPATH2, O_WRONLY);
	ASSERT_EQ(-1, copy_file_range(fd1, &start1, fd2, &start2, len, 0));
	EXPECT_EQ(EIO, errno);
}

/*
 * copy_file_range should evict cached data for the modified region of the
 * destination file.
 */
TEST_F(CopyFileRange, evicts_cache)
{
	const char FULLPATH1[] = "mountpoint/src.txt";
	const char RELPATH1[] = "src.txt";
	const char FULLPATH2[] = "mountpoint/dst.txt";
	const char RELPATH2[] = "dst.txt";
	void *buf0, *buf1, *buf;
	const uint64_t ino1 = 42;
	const uint64_t ino2 = 43;
	const uint64_t fh1 = 0xdeadbeef1a7ebabe;
	const uint64_t fh2 = 0xdeadc0de88c0ffee;
	off_t fsize1 = 1 << 20;		/* 1 MiB */
	off_t fsize2 = 1 << 19;		/* 512 KiB */
	off_t start1 = 1 << 18;
	off_t start2 = 3 << 17;
	ssize_t len = m_maxbcachebuf;
	int fd1, fd2;

	buf0 = malloc(m_maxbcachebuf);
	memset(buf0, 42, m_maxbcachebuf);

	expect_lookup(RELPATH1, ino1, S_IFREG | 0644, fsize1, 1);
	expect_lookup(RELPATH2, ino2, S_IFREG | 0644, fsize2, 1);
	expect_open(ino1, 0, 1, fh1);
	expect_open(ino2, 0, 1, fh2);
	expect_read(ino2, start2, m_maxbcachebuf, m_maxbcachebuf, buf0, -1,
		fh2);
	EXPECT_CALL(*m_mock, process(
		ResultOf([=](auto in) {
			return (in.header.opcode == FUSE_COPY_FILE_RANGE &&
				in.header.nodeid == ino1 &&
				in.body.copy_file_range.fh_in == fh1 &&
				(off_t)in.body.copy_file_range.off_in == start1 &&
				in.body.copy_file_range.nodeid_out == ino2 &&
				in.body.copy_file_range.fh_out == fh2 &&
				(off_t)in.body.copy_file_range.off_out == start2 &&
				in.body.copy_file_range.len == (size_t)len &&
				in.body.copy_file_range.flags == 0);
		}, Eq(true)),
		_)
	).WillOnce(Invoke(ReturnImmediate([=](auto in __unused, auto& out) {
		SET_OUT_HEADER_LEN(out, write);
		out.body.write.size = len;
	})));

	fd1 = open(FULLPATH1, O_RDONLY);
	fd2 = open(FULLPATH2, O_RDWR);

	// Prime cache
	buf = malloc(m_maxbcachebuf);
	ASSERT_EQ(m_maxbcachebuf, pread(fd2, buf, m_maxbcachebuf, start2))
		<< strerror(errno);
	EXPECT_EQ(0, memcmp(buf0, buf, m_maxbcachebuf));

	// Tell the FUSE server overwrite the region we just read
	ASSERT_EQ(len, copy_file_range(fd1, &start1, fd2, &start2, len, 0));

	// Read again.  This should bypass the cache and read direct from server
	buf1 = malloc(m_maxbcachebuf);
	memset(buf1, 69, m_maxbcachebuf);
	start2 -= len;
	expect_read(ino2, start2, m_maxbcachebuf, m_maxbcachebuf, buf1, -1,
		fh2);
	ASSERT_EQ(m_maxbcachebuf, pread(fd2, buf, m_maxbcachebuf, start2))
		<< strerror(errno);
	EXPECT_EQ(0, memcmp(buf1, buf, m_maxbcachebuf));

	free(buf1);
	free(buf0);
	free(buf);
	leak(fd1);
	leak(fd2);
}

/*
 * If the server doesn't support FUSE_COPY_FILE_RANGE, the kernel should
 * fallback to a read/write based implementation.
 */
TEST_F(CopyFileRange, fallback)
{
	const char FULLPATH1[] = "mountpoint/src.txt";
	const char RELPATH1[] = "src.txt";
	const char FULLPATH2[] = "mountpoint/dst.txt";
	const char RELPATH2[] = "dst.txt";
	const uint64_t ino1 = 42;
	const uint64_t ino2 = 43;
	const uint64_t fh1 = 0xdeadbeef1a7ebabe;
	const uint64_t fh2 = 0xdeadc0de88c0ffee;
	off_t fsize2 = 0;
	off_t start1 = 0;
	off_t start2 = 0;
	const char *contents = "Hello, world!";
	ssize_t len;
	int fd1, fd2;

	len = strlen(contents);

	/* 
	 * Ensure that we read to EOF, just so the buffer cache's read size is
	 * predictable.
	 */
	expect_lookup(RELPATH1, ino1, S_IFREG | 0644, start1 + len, 1);
	expect_lookup(RELPATH2, ino2, S_IFREG | 0644, fsize2, 1);
	expect_open(ino1, 0, 1, fh1);
	expect_open(ino2, 0, 1, fh2);
	EXPECT_CALL(*m_mock, process(
		ResultOf([=](auto in) {
			return (in.header.opcode == FUSE_COPY_FILE_RANGE &&
				in.header.nodeid == ino1 &&
				in.body.copy_file_range.fh_in == fh1 &&
				(off_t)in.body.copy_file_range.off_in == start1 &&
				in.body.copy_file_range.nodeid_out == ino2 &&
				in.body.copy_file_range.fh_out == fh2 &&
				(off_t)in.body.copy_file_range.off_out == start2 &&
				in.body.copy_file_range.len == (size_t)len &&
				in.body.copy_file_range.flags == 0);
		}, Eq(true)),
		_)
	).WillOnce(Invoke(ReturnErrno(ENOSYS)));
	expect_maybe_lseek(ino1);
	expect_read(ino1, start1, len, len, contents, 0);
	expect_write(ino2, start2, len, len, contents);

	fd1 = open(FULLPATH1, O_RDONLY);
	ASSERT_GE(fd1, 0);
	fd2 = open(FULLPATH2, O_WRONLY);
	ASSERT_GE(fd2, 0);
	ASSERT_EQ(len, copy_file_range(fd1, &start1, fd2, &start2, len, 0));
}

/* fusefs should respect RLIMIT_FSIZE */
TEST_F(CopyFileRange, rlimit_fsize)
{
	const char FULLPATH1[] = "mountpoint/src.txt";
	const char RELPATH1[] = "src.txt";
	const char FULLPATH2[] = "mountpoint/dst.txt";
	const char RELPATH2[] = "dst.txt";
	struct rlimit rl;
	const uint64_t ino1 = 42;
	const uint64_t ino2 = 43;
	const uint64_t fh1 = 0xdeadbeef1a7ebabe;
	const uint64_t fh2 = 0xdeadc0de88c0ffee;
	off_t fsize1 = 1 << 20;		/* 1 MiB */
	off_t fsize2 = 1 << 19;		/* 512 KiB */
	off_t start1 = 1 << 18;
	off_t start2 = fsize2;
	ssize_t len = 65536;
	int fd1, fd2;

	expect_lookup(RELPATH1, ino1, S_IFREG | 0644, fsize1, 1);
	expect_lookup(RELPATH2, ino2, S_IFREG | 0644, fsize2, 1);
	expect_open(ino1, 0, 1, fh1);
	expect_open(ino2, 0, 1, fh2);
	EXPECT_CALL(*m_mock, process(
		ResultOf([=](auto in) {
			return (in.header.opcode == FUSE_COPY_FILE_RANGE);
		}, Eq(true)),
		_)
	).Times(0);

	rl.rlim_cur = fsize2;
	rl.rlim_max = 10 * fsize2;
	ASSERT_EQ(0, setrlimit(RLIMIT_FSIZE, &rl)) << strerror(errno);
	ASSERT_NE(SIG_ERR, signal(SIGXFSZ, sigxfsz_handler)) << strerror(errno);

	fd1 = open(FULLPATH1, O_RDONLY);
	fd2 = open(FULLPATH2, O_WRONLY);
	ASSERT_EQ(-1, copy_file_range(fd1, &start1, fd2, &start2, len, 0));
	EXPECT_EQ(EFBIG, errno);
	EXPECT_EQ(1, s_sigxfsz);
}

TEST_F(CopyFileRange, ok)
{
	const char FULLPATH1[] = "mountpoint/src.txt";
	const char RELPATH1[] = "src.txt";
	const char FULLPATH2[] = "mountpoint/dst.txt";
	const char RELPATH2[] = "dst.txt";
	const uint64_t ino1 = 42;
	const uint64_t ino2 = 43;
	const uint64_t fh1 = 0xdeadbeef1a7ebabe;
	const uint64_t fh2 = 0xdeadc0de88c0ffee;
	off_t fsize1 = 1 << 20;		/* 1 MiB */
	off_t fsize2 = 1 << 19;		/* 512 KiB */
	off_t start1 = 1 << 18;
	off_t start2 = 3 << 17;
	ssize_t len = 65536;
	int fd1, fd2;

	expect_lookup(RELPATH1, ino1, S_IFREG | 0644, fsize1, 1);
	expect_lookup(RELPATH2, ino2, S_IFREG | 0644, fsize2, 1);
	expect_open(ino1, 0, 1, fh1);
	expect_open(ino2, 0, 1, fh2);
	EXPECT_CALL(*m_mock, process(
		ResultOf([=](auto in) {
			return (in.header.opcode == FUSE_COPY_FILE_RANGE &&
				in.header.nodeid == ino1 &&
				in.body.copy_file_range.fh_in == fh1 &&
				(off_t)in.body.copy_file_range.off_in == start1 &&
				in.body.copy_file_range.nodeid_out == ino2 &&
				in.body.copy_file_range.fh_out == fh2 &&
				(off_t)in.body.copy_file_range.off_out == start2 &&
				in.body.copy_file_range.len == (size_t)len &&
				in.body.copy_file_range.flags == 0);
		}, Eq(true)),
		_)
	).WillOnce(Invoke(ReturnImmediate([=](auto in __unused, auto& out) {
		SET_OUT_HEADER_LEN(out, write);
		out.body.write.size = len;
	})));

	fd1 = open(FULLPATH1, O_RDONLY);
	fd2 = open(FULLPATH2, O_WRONLY);
	ASSERT_EQ(len, copy_file_range(fd1, &start1, fd2, &start2, len, 0));
}

/* 
 * copy_file_range can make copies within a single file, as long as the ranges
 * don't overlap.
 * */
TEST_F(CopyFileRange, same_file)
{
	const char FULLPATH[] = "mountpoint/src.txt";
	const char RELPATH[] = "src.txt";
	const uint64_t ino = 4;
	const uint64_t fh = 0xdeadbeefa7ebabe;
	off_t fsize = 1 << 20;		/* 1 MiB */
	off_t off_in = 1 << 18;
	off_t off_out = 3 << 17;
	ssize_t len = 65536;
	int fd;

	expect_lookup(RELPATH, ino, S_IFREG | 0644, fsize, 1);
	expect_open(ino, 0, 1, fh);
	EXPECT_CALL(*m_mock, process(
		ResultOf([=](auto in) {
			return (in.header.opcode == FUSE_COPY_FILE_RANGE &&
				in.header.nodeid == ino &&
				in.body.copy_file_range.fh_in == fh &&
				(off_t)in.body.copy_file_range.off_in == off_in &&
				in.body.copy_file_range.nodeid_out == ino &&
				in.body.copy_file_range.fh_out == fh &&
				(off_t)in.body.copy_file_range.off_out == off_out &&
				in.body.copy_file_range.len == (size_t)len &&
				in.body.copy_file_range.flags == 0);
		}, Eq(true)),
		_)
	).WillOnce(Invoke(ReturnImmediate([=](auto in __unused, auto& out) {
		SET_OUT_HEADER_LEN(out, write);
		out.body.write.size = len;
	})));

	fd = open(FULLPATH, O_RDWR);
	ASSERT_EQ(len, copy_file_range(fd, &off_in, fd, &off_out, len, 0));
}

/*
 * copy_file_range should update the destination's mtime and ctime, and
 * the source's atime.
 */
TEST_F(CopyFileRange, timestamps)
{
	const char FULLPATH1[] = "mountpoint/src.txt";
	const char RELPATH1[] = "src.txt";
	const char FULLPATH2[] = "mountpoint/dst.txt";
	const char RELPATH2[] = "dst.txt";
	struct stat sb1a, sb1b, sb2a, sb2b;
	const uint64_t ino1 = 42;
	const uint64_t ino2 = 43;
	const uint64_t fh1 = 0xdeadbeef1a7ebabe;
	const uint64_t fh2 = 0xdeadc0de88c0ffee;
	off_t fsize1 = 1 << 20;		/* 1 MiB */
	off_t fsize2 = 1 << 19;		/* 512 KiB */
	off_t start1 = 1 << 18;
	off_t start2 = 3 << 17;
	ssize_t len = 65536;
	int fd1, fd2;

	expect_lookup(RELPATH1, ino1, S_IFREG | 0644, fsize1, 1);
	expect_lookup(RELPATH2, ino2, S_IFREG | 0644, fsize2, 1);
	expect_open(ino1, 0, 1, fh1);
	expect_open(ino2, 0, 1, fh2);
	EXPECT_CALL(*m_mock, process(
		ResultOf([=](auto in) {
			return (in.header.opcode == FUSE_COPY_FILE_RANGE &&
				in.header.nodeid == ino1 &&
				in.body.copy_file_range.fh_in == fh1 &&
				(off_t)in.body.copy_file_range.off_in == start1 &&
				in.body.copy_file_range.nodeid_out == ino2 &&
				in.body.copy_file_range.fh_out == fh2 &&
				(off_t)in.body.copy_file_range.off_out == start2 &&
				in.body.copy_file_range.len == (size_t)len &&
				in.body.copy_file_range.flags == 0);
		}, Eq(true)),
		_)
	).WillOnce(Invoke(ReturnImmediate([=](auto in __unused, auto& out) {
		SET_OUT_HEADER_LEN(out, write);
		out.body.write.size = len;
	})));

	fd1 = open(FULLPATH1, O_RDONLY);
	ASSERT_GE(fd1, 0);
	fd2 = open(FULLPATH2, O_WRONLY);
	ASSERT_GE(fd2, 0);
	ASSERT_EQ(0, fstat(fd1, &sb1a)) << strerror(errno);
	ASSERT_EQ(0, fstat(fd2, &sb2a)) << strerror(errno);

	nap();

	ASSERT_EQ(len, copy_file_range(fd1, &start1, fd2, &start2, len, 0));
	ASSERT_EQ(0, fstat(fd1, &sb1b)) << strerror(errno);
	ASSERT_EQ(0, fstat(fd2, &sb2b)) << strerror(errno);

	EXPECT_NE(sb1a.st_atime, sb1b.st_atime);
	EXPECT_EQ(sb1a.st_mtime, sb1b.st_mtime);
	EXPECT_EQ(sb1a.st_ctime, sb1b.st_ctime);
	EXPECT_EQ(sb2a.st_atime, sb2b.st_atime);
	EXPECT_NE(sb2a.st_mtime, sb2b.st_mtime);
	EXPECT_NE(sb2a.st_ctime, sb2b.st_ctime);

	leak(fd1);
	leak(fd2);
}

/*
 * copy_file_range can extend the size of a file
 * */
TEST_F(CopyFileRange, extend)
{
	const char FULLPATH[] = "mountpoint/src.txt";
	const char RELPATH[] = "src.txt";
	struct stat sb;
	const uint64_t ino = 4;
	const uint64_t fh = 0xdeadbeefa7ebabe;
	off_t fsize = 65536;
	off_t off_in = 0;
	off_t off_out = 65536;
	ssize_t len = 65536;
	int fd;

	expect_lookup(RELPATH, ino, S_IFREG | 0644, fsize, 1);
	expect_open(ino, 0, 1, fh);
	EXPECT_CALL(*m_mock, process(
		ResultOf([=](auto in) {
			return (in.header.opcode == FUSE_COPY_FILE_RANGE &&
				in.header.nodeid == ino &&
				in.body.copy_file_range.fh_in == fh &&
				(off_t)in.body.copy_file_range.off_in == off_in &&
				in.body.copy_file_range.nodeid_out == ino &&
				in.body.copy_file_range.fh_out == fh &&
				(off_t)in.body.copy_file_range.off_out == off_out &&
				in.body.copy_file_range.len == (size_t)len &&
				in.body.copy_file_range.flags == 0);
		}, Eq(true)),
		_)
	).WillOnce(Invoke(ReturnImmediate([=](auto in __unused, auto& out) {
		SET_OUT_HEADER_LEN(out, write);
		out.body.write.size = len;
	})));

	fd = open(FULLPATH, O_RDWR);
	ASSERT_GE(fd, 0);
	ASSERT_EQ(len, copy_file_range(fd, &off_in, fd, &off_out, len, 0));

	/* Check that cached attributes were updated appropriately */
	ASSERT_EQ(0, fstat(fd, &sb)) << strerror(errno);
	EXPECT_EQ(fsize + len, sb.st_size);

	leak(fd);
}

/* With older protocol versions, no FUSE_COPY_FILE_RANGE should be attempted */
TEST_F(CopyFileRange_7_27, fallback)
{
	const char FULLPATH1[] = "mountpoint/src.txt";
	const char RELPATH1[] = "src.txt";
	const char FULLPATH2[] = "mountpoint/dst.txt";
	const char RELPATH2[] = "dst.txt";
	const uint64_t ino1 = 42;
	const uint64_t ino2 = 43;
	const uint64_t fh1 = 0xdeadbeef1a7ebabe;
	const uint64_t fh2 = 0xdeadc0de88c0ffee;
	off_t fsize2 = 0;
	off_t start1 = 0;
	off_t start2 = 0;
	const char *contents = "Hello, world!";
	ssize_t len;
	int fd1, fd2;

	len = strlen(contents);

	/* 
	 * Ensure that we read to EOF, just so the buffer cache's read size is
	 * predictable.
	 */
	expect_lookup(RELPATH1, ino1, S_IFREG | 0644, start1 + len, 1);
	expect_lookup(RELPATH2, ino2, S_IFREG | 0644, fsize2, 1);
	expect_open(ino1, 0, 1, fh1);
	expect_open(ino2, 0, 1, fh2);
	EXPECT_CALL(*m_mock, process(
		ResultOf([=](auto in) {
			return (in.header.opcode == FUSE_COPY_FILE_RANGE);
		}, Eq(true)),
		_)
	).Times(0);
	expect_maybe_lseek(ino1);
	expect_read(ino1, start1, len, len, contents, 0);
	expect_write(ino2, start2, len, len, contents);

	fd1 = open(FULLPATH1, O_RDONLY);
	ASSERT_GE(fd1, 0);
	fd2 = open(FULLPATH2, O_WRONLY);
	ASSERT_GE(fd2, 0);
	ASSERT_EQ(len, copy_file_range(fd1, &start1, fd2, &start2, len, 0));
}

/*
 * With -o noatime, copy_file_range should update the destination's mtime and
 * ctime, but not the source's atime.
 */
TEST_F(CopyFileRangeNoAtime, timestamps)
{
	const char FULLPATH1[] = "mountpoint/src.txt";
	const char RELPATH1[] = "src.txt";
	const char FULLPATH2[] = "mountpoint/dst.txt";
	const char RELPATH2[] = "dst.txt";
	struct stat sb1a, sb1b, sb2a, sb2b;
	const uint64_t ino1 = 42;
	const uint64_t ino2 = 43;
	const uint64_t fh1 = 0xdeadbeef1a7ebabe;
	const uint64_t fh2 = 0xdeadc0de88c0ffee;
	off_t fsize1 = 1 << 20;		/* 1 MiB */
	off_t fsize2 = 1 << 19;		/* 512 KiB */
	off_t start1 = 1 << 18;
	off_t start2 = 3 << 17;
	ssize_t len = 65536;
	int fd1, fd2;

	expect_lookup(RELPATH1, ino1, S_IFREG | 0644, fsize1, 1);
	expect_lookup(RELPATH2, ino2, S_IFREG | 0644, fsize2, 1);
	expect_open(ino1, 0, 1, fh1);
	expect_open(ino2, 0, 1, fh2);
	EXPECT_CALL(*m_mock, process(
		ResultOf([=](auto in) {
			return (in.header.opcode == FUSE_COPY_FILE_RANGE &&
				in.header.nodeid == ino1 &&
				in.body.copy_file_range.fh_in == fh1 &&
				(off_t)in.body.copy_file_range.off_in == start1 &&
				in.body.copy_file_range.nodeid_out == ino2 &&
				in.body.copy_file_range.fh_out == fh2 &&
				(off_t)in.body.copy_file_range.off_out == start2 &&
				in.body.copy_file_range.len == (size_t)len &&
				in.body.copy_file_range.flags == 0);
		}, Eq(true)),
		_)
	).WillOnce(Invoke(ReturnImmediate([=](auto in __unused, auto& out) {
		SET_OUT_HEADER_LEN(out, write);
		out.body.write.size = len;
	})));

	fd1 = open(FULLPATH1, O_RDONLY);
	ASSERT_GE(fd1, 0);
	fd2 = open(FULLPATH2, O_WRONLY);
	ASSERT_GE(fd2, 0);
	ASSERT_EQ(0, fstat(fd1, &sb1a)) << strerror(errno);
	ASSERT_EQ(0, fstat(fd2, &sb2a)) << strerror(errno);

	nap();

	ASSERT_EQ(len, copy_file_range(fd1, &start1, fd2, &start2, len, 0));
	ASSERT_EQ(0, fstat(fd1, &sb1b)) << strerror(errno);
	ASSERT_EQ(0, fstat(fd2, &sb2b)) << strerror(errno);

	EXPECT_EQ(sb1a.st_atime, sb1b.st_atime);
	EXPECT_EQ(sb1a.st_mtime, sb1b.st_mtime);
	EXPECT_EQ(sb1a.st_ctime, sb1b.st_ctime);
	EXPECT_EQ(sb2a.st_atime, sb2b.st_atime);
	EXPECT_NE(sb2a.st_mtime, sb2b.st_mtime);
	EXPECT_NE(sb2a.st_ctime, sb2b.st_ctime);

	leak(fd1);
	leak(fd2);
}
