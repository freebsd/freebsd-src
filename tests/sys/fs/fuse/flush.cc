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
#include <fcntl.h>
#include <unistd.h>
}

#include "mockfs.hh"
#include "utils.hh"

using namespace testing;

class Flush: public FuseTest {

const static uint64_t FH = 0xdeadbeef1a7ebabe;

public:
void expect_flush(uint64_t ino, int times, ProcessMockerT r)
{
	EXPECT_CALL(*m_mock, process(
		ResultOf([=](auto in) {
			return (in->header.opcode == FUSE_FLUSH &&
				in->header.nodeid == ino &&
				in->body.flush.fh == Flush::FH);
		}, Eq(true)),
		_)
	).Times(times)
	.WillRepeatedly(Invoke(r));
}

void expect_getattr(uint64_t ino)
{
	/* Until the attr cache is working, we may send an additional GETATTR */
	EXPECT_CALL(*m_mock, process(
		ResultOf([=](auto in) {
			return (in->header.opcode == FUSE_GETATTR &&
				in->header.nodeid == ino);
		}, Eq(true)),
		_)
	).WillRepeatedly(Invoke([=](auto in, auto out) {
		out->header.unique = in->header.unique;
		SET_OUT_HEADER_LEN(out, attr);
		out->body.attr.attr.ino = ino;	// Must match nodeid
		out->body.attr.attr.mode = S_IFREG | 0644;
	}));

}

void expect_lookup(const char *relpath, uint64_t ino)
{
	EXPECT_LOOKUP(1, relpath).WillRepeatedly(Invoke([=](auto in, auto out) {
		out->header.unique = in->header.unique;
		SET_OUT_HEADER_LEN(out, entry);
		out->body.entry.attr.mode = S_IFREG | 0644;
		out->body.entry.nodeid = ino;
		out->body.entry.attr_valid = UINT64_MAX;
	}));
}

void expect_open(uint64_t ino, int times)
{
	EXPECT_CALL(*m_mock, process(
		ResultOf([=](auto in) {
			return (in->header.opcode == FUSE_OPEN &&
				in->header.nodeid == ino);
		}, Eq(true)),
		_)
	).Times(times)
	.WillRepeatedly(Invoke([](auto in, auto out) {
		out->header.unique = in->header.unique;
		out->header.len = sizeof(out->header);
		SET_OUT_HEADER_LEN(out, open);
		out->body.open.fh = Flush::FH;
	}));

}

/*
 * When testing FUSE_FLUSH, the FUSE_RELEASE calls are uninteresting.  This
 * expectation will silence googlemock warnings
 */
void expect_release()
{
	EXPECT_CALL(*m_mock, process(
		ResultOf([=](auto in) {
			return (in->header.opcode == FUSE_RELEASE);
		}, Eq(true)),
		_)
	).WillRepeatedly(Invoke(ReturnErrno(0)));
}
};

// TODO: lock_owner stuff

/* If a file descriptor is duplicated, every close causes FLUSH */
/* https://bugs.freebsd.org/bugzilla/show_bug.cgi?id=236405 */
TEST_F(Flush, DISABLED_dup)
{
	const char FULLPATH[] = "mountpoint/some_file.txt";
	const char RELPATH[] = "some_file.txt";
	uint64_t ino = 42;
	int fd, fd2;

	expect_lookup(RELPATH, ino);
	expect_open(ino, 1);
	expect_getattr(ino);
	expect_flush(ino, 2, ReturnErrno(0));
	expect_release();

	fd = open(FULLPATH, O_WRONLY);
	EXPECT_LE(0, fd) << strerror(errno);

	fd2 = dup(fd);

	ASSERT_EQ(0, close(fd2)) << strerror(errno);
	ASSERT_EQ(0, close(fd)) << strerror(errno);
}

/*
 * Some FUSE filesystem cache data internally and flush it on release.  Such
 * filesystems may generate errors during release.  On Linux, these get
 * returned by close(2).  However, POSIX does not require close(2) to return
 * this error.  FreeBSD's fuse(4) should return EIO if it returns an error at
 * all.
 */
/* http://pubs.opengroup.org/onlinepubs/9699919799/functions/close.html */
/* https://bugs.freebsd.org/bugzilla/show_bug.cgi?id=236405 */
TEST_F(Flush, DISABLED_eio)
{
	const char FULLPATH[] = "mountpoint/some_file.txt";
	const char RELPATH[] = "some_file.txt";
	uint64_t ino = 42;
	int fd;

	expect_lookup(RELPATH, ino);
	expect_open(ino, 1);
	expect_getattr(ino);
	expect_flush(ino, 1, ReturnErrno(EIO));
	expect_release();

	fd = open(FULLPATH, O_WRONLY);
	EXPECT_LE(0, fd) << strerror(errno);

	ASSERT_TRUE(0 == close(fd) || errno == EIO) << strerror(errno);
}

/* A FUSE_FLUSH should be sent on close(2) */
/* https://bugs.freebsd.org/bugzilla/show_bug.cgi?id=236405 */
TEST_F(Flush, DISABLED_flush)
{
	const char FULLPATH[] = "mountpoint/some_file.txt";
	const char RELPATH[] = "some_file.txt";
	uint64_t ino = 42;
	int fd;

	expect_lookup(RELPATH, ino);
	expect_open(ino, 1);
	expect_getattr(ino);
	expect_flush(ino, 1, ReturnErrno(0));
	expect_release();

	fd = open(FULLPATH, O_WRONLY);
	EXPECT_LE(0, fd) << strerror(errno);

	ASSERT_TRUE(0 == close(fd)) << strerror(errno);
}
