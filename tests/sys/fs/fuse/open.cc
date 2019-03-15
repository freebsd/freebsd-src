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
#include <fcntl.h>
}

#include "mockfs.hh"
#include "utils.hh"

using namespace testing;

class Open: public FuseTest {

public:

/* Test an OK open of a file with the given flags */
void test_ok(int os_flags, int fuse_flags) {
	const char FULLPATH[] = "mountpoint/some_file.txt";
	const char RELPATH[] = "some_file.txt";
	uint64_t ino = 42;
	int fd;

	FuseTest::expect_lookup(RELPATH, ino, S_IFREG | 0644, 1);
	EXPECT_CALL(*m_mock, process(
		ResultOf([=](auto in) {
			return (in->header.opcode == FUSE_OPEN &&
				in->body.open.flags == (uint32_t)fuse_flags &&
				in->header.nodeid == ino);
		}, Eq(true)),
		_)
	).WillOnce(Invoke(ReturnImmediate([](auto in __unused, auto out) {
		out->header.len = sizeof(out->header);
		SET_OUT_HEADER_LEN(out, open);
	})));

	/* Until the attr cache is working, we may send an additional GETATTR */
	EXPECT_CALL(*m_mock, process(
		ResultOf([=](auto in) {
			return (in->header.opcode == FUSE_GETATTR &&
				in->header.nodeid == ino);
		}, Eq(true)),
		_)
	).WillRepeatedly(Invoke(ReturnImmediate([=](auto i __unused, auto out) {
		SET_OUT_HEADER_LEN(out, attr);
		out->body.attr.attr.ino = ino;	// Must match nodeid
		out->body.attr.attr.mode = S_IFREG | 0644;
	})));

	fd = open(FULLPATH, os_flags);
	EXPECT_LE(0, fd) << strerror(errno);
	/* Deliberately leak fd.  close(2) will be tested in release.cc */
}
};


/* 
 * The fuse daemon fails the request with enoent.  This usually indicates a
 * race condition: some other FUSE client removed the file in between when the
 * kernel checked for it with lookup and tried to open it
 */
TEST_F(Open, enoent)
{
	const char FULLPATH[] = "mountpoint/some_file.txt";
	const char RELPATH[] = "some_file.txt";
	uint64_t ino = 42;

	expect_lookup(RELPATH, ino, S_IFREG | 0644, 1);
	EXPECT_CALL(*m_mock, process(
		ResultOf([=](auto in) {
			return (in->header.opcode == FUSE_OPEN &&
				in->header.nodeid == ino);
		}, Eq(true)),
		_)
	).WillOnce(Invoke(ReturnErrno(ENOENT)));
	EXPECT_NE(0, open(FULLPATH, O_RDONLY));
	EXPECT_EQ(ENOENT, errno);
}

/* 
 * The daemon is responsible for checking file permissions (unless the
 * default_permissions mount option was used)
 */
TEST_F(Open, eperm)
{
	const char FULLPATH[] = "mountpoint/some_file.txt";
	const char RELPATH[] = "some_file.txt";
	uint64_t ino = 42;

	expect_lookup(RELPATH, ino, S_IFREG | 0644, 1);
	EXPECT_CALL(*m_mock, process(
		ResultOf([=](auto in) {
			return (in->header.opcode == FUSE_OPEN &&
				in->header.nodeid == ino);
		}, Eq(true)),
		_)
	).WillOnce(Invoke(ReturnErrno(EPERM)));
	EXPECT_NE(0, open(FULLPATH, O_RDONLY));
	EXPECT_EQ(EPERM, errno);
}

/* https://bugs.freebsd.org/bugzilla/show_bug.cgi?id=236340 */
TEST_F(Open, DISABLED_o_append)
{
	test_ok(O_WRONLY | O_APPEND, O_WRONLY | O_APPEND);
}

/* The kernel is supposed to filter out this flag */
TEST_F(Open, o_creat)
{
	test_ok(O_WRONLY | O_CREAT, O_WRONLY);
}

/* https://bugs.freebsd.org/bugzilla/show_bug.cgi?id=236340 */
TEST_F(Open, DISABLED_o_direct)
{
	test_ok(O_WRONLY | O_DIRECT, O_WRONLY | O_DIRECT);
}

/* The kernel is supposed to filter out this flag */
TEST_F(Open, o_excl)
{
	test_ok(O_WRONLY | O_EXCL, O_WRONLY);
}

/* https://bugs.freebsd.org/bugzilla/show_bug.cgi?id=236329 */
TEST_F(Open, DISABLED_o_exec)
{
	test_ok(O_EXEC, O_EXEC);
}

/* The kernel is supposed to filter out this flag */
TEST_F(Open, o_noctty)
{
	test_ok(O_WRONLY | O_NOCTTY, O_WRONLY);
}

TEST_F(Open, o_rdonly)
{
	test_ok(O_RDONLY, O_RDONLY);
}

/* https://bugs.freebsd.org/bugzilla/show_bug.cgi?id=236340 */
TEST_F(Open, DISABLED_o_trunc)
{
	test_ok(O_WRONLY | O_TRUNC, O_WRONLY | O_TRUNC);
}

TEST_F(Open, o_wronly)
{
	test_ok(O_WRONLY, O_WRONLY);
}

TEST_F(Open, o_rdwr)
{
	test_ok(O_RDWR, O_RDWR);
}

