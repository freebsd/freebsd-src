/*-
 * Copyright (c) 2019 The FreeBSD Foundation
 * All rights reserved.
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

class Unlink: public FuseTest {
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

void expect_lookup(const char *relpath, uint64_t ino, int times, int nlink=1)
{
	EXPECT_LOOKUP(FUSE_ROOT_ID, relpath)
	.Times(times)
	.WillRepeatedly(Invoke(
		ReturnImmediate([=](auto in __unused, auto& out) {
		SET_OUT_HEADER_LEN(out, entry);
		out.body.entry.attr.mode = S_IFREG | 0644;
		out.body.entry.nodeid = ino;
		out.body.entry.attr.nlink = nlink;
		out.body.entry.attr_valid = UINT64_MAX;
		out.body.entry.attr.size = 0;
	})));
}

};

/*
 * Unlinking a multiply linked file should update its ctime and nlink.  This
 * could be handled simply by invalidating the attributes, necessitating a new
 * GETATTR, but we implement it in-kernel for efficiency's sake.
 */
TEST_F(Unlink, attr_cache)
{
	const char FULLPATH0[] = "mountpoint/some_file.txt";
	const char RELPATH0[] = "some_file.txt";
	const char FULLPATH1[] = "mountpoint/other_file.txt";
	const char RELPATH1[] = "other_file.txt";
	uint64_t ino = 42;
	struct stat sb_old, sb_new;
	int fd1;

	expect_getattr(1, S_IFDIR | 0755);
	expect_lookup(RELPATH0, ino, 1, 2);
	expect_lookup(RELPATH1, ino, 1, 2);
	expect_open(ino, 0, 1);
	expect_unlink(1, RELPATH0, 0);

	fd1 = open(FULLPATH1, O_RDONLY);
	ASSERT_LE(0, fd1) << strerror(errno);

	ASSERT_EQ(0, fstat(fd1, &sb_old)) << strerror(errno);
	ASSERT_EQ(0, unlink(FULLPATH0)) << strerror(errno);
	ASSERT_EQ(0, fstat(fd1, &sb_new)) << strerror(errno);
	EXPECT_NE(sb_old.st_ctime, sb_new.st_ctime);
	EXPECT_EQ(1u, sb_new.st_nlink);

	leak(fd1);
}

/*
 * A successful unlink should clear the parent directory's attribute cache,
 * because the fuse daemon should update its mtime and ctime
 */
TEST_F(Unlink, parent_attr_cache)
{
	const char FULLPATH[] = "mountpoint/some_file.txt";
	const char RELPATH[] = "some_file.txt";
	struct stat sb;
	uint64_t ino = 42;

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
	/* Use nlink=2 so we don't get a FUSE_FORGET */
	expect_lookup(RELPATH, ino, 1, 2);
	expect_unlink(1, RELPATH, 0);

	ASSERT_EQ(0, unlink(FULLPATH)) << strerror(errno);
	EXPECT_EQ(0, stat("mountpoint", &sb)) << strerror(errno);
}

TEST_F(Unlink, eperm)
{
	const char FULLPATH[] = "mountpoint/some_file.txt";
	const char RELPATH[] = "some_file.txt";
	uint64_t ino = 42;

	expect_getattr(1, S_IFDIR | 0755);
	expect_lookup(RELPATH, ino, 1);
	expect_unlink(1, RELPATH, EPERM);

	ASSERT_NE(0, unlink(FULLPATH));
	ASSERT_EQ(EPERM, errno);
}

/*
 * Unlinking a file should expire its entry cache, even if it's multiply linked
 */
TEST_F(Unlink, entry_cache)
{
	const char FULLPATH[] = "mountpoint/some_file.txt";
	const char RELPATH[] = "some_file.txt";
	uint64_t ino = 42;

	expect_getattr(1, S_IFDIR | 0755);
	expect_lookup(RELPATH, ino, 2, 2);
	expect_unlink(1, RELPATH, 0);

	ASSERT_EQ(0, unlink(FULLPATH)) << strerror(errno);
	ASSERT_EQ(0, access(FULLPATH, F_OK)) << strerror(errno);
}

/*
 * Unlink a multiply-linked file.  There should be no FUSE_FORGET because the
 * file is still linked.
 */
TEST_F(Unlink, multiply_linked)
{
	const char FULLPATH0[] = "mountpoint/some_file.txt";
	const char RELPATH0[] = "some_file.txt";
	const char FULLPATH1[] = "mountpoint/other_file.txt";
	const char RELPATH1[] = "other_file.txt";
	uint64_t ino = 42;

	expect_getattr(1, S_IFDIR | 0755);
	expect_lookup(RELPATH0, ino, 1, 2);
	expect_unlink(1, RELPATH0, 0);
	EXPECT_CALL(*m_mock, process(
		ResultOf([=](auto in) {
			return (in.header.opcode == FUSE_FORGET &&
				in.header.nodeid == ino);
		}, Eq(true)),
		_)
	).Times(0);
	expect_lookup(RELPATH1, ino, 1, 1);

	ASSERT_EQ(0, unlink(FULLPATH0)) << strerror(errno);

	/* 
	 * The final syscall simply ensures that no FUSE_FORGET was ever sent,
	 * by scheduling an arbitrary different operation after a FUSE_FORGET
	 * would've been sent.
	 */
	ASSERT_EQ(0, access(FULLPATH1, F_OK)) << strerror(errno);
}

TEST_F(Unlink, ok)
{
	const char FULLPATH[] = "mountpoint/some_file.txt";
	const char RELPATH[] = "some_file.txt";
	uint64_t ino = 42;
	sem_t sem;

	ASSERT_EQ(0, sem_init(&sem, 0, 0)) << strerror(errno);

	expect_getattr(1, S_IFDIR | 0755);
	expect_lookup(RELPATH, ino, 1);
	expect_unlink(1, RELPATH, 0);
	expect_forget(ino, 1, &sem);

	ASSERT_EQ(0, unlink(FULLPATH)) << strerror(errno);
	sem_wait(&sem);
	sem_destroy(&sem);
}

/* Unlink an open file */
TEST_F(Unlink, open_but_deleted)
{
	const char FULLPATH0[] = "mountpoint/some_file.txt";
	const char RELPATH0[] = "some_file.txt";
	const char FULLPATH1[] = "mountpoint/other_file.txt";
	const char RELPATH1[] = "other_file.txt";
	uint64_t ino = 42;
	int fd;

	expect_getattr(1, S_IFDIR | 0755);
	expect_lookup(RELPATH0, ino, 2);
	expect_open(ino, 0, 1);
	expect_unlink(1, RELPATH0, 0);
	expect_lookup(RELPATH1, ino, 1, 1);

	fd = open(FULLPATH0, O_RDWR);
	ASSERT_LE(0, fd) << strerror(errno);
	ASSERT_EQ(0, unlink(FULLPATH0)) << strerror(errno);

	/* 
	 * The final syscall simply ensures that no FUSE_FORGET was ever sent,
	 * by scheduling an arbitrary different operation after a FUSE_FORGET
	 * would've been sent.
	 */
	ASSERT_EQ(0, access(FULLPATH1, F_OK)) << strerror(errno);
	leak(fd);
}
