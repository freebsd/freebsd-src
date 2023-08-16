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

/*
 * Tests for the "allow_other" mount option.  They must be in their own
 * file so they can be run as root
 */

extern "C" {
#include <sys/types.h>
#include <sys/extattr.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <unistd.h>
}

#include "mockfs.hh"
#include "utils.hh"

using namespace testing;

const static char FULLPATH[] = "mountpoint/some_file.txt";
const static char RELPATH[] = "some_file.txt";

class NoAllowOther: public FuseTest {

public:
/* Unprivileged user id */
int m_uid;

virtual void SetUp() {
	if (geteuid() != 0) {
		GTEST_SKIP() << "This test must be run as root";
	}

	FuseTest::SetUp();
}
};

class AllowOther: public NoAllowOther {

public:
virtual void SetUp() {
	m_allow_other = true;
	NoAllowOther::SetUp();
}
};

TEST_F(AllowOther, allowed)
{
	int status;

	fork(true, &status, [&] {
			uint64_t ino = 42;

			expect_lookup(RELPATH, ino, S_IFREG | 0644, 0, 1);
			expect_open(ino, 0, 1);
			expect_flush(ino, 1, ReturnErrno(0));
			expect_release(ino, FH);
		}, []() {
			int fd;

			fd = open(FULLPATH, O_RDONLY);
			if (fd < 0) {
				perror("open");
				return(1);
			}

			leak(fd);
			return 0;
		}
	);
	ASSERT_EQ(0, WEXITSTATUS(status));
}

/* Check that fusefs uses the correct credentials for FUSE operations */
TEST_F(AllowOther, creds)
{
	int status;
	uid_t uid;
	gid_t gid;

	get_unprivileged_id(&uid, &gid);
	fork(true, &status, [=] {
			EXPECT_CALL(*m_mock, process( ResultOf([=](auto in) {
				return (in.header.opcode == FUSE_LOOKUP &&
					in.header.uid == uid &&
					in.header.gid == gid);
				}, Eq(true)),
				_)
			).Times(1)
			.WillOnce(Invoke(ReturnErrno(ENOENT)));
		}, []() {
			eaccess(FULLPATH, F_OK);
			return 0;
		}
	);
	ASSERT_EQ(0, WEXITSTATUS(status));
}

/*
 * A variation of the Open.multiple_creds test showing how the bug can lead to a
 * privilege elevation.  The first process is privileged and opens a file only
 * visible to root.  The second process is unprivileged and shouldn't be able
 * to open the file, but does thanks to the bug
 */
TEST_F(AllowOther, privilege_escalation)
{
	int fd1, status;
	const static uint64_t ino = 42;
	const static uint64_t fh = 100;

	/* Fork a child to open the file with different credentials */
	fork(true, &status, [&] {

		expect_lookup(RELPATH, ino, S_IFREG | 0600, 0, 2);
		EXPECT_CALL(*m_mock, process(
			ResultOf([=](auto in) {
				return (in.header.opcode == FUSE_OPEN &&
					in.header.pid == (uint32_t)getpid() &&
					in.header.uid == (uint32_t)geteuid() &&
					in.header.nodeid == ino);
			}, Eq(true)),
			_)
		).WillOnce(Invoke(
			ReturnImmediate([](auto in __unused, auto& out) {
			out.body.open.fh = fh;
			out.header.len = sizeof(out.header);
			SET_OUT_HEADER_LEN(out, open);
		})));

		EXPECT_CALL(*m_mock, process(
			ResultOf([=](auto in) {
				return (in.header.opcode == FUSE_OPEN &&
					in.header.pid != (uint32_t)getpid() &&
					in.header.uid != (uint32_t)geteuid() &&
					in.header.nodeid == ino);
			}, Eq(true)),
			_)
		).Times(AnyNumber())
		.WillRepeatedly(Invoke(ReturnErrno(EPERM)));

		fd1 = open(FULLPATH, O_RDONLY);
		ASSERT_LE(0, fd1) << strerror(errno);
	}, [] {
		int fd0;

		fd0 = open(FULLPATH, O_RDONLY);
		if (fd0 >= 0) {
			fprintf(stderr, "Privilege escalation!\n");
			return 1;
		}
		if (errno != EPERM) {
			fprintf(stderr, "Unexpected error %s\n",
				strerror(errno));
			return 1;
		}
		leak(fd0);
		return 0;
	}
	);
	ASSERT_EQ(0, WEXITSTATUS(status));
	leak(fd1);
}

TEST_F(NoAllowOther, disallowed)
{
	int status;

	fork(true, &status, [] {
		}, []() {
			int fd;

			fd = open(FULLPATH, O_RDONLY);
			if (fd >= 0) {
				fprintf(stderr, "open should've failed\n");
				leak(fd);
				return(1);
			} else if (errno != EPERM) {
				fprintf(stderr, "Unexpected error: %s\n",
					strerror(errno));
				return(1);
			}
			return 0;
		}
	);
	ASSERT_EQ(0, WEXITSTATUS(status));
}

/* 
 * When -o allow_other is not used, users other than the owner aren't allowed
 * to open anything inside of the mount point, not just the mountpoint itself
 * This is a regression test for bug 237052
 */
TEST_F(NoAllowOther, disallowed_beneath_root)
{
	const static char RELPATH2[] = "other_dir";
	const static uint64_t ino = 42;
	const static uint64_t ino2 = 43;
	int dfd, status;

	expect_lookup(RELPATH, ino, S_IFDIR | 0755, 0, 1);
	EXPECT_LOOKUP(ino, RELPATH2)
	.WillRepeatedly(Invoke(ReturnImmediate([=](auto in __unused, auto& out) {
		SET_OUT_HEADER_LEN(out, entry);
		out.body.entry.attr.mode = S_IFREG | 0644;
		out.body.entry.nodeid = ino2;
		out.body.entry.attr.nlink = 1;
		out.body.entry.attr_valid = UINT64_MAX;
	})));
	expect_opendir(ino);
	dfd = open(FULLPATH, O_DIRECTORY);
	ASSERT_LE(0, dfd) << strerror(errno);

	fork(true, &status, [] {
		}, [&]() {
			int fd;

			fd = openat(dfd, RELPATH2, O_RDONLY);
			if (fd >= 0) {
				fprintf(stderr, "openat should've failed\n");
				leak(fd);
				return(1);
			} else if (errno != EPERM) {
				fprintf(stderr, "Unexpected error: %s\n",
					strerror(errno));
				return(1);
			}
			return 0;
		}
	);
	ASSERT_EQ(0, WEXITSTATUS(status));

	leak(dfd);
}

/* 
 * Provide coverage for the extattr methods, which have a slightly different
 * code path
 */
TEST_F(NoAllowOther, setextattr)
{
	int ino = 42, status;

	fork(true, &status, [&] {
			EXPECT_LOOKUP(FUSE_ROOT_ID, RELPATH)
			.WillOnce(Invoke(
			ReturnImmediate([=](auto in __unused, auto& out) {
				SET_OUT_HEADER_LEN(out, entry);
				out.body.entry.attr_valid = UINT64_MAX;
				out.body.entry.entry_valid = UINT64_MAX;
				out.body.entry.attr.mode = S_IFREG | 0644;
				out.body.entry.nodeid = ino;
			})));

			/*
			 * lookup the file to get it into the cache.
			 * Otherwise, the unprivileged lookup will fail with
			 * EACCES
			 */
			ASSERT_EQ(0, access(FULLPATH, F_OK)) << strerror(errno);
		}, [&]() {
			const char value[] = "whatever";
			ssize_t value_len = strlen(value) + 1;
			int ns = EXTATTR_NAMESPACE_USER;
			ssize_t r;

			r = extattr_set_file(FULLPATH, ns, "foo",
				(const void*)value, value_len);
			if (r >= 0) {
				fprintf(stderr, "should've failed\n");
				return(1);
			} else if (errno != EPERM) {
				fprintf(stderr, "Unexpected error: %s\n",
					strerror(errno));
				return(1);
			}
			return 0;
		}
	);
	ASSERT_EQ(0, WEXITSTATUS(status));
}
