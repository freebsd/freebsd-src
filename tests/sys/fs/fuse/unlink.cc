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
#include <fcntl.h>
}

#include "mockfs.hh"
#include "utils.hh"

using namespace testing;

class Unlink: public FuseTest {};

TEST_F(Unlink, eperm)
{
	const char FULLPATH[] = "mountpoint/some_file.txt";
	const char RELPATH[] = "some_file.txt";
	uint64_t ino = 42;

	EXPECT_LOOKUP(1, RELPATH).WillOnce(Invoke([=](auto in, auto out) {
		out->header.unique = in->header.unique;
		SET_OUT_HEADER_LEN(out, entry);
		out->body.entry.attr.mode = S_IFREG | 0644;
		out->body.entry.nodeid = ino;
		out->body.entry.attr.nlink = 1;
	}));
	EXPECT_CALL(*m_mock, process(
		ResultOf([=](auto in) {
			return (in->header.opcode == FUSE_UNLINK &&
				0 == strcmp(RELPATH, in->body.unlink) &&
				in->header.nodeid == 1);
		}, Eq(true)),
		_)
	).WillOnce(Invoke(ReturnErrno(EPERM)));

	ASSERT_NE(0, unlink(FULLPATH));
	ASSERT_EQ(EPERM, errno);
}

TEST_F(Unlink, ok)
{
	const char FULLPATH[] = "mountpoint/some_file.txt";
	const char RELPATH[] = "some_file.txt";
	uint64_t ino = 42;

	EXPECT_LOOKUP(1, RELPATH).WillOnce(Invoke([=](auto in, auto out) {
		out->header.unique = in->header.unique;
		SET_OUT_HEADER_LEN(out, entry);
		out->body.entry.attr.mode = S_IFREG | 0644;
		out->body.entry.nodeid = ino;
		out->body.entry.attr.nlink = 1;
	}));
	EXPECT_CALL(*m_mock, process(
		ResultOf([=](auto in) {
			return (in->header.opcode == FUSE_UNLINK &&
				0 == strcmp(RELPATH, in->body.unlink) &&
				in->header.nodeid == 1);
		}, Eq(true)),
		_)
	).WillOnce(Invoke(ReturnErrno(0)));

	ASSERT_EQ(0, unlink(FULLPATH)) << strerror(errno);
}
