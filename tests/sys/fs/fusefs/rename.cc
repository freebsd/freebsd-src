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
#include <stdlib.h>
#include <unistd.h>
}

#include "mockfs.hh"
#include "utils.hh"

using namespace testing;

class Rename: public FuseTest {
	public:
	int tmpfd = -1;
	char tmpfile[80] = "/tmp/fuse.rename.XXXXXX";

	virtual void TearDown() {
		if (tmpfd >= 0) {
			close(tmpfd);
			unlink(tmpfile);
		}

		FuseTest::TearDown();
	}
};

// EINVAL, dst is subdir of src
TEST_F(Rename, einval)
{
	const char FULLDST[] = "mountpoint/src/dst";
	const char RELDST[] = "dst";
	const char FULLSRC[] = "mountpoint/src";
	const char RELSRC[] = "src";
	uint64_t src_ino = 42;

	expect_lookup(RELSRC, src_ino, S_IFDIR | 0755, 0, 2);
	EXPECT_LOOKUP(src_ino, RELDST).WillOnce(Invoke(ReturnErrno(ENOENT)));

	ASSERT_NE(0, rename(FULLSRC, FULLDST));
	ASSERT_EQ(EINVAL, errno);
}

// source does not exist
TEST_F(Rename, enoent)
{
	const char FULLDST[] = "mountpoint/dst";
	const char FULLSRC[] = "mountpoint/src";
	const char RELSRC[] = "src";
	// FUSE hardcodes the mountpoint to inode 1

	EXPECT_LOOKUP(FUSE_ROOT_ID, RELSRC)
	.WillOnce(Invoke(ReturnErrno(ENOENT)));

	ASSERT_NE(0, rename(FULLSRC, FULLDST));
	ASSERT_EQ(ENOENT, errno);
}

/*
 * Renaming a file after FUSE_LOOKUP returned a negative cache entry for dst
 */
TEST_F(Rename, entry_cache_negative)
{
	const char FULLDST[] = "mountpoint/dst";
	const char RELDST[] = "dst";
	const char FULLSRC[] = "mountpoint/src";
	const char RELSRC[] = "src";
	uint64_t dst_dir_ino = FUSE_ROOT_ID;
	uint64_t ino = 42;
	/* 
	 * Set entry_valid = 0 because this test isn't concerned with whether
	 * or not we actually cache negative entries, only with whether we
	 * interpret negative cache responses correctly.
	 */
	struct timespec entry_valid = {.tv_sec = 0, .tv_nsec = 0};

	expect_lookup(RELSRC, ino, S_IFREG | 0644, 0, 1);
	/* LOOKUP returns a negative cache entry for dst */
	EXPECT_LOOKUP(FUSE_ROOT_ID, RELDST)
	.WillOnce(ReturnNegativeCache(&entry_valid));

	EXPECT_CALL(*m_mock, process(
		ResultOf([=](auto in) {
			const char *src = (const char*)in.body.bytes +
				sizeof(fuse_rename_in);
			const char *dst = src + strlen(src) + 1;
			return (in.header.opcode == FUSE_RENAME &&
				in.body.rename.newdir == dst_dir_ino &&
				(0 == strcmp(RELDST, dst)) &&
				(0 == strcmp(RELSRC, src)));
		}, Eq(true)),
		_)
	).WillOnce(Invoke(ReturnErrno(0)));

	ASSERT_EQ(0, rename(FULLSRC, FULLDST)) << strerror(errno);
}

/*
 * Renaming a file should purge any negative namecache entries for the dst
 */
TEST_F(Rename, entry_cache_negative_purge)
{
	const char FULLDST[] = "mountpoint/dst";
	const char RELDST[] = "dst";
	const char FULLSRC[] = "mountpoint/src";
	const char RELSRC[] = "src";
	uint64_t dst_dir_ino = FUSE_ROOT_ID;
	uint64_t ino = 42;
	struct timespec entry_valid = {.tv_sec = TIME_T_MAX, .tv_nsec = 0};

	expect_lookup(RELSRC, ino, S_IFREG | 0644, 0, 1);
	/* LOOKUP returns a negative cache entry for dst */
	EXPECT_LOOKUP(FUSE_ROOT_ID, RELDST)
	.WillOnce(ReturnNegativeCache(&entry_valid))
	.RetiresOnSaturation();

	EXPECT_CALL(*m_mock, process(
		ResultOf([=](auto in) {
			const char *src = (const char*)in.body.bytes +
				sizeof(fuse_rename_in);
			const char *dst = src + strlen(src) + 1;
			return (in.header.opcode == FUSE_RENAME &&
				in.body.rename.newdir == dst_dir_ino &&
				(0 == strcmp(RELDST, dst)) &&
				(0 == strcmp(RELSRC, src)));
		}, Eq(true)),
		_)
	).WillOnce(Invoke(ReturnErrno(0)));

	ASSERT_EQ(0, rename(FULLSRC, FULLDST)) << strerror(errno);

	/* Finally, a subsequent lookup should query the daemon */
	expect_lookup(RELDST, ino, S_IFREG | 0644, 0, 1);

	ASSERT_EQ(0, access(FULLDST, F_OK)) << strerror(errno);
}

static volatile int stopit = 0;

static void* setattr_th(void* arg) {
	char *path = (char*)arg;

	while (stopit == 0)
		 chmod(path, 0777);
	return 0;
}

/*
 * Rename restarts the syscall to avoid a LOR
 *
 * This test triggers a race: the chmod() calls VOP_SETATTR, which locks its
 * vnode, but fuse_vnop_rename also tries to lock the same vnode.  The result
 * is that, in order to avoid a LOR, fuse_vnop_rename returns ERELOOKUP to
 * restart the syscall.
 *
 * To verify that the race is hit, watch the fusefs:fusefs:vnops:erelookup
 * dtrace probe while the test is running.  On my system, that probe fires more
 * than 100 times per second during this test.
 */
TEST_F(Rename, erelookup)
{
	const char FULLDST[] = "mountpoint/dstdir/dst";
	const char RELDSTDIR[] = "dstdir";
	const char RELDST[] = "dst";
	const char FULLSRC[] = "mountpoint/src";
	const char RELSRC[] = "src";
	pthread_t th0;
	uint64_t ino = 42;
	uint64_t dst_dir_ino = 43;
	uint32_t mode = S_IFDIR | 0644;
	struct timespec now, timeout;

	EXPECT_LOOKUP(FUSE_ROOT_ID, RELSRC)
	.WillRepeatedly(Invoke(
		ReturnImmediate([=](auto i __unused, auto& out) {
			SET_OUT_HEADER_LEN(out, entry);
			out.body.entry.attr.mode = mode;
			out.body.entry.nodeid = ino;
			out.body.entry.attr.nlink = 2;
			out.body.entry.attr_valid = UINT64_MAX;
			out.body.entry.attr.uid = 0;
			out.body.entry.attr.gid = 0;
			out.body.entry.entry_valid = UINT64_MAX;
		}))
	);
	EXPECT_LOOKUP(FUSE_ROOT_ID, RELDSTDIR)
	.WillRepeatedly(Invoke(ReturnImmediate([=](auto in __unused, auto& out)
	{
		SET_OUT_HEADER_LEN(out, entry);
		out.body.entry.nodeid = dst_dir_ino;
		out.body.entry.entry_valid = UINT64_MAX;
		out.body.entry.attr_valid = UINT64_MAX;
		out.body.entry.attr.mode = S_IFDIR | 0755;
		out.body.entry.attr.ino = dst_dir_ino;
		out.body.entry.attr.uid = geteuid();
		out.body.entry.attr.gid = getegid();
	})));
	EXPECT_LOOKUP(dst_dir_ino, RELDST)
	.WillRepeatedly(Invoke(ReturnErrno(ENOENT)));
	EXPECT_CALL(*m_mock, process(
		ResultOf([=](auto in) {
			return (in.header.opcode == FUSE_SETATTR &&
				in.header.nodeid == ino);
		}, Eq(true)),
		_)
	).WillRepeatedly(Invoke(ReturnErrno(EIO)));
	EXPECT_CALL(*m_mock, process(
		ResultOf([=](auto in) {
			return (in.header.opcode == FUSE_RENAME);
		}, Eq(true)),
		_)
	).WillRepeatedly(Invoke(ReturnErrno(EIO)));

	ASSERT_EQ(0, pthread_create(&th0, NULL, setattr_th, (void*)FULLSRC))
		<< strerror(errno);

	ASSERT_EQ(0, clock_gettime(CLOCK_MONOTONIC, &timeout));
	timeout.tv_nsec += NAP_NS;
	do {
		ASSERT_EQ(-1, rename(FULLSRC, FULLDST));
		EXPECT_EQ(EIO, errno);
		ASSERT_EQ(0, clock_gettime(CLOCK_MONOTONIC, &now));
	} while (timespeccmp(&now, &timeout, <));
	stopit = 1;
	pthread_join(th0, NULL);
}

TEST_F(Rename, exdev)
{
	const char FULLB[] = "mountpoint/src";
	const char RELB[] = "src";
	// FUSE hardcodes the mountpoint to inode 1
	uint64_t b_ino = 42;

	tmpfd = mkstemp(tmpfile);
	ASSERT_LE(0, tmpfd) << strerror(errno);

	expect_lookup(RELB, b_ino, S_IFREG | 0644, 0, 2);

	ASSERT_NE(0, rename(tmpfile, FULLB));
	ASSERT_EQ(EXDEV, errno);

	ASSERT_NE(0, rename(FULLB, tmpfile));
	ASSERT_EQ(EXDEV, errno);
}

TEST_F(Rename, ok)
{
	const char FULLDST[] = "mountpoint/dst";
	const char RELDST[] = "dst";
	const char FULLSRC[] = "mountpoint/src";
	const char RELSRC[] = "src";
	uint64_t dst_dir_ino = FUSE_ROOT_ID;
	uint64_t ino = 42;

	expect_lookup(RELSRC, ino, S_IFREG | 0644, 0, 1);
	EXPECT_LOOKUP(FUSE_ROOT_ID, RELDST)
	.WillOnce(Invoke(ReturnErrno(ENOENT)));

	EXPECT_CALL(*m_mock, process(
		ResultOf([=](auto in) {
			const char *src = (const char*)in.body.bytes +
				sizeof(fuse_rename_in);
			const char *dst = src + strlen(src) + 1;
			return (in.header.opcode == FUSE_RENAME &&
				in.body.rename.newdir == dst_dir_ino &&
				(0 == strcmp(RELDST, dst)) &&
				(0 == strcmp(RELSRC, src)));
		}, Eq(true)),
		_)
	).WillOnce(Invoke(ReturnErrno(0)));

	ASSERT_EQ(0, rename(FULLSRC, FULLDST)) << strerror(errno);
}

/* When moving a file to a new directory, update its parent */
TEST_F(Rename, parent)
{
	const char FULLDST[] = "mountpoint/dstdir/dst";
	const char RELDSTDIR[] = "dstdir";
	const char RELDST[] = "dst";
	const char FULLSRC[] = "mountpoint/src";
	const char RELSRC[] = "src";
	const char FULLDSTPARENT[] = "mountpoint/dstdir";
	const char FULLDSTDOTDOT[] = "mountpoint/dstdir/dst/..";
	Sequence seq;
	uint64_t dst_dir_ino = 43;
	uint64_t ino = 42;
	struct stat sb;

	expect_lookup(RELSRC, ino, S_IFDIR | 0755, 0, 1);
	EXPECT_LOOKUP(FUSE_ROOT_ID, RELDSTDIR)
	.WillOnce(Invoke(ReturnImmediate([=](auto in __unused, auto& out) {
		SET_OUT_HEADER_LEN(out, entry);
		out.body.entry.nodeid = dst_dir_ino;
		out.body.entry.entry_valid = UINT64_MAX;
		out.body.entry.attr_valid = UINT64_MAX;
		out.body.entry.attr.mode = S_IFDIR | 0755;
		out.body.entry.attr.ino = dst_dir_ino;
		out.body.entry.attr.nlink = 2;
	})));
	EXPECT_LOOKUP(dst_dir_ino, RELDST)
	.InSequence(seq)
	.WillOnce(Invoke(ReturnErrno(ENOENT)));
	EXPECT_CALL(*m_mock, process(
		ResultOf([=](auto in) {
			const char *src = (const char*)in.body.bytes +
				sizeof(fuse_rename_in);
			const char *dst = src + strlen(src) + 1;
			return (in.header.opcode == FUSE_RENAME &&
				in.body.rename.newdir == dst_dir_ino &&
				(0 == strcmp(RELDST, dst)) &&
				(0 == strcmp(RELSRC, src)));
		}, Eq(true)),
		_)
	).WillOnce(Invoke(ReturnErrno(0)));
	EXPECT_CALL(*m_mock, process(
		ResultOf([](auto in) {
			return (in.header.opcode == FUSE_GETATTR &&
				in.header.nodeid == 1);
		}, Eq(true)),
		_)
	).InSequence(seq)
	.WillOnce(Invoke(ReturnImmediate([=](auto i __unused, auto& out) {
		SET_OUT_HEADER_LEN(out, attr);
		out.body.attr.attr_valid = UINT64_MAX;
		out.body.attr.attr.ino = 1;
		out.body.attr.attr.mode = S_IFDIR | 0755;
		out.body.attr.attr.nlink = 2;
	})));
	EXPECT_LOOKUP(FUSE_ROOT_ID, RELDSTDIR)
	.InSequence(seq)
	.WillOnce(Invoke(ReturnImmediate([=](auto in __unused, auto& out) {
		SET_OUT_HEADER_LEN(out, entry);
		out.body.entry.nodeid = dst_dir_ino;
		out.body.entry.entry_valid = UINT64_MAX;
		out.body.entry.attr_valid = UINT64_MAX;
		out.body.entry.attr.mode = S_IFDIR | 0755;
		out.body.entry.attr.ino = dst_dir_ino;
		out.body.entry.attr.nlink = 3;
	})));
	EXPECT_LOOKUP(dst_dir_ino, RELDST)
	.InSequence(seq)
	.WillOnce(Invoke(ReturnImmediate([=](auto in __unused, auto& out) {
		SET_OUT_HEADER_LEN(out, entry);
		out.body.entry.attr.mode = S_IFDIR | 0755;
		out.body.entry.nodeid = ino;
		out.body.entry.entry_valid = UINT64_MAX;
		out.body.entry.attr_valid = UINT64_MAX;
	})));

	ASSERT_EQ(0, rename(FULLSRC, FULLDST)) << strerror(errno);

	ASSERT_EQ(0, stat("mountpoint", &sb)) << strerror(errno);
	EXPECT_EQ(2ul, sb.st_nlink);

	ASSERT_EQ(0, stat(FULLDSTPARENT, &sb)) << strerror(errno);
	EXPECT_EQ(3ul, sb.st_nlink);

	ASSERT_EQ(0, stat(FULLDSTDOTDOT, &sb)) << strerror(errno);
	ASSERT_EQ(dst_dir_ino, sb.st_ino);
}

// Rename overwrites an existing destination file
TEST_F(Rename, overwrite)
{
	const char FULLDST[] = "mountpoint/dst";
	const char RELDST[] = "dst";
	const char FULLSRC[] = "mountpoint/src";
	const char RELSRC[] = "src";
	// The inode of the already-existing destination file
	uint64_t dst_ino = 2;
	uint64_t dst_dir_ino = FUSE_ROOT_ID;
	uint64_t ino = 42;

	expect_lookup(RELSRC, ino, S_IFREG | 0644, 0, 1);
	expect_lookup(RELDST, dst_ino, S_IFREG | 0644, 0, 1);
	EXPECT_CALL(*m_mock, process(
		ResultOf([=](auto in) {
			const char *src = (const char*)in.body.bytes +
				sizeof(fuse_rename_in);
			const char *dst = src + strlen(src) + 1;
			return (in.header.opcode == FUSE_RENAME &&
				in.body.rename.newdir == dst_dir_ino &&
				(0 == strcmp(RELDST, dst)) &&
				(0 == strcmp(RELSRC, src)));
		}, Eq(true)),
		_)
	).WillOnce(Invoke(ReturnErrno(0)));

	ASSERT_EQ(0, rename(FULLSRC, FULLDST)) << strerror(errno);
}
