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
}

#include "mockfs.hh"
#include "utils.hh"

using namespace testing;

class Create: public FuseTest {
public:

void expect_create(const char *relpath, mode_t mode, ProcessMockerT r)
{
	mode_t mask = umask(0);
	(void)umask(mask);

	EXPECT_CALL(*m_mock, process(
		ResultOf([=](auto in) {
			const char *name = (const char*)in.body.bytes +
				sizeof(fuse_create_in);
			return (in.header.opcode == FUSE_CREATE &&
				in.body.create.mode == mode &&
				in.body.create.umask == mask &&
				(0 == strcmp(relpath, name)));
		}, Eq(true)),
		_)
	).WillOnce(Invoke(r));
}

};

/* FUSE_CREATE operations for a protocol 7.8 server */
class Create_7_8: public Create {
public:
virtual void SetUp() {
	m_kernel_minor_version = 8;
	Create::SetUp();
}

void expect_create(const char *relpath, mode_t mode, ProcessMockerT r)
{
	EXPECT_CALL(*m_mock, process(
		ResultOf([=](auto in) {
			const char *name = (const char*)in.body.bytes +
				sizeof(fuse_open_in);
			return (in.header.opcode == FUSE_CREATE &&
				in.body.create.mode == mode &&
				(0 == strcmp(relpath, name)));
		}, Eq(true)),
		_)
	).WillOnce(Invoke(r));
}

};

/* FUSE_CREATE operations for a server built at protocol <= 7.11 */
class Create_7_11: public FuseTest {
public:
virtual void SetUp() {
	m_kernel_minor_version = 11;
	FuseTest::SetUp();
}

void expect_create(const char *relpath, mode_t mode, ProcessMockerT r)
{
	EXPECT_CALL(*m_mock, process(
		ResultOf([=](auto in) {
			const char *name = (const char*)in.body.bytes +
				sizeof(fuse_open_in);
			return (in.header.opcode == FUSE_CREATE &&
				in.body.create.mode == mode &&
				(0 == strcmp(relpath, name)));
		}, Eq(true)),
		_)
	).WillOnce(Invoke(r));
}

};


/*
 * If FUSE_CREATE sets attr_valid, then subsequent GETATTRs should use the
 * attribute cache
 */
TEST_F(Create, attr_cache)
{
	const char FULLPATH[] = "mountpoint/some_file.txt";
	const char RELPATH[] = "some_file.txt";
	mode_t mode = S_IFREG | 0755;
	uint64_t ino = 42;
	int fd;

	EXPECT_LOOKUP(FUSE_ROOT_ID, RELPATH)
		.WillOnce(Invoke(ReturnErrno(ENOENT)));
	expect_create(RELPATH, mode,
		ReturnImmediate([=](auto in __unused, auto& out) {
		SET_OUT_HEADER_LEN(out, create);
		out.body.create.entry.attr.mode = mode;
		out.body.create.entry.nodeid = ino;
		out.body.create.entry.entry_valid = UINT64_MAX;
		out.body.create.entry.attr_valid = UINT64_MAX;
	}));

	EXPECT_CALL(*m_mock, process(
		ResultOf([=](auto in) {
			return (in.header.opcode == FUSE_GETATTR &&
				in.header.nodeid == ino);
		}, Eq(true)),
		_)
	).Times(0);

	fd = open(FULLPATH, O_CREAT | O_EXCL, mode);
	ASSERT_LE(0, fd) << strerror(errno);
	leak(fd);
}

/* A successful CREATE operation should purge the parent dir's attr cache */
TEST_F(Create, clear_attr_cache)
{
	const char FULLPATH[] = "mountpoint/src";
	const char RELPATH[] = "src";
	mode_t mode = S_IFREG | 0755;
	uint64_t ino = 42;
	int fd;
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

	expect_create(RELPATH, mode,
		ReturnImmediate([=](auto in __unused, auto& out) {
		SET_OUT_HEADER_LEN(out, create);
		out.body.create.entry.attr.mode = mode;
		out.body.create.entry.nodeid = ino;
		out.body.create.entry.entry_valid = UINT64_MAX;
		out.body.create.entry.attr_valid = UINT64_MAX;
	}));

	EXPECT_EQ(0, stat("mountpoint", &sb)) << strerror(errno);
	fd = open(FULLPATH, O_CREAT | O_EXCL, mode);
	ASSERT_LE(0, fd) << strerror(errno);
	EXPECT_EQ(0, stat("mountpoint", &sb)) << strerror(errno);

	leak(fd);
}

/* 
 * The fuse daemon fails the request with EEXIST.  This usually indicates a
 * race condition: some other FUSE client created the file in between when the
 * kernel checked for it with lookup and tried to create it with create
 */
TEST_F(Create, eexist)
{
	const char FULLPATH[] = "mountpoint/some_file.txt";
	const char RELPATH[] = "some_file.txt";
	mode_t mode = S_IFREG | 0755;

	EXPECT_LOOKUP(FUSE_ROOT_ID, RELPATH)
		.WillOnce(Invoke(ReturnErrno(ENOENT)));
	expect_create(RELPATH, mode, ReturnErrno(EEXIST));
	EXPECT_EQ(-1, open(FULLPATH, O_CREAT | O_EXCL, mode));
	EXPECT_EQ(EEXIST, errno);
}

/*
 * If the daemon doesn't implement FUSE_CREATE, then the kernel should fallback
 * to FUSE_MKNOD/FUSE_OPEN
 */
TEST_F(Create, Enosys)
{
	const char FULLPATH[] = "mountpoint/some_file.txt";
	const char RELPATH[] = "some_file.txt";
	mode_t mode = S_IFREG | 0755;
	uint64_t ino = 42;
	int fd;

	EXPECT_LOOKUP(FUSE_ROOT_ID, RELPATH)
		.WillOnce(Invoke(ReturnErrno(ENOENT)));
	expect_create(RELPATH, mode, ReturnErrno(ENOSYS));

	EXPECT_CALL(*m_mock, process(
		ResultOf([=](auto in) {
			const char *name = (const char*)in.body.bytes +
				sizeof(fuse_mknod_in);
			return (in.header.opcode == FUSE_MKNOD &&
				in.body.mknod.mode == (S_IFREG | mode) &&
				in.body.mknod.rdev == 0 &&
				(0 == strcmp(RELPATH, name)));
		}, Eq(true)),
		_)
	).WillOnce(Invoke(ReturnImmediate([=](auto in __unused, auto& out) {
		SET_OUT_HEADER_LEN(out, entry);
		out.body.entry.attr.mode = mode;
		out.body.entry.nodeid = ino;
		out.body.entry.entry_valid = UINT64_MAX;
		out.body.entry.attr_valid = UINT64_MAX;
	})));

	EXPECT_CALL(*m_mock, process(
		ResultOf([=](auto in) {
			return (in.header.opcode == FUSE_OPEN &&
				in.header.nodeid == ino);
		}, Eq(true)),
		_)
	).WillOnce(Invoke(ReturnImmediate([](auto in __unused, auto& out) {
		out.header.len = sizeof(out.header);
		SET_OUT_HEADER_LEN(out, open);
	})));

	fd = open(FULLPATH, O_CREAT | O_EXCL, mode);
	ASSERT_LE(0, fd) << strerror(errno);
	leak(fd);
}

/*
 * Creating a new file after FUSE_LOOKUP returned a negative cache entry
 */
TEST_F(Create, entry_cache_negative)
{
	const char FULLPATH[] = "mountpoint/some_file.txt";
	const char RELPATH[] = "some_file.txt";
	mode_t mode = S_IFREG | 0755;
	uint64_t ino = 42;
	int fd;
	/* 
	 * Set entry_valid = 0 because this test isn't concerned with whether
	 * or not we actually cache negative entries, only with whether we
	 * interpret negative cache responses correctly.
	 */
	struct timespec entry_valid = {.tv_sec = 0, .tv_nsec = 0};

	/* create will first do a LOOKUP, adding a negative cache entry */
	EXPECT_LOOKUP(FUSE_ROOT_ID, RELPATH)
		.WillOnce(ReturnNegativeCache(&entry_valid));
	expect_create(RELPATH, mode,
		ReturnImmediate([=](auto in __unused, auto& out) {
		SET_OUT_HEADER_LEN(out, create);
		out.body.create.entry.attr.mode = mode;
		out.body.create.entry.nodeid = ino;
		out.body.create.entry.entry_valid = UINT64_MAX;
		out.body.create.entry.attr_valid = UINT64_MAX;
	}));

	fd = open(FULLPATH, O_CREAT | O_EXCL, mode);
	ASSERT_LE(0, fd) << strerror(errno);
	leak(fd);
}

/*
 * Creating a new file should purge any negative namecache entries
 */
TEST_F(Create, entry_cache_negative_purge)
{
	const char FULLPATH[] = "mountpoint/some_file.txt";
	const char RELPATH[] = "some_file.txt";
	mode_t mode = S_IFREG | 0755;
	uint64_t ino = 42;
	int fd;
	struct timespec entry_valid = {.tv_sec = TIME_T_MAX, .tv_nsec = 0};

	/* create will first do a LOOKUP, adding a negative cache entry */
	EXPECT_LOOKUP(FUSE_ROOT_ID, RELPATH).Times(1)
		.WillOnce(Invoke(ReturnNegativeCache(&entry_valid)))
	.RetiresOnSaturation();

	/* Then the CREATE should purge the negative cache entry */
	expect_create(RELPATH, mode,
		ReturnImmediate([=](auto in __unused, auto& out) {
		SET_OUT_HEADER_LEN(out, create);
		out.body.create.entry.attr.mode = mode;
		out.body.create.entry.nodeid = ino;
		out.body.create.entry.attr_valid = UINT64_MAX;
	}));

	fd = open(FULLPATH, O_CREAT | O_EXCL, mode);
	ASSERT_LE(0, fd) << strerror(errno);

	/* Finally, a subsequent lookup should query the daemon */
	expect_lookup(RELPATH, ino, S_IFREG | mode, 0, 1);

	ASSERT_EQ(0, access(FULLPATH, F_OK)) << strerror(errno);
	leak(fd);
}

/* 
 * The daemon is responsible for checking file permissions (unless the
 * default_permissions mount option was used)
 */
TEST_F(Create, eperm)
{
	const char FULLPATH[] = "mountpoint/some_file.txt";
	const char RELPATH[] = "some_file.txt";
	mode_t mode = S_IFREG | 0755;

	EXPECT_LOOKUP(FUSE_ROOT_ID, RELPATH)
		.WillOnce(Invoke(ReturnErrno(ENOENT)));
	expect_create(RELPATH, mode, ReturnErrno(EPERM));

	EXPECT_EQ(-1, open(FULLPATH, O_CREAT | O_EXCL, mode));
	EXPECT_EQ(EPERM, errno);
}

TEST_F(Create, ok)
{
	const char FULLPATH[] = "mountpoint/some_file.txt";
	const char RELPATH[] = "some_file.txt";
	mode_t mode = S_IFREG | 0755;
	uint64_t ino = 42;
	int fd;

	EXPECT_LOOKUP(FUSE_ROOT_ID, RELPATH)
		.WillOnce(Invoke(ReturnErrno(ENOENT)));
	expect_create(RELPATH, mode,
		ReturnImmediate([=](auto in __unused, auto& out) {
		SET_OUT_HEADER_LEN(out, create);
		out.body.create.entry.attr.mode = mode;
		out.body.create.entry.nodeid = ino;
		out.body.create.entry.entry_valid = UINT64_MAX;
		out.body.create.entry.attr_valid = UINT64_MAX;
	}));

	fd = open(FULLPATH, O_CREAT | O_EXCL, mode);
	ASSERT_LE(0, fd) << strerror(errno);
	leak(fd);
}

/*
 * Nothing bad should happen if the server returns the parent's inode number
 * for the newly created file.  Regression test for bug 263662
 * https://bugs.freebsd.org/bugzilla/show_bug.cgi?id=263662
 */
TEST_F(Create, parent_inode)
{
	const char FULLPATH[] = "mountpoint/some_dir/some_file.txt";
	const char RELDIRPATH[] = "some_dir";
	const char RELPATH[] = "some_file.txt";
	mode_t mode = 0755;
	uint64_t ino = 42;
	int fd;

	expect_lookup(RELDIRPATH, ino, S_IFDIR | mode, 0, 1);
	EXPECT_LOOKUP(ino, RELPATH)
		.WillOnce(Invoke(ReturnErrno(ENOENT)));
	expect_create(RELPATH, S_IFREG | mode,
		ReturnImmediate([=](auto in __unused, auto& out) {
		SET_OUT_HEADER_LEN(out, create);
		out.body.create.entry.attr.mode = S_IFREG | mode;
		/* Return the same inode as the parent dir */
		out.body.create.entry.nodeid = ino;
		out.body.create.entry.entry_valid = UINT64_MAX;
		out.body.create.entry.attr_valid = UINT64_MAX;
	}));
	// FUSE_RELEASE happens asynchronously, so it may or may not arrive
	// before the test completes.
	EXPECT_CALL(*m_mock, process(
		ResultOf([=](auto in) {
			return (in.header.opcode == FUSE_RELEASE);
		}, Eq(true)),
		_)
	).Times(AtMost(1))
	.WillOnce(Invoke([=](auto in __unused, auto &out __unused) { }));

	fd = open(FULLPATH, O_CREAT | O_EXCL, mode);
	ASSERT_EQ(-1, fd);
	EXPECT_EQ(EIO, errno);
}

/*
 * A regression test for a bug that affected old FUSE implementations:
 * open(..., O_WRONLY | O_CREAT, 0444) should work despite the seeming
 * contradiction between O_WRONLY and 0444
 *
 * For example:
 * https://bugs.launchpad.net/ubuntu/+source/sshfs-fuse/+bug/44886
 */
TEST_F(Create, wronly_0444)
{
	const char FULLPATH[] = "mountpoint/some_file.txt";
	const char RELPATH[] = "some_file.txt";
	mode_t mode = S_IFREG | 0444;
	uint64_t ino = 42;
	int fd;

	EXPECT_LOOKUP(FUSE_ROOT_ID, RELPATH)
		.WillOnce(Invoke(ReturnErrno(ENOENT)));
	expect_create(RELPATH, mode,
		ReturnImmediate([=](auto in __unused, auto& out) {
		SET_OUT_HEADER_LEN(out, create);
		out.body.create.entry.attr.mode = mode;
		out.body.create.entry.nodeid = ino;
		out.body.create.entry.entry_valid = UINT64_MAX;
		out.body.create.entry.attr_valid = UINT64_MAX;
	}));

	fd = open(FULLPATH, O_CREAT | O_WRONLY, mode);
	ASSERT_LE(0, fd) << strerror(errno);
	leak(fd);
}

TEST_F(Create_7_8, ok)
{
	const char FULLPATH[] = "mountpoint/some_file.txt";
	const char RELPATH[] = "some_file.txt";
	mode_t mode = S_IFREG | 0755;
	uint64_t ino = 42;
	int fd;

	EXPECT_LOOKUP(FUSE_ROOT_ID, RELPATH)
		.WillOnce(Invoke(ReturnErrno(ENOENT)));
	expect_create(RELPATH, mode,
		ReturnImmediate([=](auto in __unused, auto& out) {
		SET_OUT_HEADER_LEN(out, create_7_8);
		out.body.create_7_8.entry.attr.mode = mode;
		out.body.create_7_8.entry.nodeid = ino;
		out.body.create_7_8.entry.entry_valid = UINT64_MAX;
		out.body.create_7_8.entry.attr_valid = UINT64_MAX;
		out.body.create_7_8.open.fh = FH;
	}));
	expect_flush(ino, 1, ReturnErrno(0));
	expect_release(ino, FH);

	fd = open(FULLPATH, O_CREAT | O_EXCL, mode);
	ASSERT_LE(0, fd) << strerror(errno);
	close(fd);
}

TEST_F(Create_7_11, ok)
{
	const char FULLPATH[] = "mountpoint/some_file.txt";
	const char RELPATH[] = "some_file.txt";
	mode_t mode = S_IFREG | 0755;
	uint64_t ino = 42;
	int fd;

	EXPECT_LOOKUP(FUSE_ROOT_ID, RELPATH)
		.WillOnce(Invoke(ReturnErrno(ENOENT)));
	expect_create(RELPATH, mode,
		ReturnImmediate([=](auto in __unused, auto& out) {
		SET_OUT_HEADER_LEN(out, create);
		out.body.create.entry.attr.mode = mode;
		out.body.create.entry.nodeid = ino;
		out.body.create.entry.entry_valid = UINT64_MAX;
		out.body.create.entry.attr_valid = UINT64_MAX;
	}));

	fd = open(FULLPATH, O_CREAT | O_EXCL, mode);
	ASSERT_LE(0, fd) << strerror(errno);
	leak(fd);
}
