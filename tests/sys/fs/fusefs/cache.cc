/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
 * Copyright (c) 2020 Alan Somers
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
#include <sys/param.h>
#include <fcntl.h>
}

#include "mockfs.hh"
#include "utils.hh"

/*
 * Tests for thorny cache problems not specific to any one opcode
 */

using namespace testing;

/*
 * Parameters
 * - reopen file	- If true, close and reopen the file between reads
 * - cache lookups	- If true, allow lookups to be cached
 * - cache attrs	- If true, allow file attributes to be cached
 * - cache_mode		- uncached, writeback, or writethrough
 * - initial size	- File size before truncation
 * - truncated size	- File size after truncation
 */
typedef tuple<tuple<bool, bool, bool>, cache_mode, ssize_t, ssize_t> CacheParam;

class Cache: public FuseTest, public WithParamInterface<CacheParam> {
public:
bool m_direct_io;

Cache(): m_direct_io(false) {};

virtual void SetUp() {
	int cache_mode = get<1>(GetParam());
	switch (cache_mode) {
		case Uncached:
			m_direct_io = true;
			break;
		case WritebackAsync:
			m_async = true;
			/* FALLTHROUGH */
		case Writeback:
			m_init_flags |= FUSE_WRITEBACK_CACHE;
			/* FALLTHROUGH */
		case Writethrough:
			break;
		default:
			FAIL() << "Unknown cache mode";
	}
	m_noatime = true;	// To prevent SETATTR for atime on close

	FuseTest::SetUp();
	if (IsSkipped())
		return;
}

void expect_getattr(uint64_t ino, int times, uint64_t size, uint64_t attr_valid)
{
	EXPECT_CALL(*m_mock, process(
		ResultOf([=](auto in) {
			return (in.header.opcode == FUSE_GETATTR &&
				in.header.nodeid == ino);
		}, Eq(true)),
		_)
	).Times(times)
	.WillRepeatedly(Invoke(ReturnImmediate([=](auto i __unused, auto& out) {
		SET_OUT_HEADER_LEN(out, attr);
		out.body.attr.attr_valid = attr_valid;
		out.body.attr.attr.ino = ino;
		out.body.attr.attr.mode = S_IFREG | 0644;
		out.body.attr.attr.size = size;
	})));
}

void expect_lookup(const char *relpath, uint64_t ino,
	uint64_t size, uint64_t entry_valid, uint64_t attr_valid)
{
	EXPECT_LOOKUP(FUSE_ROOT_ID, relpath)
	.WillRepeatedly(Invoke(ReturnImmediate([=](auto in __unused, auto& out) {
		SET_OUT_HEADER_LEN(out, entry);
		out.body.entry.attr.mode = S_IFREG | 0644;
		out.body.entry.nodeid = ino;
		out.body.entry.attr.nlink = 1;
		out.body.entry.attr_valid = attr_valid;
		out.body.entry.attr.size = size;
		out.body.entry.entry_valid = entry_valid;
	})));
}

void expect_open(uint64_t ino, int times)
{
	FuseTest::expect_open(ino, m_direct_io ? FOPEN_DIRECT_IO: 0, times);
}

void expect_release(uint64_t ino, ProcessMockerT r)
{
	EXPECT_CALL(*m_mock, process(
		ResultOf([=](auto in) {
			return (in.header.opcode == FUSE_RELEASE &&
				in.header.nodeid == ino);
		}, Eq(true)),
		_)
	).WillRepeatedly(Invoke(r));
}

};

// If the server truncates the file behind the kernel's back, the kernel should
// invalidate cached pages beyond the new EOF
TEST_P(Cache, truncate_by_surprise_invalidates_cache)
{
	const char FULLPATH[] = "mountpoint/some_file.txt";
	const char RELPATH[] = "some_file.txt";
	const char *CONTENTS = "abcdefghijklmnopqrstuvwxyz";
	uint64_t ino = 42;
	uint64_t attr_valid, entry_valid;
	int fd;
	ssize_t bufsize = strlen(CONTENTS);
	uint8_t buf[bufsize];
	bool reopen = get<0>(get<0>(GetParam()));
	bool cache_lookups = get<1>(get<0>(GetParam()));
	bool cache_attrs = get<2>(get<0>(GetParam()));
	ssize_t osize = get<2>(GetParam());
	ssize_t nsize = get<3>(GetParam());

	ASSERT_LE(osize, bufsize);
	ASSERT_LE(nsize, bufsize);
	if (cache_attrs)
		attr_valid = UINT64_MAX;
	else
		attr_valid = 0;
	if (cache_lookups)
		entry_valid = UINT64_MAX;
	else
		entry_valid = 0;

	expect_lookup(RELPATH, ino, osize, entry_valid, attr_valid);
	expect_open(ino, 1);
	if (!cache_attrs)
		expect_getattr(ino, 2, osize, attr_valid);
	expect_read(ino, 0, osize, osize, CONTENTS);

	fd = open(FULLPATH, O_RDONLY);
	ASSERT_LE(0, fd) << strerror(errno);

	ASSERT_EQ(osize, read(fd, buf, bufsize)) << strerror(errno);
	ASSERT_EQ(0, memcmp(buf, CONTENTS, osize));

	// Now truncate the file behind the kernel's back.  The next read
	// should discard cache and fetch from disk again.
	if (reopen) {
		// Close and reopen the file
		expect_flush(ino, 1, ReturnErrno(ENOSYS));
		expect_release(ino, ReturnErrno(0));
		ASSERT_EQ(0, close(fd));
		expect_lookup(RELPATH, ino, nsize, entry_valid, attr_valid);
		expect_open(ino, 1);
		fd = open(FULLPATH, O_RDONLY);
		ASSERT_LE(0, fd) << strerror(errno);
	}

	if (!cache_attrs)
		expect_getattr(ino, 1, nsize, attr_valid);
	expect_read(ino, 0, nsize, nsize, CONTENTS);
	ASSERT_EQ(0, lseek(fd, 0, SEEK_SET));
	ASSERT_EQ(nsize, read(fd, buf, bufsize)) << strerror(errno);
	ASSERT_EQ(0, memcmp(buf, CONTENTS, nsize));

	leak(fd);
}

INSTANTIATE_TEST_CASE_P(Cache, Cache,
	Combine(
		/* Test every combination that:
		 * - does not cache at least one of entries and attrs
		 * - either doesn't cache attrs, or reopens the file
		 * In the other combinations, the kernel will never learn that
		 * the file's size has changed.
		 */
		Values(
			std::make_tuple(false, false, false),
			std::make_tuple(false, true, false),
			std::make_tuple(true, false, false),
			std::make_tuple(true, false, true),
			std::make_tuple(true, true, false)
		),
		Values(Writethrough, Writeback),
		/* Test both reductions and extensions to file size */
		Values(20),
		Values(10, 25)
	)
);
