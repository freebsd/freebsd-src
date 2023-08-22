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

extern "C" {
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <fcntl.h>
}

#include "mockfs.hh"
#include "utils.hh"

using namespace testing;

const char FULLPATH[] = "mountpoint/some_fifo";
const char RELPATH[] = "some_fifo";
const char MESSAGE[] = "Hello, World!\n";
const int msgsize = sizeof(MESSAGE);

class Fifo: public FuseTest {
public:
pthread_t m_child;

Fifo(): m_child(NULL) {};

void TearDown() {
	if (m_child != NULL) {
		pthread_join(m_child, NULL);
	}
	FuseTest::TearDown();
}
};

class Socket: public Fifo {};

/* Writer thread */
static void* writer(void* arg) {
	ssize_t sent = 0;
	int fd;

	fd = *(int*)arg;
	while (sent < msgsize) {
		ssize_t r;

		r = write(fd, MESSAGE + sent, msgsize - sent);
		if (r < 0)
			return (void*)(intptr_t)errno;
		else
			sent += r;
		
	}
	return 0;
}

/* 
 * Reading and writing FIFOs works.  None of the I/O actually goes through FUSE
 */
TEST_F(Fifo, read_write)
{
	mode_t mode = S_IFIFO | 0755;
	const int bufsize = 80;
	char message[bufsize];
	ssize_t recvd = 0, r;
	uint64_t ino = 42;
	int fd;

	expect_lookup(RELPATH, ino, mode, 0, 1);

	fd = open(FULLPATH, O_RDWR);
	ASSERT_LE(0, fd) << strerror(errno);
	ASSERT_EQ(0, pthread_create(&m_child, NULL, writer, &fd))
		<< strerror(errno);
	while (recvd < msgsize) {
		r = read(fd, message + recvd, bufsize - recvd);
		ASSERT_LE(0, r) << strerror(errno);
		ASSERT_LT(0, r) << "unexpected EOF";
		recvd += r;
	}
	ASSERT_STREQ(message, MESSAGE);

	leak(fd);
}

/* Writer thread */
static void* socket_writer(void* arg __unused) {
	ssize_t sent = 0;
	int fd, err;
	struct sockaddr_un sa;

	fd = socket(AF_UNIX, SOCK_STREAM, 0);
	if (fd < 0) {
		perror("socket");
		return (void*)(intptr_t)errno;
	}
	sa.sun_family = AF_UNIX;
	strlcpy(sa.sun_path, FULLPATH, sizeof(sa.sun_path));
	sa.sun_len = sizeof(FULLPATH);
	err = connect(fd, (struct sockaddr*)&sa, sizeof(sa));
	if (err < 0) {
		perror("connect");
		return (void*)(intptr_t)errno;
	}

	while (sent < msgsize) {
		ssize_t r;

		r = write(fd, MESSAGE + sent, msgsize - sent);
		if (r < 0)
			return (void*)(intptr_t)errno;
		else
			sent += r;
		
	}

	FuseTest::leak(fd);
	return 0;
}

/* 
 * Reading and writing unix-domain sockets works.  None of the I/O actually
 * goes through FUSE.
 */
TEST_F(Socket, read_write)
{
	mode_t mode = S_IFSOCK | 0755;
	const int bufsize = 80;
	char message[bufsize];
	struct sockaddr_un sa;
	ssize_t recvd = 0, r;
	uint64_t ino = 42;
	int fd, connected;
	Sequence seq;

	EXPECT_LOOKUP(FUSE_ROOT_ID, RELPATH)
		.WillOnce(Invoke(ReturnErrno(ENOENT)));
	EXPECT_CALL(*m_mock, process(
		ResultOf([=](auto in) {
			return (in.header.opcode == FUSE_MKNOD);
		}, Eq(true)),
		_)
	).InSequence(seq)
	.WillOnce(Invoke(ReturnImmediate([=](auto in __unused, auto& out) {
		SET_OUT_HEADER_LEN(out, entry);
		out.body.entry.attr.mode = mode;
		out.body.entry.nodeid = ino;
		out.body.entry.entry_valid = UINT64_MAX;
		out.body.entry.attr_valid = UINT64_MAX;
	})));

	EXPECT_LOOKUP(FUSE_ROOT_ID, RELPATH)
	.InSequence(seq)
	.WillOnce(Invoke(ReturnImmediate([=](auto in __unused, auto& out) {
		SET_OUT_HEADER_LEN(out, entry);
		out.body.entry.attr.mode = mode;
		out.body.entry.nodeid = ino;
		out.body.entry.attr.nlink = 1;
		out.body.entry.attr_valid = UINT64_MAX;
		out.body.entry.entry_valid = UINT64_MAX;
	})));

	fd = socket(AF_UNIX, SOCK_STREAM, 0);
	ASSERT_LE(0, fd) << strerror(errno);
	sa.sun_family = AF_UNIX;
	strlcpy(sa.sun_path, FULLPATH, sizeof(sa.sun_path));
	sa.sun_len = sizeof(FULLPATH);
	ASSERT_EQ(0, bind(fd, (struct sockaddr*)&sa, sizeof(sa)))
		<< strerror(errno);
	listen(fd, 5);
	ASSERT_EQ(0, pthread_create(&m_child, NULL, socket_writer, NULL))
		<< strerror(errno);
	connected = accept(fd, 0, 0);
	ASSERT_LE(0, connected) << strerror(errno);

	while (recvd < msgsize) {
		r = read(connected, message + recvd, bufsize - recvd);
		ASSERT_LE(0, r) << strerror(errno);
		ASSERT_LT(0, r) << "unexpected EOF";
		recvd += r;
	}
	ASSERT_STREQ(message, MESSAGE);

	leak(fd);
}
