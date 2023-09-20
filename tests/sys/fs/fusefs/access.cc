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
#include <sys/types.h>
#include <sys/extattr.h>

#include <fcntl.h>
#include <unistd.h>
}

#include "mockfs.hh"
#include "utils.hh"

using namespace testing;

class Access: public FuseTest {
public:
virtual void SetUp() {
	FuseTest::SetUp();
	// Clear the default FUSE_ACCESS expectation
	Mock::VerifyAndClearExpectations(m_mock);
}

void expect_lookup(const char *relpath, uint64_t ino)
{
	FuseTest::expect_lookup(relpath, ino, S_IFREG | 0644, 0, 1);
}

/* 
 * Expect that FUSE_ACCESS will never be called for the given inode, with any
 * bits in the supplied access_mask set
 */
void expect_noaccess(uint64_t ino, mode_t access_mask)
{
	EXPECT_CALL(*m_mock, process(
		ResultOf([=](auto in) {
			return (in.header.opcode == FUSE_ACCESS &&
				in.header.nodeid == ino &&
				in.body.access.mask & access_mask);
		}, Eq(true)),
		_)
	).Times(0);
}

};

class RofsAccess: public Access {
public:
virtual void SetUp() {
	m_ro = true;
	Access::SetUp();
}
};

/*
 * Change the mode of a file.
 *
 * There should never be a FUSE_ACCESS sent for this operation, except for
 * search permissions on the parent directory.
 * https://bugs.freebsd.org/bugzilla/show_bug.cgi?id=245689
 */
TEST_F(Access, chmod)
{
	const char FULLPATH[] = "mountpoint/some_file.txt";
	const char RELPATH[] = "some_file.txt";
	const uint64_t ino = 42;
	const mode_t newmode = 0644;

	expect_access(FUSE_ROOT_ID, X_OK, 0);
	expect_lookup(RELPATH, ino);
	expect_noaccess(ino, 0);
	EXPECT_CALL(*m_mock, process(
		ResultOf([](auto in) {
			return (in.header.opcode == FUSE_SETATTR &&
				in.header.nodeid == ino);
		}, Eq(true)),
		_)
	).WillOnce(Invoke(ReturnImmediate([](auto in __unused, auto& out) {
		SET_OUT_HEADER_LEN(out, attr);
		out.body.attr.attr.ino = ino;	// Must match nodeid
		out.body.attr.attr.mode = S_IFREG | newmode;
	})));

	EXPECT_EQ(0, chmod(FULLPATH, newmode)) << strerror(errno);
}

/*
 * Create a new file
 *
 * There should never be a FUSE_ACCESS sent for this operation, except for
 * search permissions on the parent directory.
 * https://bugs.freebsd.org/bugzilla/show_bug.cgi?id=245689
 */
TEST_F(Access, create)
{
	const char FULLPATH[] = "mountpoint/some_file.txt";
	const char RELPATH[] = "some_file.txt";
	mode_t mode = S_IFREG | 0755;
	uint64_t ino = 42;

	expect_access(FUSE_ROOT_ID, X_OK, 0);
	expect_noaccess(FUSE_ROOT_ID, R_OK | W_OK);
	EXPECT_LOOKUP(FUSE_ROOT_ID, RELPATH)
		.WillOnce(Invoke(ReturnErrno(ENOENT)));
	expect_noaccess(ino, 0);
	EXPECT_CALL(*m_mock, process(
		ResultOf([=](auto in) {
			return (in.header.opcode == FUSE_CREATE);
		}, Eq(true)),
		_)
	).WillOnce(ReturnErrno(EPERM));

	EXPECT_EQ(-1, open(FULLPATH, O_CREAT | O_EXCL, mode));
	EXPECT_EQ(EPERM, errno);
}

/* The error case of FUSE_ACCESS.  */
TEST_F(Access, eaccess)
{
	const char FULLPATH[] = "mountpoint/some_file.txt";
	const char RELPATH[] = "some_file.txt";
	uint64_t ino = 42;
	mode_t	access_mode = X_OK;

	expect_access(FUSE_ROOT_ID, X_OK, 0);
	expect_lookup(RELPATH, ino);
	expect_access(ino, access_mode, EACCES);

	ASSERT_NE(0, access(FULLPATH, access_mode));
	ASSERT_EQ(EACCES, errno);
}

/*
 * If the filesystem returns ENOSYS, then it is treated as a permanent success,
 * and subsequent VOP_ACCESS calls will succeed automatically without querying
 * the daemon.
 */
TEST_F(Access, enosys)
{
	const char FULLPATH[] = "mountpoint/some_file.txt";
	const char RELPATH[] = "some_file.txt";
	uint64_t ino = 42;
	mode_t	access_mode = R_OK;

	expect_access(FUSE_ROOT_ID, X_OK, ENOSYS);
	FuseTest::expect_lookup(RELPATH, ino, S_IFREG | 0644, 0, 2);

	ASSERT_EQ(0, access(FULLPATH, access_mode)) << strerror(errno);
	ASSERT_EQ(0, access(FULLPATH, access_mode)) << strerror(errno);
}

TEST_F(RofsAccess, erofs)
{
	const char FULLPATH[] = "mountpoint/some_file.txt";
	const char RELPATH[] = "some_file.txt";
	uint64_t ino = 42;
	mode_t	access_mode = W_OK;

	expect_access(FUSE_ROOT_ID, X_OK, 0);
	expect_lookup(RELPATH, ino);

	ASSERT_NE(0, access(FULLPATH, access_mode));
	ASSERT_EQ(EROFS, errno);
}


/*
 * Lookup an extended attribute
 *
 * There should never be a FUSE_ACCESS sent for this operation, except for
 * search permissions on the parent directory.
 * https://bugs.freebsd.org/bugzilla/show_bug.cgi?id=245689
 */
TEST_F(Access, Getxattr)
{
	const char FULLPATH[] = "mountpoint/some_file.txt";
	const char RELPATH[] = "some_file.txt";
	uint64_t ino = 42;
	char data[80];
	int ns = EXTATTR_NAMESPACE_USER;
	ssize_t r;

	expect_access(FUSE_ROOT_ID, X_OK, 0);
	expect_lookup(RELPATH, ino);
	expect_noaccess(ino, 0);
	expect_getxattr(ino, "user.foo", ReturnErrno(ENOATTR));

	r = extattr_get_file(FULLPATH, ns, "foo", data, sizeof(data));
	ASSERT_EQ(-1, r);
	ASSERT_EQ(ENOATTR, errno);
}

/* The successful case of FUSE_ACCESS.  */
TEST_F(Access, ok)
{
	const char FULLPATH[] = "mountpoint/some_file.txt";
	const char RELPATH[] = "some_file.txt";
	uint64_t ino = 42;
	mode_t	access_mode = R_OK;

	expect_access(FUSE_ROOT_ID, X_OK, 0);
	expect_lookup(RELPATH, ino);
	expect_access(ino, access_mode, 0);

	ASSERT_EQ(0, access(FULLPATH, access_mode)) << strerror(errno);
}

/*
 * Unlink a file
 *
 * There should never be a FUSE_ACCESS sent for this operation, except for
 * search permissions on the parent directory.
 * https://bugs.freebsd.org/bugzilla/show_bug.cgi?id=245689
 */
TEST_F(Access, unlink)
{
	const char FULLPATH[] = "mountpoint/some_file.txt";
	const char RELPATH[] = "some_file.txt";
	uint64_t ino = 42;

	expect_access(FUSE_ROOT_ID, X_OK, 0);
	expect_noaccess(FUSE_ROOT_ID, W_OK | R_OK);
	expect_noaccess(ino, 0);
	expect_lookup(RELPATH, ino);
	expect_unlink(1, RELPATH, EPERM);

	ASSERT_NE(0, unlink(FULLPATH));
	ASSERT_EQ(EPERM, errno);
}

/*
 * Unlink a file whose parent diretory's sticky bit is set
 *
 * There should never be a FUSE_ACCESS sent for this operation, except for
 * search permissions on the parent directory.
 * https://bugs.freebsd.org/bugzilla/show_bug.cgi?id=245689
 */
TEST_F(Access, unlink_sticky_directory)
{
	const char FULLPATH[] = "mountpoint/some_file.txt";
	const char RELPATH[] = "some_file.txt";
	uint64_t ino = 42;

	expect_access(FUSE_ROOT_ID, X_OK, 0);
	expect_noaccess(FUSE_ROOT_ID, W_OK | R_OK);
	expect_noaccess(ino, 0);
	EXPECT_CALL(*m_mock, process(
		ResultOf([=](auto in) {
			return (in.header.opcode == FUSE_GETATTR &&
				in.header.nodeid == FUSE_ROOT_ID);
		}, Eq(true)),
		_)
	).WillRepeatedly(Invoke(ReturnImmediate([=](auto i __unused, auto& out)
	{
		SET_OUT_HEADER_LEN(out, attr);
		out.body.attr.attr.ino = FUSE_ROOT_ID;
		out.body.attr.attr.mode = S_IFDIR | 01777;
		out.body.attr.attr.uid = 0;
		out.body.attr.attr_valid = UINT64_MAX;
	})));
	EXPECT_CALL(*m_mock, process(
		ResultOf([=](auto in) {
			return (in.header.opcode == FUSE_ACCESS &&
				in.header.nodeid == ino);
		}, Eq(true)),
		_)
	).Times(0);
	expect_lookup(RELPATH, ino);
	expect_unlink(FUSE_ROOT_ID, RELPATH, EPERM);

	ASSERT_EQ(-1, unlink(FULLPATH));
	ASSERT_EQ(EPERM, errno);
}
