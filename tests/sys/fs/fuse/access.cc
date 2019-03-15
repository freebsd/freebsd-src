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

extern "C" {
//#include <sys/param.h>
#include <fcntl.h>
#include <unistd.h>
}

#include "mockfs.hh"
#include "utils.hh"

using namespace testing;

class Access: public FuseTest {
public:
void expect_access(uint64_t ino, mode_t access_mode, int error)
{
	EXPECT_CALL(*m_mock, process(
		ResultOf([=](auto in) {
			return (in->header.opcode == FUSE_ACCESS &&
				in->header.nodeid == ino &&
				in->body.access.mask == access_mode);
		}, Eq(true)),
		_)
	).WillOnce(Invoke(ReturnErrno(error)));
}

void expect_lookup(const char *relpath, uint64_t ino)
{
	FuseTest::expect_lookup(relpath, ino, S_IFREG | 0644, 1);
}
};

/* TODO: test methods for the default_permissions mount option */

/* The error case of FUSE_ACCESS.  */
/* https://bugs.freebsd.org/bugzilla/show_bug.cgi?id=236291 */
TEST_F(Access, DISABLED_eaccess)
{
	const char FULLPATH[] = "mountpoint/some_file.txt";
	const char RELPATH[] = "some_file.txt";
	uint64_t ino = 42;
	mode_t	access_mode = X_OK;

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
/* https://bugs.freebsd.org/bugzilla/show_bug.cgi?id=236557 */
/* https://bugs.freebsd.org/bugzilla/show_bug.cgi?id=236291 */
TEST_F(Access, DISABLED_enosys)
{
	const char FULLPATH[] = "mountpoint/some_file.txt";
	const char RELPATH[] = "some_file.txt";
	uint64_t ino = 42;
	mode_t	access_mode = R_OK;

	expect_lookup(RELPATH, ino);
	expect_access(ino, access_mode, ENOSYS);

	ASSERT_EQ(0, access(FULLPATH, access_mode)) << strerror(errno);
	ASSERT_EQ(0, access(FULLPATH, access_mode)) << strerror(errno);
}

/* The successful case of FUSE_ACCESS.  */
/* https://bugs.freebsd.org/bugzilla/show_bug.cgi?id=236291 */
TEST_F(Access, DISABLED_ok)
{
	const char FULLPATH[] = "mountpoint/some_file.txt";
	const char RELPATH[] = "some_file.txt";
	uint64_t ino = 42;
	mode_t	access_mode = R_OK;

	expect_lookup(RELPATH, ino);
	expect_access(ino, access_mode, 0);

	ASSERT_EQ(0, access(FULLPATH, access_mode)) << strerror(errno);
}
