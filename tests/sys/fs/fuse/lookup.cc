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
 */

extern "C" {
#include <unistd.h>
}

#include "mockfs.hh"
#include "utils.hh"

using namespace testing;

class Lookup: public FuseTest {};

/*
 * If lookup returns a non-zero cache timeout, then subsequent VOP_GETATTRs
 * should use the cached attributes, rather than query the daemon
 */
/* https://bugs.freebsd.org/bugzilla/show_bug.cgi?id=235775 */
TEST_F(Lookup, DISABLED_attr_cache)
{
	const char FULLPATH[] = "mountpoint/some_file.txt";
	const char RELPATH[] = "some_file.txt";
	const uint64_t ino = 42;
	struct stat sb;

	EXPECT_LOOKUP(1, RELPATH).WillOnce(Invoke([=](auto in, auto out) {
		out->header.unique = in->header.unique;
		SET_OUT_HEADER_LEN(out, entry);
		out->body.entry.nodeid = ino;
		out->body.entry.attr_valid = UINT64_MAX;
		out->body.entry.attr.ino = ino;	// Must match nodeid
		out->body.entry.attr.mode = S_IFREG | 0644;
		out->body.entry.attr.size = 1;
		out->body.entry.attr.blocks = 2;
		out->body.entry.attr.atime = 3;
		out->body.entry.attr.mtime = 4;
		out->body.entry.attr.ctime = 5;
		out->body.entry.attr.atimensec = 6;
		out->body.entry.attr.mtimensec = 7;
		out->body.entry.attr.ctimensec = 8;
		out->body.entry.attr.nlink = 9;
		out->body.entry.attr.uid = 10;
		out->body.entry.attr.gid = 11;
		out->body.entry.attr.rdev = 12;
	}));
	/* stat(2) issues a VOP_LOOKUP followed by a VOP_GETATTR */
	ASSERT_EQ(0, stat(FULLPATH, &sb)) << strerror(errno);
	EXPECT_EQ(1, sb.st_size);
	EXPECT_EQ(2, sb.st_blocks);
	EXPECT_EQ(3, sb.st_atim.tv_sec);
	EXPECT_EQ(6, sb.st_atim.tv_nsec);
	EXPECT_EQ(4, sb.st_mtim.tv_sec);
	EXPECT_EQ(7, sb.st_mtim.tv_nsec);
	EXPECT_EQ(5, sb.st_ctim.tv_sec);
	EXPECT_EQ(8, sb.st_ctim.tv_nsec);
	EXPECT_EQ(9ull, sb.st_nlink);
	EXPECT_EQ(10ul, sb.st_uid);
	EXPECT_EQ(11ul, sb.st_gid);
	EXPECT_EQ(12ul, sb.st_rdev);
	EXPECT_EQ(ino, sb.st_ino);
	EXPECT_EQ(S_IFREG | 0644, sb.st_mode);

	// fuse(4) does not _yet_ support inode generations
	//EXPECT_EQ(generation, sb.st_gen);

	//st_birthtim and st_flags are not supported by protocol 7.8.  They're
	//only supported as OS-specific extensions to OSX.
	//EXPECT_EQ(, sb.st_birthtim);
	//EXPECT_EQ(, sb.st_flags);
	
	//FUSE can't set st_blksize until protocol 7.9
}

/*
 * If lookup returns a finite but non-zero cache timeout, then we should discard
 * the cached attributes and requery the daemon.
 */
/* https://bugs.freebsd.org/bugzilla/show_bug.cgi?id=235773 */
TEST_F(Lookup, attr_cache_timeout)
{
	const char FULLPATH[] = "mountpoint/some_file.txt";
	const char RELPATH[] = "some_file.txt";
	const uint64_t ino = 42;
	struct stat sb;
	/* 
	 * The timeout should be longer than the longest plausible time the
	 * daemon would take to complete a write(2) to /dev/fuse, but no longer.
	 */
	long timeout_ns = 250'000'000;

	EXPECT_LOOKUP(1, RELPATH).WillRepeatedly(Invoke([=](auto in, auto out) {
		out->header.unique = in->header.unique;
		SET_OUT_HEADER_LEN(out, entry);
		out->body.entry.nodeid = ino;
		out->body.entry.attr_valid_nsec = timeout_ns;
		out->body.entry.attr.ino = ino;	// Must match nodeid
		out->body.entry.attr.mode = S_IFREG | 0644;
	}));
	EXPECT_CALL(*m_mock, process(
		ResultOf([](auto in) {
			return (in->header.opcode == FUSE_GETATTR &&
				in->header.nodeid == ino);
		}, Eq(true)),
		_)
	).WillOnce(Invoke([](auto in, auto out) {
		out->header.unique = in->header.unique;
		SET_OUT_HEADER_LEN(out, attr);
		out->body.attr.attr.ino = ino;	// Must match nodeid
		out->body.attr.attr.mode = S_IFREG | 0644;
	}));

	/* access(2) will issue a VOP_LOOKUP but not a VOP_GETATTR */
	ASSERT_EQ(0, access(FULLPATH, F_OK)) << strerror(errno);
	usleep(2 * timeout_ns / 1000);
	/* The cache has timed out; VOP_GETATTR should query the daemon*/
	ASSERT_EQ(0, stat(FULLPATH, &sb)) << strerror(errno);
}

TEST_F(Lookup, enoent)
{
	const char FULLPATH[] = "mountpoint/does_not_exist";
	const char RELPATH[] = "does_not_exist";

	EXPECT_LOOKUP(1, RELPATH).WillOnce(Invoke(ReturnErrno(ENOENT)));
	EXPECT_NE(0, access(FULLPATH, F_OK));
	EXPECT_EQ(ENOENT, errno);
}

/*
 * If lookup returns a non-zero entry timeout, then subsequent VOP_LOOKUPs
 * should use the cached inode rather than requery the daemon
 */
TEST_F(Lookup, entry_cache)
{
	const char FULLPATH[] = "mountpoint/some_file.txt";
	const char RELPATH[] = "some_file.txt";

	EXPECT_LOOKUP(1, RELPATH).WillOnce(Invoke([=](auto in, auto out) {
		out->header.unique = in->header.unique;
		SET_OUT_HEADER_LEN(out, entry);
		out->body.entry.entry_valid = UINT64_MAX;
		out->body.entry.attr.mode = S_IFREG | 0644;
		out->body.entry.nodeid = 14;
	}));
	ASSERT_EQ(0, access(FULLPATH, F_OK)) << strerror(errno);
	/* The second access(2) should use the cache */
	ASSERT_EQ(0, access(FULLPATH, F_OK)) << strerror(errno);
}

/* 
 * If the daemon returns an error of 0 and an inode of 0, that's a flag for
 * "ENOENT and cache it" with the given entry_timeout
 */
/* https://bugs.freebsd.org/bugzilla/show_bug.cgi?id=236226 */
TEST_F(Lookup, DISABLED_entry_cache_negative)
{
	EXPECT_LOOKUP(1, "does_not_exist").Times(1)
	.WillOnce(Invoke([](auto in, auto out) {
		out->header.unique = in->header.unique;
		out->header.error = 0;
		out->body.entry.nodeid = 0;
		out->body.entry.entry_valid = UINT64_MAX;
		SET_OUT_HEADER_LEN(out, entry);
	}));
	EXPECT_NE(0, access("mountpoint/does_not_exist", F_OK));
	EXPECT_EQ(ENOENT, errno);
	EXPECT_NE(0, access("mountpoint/does_not_exist", F_OK));
	EXPECT_EQ(ENOENT, errno);
}

/* Negative entry caches should timeout, too */
TEST_F(Lookup, entry_cache_negative_timeout)
{
	const char *RELPATH = "does_not_exist";
	const char *FULLPATH = "mountpoint/does_not_exist";
	/* 
	 * The timeout should be longer than the longest plausible time the
	 * daemon would take to complete a write(2) to /dev/fuse, but no longer.
	 */
	long timeout_ns = 250'000'000;

	EXPECT_LOOKUP(1, RELPATH).Times(2)
	.WillRepeatedly(Invoke([=](auto in, auto out) {
		out->header.unique = in->header.unique;
		out->header.error = 0;
		out->body.entry.nodeid = 0;
		out->body.entry.entry_valid_nsec = timeout_ns;
		SET_OUT_HEADER_LEN(out, entry);
	}));
	EXPECT_NE(0, access(FULLPATH, F_OK));
	EXPECT_EQ(ENOENT, errno);

	usleep(2 * timeout_ns / 1000);

	/* The cache has timed out; VOP_LOOKUP should requery the daemon*/
	EXPECT_NE(0, access(FULLPATH, F_OK));
	EXPECT_EQ(ENOENT, errno);
}

/*
 * If lookup returns a finite but non-zero entry cache timeout, then we should
 * discard the cached inode and requery the daemon
 */
/* https://bugs.freebsd.org/bugzilla/show_bug.cgi?id=235773 */
TEST_F(Lookup, DISABLED_entry_cache_timeout)
{
	const char FULLPATH[] = "mountpoint/some_file.txt";
	const char RELPATH[] = "some_file.txt";
	/* 
	 * The timeout should be longer than the longest plausible time the
	 * daemon would take to complete a write(2) to /dev/fuse, but no longer.
	 */
	long timeout_ns = 250'000'000;

	EXPECT_LOOKUP(1, RELPATH).Times(2)
	.WillRepeatedly(Invoke([=](auto in, auto out) {
		out->header.unique = in->header.unique;
		SET_OUT_HEADER_LEN(out, entry);
		out->body.entry.entry_valid_nsec = timeout_ns;
		out->body.entry.attr.mode = S_IFREG | 0644;
		out->body.entry.nodeid = 14;
	}));
	ASSERT_EQ(0, access(FULLPATH, F_OK)) << strerror(errno);
	usleep(2 * timeout_ns / 1000);
	/* The cache has timed out; VOP_LOOKUP should query the daemon*/
	ASSERT_EQ(0, access(FULLPATH, F_OK)) << strerror(errno);
}

TEST_F(Lookup, ok)
{
	const char FULLPATH[] = "mountpoint/some_file.txt";
	const char RELPATH[] = "some_file.txt";

	EXPECT_LOOKUP(1, RELPATH).WillOnce(Invoke([=](auto in, auto out) {
		out->header.unique = in->header.unique;
		SET_OUT_HEADER_LEN(out, entry);
		out->body.entry.attr.mode = S_IFREG | 0644;
		out->body.entry.nodeid = 14;
	}));
	/*
	 * access(2) is one of the few syscalls that will not (always) follow
	 * up a successful VOP_LOOKUP with another VOP.
	 */
	ASSERT_EQ(0, access(FULLPATH, F_OK)) << strerror(errno);
}

// Lookup in a subdirectory of the fuse mount
TEST_F(Lookup, subdir)
{
	const char FULLPATH[] = "mountpoint/some_dir/some_file.txt";
	const char DIRPATH[] = "some_dir";
	const char RELPATH[] = "some_file.txt";
	uint64_t dir_ino = 2;
	uint64_t file_ino = 3;

	EXPECT_LOOKUP(1, DIRPATH).WillOnce(Invoke([=](auto in, auto out) {
		out->header.unique = in->header.unique;
		SET_OUT_HEADER_LEN(out, entry);
		out->body.entry.attr.mode = S_IFDIR | 0755;
		out->body.entry.nodeid = dir_ino;
	}));
	EXPECT_LOOKUP(dir_ino, RELPATH).WillOnce(Invoke([=](auto in, auto out) {
		out->header.unique = in->header.unique;
		SET_OUT_HEADER_LEN(out, entry);
		out->body.entry.attr.mode = S_IFREG | 0644;
		out->body.entry.nodeid = file_ino;
	}));
	/*
	 * access(2) is one of the few syscalls that will not (always) follow
	 * up a successful VOP_LOOKUP with another VOP.
	 */
	ASSERT_EQ(0, access(FULLPATH, F_OK)) << strerror(errno);
}


