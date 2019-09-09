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
#include <sys/types.h>
#include <sys/sysctl.h>

#include <fcntl.h>
#include <pthread.h>
}

#include "mockfs.hh"
#include "utils.hh"

using namespace testing;

/*
 * FUSE asynchonous notification
 *
 * FUSE servers can send unprompted notification messages for things like cache
 * invalidation.  This file tests our client's handling of those messages.
 */

class Notify: public FuseTest {
public:
/* Ignore an optional FUSE_FSYNC */
void maybe_expect_fsync(uint64_t ino)
{
	EXPECT_CALL(*m_mock, process(
		ResultOf([=](auto in) {
			return (in.header.opcode == FUSE_FSYNC &&
				in.header.nodeid == ino);
		}, Eq(true)),
		_)
	).WillOnce(Invoke(ReturnErrno(0)));
}

void expect_lookup(uint64_t parent, const char *relpath, uint64_t ino,
	off_t size, Sequence &seq)
{
	EXPECT_LOOKUP(parent, relpath)
	.InSequence(seq)
	.WillOnce(Invoke(
		ReturnImmediate([=](auto in __unused, auto& out) {
		SET_OUT_HEADER_LEN(out, entry);
		out.body.entry.attr.mode = S_IFREG | 0644;
		out.body.entry.nodeid = ino;
		out.body.entry.attr.ino = ino;
		out.body.entry.attr.nlink = 1;
		out.body.entry.attr.size = size;
		out.body.entry.attr_valid = UINT64_MAX;
		out.body.entry.entry_valid = UINT64_MAX;
	})));
}
};

class NotifyWriteback: public Notify {
public:
virtual void SetUp() {
	m_init_flags |= FUSE_WRITEBACK_CACHE;
	m_async = true;
	Notify::SetUp();
	if (IsSkipped())
		return;
}

void expect_write(uint64_t ino, uint64_t offset, uint64_t size,
	const void *contents)
{
	FuseTest::expect_write(ino, offset, size, size, 0, 0, contents);
}

};

struct inval_entry_args {
	MockFS		*mock;
	ino_t		parent;
	const char	*name;
	size_t		namelen;
};

static void* inval_entry(void* arg) {
	const struct inval_entry_args *iea = (struct inval_entry_args*)arg;
	ssize_t r;

	r = iea->mock->notify_inval_entry(iea->parent, iea->name, iea->namelen);
	if (r >= 0)
		return 0;
	else
		return (void*)(intptr_t)errno;
}

struct inval_inode_args {
	MockFS		*mock;
	ino_t		ino;
	off_t		off;
	ssize_t		len;
};

struct store_args {
	MockFS		*mock;
	ino_t		nodeid;
	off_t		offset;
	ssize_t		size;
	const void*	data;
};

static void* inval_inode(void* arg) {
	const struct inval_inode_args *iia = (struct inval_inode_args*)arg;
	ssize_t r;

	r = iia->mock->notify_inval_inode(iia->ino, iia->off, iia->len);
	if (r >= 0)
		return 0;
	else
		return (void*)(intptr_t)errno;
}

static void* store(void* arg) {
	const struct store_args *sa = (struct store_args*)arg;
	ssize_t r;

	r = sa->mock->notify_store(sa->nodeid, sa->offset, sa->data, sa->size);
	if (r >= 0)
		return 0;
	else
		return (void*)(intptr_t)errno;
}

/* Invalidate a nonexistent entry */
TEST_F(Notify, inval_entry_nonexistent)
{
	const static char *name = "foo";
	struct inval_entry_args iea;
	void *thr0_value;
	pthread_t th0;

	iea.mock = m_mock;
	iea.parent = FUSE_ROOT_ID;
	iea.name = name;
	iea.namelen = strlen(name);
	ASSERT_EQ(0, pthread_create(&th0, NULL, inval_entry, &iea))
		<< strerror(errno);
	pthread_join(th0, &thr0_value);
	/* It's not an error for an entry to not be cached */
	EXPECT_EQ(0, (intptr_t)thr0_value);
}

/* Invalidate a cached entry */
TEST_F(Notify, inval_entry)
{
	const static char FULLPATH[] = "mountpoint/foo";
	const static char RELPATH[] = "foo";
	struct inval_entry_args iea;
	struct stat sb;
	void *thr0_value;
	uint64_t ino0 = 42;
	uint64_t ino1 = 43;
	Sequence seq;
	pthread_t th0;

	expect_lookup(FUSE_ROOT_ID, RELPATH, ino0, 0, seq);
	expect_lookup(FUSE_ROOT_ID, RELPATH, ino1, 0, seq);

	/* Fill the entry cache */
	ASSERT_EQ(0, stat(FULLPATH, &sb)) << strerror(errno);
	EXPECT_EQ(ino0, sb.st_ino);

	/* Now invalidate the entry */
	iea.mock = m_mock;
	iea.parent = FUSE_ROOT_ID;
	iea.name = RELPATH;
	iea.namelen = strlen(RELPATH);
	ASSERT_EQ(0, pthread_create(&th0, NULL, inval_entry, &iea))
		<< strerror(errno);
	pthread_join(th0, &thr0_value);
	EXPECT_EQ(0, (intptr_t)thr0_value);

	/* The second lookup should return the alternate ino */
	ASSERT_EQ(0, stat(FULLPATH, &sb)) << strerror(errno);
	EXPECT_EQ(ino1, sb.st_ino);
}

/*
 * Invalidate a cached entry beneath the root, which uses a slightly different
 * code path.
 */
TEST_F(Notify, inval_entry_below_root)
{
	const static char FULLPATH[] = "mountpoint/some_dir/foo";
	const static char DNAME[] = "some_dir";
	const static char FNAME[] = "foo";
	struct inval_entry_args iea;
	struct stat sb;
	void *thr0_value;
	uint64_t dir_ino = 41;
	uint64_t ino0 = 42;
	uint64_t ino1 = 43;
	Sequence seq;
	pthread_t th0;

	EXPECT_LOOKUP(FUSE_ROOT_ID, DNAME)
	.WillOnce(Invoke(
		ReturnImmediate([=](auto in __unused, auto& out) {
		SET_OUT_HEADER_LEN(out, entry);
		out.body.entry.attr.mode = S_IFDIR | 0755;
		out.body.entry.nodeid = dir_ino;
		out.body.entry.attr.nlink = 2;
		out.body.entry.attr_valid = UINT64_MAX;
		out.body.entry.entry_valid = UINT64_MAX;
	})));
	expect_lookup(dir_ino, FNAME, ino0, 0, seq);
	expect_lookup(dir_ino, FNAME, ino1, 0, seq);

	/* Fill the entry cache */
	ASSERT_EQ(0, stat(FULLPATH, &sb)) << strerror(errno);
	EXPECT_EQ(ino0, sb.st_ino);

	/* Now invalidate the entry */
	iea.mock = m_mock;
	iea.parent = dir_ino;
	iea.name = FNAME;
	iea.namelen = strlen(FNAME);
	ASSERT_EQ(0, pthread_create(&th0, NULL, inval_entry, &iea))
		<< strerror(errno);
	pthread_join(th0, &thr0_value);
	EXPECT_EQ(0, (intptr_t)thr0_value);

	/* The second lookup should return the alternate ino */
	ASSERT_EQ(0, stat(FULLPATH, &sb)) << strerror(errno);
	EXPECT_EQ(ino1, sb.st_ino);
}

/* Invalidating an entry invalidates the parent directory's attributes */
TEST_F(Notify, inval_entry_invalidates_parent_attrs)
{
	const static char FULLPATH[] = "mountpoint/foo";
	const static char RELPATH[] = "foo";
	struct inval_entry_args iea;
	struct stat sb;
	void *thr0_value;
	uint64_t ino = 42;
	Sequence seq;
	pthread_t th0;

	expect_lookup(FUSE_ROOT_ID, RELPATH, ino, 0, seq);
	EXPECT_CALL(*m_mock, process(
		ResultOf([=](auto in) {
			return (in.header.opcode == FUSE_GETATTR &&
				in.header.nodeid == FUSE_ROOT_ID);
		}, Eq(true)),
		_)
	).Times(2)
	.WillRepeatedly(Invoke(ReturnImmediate([=](auto i __unused, auto& out) {
		SET_OUT_HEADER_LEN(out, attr);
		out.body.attr.attr.mode = S_IFDIR | 0755;
		out.body.attr.attr_valid = UINT64_MAX;
	})));

	/* Fill the attr and entry cache */
	ASSERT_EQ(0, stat("mountpoint", &sb)) << strerror(errno);
	ASSERT_EQ(0, stat(FULLPATH, &sb)) << strerror(errno);

	/* Now invalidate the entry */
	iea.mock = m_mock;
	iea.parent = FUSE_ROOT_ID;
	iea.name = RELPATH;
	iea.namelen = strlen(RELPATH);
	ASSERT_EQ(0, pthread_create(&th0, NULL, inval_entry, &iea))
		<< strerror(errno);
	pthread_join(th0, &thr0_value);
	EXPECT_EQ(0, (intptr_t)thr0_value);

	/* /'s attribute cache should be cleared */
	ASSERT_EQ(0, stat("mountpoint", &sb)) << strerror(errno);
}


TEST_F(Notify, inval_inode_nonexistent)
{
	struct inval_inode_args iia;
	ino_t ino = 42;
	void *thr0_value;
	pthread_t th0;

	iia.mock = m_mock;
	iia.ino = ino;
	iia.off = 0;
	iia.len = 0;
	ASSERT_EQ(0, pthread_create(&th0, NULL, inval_inode, &iia))
		<< strerror(errno);
	pthread_join(th0, &thr0_value);
	/* It's not an error for an inode to not be cached */
	EXPECT_EQ(0, (intptr_t)thr0_value);
}

TEST_F(Notify, inval_inode_with_clean_cache)
{
	const static char FULLPATH[] = "mountpoint/foo";
	const static char RELPATH[] = "foo";
	const char CONTENTS0[] = "abcdefgh";
	const char CONTENTS1[] = "ijklmnopqrstuvwxyz";
	struct inval_inode_args iia;
	struct stat sb;
	ino_t ino = 42;
	void *thr0_value;
	Sequence seq;
	uid_t uid = 12345;
	pthread_t th0;
	ssize_t size0 = sizeof(CONTENTS0);
	ssize_t size1 = sizeof(CONTENTS1);
	char buf[80];
	int fd;

	expect_lookup(FUSE_ROOT_ID, RELPATH, ino, size0, seq);
	expect_open(ino, 0, 1);
	EXPECT_CALL(*m_mock, process(
		ResultOf([=](auto in) {
			return (in.header.opcode == FUSE_GETATTR &&
				in.header.nodeid == ino);
		}, Eq(true)),
		_)
	).WillOnce(Invoke(ReturnImmediate([=](auto i __unused, auto& out) {
		SET_OUT_HEADER_LEN(out, attr);
		out.body.attr.attr.mode = S_IFREG | 0644;
		out.body.attr.attr_valid = UINT64_MAX;
		out.body.attr.attr.size = size1;
		out.body.attr.attr.uid = uid;
	})));
	expect_read(ino, 0, size0, size0, CONTENTS0);
	expect_read(ino, 0, size1, size1, CONTENTS1);

	/* Fill the data cache */
	fd = open(FULLPATH, O_RDWR);
	ASSERT_LE(0, fd) << strerror(errno);
	ASSERT_EQ(size0, read(fd, buf, size0)) << strerror(errno);
	EXPECT_EQ(0, memcmp(buf, CONTENTS0, size0));

	/* Evict the data cache */
	iia.mock = m_mock;
	iia.ino = ino;
	iia.off = 0;
	iia.len = 0;
	ASSERT_EQ(0, pthread_create(&th0, NULL, inval_inode, &iia))
		<< strerror(errno);
	pthread_join(th0, &thr0_value);
	EXPECT_EQ(0, (intptr_t)thr0_value);

	/* cache attributes were been purged; this will trigger a new GETATTR */
	ASSERT_EQ(0, stat(FULLPATH, &sb)) << strerror(errno);
	EXPECT_EQ(uid, sb.st_uid);
	EXPECT_EQ(size1, sb.st_size);

	/* This read should not be serviced by cache */
	ASSERT_EQ(0, lseek(fd, 0, SEEK_SET)) << strerror(errno);
	ASSERT_EQ(size1, read(fd, buf, size1)) << strerror(errno);
	EXPECT_EQ(0, memcmp(buf, CONTENTS1, size1));

	leak(fd);
}

/* FUSE_NOTIFY_STORE with a file that's not in the entry cache */
/* disabled because FUSE_NOTIFY_STORE is not yet implemented */
TEST_F(Notify, DISABLED_store_nonexistent)
{
	struct store_args sa;
	ino_t ino = 42;
	void *thr0_value;
	pthread_t th0;

	sa.mock = m_mock;
	sa.nodeid = ino;
	sa.offset = 0;
	sa.size = 0;
	ASSERT_EQ(0, pthread_create(&th0, NULL, store, &sa)) << strerror(errno);
	pthread_join(th0, &thr0_value);
	/* It's not an error for a file to be unknown to the kernel */
	EXPECT_EQ(0, (intptr_t)thr0_value);
}

/* Store data into for a file that does not yet have anything cached */
/* disabled because FUSE_NOTIFY_STORE is not yet implemented */
TEST_F(Notify, DISABLED_store_with_blank_cache)
{
	const static char FULLPATH[] = "mountpoint/foo";
	const static char RELPATH[] = "foo";
	const char CONTENTS1[] = "ijklmnopqrstuvwxyz";
	struct store_args sa;
	ino_t ino = 42;
	void *thr0_value;
	Sequence seq;
	pthread_t th0;
	ssize_t size1 = sizeof(CONTENTS1);
	char buf[80];
	int fd;

	expect_lookup(FUSE_ROOT_ID, RELPATH, ino, size1, seq);
	expect_open(ino, 0, 1);

	/* Fill the data cache */
	fd = open(FULLPATH, O_RDWR);
	ASSERT_LE(0, fd) << strerror(errno);

	/* Evict the data cache */
	sa.mock = m_mock;
	sa.nodeid = ino;
	sa.offset = 0;
	sa.size = size1;
	sa.data = (const void*)CONTENTS1;
	ASSERT_EQ(0, pthread_create(&th0, NULL, store, &sa)) << strerror(errno);
	pthread_join(th0, &thr0_value);
	EXPECT_EQ(0, (intptr_t)thr0_value);

	/* This read should be serviced by cache */
	ASSERT_EQ(size1, read(fd, buf, size1)) << strerror(errno);
	EXPECT_EQ(0, memcmp(buf, CONTENTS1, size1));

	leak(fd);
}

TEST_F(NotifyWriteback, inval_inode_with_dirty_cache)
{
	const static char FULLPATH[] = "mountpoint/foo";
	const static char RELPATH[] = "foo";
	const char CONTENTS[] = "abcdefgh";
	struct inval_inode_args iia;
	ino_t ino = 42;
	void *thr0_value;
	Sequence seq;
	pthread_t th0;
	ssize_t bufsize = sizeof(CONTENTS);
	int fd;

	expect_lookup(FUSE_ROOT_ID, RELPATH, ino, 0, seq);
	expect_open(ino, 0, 1);

	/* Fill the data cache */
	fd = open(FULLPATH, O_RDWR);
	ASSERT_LE(0, fd);
	ASSERT_EQ(bufsize, write(fd, CONTENTS, bufsize)) << strerror(errno);

	expect_write(ino, 0, bufsize, CONTENTS);
	/* 
	 * The FUSE protocol does not require an fsync here, but FreeBSD's
	 * bufobj_invalbuf sends it anyway
	 */
	maybe_expect_fsync(ino);

	/* Evict the data cache */
	iia.mock = m_mock;
	iia.ino = ino;
	iia.off = 0;
	iia.len = 0;
	ASSERT_EQ(0, pthread_create(&th0, NULL, inval_inode, &iia))
		<< strerror(errno);
	pthread_join(th0, &thr0_value);
	EXPECT_EQ(0, (intptr_t)thr0_value);

	leak(fd);
}

TEST_F(NotifyWriteback, inval_inode_attrs_only)
{
	const static char FULLPATH[] = "mountpoint/foo";
	const static char RELPATH[] = "foo";
	const char CONTENTS[] = "abcdefgh";
	struct inval_inode_args iia;
	struct stat sb;
	uid_t uid = 12345;
	ino_t ino = 42;
	void *thr0_value;
	Sequence seq;
	pthread_t th0;
	ssize_t bufsize = sizeof(CONTENTS);
	int fd;

	expect_lookup(FUSE_ROOT_ID, RELPATH, ino, 0, seq);
	expect_open(ino, 0, 1);
	EXPECT_CALL(*m_mock, process(
		ResultOf([=](auto in) {
			return (in.header.opcode == FUSE_WRITE);
		}, Eq(true)),
		_)
	).Times(0);
	EXPECT_CALL(*m_mock, process(
		ResultOf([=](auto in) {
			return (in.header.opcode == FUSE_GETATTR &&
				in.header.nodeid == ino);
		}, Eq(true)),
		_)
	).WillOnce(Invoke(ReturnImmediate([=](auto i __unused, auto& out) {
		SET_OUT_HEADER_LEN(out, attr);
		out.body.attr.attr.mode = S_IFREG | 0644;
		out.body.attr.attr_valid = UINT64_MAX;
		out.body.attr.attr.size = bufsize;
		out.body.attr.attr.uid = uid;
	})));

	/* Fill the data cache */
	fd = open(FULLPATH, O_RDWR);
	ASSERT_LE(0, fd) << strerror(errno);
	ASSERT_EQ(bufsize, write(fd, CONTENTS, bufsize)) << strerror(errno);

	/* Evict the attributes, but not data cache */
	iia.mock = m_mock;
	iia.ino = ino;
	iia.off = -1;
	iia.len = 0;
	ASSERT_EQ(0, pthread_create(&th0, NULL, inval_inode, &iia))
		<< strerror(errno);
	pthread_join(th0, &thr0_value);
	EXPECT_EQ(0, (intptr_t)thr0_value);

	/* cache attributes were been purged; this will trigger a new GETATTR */
	ASSERT_EQ(0, stat(FULLPATH, &sb)) << strerror(errno);
	EXPECT_EQ(uid, sb.st_uid);
	EXPECT_EQ(bufsize, sb.st_size);

	leak(fd);
}
