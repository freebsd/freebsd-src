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

class Link: public FuseTest {};

TEST_F(Link, emlink)
{
	const char FULLPATH[] = "mountpoint/lnk";
	const char RELPATH[] = "lnk";
	const char FULLDST[] = "mountpoint/dst";
	const char RELDST[] = "dst";
	uint64_t dst_ino = 42;

	EXPECT_LOOKUP(1, RELPATH).WillOnce(Invoke(ReturnErrno(ENOENT)));
	EXPECT_LOOKUP(1, RELDST).WillOnce(Invoke([=](auto in, auto out) {
		out->header.unique = in->header.unique;
		out->body.entry.attr.mode = S_IFREG | 0644;
		out->body.entry.nodeid = dst_ino;
		SET_OUT_HEADER_LEN(out, entry);
	}));

	EXPECT_CALL(*m_mock, process(
		ResultOf([=](auto in) {
			const char *name = (const char*)in->body.bytes
				+ sizeof(struct fuse_link_in);
			return (in->header.opcode == FUSE_LINK &&
				in->body.link.oldnodeid == dst_ino &&
				(0 == strcmp(name, RELPATH)));
		}, Eq(true)),
		_)
	).WillOnce(Invoke(ReturnErrno(EMLINK)));

	EXPECT_EQ(-1, link(FULLDST, FULLPATH));
	EXPECT_EQ(EMLINK, errno);
}

TEST_F(Link, ok)
{
	const char FULLPATH[] = "mountpoint/src";
	const char RELPATH[] = "src";
	const char FULLDST[] = "mountpoint/dst";
	const char RELDST[] = "dst";
	uint64_t dst_ino = 42;
	const uint64_t ino = 42;

	EXPECT_LOOKUP(1, RELPATH).WillOnce(Invoke(ReturnErrno(ENOENT)));
	EXPECT_LOOKUP(1, RELDST).WillOnce(Invoke([=](auto in, auto out) {
		out->header.unique = in->header.unique;
		out->body.entry.attr.mode = S_IFREG | 0644;
		out->body.entry.nodeid = dst_ino;
		SET_OUT_HEADER_LEN(out, entry);
	}));

	EXPECT_CALL(*m_mock, process(
		ResultOf([=](auto in) {
			const char *name = (const char*)in->body.bytes
				+ sizeof(struct fuse_link_in);
			return (in->header.opcode == FUSE_LINK &&
				in->body.link.oldnodeid == dst_ino &&
				(0 == strcmp(name, RELPATH)));
		}, Eq(true)),
		_)
	).WillOnce(Invoke([=](auto in, auto out) {
		out->header.unique = in->header.unique;
		SET_OUT_HEADER_LEN(out, entry);
		out->body.entry.attr.mode = S_IFREG | 0644;
		out->body.entry.nodeid = ino;
	}));

	EXPECT_EQ(0, link(FULLDST, FULLPATH)) << strerror(errno);
}
