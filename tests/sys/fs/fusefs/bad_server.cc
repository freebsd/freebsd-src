/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2023 Axcient
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
#include <fcntl.h>
#include <unistd.h>
}

#include "mockfs.hh"
#include "utils.hh"

using namespace testing;

class BadServer: public FuseTest {};

/*
 * If the server sends a response for an unknown request, the kernel should
 * gracefully return EINVAL.
 */
TEST_F(BadServer, UnknownUnique)
{
	mockfs_buf_out out;

	out.header.len = sizeof(out.header);
	out.header.error = 0;
	out.header.unique = 99999;		// Invalid!
	out.expected_errno = EINVAL;
	m_mock->write_response(out);
}

/*
 * If the server sends less than a header's worth of data, the kernel should
 * gracefully return EINVAL.
 */
TEST_F(BadServer, ShortWrite)
{
	mockfs_buf_out out;

	out.header.len = sizeof(out.header) - 1;
	out.header.error = 0;
	out.header.unique = 0;			// Asynchronous notification
	out.expected_errno = EINVAL;
	m_mock->write_response(out);
}

/*
 * It is illegal to report an error, and also send back a payload.
 */
TEST_F(BadServer, ErrorWithPayload)
{
	const char FULLPATH[] = "mountpoint/some_file.txt";
	const char RELPATH[] = "some_file.txt";

	EXPECT_LOOKUP(FUSE_ROOT_ID, RELPATH)
	.WillOnce(Invoke([&](auto in, auto &out) {
		// First send an invalid response
		std::unique_ptr<mockfs_buf_out> out0(new mockfs_buf_out);
		out0->header.unique = in.header.unique;
		out0->header.error = -ENOENT;
		SET_OUT_HEADER_LEN(*out0, entry);	// Invalid!
		out0->expected_errno = EINVAL;
		out.push_back(std::move(out0));

		// Then, respond to the lookup so we can complete the test
		std::unique_ptr<mockfs_buf_out> out1(new mockfs_buf_out);
		out1->header.unique = in.header.unique;
		out1->header.error = -ENOENT;
		out1->header.len = sizeof(out1->header);
		out.push_back(std::move(out1));

		// The kernel may disconnect us for bad behavior, so don't try
		// to read any more.
		m_mock->m_quit = true;
	}));

	EXPECT_NE(0, access(FULLPATH, F_OK));

	EXPECT_EQ(ENOENT, errno);
}
