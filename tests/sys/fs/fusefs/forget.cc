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
#include <semaphore.h>
#include <unistd.h>
}

#include "mockfs.hh"
#include "utils.hh"

using namespace testing;

const char reclaim_mib[] = "debug.try_reclaim_vnode";

class Forget: public FuseTest {
public:
void SetUp() {
	if (geteuid() != 0)
		GTEST_SKIP() << "Only root may use " << reclaim_mib;

	if (-1 == sysctlbyname(reclaim_mib, NULL, 0, NULL, 0) &&
	    errno == ENOENT)
		GTEST_SKIP() << reclaim_mib << " is not available";

	FuseTest::SetUp();
}

};

/*
 * When a fusefs vnode is reclaimed, it should send a FUSE_FORGET operation.
 */
TEST_F(Forget, ok)
{
	const char FULLPATH[] = "mountpoint/some_file.txt";
	const char RELPATH[] = "some_file.txt";
	uint64_t ino = 42;
	mode_t mode = S_IFREG | 0755;
	sem_t sem;
	int err;

	ASSERT_EQ(0, sem_init(&sem, 0, 0)) << strerror(errno);

	EXPECT_LOOKUP(FUSE_ROOT_ID, RELPATH)
	.Times(3)
	.WillRepeatedly(Invoke(
		ReturnImmediate([=](auto in __unused, auto& out) {
		SET_OUT_HEADER_LEN(out, entry);
		out.body.entry.attr.mode = mode;
		out.body.entry.nodeid = ino;
		out.body.entry.attr.nlink = 1;
		out.body.entry.attr_valid = UINT64_MAX;
	})));
	expect_forget(ino, 3, &sem);

	/*
	 * access(2) the file to force a lookup.  Access it twice to double its
	 * lookup count.
	 */
	ASSERT_EQ(0, access(FULLPATH, F_OK)) << strerror(errno);
	ASSERT_EQ(0, access(FULLPATH, F_OK)) << strerror(errno);

	err = sysctlbyname(reclaim_mib, NULL, 0, FULLPATH, sizeof(FULLPATH));
	ASSERT_EQ(0, err) << strerror(errno);

	sem_wait(&sem);
	sem_destroy(&sem);
}

/*
 * When a directory is reclaimed, the names of its entries vanish from the
 * namecache
 */
TEST_F(Forget, invalidate_names)
{
	const char FULLFPATH[] = "mountpoint/some_dir/some_file.txt";
	const char FULLDPATH[] = "mountpoint/some_dir";
	const char DNAME[] = "some_dir";
	const char FNAME[] = "some_file.txt";
	uint64_t dir_ino = 42;
	uint64_t file_ino = 43;
	int err;

	EXPECT_LOOKUP(FUSE_ROOT_ID, DNAME)
	.WillRepeatedly(Invoke(
		ReturnImmediate([=](auto in __unused, auto& out) {
		SET_OUT_HEADER_LEN(out, entry);
		out.body.entry.attr.mode = S_IFDIR | 0755;
		out.body.entry.nodeid = dir_ino;
		out.body.entry.attr.nlink = 2;
		out.body.entry.attr_valid = UINT64_MAX;
		out.body.entry.entry_valid = UINT64_MAX;
	})));

	/* 
	 * Even though we don't reclaim FNAME and its entry is cacheable, we
	 * should get two lookups because the reclaim of DNAME will invalidate
	 * the cached FNAME entry.
	 */
	EXPECT_LOOKUP(dir_ino, FNAME)
	.Times(2)
	.WillRepeatedly(Invoke(
		ReturnImmediate([=](auto in __unused, auto& out) {
		SET_OUT_HEADER_LEN(out, entry);
		out.body.entry.attr.mode = S_IFREG | 0644;
		out.body.entry.nodeid = file_ino;
		out.body.entry.attr.nlink = 1;
		out.body.entry.attr_valid = UINT64_MAX;
		out.body.entry.entry_valid = UINT64_MAX;
	})));
	expect_forget(dir_ino, 2);

	/* Access the file to cache its name */
	ASSERT_EQ(0, access(FULLFPATH, F_OK)) << strerror(errno);
	
	/* Reclaim the directory, invalidating its children from namecache */
	err = sysctlbyname(reclaim_mib, NULL, 0, FULLDPATH, sizeof(FULLDPATH));
	ASSERT_EQ(0, err) << strerror(errno);

	/* Access the file again, causing another lookup */
	ASSERT_EQ(0, access(FULLFPATH, F_OK)) << strerror(errno);
}
