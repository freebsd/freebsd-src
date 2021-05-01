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

#include <fcntl.h>
}

#include "mockfs.hh"
#include "utils.hh"

using namespace testing;

class Lseek: public FuseTest {};
class LseekPathconf: public Lseek {};
class LseekPathconf_7_23: public LseekPathconf {
public:
virtual void SetUp() {
	m_kernel_minor_version = 23;
	FuseTest::SetUp();
}
};
class LseekSeekHole: public Lseek {};
class LseekSeekData: public Lseek {};

/*
 * If a previous lseek operation has already returned enosys, then pathconf can
 * return EINVAL immediately.
 */
TEST_F(LseekPathconf, already_enosys)
{
	const char FULLPATH[] = "mountpoint/some_file.txt";
	const char RELPATH[] = "some_file.txt";
	const uint64_t ino = 42;
	off_t fsize = 1 << 30;	/* 1 GiB */
	off_t offset_in = 1 << 28;
	int fd;

	expect_lookup(RELPATH, ino, S_IFREG | 0644, fsize, 1);
	expect_open(ino, 0, 1);
	EXPECT_CALL(*m_mock, process(
		ResultOf([=](auto in) {
			return (in.header.opcode == FUSE_LSEEK);
		}, Eq(true)),
		_)
	).WillOnce(Invoke(ReturnErrno(ENOSYS)));

	fd = open(FULLPATH, O_RDONLY);

	EXPECT_EQ(offset_in, lseek(fd, offset_in, SEEK_DATA));
	EXPECT_EQ(-1, fpathconf(fd, _PC_MIN_HOLE_SIZE));
	EXPECT_EQ(EINVAL, errno);
}

/*
 * If a previous lseek operation has already returned successfully, then
 * pathconf can return 1 immediately.  1 means "holes are reported, but size is
 * not specified".
 */
TEST_F(LseekPathconf, already_seeked)
{
	const char FULLPATH[] = "mountpoint/some_file.txt";
	const char RELPATH[] = "some_file.txt";
	const uint64_t ino = 42;
	off_t fsize = 1 << 30;	/* 1 GiB */
	off_t offset = 1 << 28;
	int fd;

	expect_lookup(RELPATH, ino, S_IFREG | 0644, fsize, 1);
	expect_open(ino, 0, 1);
	EXPECT_CALL(*m_mock, process(
		ResultOf([=](auto in) {
			return (in.header.opcode == FUSE_LSEEK);
		}, Eq(true)),
		_)
	).WillOnce(Invoke(ReturnImmediate([=](auto i, auto& out) {
		SET_OUT_HEADER_LEN(out, lseek);
		out.body.lseek.offset = i.body.lseek.offset;
	})));
	fd = open(FULLPATH, O_RDONLY);
	EXPECT_EQ(offset, lseek(fd, offset, SEEK_DATA));

	EXPECT_EQ(1, fpathconf(fd, _PC_MIN_HOLE_SIZE));
}

/*
 * If no FUSE_LSEEK operation has been attempted since mount, try once as soon
 * as a pathconf request comes in.
 */
TEST_F(LseekPathconf, enosys_now)
{
	const char FULLPATH[] = "mountpoint/some_file.txt";
	const char RELPATH[] = "some_file.txt";
	const uint64_t ino = 42;
	off_t fsize = 1 << 30;	/* 1 GiB */
	int fd;

	expect_lookup(RELPATH, ino, S_IFREG | 0644, fsize, 1);
	expect_open(ino, 0, 1);
	EXPECT_CALL(*m_mock, process(
		ResultOf([=](auto in) {
			return (in.header.opcode == FUSE_LSEEK);
		}, Eq(true)),
		_)
	).WillOnce(Invoke(ReturnErrno(ENOSYS)));

	fd = open(FULLPATH, O_RDONLY);

	EXPECT_EQ(-1, fpathconf(fd, _PC_MIN_HOLE_SIZE));
	EXPECT_EQ(EINVAL, errno);
}

/*
 * If no FUSE_LSEEK operation has been attempted since mount, try one as soon
 * as a pathconf request comes in.  This is the typical pattern of bsdtar.  It
 * will only try SEEK_HOLE/SEEK_DATA if fpathconf says they're supported.
 */
TEST_F(LseekPathconf, seek_now)
{
	const char FULLPATH[] = "mountpoint/some_file.txt";
	const char RELPATH[] = "some_file.txt";
	const uint64_t ino = 42;
	off_t fsize = 1 << 30;	/* 1 GiB */
	off_t offset_initial = 1 << 27;
	off_t offset_out = 1 << 29;
	int fd;

	expect_lookup(RELPATH, ino, S_IFREG | 0644, fsize, 1);
	expect_open(ino, 0, 1);
	EXPECT_CALL(*m_mock, process(
		ResultOf([=](auto in) {
			return (in.header.opcode == FUSE_LSEEK);
		}, Eq(true)),
		_)
	).WillOnce(Invoke(ReturnImmediate([=](auto i __unused, auto& out) {
		SET_OUT_HEADER_LEN(out, lseek);
		out.body.lseek.offset = offset_out;
	})));

	fd = open(FULLPATH, O_RDONLY);
	EXPECT_EQ(offset_initial, lseek(fd, offset_initial, SEEK_SET));
	EXPECT_EQ(1, fpathconf(fd, _PC_MIN_HOLE_SIZE));
	/* And check that the file pointer hasn't changed */
	EXPECT_EQ(offset_initial, lseek(fd, 0, SEEK_CUR));
}

/*
 * For servers using older protocol versions, no FUSE_LSEEK should be attempted
 */
TEST_F(LseekPathconf_7_23, already_enosys)
{
	const char FULLPATH[] = "mountpoint/some_file.txt";
	const char RELPATH[] = "some_file.txt";
	const uint64_t ino = 42;
	off_t fsize = 1 << 30;	/* 1 GiB */
	int fd;

	expect_lookup(RELPATH, ino, S_IFREG | 0644, fsize, 1);
	expect_open(ino, 0, 1);
	EXPECT_CALL(*m_mock, process(
		ResultOf([=](auto in) {
			return (in.header.opcode == FUSE_LSEEK);
		}, Eq(true)),
		_)
	).Times(0);

	fd = open(FULLPATH, O_RDONLY);
	EXPECT_EQ(-1, fpathconf(fd, _PC_MIN_HOLE_SIZE));
	EXPECT_EQ(EINVAL, errno);
}

TEST_F(LseekSeekData, ok)
{
	const char FULLPATH[] = "mountpoint/some_file.txt";
	const char RELPATH[] = "some_file.txt";
	const uint64_t ino = 42;
	off_t fsize = 1 << 30;	/* 1 GiB */
	off_t offset_in = 1 << 28;
	off_t offset_out = 1 << 29;
	int fd;

	expect_lookup(RELPATH, ino, S_IFREG | 0644, fsize, 1);
	expect_open(ino, 0, 1);
	EXPECT_CALL(*m_mock, process(
		ResultOf([=](auto in) {
			return (in.header.opcode == FUSE_LSEEK &&
				in.header.nodeid == ino &&
				in.body.lseek.fh == FH &&
				(off_t)in.body.lseek.offset == offset_in &&
				in.body.lseek.whence == SEEK_DATA);
		}, Eq(true)),
		_)
	).WillOnce(Invoke(ReturnImmediate([=](auto i __unused, auto& out) {
		SET_OUT_HEADER_LEN(out, lseek);
		out.body.lseek.offset = offset_out;
	})));
	fd = open(FULLPATH, O_RDONLY);
	EXPECT_EQ(offset_out, lseek(fd, offset_in, SEEK_DATA));
	EXPECT_EQ(offset_out, lseek(fd, 0, SEEK_CUR));
}

/*
 * If the server returns ENOSYS, fusefs should fall back to the default
 * behavior, and never query the server again.
 */
TEST_F(LseekSeekData, enosys)
{
	const char FULLPATH[] = "mountpoint/some_file.txt";
	const char RELPATH[] = "some_file.txt";
	const uint64_t ino = 42;
	off_t fsize = 1 << 30;	/* 1 GiB */
	off_t offset_in = 1 << 28;
	int fd;

	expect_lookup(RELPATH, ino, S_IFREG | 0644, fsize, 1);
	expect_open(ino, 0, 1);
	EXPECT_CALL(*m_mock, process(
		ResultOf([=](auto in) {
			return (in.header.opcode == FUSE_LSEEK &&
				in.header.nodeid == ino &&
				in.body.lseek.fh == FH &&
				(off_t)in.body.lseek.offset == offset_in &&
				in.body.lseek.whence == SEEK_DATA);
		}, Eq(true)),
		_)
	).WillOnce(Invoke(ReturnErrno(ENOSYS)));
	fd = open(FULLPATH, O_RDONLY);

	/*
	 * Default behavior: ENXIO if offset is < 0 or >= fsize, offset
	 * otherwise.
	 */
	EXPECT_EQ(offset_in, lseek(fd, offset_in, SEEK_DATA));
	EXPECT_EQ(-1, lseek(fd, -1, SEEK_HOLE));
	EXPECT_EQ(ENXIO, errno);
	EXPECT_EQ(-1, lseek(fd, fsize, SEEK_HOLE));
	EXPECT_EQ(ENXIO, errno);
}

TEST_F(LseekSeekHole, ok)
{
	const char FULLPATH[] = "mountpoint/some_file.txt";
	const char RELPATH[] = "some_file.txt";
	const uint64_t ino = 42;
	off_t fsize = 1 << 30;	/* 1 GiB */
	off_t offset_in = 1 << 28;
	off_t offset_out = 1 << 29;
	int fd;

	expect_lookup(RELPATH, ino, S_IFREG | 0644, fsize, 1);
	expect_open(ino, 0, 1);
	EXPECT_CALL(*m_mock, process(
		ResultOf([=](auto in) {
			return (in.header.opcode == FUSE_LSEEK &&
				in.header.nodeid == ino &&
				in.body.lseek.fh == FH &&
				(off_t)in.body.lseek.offset == offset_in &&
				in.body.lseek.whence == SEEK_HOLE);
		}, Eq(true)),
		_)
	).WillOnce(Invoke(ReturnImmediate([=](auto i __unused, auto& out) {
		SET_OUT_HEADER_LEN(out, lseek);
		out.body.lseek.offset = offset_out;
	})));
	fd = open(FULLPATH, O_RDONLY);
	EXPECT_EQ(offset_out, lseek(fd, offset_in, SEEK_HOLE));
	EXPECT_EQ(offset_out, lseek(fd, 0, SEEK_CUR));
}

/*
 * If the server returns ENOSYS, fusefs should fall back to the default
 * behavior, and never query the server again.
 */
TEST_F(LseekSeekHole, enosys)
{
	const char FULLPATH[] = "mountpoint/some_file.txt";
	const char RELPATH[] = "some_file.txt";
	const uint64_t ino = 42;
	off_t fsize = 1 << 30;	/* 1 GiB */
	off_t offset_in = 1 << 28;
	int fd;

	expect_lookup(RELPATH, ino, S_IFREG | 0644, fsize, 1);
	expect_open(ino, 0, 1);
	EXPECT_CALL(*m_mock, process(
		ResultOf([=](auto in) {
			return (in.header.opcode == FUSE_LSEEK &&
				in.header.nodeid == ino &&
				in.body.lseek.fh == FH &&
				(off_t)in.body.lseek.offset == offset_in &&
				in.body.lseek.whence == SEEK_HOLE);
		}, Eq(true)),
		_)
	).WillOnce(Invoke(ReturnErrno(ENOSYS)));
	fd = open(FULLPATH, O_RDONLY);

	/*
	 * Default behavior: ENXIO if offset is < 0 or >= fsize, fsize
	 * otherwise.
	 */
	EXPECT_EQ(fsize, lseek(fd, offset_in, SEEK_HOLE));
	EXPECT_EQ(-1, lseek(fd, -1, SEEK_HOLE));
	EXPECT_EQ(ENXIO, errno);
	EXPECT_EQ(-1, lseek(fd, fsize, SEEK_HOLE));
	EXPECT_EQ(ENXIO, errno);
}

/* lseek should return ENXIO when offset points to EOF */
TEST_F(LseekSeekHole, enxio)
{
	const char FULLPATH[] = "mountpoint/some_file.txt";
	const char RELPATH[] = "some_file.txt";
	const uint64_t ino = 42;
	off_t fsize = 1 << 30;	/* 1 GiB */
	off_t offset_in = fsize;
	int fd;

	expect_lookup(RELPATH, ino, S_IFREG | 0644, fsize, 1);
	expect_open(ino, 0, 1);
	EXPECT_CALL(*m_mock, process(
		ResultOf([=](auto in) {
			return (in.header.opcode == FUSE_LSEEK &&
				in.header.nodeid == ino &&
				in.body.lseek.fh == FH &&
				(off_t)in.body.lseek.offset == offset_in &&
				in.body.lseek.whence == SEEK_HOLE);
		}, Eq(true)),
		_)
	).WillOnce(Invoke(ReturnErrno(ENXIO)));
	fd = open(FULLPATH, O_RDONLY);
	EXPECT_EQ(-1, lseek(fd, offset_in, SEEK_HOLE));
	EXPECT_EQ(ENXIO, errno);
}
