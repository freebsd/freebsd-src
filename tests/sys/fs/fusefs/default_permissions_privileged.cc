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

/*
 * Tests for the "default_permissions" mount option that require a privileged
 * user.
 */

extern "C" {
#include <sys/types.h>
#include <sys/extattr.h>

#include <fcntl.h>
#include <unistd.h>
}

#include "mockfs.hh"
#include "utils.hh"

using namespace testing;

class DefaultPermissionsPrivileged: public FuseTest {
virtual void SetUp() {
	m_default_permissions = true;
	FuseTest::SetUp();
	if (HasFatalFailure() || IsSkipped())
		return;

	if (geteuid() != 0) {
		GTEST_SKIP() << "This test requires a privileged user";
	}
	
	/* With -o default_permissions, FUSE_ACCESS should never be called */
	EXPECT_CALL(*m_mock, process(
		ResultOf([=](auto in) {
			return (in.header.opcode == FUSE_ACCESS);
		}, Eq(true)),
		_)
	).Times(0);
}

public:
void expect_getattr(uint64_t ino, mode_t mode, uint64_t attr_valid, int times,
	uid_t uid = 0, gid_t gid = 0)
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
		out.body.attr.attr.ino = ino;	// Must match nodeid
		out.body.attr.attr.mode = mode;
		out.body.attr.attr.size = 0;
		out.body.attr.attr.uid = uid;
		out.body.attr.attr.uid = gid;
		out.body.attr.attr_valid = attr_valid;
	})));
}

void expect_lookup(const char *relpath, uint64_t ino, mode_t mode,
	uint64_t attr_valid, uid_t uid = 0, gid_t gid = 0)
{
	FuseTest::expect_lookup(relpath, ino, mode, 0, 1, attr_valid, uid, gid);
}

};

class Setattr: public DefaultPermissionsPrivileged {};

TEST_F(Setattr, sticky_regular_file)
{
	const char FULLPATH[] = "mountpoint/some_file.txt";
	const char RELPATH[] = "some_file.txt";
	const uint64_t ino = 42;
	const mode_t oldmode = 0644;
	const mode_t newmode = 01644;

	expect_getattr(1, S_IFDIR | 0755, UINT64_MAX, 1);
	expect_lookup(RELPATH, ino, S_IFREG | oldmode, UINT64_MAX, geteuid());
	EXPECT_CALL(*m_mock, process(
		ResultOf([](auto in) {
			return (in.header.opcode == FUSE_SETATTR);
		}, Eq(true)),
		_)
	).WillOnce(Invoke(ReturnImmediate([](auto in __unused, auto& out) {
		SET_OUT_HEADER_LEN(out, attr);
		out.body.attr.attr.mode = S_IFREG | newmode;
	})));

	EXPECT_EQ(0, chmod(FULLPATH, newmode)) << strerror(errno);
}


