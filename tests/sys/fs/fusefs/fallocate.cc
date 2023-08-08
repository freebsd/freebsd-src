/*-
 * SPDX-License-Identifier: BSD-2-Clause
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

/* Is buf all zero? */
static bool
is_zero(const char *buf, uint64_t size)
{
    return buf[0] == 0 && !memcmp(buf, buf + 1, size - 1);
}

class Fallocate: public FuseTest {
public:
/*
 * expect VOP_DEALLOCATE to be implemented by vop_stddeallocate.
 */
void expect_vop_stddeallocate(uint64_t ino, uint64_t off, uint64_t length)
{
	/* XXX read offset and size may depend on cache mode */
	EXPECT_CALL(*m_mock, process(
		ResultOf([=](auto in) {
			return (in.header.opcode == FUSE_READ &&
				in.header.nodeid == ino &&
				in.body.read.offset <= off &&
				in.body.read.offset + in.body.read.size >=
					off + length);
		}, Eq(true)),
		_)
	).WillOnce(Invoke(ReturnImmediate([=](auto in, auto& out) {
		assert(in.body.read.size <= sizeof(out.body.bytes));
		out.header.len = sizeof(struct fuse_out_header) +
			in.body.read.size;
		memset(out.body.bytes, 'X', in.body.read.size);
	}))).RetiresOnSaturation();
	EXPECT_CALL(*m_mock, process(
		ResultOf([=](auto in) {
			const char *buf = (const char*)in.body.bytes +
				sizeof(struct fuse_write_in);

			assert(length <= sizeof(in.body.bytes) -
				sizeof(struct fuse_write_in));
			return (in.header.opcode == FUSE_WRITE &&
				in.header.nodeid == ino &&
				in.body.write.offset == off  &&
				in.body.write.size == length &&
				is_zero(buf, length));
		}, Eq(true)),
		_)
	).WillOnce(Invoke(ReturnImmediate([=](auto in __unused, auto& out) {
		SET_OUT_HEADER_LEN(out, write);
		out.body.write.size = length;
	})));
}
};

class Fspacectl: public Fallocate {};

class Fspacectl_7_18: public Fspacectl {
public:
virtual void SetUp() {
	m_kernel_minor_version = 18;
	Fspacectl::SetUp();
}
};

class FspacectlCache: public Fspacectl, public WithParamInterface<cache_mode> {
public:
bool m_direct_io;

FspacectlCache(): m_direct_io(false) {};

virtual void SetUp() {
	int cache_mode = GetParam();
	switch (cache_mode) {
		case Uncached:
			m_direct_io = true;
			break;
		case WritebackAsync:
			m_async = true;
			/* FALLTHROUGH */
		case Writeback:
			m_init_flags |= FUSE_WRITEBACK_CACHE;
			/* FALLTHROUGH */
		case Writethrough:
			break;
		default:
			FAIL() << "Unknown cache mode";
	}

	FuseTest::SetUp();
	if (IsSkipped())
		return;
}
};

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
 * FUSE_FALLOCATE.  This and future calls should fall back to vop_stddeallocate.
 */
TEST_F(Fspacectl, enosys)
{
	const char FULLPATH[] = "mountpoint/some_file.txt";
	const char RELPATH[] = "some_file.txt";
	off_t fsize = 1 << 20;
	off_t off0 = 100;
	off_t len0 = 500;
	struct spacectl_range rqsr = { .r_offset = off0, .r_len = len0 };
	uint64_t ino = 42;
	uint64_t off1 = fsize;
	uint64_t len1 = 1000;
	off_t off2 = fsize / 2;
	off_t len2 = 500;
	int fd;

	expect_lookup(RELPATH, ino, S_IFREG | 0644, fsize, 1);
	expect_open(ino, 0, 1);
	expect_fallocate(ino, off0, len0,
		FUSE_FALLOC_FL_KEEP_SIZE | FUSE_FALLOC_FL_PUNCH_HOLE, ENOSYS);
	expect_vop_stddeallocate(ino, off0, len0);
	expect_vop_stddeallocate(ino, off2, len2);

	fd = open(FULLPATH, O_RDWR);
	ASSERT_LE(0, fd) << strerror(errno);
	EXPECT_EQ(0, fspacectl(fd, SPACECTL_DEALLOC, &rqsr, 0, NULL));

	/* Subsequent calls shouldn't query the daemon either */
	rqsr.r_offset = off2;
	rqsr.r_len = len2;
	EXPECT_EQ(0, fspacectl(fd, SPACECTL_DEALLOC, &rqsr, 0, NULL));

	/* Neither should posix_fallocate query the daemon */
	EXPECT_EQ(EINVAL, posix_fallocate(fd, off1, len1));

	leak(fd);
}

/*
 * EOPNOTSUPP means "the file system does not support fallocate with the
 * supplied mode on this particular file".  So we should fallback, but not
 * assume anything about whether the operation will fail on a different file or
 * with a different mode.
 */
TEST_F(Fspacectl, eopnotsupp)
{
	const char FULLPATH[] = "mountpoint/some_file.txt";
	const char RELPATH[] = "some_file.txt";
	struct spacectl_range rqsr;
	uint64_t ino = 42;
	uint64_t fsize = 1 << 20;
	uint64_t off0 = 500;
	uint64_t len = 1000;
	uint64_t off1 = fsize / 2;
	int fd;

	expect_lookup(RELPATH, ino, S_IFREG | 0644, fsize, 1);
	expect_open(ino, 0, 1);
	expect_fallocate(ino, off0, len,
		FUSE_FALLOC_FL_KEEP_SIZE | FUSE_FALLOC_FL_PUNCH_HOLE,
	                EOPNOTSUPP);
	expect_vop_stddeallocate(ino, off0, len);
	expect_fallocate(ino, off1, len,
		FUSE_FALLOC_FL_KEEP_SIZE | FUSE_FALLOC_FL_PUNCH_HOLE,
	                EOPNOTSUPP);
	expect_vop_stddeallocate(ino, off1, len);
	expect_fallocate(ino, fsize, len, 0, 0);

	fd = open(FULLPATH, O_RDWR);
	ASSERT_LE(0, fd) << strerror(errno);

	/*
	 * Though the FUSE daemon will reject the call, the kernel should fall
	 * back to a read-modify-write approach.
	 */
	rqsr.r_offset = off0;
	rqsr.r_len = len;
	EXPECT_EQ(0, fspacectl(fd, SPACECTL_DEALLOC, &rqsr, 0, NULL));

	/* Subsequent calls should still query the daemon */
	rqsr.r_offset = off1;
	rqsr.r_len = len;
	EXPECT_EQ(0, fspacectl(fd, SPACECTL_DEALLOC, &rqsr, 0, NULL));

	/* But subsequent posix_fallocate calls _should_ query the daemon */
	EXPECT_EQ(0, posix_fallocate(fd, fsize, len));

	leak(fd);
}

TEST_F(Fspacectl, erofs)
{
	const char FULLPATH[] = "mountpoint/some_file.txt";
	const char RELPATH[] = "some_file.txt";
	struct statfs statbuf;
	uint64_t fsize = 2000;
	struct spacectl_range rqsr = { .r_offset = 0, .r_len = 1 };
	struct iovec *iov = NULL;
	int iovlen = 0;
	uint64_t ino = 42;
	int fd;
	int newflags;

	expect_lookup(RELPATH, ino, S_IFREG | 0644, fsize, 1);
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

	EXPECT_EQ(-1, fspacectl(fd, SPACECTL_DEALLOC, &rqsr, 0, NULL));
	EXPECT_EQ(EROFS, errno);

	leak(fd);
}

TEST_F(Fspacectl, ok)
{
	const char FULLPATH[] = "mountpoint/some_file.txt";
	const char RELPATH[] = "some_file.txt";
	struct spacectl_range rqsr, rmsr;
	struct stat sb0, sb1;
	uint64_t ino = 42;
	uint64_t fsize = 2000;
	uint64_t offset = 500;
	uint64_t length = 1000;
	int fd;

	expect_lookup(RELPATH, ino, S_IFREG | 0644, fsize, 1);
	expect_open(ino, 0, 1);
	expect_fallocate(ino, offset, length,
		FUSE_FALLOC_FL_KEEP_SIZE | FUSE_FALLOC_FL_PUNCH_HOLE, 0);

	fd = open(FULLPATH, O_RDWR);
	ASSERT_LE(0, fd) << strerror(errno);
	ASSERT_EQ(0, fstat(fd, &sb0)) << strerror(errno);
	rqsr.r_offset = offset;
	rqsr.r_len = length;
	EXPECT_EQ(0, fspacectl(fd, SPACECTL_DEALLOC, &rqsr, 0, &rmsr));
	EXPECT_EQ(0, rmsr.r_len);
	EXPECT_EQ((off_t)(offset + length), rmsr.r_offset);

	/*
	 * The file's attributes should not have been invalidated, so this fstat
	 * will not requery the daemon.
	 */
	EXPECT_EQ(0, fstat(fd, &sb1));
	EXPECT_EQ(fsize, (uint64_t)sb1.st_size);

	/* mtime and ctime should be updated */
	EXPECT_EQ(sb0.st_atime, sb1.st_atime);
	EXPECT_NE(sb0.st_mtime, sb1.st_mtime);
	EXPECT_NE(sb0.st_ctime, sb1.st_ctime);

	leak(fd);
}

/* The returned rqsr.r_off should be clipped at EoF */
TEST_F(Fspacectl, past_eof)
{
	const char FULLPATH[] = "mountpoint/some_file.txt";
	const char RELPATH[] = "some_file.txt";
	struct spacectl_range rqsr, rmsr;
	uint64_t ino = 42;
	uint64_t fsize = 1000;
	uint64_t offset = 1500;
	uint64_t length = 1000;
	int fd;

	expect_lookup(RELPATH, ino, S_IFREG | 0644, fsize, 1);
	expect_open(ino, 0, 1);
	expect_fallocate(ino, offset, length,
		FUSE_FALLOC_FL_KEEP_SIZE | FUSE_FALLOC_FL_PUNCH_HOLE, 0);

	fd = open(FULLPATH, O_RDWR);
	ASSERT_LE(0, fd) << strerror(errno);
	rqsr.r_offset = offset;
	rqsr.r_len = length;
	EXPECT_EQ(0, fspacectl(fd, SPACECTL_DEALLOC, &rqsr, 0, &rmsr));
	EXPECT_EQ(0, rmsr.r_len);
	EXPECT_EQ((off_t)fsize, rmsr.r_offset);

	leak(fd);
}

/* The returned rqsr.r_off should be clipped at EoF */
TEST_F(Fspacectl, spans_eof)
{
	const char FULLPATH[] = "mountpoint/some_file.txt";
	const char RELPATH[] = "some_file.txt";
	struct spacectl_range rqsr, rmsr;
	uint64_t ino = 42;
	uint64_t fsize = 1000;
	uint64_t offset = 500;
	uint64_t length = 1000;
	int fd;

	expect_lookup(RELPATH, ino, S_IFREG | 0644, fsize, 1);
	expect_open(ino, 0, 1);
	expect_fallocate(ino, offset, length,
		FUSE_FALLOC_FL_KEEP_SIZE | FUSE_FALLOC_FL_PUNCH_HOLE, 0);

	fd = open(FULLPATH, O_RDWR);
	ASSERT_LE(0, fd) << strerror(errno);
	rqsr.r_offset = offset;
	rqsr.r_len = length;
	EXPECT_EQ(0, fspacectl(fd, SPACECTL_DEALLOC, &rqsr, 0, &rmsr));
	EXPECT_EQ(0, rmsr.r_len);
	EXPECT_EQ((off_t)fsize, rmsr.r_offset);

	leak(fd);
}

/*
 * With older servers, no FUSE_FALLOCATE should be attempted.  The kernel
 * should fall back to vop_stddeallocate.
 */
TEST_F(Fspacectl_7_18, ok)
{
	const char FULLPATH[] = "mountpoint/some_file.txt";
	const char RELPATH[] = "some_file.txt";
	struct spacectl_range rqsr, rmsr;
	void *buf;
	uint64_t ino = 42;
	uint64_t fsize = 2000;
	uint64_t offset = 500;
	uint64_t length = 1000;
	int fd;

	buf = malloc(length);

	expect_lookup(RELPATH, ino, S_IFREG | 0644, fsize, 1);
	expect_open(ino, 0, 1);
	expect_vop_stddeallocate(ino, offset, length);

	fd = open(FULLPATH, O_RDWR);
	ASSERT_LE(0, fd) << strerror(errno);
	rqsr.r_offset = offset;
	rqsr.r_len = length;
	EXPECT_EQ(0, fspacectl(fd, SPACECTL_DEALLOC, &rqsr, 0, &rmsr));
	EXPECT_EQ(0, rmsr.r_len);
	EXPECT_EQ((off_t)(offset + length), rmsr.r_offset);

	leak(fd);
	free(buf);
}

/*
 * A successful fspacectl should clear the zeroed data from the kernel cache.
 */
TEST_P(FspacectlCache, clears_cache)
{
	const char FULLPATH[] = "mountpoint/some_file.txt";
	const char RELPATH[] = "some_file.txt";
	const char *CONTENTS = "abcdefghijklmnopqrstuvwxyz";
	struct spacectl_range rqsr, rmsr;
	uint64_t ino = 42;
	ssize_t bufsize = strlen(CONTENTS);
	uint64_t fsize = bufsize;
	uint8_t buf[bufsize];
	char zbuf[bufsize];
	uint64_t offset = 0;
	uint64_t length = bufsize;
	int fd;

	bzero(zbuf, bufsize);

	expect_lookup(RELPATH, ino, S_IFREG | 0644, fsize, 1);
	expect_open(ino, 0, 1);
	/* NB: expectations are applied in LIFO order */
	expect_read(ino, 0, fsize, fsize, zbuf);
	expect_read(ino, 0, fsize, fsize, CONTENTS);
	expect_fallocate(ino, offset, length,
		FUSE_FALLOC_FL_KEEP_SIZE | FUSE_FALLOC_FL_PUNCH_HOLE, 0);

	fd = open(FULLPATH, O_RDWR);
	ASSERT_LE(0, fd) << strerror(errno);

	/* Populate the cache */
	ASSERT_EQ(fsize, (uint64_t)pread(fd, buf, bufsize, 0))
		<< strerror(errno);
	ASSERT_EQ(0, memcmp(buf, CONTENTS, fsize));

	/* Zero the file */
	rqsr.r_offset = offset;
	rqsr.r_len = length;
	EXPECT_EQ(0, fspacectl(fd, SPACECTL_DEALLOC, &rqsr, 0, &rmsr));
	EXPECT_EQ(0, rmsr.r_len);
	EXPECT_EQ((off_t)(offset + length), rmsr.r_offset);

	/* Read again.  This should query the daemon */
	ASSERT_EQ(fsize, (uint64_t)pread(fd, buf, bufsize, 0))
		<< strerror(errno);
	ASSERT_EQ(0, memcmp(buf, zbuf, fsize));

	leak(fd);
}

INSTANTIATE_TEST_SUITE_P(FspacectlCache, FspacectlCache,
	Values(Uncached, Writethrough, Writeback)
);

/*
 * If the server returns ENOSYS, it indicates that the server does not support
 * FUSE_FALLOCATE.  This and future calls should return EINVAL.
 */
TEST_F(PosixFallocate, enosys)
{
	const char FULLPATH[] = "mountpoint/some_file.txt";
	const char RELPATH[] = "some_file.txt";
	uint64_t ino = 42;
	uint64_t off0 = 0;
	uint64_t len0 = 1000;
	off_t off1 = 100;
	off_t len1 = 200;
	uint64_t fsize = 500;
	struct spacectl_range rqsr = { .r_offset = off1, .r_len = len1 };
	int fd;

	expect_lookup(RELPATH, ino, S_IFREG | 0644, fsize, 1);
	expect_open(ino, 0, 1);
	expect_fallocate(ino, off0, len0, 0, ENOSYS);
	expect_vop_stddeallocate(ino, off1, len1);

	fd = open(FULLPATH, O_RDWR);
	ASSERT_LE(0, fd) << strerror(errno);
	EXPECT_EQ(EINVAL, posix_fallocate(fd, off0, len0));

	/* Subsequent calls shouldn't query the daemon*/
	EXPECT_EQ(EINVAL, posix_fallocate(fd, off0, len0));

	/* Neither should VOP_DEALLOCATE query the daemon */
	EXPECT_EQ(0, fspacectl(fd, SPACECTL_DEALLOC, &rqsr, 0, NULL));

	leak(fd);
}

/*
 * EOPNOTSUPP means "the file system does not support fallocate with the
 * supplied mode on this particular file".  So we should fallback, but not
 * assume anything about whether the operation will fail on a different file or
 * with a different mode.
 */
TEST_F(PosixFallocate, eopnotsupp)
{
	const char FULLPATH[] = "mountpoint/some_file.txt";
	const char RELPATH[] = "some_file.txt";
	struct spacectl_range rqsr;
	uint64_t ino = 42;
	uint64_t fsize = 2000;
	uint64_t offset = 0;
	uint64_t length = 1000;
	int fd;

	expect_lookup(RELPATH, ino, S_IFREG | 0644, fsize, 1);
	expect_open(ino, 0, 1);
	expect_fallocate(ino, fsize, length, 0, EOPNOTSUPP);
	expect_fallocate(ino, offset, length, 0, EOPNOTSUPP);
	expect_fallocate(ino, offset, length,
		FUSE_FALLOC_FL_KEEP_SIZE | FUSE_FALLOC_FL_PUNCH_HOLE, 0);

	fd = open(FULLPATH, O_RDWR);
	ASSERT_LE(0, fd) << strerror(errno);
	EXPECT_EQ(EINVAL, posix_fallocate(fd, fsize, length));

	/* Subsequent calls should still query the daemon*/
	EXPECT_EQ(EINVAL, posix_fallocate(fd, offset, length));

	/* And subsequent VOP_DEALLOCATE calls should also query the daemon */
	rqsr.r_len = length;
	rqsr.r_offset = offset;
	EXPECT_EQ(0, fspacectl(fd, SPACECTL_DEALLOC, &rqsr, 0, NULL));

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
