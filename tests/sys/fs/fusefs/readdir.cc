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
using namespace std;

class Readdir: public FuseTest {
public:
void expect_lookup(const char *relpath, uint64_t ino)
{
	FuseTest::expect_lookup(relpath, ino, S_IFDIR | 0755, 0, 1);
}
};

class Readdir_7_8: public Readdir {
public:
virtual void SetUp() {
	m_kernel_minor_version = 8;
	Readdir::SetUp();
}

void expect_lookup(const char *relpath, uint64_t ino)
{
	FuseTest::expect_lookup_7_8(relpath, ino, S_IFDIR | 0755, 0, 1);
}
};

const char dot[] = ".";
const char dotdot[] = "..";

/* FUSE_READDIR returns nothing but "." and ".." */
TEST_F(Readdir, dots)
{
	const char FULLPATH[] = "mountpoint/some_dir";
	const char RELPATH[] = "some_dir";
	uint64_t ino = 42;
	DIR *dir;
	struct dirent *de;
	vector<struct dirent> ents(2);
	vector<struct dirent> empty_ents(0);

	expect_lookup(RELPATH, ino);
	expect_opendir(ino);
	ents[0].d_fileno = 2;
	ents[0].d_off = 2000;
	ents[0].d_namlen = sizeof(dotdot);
	ents[0].d_type = DT_DIR;
	strncpy(ents[0].d_name, dotdot, ents[0].d_namlen);
	ents[1].d_fileno = 3;
	ents[1].d_off = 3000;
	ents[1].d_namlen = sizeof(dot);
	ents[1].d_type = DT_DIR;
	strncpy(ents[1].d_name, dot, ents[1].d_namlen);
	expect_readdir(ino, 0, ents);
	expect_readdir(ino, 3000, empty_ents);

	errno = 0;
	dir = opendir(FULLPATH);
	ASSERT_NE(nullptr, dir) << strerror(errno);

	errno = 0;
	de = readdir(dir);
	ASSERT_NE(nullptr, de) << strerror(errno);
	EXPECT_EQ(2ul, de->d_fileno);
	EXPECT_EQ(DT_DIR, de->d_type);
	EXPECT_EQ(sizeof(dotdot), de->d_namlen);
	EXPECT_EQ(0, strcmp(dotdot, de->d_name));

	errno = 0;
	de = readdir(dir);
	ASSERT_NE(nullptr, de) << strerror(errno);
	EXPECT_EQ(3ul, de->d_fileno);
	EXPECT_EQ(DT_DIR, de->d_type);
	EXPECT_EQ(sizeof(dot), de->d_namlen);
	EXPECT_EQ(0, strcmp(dot, de->d_name));

	ASSERT_EQ(nullptr, readdir(dir));
	ASSERT_EQ(0, errno);

	leakdir(dir);
}

TEST_F(Readdir, eio)
{
	const char FULLPATH[] = "mountpoint/some_dir";
	const char RELPATH[] = "some_dir";
	uint64_t ino = 42;
	DIR *dir;
	struct dirent *de;

	expect_lookup(RELPATH, ino);
	expect_opendir(ino);
	EXPECT_CALL(*m_mock, process(
		ResultOf([=](auto in) {
			return (in.header.opcode == FUSE_READDIR &&
				in.header.nodeid == ino &&
				in.body.readdir.offset == 0);
		}, Eq(true)),
		_)
	).WillOnce(Invoke(ReturnErrno(EIO)));

	errno = 0;
	dir = opendir(FULLPATH);
	ASSERT_NE(nullptr, dir) << strerror(errno);

	errno = 0;
	de = readdir(dir);
	ASSERT_EQ(nullptr, de);
	ASSERT_EQ(EIO, errno);

	leakdir(dir);
}

/*
 * getdirentries(2) can use a larger buffer size than readdir(3).  It also has
 * some additional non-standardized fields in the returned dirent.
 */
TEST_F(Readdir, getdirentries_empty)
{
	const char FULLPATH[] = "mountpoint/some_dir";
	const char RELPATH[] = "some_dir";
	uint64_t ino = 42;
	int fd;
	char buf[8192];
	ssize_t r;

	expect_lookup(RELPATH, ino);
	expect_opendir(ino);

	EXPECT_CALL(*m_mock, process(
		ResultOf([=](auto in) {
			return (in.header.opcode == FUSE_READDIR &&
				in.header.nodeid == ino &&
				in.body.readdir.size == 8192);
		}, Eq(true)),
		_)
	).WillOnce(Invoke(ReturnImmediate([=](auto in __unused, auto& out) {
		out.header.error = 0;
		out.header.len = sizeof(out.header);
	})));

	fd = open(FULLPATH, O_DIRECTORY);
	ASSERT_LE(0, fd) << strerror(errno);
	r = getdirentries(fd, buf, sizeof(buf), 0);
	ASSERT_EQ(0, r) << strerror(errno);

	leak(fd);
}

/*
 * The dirent.d_off field can be used with lseek to position the directory so
 * that getdirentries will return the subsequent dirent.
 */
TEST_F(Readdir, getdirentries_seek)
{
	const char FULLPATH[] = "mountpoint/some_dir";
	const char RELPATH[] = "some_dir";
	vector<struct dirent> ents0(2);
	vector<struct dirent> ents1(1);
	uint64_t ino = 42;
	int fd;
	const size_t bufsize = 8192;
	char buf[bufsize];
	struct dirent *de0, *de1;
	ssize_t r;

	expect_lookup(RELPATH, ino);
	expect_opendir(ino);

	ents0[0].d_fileno = 2;
	ents0[0].d_off = 2000;
	ents0[0].d_namlen = sizeof(dotdot);
	ents0[0].d_type = DT_DIR;
	strncpy(ents0[0].d_name, dotdot, ents0[0].d_namlen);
	expect_readdir(ino, 0, ents0);
	ents0[1].d_fileno = 3;
	ents0[1].d_off = 3000;
	ents0[1].d_namlen = sizeof(dot);
	ents0[1].d_type = DT_DIR;
	ents1[0].d_fileno = 3;
	ents1[0].d_off = 3000;
	ents1[0].d_namlen = sizeof(dot);
	ents1[0].d_type = DT_DIR;
	strncpy(ents1[0].d_name, dot, ents1[0].d_namlen);
	expect_readdir(ino, 0, ents0);
	expect_readdir(ino, 2000, ents1);

	fd = open(FULLPATH, O_DIRECTORY);
	ASSERT_LE(0, fd) << strerror(errno);
	r = getdirentries(fd, buf, sizeof(buf), 0);
	ASSERT_LT(0, r) << strerror(errno);
	de0 = (struct dirent*)&buf[0];
	ASSERT_EQ(2000, de0->d_off);
	ASSERT_LT(de0->d_reclen + offsetof(struct dirent, d_fileno), bufsize);
	de1 = (struct dirent*)(&(buf[de0->d_reclen]));
	ASSERT_EQ(3ul, de1->d_fileno);

	r = lseek(fd, de0->d_off, SEEK_SET);
	ASSERT_LE(0, r);
	r = getdirentries(fd, buf, sizeof(buf), 0);
	ASSERT_LT(0, r) << strerror(errno);
	de0 = (struct dirent*)&buf[0];
	ASSERT_EQ(3000, de0->d_off);
}

/* 
 * Nothing bad should happen if getdirentries is called on two file descriptors
 * which were concurrently open, but one has already been closed.
 * This is a regression test for a specific bug dating from r238402.
 */
TEST_F(Readdir, getdirentries_concurrent)
{
	const char FULLPATH[] = "mountpoint/some_dir";
	const char RELPATH[] = "some_dir";
	uint64_t ino = 42;
	int fd0, fd1;
	char buf[8192];
	ssize_t r;

	FuseTest::expect_lookup(RELPATH, ino, S_IFDIR | 0755, 0, 2);
	expect_opendir(ino);

	EXPECT_CALL(*m_mock, process(
		ResultOf([=](auto in) {
			return (in.header.opcode == FUSE_READDIR &&
				in.header.nodeid == ino &&
				in.body.readdir.size == 8192);
		}, Eq(true)),
		_)
	).Times(2)
	.WillRepeatedly(Invoke(ReturnImmediate([=](auto in __unused, auto& out) {
		out.header.error = 0;
		out.header.len = sizeof(out.header);
	})));

	fd0 = open(FULLPATH, O_DIRECTORY);
	ASSERT_LE(0, fd0) << strerror(errno);

	fd1 = open(FULLPATH, O_DIRECTORY);
	ASSERT_LE(0, fd1) << strerror(errno);

	r = getdirentries(fd0, buf, sizeof(buf), 0);
	ASSERT_EQ(0, r) << strerror(errno);

	EXPECT_EQ(0, close(fd0)) << strerror(errno);

	r = getdirentries(fd1, buf, sizeof(buf), 0);
	ASSERT_EQ(0, r) << strerror(errno);

	leak(fd0);
	leak(fd1);
}

/*
 * FUSE_READDIR returns nothing, not even "." and "..".  This is legal, though
 * the filesystem obviously won't be fully functional.
 */
TEST_F(Readdir, nodots)
{
	const char FULLPATH[] = "mountpoint/some_dir";
	const char RELPATH[] = "some_dir";
	uint64_t ino = 42;
	DIR *dir;

	expect_lookup(RELPATH, ino);
	expect_opendir(ino);

	EXPECT_CALL(*m_mock, process(
		ResultOf([=](auto in) {
			return (in.header.opcode == FUSE_READDIR &&
				in.header.nodeid == ino);
		}, Eq(true)),
		_)
	).WillOnce(Invoke(ReturnImmediate([=](auto in __unused, auto& out) {
		out.header.error = 0;
		out.header.len = sizeof(out.header);
	})));

	errno = 0;
	dir = opendir(FULLPATH);
	ASSERT_NE(nullptr, dir) << strerror(errno);
	errno = 0;
	ASSERT_EQ(nullptr, readdir(dir));
	ASSERT_EQ(0, errno);

	leakdir(dir);
}

/* telldir(3) and seekdir(3) should work with fuse */
TEST_F(Readdir, seekdir)
{
	const char FULLPATH[] = "mountpoint/some_dir";
	const char RELPATH[] = "some_dir";
	uint64_t ino = 42;
	DIR *dir;
	struct dirent *de;
	/*
	 * use enough entries to be > 4096 bytes, so getdirentries must be
	 * called
	 * multiple times.
	 */
	vector<struct dirent> ents0(122), ents1(102), ents2(30);
	long bookmark;
	int i = 0;

	for (auto& it: ents0) {
		snprintf(it.d_name, MAXNAMLEN, "file.%d", i);
		it.d_fileno = 2 + i;
		it.d_off = (2 + i) * 1000;
		it.d_namlen = strlen(it.d_name);
		it.d_type = DT_REG;
		i++;
	}
	for (auto& it: ents1) {
		snprintf(it.d_name, MAXNAMLEN, "file.%d", i);
		it.d_fileno = 2 + i;
		it.d_off = (2 + i) * 1000;
		it.d_namlen = strlen(it.d_name);
		it.d_type = DT_REG;
		i++;
	}
	for (auto& it: ents2) {
		snprintf(it.d_name, MAXNAMLEN, "file.%d", i);
		it.d_fileno = 2 + i;
		it.d_off = (2 + i) * 1000;
		it.d_namlen = strlen(it.d_name);
		it.d_type = DT_REG;
		i++;
	}

	expect_lookup(RELPATH, ino);
	expect_opendir(ino);

	expect_readdir(ino, 0, ents0);
	expect_readdir(ino, 123000, ents1);
	expect_readdir(ino, 225000, ents2);

	errno = 0;
	dir = opendir(FULLPATH);
	ASSERT_NE(nullptr, dir) << strerror(errno);

	for (i=0; i < 128; i++) {
		errno = 0;
		de = readdir(dir);
		ASSERT_NE(nullptr, de) << strerror(errno);
		EXPECT_EQ(2 + (ino_t)i, de->d_fileno);
	}
	bookmark = telldir(dir);

	for (; i < 232; i++) {
		errno = 0;
		de = readdir(dir);
		ASSERT_NE(nullptr, de) << strerror(errno);
		EXPECT_EQ(2 + (ino_t)i, de->d_fileno);
	}

	seekdir(dir, bookmark);
	de = readdir(dir);
	ASSERT_NE(nullptr, de) << strerror(errno);
	EXPECT_EQ(130ul, de->d_fileno);

	leakdir(dir);
}

TEST_F(Readdir_7_8, nodots)
{
	const char FULLPATH[] = "mountpoint/some_dir";
	const char RELPATH[] = "some_dir";
	uint64_t ino = 42;
	DIR *dir;

	expect_lookup(RELPATH, ino);
	expect_opendir(ino);

	EXPECT_CALL(*m_mock, process(
		ResultOf([=](auto in) {
			return (in.header.opcode == FUSE_READDIR &&
				in.header.nodeid == ino);
		}, Eq(true)),
		_)
	).WillOnce(Invoke(ReturnImmediate([=](auto in __unused, auto& out) {
		out.header.error = 0;
		out.header.len = sizeof(out.header);
	})));

	errno = 0;
	dir = opendir(FULLPATH);
	ASSERT_NE(nullptr, dir) << strerror(errno);
	errno = 0;
	ASSERT_EQ(nullptr, readdir(dir));
	ASSERT_EQ(0, errno);

	leakdir(dir);
}
