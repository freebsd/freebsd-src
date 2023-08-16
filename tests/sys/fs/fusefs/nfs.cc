/*-
 * SPDX-License-Identifier: BSD-2-Clause
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

/* This file tests functionality needed by NFS servers */
extern "C" {
#include <sys/param.h>
#include <sys/mount.h>

#include <dirent.h>
#include <fcntl.h>
#include <unistd.h>
}

#include "mockfs.hh"
#include "utils.hh"

using namespace std;
using namespace testing;


class Nfs: public FuseTest {
public:
virtual void SetUp() {
	if (geteuid() != 0)
                GTEST_SKIP() << "This test requires a privileged user";
	FuseTest::SetUp();
}
};

class Exportable: public Nfs {
public:
virtual void SetUp() {
	m_init_flags = FUSE_EXPORT_SUPPORT;
	Nfs::SetUp();
}
};

class Fhstat: public Exportable {};
class FhstatNotExportable: public Nfs {};
class Getfh: public Exportable {};
class Readdir: public Exportable {};

/* If the server returns a different generation number, then file is stale */
TEST_F(Fhstat, estale)
{
	const char FULLPATH[] = "mountpoint/some_dir/.";
	const char RELDIRPATH[] = "some_dir";
	fhandle_t fhp;
	struct stat sb;
	const uint64_t ino = 42;
	const mode_t mode = S_IFDIR | 0755;
	Sequence seq;

	EXPECT_LOOKUP(FUSE_ROOT_ID, RELDIRPATH)
	.InSequence(seq)
	.WillOnce(Invoke(ReturnImmediate([=](auto in __unused, auto& out) {
		SET_OUT_HEADER_LEN(out, entry);
		out.body.entry.attr.mode = mode;
		out.body.entry.nodeid = ino;
		out.body.entry.generation = 1;
		out.body.entry.attr_valid = UINT64_MAX;
		out.body.entry.entry_valid = 0;
	})));

	EXPECT_LOOKUP(ino, ".")
	.InSequence(seq)
	.WillOnce(Invoke(ReturnImmediate([=](auto in __unused, auto& out) {
		SET_OUT_HEADER_LEN(out, entry);
		out.body.entry.attr.mode = mode;
		out.body.entry.nodeid = ino;
		out.body.entry.generation = 2;
		out.body.entry.attr_valid = UINT64_MAX;
		out.body.entry.entry_valid = 0;
	})));

	ASSERT_EQ(0, getfh(FULLPATH, &fhp)) << strerror(errno);
	ASSERT_EQ(-1, fhstat(&fhp, &sb));
	EXPECT_EQ(ESTALE, errno);
}

/* If we must lookup an entry from the server, send a LOOKUP request for "." */
TEST_F(Fhstat, lookup_dot)
{
	const char FULLPATH[] = "mountpoint/some_dir/.";
	const char RELDIRPATH[] = "some_dir";
	fhandle_t fhp;
	struct stat sb;
	const uint64_t ino = 42;
	const mode_t mode = S_IFDIR | 0755;
	const uid_t uid = 12345;

	EXPECT_LOOKUP(FUSE_ROOT_ID, RELDIRPATH)
	.WillOnce(Invoke(ReturnImmediate([=](auto in __unused, auto& out) {
		SET_OUT_HEADER_LEN(out, entry);
		out.body.entry.attr.mode = mode;
		out.body.entry.nodeid = ino;
		out.body.entry.generation = 1;
		out.body.entry.attr.uid = uid;
		out.body.entry.attr_valid = UINT64_MAX;
		out.body.entry.entry_valid = 0;
	})));

	EXPECT_LOOKUP(ino, ".")
	.WillOnce(Invoke(ReturnImmediate([=](auto in __unused, auto& out) {
		SET_OUT_HEADER_LEN(out, entry);
		out.body.entry.attr.mode = mode;
		out.body.entry.nodeid = ino;
		out.body.entry.generation = 1;
		out.body.entry.attr.uid = uid;
		out.body.entry.attr_valid = UINT64_MAX;
		out.body.entry.entry_valid = 0;
	})));

	ASSERT_EQ(0, getfh(FULLPATH, &fhp)) << strerror(errno);
	ASSERT_EQ(0, fhstat(&fhp, &sb)) << strerror(errno);
	EXPECT_EQ(uid, sb.st_uid);
	EXPECT_EQ(mode, sb.st_mode);
}

/* Use a file handle whose entry is still cached */
TEST_F(Fhstat, cached)
{
	const char FULLPATH[] = "mountpoint/some_dir/.";
	const char RELDIRPATH[] = "some_dir";
	fhandle_t fhp;
	struct stat sb;
	const uint64_t ino = 42;
	const mode_t mode = S_IFDIR | 0755;

	EXPECT_LOOKUP(FUSE_ROOT_ID, RELDIRPATH)
	.WillOnce(Invoke(ReturnImmediate([=](auto in __unused, auto& out) {
		SET_OUT_HEADER_LEN(out, entry);
		out.body.entry.attr.mode = mode;
		out.body.entry.nodeid = ino;
		out.body.entry.generation = 1;
		out.body.entry.attr.ino = ino;
		out.body.entry.attr_valid = UINT64_MAX;
		out.body.entry.entry_valid = UINT64_MAX;
	})));

	ASSERT_EQ(0, getfh(FULLPATH, &fhp)) << strerror(errno);
	ASSERT_EQ(0, fhstat(&fhp, &sb)) << strerror(errno);
	EXPECT_EQ(ino, sb.st_ino);
}

/* File handle entries should expire from the cache, too */
TEST_F(Fhstat, cache_expired)
{
	const char FULLPATH[] = "mountpoint/some_dir/.";
	const char RELDIRPATH[] = "some_dir";
	fhandle_t fhp;
	struct stat sb;
	const uint64_t ino = 42;
	const mode_t mode = S_IFDIR | 0755;

	EXPECT_LOOKUP(FUSE_ROOT_ID, RELDIRPATH)
	.WillOnce(Invoke(ReturnImmediate([=](auto in __unused, auto& out) {
		SET_OUT_HEADER_LEN(out, entry);
		out.body.entry.attr.mode = mode;
		out.body.entry.nodeid = ino;
		out.body.entry.generation = 1;
		out.body.entry.attr.ino = ino;
		out.body.entry.attr_valid = UINT64_MAX;
		out.body.entry.entry_valid_nsec = NAP_NS / 2;
	})));

	EXPECT_LOOKUP(ino, ".")
	.WillOnce(Invoke(ReturnImmediate([=](auto in __unused, auto& out) {
		SET_OUT_HEADER_LEN(out, entry);
		out.body.entry.attr.mode = mode;
		out.body.entry.nodeid = ino;
		out.body.entry.generation = 1;
		out.body.entry.attr.ino = ino;
		out.body.entry.attr_valid = UINT64_MAX;
		out.body.entry.entry_valid = 0;
	})));

	ASSERT_EQ(0, getfh(FULLPATH, &fhp)) << strerror(errno);
	ASSERT_EQ(0, fhstat(&fhp, &sb)) << strerror(errno);
	EXPECT_EQ(ino, sb.st_ino);

	nap();

	/* Cache should be expired; fuse should issue a FUSE_LOOKUP */
	ASSERT_EQ(0, fhstat(&fhp, &sb)) << strerror(errno);
	EXPECT_EQ(ino, sb.st_ino);
}

/* 
 * If the server doesn't set FUSE_EXPORT_SUPPORT, then we can't do NFS-style
 * lookups
 */
TEST_F(FhstatNotExportable, lookup_dot)
{
	const char FULLPATH[] = "mountpoint/some_dir/.";
	const char RELDIRPATH[] = "some_dir";
	fhandle_t fhp;
	const uint64_t ino = 42;
	const mode_t mode = S_IFDIR | 0755;

	EXPECT_LOOKUP(FUSE_ROOT_ID, RELDIRPATH)
	.WillOnce(Invoke(ReturnImmediate([=](auto in __unused, auto& out) {
		SET_OUT_HEADER_LEN(out, entry);
		out.body.entry.attr.mode = mode;
		out.body.entry.nodeid = ino;
		out.body.entry.generation = 1;
		out.body.entry.attr_valid = UINT64_MAX;
		out.body.entry.entry_valid = 0;
	})));

	ASSERT_EQ(-1, getfh(FULLPATH, &fhp));
	ASSERT_EQ(EOPNOTSUPP, errno);
}

/* FreeBSD's fid struct doesn't have enough space for 64-bit generations */
TEST_F(Getfh, eoverflow)
{
	const char FULLPATH[] = "mountpoint/some_dir/.";
	const char RELDIRPATH[] = "some_dir";
	fhandle_t fhp;
	uint64_t ino = 42;

	EXPECT_LOOKUP(FUSE_ROOT_ID, RELDIRPATH)
	.WillOnce(Invoke(ReturnImmediate([=](auto in __unused, auto& out) {
		SET_OUT_HEADER_LEN(out, entry);
		out.body.entry.attr.mode = S_IFDIR | 0755;
		out.body.entry.nodeid = ino;
		out.body.entry.generation = (uint64_t)UINT32_MAX + 1;
		out.body.entry.attr_valid = UINT64_MAX;
		out.body.entry.entry_valid = UINT64_MAX;
	})));

	ASSERT_NE(0, getfh(FULLPATH, &fhp));
	EXPECT_EQ(EOVERFLOW, errno);
}

/* Get an NFS file handle */
TEST_F(Getfh, ok)
{
	const char FULLPATH[] = "mountpoint/some_dir/.";
	const char RELDIRPATH[] = "some_dir";
	fhandle_t fhp;
	uint64_t ino = 42;

	EXPECT_LOOKUP(FUSE_ROOT_ID, RELDIRPATH)
	.WillOnce(Invoke(ReturnImmediate([=](auto in __unused, auto& out) {
		SET_OUT_HEADER_LEN(out, entry);
		out.body.entry.attr.mode = S_IFDIR | 0755;
		out.body.entry.nodeid = ino;
		out.body.entry.attr_valid = UINT64_MAX;
		out.body.entry.entry_valid = UINT64_MAX;
	})));

	ASSERT_EQ(0, getfh(FULLPATH, &fhp)) << strerror(errno);
}

/* 
 * Call readdir via a file handle.
 *
 * This is how a userspace nfs server like nfs-ganesha or unfs3 would call
 * readdir.  The in-kernel NFS server never does any equivalent of open.  I
 * haven't discovered a way to mimic nfsd's behavior short of actually running
 * nfsd.
 */
TEST_F(Readdir, getdirentries)
{
	const char FULLPATH[] = "mountpoint/some_dir";
	const char RELPATH[] = "some_dir";
	uint64_t ino = 42;
	mode_t mode = S_IFDIR | 0755;
	fhandle_t fhp;
	int fd;
	char buf[8192];
	ssize_t r;

	EXPECT_LOOKUP(FUSE_ROOT_ID, RELPATH)
	.WillOnce(Invoke(ReturnImmediate([=](auto in __unused, auto& out) {
		SET_OUT_HEADER_LEN(out, entry);
		out.body.entry.attr.mode = mode;
		out.body.entry.nodeid = ino;
		out.body.entry.generation = 1;
		out.body.entry.attr_valid = UINT64_MAX;
		out.body.entry.entry_valid = 0;
	})));

	EXPECT_LOOKUP(ino, ".")
	.WillOnce(Invoke(ReturnImmediate([=](auto in __unused, auto& out) {
		SET_OUT_HEADER_LEN(out, entry);
		out.body.entry.attr.mode = mode;
		out.body.entry.nodeid = ino;
		out.body.entry.generation = 1;
		out.body.entry.attr_valid = UINT64_MAX;
		out.body.entry.entry_valid = 0;
	})));

	expect_opendir(ino);

	EXPECT_CALL(*m_mock, process(
		ResultOf([=](auto in) {
			return (in.header.opcode == FUSE_READDIR &&
				in.header.nodeid == ino &&
				in.body.readdir.size == sizeof(buf));
		}, Eq(true)),
		_)
	).WillOnce(Invoke(ReturnImmediate([=](auto in __unused, auto& out) {
		out.header.error = 0;
		out.header.len = sizeof(out.header);
	})));

	ASSERT_EQ(0, getfh(FULLPATH, &fhp)) << strerror(errno);
	fd = fhopen(&fhp, O_DIRECTORY);
	ASSERT_LE(0, fd) << strerror(errno);
	r = getdirentries(fd, buf, sizeof(buf), 0);
	ASSERT_EQ(0, r) << strerror(errno);

	leak(fd);
}
