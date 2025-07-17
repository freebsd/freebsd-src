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
#include <unistd.h>
}

#include "mockfs.hh"
#include "utils.hh"

using namespace testing;

class Link: public FuseTest {
public:
void expect_link(uint64_t ino, const char *relpath, mode_t mode, uint32_t nlink)
{
	EXPECT_CALL(*m_mock, process(
		ResultOf([=](auto in) {
			const char *name = (const char*)in.body.bytes
				+ sizeof(struct fuse_link_in);
			return (in.header.opcode == FUSE_LINK &&
				in.body.link.oldnodeid == ino &&
				(0 == strcmp(name, relpath)));
		}, Eq(true)),
		_)
	).WillOnce(Invoke(ReturnImmediate([=](auto in __unused, auto& out) {
		SET_OUT_HEADER_LEN(out, entry);
		out.body.entry.nodeid = ino;
		out.body.entry.attr.mode = mode;
		out.body.entry.attr.nlink = nlink;
		out.body.entry.attr_valid = UINT64_MAX;
		out.body.entry.entry_valid = UINT64_MAX;
	})));
}

void expect_lookup(const char *relpath, uint64_t ino)
{
	FuseTest::expect_lookup(relpath, ino, S_IFREG | 0644, 0, 1);
}
};

class Link_7_8: public FuseTest {
public:
virtual void SetUp() {
	m_kernel_minor_version = 8;
	FuseTest::SetUp();
}

void expect_link(uint64_t ino, const char *relpath, mode_t mode, uint32_t nlink)
{
	EXPECT_CALL(*m_mock, process(
		ResultOf([=](auto in) {
			const char *name = (const char*)in.body.bytes
				+ sizeof(struct fuse_link_in);
			return (in.header.opcode == FUSE_LINK &&
				in.body.link.oldnodeid == ino &&
				(0 == strcmp(name, relpath)));
		}, Eq(true)),
		_)
	).WillOnce(Invoke(ReturnImmediate([=](auto in __unused, auto& out) {
		SET_OUT_HEADER_LEN(out, entry_7_8);
		out.body.entry.nodeid = ino;
		out.body.entry.attr.mode = mode;
		out.body.entry.attr.nlink = nlink;
		out.body.entry.attr_valid = UINT64_MAX;
		out.body.entry.entry_valid = UINT64_MAX;
	})));
}

void expect_lookup(const char *relpath, uint64_t ino)
{
	FuseTest::expect_lookup_7_8(relpath, ino, S_IFREG | 0644, 0, 1);
}
};

/*
 * A successful link should clear the parent directory's attribute cache,
 * because the fuse daemon should update its mtime and ctime
 */
TEST_F(Link, clear_attr_cache)
{
	const char FULLPATH[] = "mountpoint/src";
	const char RELPATH[] = "src";
	const char FULLDST[] = "mountpoint/dst";
	const char RELDST[] = "dst";
	const uint64_t ino = 42;
	mode_t mode = S_IFREG | 0644;
	struct stat sb;

	EXPECT_LOOKUP(FUSE_ROOT_ID, RELPATH)
	.WillOnce(Invoke(ReturnErrno(ENOENT)));
	EXPECT_CALL(*m_mock, process(
		ResultOf([=](auto in) {
			return (in.header.opcode == FUSE_GETATTR &&
				in.header.nodeid == FUSE_ROOT_ID);
		}, Eq(true)),
		_)
	).Times(2)
	.WillRepeatedly(Invoke(ReturnImmediate([=](auto i __unused, auto& out) {
		SET_OUT_HEADER_LEN(out, attr);
		out.body.attr.attr.ino = FUSE_ROOT_ID;
		out.body.attr.attr.mode = S_IFDIR | 0755;
		out.body.attr.attr_valid = UINT64_MAX;
	})));

	EXPECT_LOOKUP(FUSE_ROOT_ID, RELDST)
	
	.WillOnce(Invoke(ReturnImmediate([=](auto in __unused, auto& out) {
		SET_OUT_HEADER_LEN(out, entry);
		out.body.entry.attr.mode = mode;
		out.body.entry.nodeid = ino;
		out.body.entry.attr.nlink = 1;
		out.body.entry.attr_valid = UINT64_MAX;
		out.body.entry.entry_valid = UINT64_MAX;
	})));
	expect_link(ino, RELPATH, mode, 2);

	EXPECT_EQ(0, stat("mountpoint", &sb)) << strerror(errno);
	EXPECT_EQ(0, link(FULLDST, FULLPATH)) << strerror(errno);
	EXPECT_EQ(0, stat("mountpoint", &sb)) << strerror(errno);
}

TEST_F(Link, emlink)
{
	const char FULLPATH[] = "mountpoint/lnk";
	const char RELPATH[] = "lnk";
	const char FULLDST[] = "mountpoint/dst";
	const char RELDST[] = "dst";
	uint64_t dst_ino = 42;

	EXPECT_LOOKUP(FUSE_ROOT_ID, RELPATH)
	.WillOnce(Invoke(ReturnErrno(ENOENT)));
	expect_lookup(RELDST, dst_ino);

	EXPECT_CALL(*m_mock, process(
		ResultOf([=](auto in) {
			const char *name = (const char*)in.body.bytes
				+ sizeof(struct fuse_link_in);
			return (in.header.opcode == FUSE_LINK &&
				in.body.link.oldnodeid == dst_ino &&
				(0 == strcmp(name, RELPATH)));
		}, Eq(true)),
		_)
	).WillOnce(Invoke(ReturnErrno(EMLINK)));

	EXPECT_EQ(-1, link(FULLDST, FULLPATH));
	EXPECT_EQ(EMLINK, errno);
}

/*
 * A hard link should always have the same inode as its source.  If it doesn't,
 * then it's not a hard link.
 */
TEST_F(Link, bad_inode)
{
	const char FULLPATH[] = "mountpoint/src";
	const char RELPATH[] = "src";
	const char FULLDST[] = "mountpoint/dst";
	const char RELDST[] = "dst";
	const uint64_t src_ino = 42;
	const uint64_t dst_ino = 43;
	mode_t mode = S_IFREG | 0644;

	EXPECT_LOOKUP(FUSE_ROOT_ID, RELPATH)
	.WillOnce(Invoke(ReturnErrno(ENOENT)));
	EXPECT_LOOKUP(FUSE_ROOT_ID, RELDST)
	.WillOnce(Invoke(ReturnImmediate([=](auto in __unused, auto& out) {
		SET_OUT_HEADER_LEN(out, entry);
		out.body.entry.attr.mode = mode;
		out.body.entry.nodeid = dst_ino;
		out.body.entry.attr.nlink = 1;
		out.body.entry.attr_valid = UINT64_MAX;
		out.body.entry.entry_valid = UINT64_MAX;
	})));
	EXPECT_CALL(*m_mock, process(
		ResultOf([=](auto in) {
			const char *name = (const char*)in.body.bytes
				+ sizeof(struct fuse_link_in);
			return (in.header.opcode == FUSE_LINK &&
				in.body.link.oldnodeid == dst_ino &&
				(0 == strcmp(name, RELPATH)));
		}, Eq(true)),
		_)
	).WillOnce(Invoke(ReturnImmediate([=](auto in __unused, auto& out) {
		SET_OUT_HEADER_LEN(out, entry);
		out.body.entry.nodeid = src_ino;
		out.body.entry.attr.mode = mode;
		out.body.entry.attr.nlink = 2;
		out.body.entry.attr_valid = UINT64_MAX;
		out.body.entry.entry_valid = UINT64_MAX;
	})));

	ASSERT_EQ(-1, link(FULLDST, FULLPATH));
	ASSERT_EQ(EIO, errno);
}

TEST_F(Link, ok)
{
	const char FULLPATH[] = "mountpoint/src";
	const char RELPATH[] = "src";
	const char FULLDST[] = "mountpoint/dst";
	const char RELDST[] = "dst";
	const uint64_t ino = 42;
	mode_t mode = S_IFREG | 0644;
	struct stat sb;

	EXPECT_LOOKUP(FUSE_ROOT_ID, RELPATH)
	.WillOnce(Invoke(ReturnErrno(ENOENT)));
	EXPECT_LOOKUP(FUSE_ROOT_ID, RELDST)
	.WillOnce(Invoke(ReturnImmediate([=](auto in __unused, auto& out) {
		SET_OUT_HEADER_LEN(out, entry);
		out.body.entry.attr.mode = mode;
		out.body.entry.nodeid = ino;
		out.body.entry.attr.nlink = 1;
		out.body.entry.attr_valid = UINT64_MAX;
		out.body.entry.entry_valid = UINT64_MAX;
	})));
	expect_link(ino, RELPATH, mode, 2);

	ASSERT_EQ(0, link(FULLDST, FULLPATH)) << strerror(errno);
	// Check that the original file's nlink count has increased.
	ASSERT_EQ(0, stat(FULLDST, &sb)) << strerror(errno);
	EXPECT_EQ(2ul, sb.st_nlink);
}

TEST_F(Link_7_8, ok)
{
	const char FULLPATH[] = "mountpoint/src";
	const char RELPATH[] = "src";
	const char FULLDST[] = "mountpoint/dst";
	const char RELDST[] = "dst";
	const uint64_t ino = 42;
	mode_t mode = S_IFREG | 0644;
	struct stat sb;

	EXPECT_LOOKUP(FUSE_ROOT_ID, RELPATH)
	.WillOnce(Invoke(ReturnErrno(ENOENT)));
	EXPECT_LOOKUP(FUSE_ROOT_ID, RELDST)
	.WillOnce(Invoke(ReturnImmediate([=](auto in __unused, auto& out) {
		SET_OUT_HEADER_LEN(out, entry_7_8);
		out.body.entry.attr.mode = mode;
		out.body.entry.nodeid = ino;
		out.body.entry.attr.nlink = 1;
		out.body.entry.attr_valid = UINT64_MAX;
		out.body.entry.entry_valid = UINT64_MAX;
	})));
	expect_link(ino, RELPATH, mode, 2);

	ASSERT_EQ(0, link(FULLDST, FULLPATH)) << strerror(errno);
	// Check that the original file's nlink count has increased.
	ASSERT_EQ(0, stat(FULLDST, &sb)) << strerror(errno);
	EXPECT_EQ(2ul, sb.st_nlink);
}
