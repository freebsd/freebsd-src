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
 *
 * $FreeBSD$
 */

extern "C" {
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/un.h>
}

#include "mockfs.hh"
#include "utils.hh"

using namespace testing;

#ifndef VNOVAL
#define VNOVAL (-1)	/* Defined in sys/vnode.h */
#endif

class Mknod: public FuseTest {

mode_t m_oldmask;
const static mode_t c_umask = 022;

public:

Mknod() {
	m_oldmask = umask(c_umask);
}

virtual void SetUp() {
	if (geteuid() != 0) {
		GTEST_SKIP() << "Only root may use most mknod(2) variations";
	}
	FuseTest::SetUp();
}

virtual void TearDown() {
	FuseTest::TearDown();
	(void)umask(m_oldmask);
}

/* Test an OK creation of a file with the given mode and device number */
void expect_mknod(uint64_t parent_ino, const char* relpath, uint64_t ino,
		mode_t mode, dev_t dev)
{
	EXPECT_CALL(*m_mock, process(
		ResultOf([=](auto in) {
			const char *name = (const char*)in.body.bytes +
				sizeof(fuse_mknod_in);
			return (in.header.nodeid == parent_ino &&
				in.header.opcode == FUSE_MKNOD &&
				in.body.mknod.mode == mode &&
				in.body.mknod.rdev == (uint32_t)dev &&
				in.body.mknod.umask == c_umask &&
				(0 == strcmp(relpath, name)));
		}, Eq(true)),
		_)
	).WillOnce(Invoke(ReturnImmediate([=](auto in __unused, auto& out) {
		SET_OUT_HEADER_LEN(out, entry);
		out.body.entry.attr.mode = mode;
		out.body.entry.nodeid = ino;
		out.body.entry.entry_valid = UINT64_MAX;
		out.body.entry.attr_valid = UINT64_MAX;
		out.body.entry.attr.rdev = dev;
	})));
}

};

class Mknod_7_11: public FuseTest {
public:
virtual void SetUp() {
	m_kernel_minor_version = 11;
	if (geteuid() != 0) {
		GTEST_SKIP() << "Only root may use most mknod(2) variations";
	}
	FuseTest::SetUp();
}

void expect_lookup(const char *relpath, uint64_t ino, uint64_t size)
{
	FuseTest::expect_lookup_7_8(relpath, ino, S_IFREG | 0644, size, 1);
}

/* Test an OK creation of a file with the given mode and device number */
void expect_mknod(uint64_t parent_ino, const char* relpath, uint64_t ino,
		mode_t mode, dev_t dev)
{
	EXPECT_CALL(*m_mock, process(
		ResultOf([=](auto in) {
			const char *name = (const char*)in.body.bytes +
				FUSE_COMPAT_MKNOD_IN_SIZE;
			return (in.header.nodeid == parent_ino &&
				in.header.opcode == FUSE_MKNOD &&
				in.body.mknod.mode == mode &&
				in.body.mknod.rdev == (uint32_t)dev &&
				(0 == strcmp(relpath, name)));
		}, Eq(true)),
		_)
	).WillOnce(Invoke(ReturnImmediate([=](auto in __unused, auto& out) {
		SET_OUT_HEADER_LEN(out, entry);
		out.body.entry.attr.mode = mode;
		out.body.entry.nodeid = ino;
		out.body.entry.entry_valid = UINT64_MAX;
		out.body.entry.attr_valid = UINT64_MAX;
		out.body.entry.attr.rdev = dev;
	})));
}

};

/* 
 * mknod(2) should be able to create block devices on a FUSE filesystem.  Even
 * though FreeBSD doesn't use block devices, this is useful when copying media
 * from or preparing media for other operating systems.
 */
TEST_F(Mknod, blk)
{
	const char FULLPATH[] = "mountpoint/some_node";
	const char RELPATH[] = "some_node";
	mode_t mode = S_IFBLK | 0755;
	dev_t rdev = 0xfe00; /* /dev/vda's device number on Linux */
	uint64_t ino = 42;

	EXPECT_LOOKUP(FUSE_ROOT_ID, RELPATH)
	.WillOnce(Invoke(ReturnErrno(ENOENT)));
	expect_mknod(FUSE_ROOT_ID, RELPATH, ino, mode, rdev);

	EXPECT_EQ(0, mknod(FULLPATH, mode, rdev)) << strerror(errno);
}

TEST_F(Mknod, chr)
{
	const char FULLPATH[] = "mountpoint/some_node";
	const char RELPATH[] = "some_node";
	mode_t mode = S_IFCHR | 0755;
	dev_t rdev = 54;			/* /dev/fuse's device number */
	uint64_t ino = 42;

	EXPECT_LOOKUP(FUSE_ROOT_ID, RELPATH)
	.WillOnce(Invoke(ReturnErrno(ENOENT)));
	expect_mknod(FUSE_ROOT_ID, RELPATH, ino, mode, rdev);

	EXPECT_EQ(0, mknod(FULLPATH, mode, rdev)) << strerror(errno);
}

/* 
 * The daemon is responsible for checking file permissions (unless the
 * default_permissions mount option was used)
 */
TEST_F(Mknod, eperm)
{
	const char FULLPATH[] = "mountpoint/some_node";
	const char RELPATH[] = "some_node";
	mode_t mode = S_IFIFO | 0755;

	EXPECT_LOOKUP(FUSE_ROOT_ID, RELPATH)
	.WillOnce(Invoke(ReturnErrno(ENOENT)));

	EXPECT_CALL(*m_mock, process(
		ResultOf([=](auto in) {
			const char *name = (const char*)in.body.bytes +
				sizeof(fuse_mknod_in);
			return (in.header.opcode == FUSE_MKNOD &&
				in.body.mknod.mode == mode &&
				(0 == strcmp(RELPATH, name)));
		}, Eq(true)),
		_)
	).WillOnce(Invoke(ReturnErrno(EPERM)));
	EXPECT_NE(0, mkfifo(FULLPATH, mode));
	EXPECT_EQ(EPERM, errno);
}

TEST_F(Mknod, fifo)
{
	const char FULLPATH[] = "mountpoint/some_node";
	const char RELPATH[] = "some_node";
	mode_t mode = S_IFIFO | 0755;
	dev_t rdev = VNOVAL;		/* Fifos don't have device numbers */
	uint64_t ino = 42;

	EXPECT_LOOKUP(FUSE_ROOT_ID, RELPATH)
	.WillOnce(Invoke(ReturnErrno(ENOENT)));
	expect_mknod(FUSE_ROOT_ID, RELPATH, ino, mode, rdev);

	EXPECT_EQ(0, mkfifo(FULLPATH, mode)) << strerror(errno);
}

/*
 * Create a unix-domain socket.
 *
 * This test case doesn't actually need root privileges.
 */
TEST_F(Mknod, socket)
{
	const char FULLPATH[] = "mountpoint/some_node";
	const char RELPATH[] = "some_node";
	mode_t mode = S_IFSOCK | 0755;
	struct sockaddr_un sa;
	int fd;
	dev_t rdev = -1;	/* Really it's a don't care */
	uint64_t ino = 42;

	EXPECT_LOOKUP(FUSE_ROOT_ID, RELPATH)
	.WillOnce(Invoke(ReturnErrno(ENOENT)));
	expect_mknod(FUSE_ROOT_ID, RELPATH, ino, mode, rdev);

	fd = socket(AF_UNIX, SOCK_STREAM, 0);
	ASSERT_LE(0, fd) << strerror(errno);
	sa.sun_family = AF_UNIX;
	strlcpy(sa.sun_path, FULLPATH, sizeof(sa.sun_path));
	sa.sun_len = sizeof(FULLPATH);
	ASSERT_EQ(0, bind(fd, (struct sockaddr*)&sa, sizeof(sa)))
		<< strerror(errno);

	leak(fd);
}

/*
 * Nothing bad should happen if the server returns the parent's inode number
 * for the newly created file.  Regression test for bug 263662.
 * https://bugs.freebsd.org/bugzilla/show_bug.cgi?id=263662
 */
TEST_F(Mknod, parent_inode)
{
	const char FULLPATH[] = "mountpoint/parent/some_node";
	const char PPATH[] = "parent";
	const char RELPATH[] = "some_node";
	mode_t mode = S_IFSOCK | 0755;
	struct sockaddr_un sa;
	int fd;
	dev_t rdev = -1;	/* Really it's a don't care */
	uint64_t ino = 42;

	expect_lookup(PPATH, ino, S_IFDIR | 0755, 0, 1);
	EXPECT_LOOKUP(ino, RELPATH)
	.WillOnce(Invoke(ReturnErrno(ENOENT)));
	expect_mknod(ino, RELPATH, ino, mode, rdev);

	fd = socket(AF_UNIX, SOCK_STREAM, 0);
	ASSERT_LE(0, fd) << strerror(errno);
	sa.sun_family = AF_UNIX;
	strlcpy(sa.sun_path, FULLPATH, sizeof(sa.sun_path));
	sa.sun_len = sizeof(FULLPATH);
	ASSERT_EQ(-1, bind(fd, (struct sockaddr*)&sa, sizeof(sa)));
	ASSERT_EQ(EIO, errno);

	leak(fd);
}

/* 
 * fusefs(5) lacks VOP_WHITEOUT support.  No bugzilla entry, because that's a
 * feature, not a bug
 */
TEST_F(Mknod, DISABLED_whiteout)
{
	const char FULLPATH[] = "mountpoint/some_node";
	const char RELPATH[] = "some_node";
	mode_t mode = S_IFWHT | 0755;
	dev_t rdev = VNOVAL;	/* whiteouts don't have device numbers */
	uint64_t ino = 42;

	EXPECT_LOOKUP(FUSE_ROOT_ID, RELPATH)
	.WillOnce(Invoke(ReturnErrno(ENOENT)));
	expect_mknod(FUSE_ROOT_ID, RELPATH, ino, mode, rdev);

	EXPECT_EQ(0, mknod(FULLPATH, mode, 0)) << strerror(errno);
}

/* A server built at protocol version 7.11 or earlier can still use mknod */
TEST_F(Mknod_7_11, fifo)
{
	const char FULLPATH[] = "mountpoint/some_node";
	const char RELPATH[] = "some_node";
	mode_t mode = S_IFIFO | 0755;
	dev_t rdev = VNOVAL;
	uint64_t ino = 42;

	EXPECT_LOOKUP(FUSE_ROOT_ID, RELPATH)
	.WillOnce(Invoke(ReturnErrno(ENOENT)));
	expect_mknod(FUSE_ROOT_ID, RELPATH, ino, mode, rdev);

	EXPECT_EQ(0, mkfifo(FULLPATH, mode)) << strerror(errno);
}
