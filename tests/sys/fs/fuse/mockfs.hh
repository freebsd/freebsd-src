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
#include <sys/types.h>

#include <pthread.h>

#include "fuse_kernel.h"
}

#include <gmock/gmock.h>

#define TIME_T_MAX (std::numeric_limits<time_t>::max())

#define SET_OUT_HEADER_LEN(out, variant) { \
	(out)->header.len = (sizeof((out)->header) + \
			     sizeof((out)->body.variant)); \
}

/*
 * Create an expectation on FUSE_LOOKUP and return it so the caller can set
 * actions.
 *
 * This must be a macro instead of a method because EXPECT_CALL returns a type
 * with a deleted constructor.
 */
#define EXPECT_LOOKUP(parent, path)					\
	EXPECT_CALL(*m_mock, process(					\
		ResultOf([=](auto in) {					\
			return (in->header.opcode == FUSE_LOOKUP &&	\
				in->header.nodeid == (parent) &&	\
				strcmp(in->body.lookup, (path)) == 0);	\
		}, Eq(true)),						\
		_)							\
	)

extern int verbosity;

/* This struct isn't defined by fuse_kernel.h or libfuse, but it should be */
struct fuse_create_out {
	struct fuse_entry_out	entry;
	struct fuse_open_out	open;
};

union fuse_payloads_in {
	fuse_access_in	access;
	/* value is from fuse_kern_chan.c in fusefs-libs */
	uint8_t		bytes[0x21000 - sizeof(struct fuse_in_header)];
	fuse_forget_in	forget;
	fuse_init_in	init;
	fuse_link_in	link;
	char		lookup[0];
	fuse_mkdir_in	mkdir;
	fuse_mknod_in	mknod;
	fuse_open_in	open;
	fuse_rename_in	rename;
	char		rmdir[0];
	fuse_setattr_in	setattr;
	char		unlink[0];
};

struct mockfs_buf_in {
	fuse_in_header		header;
	union fuse_payloads_in	body;
};

union fuse_payloads_out {
	fuse_attr_out		attr;
	fuse_create_out		create;
	fuse_entry_out		entry;
	fuse_init_out		init;
	fuse_open_out		open;
	fuse_statfs_out		statfs;
	/*
	 * The protocol places no limits on the length of the string.  This is
	 * merely convenient for testing.
	 */
	char			str[80];
};

struct mockfs_buf_out {
	fuse_out_header		header;
	union fuse_payloads_out	body;
};

/*
 * Helper function used for setting an error expectation for any fuse operation.
 * The operation will return the supplied error
 */
std::function<void (const struct mockfs_buf_in *in, struct mockfs_buf_out *out)>
ReturnErrno(int error);

/* Helper function used for returning negative cache entries for LOOKUP */
std::function<void (const struct mockfs_buf_in *in, struct mockfs_buf_out *out)>
ReturnNegativeCache(const struct timespec *entry_valid);

/*
 * Fake FUSE filesystem
 *
 * "Mounts" a filesystem to a temporary directory and services requests
 * according to the programmed expectations.
 *
 * Operates directly on the fuse(4) kernel API, not the libfuse(3) user api.
 */
class MockFS {
	public:
	/*
	 * thread id of the fuse daemon thread
	 *
	 * It must run in a separate thread so it doesn't deadlock with the
	 * client test code.
	 */
	pthread_t m_daemon_id;

	private:
	/* file descriptor of /dev/fuse control device */
	int m_fuse_fd;
	
	/* pid of the test process */
	pid_t m_pid;

	/* Initialize a session after mounting */
	void init();

	/* Default request handler */
	void process_default(const mockfs_buf_in*, mockfs_buf_out*);

	/* Entry point for the daemon thread */
	static void* service(void*);

	/* Read, but do not process, a single request from the kernel */
	void read_request(mockfs_buf_in*);

	public:
	/* Create a new mockfs and mount it to a tempdir */
	MockFS();
	virtual ~MockFS();

	/* Kill the filesystem daemon without unmounting the filesystem */
	void kill_daemon();

	/* Process FUSE requests endlessly */
	void loop();

	/* 
	 * Request handler
	 *
	 * This method is expected to provide the response to each FUSE
	 * operation.  Responses must be immediate (so this method can't be used
	 * for testing a daemon with queue depth > 1).  Test cases must define
	 * each response using Googlemock expectations
	 */
	MOCK_METHOD2(process, void(const mockfs_buf_in*, mockfs_buf_out*));

	/* Gracefully unmount */
	void unmount();
};
