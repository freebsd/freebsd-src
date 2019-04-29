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
#include <sys/types.h>
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
};

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
	pthread_t th0;
	mode_t mode = S_IFIFO | 0755;
	const int bufsize = 80;
	char message[bufsize];
	ssize_t recvd = 0, r;
	uint64_t ino = 42;
	int fd;

	expect_lookup(RELPATH, ino, mode, 0, 1);

	fd = open(FULLPATH, O_RDWR);
	ASSERT_LE(0, fd) << strerror(errno);
	ASSERT_EQ(0, pthread_create(&th0, NULL, writer, &fd))
		<< strerror(errno);
	while (recvd < msgsize) {
		r = read(fd, message + recvd, bufsize - recvd);
		ASSERT_LE(0, r) << strerror(errno);
		ASSERT_LT(0, r) << "unexpected EOF";
		recvd += r;
	}
	ASSERT_STREQ(message, MESSAGE);

	/* Deliberately leak fd */
}
