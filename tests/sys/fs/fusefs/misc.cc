/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 Alan Somers
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

/* Miscellaneous tests that don't relate to any particular fuse operation */

extern "C" {
#include <sys/wait.h>

#include <fcntl.h>
#include <unistd.h>
}

#include "mockfs.hh"
#include "utils.hh"

using namespace testing;


class Execv: public FuseTest {
public:
virtual void SetUp() {
	/* Enable FUSE_ASYNC_READ to allow shared vnode locks */
	m_init_flags = FUSE_ASYNC_READ;
	FuseTest::SetUp();
}
};
class Fexecv: public Execv {};

class FexecvDefaultPermissions: public Fexecv {
virtual void SetUp() {
	m_default_permissions = true;
	Fexecv::SetUp();
}
};

/*
 * Execute a file mounted on a fusefs file system.  The server should get the
 * FUSE_RELEASE request when sys_fexecve closes the file.
 *
 * Crucially, execve ignores the file system's MNTK_EXTENDED_SHARED flag.
 *
 * Regression test for https://bugs.freebsd.org/bugzilla/show_bug.cgi?id=295957
 */
TEST_F(Execv, close)
{
	const static char FULLPATH[] = "mountpoint/true";
	const static char RELPATH[] = "true";
	const static size_t BUFSIZE = 16384;
	FILE *true_file;
	uint64_t ino = 42;
	size_t true_len;
	int status;
	char *buf;

	buf = new char[BUFSIZE];
	true_file = fopen("/usr/bin/true", "r");
	ASSERT_TRUE(true_file) << strerror(errno);
	true_len = fread(buf, 1, BUFSIZE, true_file);
	ASSERT_LT(true_len, BUFSIZE) << "Must increase BUFSIZE";
	fclose(true_file);

	fork(false, &status, [&] {
		expect_lookup(RELPATH, ino, S_IFREG | 0755, true_len, 1,
				UINT64_MAX);
		expect_open(ino, 0, 1);
		expect_read(ino, 0, true_len, true_len, buf);
		expect_flush(ino, 1, ReturnErrno(ENOSYS));
		EXPECT_CALL(*m_mock, process(
			ResultOf([=](auto in) {
				return (in.header.opcode == FUSE_RELEASE &&
					in.header.nodeid == ino);
			}, Eq(true)),
			_)
		).Times(1)
		.WillRepeatedly(Invoke(ReturnErrno(0)));

	}, [&] {
		char *const argv[] = {__DECONST(char *, "true"), NULL};
		char *const env[] = {NULL};

		execve(FULLPATH, argv, env);
		fprintf(stderr, "execv: %s\n", strerror(errno));
		return 1;
	});
	ASSERT_EQ(0, WEXITSTATUS(status));

	delete[] buf;
}

TEST_F(Fexecv, close)
{
	const static char FULLPATH[] = "mountpoint/true";
	const static char RELPATH[] = "true";
	const static size_t BUFSIZE = 16384;
	FILE *true_file;
	uint64_t ino = 42;
	size_t true_len;
	int status;
	char *buf;

	buf = new char[BUFSIZE];
	true_file = fopen("/usr/bin/true", "r");
	ASSERT_TRUE(true_file) << strerror(errno);
	true_len = fread(buf, 1, BUFSIZE, true_file);
	ASSERT_LT(true_len, BUFSIZE) << "Must increase BUFSIZE";
	fclose(true_file);

	fork(false, &status, [&] {
		expect_lookup(RELPATH, ino, S_IFREG | 0755, true_len, 1,
				UINT64_MAX);
		expect_open(ino, 0, 2);
		expect_read(ino, 0, true_len, true_len, buf);
		expect_flush(ino, 1, ReturnErrno(ENOSYS));
		EXPECT_CALL(*m_mock, process(
			ResultOf([=](auto in) {
				return (in.header.opcode == FUSE_RELEASE &&
					in.header.nodeid == ino);
			}, Eq(true)),
			_)
		).Times(2)
		.WillRepeatedly(Invoke(ReturnErrno(0)));

	}, [&] {
		char *const argv[] = {__DECONST(char *, "true"), NULL};
		char *const env[] = {NULL};
		int fd;

		fd = open(FULLPATH, O_EXEC);
		if (fd < 0) {
			fprintf(stderr, "open: %s\n", strerror(errno));
			return 1;
		}
		fexecve(fd, argv, env);
		fprintf(stderr, "execv: %s\n", strerror(errno));
		return 1;
	});
	ASSERT_EQ(0, WEXITSTATUS(status));

	delete[] buf;
}

/* 
 * Execute a file stored on a fusefs file system that does not implement
 * FUSE_OPEN
 */
TEST_F(Fexecv, close_noopen)
{
	const static char FULLPATH[] = "mountpoint/true";
	const static char RELPATH[] = "true";
	const static size_t BUFSIZE = 16384;
	FILE *true_file;
	uint64_t ino = 42;
	size_t true_len;
	int status;
	char *buf;

	buf = new char[BUFSIZE];
	true_file = fopen("/usr/bin/true", "r");
	ASSERT_TRUE(true_file) << strerror(errno);
	true_len = fread(buf, 1, BUFSIZE, true_file);
	ASSERT_LT(true_len, BUFSIZE) << "Must increase BUFSIZE";
	fclose(true_file);

	fork(false, &status, [&] {
		expect_lookup(RELPATH, ino, S_IFREG | 0755, true_len, 1,
				UINT64_MAX);
		EXPECT_CALL(*m_mock, process(
			ResultOf([=](auto in) {
				return (in.header.opcode == FUSE_OPEN &&
					in.header.nodeid == ino);
			}, Eq(true)),
			_)
		).Times(1)
		.WillOnce(Invoke(ReturnErrno(ENOSYS)));
		expect_read(ino, 0, true_len, true_len, buf, -1, 0);
		expect_flush(ino, 1, ReturnErrno(ENOSYS));
	}, [&] {
		char *const argv[] = {__DECONST(char *, "true"), NULL};
		char *const env[] = {NULL};
		int fd;

		fd = open(FULLPATH, O_EXEC);
		if (fd < 0) {
			fprintf(stderr, "open: %s\n", strerror(errno));
			return 1;
		}
		fexecve(fd, argv, env);
		fprintf(stderr, "execv: %s\n", strerror(errno));
		return 1;
	});
	ASSERT_EQ(0, WEXITSTATUS(status));

	delete[] buf;
}

/*
 * When execute a file with a dirty atime, fusefs must send FUSE_SETATTR to the
 * daemon during close.
 */
TEST_F(FexecvDefaultPermissions, atime)
{
	const static char FULLPATH[] = "mountpoint/true";
	const static char RELPATH[] = "true";
	const static size_t BUFSIZE = 16384;
	FILE *true_file;
	uint64_t ino = 42;
	size_t true_len;
	int status;
	char *buf;

	buf = new char[BUFSIZE];
	true_file = fopen("/usr/bin/true", "r");
	ASSERT_TRUE(true_file) << strerror(errno);
	true_len = fread(buf, 1, BUFSIZE, true_file);
	ASSERT_LT(true_len, BUFSIZE) << "Must increase BUFSIZE";
	fclose(true_file);

	fork(false, &status, [&] {
		expect_lookup(RELPATH, ino, S_IFREG | 0777, true_len, 1,
				UINT64_MAX);
		EXPECT_CALL(*m_mock, process(
			ResultOf([=](auto in) {
				return (in.header.opcode == FUSE_GETATTR &&
					in.header.nodeid == FUSE_ROOT_ID);
			}, Eq(true)),
			_)
		).WillRepeatedly(Invoke(ReturnImmediate([=](auto i __unused, auto& out) {
			SET_OUT_HEADER_LEN(out, attr);
			out.body.attr.attr.ino = FUSE_ROOT_ID;
			out.body.attr.attr.mode = S_IFDIR | 0777;
			out.body.attr.attr.size = 0;
			out.body.attr.attr_valid = UINT64_MAX;
		})));
		EXPECT_CALL(*m_mock, process(
			ResultOf([=](auto in) {
				return (in.header.opcode == FUSE_OPEN &&
					in.header.nodeid == ino);
			}, Eq(true)),
			_)
		).Times(1)
		.WillOnce(Invoke(ReturnErrno(ENOSYS)));
		expect_read(ino, 0, true_len, true_len, buf, -1, 0);
		expect_flush(ino, 1, ReturnErrno(ENOSYS));
		EXPECT_CALL(*m_mock, process(
			ResultOf([&](auto in) {
				return (in.header.opcode == FUSE_SETATTR &&
					in.header.nodeid == ino &&
					in.body.setattr.valid == FATTR_ATIME);
			}, Eq(true)),
			_)
		).WillOnce(Invoke(ReturnImmediate([=](auto in __unused, auto& out) {
			SET_OUT_HEADER_LEN(out, attr);
			out.body.attr.attr.ino = ino;
			out.body.attr.attr.mode = S_IFREG | 0777;
		})));
	}, [&] {
		char *const argv[] = {__DECONST(char *, "true"), NULL};
		char *const env[] = {NULL};
		char cbuf[8];
		int fd;

		/* Note that fexecve doesn't actually require O_EXEC */
		fd = open(FULLPATH, O_RDONLY);
		if (fd < 0) {
			fprintf(stderr, "open: %s\n", strerror(errno));
			return 1;
		}
		/* Read a few bytes, just to dirty the file's atime */
		if (read(fd, cbuf, sizeof(cbuf)) < 0) {
			fprintf(stderr, "read: %s\n", strerror(errno));
			return 1;
		}
		fexecve(fd, argv, env);
		fprintf(stderr, "execv: %s\n", strerror(errno));
		return 1;
	});
	ASSERT_EQ(0, WEXITSTATUS(status));

	delete[] buf;
}
