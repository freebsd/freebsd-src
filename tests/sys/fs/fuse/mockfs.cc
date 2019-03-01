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
#include <sys/param.h>

#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/uio.h>

#include <fcntl.h>
#include <pthread.h>
#include <signal.h>
#include <stdlib.h>
#include <unistd.h>

#include "mntopts.h"	// for build_iovec
}

#include <gtest/gtest.h>

#include "mockfs.hh"

using namespace testing;

int verbosity = 0;
static sig_atomic_t quit = 0;

const char* opcode2opname(uint32_t opcode)
{
	const int NUM_OPS = 39;
	const char* table[NUM_OPS] = {
		"Unknown (opcode 0)",
		"FUSE_LOOKUP",
		"FUSE_FORGET",
		"FUSE_GETATTR",
		"FUSE_SETATTR",
		"FUSE_READLINK",
		"FUSE_SYMLINK",
		"Unknown (opcode 7)",
		"FUSE_MKNOD",
		"FUSE_MKDIR",
		"FUSE_UNLINK",
		"FUSE_RMDIR",
		"FUSE_RENAME",
		"FUSE_LINK",
		"FUSE_OPEN",
		"FUSE_READ",
		"FUSE_WRITE",
		"FUSE_STATFS",
		"FUSE_RELEASE",
		"Unknown (opcode 19)",
		"FUSE_FSYNC",
		"FUSE_SETXATTR",
		"FUSE_GETXATTR",
		"FUSE_LISTXATTR",
		"FUSE_REMOVEXATTR",
		"FUSE_FLUSH",
		"FUSE_INIT",
		"FUSE_OPENDIR",
		"FUSE_READDIR",
		"FUSE_RELEASEDIR",
		"FUSE_FSYNCDIR",
		"FUSE_GETLK",
		"FUSE_SETLK",
		"FUSE_SETLKW",
		"FUSE_ACCESS",
		"FUSE_CREATE",
		"FUSE_INTERRUPT",
		"FUSE_BMAP",
		"FUSE_DESTROY"
	};
	if (opcode >= NUM_OPS)
		return ("Unknown (opcode > max)");
	else
		return (table[opcode]);
}

void sigint_handler(int __unused sig) {
	quit = 1;
}

MockFS::MockFS() {
	struct iovec *iov = NULL;
	int iovlen = 0;
	char fdstr[15];

	/*
	 * Kyua sets pwd to a testcase-unique tempdir; no need to use
	 * mkdtemp
	 */
	/*
	 * googletest doesn't allow ASSERT_ in constructors, so we must throw
	 * instead.
	 */
	if (mkdir("mountpoint" , 0644) && errno != EEXIST)
		throw(std::system_error(errno, std::system_category(),
			"Couldn't make mountpoint directory"));

	m_fuse_fd = open("/dev/fuse", O_RDWR);
	if (m_fuse_fd < 0)
		throw(std::system_error(errno, std::system_category(),
			"Couldn't open /dev/fuse"));
	sprintf(fdstr, "%d", m_fuse_fd);

	m_pid = getpid();

	build_iovec(&iov, &iovlen, "fstype", __DECONST(void *, "fusefs"), -1);
	build_iovec(&iov, &iovlen, "fspath",
		    __DECONST(void *, "mountpoint"), -1);
	build_iovec(&iov, &iovlen, "from", __DECONST(void *, "/dev/fuse"), -1);
	build_iovec(&iov, &iovlen, "fd", fdstr, -1);
	if (nmount(iov, iovlen, 0))
		throw(std::system_error(errno, std::system_category(),
			"Couldn't mount filesystem"));

	// Setup default handler
	ON_CALL(*this, process(_, _))
		.WillByDefault(Invoke(this, &MockFS::process_default));

	init();
	if (pthread_create(&m_thr, NULL, service, (void*)this))
		throw(std::system_error(errno, std::system_category(),
			"Couldn't Couldn't start fuse thread"));
}

MockFS::~MockFS() {
	pthread_kill(m_daemon_id, SIGUSR1);
	// Closing the /dev/fuse file descriptor first allows unmount to
	// succeed even if the daemon doesn't correctly respond to commands
	// during the unmount sequence.
	close(m_fuse_fd);
	pthread_join(m_daemon_id, NULL);
	::unmount("mountpoint", MNT_FORCE);
	rmdir("mountpoint");
}

void MockFS::init() {
	mockfs_buf_in *in;
	mockfs_buf_out out;

	in = (mockfs_buf_in*) malloc(sizeof(*in));
	ASSERT_TRUE(in != NULL);

	read_request(in);
	ASSERT_EQ(FUSE_INIT, in->header.opcode);

	memset(&out, 0, sizeof(out));
	out.header.unique = in->header.unique;
	out.header.error = 0;
	out.body.init.major = FUSE_KERNEL_VERSION;
	out.body.init.minor = FUSE_KERNEL_MINOR_VERSION;
	SET_OUT_HEADER_LEN(&out, init);
	write(m_fuse_fd, &out, out.header.len);

	free(in);
}

void MockFS::loop() {
	mockfs_buf_in *in;
	mockfs_buf_out out;

	in = (mockfs_buf_in*) malloc(sizeof(*in));
	ASSERT_TRUE(in != NULL);
	while (!quit) {
		bzero(in, sizeof(*in));
		bzero(&out, sizeof(out));
		read_request(in);
		if (quit)
			break;
		if (verbosity > 0) {
			printf("Got request %s\n",
				opcode2opname(in->header.opcode));
		}
		if ((pid_t)in->header.pid != m_pid) {
			/* 
			 * Reject any requests from unknown processes.  Because
			 * we actually do mount a filesystem, plenty of
			 * unrelated system daemons may try to access it.
			 */
			process_default(in, &out);
		} else {
			process(in, &out);
		}
		if (in->header.opcode == FUSE_FORGET) {
			/*Alone among the opcodes, FORGET expects no response*/
			continue;
		}
		ASSERT_TRUE(write(m_fuse_fd, &out, out.header.len) > 0 ||
			    errno == EAGAIN)
			<< strerror(errno);
	}
	free(in);
}

void MockFS::process_default(const mockfs_buf_in *in, mockfs_buf_out* out) {
	out->header.unique = in->header.unique;
	out->header.error = -EOPNOTSUPP;
	out->header.len = sizeof(out->header);
}

void MockFS::read_request(mockfs_buf_in *in) {
	ssize_t res;

	res = read(m_fuse_fd, in, sizeof(*in));
	if (res < 0 && !quit)
		perror("read");
	ASSERT_TRUE(res >= (ssize_t)sizeof(in->header) || quit);
}

void* MockFS::service(void *pthr_data) {
	MockFS *mock_fs = (MockFS*)pthr_data;
	mock_fs->m_daemon_id = pthread_self();

	quit = 0;
	signal(SIGUSR1, sigint_handler);

	mock_fs->loop();

	return (NULL);
}

void MockFS::unmount() {
	::unmount("mountpoint", 0);
}
