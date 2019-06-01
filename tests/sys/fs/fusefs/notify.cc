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
public:
virtual void SetUp() {
	m_init_flags = FUSE_EXPORT_SUPPORT;
	FuseTest::SetUp();
}

void expect_lookup(uint64_t parent, const char *relpath, uint64_t ino,
	Sequence &seq)
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
		out.body.entry.attr_valid = UINT64_MAX;
		out.body.entry.entry_valid = UINT64_MAX;
	})));
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

	expect_lookup(FUSE_ROOT_ID, RELPATH, ino0, seq);
	expect_lookup(FUSE_ROOT_ID, RELPATH, ino1, seq);

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
	/* It's not an error for an entry to not be cached */
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
	expect_lookup(dir_ino, FNAME, ino0, seq);
	expect_lookup(dir_ino, FNAME, ino1, seq);

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
	/* It's not an error for an entry to not be cached */
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

	expect_lookup(FUSE_ROOT_ID, RELPATH, ino, seq);
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
	/* It's not an error for an entry to not be cached */
	EXPECT_EQ(0, (intptr_t)thr0_value);

	/* /'s attribute cache should be cleared */
	ASSERT_EQ(0, stat("mountpoint", &sb)) << strerror(errno);
}
