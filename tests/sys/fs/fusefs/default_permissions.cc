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

/*
 * Tests for the "default_permissions" mount option.  They must be in their own
 * file so they can be run as an unprivileged user
 */

extern "C" {
#include <fcntl.h>
#include <unistd.h>
}

#include "mockfs.hh"
#include "utils.hh"

using namespace testing;

class DefaultPermissions: public FuseTest {

virtual void SetUp() {
	FuseTest::SetUp();

	if (geteuid() == 0) {
		GTEST_SKIP() << "This test requires an unprivileged user";
	}
	m_default_permissions = true;
}

public:
void expect_lookup(const char *relpath, uint64_t ino, mode_t mode)
{
	FuseTest::expect_lookup(relpath, ino, S_IFREG | mode, 0, 1);
}

};

class Access: public DefaultPermissions {};
class Open: public DefaultPermissions {};

/* https://bugs.freebsd.org/bugzilla/show_bug.cgi?id=216391 */
TEST_F(Access, DISABLED_eaccess)
{
	const char FULLPATH[] = "mountpoint/some_file.txt";
	const char RELPATH[] = "some_file.txt";
	uint64_t ino = 42;
	mode_t	access_mode = X_OK;

	expect_lookup(RELPATH, ino, S_IFREG | 0644);
	/* 
	 * Once default_permissions is properly implemented, there might be
	 * another FUSE_GETATTR or something in here.  But there should not be
	 * a FUSE_ACCESS
	 */

	ASSERT_NE(0, access(FULLPATH, access_mode));
	ASSERT_EQ(EACCES, errno);
}

/* https://bugs.freebsd.org/bugzilla/show_bug.cgi?id=236291 */
TEST_F(Access, DISABLED_ok)
{
	const char FULLPATH[] = "mountpoint/some_file.txt";
	const char RELPATH[] = "some_file.txt";
	uint64_t ino = 42;
	mode_t	access_mode = R_OK;

	expect_lookup(RELPATH, ino, S_IFREG | 0644);
	expect_access(ino, access_mode, 0);
	/* 
	 * Once default_permissions is properly implemented, there might be
	 * another FUSE_GETATTR or something in here.
	 */

	ASSERT_EQ(0, access(FULLPATH, access_mode)) << strerror(errno);
}

TEST_F(Open, ok)
{
	const char FULLPATH[] = "mountpoint/some_file.txt";
	const char RELPATH[] = "some_file.txt";
	uint64_t ino = 42;
	int fd;

	expect_lookup(RELPATH, ino, S_IFREG | 0644);
	expect_open(ino, 0, 1);
	/* Until the attr cache is working, we may send an additional GETATTR */
	expect_getattr(ino, 0);

	fd = open(FULLPATH, O_RDONLY);
	EXPECT_LE(0, fd) << strerror(errno);
	/* Deliberately leak fd.  close(2) will be tested in release.cc */
}

/* https://bugs.freebsd.org/bugzilla/show_bug.cgi?id=216391 */
TEST_F(Open, DISABLED_eperm)
{
	const char FULLPATH[] = "mountpoint/some_file.txt";
	const char RELPATH[] = "some_file.txt";
	uint64_t ino = 42;

	expect_lookup(RELPATH, ino, S_IFREG | 0644);
	/* 
	 * Once default_permissions is properly implemented, there might be
	 * another FUSE_GETATTR or something in here.  But there should not be
	 * a FUSE_ACCESS
	 */

	EXPECT_NE(0, open(FULLPATH, O_RDWR));
	EXPECT_EQ(EPERM, errno);
}
