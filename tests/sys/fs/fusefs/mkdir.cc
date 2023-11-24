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
#include <fcntl.h>
}

#include "mockfs.hh"
#include "utils.hh"

using namespace testing;

class Mkdir: public FuseTest {};
class Mkdir_7_8: public FuseTest {
public:
virtual void SetUp() {
	m_kernel_minor_version = 8;
	FuseTest::SetUp();
}
};

/* 
 * EMLINK is possible on filesystems that limit the number of hard links to a
 * single file, like early versions of BtrFS
 */
TEST_F(Mkdir, emlink)
{
	const char FULLPATH[] = "mountpoint/some_dir";
	const char RELPATH[] = "some_dir";
	mode_t mode = 0755;

	EXPECT_LOOKUP(FUSE_ROOT_ID, RELPATH)
	.WillOnce(Invoke(ReturnErrno(ENOENT)));

	EXPECT_CALL(*m_mock, process(
		ResultOf([=](auto in) {
			const char *name = (const char*)in.body.bytes +
				sizeof(fuse_mkdir_in);
			return (in.header.opcode == FUSE_MKDIR &&
				in.body.mkdir.mode == (S_IFDIR | mode) &&
				(0 == strcmp(RELPATH, name)));
		}, Eq(true)),
		_)
	).WillOnce(Invoke(ReturnErrno(EMLINK)));

	ASSERT_NE(1, mkdir(FULLPATH, mode));
	ASSERT_EQ(EMLINK, errno);
}

/*
 * Creating a new directory after FUSE_LOOKUP returned a negative cache entry
 */
TEST_F(Mkdir, entry_cache_negative)
{
	const char FULLPATH[] = "mountpoint/some_file.txt";
	const char RELPATH[] = "some_file.txt";
	mode_t mode = 0755;
	uint64_t ino = 42;
	/* 
	 * Set entry_valid = 0 because this test isn't concerned with whether
	 * or not we actually cache negative entries, only with whether we
	 * interpret negative cache responses correctly.
	 */
	struct timespec entry_valid = {.tv_sec = 0, .tv_nsec = 0};

	/* mkdir will first do a LOOKUP, adding a negative cache entry */
	EXPECT_LOOKUP(FUSE_ROOT_ID, RELPATH)
	.WillOnce(ReturnNegativeCache(&entry_valid));

	EXPECT_CALL(*m_mock, process(
		ResultOf([=](auto in) {
			const char *name = (const char*)in.body.bytes +
				sizeof(fuse_open_in);
			return (in.header.opcode == FUSE_MKDIR &&
				in.body.mkdir.mode == (S_IFDIR | mode) &&
				(0 == strcmp(RELPATH, name)));
		}, Eq(true)),
		_)
	).WillOnce(Invoke(ReturnImmediate([=](auto in __unused, auto& out) {
		SET_OUT_HEADER_LEN(out, entry);
		out.body.create.entry.attr.mode = S_IFDIR | mode;
		out.body.create.entry.nodeid = ino;
		out.body.create.entry.entry_valid = UINT64_MAX;
		out.body.create.entry.attr_valid = UINT64_MAX;
	})));

	ASSERT_EQ(0, mkdir(FULLPATH, mode)) << strerror(errno);
}

/*
 * Creating a new directory should purge any negative namecache entries
 */
TEST_F(Mkdir, entry_cache_negative_purge)
{
	const char FULLPATH[] = "mountpoint/some_file.txt";
	const char RELPATH[] = "some_file.txt";
	mode_t mode = 0755;
	uint64_t ino = 42;
	struct timespec entry_valid = {.tv_sec = TIME_T_MAX, .tv_nsec = 0};

	/* mkdir will first do a LOOKUP, adding a negative cache entry */
	EXPECT_LOOKUP(FUSE_ROOT_ID, RELPATH)
	.Times(1)
	.WillOnce(Invoke(ReturnNegativeCache(&entry_valid)))
	.RetiresOnSaturation();

	/* Then the MKDIR should purge the negative cache entry */
	EXPECT_CALL(*m_mock, process(
		ResultOf([=](auto in) {
			const char *name = (const char*)in.body.bytes +
				sizeof(fuse_open_in);
			return (in.header.opcode == FUSE_MKDIR &&
				in.body.mkdir.mode == (S_IFDIR | mode) &&
				(0 == strcmp(RELPATH, name)));
		}, Eq(true)),
		_)
	).WillOnce(Invoke(ReturnImmediate([=](auto in __unused, auto& out) {
		SET_OUT_HEADER_LEN(out, entry);
		out.body.entry.attr.mode = S_IFDIR | mode;
		out.body.entry.nodeid = ino;
		out.body.entry.attr_valid = UINT64_MAX;
	})));

	ASSERT_EQ(0, mkdir(FULLPATH, mode)) << strerror(errno);

	/* Finally, a subsequent lookup should query the daemon */
	expect_lookup(RELPATH, ino, S_IFDIR | mode, 0, 1);

	ASSERT_EQ(0, access(FULLPATH, F_OK)) << strerror(errno);
}

TEST_F(Mkdir, ok)
{
	const char FULLPATH[] = "mountpoint/some_dir";
	const char RELPATH[] = "some_dir";
	mode_t mode = 0755;
	uint64_t ino = 42;
	mode_t mask;

	mask = umask(0);
	(void)umask(mask);

	EXPECT_LOOKUP(FUSE_ROOT_ID, RELPATH)
	.WillOnce(Invoke(ReturnErrno(ENOENT)));

	EXPECT_CALL(*m_mock, process(
		ResultOf([=](auto in) {
			const char *name = (const char*)in.body.bytes +
				sizeof(fuse_mkdir_in);
			return (in.header.opcode == FUSE_MKDIR &&
				in.body.mkdir.mode == (S_IFDIR | mode) &&
				in.body.mkdir.umask == mask &&
				(0 == strcmp(RELPATH, name)));
		}, Eq(true)),
		_)
	).WillOnce(Invoke(ReturnImmediate([=](auto in __unused, auto& out) {
		SET_OUT_HEADER_LEN(out, entry);
		out.body.create.entry.attr.mode = S_IFDIR | mode;
		out.body.create.entry.nodeid = ino;
		out.body.create.entry.entry_valid = UINT64_MAX;
		out.body.create.entry.attr_valid = UINT64_MAX;
	})));

	ASSERT_EQ(0, mkdir(FULLPATH, mode)) << strerror(errno);
}

/*
 * Nothing bad should happen if the server returns the parent's inode number
 * for the newly created directory.  Regression test for bug 263662.
 * https://bugs.freebsd.org/bugzilla/show_bug.cgi?id=263662
 */
TEST_F(Mkdir, parent_inode)
{
	const char FULLPATH[] = "mountpoint/parent/some_dir";
	const char PPATH[] = "parent";
	const char RELPATH[] = "some_dir";
	mode_t mode = 0755;
	uint64_t ino = 42;
	mode_t mask;

	mask = umask(0);
	(void)umask(mask);

	expect_lookup(PPATH, ino, S_IFDIR | 0755, 0, 1);
	EXPECT_LOOKUP(ino, RELPATH)
	.WillOnce(Invoke(ReturnErrno(ENOENT)));

	EXPECT_CALL(*m_mock, process(
		ResultOf([=](auto in) {
			const char *name = (const char*)in.body.bytes +
				sizeof(fuse_mkdir_in);
			return (in.header.opcode == FUSE_MKDIR &&
				in.body.mkdir.mode == (S_IFDIR | mode) &&
				in.body.mkdir.umask == mask &&
				(0 == strcmp(RELPATH, name)));
		}, Eq(true)),
		_)
	).WillOnce(Invoke(ReturnImmediate([=](auto in __unused, auto& out) {
		SET_OUT_HEADER_LEN(out, entry);
		out.body.create.entry.attr.mode = S_IFDIR | mode;
		out.body.create.entry.nodeid = ino;
		out.body.create.entry.entry_valid = UINT64_MAX;
		out.body.create.entry.attr_valid = UINT64_MAX;
	})));
	// FUSE_FORGET happens asynchronously, so it may or may not arrive
	// before the test completes.
	EXPECT_CALL(*m_mock, process(
		ResultOf([=](auto in) {
			return (in.header.opcode == FUSE_FORGET);
		}, Eq(true)),
		_)
	).Times(AtMost(1))
	.WillOnce(Invoke([=](auto in __unused, auto &out __unused) { }));

	ASSERT_EQ(-1, mkdir(FULLPATH, mode));
	ASSERT_EQ(EIO, errno);
}

TEST_F(Mkdir_7_8, ok)
{
	const char FULLPATH[] = "mountpoint/some_dir";
	const char RELPATH[] = "some_dir";
	mode_t mode = 0755;
	uint64_t ino = 42;

	EXPECT_LOOKUP(FUSE_ROOT_ID, RELPATH)
	.WillOnce(Invoke(ReturnErrno(ENOENT)));

	EXPECT_CALL(*m_mock, process(
		ResultOf([=](auto in) {
			const char *name = (const char*)in.body.bytes +
				sizeof(fuse_mkdir_in);
			return (in.header.opcode == FUSE_MKDIR &&
				in.body.mkdir.mode == (S_IFDIR | mode) &&
				(0 == strcmp(RELPATH, name)));
		}, Eq(true)),
		_)
	).WillOnce(Invoke(ReturnImmediate([=](auto in __unused, auto& out) {
		SET_OUT_HEADER_LEN(out, entry_7_8);
		out.body.create.entry.attr.mode = S_IFDIR | mode;
		out.body.create.entry.nodeid = ino;
		out.body.create.entry.entry_valid = UINT64_MAX;
		out.body.create.entry.attr_valid = UINT64_MAX;
	})));

	ASSERT_EQ(0, mkdir(FULLPATH, mode)) << strerror(errno);
}
