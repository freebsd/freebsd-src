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
#include <sys/param.h>

#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/uio.h>
#include <sys/user.h>

#include <fcntl.h>
#include <libutil.h>
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
		"LOOKUP",
		"FORGET",
		"GETATTR",
		"SETATTR",
		"READLINK",
		"SYMLINK",
		"Unknown (opcode 7)",
		"MKNOD",
		"MKDIR",
		"UNLINK",
		"RMDIR",
		"RENAME",
		"LINK",
		"OPEN",
		"READ",
		"WRITE",
		"STATFS",
		"RELEASE",
		"Unknown (opcode 19)",
		"FSYNC",
		"SETXATTR",
		"GETXATTR",
		"LISTXATTR",
		"REMOVEXATTR",
		"FLUSH",
		"INIT",
		"OPENDIR",
		"READDIR",
		"RELEASEDIR",
		"FSYNCDIR",
		"GETLK",
		"SETLK",
		"SETLKW",
		"ACCESS",
		"CREATE",
		"INTERRUPT",
		"BMAP",
		"DESTROY"
	};
	if (opcode >= NUM_OPS)
		return ("Unknown (opcode > max)");
	else
		return (table[opcode]);
}

std::function<void (const struct mockfs_buf_in *in, struct mockfs_buf_out *out)>
ReturnErrno(int error)
{
	return([=](auto in, auto out) {
		out->header.unique = in->header.unique;
		out->header.error = -error;
		out->header.len = sizeof(out->header);
	});
}

/* Helper function used for returning negative cache entries for LOOKUP */
std::function<void (const struct mockfs_buf_in *in, struct mockfs_buf_out *out)>
ReturnNegativeCache(const struct timespec *entry_valid)
{
	return([=](auto in, auto out) {
		/* nodeid means ENOENT and cache it */
		out->body.entry.nodeid = 0;
		out->header.unique = in->header.unique;
		out->header.error = 0;
		out->body.entry.entry_valid = entry_valid->tv_sec;
		out->body.entry.entry_valid_nsec = entry_valid->tv_nsec;
		SET_OUT_HEADER_LEN(out, entry);
	});
}

void sigint_handler(int __unused sig) {
	quit = 1;
}

void debug_fuseop(const mockfs_buf_in *in)
{
	printf("%-11s ino=%2lu", opcode2opname(in->header.opcode),
		in->header.nodeid);
	if (verbosity > 1) {
		printf(" uid=%5u gid=%5u pid=%5u unique=%lu len=%u",
			in->header.uid, in->header.gid, in->header.pid,
			in->header.unique, in->header.len);
	}
	switch (in->header.opcode) {
		case FUSE_FSYNC:
			printf(" flags=%#x", in->body.fsync.fsync_flags);
			break;
		case FUSE_FSYNCDIR:
			printf(" flags=%#x", in->body.fsyncdir.fsync_flags);
			break;
		case FUSE_LOOKUP:
			printf(" %s", in->body.lookup);
			break;
		case FUSE_OPEN:
			printf(" flags=%#x mode=%#o",
				in->body.open.flags, in->body.open.mode);
			break;
		case FUSE_OPENDIR:
			printf(" flags=%#x mode=%#o",
				in->body.opendir.flags, in->body.opendir.mode);
			break;
		case FUSE_READ:
			printf(" offset=%lu size=%u", in->body.read.offset,
				in->body.read.size);
			break;
		case FUSE_READDIR:
			printf(" offset=%lu size=%u", in->body.readdir.offset,
				in->body.readdir.size);
			break;
		case FUSE_SETATTR:
			printf(" valid=%#x", in->body.setattr.valid);
			break;
		case FUSE_WRITE:
			printf(" offset=%lu size=%u flags=%u",
				in->body.write.offset, in->body.write.size,
				in->body.write.write_flags);
			break;
		default:
			break;
	}
	printf("\n");
}

MockFS::MockFS(int max_readahead) {
	struct iovec *iov = NULL;
	int iovlen = 0;
	char fdstr[15];

	m_daemon_id = NULL;
	m_maxreadahead = max_readahead;
	quit = 0;

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
	signal(SIGUSR1, sigint_handler);
	if (pthread_create(&m_daemon_id, NULL, service, (void*)this))
		throw(std::system_error(errno, std::system_category(),
			"Couldn't Couldn't start fuse thread"));
}

MockFS::~MockFS() {
	kill_daemon();
	::unmount("mountpoint", MNT_FORCE);
	rmdir("mountpoint");
}

void MockFS::init() {
	mockfs_buf_in *in;
	mockfs_buf_out *out;

	in = (mockfs_buf_in*) malloc(sizeof(*in));
	ASSERT_TRUE(in != NULL);
	out = (mockfs_buf_out*) malloc(sizeof(*out));
	ASSERT_TRUE(out != NULL);

	read_request(in);
	ASSERT_EQ(FUSE_INIT, in->header.opcode);

	memset(out, 0, sizeof(*out));
	out->header.unique = in->header.unique;
	out->header.error = 0;
	out->body.init.major = FUSE_KERNEL_VERSION;
	out->body.init.minor = FUSE_KERNEL_MINOR_VERSION;

	/*
	 * The default max_write is set to this formula in libfuse, though
	 * individual filesystems can lower it.  The "- 4096" was added in
	 * commit 154ffe2, with the commit message "fix".
	 */
	uint32_t default_max_write = 32 * getpagesize() + 0x1000 - 4096;
	/* For testing purposes, it should be distinct from MAXPHYS */
	m_max_write = MIN(default_max_write, MAXPHYS / 2);
	out->body.init.max_write = m_max_write;

	out->body.init.max_readahead = m_maxreadahead;
	SET_OUT_HEADER_LEN(out, init);
	write(m_fuse_fd, out, out->header.len);

	free(in);
}

void MockFS::kill_daemon() {
	if (m_daemon_id != NULL) {
		pthread_kill(m_daemon_id, SIGUSR1);
		// Closing the /dev/fuse file descriptor first allows unmount
		// to succeed even if the daemon doesn't correctly respond to
		// commands during the unmount sequence.
		close(m_fuse_fd);
		pthread_join(m_daemon_id, NULL);
		m_daemon_id = NULL;
	}
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
		if (verbosity > 0)
			debug_fuseop(in);
		if (pid_ok((pid_t)in->header.pid)) {
			process(in, &out);
		} else {
			/* 
			 * Reject any requests from unknown processes.  Because
			 * we actually do mount a filesystem, plenty of
			 * unrelated system daemons may try to access it.
			 */
			process_default(in, &out);
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

bool MockFS::pid_ok(pid_t pid) {
	if (pid == m_pid) {
		return (true);
	} else {
		struct kinfo_proc *ki;
		bool ok = false;

		ki = kinfo_getproc(pid);
		if (ki == NULL)
			return (false);
		/* 
		 * Allow access by the aio daemon processes so that our tests
		 * can use aio functions
		 */
		if (0 == strncmp("aiod", ki->ki_comm, 4))
			ok = true;
		free(ki);
		return (ok);
	}
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

	mock_fs->loop();

	return (NULL);
}

void MockFS::unmount() {
	::unmount("mountpoint", 0);
}
