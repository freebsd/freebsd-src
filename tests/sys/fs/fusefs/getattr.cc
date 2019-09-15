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
#include <sys/param.h>
}

#include "mockfs.hh"
#include "utils.hh"

using namespace testing;

class Getattr : public FuseTest {
public:
void expect_lookup(const char *relpath, uint64_t ino, mode_t mode,
	uint64_t size, int times, uint64_t attr_valid, uint32_t attr_valid_nsec)
{
	EXPECT_LOOKUP(FUSE_ROOT_ID, relpath)
	.Times(times)
	.WillRepeatedly(Invoke(ReturnImmediate([=](auto in __unused, auto& out) {
		SET_OUT_HEADER_LEN(out, entry);
		out.body.entry.attr.mode = mode;
		out.body.entry.nodeid = ino;
		out.body.entry.attr.nlink = 1;
		out.body.entry.attr_valid = attr_valid;
		out.body.entry.attr_valid_nsec = attr_valid_nsec;
		out.body.entry.attr.size = size;
		out.body.entry.entry_valid = UINT64_MAX;
	})));
}
};

class Getattr_7_8: public FuseTest {
public:
virtual void SetUp() {
	m_kernel_minor_version = 8;
	FuseTest::SetUp();
}
};

/*
 * If getattr returns a non-zero cache timeout, then subsequent VOP_GETATTRs
 * should use the cached attributes, rather than query the daemon
 */
TEST_F(Getattr, attr_cache)
{
	const char FULLPATH[] = "mountpoint/some_file.txt";
	const char RELPATH[] = "some_file.txt";
	const uint64_t ino = 42;
	struct stat sb;

	EXPECT_LOOKUP(FUSE_ROOT_ID, RELPATH)
	.WillRepeatedly(Invoke(ReturnImmediate([=](auto i __unused, auto& out) {
		SET_OUT_HEADER_LEN(out, entry);
		out.body.entry.attr.mode = S_IFREG | 0644;
		out.body.entry.nodeid = ino;
		out.body.entry.entry_valid = UINT64_MAX;
	})));
	EXPECT_CALL(*m_mock, process(
		ResultOf([](auto in) {
			return (in.header.opcode == FUSE_GETATTR &&
				in.header.nodeid == ino);
		}, Eq(true)),
		_)
	).WillOnce(Invoke(ReturnImmediate([](auto i __unused, auto& out) {
		SET_OUT_HEADER_LEN(out, attr);
		out.body.attr.attr_valid = UINT64_MAX;
		out.body.attr.attr.ino = ino;	// Must match nodeid
		out.body.attr.attr.mode = S_IFREG | 0644;
	})));
	EXPECT_EQ(0, stat(FULLPATH, &sb));
	/* The second stat(2) should use cached attributes */
	EXPECT_EQ(0, stat(FULLPATH, &sb));
}

/*
 * If getattr returns a finite but non-zero cache timeout, then we should
 * discard the cached attributes and requery the daemon after the timeout
 * period passes.
 */
TEST_F(Getattr, attr_cache_timeout)
{
	const char FULLPATH[] = "mountpoint/some_file.txt";
	const char RELPATH[] = "some_file.txt";
	const uint64_t ino = 42;
	struct stat sb;

	expect_lookup(RELPATH, ino, S_IFREG | 0644, 0, 1, 0, 0);
	EXPECT_CALL(*m_mock, process(
		ResultOf([](auto in) {
			return (in.header.opcode == FUSE_GETATTR &&
				in.header.nodeid == ino);
		}, Eq(true)),
		_)
	).Times(2)
	.WillRepeatedly(Invoke(ReturnImmediate([=](auto i __unused, auto& out) {
		SET_OUT_HEADER_LEN(out, attr);
		out.body.attr.attr_valid_nsec = NAP_NS / 2;
		out.body.attr.attr_valid = 0;
		out.body.attr.attr.ino = ino;	// Must match nodeid
		out.body.attr.attr.mode = S_IFREG | 0644;
	})));

	EXPECT_EQ(0, stat(FULLPATH, &sb));
	nap();
	/* Timeout has expired. stat(2) should requery the daemon */
	EXPECT_EQ(0, stat(FULLPATH, &sb));
}

/* 
 * If attr.blksize is zero, then the kernel should use a default value for
 * st_blksize
 */
TEST_F(Getattr, blksize_zero)
{
	const char FULLPATH[] = "mountpoint/some_file.txt";
	const char RELPATH[] = "some_file.txt";
	const uint64_t ino = 42;
	struct stat sb;

	expect_lookup(RELPATH, ino, S_IFREG | 0644, 1, 1, 0, 0);
	EXPECT_CALL(*m_mock, process(
		ResultOf([](auto in) {
			return (in.header.opcode == FUSE_GETATTR &&
				in.header.nodeid == ino);
		}, Eq(true)),
		_)
	).WillOnce(Invoke(ReturnImmediate([](auto i __unused, auto& out) {
		SET_OUT_HEADER_LEN(out, attr);
		out.body.attr.attr.mode = S_IFREG | 0644;
		out.body.attr.attr.ino = ino;	// Must match nodeid
		out.body.attr.attr.blksize = 0;
	})));

	ASSERT_EQ(0, stat(FULLPATH, &sb)) << strerror(errno);
	EXPECT_EQ((blksize_t)PAGE_SIZE, sb.st_blksize);
}

TEST_F(Getattr, enoent)
{
	const char FULLPATH[] = "mountpoint/some_file.txt";
	const char RELPATH[] = "some_file.txt";
	struct stat sb;
	const uint64_t ino = 42;

	expect_lookup(RELPATH, ino, S_IFREG | 0644, 0, 1, 0, 0);
	EXPECT_CALL(*m_mock, process(
		ResultOf([](auto in) {
			return (in.header.opcode == FUSE_GETATTR &&
				in.header.nodeid == ino);
		}, Eq(true)),
		_)
	).WillOnce(Invoke(ReturnErrno(ENOENT)));
	EXPECT_NE(0, stat(FULLPATH, &sb));
	EXPECT_EQ(ENOENT, errno);
}

TEST_F(Getattr, ok)
{
	const char FULLPATH[] = "mountpoint/some_file.txt";
	const char RELPATH[] = "some_file.txt";
	const uint64_t ino = 42;
	struct stat sb;

	expect_lookup(RELPATH, ino, S_IFREG | 0644, 1, 1, 0, 0);
	EXPECT_CALL(*m_mock, process(
		ResultOf([](auto in) {
			return (in.header.opcode == FUSE_GETATTR &&
				in.body.getattr.getattr_flags == 0 &&
				in.header.nodeid == ino);
		}, Eq(true)),
		_)
	).WillOnce(Invoke(ReturnImmediate([](auto i __unused, auto& out) {
		SET_OUT_HEADER_LEN(out, attr);
		out.body.attr.attr.ino = ino;	// Must match nodeid
		out.body.attr.attr.mode = S_IFREG | 0644;
		out.body.attr.attr.size = 1;
		out.body.attr.attr.blocks = 2;
		out.body.attr.attr.atime = 3;
		out.body.attr.attr.mtime = 4;
		out.body.attr.attr.ctime = 5;
		out.body.attr.attr.atimensec = 6;
		out.body.attr.attr.mtimensec = 7;
		out.body.attr.attr.ctimensec = 8;
		out.body.attr.attr.nlink = 9;
		out.body.attr.attr.uid = 10;
		out.body.attr.attr.gid = 11;
		out.body.attr.attr.rdev = 12;
		out.body.attr.attr.blksize = 12345;
	})));

	ASSERT_EQ(0, stat(FULLPATH, &sb)) << strerror(errno);
	EXPECT_EQ(1, sb.st_size);
	EXPECT_EQ(2, sb.st_blocks);
	EXPECT_EQ(3, sb.st_atim.tv_sec);
	EXPECT_EQ(6, sb.st_atim.tv_nsec);
	EXPECT_EQ(4, sb.st_mtim.tv_sec);
	EXPECT_EQ(7, sb.st_mtim.tv_nsec);
	EXPECT_EQ(5, sb.st_ctim.tv_sec);
	EXPECT_EQ(8, sb.st_ctim.tv_nsec);
	EXPECT_EQ(9ull, sb.st_nlink);
	EXPECT_EQ(10ul, sb.st_uid);
	EXPECT_EQ(11ul, sb.st_gid);
	EXPECT_EQ(12ul, sb.st_rdev);
	EXPECT_EQ((blksize_t)12345, sb.st_blksize);
	EXPECT_EQ(ino, sb.st_ino);
	EXPECT_EQ(S_IFREG | 0644, sb.st_mode);

	//st_birthtim and st_flags are not supported by protocol 7.8.  They're
	//only supported as OS-specific extensions to OSX.
	//EXPECT_EQ(, sb.st_birthtim);
	//EXPECT_EQ(, sb.st_flags);
	
	//FUSE can't set st_blksize until protocol 7.9
}

TEST_F(Getattr_7_8, ok)
{
	const char FULLPATH[] = "mountpoint/some_file.txt";
	const char RELPATH[] = "some_file.txt";
	const uint64_t ino = 42;
	struct stat sb;

	EXPECT_LOOKUP(FUSE_ROOT_ID, RELPATH)
	.WillOnce(Invoke(ReturnImmediate([=](auto in __unused, auto& out) {
		SET_OUT_HEADER_LEN(out, entry_7_8);
		out.body.entry.attr.mode = S_IFREG | 0644;
		out.body.entry.nodeid = ino;
		out.body.entry.attr.nlink = 1;
		out.body.entry.attr.size = 1;
	})));
	EXPECT_CALL(*m_mock, process(
		ResultOf([](auto in) {
			return (in.header.opcode == FUSE_GETATTR &&
				in.header.nodeid == ino);
		}, Eq(true)),
		_)
	).WillOnce(Invoke(ReturnImmediate([](auto i __unused, auto& out) {
		SET_OUT_HEADER_LEN(out, attr_7_8);
		out.body.attr.attr.ino = ino;	// Must match nodeid
		out.body.attr.attr.mode = S_IFREG | 0644;
		out.body.attr.attr.size = 1;
		out.body.attr.attr.blocks = 2;
		out.body.attr.attr.atime = 3;
		out.body.attr.attr.mtime = 4;
		out.body.attr.attr.ctime = 5;
		out.body.attr.attr.atimensec = 6;
		out.body.attr.attr.mtimensec = 7;
		out.body.attr.attr.ctimensec = 8;
		out.body.attr.attr.nlink = 9;
		out.body.attr.attr.uid = 10;
		out.body.attr.attr.gid = 11;
		out.body.attr.attr.rdev = 12;
	})));

	ASSERT_EQ(0, stat(FULLPATH, &sb)) << strerror(errno);
	EXPECT_EQ(1, sb.st_size);
	EXPECT_EQ(2, sb.st_blocks);
	EXPECT_EQ(3, sb.st_atim.tv_sec);
	EXPECT_EQ(6, sb.st_atim.tv_nsec);
	EXPECT_EQ(4, sb.st_mtim.tv_sec);
	EXPECT_EQ(7, sb.st_mtim.tv_nsec);
	EXPECT_EQ(5, sb.st_ctim.tv_sec);
	EXPECT_EQ(8, sb.st_ctim.tv_nsec);
	EXPECT_EQ(9ull, sb.st_nlink);
	EXPECT_EQ(10ul, sb.st_uid);
	EXPECT_EQ(11ul, sb.st_gid);
	EXPECT_EQ(12ul, sb.st_rdev);
	EXPECT_EQ(ino, sb.st_ino);
	EXPECT_EQ(S_IFREG | 0644, sb.st_mode);

	//st_birthtim and st_flags are not supported by protocol 7.8.  They're
	//only supported as OS-specific extensions to OSX.
}
