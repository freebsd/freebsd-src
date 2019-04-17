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
#include <pthread.h>
#include <signal.h>
}

#include "mockfs.hh"
#include "utils.hh"

using namespace testing;

/* Don't do anything; all we care about is that the syscall gets interrupted */
void sigusr2_handler(int __unused sig) {
	if (verbosity > 1) {
		printf("Signaled!  thread %p\n", pthread_self());
	}

}

void* killer(void* target) {
	/* 
	 * Sleep for awhile so we can be mostly confident that the main thread
	 * is already blocked in write(2)
	 */
	usleep(250'000);
	if (verbosity > 1)
		printf("Signalling!  thread %p\n", target);
	pthread_kill((pthread_t)target, SIGUSR2);

	return(NULL);
}

class Interrupt: public FuseTest {
public:
pthread_t m_child;

Interrupt(): m_child(NULL) {};

void expect_lookup(const char *relpath, uint64_t ino)
{
	FuseTest::expect_lookup(relpath, ino, S_IFREG | 0644, 0, 1);
}

/* 
 * Expect a FUSE_WRITE but don't reply.  Instead, just record the unique value
 * to the provided pointer
 */
void expect_write(uint64_t ino, uint64_t *write_unique)
{
	EXPECT_CALL(*m_mock, process(
		ResultOf([=](auto in) {
			return (in->header.opcode == FUSE_WRITE &&
				in->header.nodeid == ino);
		}, Eq(true)),
		_)
	).WillOnce(Invoke([=](auto in, auto &out __unused) {
		*write_unique = in->header.unique;
	}));
}

void setup_interruptor(pthread_t self)
{
	ASSERT_EQ(0, signal(SIGUSR2, sigusr2_handler)) << strerror(errno);
	ASSERT_EQ(0, pthread_create(&m_child, NULL, killer, (void*)self))
		<< strerror(errno);
}

void TearDown() {
	struct sigaction sa;

	if (m_child != NULL) {
		pthread_join(m_child, NULL);
	}
	bzero(&sa, sizeof(sa));
	sa.sa_handler = SIG_DFL;
	sigaction(SIGUSR2, &sa, NULL);

	FuseTest::TearDown();
}
};

/* 
 * An interrupt operation that gets received after the original command is
 * complete should generate an EAGAIN response.
 */
/* https://bugs.freebsd.org/bugzilla/show_bug.cgi?id=236530 */
TEST_F(Interrupt, already_complete)
{
	const char FULLPATH[] = "mountpoint/some_file.txt";
	const char RELPATH[] = "some_file.txt";
	const char *CONTENTS = "abcdefgh";
	uint64_t ino = 42;
	int fd;
	ssize_t bufsize = strlen(CONTENTS);
	pthread_t self;
	uint64_t write_unique = 0;

	self = pthread_self();

	expect_lookup(RELPATH, ino);
	expect_open(ino, 0, 1);
	expect_write(ino, &write_unique);
	EXPECT_CALL(*m_mock, process(
		ResultOf([&](auto in) {
			return (in->header.opcode == FUSE_INTERRUPT &&
				in->body.interrupt.unique == write_unique);
		}, Eq(true)),
		_)
	).WillOnce(Invoke([&](auto in, auto &out) {
		// First complete the write request
		auto out0 = new mockfs_buf_out;
		out0->header.unique = write_unique;
		SET_OUT_HEADER_LEN(out0, write);
		out0->body.write.size = bufsize;
		out.push_back(out0);

		// Then, respond EAGAIN to the interrupt request
		auto out1 = new mockfs_buf_out;
		out1->header.unique = in->header.unique;
		out1->header.error = -EAGAIN;
		out1->header.len = sizeof(out1->header);
		out.push_back(out1);
	}));

	fd = open(FULLPATH, O_WRONLY);
	ASSERT_LE(0, fd) << strerror(errno);

	setup_interruptor(self);
	EXPECT_EQ(bufsize, write(fd, CONTENTS, bufsize)) << strerror(errno);

	/* Deliberately leak fd.  close(2) will be tested in release.cc */
}

/*
 * A FUSE filesystem is legally allowed to ignore INTERRUPT operations, and
 * complete the original operation whenever it damn well pleases.
 */
/* https://bugs.freebsd.org/bugzilla/show_bug.cgi?id=236530 */
TEST_F(Interrupt, ignore)
{
	const char FULLPATH[] = "mountpoint/some_file.txt";
	const char RELPATH[] = "some_file.txt";
	const char *CONTENTS = "abcdefgh";
	uint64_t ino = 42;
	int fd;
	ssize_t bufsize = strlen(CONTENTS);
	pthread_t self;
	uint64_t write_unique;

	self = pthread_self();

	expect_lookup(RELPATH, ino);
	expect_open(ino, 0, 1);
	expect_write(ino, &write_unique);
	EXPECT_CALL(*m_mock, process(
		ResultOf([&](auto in) {
			return (in->header.opcode == FUSE_INTERRUPT &&
				in->body.interrupt.unique == write_unique);
		}, Eq(true)),
		_)
	).WillOnce(Invoke([&](auto in __unused, auto &out) {
		// Ignore FUSE_INTERRUPT; respond to the FUSE_WRITE
		auto out0 = new mockfs_buf_out;
		out0->header.unique = write_unique;
		SET_OUT_HEADER_LEN(out0, write);
		out0->body.write.size = bufsize;
		out.push_back(out0);
	}));

	fd = open(FULLPATH, O_WRONLY);
	ASSERT_LE(0, fd) << strerror(errno);

	setup_interruptor(self);
	ASSERT_EQ(bufsize, write(fd, CONTENTS, bufsize)) << strerror(errno);

	/* Deliberately leak fd.  close(2) will be tested in release.cc */
}

/* 
 * A syscall that gets interrupted while blocking on FUSE I/O should send a
 * FUSE_INTERRUPT command to the fuse filesystem, which should then send EINTR
 * in response to the _original_ operation.  The kernel should ultimately
 * return EINTR to userspace
 */
/* https://bugs.freebsd.org/bugzilla/show_bug.cgi?id=236530 */
TEST_F(Interrupt, in_progress)
{
	const char FULLPATH[] = "mountpoint/some_file.txt";
	const char RELPATH[] = "some_file.txt";
	const char *CONTENTS = "abcdefgh";
	uint64_t ino = 42;
	int fd;
	ssize_t bufsize = strlen(CONTENTS);
	pthread_t self;
	uint64_t write_unique;

	self = pthread_self();

	expect_lookup(RELPATH, ino);
	expect_open(ino, 0, 1);
	expect_write(ino, &write_unique);
	EXPECT_CALL(*m_mock, process(
		ResultOf([&](auto in) {
			return (in->header.opcode == FUSE_INTERRUPT &&
				in->body.interrupt.unique == write_unique);
		}, Eq(true)),
		_)
	).WillOnce(Invoke([&](auto in __unused, auto &out) {
		auto out0 = new mockfs_buf_out;
		out0->header.error = -EINTR;
		out0->header.unique = write_unique;
		out0->header.len = sizeof(out0->header);
		out.push_back(out0);
	}));

	fd = open(FULLPATH, O_WRONLY);
	ASSERT_LE(0, fd) << strerror(errno);

	setup_interruptor(self);
	ASSERT_EQ(-1, write(fd, CONTENTS, bufsize));
	EXPECT_EQ(EINTR, errno);

	/* Deliberately leak fd.  close(2) will be tested in release.cc */
}

/*
 * If the FUSE filesystem receives the FUSE_INTERRUPT operation before
 * processing the original, then it should wait for "some timeout" for the
 * original operation to arrive.  If not, it should send EAGAIN to the
 * INTERRUPT operation, and the kernel should requeue the INTERRUPT.
 *
 * In this test, we'll pretend that the INTERRUPT arrives too soon, gets
 * EAGAINed, then the kernel requeues it, and the second time around it
 * successfully interrupts the original
 */
/* https://bugs.freebsd.org/bugzilla/show_bug.cgi?id=236530 */
TEST_F(Interrupt, too_soon)
{
	const char FULLPATH[] = "mountpoint/some_file.txt";
	const char RELPATH[] = "some_file.txt";
	const char *CONTENTS = "abcdefgh";
	Sequence seq;
	uint64_t ino = 42;
	int fd;
	ssize_t bufsize = strlen(CONTENTS);
	pthread_t self;
	uint64_t write_unique;

	self = pthread_self();

	expect_lookup(RELPATH, ino);
	expect_open(ino, 0, 1);
	expect_write(ino, &write_unique);

	EXPECT_CALL(*m_mock, process(
		ResultOf([&](auto in) {
			return (in->header.opcode == FUSE_INTERRUPT &&
				in->body.interrupt.unique == write_unique);
		}, Eq(true)),
		_)
	).InSequence(seq)
	.WillOnce(Invoke(ReturnErrno(EAGAIN)));

	EXPECT_CALL(*m_mock, process(
		ResultOf([&](auto in) {
			return (in->header.opcode == FUSE_INTERRUPT &&
				in->body.interrupt.unique == write_unique);
		}, Eq(true)),
		_)
	).InSequence(seq)
	.WillOnce(Invoke([&](auto in __unused, auto &out __unused) {
		auto out0 = new mockfs_buf_out;
		out0->header.error = -EINTR;
		out0->header.unique = write_unique;
		out0->header.len = sizeof(out0->header);
		out.push_back(out0);
	}));

	fd = open(FULLPATH, O_WRONLY);
	ASSERT_LE(0, fd) << strerror(errno);

	setup_interruptor(self);
	ASSERT_EQ(-1, write(fd, CONTENTS, bufsize));
	EXPECT_EQ(EINTR, errno);

	/* Deliberately leak fd.  close(2) will be tested in release.cc */
}


// TODO: add a test that uses siginterrupt and an interruptible signal
// TODO: add a test that verifies a process can be cleanly killed even if a
// FUSE_WRITE command never returns.
// TODO: write in-progress tests for read and other operations
// TODO: add a test where write returns EWOULDBLOCK
// TODO: test that if a fatal signal is received, fticket_wait_answer will
// return without waiting for a response to the interrupted operation.
// TODO: test that operations that haven't been received by the server can be
// interrupted without generating a FUSE_INTERRUPT.
