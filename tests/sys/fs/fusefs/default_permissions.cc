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

/*
 * Tests for the "default_permissions" mount option.  They must be in their own
 * file so they can be run as an unprivileged user
 */

extern "C" {
#include <sys/types.h>
#include <sys/extattr.h>

#include <fcntl.h>
#include <semaphore.h>
#include <unistd.h>
}

#include "mockfs.hh"
#include "utils.hh"

using namespace testing;

class DefaultPermissions: public FuseTest {

virtual void SetUp() {
	m_default_permissions = true;
	FuseTest::SetUp();
	if (HasFatalFailure() || IsSkipped())
		return;

	if (geteuid() == 0) {
		GTEST_SKIP() << "This test requires an unprivileged user";
	}
	
	/* With -o default_permissions, FUSE_ACCESS should never be called */
	EXPECT_CALL(*m_mock, process(
		ResultOf([=](auto in) {
			return (in.header.opcode == FUSE_ACCESS);
		}, Eq(true)),
		_)
	).Times(0);
}

public:
void expect_chmod(uint64_t ino, mode_t mode, uint64_t size = 0)
{
	EXPECT_CALL(*m_mock, process(
		ResultOf([=](auto in) {
			return (in.header.opcode == FUSE_SETATTR &&
				in.header.nodeid == ino &&
				in.body.setattr.valid == FATTR_MODE &&
				in.body.setattr.mode == mode);
		}, Eq(true)),
		_)
	).WillOnce(Invoke(ReturnImmediate([=](auto in __unused, auto& out) {
		SET_OUT_HEADER_LEN(out, attr);
		out.body.attr.attr.ino = ino;	// Must match nodeid
		out.body.attr.attr.mode = S_IFREG | mode;
		out.body.attr.attr.size = size;
		out.body.attr.attr_valid = UINT64_MAX;
	})));
}

void expect_create(const char *relpath, uint64_t ino)
{
	EXPECT_CALL(*m_mock, process(
		ResultOf([=](auto in) {
			const char *name = (const char*)in.body.bytes +
				sizeof(fuse_create_in);
			return (in.header.opcode == FUSE_CREATE &&
				(0 == strcmp(relpath, name)));
		}, Eq(true)),
		_)
	).WillOnce(Invoke(ReturnImmediate([=](auto in __unused, auto& out) {
		SET_OUT_HEADER_LEN(out, create);
		out.body.create.entry.attr.mode = S_IFREG | 0644;
		out.body.create.entry.nodeid = ino;
		out.body.create.entry.entry_valid = UINT64_MAX;
		out.body.create.entry.attr_valid = UINT64_MAX;
	})));
}

void expect_copy_file_range(uint64_t ino_in, uint64_t off_in, uint64_t ino_out,
    uint64_t off_out, uint64_t len)
{
	EXPECT_CALL(*m_mock, process(
		ResultOf([=](auto in) {
			return (in.header.opcode == FUSE_COPY_FILE_RANGE &&
				in.header.nodeid == ino_in &&
				in.body.copy_file_range.off_in == off_in &&
				in.body.copy_file_range.nodeid_out == ino_out &&
				in.body.copy_file_range.off_out == off_out &&
				in.body.copy_file_range.len == len);
		}, Eq(true)),
		_)
	).WillOnce(Invoke(ReturnImmediate([=](auto in __unused, auto& out) {
		SET_OUT_HEADER_LEN(out, write);
		out.body.write.size = len;
	})));
}

void expect_getattr(uint64_t ino, mode_t mode, uint64_t attr_valid, int times,
	uid_t uid = 0, gid_t gid = 0)
{
	EXPECT_CALL(*m_mock, process(
		ResultOf([=](auto in) {
			return (in.header.opcode == FUSE_GETATTR &&
				in.header.nodeid == ino);
		}, Eq(true)),
		_)
	).Times(times)
	.WillRepeatedly(Invoke(ReturnImmediate([=](auto i __unused, auto& out) {
		SET_OUT_HEADER_LEN(out, attr);
		out.body.attr.attr.ino = ino;	// Must match nodeid
		out.body.attr.attr.mode = mode;
		out.body.attr.attr.size = 0;
		out.body.attr.attr.uid = uid;
		out.body.attr.attr.gid = gid;
		out.body.attr.attr_valid = attr_valid;
	})));
}

void expect_lookup(const char *relpath, uint64_t ino, mode_t mode,
	uint64_t attr_valid, uid_t uid = 0, gid_t gid = 0)
{
	FuseTest::expect_lookup(relpath, ino, mode, 0, 1, attr_valid, uid, gid);
}

};

class Access: public DefaultPermissions {};
class Chown: public DefaultPermissions {};
class Chgrp: public DefaultPermissions {};
class CopyFileRange: public DefaultPermissions {};
class Lookup: public DefaultPermissions {};
class Open: public DefaultPermissions {};
class PosixFallocate: public DefaultPermissions {};
class Setattr: public DefaultPermissions {};
class Unlink: public DefaultPermissions {};
class Utimensat: public DefaultPermissions {};
class Write: public DefaultPermissions {};

/* 
 * Test permission handling during create, mkdir, mknod, link, symlink, and
 * rename vops (they all share a common path for permission checks in
 * VOP_LOOKUP)
 */
class Create: public DefaultPermissions {};

class Deleteextattr: public DefaultPermissions {
public:
void expect_removexattr()
{
	EXPECT_CALL(*m_mock, process(
		ResultOf([=](auto in) {
			return (in.header.opcode == FUSE_REMOVEXATTR);
		}, Eq(true)),
		_)
	).WillOnce(Invoke(ReturnErrno(0)));
}
};

class Getextattr: public DefaultPermissions {
public:
void expect_getxattr(ProcessMockerT r)
{
	EXPECT_CALL(*m_mock, process(
		ResultOf([=](auto in) {
			return (in.header.opcode == FUSE_GETXATTR);
		}, Eq(true)),
		_)
	).WillOnce(Invoke(r));
}
};

class Listextattr: public DefaultPermissions {
public:
void expect_listxattr()
{
	EXPECT_CALL(*m_mock, process(
		ResultOf([=](auto in) {
			return (in.header.opcode == FUSE_LISTXATTR);
		}, Eq(true)),
		_)
	).WillOnce(Invoke(ReturnImmediate([](auto i __unused, auto& out) {
		out.body.listxattr.size = 0;
		SET_OUT_HEADER_LEN(out, listxattr);
	})));
}
};

class Rename: public DefaultPermissions {
public:
	/* 
	 * Expect a rename and respond with the given error.  Don't both to
	 * validate arguments; the tests in rename.cc do that.
	 */
	void expect_rename(int error)
	{
		EXPECT_CALL(*m_mock, process(
			ResultOf([=](auto in) {
				return (in.header.opcode == FUSE_RENAME);
			}, Eq(true)),
			_)
		).WillOnce(Invoke(ReturnErrno(error)));
	}
};

class Setextattr: public DefaultPermissions {
public:
void expect_setxattr(int error)
{
	EXPECT_CALL(*m_mock, process(
		ResultOf([=](auto in) {
			return (in.header.opcode == FUSE_SETXATTR);
		}, Eq(true)),
		_)
	).WillOnce(Invoke(ReturnErrno(error)));
}
};

/* Return a group to which this user does not belong */
static gid_t excluded_group()
{
	int i, ngroups = 64;
	gid_t newgid, groups[ngroups];

	getgrouplist(getlogin(), getegid(), groups, &ngroups);
	for (newgid = 0; ; newgid++) {
		bool belongs = false;

		for (i = 0; i < ngroups; i++) {
			if (groups[i] == newgid)
				belongs = true;
		}
		if (!belongs)
			break;
	}
	/* newgid is now a group to which the current user does not belong */
	return newgid;
}

TEST_F(Access, eacces)
{
	const char FULLPATH[] = "mountpoint/some_file.txt";
	const char RELPATH[] = "some_file.txt";
	uint64_t ino = 42;
	mode_t	access_mode = X_OK;

	expect_getattr(FUSE_ROOT_ID, S_IFDIR | 0755, UINT64_MAX, 1);
	expect_lookup(RELPATH, ino, S_IFREG | 0644, UINT64_MAX);

	ASSERT_NE(0, access(FULLPATH, access_mode));
	ASSERT_EQ(EACCES, errno);
}

TEST_F(Access, eacces_no_cached_attrs)
{
	const char FULLPATH[] = "mountpoint/some_file.txt";
	const char RELPATH[] = "some_file.txt";
	uint64_t ino = 42;
	mode_t	access_mode = X_OK;

	expect_getattr(FUSE_ROOT_ID, S_IFDIR | 0755, 0, 1);
	expect_lookup(RELPATH, ino, S_IFREG | 0644, 0);
	expect_getattr(ino, S_IFREG | 0644, 0, 1);
	/* 
	 * Once default_permissions is properly implemented, there might be
	 * another FUSE_GETATTR or something in here.  But there should not be
	 * a FUSE_ACCESS
	 */

	ASSERT_NE(0, access(FULLPATH, access_mode));
	ASSERT_EQ(EACCES, errno);
}

TEST_F(Access, ok)
{
	const char FULLPATH[] = "mountpoint/some_file.txt";
	const char RELPATH[] = "some_file.txt";
	uint64_t ino = 42;
	mode_t	access_mode = R_OK;

	expect_getattr(FUSE_ROOT_ID, S_IFDIR | 0755, UINT64_MAX, 1);
	expect_lookup(RELPATH, ino, S_IFREG | 0644, UINT64_MAX);
	/* 
	 * Once default_permissions is properly implemented, there might be
	 * another FUSE_GETATTR or something in here.
	 */

	ASSERT_EQ(0, access(FULLPATH, access_mode)) << strerror(errno);
}

/* Unprivileged users may chown a file to their own uid */
TEST_F(Chown, chown_to_self)
{
	const char FULLPATH[] = "mountpoint/some_file.txt";
	const char RELPATH[] = "some_file.txt";
	const uint64_t ino = 42;
	const mode_t mode = 0755;
	uid_t uid;

	uid = geteuid();

	expect_getattr(FUSE_ROOT_ID, S_IFDIR | 0755, UINT64_MAX, 1, uid);
	expect_lookup(RELPATH, ino, S_IFREG | mode, UINT64_MAX, uid);
	/* The OS may optimize chown by omitting the redundant setattr */
	EXPECT_CALL(*m_mock, process(
		ResultOf([](auto in) {
			return (in.header.opcode == FUSE_SETATTR);
		}, Eq(true)),
		_)
	).WillRepeatedly(Invoke(ReturnImmediate([=](auto in __unused, auto& out){
		SET_OUT_HEADER_LEN(out, attr);
		out.body.attr.attr.mode = S_IFREG | mode;
		out.body.attr.attr.uid = uid;
	})));

	EXPECT_EQ(0, chown(FULLPATH, uid, -1)) << strerror(errno);
}

/*
 * A successful chown by a non-privileged non-owner should clear a file's SUID
 * bit
 */
TEST_F(Chown, clear_suid)
{
	const char FULLPATH[] = "mountpoint/some_file.txt";
	const char RELPATH[] = "some_file.txt";
	uint64_t ino = 42;
	const mode_t oldmode = 06755;
	const mode_t newmode = 0755;
	uid_t uid = geteuid();
	uint32_t valid = FATTR_UID | FATTR_MODE;

	expect_getattr(FUSE_ROOT_ID, S_IFDIR | 0755, UINT64_MAX, 1, uid);
	expect_lookup(RELPATH, ino, S_IFREG | oldmode, UINT64_MAX, uid);
	EXPECT_CALL(*m_mock, process(
		ResultOf([=](auto in) {
			return (in.header.opcode == FUSE_SETATTR &&
				in.header.nodeid == ino &&
				in.body.setattr.valid == valid &&
				in.body.setattr.mode == newmode);
		}, Eq(true)),
		_)
	).WillOnce(Invoke(ReturnImmediate([=](auto in __unused, auto& out) {
		SET_OUT_HEADER_LEN(out, attr);
		out.body.attr.attr.ino = ino;	// Must match nodeid
		out.body.attr.attr.mode = S_IFREG | newmode;
		out.body.attr.attr_valid = UINT64_MAX;
	})));

	EXPECT_EQ(0, chown(FULLPATH, uid, -1)) << strerror(errno);
}


/* Only root may change a file's owner */
TEST_F(Chown, eperm)
{
	const char FULLPATH[] = "mountpoint/some_file.txt";
	const char RELPATH[] = "some_file.txt";
	const uint64_t ino = 42;
	const mode_t mode = 0755;

	expect_getattr(FUSE_ROOT_ID, S_IFDIR | 0755, UINT64_MAX, 1, geteuid());
	expect_lookup(RELPATH, ino, S_IFREG | mode, UINT64_MAX, geteuid());
	EXPECT_CALL(*m_mock, process(
		ResultOf([](auto in) {
			return (in.header.opcode == FUSE_SETATTR);
		}, Eq(true)),
		_)
	).Times(0);

	EXPECT_NE(0, chown(FULLPATH, 0, -1));
	EXPECT_EQ(EPERM, errno);
}

/*
 * A successful chgrp by a non-privileged non-owner should clear a file's SUID
 * bit
 */
TEST_F(Chgrp, clear_suid)
{
	const char FULLPATH[] = "mountpoint/some_file.txt";
	const char RELPATH[] = "some_file.txt";
	uint64_t ino = 42;
	const mode_t oldmode = 06755;
	const mode_t newmode = 0755;
	uid_t uid = geteuid();
	gid_t gid = getegid();
	uint32_t valid = FATTR_GID | FATTR_MODE;

	expect_getattr(FUSE_ROOT_ID, S_IFDIR | 0755, UINT64_MAX, 1, uid);
	expect_lookup(RELPATH, ino, S_IFREG | oldmode, UINT64_MAX, uid, gid);
	EXPECT_CALL(*m_mock, process(
		ResultOf([=](auto in) {
			return (in.header.opcode == FUSE_SETATTR &&
				in.header.nodeid == ino &&
				in.body.setattr.valid == valid &&
				in.body.setattr.mode == newmode);
		}, Eq(true)),
		_)
	).WillOnce(Invoke(ReturnImmediate([=](auto in __unused, auto& out) {
		SET_OUT_HEADER_LEN(out, attr);
		out.body.attr.attr.ino = ino;	// Must match nodeid
		out.body.attr.attr.mode = S_IFREG | newmode;
		out.body.attr.attr_valid = UINT64_MAX;
	})));

	EXPECT_EQ(0, chown(FULLPATH, -1, gid)) << strerror(errno);
}

/* non-root users may only chgrp a file to a group they belong to */
TEST_F(Chgrp, eperm)
{
	const char FULLPATH[] = "mountpoint/some_file.txt";
	const char RELPATH[] = "some_file.txt";
	const uint64_t ino = 42;
	const mode_t mode = 0755;
	uid_t uid;
	gid_t gid, newgid;

	uid = geteuid();
	gid = getegid();
	newgid = excluded_group();

	expect_getattr(FUSE_ROOT_ID, S_IFDIR | 0755, UINT64_MAX, 1, uid, gid);
	expect_lookup(RELPATH, ino, S_IFREG | mode, UINT64_MAX, uid, gid);
	EXPECT_CALL(*m_mock, process(
		ResultOf([](auto in) {
			return (in.header.opcode == FUSE_SETATTR);
		}, Eq(true)),
		_)
	).Times(0);

	EXPECT_NE(0, chown(FULLPATH, -1, newgid));
	EXPECT_EQ(EPERM, errno);
}

TEST_F(Chgrp, ok)
{
	const char FULLPATH[] = "mountpoint/some_file.txt";
	const char RELPATH[] = "some_file.txt";
	const uint64_t ino = 42;
	const mode_t mode = 0755;
	uid_t uid;
	gid_t gid, newgid;

	uid = geteuid();
	gid = 0;
	newgid = getegid();

	expect_getattr(FUSE_ROOT_ID, S_IFDIR | 0755, UINT64_MAX, 1, uid, gid);
	expect_lookup(RELPATH, ino, S_IFREG | mode, UINT64_MAX, uid, gid);
	/* The OS may optimize chgrp by omitting the redundant setattr */
	EXPECT_CALL(*m_mock, process(
		ResultOf([](auto in) {
			return (in.header.opcode == FUSE_SETATTR &&
				in.header.nodeid == ino);
		}, Eq(true)),
		_)
	).WillRepeatedly(Invoke(ReturnImmediate([=](auto in __unused, auto& out){
		SET_OUT_HEADER_LEN(out, attr);
		out.body.attr.attr.mode = S_IFREG | mode;
		out.body.attr.attr.uid = uid;
		out.body.attr.attr.gid = newgid;
	})));

	EXPECT_EQ(0, chown(FULLPATH, -1, newgid)) << strerror(errno);
}

/* A write by a non-owner should clear a file's SGID bit */
TEST_F(CopyFileRange, clear_sgid)
{
	const char FULLPATH_IN[] = "mountpoint/in.txt";
	const char RELPATH_IN[] = "in.txt";
	const char FULLPATH_OUT[] = "mountpoint/out.txt";
	const char RELPATH_OUT[] = "out.txt";
	struct stat sb;
	uint64_t ino_in = 42;
	uint64_t ino_out = 43;
	mode_t oldmode = 02777;
	mode_t newmode = 0777;
	off_t fsize = 16;
	off_t off_in = 0;
	off_t off_out = 8;
	off_t len = 8;
	int fd_in, fd_out;

	expect_getattr(FUSE_ROOT_ID, S_IFDIR | 0755, UINT64_MAX, 1);
	FuseTest::expect_lookup(RELPATH_IN, ino_in, S_IFREG | oldmode, fsize, 1,
	    UINT64_MAX, 0, 0);
	expect_open(ino_in, 0, 1);
	FuseTest::expect_lookup(RELPATH_OUT, ino_out, S_IFREG | oldmode, fsize,
	    1, UINT64_MAX, 0, 0);
	expect_open(ino_out, 0, 1);
	expect_copy_file_range(ino_in, off_in, ino_out, off_out, len);
	expect_chmod(ino_out, newmode, fsize);

	fd_in = open(FULLPATH_IN, O_RDONLY);
	ASSERT_LE(0, fd_in) << strerror(errno);
	fd_out = open(FULLPATH_OUT, O_WRONLY);
	ASSERT_LE(0, fd_out) << strerror(errno);
	ASSERT_EQ(len,
	    copy_file_range(fd_in, &off_in, fd_out, &off_out, len, 0))
	    << strerror(errno);
	ASSERT_EQ(0, fstat(fd_out, &sb)) << strerror(errno);
	EXPECT_EQ(S_IFREG | newmode, sb.st_mode);
	ASSERT_EQ(0, fstat(fd_in, &sb)) << strerror(errno);
	EXPECT_EQ(S_IFREG | oldmode, sb.st_mode);

	leak(fd_in);
	leak(fd_out);
}

/* A write by a non-owner should clear a file's SUID bit */
TEST_F(CopyFileRange, clear_suid)
{
	const char FULLPATH_IN[] = "mountpoint/in.txt";
	const char RELPATH_IN[] = "in.txt";
	const char FULLPATH_OUT[] = "mountpoint/out.txt";
	const char RELPATH_OUT[] = "out.txt";
	struct stat sb;
	uint64_t ino_in = 42;
	uint64_t ino_out = 43;
	mode_t oldmode = 04777;
	mode_t newmode = 0777;
	off_t fsize = 16;
	off_t off_in = 0;
	off_t off_out = 8;
	off_t len = 8;
	int fd_in, fd_out;

	expect_getattr(FUSE_ROOT_ID, S_IFDIR | 0755, UINT64_MAX, 1);
	FuseTest::expect_lookup(RELPATH_IN, ino_in, S_IFREG | oldmode, fsize, 1,
	    UINT64_MAX, 0, 0);
	expect_open(ino_in, 0, 1);
	FuseTest::expect_lookup(RELPATH_OUT, ino_out, S_IFREG | oldmode, fsize,
	    1, UINT64_MAX, 0, 0);
	expect_open(ino_out, 0, 1);
	expect_copy_file_range(ino_in, off_in, ino_out, off_out, len);
	expect_chmod(ino_out, newmode, fsize);

	fd_in = open(FULLPATH_IN, O_RDONLY);
	ASSERT_LE(0, fd_in) << strerror(errno);
	fd_out = open(FULLPATH_OUT, O_WRONLY);
	ASSERT_LE(0, fd_out) << strerror(errno);
	ASSERT_EQ(len,
	    copy_file_range(fd_in, &off_in, fd_out, &off_out, len, 0))
	    << strerror(errno);
	ASSERT_EQ(0, fstat(fd_out, &sb)) << strerror(errno);
	EXPECT_EQ(S_IFREG | newmode, sb.st_mode);
	ASSERT_EQ(0, fstat(fd_in, &sb)) << strerror(errno);
	EXPECT_EQ(S_IFREG | oldmode, sb.st_mode);

	leak(fd_in);
	leak(fd_out);
}

TEST_F(Create, ok)
{
	const char FULLPATH[] = "mountpoint/some_file.txt";
	const char RELPATH[] = "some_file.txt";
	uint64_t ino = 42;
	int fd;

	expect_getattr(FUSE_ROOT_ID, S_IFDIR | 0777, UINT64_MAX, 1);
	EXPECT_LOOKUP(FUSE_ROOT_ID, RELPATH)
		.WillOnce(Invoke(ReturnErrno(ENOENT)));
	expect_create(RELPATH, ino);

	fd = open(FULLPATH, O_CREAT | O_EXCL, 0644);
	ASSERT_LE(0, fd) << strerror(errno);
	leak(fd);
}

TEST_F(Create, eacces)
{
	const char FULLPATH[] = "mountpoint/some_file.txt";
	const char RELPATH[] = "some_file.txt";

	expect_getattr(FUSE_ROOT_ID, S_IFDIR | 0755, UINT64_MAX, 1);
	EXPECT_LOOKUP(FUSE_ROOT_ID, RELPATH)
		.WillOnce(Invoke(ReturnErrno(ENOENT)));

	ASSERT_EQ(-1, open(FULLPATH, O_CREAT | O_EXCL, 0644));
	EXPECT_EQ(EACCES, errno);
}

TEST_F(Deleteextattr, eacces)
{
	const char FULLPATH[] = "mountpoint/some_file.txt";
	const char RELPATH[] = "some_file.txt";
	uint64_t ino = 42;
	int ns = EXTATTR_NAMESPACE_USER;

	expect_getattr(FUSE_ROOT_ID, S_IFDIR | 0755, UINT64_MAX, 1);
	expect_lookup(RELPATH, ino, S_IFREG | 0644, UINT64_MAX, 0);

	ASSERT_EQ(-1, extattr_delete_file(FULLPATH, ns, "foo"));
	ASSERT_EQ(EACCES, errno);
}

TEST_F(Deleteextattr, ok)
{
	const char FULLPATH[] = "mountpoint/some_file.txt";
	const char RELPATH[] = "some_file.txt";
	uint64_t ino = 42;
	int ns = EXTATTR_NAMESPACE_USER;

	expect_getattr(FUSE_ROOT_ID, S_IFDIR | 0755, UINT64_MAX, 1);
	expect_lookup(RELPATH, ino, S_IFREG | 0644, UINT64_MAX, geteuid());
	expect_removexattr();

	ASSERT_EQ(0, extattr_delete_file(FULLPATH, ns, "foo"))
		<< strerror(errno);
}

/* Delete system attributes requires superuser privilege */
TEST_F(Deleteextattr, system)
{
	const char FULLPATH[] = "mountpoint/some_file.txt";
	const char RELPATH[] = "some_file.txt";
	uint64_t ino = 42;
	int ns = EXTATTR_NAMESPACE_SYSTEM;

	expect_getattr(FUSE_ROOT_ID, S_IFDIR | 0755, UINT64_MAX, 1);
	expect_lookup(RELPATH, ino, S_IFREG | 0666, UINT64_MAX, geteuid());

	ASSERT_EQ(-1, extattr_delete_file(FULLPATH, ns, "foo"));
	ASSERT_EQ(EPERM, errno);
}

/* Anybody with write permission can set both timestamps to UTIME_NOW */
TEST_F(Utimensat, utime_now)
{
	const char FULLPATH[] = "mountpoint/some_file.txt";
	const char RELPATH[] = "some_file.txt";
	const uint64_t ino = 42;
	/* Write permissions for everybody */
	const mode_t mode = 0666;
	uid_t owner = 0;
	const timespec times[2] = {
		{.tv_sec = 0, .tv_nsec = UTIME_NOW},
		{.tv_sec = 0, .tv_nsec = UTIME_NOW},
	};

	expect_getattr(FUSE_ROOT_ID, S_IFDIR | 0755, UINT64_MAX, 1);
	expect_lookup(RELPATH, ino, S_IFREG | mode, UINT64_MAX, owner);
	EXPECT_CALL(*m_mock, process(
		ResultOf([](auto in) {
			return (in.header.opcode == FUSE_SETATTR &&
				in.header.nodeid == ino &&
				in.body.setattr.valid & FATTR_ATIME &&
				in.body.setattr.valid & FATTR_MTIME);
		}, Eq(true)),
		_)
	).WillOnce(Invoke(ReturnImmediate([](auto in __unused, auto& out) {
		SET_OUT_HEADER_LEN(out, attr);
		out.body.attr.attr.mode = S_IFREG | mode;
	})));

	ASSERT_EQ(0, utimensat(AT_FDCWD, FULLPATH, &times[0], 0))
		<< strerror(errno);
}

/* Anybody can set both timestamps to UTIME_OMIT */
TEST_F(Utimensat, utime_omit)
{
	const char FULLPATH[] = "mountpoint/some_file.txt";
	const char RELPATH[] = "some_file.txt";
	const uint64_t ino = 42;
	/* Write permissions for no one */
	const mode_t mode = 0444;
	uid_t owner = 0;
	const timespec times[2] = {
		{.tv_sec = 0, .tv_nsec = UTIME_OMIT},
		{.tv_sec = 0, .tv_nsec = UTIME_OMIT},
	};

	expect_getattr(FUSE_ROOT_ID, S_IFDIR | 0755, UINT64_MAX, 1);
	expect_lookup(RELPATH, ino, S_IFREG | mode, UINT64_MAX, owner);

	ASSERT_EQ(0, utimensat(AT_FDCWD, FULLPATH, &times[0], 0))
		<< strerror(errno);
}

/* Deleting user attributes merely requires WRITE privilege */
TEST_F(Deleteextattr, user)
{
	const char FULLPATH[] = "mountpoint/some_file.txt";
	const char RELPATH[] = "some_file.txt";
	uint64_t ino = 42;
	int ns = EXTATTR_NAMESPACE_USER;

	expect_getattr(FUSE_ROOT_ID, S_IFDIR | 0755, UINT64_MAX, 1);
	expect_lookup(RELPATH, ino, S_IFREG | 0666, UINT64_MAX, 0);
	expect_removexattr();

	ASSERT_EQ(0, extattr_delete_file(FULLPATH, ns, "foo"))
		<< strerror(errno);
}

TEST_F(Getextattr, eacces)
{
	const char FULLPATH[] = "mountpoint/some_file.txt";
	const char RELPATH[] = "some_file.txt";
	uint64_t ino = 42;
	char data[80];
	int ns = EXTATTR_NAMESPACE_USER;

	expect_getattr(FUSE_ROOT_ID, S_IFDIR | 0755, UINT64_MAX, 1);
	expect_lookup(RELPATH, ino, S_IFREG | 0600, UINT64_MAX, 0);

	ASSERT_EQ(-1,
		extattr_get_file(FULLPATH, ns, "foo", data, sizeof(data)));
	ASSERT_EQ(EACCES, errno);
}

TEST_F(Getextattr, ok)
{
	const char FULLPATH[] = "mountpoint/some_file.txt";
	const char RELPATH[] = "some_file.txt";
	uint64_t ino = 42;
	char data[80];
	const char value[] = "whatever";
	ssize_t value_len = strlen(value) + 1;
	int ns = EXTATTR_NAMESPACE_USER;
	ssize_t r;

	expect_getattr(FUSE_ROOT_ID, S_IFDIR | 0755, UINT64_MAX, 1);
	/* Getting user attributes only requires read access */
	expect_lookup(RELPATH, ino, S_IFREG | 0444, UINT64_MAX, 0);
	expect_getxattr(
		ReturnImmediate([&](auto in __unused, auto& out) {
			memcpy((void*)out.body.bytes, value, value_len);
			out.header.len = sizeof(out.header) + value_len;
		})
	);

	r = extattr_get_file(FULLPATH, ns, "foo", data, sizeof(data));
	ASSERT_EQ(value_len, r)  << strerror(errno);
	EXPECT_STREQ(value, data);
}

/* Getting system attributes requires superuser privileges */
TEST_F(Getextattr, system)
{
	const char FULLPATH[] = "mountpoint/some_file.txt";
	const char RELPATH[] = "some_file.txt";
	uint64_t ino = 42;
	char data[80];
	int ns = EXTATTR_NAMESPACE_SYSTEM;

	expect_getattr(FUSE_ROOT_ID, S_IFDIR | 0755, UINT64_MAX, 1);
	expect_lookup(RELPATH, ino, S_IFREG | 0666, UINT64_MAX, geteuid());

	ASSERT_EQ(-1,
		extattr_get_file(FULLPATH, ns, "foo", data, sizeof(data)));
	ASSERT_EQ(EPERM, errno);
}

TEST_F(Listextattr, eacces)
{
	const char FULLPATH[] = "mountpoint/some_file.txt";
	const char RELPATH[] = "some_file.txt";
	uint64_t ino = 42;
	int ns = EXTATTR_NAMESPACE_USER;

	expect_getattr(FUSE_ROOT_ID, S_IFDIR | 0777, UINT64_MAX, 1);
	expect_lookup(RELPATH, ino, S_IFREG | 0600, UINT64_MAX, 0);

	ASSERT_EQ(-1, extattr_list_file(FULLPATH, ns, NULL, 0));
	ASSERT_EQ(EACCES, errno);
}

TEST_F(Listextattr, ok)
{
	const char FULLPATH[] = "mountpoint/some_file.txt";
	const char RELPATH[] = "some_file.txt";
	uint64_t ino = 42;
	int ns = EXTATTR_NAMESPACE_USER;

	expect_getattr(FUSE_ROOT_ID, S_IFDIR | 0777, UINT64_MAX, 1);
	/* Listing user extended attributes merely requires read access */
	expect_lookup(RELPATH, ino, S_IFREG | 0644, UINT64_MAX, 0);
	expect_listxattr();

	ASSERT_EQ(0, extattr_list_file(FULLPATH, ns, NULL, 0))
		<< strerror(errno);
}

/* Listing system xattrs requires superuser privileges */
TEST_F(Listextattr, system)
{
	const char FULLPATH[] = "mountpoint/some_file.txt";
	const char RELPATH[] = "some_file.txt";
	uint64_t ino = 42;
	int ns = EXTATTR_NAMESPACE_SYSTEM;

	expect_getattr(FUSE_ROOT_ID, S_IFDIR | 0777, UINT64_MAX, 1);
	/* Listing user extended attributes merely requires read access */
	expect_lookup(RELPATH, ino, S_IFREG | 0644, UINT64_MAX, geteuid());

	ASSERT_EQ(-1, extattr_list_file(FULLPATH, ns, NULL, 0));
	ASSERT_EQ(EPERM, errno);
}

/* A component of the search path lacks execute permissions */
TEST_F(Lookup, eacces)
{
	const char FULLPATH[] = "mountpoint/some_dir/some_file.txt";
	const char RELDIRPATH[] = "some_dir";
	uint64_t dir_ino = 42;

	expect_getattr(FUSE_ROOT_ID, S_IFDIR | 0755, UINT64_MAX, 1);
	expect_lookup(RELDIRPATH, dir_ino, S_IFDIR | 0700, UINT64_MAX, 0);

	EXPECT_EQ(-1, access(FULLPATH, F_OK));
	EXPECT_EQ(EACCES, errno);
}

TEST_F(Open, eacces)
{
	const char FULLPATH[] = "mountpoint/some_file.txt";
	const char RELPATH[] = "some_file.txt";
	uint64_t ino = 42;

	expect_getattr(FUSE_ROOT_ID, S_IFDIR | 0755, UINT64_MAX, 1);
	expect_lookup(RELPATH, ino, S_IFREG | 0644, UINT64_MAX);

	EXPECT_EQ(-1, open(FULLPATH, O_RDWR));
	EXPECT_EQ(EACCES, errno);
}

TEST_F(Open, ok)
{
	const char FULLPATH[] = "mountpoint/some_file.txt";
	const char RELPATH[] = "some_file.txt";
	uint64_t ino = 42;
	int fd;

	expect_getattr(FUSE_ROOT_ID, S_IFDIR | 0755, UINT64_MAX, 1);
	expect_lookup(RELPATH, ino, S_IFREG | 0644, UINT64_MAX);
	expect_open(ino, 0, 1);

	fd = open(FULLPATH, O_RDONLY);
	ASSERT_LE(0, fd) << strerror(errno);
	leak(fd);
}

/* A write by a non-owner should clear a file's SGID bit */
TEST_F(PosixFallocate, clear_sgid)
{
	const char FULLPATH[] = "mountpoint/file.txt";
	const char RELPATH[] = "file.txt";
	struct stat sb;
	uint64_t ino = 42;
	mode_t oldmode = 02777;
	mode_t newmode = 0777;
	off_t fsize = 16;
	off_t off = 8;
	off_t len = 8;
	int fd;

	expect_getattr(FUSE_ROOT_ID, S_IFDIR | 0755, UINT64_MAX, 1);
	FuseTest::expect_lookup(RELPATH, ino, S_IFREG | oldmode, fsize,
	    1, UINT64_MAX, 0, 0);
	expect_open(ino, 0, 1);
	expect_fallocate(ino, off, len, 0, 0);
	expect_chmod(ino, newmode, fsize);

	fd = open(FULLPATH, O_WRONLY);
	ASSERT_LE(0, fd) << strerror(errno);
	EXPECT_EQ(0, posix_fallocate(fd, off, len)) << strerror(errno);
	ASSERT_EQ(0, fstat(fd, &sb)) << strerror(errno);
	EXPECT_EQ(S_IFREG | newmode, sb.st_mode);

	leak(fd);
}

/* A write by a non-owner should clear a file's SUID bit */
TEST_F(PosixFallocate, clear_suid)
{
	const char FULLPATH[] = "mountpoint/file.txt";
	const char RELPATH[] = "file.txt";
	struct stat sb;
	uint64_t ino = 42;
	mode_t oldmode = 04777;
	mode_t newmode = 0777;
	off_t fsize = 16;
	off_t off = 8;
	off_t len = 8;
	int fd;

	expect_getattr(FUSE_ROOT_ID, S_IFDIR | 0755, UINT64_MAX, 1);
	FuseTest::expect_lookup(RELPATH, ino, S_IFREG | oldmode, fsize,
	    1, UINT64_MAX, 0, 0);
	expect_open(ino, 0, 1);
	expect_fallocate(ino, off, len, 0, 0);
	expect_chmod(ino, newmode, fsize);

	fd = open(FULLPATH, O_WRONLY);
	ASSERT_LE(0, fd) << strerror(errno);
	EXPECT_EQ(0, posix_fallocate(fd, off, len)) << strerror(errno);
	ASSERT_EQ(0, fstat(fd, &sb)) << strerror(errno);
	EXPECT_EQ(S_IFREG | newmode, sb.st_mode);

	leak(fd);
}

/*
 * posix_fallcoate() of a file without writable permissions should succeed as
 * long as the file descriptor is writable.  This is important when combined
 * with O_CREAT
 */
TEST_F(PosixFallocate, posix_fallocate_of_newly_created_file)
{
	const char FULLPATH[] = "mountpoint/some_file.txt";
	const char RELPATH[] = "some_file.txt";
	const uint64_t ino = 42;
	off_t off = 8;
	off_t len = 8;
	int fd;

	expect_getattr(FUSE_ROOT_ID, S_IFDIR | 0777, UINT64_MAX, 1);
	EXPECT_LOOKUP(FUSE_ROOT_ID, RELPATH)
		.WillOnce(Invoke(ReturnErrno(ENOENT)));
	expect_create(RELPATH, ino);
	expect_fallocate(ino, off, len, 0, 0);

	fd = open(FULLPATH, O_CREAT | O_RDWR, 0);
	ASSERT_LE(0, fd) << strerror(errno);
	EXPECT_EQ(0, posix_fallocate(fd, off, len)) << strerror(errno);
	leak(fd);
}

TEST_F(Rename, eacces_on_srcdir)
{
	const char FULLDST[] = "mountpoint/d/dst";
	const char RELDST[] = "d/dst";
	const char FULLSRC[] = "mountpoint/src";
	const char RELSRC[] = "src";
	uint64_t ino = 42;

	expect_getattr(FUSE_ROOT_ID, S_IFDIR | 0755, UINT64_MAX, 1, 0);
	expect_lookup(RELSRC, ino, S_IFREG | 0644, UINT64_MAX);
	EXPECT_LOOKUP(FUSE_ROOT_ID, RELDST)
		.Times(AnyNumber())
		.WillRepeatedly(Invoke(ReturnErrno(ENOENT)));

	ASSERT_EQ(-1, rename(FULLSRC, FULLDST));
	ASSERT_EQ(EACCES, errno);
}

TEST_F(Rename, eacces_on_dstdir_for_creating)
{
	const char FULLDST[] = "mountpoint/d/dst";
	const char RELDSTDIR[] = "d";
	const char RELDST[] = "dst";
	const char FULLSRC[] = "mountpoint/src";
	const char RELSRC[] = "src";
	uint64_t src_ino = 42;
	uint64_t dstdir_ino = 43;

	expect_getattr(FUSE_ROOT_ID, S_IFDIR | 0777, UINT64_MAX, 1, 0);
	expect_lookup(RELSRC, src_ino, S_IFREG | 0644, UINT64_MAX);
	expect_lookup(RELDSTDIR, dstdir_ino, S_IFDIR | 0755, UINT64_MAX);
	EXPECT_LOOKUP(dstdir_ino, RELDST).WillOnce(Invoke(ReturnErrno(ENOENT)));

	ASSERT_EQ(-1, rename(FULLSRC, FULLDST));
	ASSERT_EQ(EACCES, errno);
}

TEST_F(Rename, eacces_on_dstdir_for_removing)
{
	const char FULLDST[] = "mountpoint/d/dst";
	const char RELDSTDIR[] = "d";
	const char RELDST[] = "dst";
	const char FULLSRC[] = "mountpoint/src";
	const char RELSRC[] = "src";
	uint64_t src_ino = 42;
	uint64_t dstdir_ino = 43;

	expect_getattr(FUSE_ROOT_ID, S_IFDIR | 0777, UINT64_MAX, 1, 0);
	expect_lookup(RELSRC, src_ino, S_IFREG | 0644, UINT64_MAX);
	expect_lookup(RELDSTDIR, dstdir_ino, S_IFDIR | 0755, UINT64_MAX);
	EXPECT_LOOKUP(dstdir_ino, RELDST).WillOnce(Invoke(ReturnErrno(ENOENT)));

	ASSERT_EQ(-1, rename(FULLSRC, FULLDST));
	ASSERT_EQ(EACCES, errno);
}

TEST_F(Rename, eperm_on_sticky_srcdir)
{
	const char FULLDST[] = "mountpoint/d/dst";
	const char FULLSRC[] = "mountpoint/src";
	const char RELSRC[] = "src";
	uint64_t ino = 42;

	expect_getattr(FUSE_ROOT_ID, S_IFDIR | 01777, UINT64_MAX, 1, 0);
	expect_lookup(RELSRC, ino, S_IFREG | 0644, UINT64_MAX);

	ASSERT_EQ(-1, rename(FULLSRC, FULLDST));
	ASSERT_EQ(EPERM, errno);
}

/* 
 * A user cannot move out a subdirectory that he does not own, because that
 * would require changing the subdirectory's ".." dirent
 */
TEST_F(Rename, eperm_for_subdirectory)
{
	const char FULLDST[] = "mountpoint/d/dst";
	const char FULLSRC[] = "mountpoint/src";
	const char RELDSTDIR[] = "d";
	const char RELDST[] = "dst";
	const char RELSRC[] = "src";
	uint64_t ino = 42;
	uint64_t dstdir_ino = 43;

	expect_getattr(FUSE_ROOT_ID, S_IFDIR | 0777, UINT64_MAX, 1, 0);
	expect_lookup(RELSRC, ino, S_IFDIR | 0755, UINT64_MAX, 0);
	expect_lookup(RELDSTDIR, dstdir_ino, S_IFDIR | 0777, UINT64_MAX, 0);
	EXPECT_LOOKUP(dstdir_ino, RELDST).WillOnce(Invoke(ReturnErrno(ENOENT)));

	ASSERT_EQ(-1, rename(FULLSRC, FULLDST));
	ASSERT_EQ(EACCES, errno);
}

/*
 * A user _can_ rename a subdirectory to which he lacks write permissions, if
 * it will keep the same parent
 */
TEST_F(Rename, subdirectory_to_same_dir)
{
	const char FULLDST[] = "mountpoint/dst";
	const char FULLSRC[] = "mountpoint/src";
	const char RELDST[] = "dst";
	const char RELSRC[] = "src";
	uint64_t ino = 42;

	expect_getattr(FUSE_ROOT_ID, S_IFDIR | 0777, UINT64_MAX, 1, 0);
	expect_lookup(RELSRC, ino, S_IFDIR | 0755, UINT64_MAX, 0);
	EXPECT_LOOKUP(FUSE_ROOT_ID, RELDST)
		.WillOnce(Invoke(ReturnErrno(ENOENT)));
	expect_rename(0);

	ASSERT_EQ(0, rename(FULLSRC, FULLDST)) << strerror(errno);
}

TEST_F(Rename, eperm_on_sticky_dstdir)
{
	const char FULLDST[] = "mountpoint/d/dst";
	const char RELDSTDIR[] = "d";
	const char RELDST[] = "dst";
	const char FULLSRC[] = "mountpoint/src";
	const char RELSRC[] = "src";
	uint64_t src_ino = 42;
	uint64_t dstdir_ino = 43;
	uint64_t dst_ino = 44;

	expect_getattr(FUSE_ROOT_ID, S_IFDIR | 0777, UINT64_MAX, 1, 0);
	expect_lookup(RELSRC, src_ino, S_IFREG | 0644, UINT64_MAX);
	expect_lookup(RELDSTDIR, dstdir_ino, S_IFDIR | 01777, UINT64_MAX);
	EXPECT_LOOKUP(dstdir_ino, RELDST)
	.WillOnce(Invoke(ReturnImmediate([=](auto in __unused, auto& out) {
		SET_OUT_HEADER_LEN(out, entry);
		out.body.entry.attr.mode = S_IFREG | 0644;
		out.body.entry.nodeid = dst_ino;
		out.body.entry.attr_valid = UINT64_MAX;
		out.body.entry.entry_valid = UINT64_MAX;
		out.body.entry.attr.uid = 0;
	})));

	ASSERT_EQ(-1, rename(FULLSRC, FULLDST));
	ASSERT_EQ(EPERM, errno);
}

/* Successfully rename a file, overwriting the destination */
TEST_F(Rename, ok)
{
	const char FULLDST[] = "mountpoint/dst";
	const char RELDST[] = "dst";
	const char FULLSRC[] = "mountpoint/src";
	const char RELSRC[] = "src";
	// The inode of the already-existing destination file
	uint64_t dst_ino = 2;
	uint64_t ino = 42;

	expect_getattr(FUSE_ROOT_ID, S_IFDIR | 0777, UINT64_MAX, 1, geteuid());
	expect_lookup(RELSRC, ino, S_IFREG | 0644, UINT64_MAX);
	expect_lookup(RELDST, dst_ino, S_IFREG | 0644, UINT64_MAX);
	expect_rename(0);

	ASSERT_EQ(0, rename(FULLSRC, FULLDST)) << strerror(errno);
}

TEST_F(Rename, ok_to_remove_src_because_of_stickiness)
{
	const char FULLDST[] = "mountpoint/dst";
	const char RELDST[] = "dst";
	const char FULLSRC[] = "mountpoint/src";
	const char RELSRC[] = "src";
	uint64_t ino = 42;

	expect_getattr(FUSE_ROOT_ID, S_IFDIR | 01777, UINT64_MAX, 1, 0);
	expect_lookup(RELSRC, ino, S_IFREG | 0644, UINT64_MAX, geteuid());
	EXPECT_LOOKUP(FUSE_ROOT_ID, RELDST)
		.WillOnce(Invoke(ReturnErrno(ENOENT)));
	expect_rename(0);

	ASSERT_EQ(0, rename(FULLSRC, FULLDST)) << strerror(errno);
}

TEST_F(Setattr, ok)
{
	const char FULLPATH[] = "mountpoint/some_file.txt";
	const char RELPATH[] = "some_file.txt";
	const uint64_t ino = 42;
	const mode_t oldmode = 0755;
	const mode_t newmode = 0644;

	expect_getattr(FUSE_ROOT_ID, S_IFDIR | 0755, UINT64_MAX, 1);
	expect_lookup(RELPATH, ino, S_IFREG | oldmode, UINT64_MAX, geteuid());
	EXPECT_CALL(*m_mock, process(
		ResultOf([](auto in) {
			return (in.header.opcode == FUSE_SETATTR &&
				in.header.nodeid == ino &&
				in.body.setattr.mode == newmode);
		}, Eq(true)),
		_)
	).WillOnce(Invoke(ReturnImmediate([](auto in __unused, auto& out) {
		SET_OUT_HEADER_LEN(out, attr);
		out.body.attr.attr.mode = S_IFREG | newmode;
	})));

	EXPECT_EQ(0, chmod(FULLPATH, newmode)) << strerror(errno);
}

TEST_F(Setattr, eacces)
{
	const char FULLPATH[] = "mountpoint/some_file.txt";
	const char RELPATH[] = "some_file.txt";
	const uint64_t ino = 42;
	const mode_t oldmode = 0755;
	const mode_t newmode = 0644;

	expect_getattr(FUSE_ROOT_ID, S_IFDIR | 0755, UINT64_MAX, 1);
	expect_lookup(RELPATH, ino, S_IFREG | oldmode, UINT64_MAX, 0);
	EXPECT_CALL(*m_mock, process(
		ResultOf([](auto in) {
			return (in.header.opcode == FUSE_SETATTR);
		}, Eq(true)),
		_)
	).Times(0);

	EXPECT_NE(0, chmod(FULLPATH, newmode));
	EXPECT_EQ(EPERM, errno);
}

/*
 * ftruncate() of a file without writable permissions should succeed as long as
 * the file descriptor is writable.  This is important when combined with
 * O_CREAT
 */
TEST_F(Setattr, ftruncate_of_newly_created_file)
{
	const char FULLPATH[] = "mountpoint/some_file.txt";
	const char RELPATH[] = "some_file.txt";
	const uint64_t ino = 42;
	const mode_t mode = 0000;
	int fd;

	expect_getattr(FUSE_ROOT_ID, S_IFDIR | 0777, UINT64_MAX, 1);
	EXPECT_LOOKUP(FUSE_ROOT_ID, RELPATH)
		.WillOnce(Invoke(ReturnErrno(ENOENT)));
	expect_create(RELPATH, ino);
	EXPECT_CALL(*m_mock, process(
		ResultOf([](auto in) {
			return (in.header.opcode == FUSE_SETATTR &&
				in.header.nodeid == ino &&
				(in.body.setattr.valid & FATTR_SIZE));
		}, Eq(true)),
		_)
	).WillOnce(Invoke(ReturnImmediate([=](auto in __unused, auto& out) {
		SET_OUT_HEADER_LEN(out, attr);
		out.body.attr.attr.ino = ino;
		out.body.attr.attr.mode = S_IFREG | mode;
		out.body.attr.attr_valid = UINT64_MAX;
	})));

	fd = open(FULLPATH, O_CREAT | O_RDWR, 0);
	ASSERT_LE(0, fd) << strerror(errno);
	ASSERT_EQ(0, ftruncate(fd, 100)) << strerror(errno);
	leak(fd);
}

/* 
 * Setting the sgid bit should fail for an unprivileged user who doesn't belong
 * to the file's group
 */
TEST_F(Setattr, sgid_by_non_group_member)
{
	const char FULLPATH[] = "mountpoint/some_file.txt";
	const char RELPATH[] = "some_file.txt";
	const uint64_t ino = 42;
	const mode_t oldmode = 0755;
	const mode_t newmode = 02755;
	uid_t uid = geteuid();
	gid_t gid = excluded_group();

	expect_getattr(FUSE_ROOT_ID, S_IFDIR | 0755, UINT64_MAX, 1);
	expect_lookup(RELPATH, ino, S_IFREG | oldmode, UINT64_MAX, uid, gid);
	EXPECT_CALL(*m_mock, process(
		ResultOf([](auto in) {
			return (in.header.opcode == FUSE_SETATTR);
		}, Eq(true)),
		_)
	).Times(0);

	EXPECT_NE(0, chmod(FULLPATH, newmode));
	EXPECT_EQ(EPERM, errno);
}

/* Only the superuser may set the sticky bit on a non-directory */
TEST_F(Setattr, sticky_regular_file)
{
	const char FULLPATH[] = "mountpoint/some_file.txt";
	const char RELPATH[] = "some_file.txt";
	const uint64_t ino = 42;
	const mode_t oldmode = 0644;
	const mode_t newmode = 01644;

	expect_getattr(FUSE_ROOT_ID, S_IFDIR | 0755, UINT64_MAX, 1);
	expect_lookup(RELPATH, ino, S_IFREG | oldmode, UINT64_MAX, geteuid());
	EXPECT_CALL(*m_mock, process(
		ResultOf([](auto in) {
			return (in.header.opcode == FUSE_SETATTR);
		}, Eq(true)),
		_)
	).Times(0);

	EXPECT_NE(0, chmod(FULLPATH, newmode));
	EXPECT_EQ(EFTYPE, errno);
}

TEST_F(Setextattr, ok)
{
	const char FULLPATH[] = "mountpoint/some_file.txt";
	const char RELPATH[] = "some_file.txt";
	uint64_t ino = 42;
	const char value[] = "whatever";
	ssize_t value_len = strlen(value) + 1;
	int ns = EXTATTR_NAMESPACE_USER;
	ssize_t r;

	expect_getattr(FUSE_ROOT_ID, S_IFDIR | 0755, UINT64_MAX, 1);
	expect_lookup(RELPATH, ino, S_IFREG | 0644, UINT64_MAX, geteuid());
	expect_setxattr(0);

	r = extattr_set_file(FULLPATH, ns, "foo", (const void*)value,
		value_len);
	ASSERT_EQ(value_len, r) << strerror(errno);
}

TEST_F(Setextattr, eacces)
{
	const char FULLPATH[] = "mountpoint/some_file.txt";
	const char RELPATH[] = "some_file.txt";
	uint64_t ino = 42;
	const char value[] = "whatever";
	ssize_t value_len = strlen(value) + 1;
	int ns = EXTATTR_NAMESPACE_USER;

	expect_getattr(FUSE_ROOT_ID, S_IFDIR | 0755, UINT64_MAX, 1);
	expect_lookup(RELPATH, ino, S_IFREG | 0644, UINT64_MAX, 0);

	ASSERT_EQ(-1, extattr_set_file(FULLPATH, ns, "foo", (const void*)value,
		value_len));
	ASSERT_EQ(EACCES, errno);
}

// Setting system attributes requires superuser privileges
TEST_F(Setextattr, system)
{
	const char FULLPATH[] = "mountpoint/some_file.txt";
	const char RELPATH[] = "some_file.txt";
	uint64_t ino = 42;
	const char value[] = "whatever";
	ssize_t value_len = strlen(value) + 1;
	int ns = EXTATTR_NAMESPACE_SYSTEM;

	expect_getattr(FUSE_ROOT_ID, S_IFDIR | 0755, UINT64_MAX, 1);
	expect_lookup(RELPATH, ino, S_IFREG | 0666, UINT64_MAX, geteuid());

	ASSERT_EQ(-1, extattr_set_file(FULLPATH, ns, "foo", (const void*)value,
		value_len));
	ASSERT_EQ(EPERM, errno);
}

// Setting user attributes merely requires write privileges
TEST_F(Setextattr, user)
{
	const char FULLPATH[] = "mountpoint/some_file.txt";
	const char RELPATH[] = "some_file.txt";
	uint64_t ino = 42;
	const char value[] = "whatever";
	ssize_t value_len = strlen(value) + 1;
	int ns = EXTATTR_NAMESPACE_USER;
	ssize_t r;

	expect_getattr(FUSE_ROOT_ID, S_IFDIR | 0755, UINT64_MAX, 1);
	expect_lookup(RELPATH, ino, S_IFREG | 0666, UINT64_MAX, 0);
	expect_setxattr(0);

	r = extattr_set_file(FULLPATH, ns, "foo", (const void*)value,
		value_len);
	ASSERT_EQ(value_len, r) << strerror(errno);
}

TEST_F(Unlink, ok)
{
	const char FULLPATH[] = "mountpoint/some_file.txt";
	const char RELPATH[] = "some_file.txt";
	uint64_t ino = 42;
	sem_t sem;

	ASSERT_EQ(0, sem_init(&sem, 0, 0)) << strerror(errno);

	expect_getattr(FUSE_ROOT_ID, S_IFDIR | 0777, UINT64_MAX, 1);
	expect_lookup(RELPATH, ino, S_IFREG | 0644, UINT64_MAX, geteuid());
	expect_unlink(FUSE_ROOT_ID, RELPATH, 0);
	expect_forget(ino, 1, &sem);

	ASSERT_EQ(0, unlink(FULLPATH)) << strerror(errno);

	sem_wait(&sem);
	sem_destroy(&sem);
}

/*
 * Ensure that a cached name doesn't cause unlink to bypass permission checks
 * in VOP_LOOKUP.
 *
 * This test should pass because lookup(9) purges the namecache entry by doing
 * a vfs_cache_lookup with ~MAKEENTRY when nameiop == DELETE.
 */
TEST_F(Unlink, cached_unwritable_directory)
{
	const char FULLPATH[] = "mountpoint/some_file.txt";
	const char RELPATH[] = "some_file.txt";
	uint64_t ino = 42;

	expect_getattr(FUSE_ROOT_ID, S_IFDIR | 0755, UINT64_MAX, 1);
	EXPECT_LOOKUP(FUSE_ROOT_ID, RELPATH)
	.Times(AnyNumber())
	.WillRepeatedly(Invoke(
		ReturnImmediate([=](auto i __unused, auto& out) {
			SET_OUT_HEADER_LEN(out, entry);
			out.body.entry.attr.mode = S_IFREG | 0644;
			out.body.entry.nodeid = ino;
			out.body.entry.entry_valid = UINT64_MAX;
		}))
	);

	/* Fill name cache */
	ASSERT_EQ(0, access(FULLPATH, F_OK)) << strerror(errno);
	/* Despite cached name , unlink should fail */
	ASSERT_EQ(-1, unlink(FULLPATH));
	ASSERT_EQ(EACCES, errno);
}

TEST_F(Unlink, unwritable_directory)
{
	const char FULLPATH[] = "mountpoint/some_file.txt";
	const char RELPATH[] = "some_file.txt";
	uint64_t ino = 42;

	expect_getattr(FUSE_ROOT_ID, S_IFDIR | 0755, UINT64_MAX, 1);
	expect_lookup(RELPATH, ino, S_IFREG | 0644, UINT64_MAX, geteuid());

	ASSERT_EQ(-1, unlink(FULLPATH));
	ASSERT_EQ(EACCES, errno);
}

TEST_F(Unlink, sticky_directory)
{
	const char FULLPATH[] = "mountpoint/some_file.txt";
	const char RELPATH[] = "some_file.txt";
	uint64_t ino = 42;

	expect_getattr(FUSE_ROOT_ID, S_IFDIR | 01777, UINT64_MAX, 1);
	expect_lookup(RELPATH, ino, S_IFREG | 0644, UINT64_MAX, 0);

	ASSERT_EQ(-1, unlink(FULLPATH));
	ASSERT_EQ(EPERM, errno);
}

/* A write by a non-owner should clear a file's SUID bit */
TEST_F(Write, clear_suid)
{
	const char FULLPATH[] = "mountpoint/some_file.txt";
	const char RELPATH[] = "some_file.txt";
	struct stat sb;
	uint64_t ino = 42;
	mode_t oldmode = 04777;
	mode_t newmode = 0777;
	char wbuf[1] = {'x'};
	int fd;

	expect_getattr(FUSE_ROOT_ID, S_IFDIR | 0755, UINT64_MAX, 1);
	expect_lookup(RELPATH, ino, S_IFREG | oldmode, UINT64_MAX);
	expect_open(ino, 0, 1);
	expect_write(ino, 0, sizeof(wbuf), sizeof(wbuf), 0, 0, wbuf);
	expect_chmod(ino, newmode, sizeof(wbuf));

	fd = open(FULLPATH, O_WRONLY);
	ASSERT_LE(0, fd) << strerror(errno);
	ASSERT_EQ(1, write(fd, wbuf, sizeof(wbuf))) << strerror(errno);
	ASSERT_EQ(0, fstat(fd, &sb)) << strerror(errno);
	EXPECT_EQ(S_IFREG | newmode, sb.st_mode);
	leak(fd);
}

/* A write by a non-owner should clear a file's SGID bit */
TEST_F(Write, clear_sgid)
{
	const char FULLPATH[] = "mountpoint/some_file.txt";
	const char RELPATH[] = "some_file.txt";
	struct stat sb;
	uint64_t ino = 42;
	mode_t oldmode = 02777;
	mode_t newmode = 0777;
	char wbuf[1] = {'x'};
	int fd;

	expect_getattr(FUSE_ROOT_ID, S_IFDIR | 0755, UINT64_MAX, 1);
	expect_lookup(RELPATH, ino, S_IFREG | oldmode, UINT64_MAX);
	expect_open(ino, 0, 1);
	expect_write(ino, 0, sizeof(wbuf), sizeof(wbuf), 0, 0, wbuf);
	expect_chmod(ino, newmode, sizeof(wbuf));

	fd = open(FULLPATH, O_WRONLY);
	ASSERT_LE(0, fd) << strerror(errno);
	ASSERT_EQ(1, write(fd, wbuf, sizeof(wbuf))) << strerror(errno);
	ASSERT_EQ(0, fstat(fd, &sb)) << strerror(errno);
	EXPECT_EQ(S_IFREG | newmode, sb.st_mode);
	leak(fd);
}

/* Regression test for a specific recurse-of-nonrecursive-lock panic
 *
 * With writeback caching, we can't call vtruncbuf from fuse_io_strategy, or it
 * may panic.  That happens if the FUSE_SETATTR response indicates that the
 * file's size has changed since the write.
 */
TEST_F(Write, recursion_panic_while_clearing_suid)
{
	const char FULLPATH[] = "mountpoint/some_file.txt";
	const char RELPATH[] = "some_file.txt";
	uint64_t ino = 42;
	mode_t oldmode = 04777;
	mode_t newmode = 0777;
	char wbuf[1] = {'x'};
	int fd;

	expect_getattr(FUSE_ROOT_ID, S_IFDIR | 0755, UINT64_MAX, 1);
	expect_lookup(RELPATH, ino, S_IFREG | oldmode, UINT64_MAX);
	expect_open(ino, 0, 1);
	expect_write(ino, 0, sizeof(wbuf), sizeof(wbuf), 0, 0, wbuf);
	/* XXX Return a smaller file size than what we just wrote! */
	expect_chmod(ino, newmode, 0);

	fd = open(FULLPATH, O_WRONLY);
	ASSERT_LE(0, fd) << strerror(errno);
	ASSERT_EQ(1, write(fd, wbuf, sizeof(wbuf))) << strerror(errno);
	leak(fd);
}
