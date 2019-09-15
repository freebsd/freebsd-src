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
#include <unistd.h>
}

#include "mockfs.hh"
#include "utils.hh"

using namespace testing;

class Symlink: public FuseTest {
public:

void expect_symlink(uint64_t ino, const char *target, const char *relpath)
{
	EXPECT_CALL(*m_mock, process(
		ResultOf([=](auto in) {
			const char *name = (const char*)in.body.bytes;
			const char *linkname = name + strlen(name) + 1;
			return (in.header.opcode == FUSE_SYMLINK &&
				(0 == strcmp(linkname, target)) &&
				(0 == strcmp(name, relpath)));
		}, Eq(true)),
		_)
	).WillOnce(Invoke(ReturnImmediate([=](auto in __unused, auto& out) {
		SET_OUT_HEADER_LEN(out, entry);
		out.body.entry.attr.mode = S_IFLNK | 0777;
		out.body.entry.nodeid = ino;
		out.body.entry.entry_valid = UINT64_MAX;
		out.body.entry.attr_valid = UINT64_MAX;
	})));
}

};

class Symlink_7_8: public FuseTest {
public:
virtual void SetUp() {
	m_kernel_minor_version = 8;
	FuseTest::SetUp();
}

void expect_symlink(uint64_t ino, const char *target, const char *relpath)
{
	EXPECT_CALL(*m_mock, process(
		ResultOf([=](auto in) {
			const char *name = (const char*)in.body.bytes;
			const char *linkname = name + strlen(name) + 1;
			return (in.header.opcode == FUSE_SYMLINK &&
				(0 == strcmp(linkname, target)) &&
				(0 == strcmp(name, relpath)));
		}, Eq(true)),
		_)
	).WillOnce(Invoke(ReturnImmediate([=](auto in __unused, auto& out) {
		SET_OUT_HEADER_LEN(out, entry_7_8);
		out.body.entry.attr.mode = S_IFLNK | 0777;
		out.body.entry.nodeid = ino;
		out.body.entry.entry_valid = UINT64_MAX;
		out.body.entry.attr_valid = UINT64_MAX;
	})));
}

};

/*
 * A successful symlink should clear the parent directory's attribute cache,
 * because the fuse daemon should update its mtime and ctime
 */
TEST_F(Symlink, clear_attr_cache)
{
	const char FULLPATH[] = "mountpoint/src";
	const char RELPATH[] = "src";
	const char dst[] = "dst";
	const uint64_t ino = 42;
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
	expect_symlink(ino, dst, RELPATH);

	EXPECT_EQ(0, stat("mountpoint", &sb)) << strerror(errno);
	EXPECT_EQ(0, symlink(dst, FULLPATH)) << strerror(errno);
	EXPECT_EQ(0, stat("mountpoint", &sb)) << strerror(errno);
}

TEST_F(Symlink, enospc)
{
	const char FULLPATH[] = "mountpoint/lnk";
	const char RELPATH[] = "lnk";
	const char dst[] = "dst";

	EXPECT_LOOKUP(FUSE_ROOT_ID, RELPATH)
	.WillOnce(Invoke(ReturnErrno(ENOENT)));

	EXPECT_CALL(*m_mock, process(
		ResultOf([=](auto in) {
			const char *name = (const char*)in.body.bytes;
			const char *linkname = name + strlen(name) + 1;
			return (in.header.opcode == FUSE_SYMLINK &&
				(0 == strcmp(linkname, dst)) &&
				(0 == strcmp(name, RELPATH)));
		}, Eq(true)),
		_)
	).WillOnce(Invoke(ReturnErrno(ENOSPC)));

	EXPECT_EQ(-1, symlink(dst, FULLPATH));
	EXPECT_EQ(ENOSPC, errno);
}

TEST_F(Symlink, ok)
{
	const char FULLPATH[] = "mountpoint/src";
	const char RELPATH[] = "src";
	const char dst[] = "dst";
	const uint64_t ino = 42;

	EXPECT_LOOKUP(FUSE_ROOT_ID, RELPATH)
	.WillOnce(Invoke(ReturnErrno(ENOENT)));
	expect_symlink(ino, dst, RELPATH);

	EXPECT_EQ(0, symlink(dst, FULLPATH)) << strerror(errno);
}

TEST_F(Symlink_7_8, ok)
{
	const char FULLPATH[] = "mountpoint/src";
	const char RELPATH[] = "src";
	const char dst[] = "dst";
	const uint64_t ino = 42;

	EXPECT_LOOKUP(FUSE_ROOT_ID, RELPATH)
	.WillOnce(Invoke(ReturnErrno(ENOENT)));
	expect_symlink(ino, dst, RELPATH);

	EXPECT_EQ(0, symlink(dst, FULLPATH)) << strerror(errno);
}
