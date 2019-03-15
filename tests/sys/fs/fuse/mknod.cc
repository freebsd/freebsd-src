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

class Mknod: public FuseTest {

public:

virtual void SetUp() {
	if (geteuid() != 0) {
		// TODO: With GoogleTest 1.8.2, use SKIP instead
		FAIL() << "Only root may use most mknod(2) variations";
	}
	FuseTest::SetUp();
}

/* Test an OK creation of a file with the given mode and device number */
void test_ok(mode_t mode, dev_t dev) {
	const char FULLPATH[] = "mountpoint/some_file.txt";
	const char RELPATH[] = "some_file.txt";
	uint64_t ino = 42;

	EXPECT_LOOKUP(1, RELPATH).WillOnce(Invoke(ReturnErrno(ENOENT)));

	EXPECT_CALL(*m_mock, process(
		ResultOf([=](auto in) {
			const char *name = (const char*)in->body.bytes +
				sizeof(fuse_mknod_in);
			return (in->header.opcode == FUSE_MKNOD &&
				in->body.mknod.mode == mode &&
				in->body.mknod.rdev == dev &&
				(0 == strcmp(RELPATH, name)));
		}, Eq(true)),
		_)
	).WillOnce(Invoke(ReturnImmediate([=](auto in __unused, auto out) {
		SET_OUT_HEADER_LEN(out, create);
		out->body.create.entry.attr.mode = mode;
		out->body.create.entry.nodeid = ino;
		out->body.create.entry.entry_valid = UINT64_MAX;
		out->body.create.entry.attr_valid = UINT64_MAX;
		out->body.create.entry.attr.rdev = dev;
	})));
	EXPECT_EQ(0, mknod(FULLPATH, mode, dev)) << strerror(errno);
}

};

/* 
 * mknod(2) should be able to create block devices on a FUSE filesystem.  Even
 * though FreeBSD doesn't use block devices, this is useful when copying media
 * from or preparing media for other operating systems.
 */
/* https://bugs.freebsd.org/bugzilla/show_bug.cgi?id=236236 */
TEST_F(Mknod, DISABLED_blk)
{
	test_ok(S_IFBLK | 0755, 0xfe00); /* /dev/vda's device number on Linux */
}

/* https://bugs.freebsd.org/bugzilla/show_bug.cgi?id=236236 */
TEST_F(Mknod, DISABLED_chr)
{
	test_ok(S_IFCHR | 0755, 0x64);	/* /dev/fuse's device number */
}

/* 
 * The daemon is responsible for checking file permissions (unless the
 * default_permissions mount option was used)
 */
/* https://bugs.freebsd.org/bugzilla/show_bug.cgi?id=236236 */
TEST_F(Mknod, DISABLED_eperm)
{
	const char FULLPATH[] = "mountpoint/some_file.txt";
	const char RELPATH[] = "some_file.txt";
	mode_t mode = S_IFIFO | 0755;

	EXPECT_LOOKUP(1, RELPATH).WillOnce(Invoke(ReturnErrno(ENOENT)));

	EXPECT_CALL(*m_mock, process(
		ResultOf([=](auto in) {
			const char *name = (const char*)in->body.bytes +
				sizeof(fuse_mknod_in);
			return (in->header.opcode == FUSE_MKNOD &&
				in->body.mknod.mode == mode &&
				(0 == strcmp(RELPATH, name)));
		}, Eq(true)),
		_)
	).WillOnce(Invoke(ReturnErrno(EPERM)));
	EXPECT_NE(0, mknod(FULLPATH, mode, 0));
	EXPECT_EQ(EPERM, errno);
}


/* https://bugs.freebsd.org/bugzilla/show_bug.cgi?id=236236 */
TEST_F(Mknod, DISABLED_fifo)
{
	test_ok(S_IFIFO | 0755, 0);
}

/* https://bugs.freebsd.org/bugzilla/show_bug.cgi?id=236236 */
TEST_F(Mknod, DISABLED_whiteout)
{
	test_ok(S_IFWHT | 0755, 0);
}
