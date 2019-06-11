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
#include <stdlib.h>
#include <unistd.h>
}

#include "mockfs.hh"
#include "utils.hh"

/* 
 * For testing I/O like fsx does, but deterministically and without a real
 * underlying file system
 *
 * TODO: after fusefs gains the options to select cache mode for each mount
 * point, run each of these tests for all cache modes.
 */

using namespace testing;

const char FULLPATH[] = "mountpoint/some_file.txt";
const char RELPATH[] = "some_file.txt";
const uint64_t ino = 42;

class Io: public FuseTest {
public:
int m_backing_fd, m_control_fd, m_test_fd;

Io(): m_backing_fd(-1), m_control_fd(-1) {};

void SetUp()
{
	m_backing_fd = open("backing_file", O_RDWR | O_CREAT | O_TRUNC);
	if (m_backing_fd < 0)
		FAIL() << strerror(errno);
	m_control_fd = open("control", O_RDWR | O_CREAT | O_TRUNC);
	if (m_control_fd < 0)
		FAIL() << strerror(errno);
	srandom(22'9'1982);	// Seed with my birthday
	FuseTest::SetUp();
	if (IsSkipped())
		return;

	expect_lookup(RELPATH, ino, S_IFREG | 0644, 0, 1);
	expect_open(ino, 0, 1);
	EXPECT_CALL(*m_mock, process(
		ResultOf([=](auto in) {
			return (in.header.opcode == FUSE_WRITE &&
				in.header.nodeid == ino);
		}, Eq(true)),
		_)
	).WillRepeatedly(Invoke(ReturnImmediate([=](auto in, auto& out) {
		const char *buf = (const char*)in.body.bytes +
			sizeof(struct fuse_write_in);
		ssize_t isize = in.body.write.size;
		off_t iofs = in.body.write.offset;

		ASSERT_EQ(isize, pwrite(m_backing_fd, buf, isize, iofs))
			<< strerror(errno);
		SET_OUT_HEADER_LEN(out, write);
		out.body.write.size = isize;
	})));
	EXPECT_CALL(*m_mock, process(
		ResultOf([=](auto in) {
			return (in.header.opcode == FUSE_READ &&
				in.header.nodeid == ino);
		}, Eq(true)),
		_)
	).WillRepeatedly(Invoke(ReturnImmediate([=](auto in, auto& out) {
		ssize_t isize = in.body.write.size;
		off_t iofs = in.body.write.offset;
		void *buf = out.body.bytes;

		ASSERT_LE(0, pread(m_backing_fd, buf, isize, iofs))
			<< strerror(errno);
		out.header.len = sizeof(struct fuse_out_header) + isize;
	})));
	EXPECT_CALL(*m_mock, process(
		ResultOf([=](auto in) {
			uint32_t valid = FATTR_SIZE | FATTR_FH;
			return (in.header.opcode == FUSE_SETATTR &&
				in.header.nodeid == ino &&
				in.body.setattr.valid == valid);
		}, Eq(true)),
		_)
	).WillRepeatedly(Invoke(ReturnImmediate([=](auto in, auto& out) {
		ASSERT_EQ(0, ftruncate(m_backing_fd, in.body.setattr.size))
			<< strerror(errno);
		SET_OUT_HEADER_LEN(out, attr);
		out.body.attr.attr.ino = ino;
		out.body.attr.attr.mode = S_IFREG | 0755;
		out.body.attr.attr.size = in.body.setattr.size;
		out.body.attr.attr_valid = UINT64_MAX;
	})));
	/* Any test that close()s will send FUSE_FLUSH and FUSE_RELEASE */
	EXPECT_CALL(*m_mock, process(
		ResultOf([=](auto in) {
			return (in.header.opcode == FUSE_FLUSH &&
				in.header.nodeid == ino);
		}, Eq(true)),
		_)
	).WillRepeatedly(Invoke(ReturnErrno(0)));
	EXPECT_CALL(*m_mock, process(
		ResultOf([=](auto in) {
			return (in.header.opcode == FUSE_RELEASE &&
				in.header.nodeid == ino);
		}, Eq(true)),
		_)
	).WillRepeatedly(Invoke(ReturnErrno(0)));

	m_test_fd = open(FULLPATH, O_RDWR );
	EXPECT_LE(0, m_test_fd) << strerror(errno);
}

void TearDown()
{
	if (m_backing_fd >= 0)
		close(m_backing_fd);
	if (m_control_fd >= 0)
		close(m_control_fd);
	FuseTest::TearDown();
	/* Deliberately leak test_fd */
}

void do_ftruncate(off_t offs)
{
	ASSERT_EQ(0, ftruncate(m_test_fd, offs)) << strerror(errno);
	ASSERT_EQ(0, ftruncate(m_control_fd, offs)) << strerror(errno);
}

void do_read(ssize_t size, off_t offs)
{
	void *test_buf, *control_buf;

	test_buf = malloc(size);
	ASSERT_NE(NULL, test_buf) << strerror(errno);
	control_buf = malloc(size);
	ASSERT_NE(NULL, control_buf) << strerror(errno);

	ASSERT_EQ(size, pread(m_test_fd, test_buf, size, offs))
		<< strerror(errno);
	ASSERT_EQ(size, pread(m_control_fd, control_buf, size, offs))
		<< strerror(errno);

	ASSERT_EQ(0, memcmp(test_buf, control_buf, size));

	free(control_buf);
	free(test_buf);
}

void do_write(ssize_t size, off_t offs)
{
	char *buf;
	long i;

	buf = (char*)malloc(size);
	ASSERT_NE(NULL, buf) << strerror(errno);
	for (i=0; i < size; i++)
		buf[i] = random();

	ASSERT_EQ(size, pwrite(m_test_fd, buf, size, offs ))
		<< strerror(errno);
	ASSERT_EQ(size, pwrite(m_control_fd, buf, size, offs))
		<< strerror(errno);
}

};

/*
 * Extend a file with dirty data in the last page of the last block.
 *
 * fsx -WR -P /tmp -S8 -N3 fsx.bin
 */
TEST_F(Io, extend_from_dirty_page)
{
	off_t wofs = 0x21a0;
	ssize_t wsize = 0xf0a8;
	off_t rofs = 0xb284;
	ssize_t rsize = 0x9b22;
	off_t truncsize = 0x28702;

	do_write(wsize, wofs);
	do_ftruncate(truncsize);
	do_read(rsize, rofs);
}

/*
 * When writing the last page of a file, it must be written synchronously.
 * Otherwise the cached page can become invalid by a subsequent extend
 * operation.
 *
 * fsx -WR -P /tmp -S642 -N3 fsx.bin
 */
TEST_F(Io, last_page)
{
	off_t wofs0 = 0x1134f;
	ssize_t wsize0 = 0xcc77;
	off_t wofs1 = 0x2096a;
	ssize_t wsize1 = 0xdfa7;
	off_t rofs = 0x1a3aa;
	ssize_t rsize = 0xb5b7;

	do_write(wsize0, wofs0);
	do_write(wsize1, wofs1);
	do_read(rsize, rofs);
}

/* 
 * Read a hole from a block that contains some cached data.
 *
 * fsx -WR -P /tmp -S55  fsx.bin
 */
TEST_F(Io, read_hole_from_cached_block)
{
	off_t wofs = 0x160c5;
	ssize_t wsize = 0xa996;
	off_t rofs = 0x472e;
	ssize_t rsize = 0xd8d5;

	do_write(wsize, wofs);
	do_read(rsize, rofs);
}

/*
 * Reliable panic; I don't yet know why.
 * Disabled because it panics.
 *
 * fsx -WR -P /tmp -S839 -d -N6 fsx.bin
 */
TEST_F(Io, DISABLED_fault_on_nofault_entry)
{
	off_t wofs0 = 0x3bad7;
	ssize_t wsize0 = 0x4529;
	off_t wofs1 = 0xc30d;
	ssize_t wsize1 = 0x5f77;
	off_t truncsize0 = 0x10916;
	off_t rofs = 0xdf17;
	ssize_t rsize = 0x29ff;
	off_t truncsize1 = 0x152b4;

	do_write(wsize0, wofs0);
	do_write(wsize1, wofs1);
	do_ftruncate(truncsize0);
	do_read(rsize, rofs);
	do_ftruncate(truncsize1);
	close(m_test_fd);
}
