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
#include <sys/ioctl.h>
#include <sys/filio.h>

#include <fcntl.h>
}

#include "mockfs.hh"
#include "utils.hh"

using namespace testing;

const static char FULLPATH[] = "mountpoint/foo";
const static char RELPATH[] = "foo";

class Bmap: public FuseTest {
public:
virtual void SetUp() {
	m_maxreadahead = UINT32_MAX;
	FuseTest::SetUp();
}
void expect_bmap(uint64_t ino, uint64_t lbn, uint32_t blocksize, uint64_t pbn)
{
	EXPECT_CALL(*m_mock, process(
		ResultOf([=](auto in) {
			return (in.header.opcode == FUSE_BMAP &&
				in.header.nodeid == ino &&
				in.body.bmap.block == lbn &&
				in.body.bmap.blocksize == blocksize);
		}, Eq(true)),
		_)
	).WillOnce(Invoke(ReturnImmediate([=](auto i __unused, auto& out) {
		SET_OUT_HEADER_LEN(out, bmap);
		out.body.bmap.block = pbn;
	})));
}
	
void expect_lookup(const char *relpath, uint64_t ino, off_t size)
{
	FuseTest::expect_lookup(relpath, ino, S_IFREG | 0644, size, 1,
		UINT64_MAX);
}
};

/*
 * Test FUSE_BMAP
 * XXX The FUSE protocol does not include the runp and runb variables, so those
 * must be guessed in-kernel.
 */
TEST_F(Bmap, bmap)
{
	struct fiobmap2_arg arg;
	const off_t filesize = 1 << 20;
	const ino_t ino = 42;
	int64_t lbn = 10;
	int64_t pbn = 12345;
	int fd;

	expect_lookup(RELPATH, 42, filesize);
	expect_open(ino, 0, 1);
	expect_bmap(ino, lbn, m_maxbcachebuf, pbn);

	fd = open(FULLPATH, O_RDWR);
	ASSERT_LE(0, fd) << strerror(errno);

	arg.bn = lbn;
	arg.runp = -1;
	arg.runb = -1;
	ASSERT_EQ(0, ioctl(fd, FIOBMAP2, &arg)) << strerror(errno);
	EXPECT_EQ(arg.bn, pbn);
	EXPECT_EQ(arg.runp, m_maxphys / m_maxbcachebuf - 1);
	EXPECT_EQ(arg.runb, m_maxphys / m_maxbcachebuf - 1);
}

/* 
 * If the daemon does not implement VOP_BMAP, fusefs should return sensible
 * defaults.
 */
TEST_F(Bmap, default_)
{
	struct fiobmap2_arg arg;
	const off_t filesize = 1 << 20;
	const ino_t ino = 42;
	int64_t lbn;
	int fd;

	expect_lookup(RELPATH, 42, filesize);
	expect_open(ino, 0, 1);
	EXPECT_CALL(*m_mock, process(
		ResultOf([=](auto in) {
			return (in.header.opcode == FUSE_BMAP);
		}, Eq(true)),
		_)
	).WillOnce(Invoke(ReturnErrno(ENOSYS)));

	fd = open(FULLPATH, O_RDWR);
	ASSERT_LE(0, fd) << strerror(errno);

	/* First block */
	lbn = 0;
	arg.bn = lbn;
	arg.runp = -1;
	arg.runb = -1;
	ASSERT_EQ(0, ioctl(fd, FIOBMAP2, &arg)) << strerror(errno);
	EXPECT_EQ(arg.bn, 0);
	EXPECT_EQ(arg.runp, m_maxphys / m_maxbcachebuf - 1);
	EXPECT_EQ(arg.runb, 0);

	/* In the middle */
	lbn = filesize / m_maxbcachebuf / 2;
	arg.bn = lbn;
	arg.runp = -1;
	arg.runb = -1;
	ASSERT_EQ(0, ioctl(fd, FIOBMAP2, &arg)) << strerror(errno);
	EXPECT_EQ(arg.bn, lbn * m_maxbcachebuf / DEV_BSIZE);
	EXPECT_EQ(arg.runp, m_maxphys / m_maxbcachebuf - 1);
	EXPECT_EQ(arg.runb, m_maxphys / m_maxbcachebuf - 1);

	/* Last block */
	lbn = filesize / m_maxbcachebuf - 1;
	arg.bn = lbn;
	arg.runp = -1;
	arg.runb = -1;
	ASSERT_EQ(0, ioctl(fd, FIOBMAP2, &arg)) << strerror(errno);
	EXPECT_EQ(arg.bn, lbn * m_maxbcachebuf / DEV_BSIZE);
	EXPECT_EQ(arg.runp, 0);
	EXPECT_EQ(arg.runb, m_maxphys / m_maxbcachebuf - 1);

	leak(fd);
}
