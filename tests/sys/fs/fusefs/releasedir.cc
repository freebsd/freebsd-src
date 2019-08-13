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

extern "C" {
#include <dirent.h>
#include <fcntl.h>
}

#include "mockfs.hh"
#include "utils.hh"

using namespace testing;

class ReleaseDir: public FuseTest {

public:
void expect_lookup(const char *relpath, uint64_t ino)
{
	FuseTest::expect_lookup(relpath, ino, S_IFDIR | 0755, 0, 1);
}
};

/* If a file descriptor is duplicated, only the last close causes RELEASE */
TEST_F(ReleaseDir, dup)
{
	const char FULLPATH[] = "mountpoint/some_file.txt";
	const char RELPATH[] = "some_file.txt";
	uint64_t ino = 42;
	DIR *dir, *dir2;

	expect_lookup(RELPATH, ino);
	expect_opendir(ino);
	EXPECT_CALL(*m_mock, process(
		ResultOf([=](auto in) {
			return (in.header.opcode == FUSE_READDIR &&
				in.header.nodeid == ino &&
				in.body.readdir.offset == 0);
		}, Eq(true)),
		_)
	).WillOnce(Invoke(ReturnImmediate([=](auto in __unused, auto& out) {
		out.header.error = 0;
		out.header.len = sizeof(out.header);
	})));
	expect_releasedir(ino, ReturnErrno(0));
	
	dir = opendir(FULLPATH);
	ASSERT_NE(nullptr, dir) << strerror(errno);

	dir2 = fdopendir(dup(dirfd(dir)));
	ASSERT_NE(nullptr, dir2) << strerror(errno);

	ASSERT_EQ(0, closedir(dir)) << strerror(errno);
	ASSERT_EQ(0, closedir(dir2)) << strerror(errno);
}

TEST_F(ReleaseDir, ok)
{
	const char FULLPATH[] = "mountpoint/some_dir";
	const char RELPATH[] = "some_dir";
	uint64_t ino = 42;
	DIR *dir;

	expect_lookup(RELPATH, ino);
	expect_opendir(ino);
	expect_releasedir(ino, ReturnErrno(0));
	
	dir = opendir(FULLPATH);
	ASSERT_NE(nullptr, dir) << strerror(errno);

	ASSERT_EQ(0, closedir(dir)) << strerror(errno);
}

/* Directories opened O_EXEC should be properly released, too */
TEST_F(ReleaseDir, o_exec)
{
	const char FULLPATH[] = "mountpoint/some_dir";
	const char RELPATH[] = "some_dir";
	uint64_t ino = 42;
	int fd;

	expect_lookup(RELPATH, ino);
	expect_opendir(ino);
	expect_releasedir(ino, ReturnErrno(0));
	
	fd = open(FULLPATH, O_EXEC | O_DIRECTORY);
	EXPECT_LE(0, fd) << strerror(errno);

	ASSERT_EQ(0, close(fd)) << strerror(errno);
}
