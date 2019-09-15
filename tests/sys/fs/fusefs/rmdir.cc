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
#include <fcntl.h>
#include <semaphore.h>
}

#include "mockfs.hh"
#include "utils.hh"

using namespace testing;

class Rmdir: public FuseTest {
public:
void expect_getattr(uint64_t ino, mode_t mode)
{
	EXPECT_CALL(*m_mock, process(
		ResultOf([=](auto in) {
			return (in.header.opcode == FUSE_GETATTR &&
				in.header.nodeid == ino);
		}, Eq(true)),
		_)
	).WillOnce(Invoke(ReturnImmediate([=](auto i __unused, auto& out) {
		SET_OUT_HEADER_LEN(out, attr);
		out.body.attr.attr.ino = ino;	// Must match nodeid
		out.body.attr.attr.mode = mode;
		out.body.attr.attr_valid = UINT64_MAX;
	})));
}

void expect_lookup(const char *relpath, uint64_t ino, int times=1)
{
	EXPECT_LOOKUP(FUSE_ROOT_ID, relpath)
	.Times(times)
	.WillRepeatedly(Invoke(ReturnImmediate([=](auto in __unused, auto& out) {
		SET_OUT_HEADER_LEN(out, entry);
		out.body.entry.attr_valid = UINT64_MAX;
		out.body.entry.attr.mode = S_IFDIR | 0755;
		out.body.entry.nodeid = ino;
		out.body.entry.attr.nlink = 2;
	})));
}

void expect_rmdir(uint64_t parent, const char *relpath, int error)
{
	EXPECT_CALL(*m_mock, process(
		ResultOf([=](auto in) {
			return (in.header.opcode == FUSE_RMDIR &&
				0 == strcmp(relpath, in.body.rmdir) &&
				in.header.nodeid == parent);
		}, Eq(true)),
		_)
	).WillOnce(Invoke(ReturnErrno(error)));
}
};

/*
 * A successful rmdir should clear the parent directory's attribute cache,
 * because the fuse daemon should update its mtime and ctime
 */
TEST_F(Rmdir, parent_attr_cache)
{
	const char FULLPATH[] = "mountpoint/some_dir";
	const char RELPATH[] = "some_dir";
	struct stat sb;
	sem_t sem;
	uint64_t ino = 42;

	ASSERT_EQ(0, sem_init(&sem, 0, 0)) << strerror(errno);

	EXPECT_CALL(*m_mock, process(
		ResultOf([=](auto in) {
			return (in.header.opcode == FUSE_GETATTR &&
				in.header.nodeid == FUSE_ROOT_ID);
		}, Eq(true)),
		_)
	).Times(2)
	.WillRepeatedly(Invoke(ReturnImmediate([=](auto i __unused, auto& out) {
		SET_OUT_HEADER_LEN(out, attr);
		out.body.attr.attr.ino = ino;	// Must match nodeid
		out.body.attr.attr.mode = S_IFDIR | 0755;
		out.body.attr.attr_valid = UINT64_MAX;
	})));
	expect_lookup(RELPATH, ino);
	expect_rmdir(FUSE_ROOT_ID, RELPATH, 0);
	expect_forget(ino, 1, &sem);

	ASSERT_EQ(0, rmdir(FULLPATH)) << strerror(errno);
	EXPECT_EQ(0, stat("mountpoint", &sb)) << strerror(errno);
	sem_wait(&sem);
	sem_destroy(&sem);
}

TEST_F(Rmdir, enotempty)
{
	const char FULLPATH[] = "mountpoint/some_dir";
	const char RELPATH[] = "some_dir";
	uint64_t ino = 42;

	expect_getattr(FUSE_ROOT_ID, S_IFDIR | 0755);
	expect_lookup(RELPATH, ino);
	expect_rmdir(FUSE_ROOT_ID, RELPATH, ENOTEMPTY);

	ASSERT_NE(0, rmdir(FULLPATH));
	ASSERT_EQ(ENOTEMPTY, errno);
}

/* Removing a directory should expire its entry cache */
TEST_F(Rmdir, entry_cache)
{
	const char FULLPATH[] = "mountpoint/some_dir";
	const char RELPATH[] = "some_dir";
	sem_t sem;
	uint64_t ino = 42;

	expect_getattr(1, S_IFDIR | 0755);
	expect_lookup(RELPATH, ino, 2);
	expect_rmdir(FUSE_ROOT_ID, RELPATH, 0);
	expect_forget(ino, 1, &sem);

	ASSERT_EQ(0, rmdir(FULLPATH)) << strerror(errno);
	ASSERT_EQ(0, access(FULLPATH, F_OK)) << strerror(errno);
	sem_wait(&sem);
	sem_destroy(&sem);
}

TEST_F(Rmdir, ok)
{
	const char FULLPATH[] = "mountpoint/some_dir";
	const char RELPATH[] = "some_dir";
	sem_t sem;
	uint64_t ino = 42;

	ASSERT_EQ(0, sem_init(&sem, 0, 0)) << strerror(errno);

	expect_getattr(FUSE_ROOT_ID, S_IFDIR | 0755);
	expect_lookup(RELPATH, ino);
	expect_rmdir(FUSE_ROOT_ID, RELPATH, 0);
	expect_forget(ino, 1, &sem);

	ASSERT_EQ(0, rmdir(FULLPATH)) << strerror(errno);
	sem_wait(&sem);
	sem_destroy(&sem);
}
