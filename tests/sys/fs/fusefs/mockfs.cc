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

extern "C" {
#include <sys/param.h>

#include <sys/mount.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <sys/uio.h>
#include <sys/user.h>

#include <fcntl.h>
#include <libutil.h>
#include <poll.h>
#include <pthread.h>
#include <signal.h>
#include <stdlib.h>
#include <unistd.h>

#include "mntopts.h"	// for build_iovec
}

#include <cinttypes>

#include <gtest/gtest.h>

#include "mockfs.hh"

using namespace testing;

int verbosity = 0;

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

ProcessMockerT
ReturnErrno(int error)
{
	return([=](auto in, auto &out) {
		std::unique_ptr<mockfs_buf_out> out0(new mockfs_buf_out);
		out0->header.unique = in.header.unique;
		out0->header.error = -error;
		out0->header.len = sizeof(out0->header);
		out.push_back(std::move(out0));
	});
}

/* Helper function used for returning negative cache entries for LOOKUP */
ProcessMockerT
ReturnNegativeCache(const struct timespec *entry_valid)
{
	return([=](auto in, auto &out) {
		/* nodeid means ENOENT and cache it */
		std::unique_ptr<mockfs_buf_out> out0(new mockfs_buf_out);
		out0->body.entry.nodeid = 0;
		out0->header.unique = in.header.unique;
		out0->header.error = 0;
		out0->body.entry.entry_valid = entry_valid->tv_sec;
		out0->body.entry.entry_valid_nsec = entry_valid->tv_nsec;
		SET_OUT_HEADER_LEN(*out0, entry);
		out.push_back(std::move(out0));
	});
}

ProcessMockerT
ReturnImmediate(std::function<void(const mockfs_buf_in& in,
				   struct mockfs_buf_out &out)> f)
{
	return([=](auto& in, auto &out) {
		std::unique_ptr<mockfs_buf_out> out0(new mockfs_buf_out);
		out0->header.unique = in.header.unique;
		f(in, *out0);
		out.push_back(std::move(out0));
	});
}

void sigint_handler(int __unused sig) {
	// Don't do anything except interrupt the daemon's read(2) call
}

void MockFS::debug_request(const mockfs_buf_in &in, ssize_t buflen)
{
	printf("%-11s ino=%2" PRIu64, opcode2opname(in.header.opcode),
		in.header.nodeid);
	if (verbosity > 1) {
		printf(" uid=%5u gid=%5u pid=%5u unique=%" PRIu64 " len=%u"
			" buflen=%zd",
			in.header.uid, in.header.gid, in.header.pid,
			in.header.unique, in.header.len, buflen);
	}
	switch (in.header.opcode) {
		const char *name, *value;

		case FUSE_ACCESS:
			printf(" mask=%#x", in.body.access.mask);
			break;
		case FUSE_BMAP:
			printf(" block=%" PRIx64 " blocksize=%#x",
				in.body.bmap.block, in.body.bmap.blocksize);
			break;
		case FUSE_CREATE:
			if (m_kernel_minor_version >= 12)
				name = (const char*)in.body.bytes +
					sizeof(fuse_create_in);
			else
				name = (const char*)in.body.bytes +
					sizeof(fuse_open_in);
			printf(" flags=%#x name=%s",
				in.body.open.flags, name);
			break;
		case FUSE_FLUSH:
			printf(" fh=%#" PRIx64 " lock_owner=%" PRIu64,
				in.body.flush.fh,
				in.body.flush.lock_owner);
			break;
		case FUSE_FORGET:
			printf(" nlookup=%" PRIu64, in.body.forget.nlookup);
			break;
		case FUSE_FSYNC:
			printf(" flags=%#x", in.body.fsync.fsync_flags);
			break;
		case FUSE_FSYNCDIR:
			printf(" flags=%#x", in.body.fsyncdir.fsync_flags);
			break;
		case FUSE_INTERRUPT:
			printf(" unique=%" PRIu64, in.body.interrupt.unique);
			break;
		case FUSE_LINK:
			printf(" oldnodeid=%" PRIu64, in.body.link.oldnodeid);
			break;
		case FUSE_LISTXATTR:
			printf(" size=%" PRIu32, in.body.listxattr.size);
			break;
		case FUSE_LOOKUP:
			printf(" %s", in.body.lookup);
			break;
		case FUSE_MKDIR:
			name = (const char*)in.body.bytes +
				sizeof(fuse_mkdir_in);
			printf(" name=%s mode=%#o umask=%#o", name,
				in.body.mkdir.mode, in.body.mkdir.umask);
			break;
		case FUSE_MKNOD:
			if (m_kernel_minor_version >= 12)
				name = (const char*)in.body.bytes +
					sizeof(fuse_mknod_in);
			else
				name = (const char*)in.body.bytes +
					FUSE_COMPAT_MKNOD_IN_SIZE;
			printf(" mode=%#o rdev=%x umask=%#o name=%s",
				in.body.mknod.mode, in.body.mknod.rdev,
				in.body.mknod.umask, name);
			break;
		case FUSE_OPEN:
			printf(" flags=%#x", in.body.open.flags);
			break;
		case FUSE_OPENDIR:
			printf(" flags=%#x", in.body.opendir.flags);
			break;
		case FUSE_READ:
			printf(" offset=%" PRIu64 " size=%u",
				in.body.read.offset,
				in.body.read.size);
			if (verbosity > 1)
				printf(" flags=%#x", in.body.read.flags);
			break;
		case FUSE_READDIR:
			printf(" fh=%#" PRIx64 " offset=%" PRIu64 " size=%u",
				in.body.readdir.fh, in.body.readdir.offset,
				in.body.readdir.size);
			break;
		case FUSE_RELEASE:
			printf(" fh=%#" PRIx64 " flags=%#x lock_owner=%" PRIu64,
				in.body.release.fh,
				in.body.release.flags,
				in.body.release.lock_owner);
			break;
		case FUSE_SETATTR:
			if (verbosity <= 1) {
				printf(" valid=%#x", in.body.setattr.valid);
				break;
			}
			if (in.body.setattr.valid & FATTR_MODE)
				printf(" mode=%#o", in.body.setattr.mode);
			if (in.body.setattr.valid & FATTR_UID)
				printf(" uid=%u", in.body.setattr.uid);
			if (in.body.setattr.valid & FATTR_GID)
				printf(" gid=%u", in.body.setattr.gid);
			if (in.body.setattr.valid & FATTR_SIZE)
				printf(" size=%" PRIu64, in.body.setattr.size);
			if (in.body.setattr.valid & FATTR_ATIME)
				printf(" atime=%" PRIu64 ".%u",
					in.body.setattr.atime,
					in.body.setattr.atimensec);
			if (in.body.setattr.valid & FATTR_MTIME)
				printf(" mtime=%" PRIu64 ".%u",
					in.body.setattr.mtime,
					in.body.setattr.mtimensec);
			if (in.body.setattr.valid & FATTR_FH)
				printf(" fh=%" PRIu64 "", in.body.setattr.fh);
			break;
		case FUSE_SETLK:
			printf(" fh=%#" PRIx64 " owner=%" PRIu64
				" type=%u pid=%u",
				in.body.setlk.fh, in.body.setlk.owner,
				in.body.setlk.lk.type,
				in.body.setlk.lk.pid);
			if (verbosity >= 2) {
				printf(" range=[%" PRIu64 "-%" PRIu64 "]",
					in.body.setlk.lk.start,
					in.body.setlk.lk.end);
			}
			break;
		case FUSE_SETXATTR:
			/* 
			 * In theory neither the xattr name and value need be
			 * ASCII, but in this test suite they always are.
			 */
			name = (const char*)in.body.bytes +
				sizeof(fuse_setxattr_in);
			value = name + strlen(name) + 1;
			printf(" %s=%s", name, value);
			break;
		case FUSE_WRITE:
			printf(" fh=%#" PRIx64 " offset=%" PRIu64
				" size=%u write_flags=%u",
				in.body.write.fh,
				in.body.write.offset, in.body.write.size,
				in.body.write.write_flags);
			if (verbosity > 1)
				printf(" flags=%#x", in.body.write.flags);
			break;
		default:
			break;
	}
	printf("\n");
}

/* 
 * Debug a FUSE response.
 *
 * This is mostly useful for asynchronous notifications, which don't correspond
 * to any request
 */
void MockFS::debug_response(const mockfs_buf_out &out) {
	const char *name;

	if (verbosity == 0)
		return;

	switch (out.header.error) {
		case FUSE_NOTIFY_INVAL_ENTRY:
			name = (const char*)out.body.bytes +
				sizeof(fuse_notify_inval_entry_out);
			printf("<- INVAL_ENTRY parent=%" PRIu64 " %s\n",
				out.body.inval_entry.parent, name);
			break;
		case FUSE_NOTIFY_INVAL_INODE:
			printf("<- INVAL_INODE ino=%" PRIu64 " off=%" PRIi64
				" len=%" PRIi64 "\n",
				out.body.inval_inode.ino,
				out.body.inval_inode.off,
				out.body.inval_inode.len);
			break;
		case FUSE_NOTIFY_STORE:
			printf("<- STORE ino=%" PRIu64 " off=%" PRIu64
				" size=%" PRIu32 "\n",
				out.body.store.nodeid,
				out.body.store.offset,
				out.body.store.size);
			break;
		default:
			break;
	}
}

MockFS::MockFS(int max_readahead, bool allow_other, bool default_permissions,
	bool push_symlinks_in, bool ro, enum poll_method pm, uint32_t flags,
	uint32_t kernel_minor_version, uint32_t max_write, bool async,
	bool noclusterr, unsigned time_gran, bool nointr)
{
	struct sigaction sa;
	struct iovec *iov = NULL;
	int iovlen = 0;
	char fdstr[15];
	const bool trueval = true;

	m_daemon_id = NULL;
	m_kernel_minor_version = kernel_minor_version;
	m_maxreadahead = max_readahead;
	m_maxwrite = max_write;
	m_nready = -1;
	m_pm = pm;
	m_time_gran = time_gran;
	m_quit = false;
	if (m_pm == KQ)
		m_kq = kqueue();
	else
		m_kq = -1;

	/*
	 * Kyua sets pwd to a testcase-unique tempdir; no need to use
	 * mkdtemp
	 */
	/*
	 * googletest doesn't allow ASSERT_ in constructors, so we must throw
	 * instead.
	 */
	if (mkdir("mountpoint" , 0755) && errno != EEXIST)
		throw(std::system_error(errno, std::system_category(),
			"Couldn't make mountpoint directory"));

	switch (m_pm) {
	case BLOCKING:
		m_fuse_fd = open("/dev/fuse", O_CLOEXEC | O_RDWR);
		break;
	default:
		m_fuse_fd = open("/dev/fuse", O_CLOEXEC | O_RDWR | O_NONBLOCK);
		break;
	}
	if (m_fuse_fd < 0)
		throw(std::system_error(errno, std::system_category(),
			"Couldn't open /dev/fuse"));

	m_pid = getpid();
	m_child_pid = -1;

	build_iovec(&iov, &iovlen, "fstype", __DECONST(void *, "fusefs"), -1);
	build_iovec(&iov, &iovlen, "fspath",
		    __DECONST(void *, "mountpoint"), -1);
	build_iovec(&iov, &iovlen, "from", __DECONST(void *, "/dev/fuse"), -1);
	sprintf(fdstr, "%d", m_fuse_fd);
	build_iovec(&iov, &iovlen, "fd", fdstr, -1);
	if (allow_other) {
		build_iovec(&iov, &iovlen, "allow_other",
			__DECONST(void*, &trueval), sizeof(bool));
	}
	if (default_permissions) {
		build_iovec(&iov, &iovlen, "default_permissions",
			__DECONST(void*, &trueval), sizeof(bool));
	}
	if (push_symlinks_in) {
		build_iovec(&iov, &iovlen, "push_symlinks_in",
			__DECONST(void*, &trueval), sizeof(bool));
	}
	if (ro) {
		build_iovec(&iov, &iovlen, "ro",
			__DECONST(void*, &trueval), sizeof(bool));
	}
	if (async) {
		build_iovec(&iov, &iovlen, "async", __DECONST(void*, &trueval),
			sizeof(bool));
	}
	if (noclusterr) {
		build_iovec(&iov, &iovlen, "noclusterr",
			__DECONST(void*, &trueval), sizeof(bool));
	}
	if (nointr) {
		build_iovec(&iov, &iovlen, "nointr",
			__DECONST(void*, &trueval), sizeof(bool));
	} else {
		build_iovec(&iov, &iovlen, "intr",
			__DECONST(void*, &trueval), sizeof(bool));
	}
	if (nmount(iov, iovlen, 0))
		throw(std::system_error(errno, std::system_category(),
			"Couldn't mount filesystem"));

	// Setup default handler
	ON_CALL(*this, process(_, _))
		.WillByDefault(Invoke(this, &MockFS::process_default));

	init(flags);
	bzero(&sa, sizeof(sa));
	sa.sa_handler = sigint_handler;
	sa.sa_flags = 0;	/* Don't set SA_RESTART! */
	if (0 != sigaction(SIGUSR1, &sa, NULL))
		throw(std::system_error(errno, std::system_category(),
			"Couldn't handle SIGUSR1"));
	if (pthread_create(&m_daemon_id, NULL, service, (void*)this))
		throw(std::system_error(errno, std::system_category(),
			"Couldn't Couldn't start fuse thread"));
}

MockFS::~MockFS() {
	kill_daemon();
	if (m_daemon_id != NULL) {
		pthread_join(m_daemon_id, NULL);
		m_daemon_id = NULL;
	}
	::unmount("mountpoint", MNT_FORCE);
	rmdir("mountpoint");
	if (m_kq >= 0)
		close(m_kq);
}

void MockFS::audit_request(const mockfs_buf_in &in, ssize_t buflen) {
	uint32_t inlen = in.header.len;
	size_t fih = sizeof(in.header);
	switch (in.header.opcode) {
	case FUSE_LOOKUP:
	case FUSE_RMDIR:
	case FUSE_SYMLINK:
	case FUSE_UNLINK:
		EXPECT_GT(inlen, fih) << "Missing request filename";
		// No redundant information for checking buflen
		break;
	case FUSE_FORGET:
		EXPECT_EQ(inlen, fih + sizeof(in.body.forget));
		EXPECT_EQ((size_t)buflen, inlen);
		break;
	case FUSE_GETATTR:
		EXPECT_EQ(inlen, fih + sizeof(in.body.getattr));
		EXPECT_EQ((size_t)buflen, inlen);
		break;
	case FUSE_SETATTR:
		EXPECT_EQ(inlen, fih + sizeof(in.body.setattr));
		EXPECT_EQ((size_t)buflen, inlen);
		break;
	case FUSE_READLINK:
		EXPECT_EQ(inlen, fih) << "Unexpected request body";
		EXPECT_EQ((size_t)buflen, inlen);
		break;
	case FUSE_MKNOD:
		{
			size_t s;
			if (m_kernel_minor_version >= 12)
				s = sizeof(in.body.mknod);
			else
				s = FUSE_COMPAT_MKNOD_IN_SIZE;
			EXPECT_GE(inlen, fih + s) << "Missing request body";
			EXPECT_GT(inlen, fih + s) << "Missing request filename";
			// No redundant information for checking buflen
			break;
		}
	case FUSE_MKDIR:
		EXPECT_GE(inlen, fih + sizeof(in.body.mkdir)) <<
			"Missing request body";
		EXPECT_GT(inlen, fih + sizeof(in.body.mkdir)) <<
			"Missing request filename";
		// No redundant information for checking buflen
		break;
	case FUSE_RENAME:
		EXPECT_GE(inlen, fih + sizeof(in.body.rename)) <<
			"Missing request body";
		EXPECT_GT(inlen, fih + sizeof(in.body.rename)) <<
			"Missing request filename";
		// No redundant information for checking buflen
		break;
	case FUSE_LINK:
		EXPECT_GE(inlen, fih + sizeof(in.body.link)) <<
			"Missing request body";
		EXPECT_GT(inlen, fih + sizeof(in.body.link)) <<
			"Missing request filename";
		// No redundant information for checking buflen
		break;
	case FUSE_OPEN:
		EXPECT_EQ(inlen, fih + sizeof(in.body.open));
		EXPECT_EQ((size_t)buflen, inlen);
		break;
	case FUSE_READ:
		EXPECT_EQ(inlen, fih + sizeof(in.body.read));
		EXPECT_EQ((size_t)buflen, inlen);
		break;
	case FUSE_WRITE:
		{
			size_t s;

			if (m_kernel_minor_version >= 9)
				s = sizeof(in.body.write);
			else
				s = FUSE_COMPAT_WRITE_IN_SIZE;
			// I suppose a 0-byte write should be allowed
			EXPECT_GE(inlen, fih + s) << "Missing request body";
			EXPECT_EQ((size_t)buflen, fih + s + in.body.write.size);
			break;
		}
	case FUSE_DESTROY:
	case FUSE_STATFS:
		EXPECT_EQ(inlen, fih);
		EXPECT_EQ((size_t)buflen, inlen);
		break;
	case FUSE_RELEASE:
		EXPECT_EQ(inlen, fih + sizeof(in.body.release));
		EXPECT_EQ((size_t)buflen, inlen);
		break;
	case FUSE_FSYNC:
	case FUSE_FSYNCDIR:
		EXPECT_EQ(inlen, fih + sizeof(in.body.fsync));
		EXPECT_EQ((size_t)buflen, inlen);
		break;
	case FUSE_SETXATTR:
		EXPECT_GE(inlen, fih + sizeof(in.body.setxattr)) <<
			"Missing request body";
		EXPECT_GT(inlen, fih + sizeof(in.body.setxattr)) <<
			"Missing request attribute name";
		// No redundant information for checking buflen
		break;
	case FUSE_GETXATTR:
		EXPECT_GE(inlen, fih + sizeof(in.body.getxattr)) <<
			"Missing request body";
		EXPECT_GT(inlen, fih + sizeof(in.body.getxattr)) <<
			"Missing request attribute name";
		// No redundant information for checking buflen
		break;
	case FUSE_LISTXATTR:
		EXPECT_EQ(inlen, fih + sizeof(in.body.listxattr));
		EXPECT_EQ((size_t)buflen, inlen);
		break;
	case FUSE_REMOVEXATTR:
		EXPECT_GT(inlen, fih) << "Missing request attribute name";
		// No redundant information for checking buflen
		break;
	case FUSE_FLUSH:
		EXPECT_EQ(inlen, fih + sizeof(in.body.flush));
		EXPECT_EQ((size_t)buflen, inlen);
		break;
	case FUSE_INIT:
		EXPECT_EQ(inlen, fih + sizeof(in.body.init));
		EXPECT_EQ((size_t)buflen, inlen);
		break;
	case FUSE_OPENDIR:
		EXPECT_EQ(inlen, fih + sizeof(in.body.opendir));
		EXPECT_EQ((size_t)buflen, inlen);
		break;
	case FUSE_READDIR:
		EXPECT_EQ(inlen, fih + sizeof(in.body.readdir));
		EXPECT_EQ((size_t)buflen, inlen);
		break;
	case FUSE_RELEASEDIR:
		EXPECT_EQ(inlen, fih + sizeof(in.body.releasedir));
		EXPECT_EQ((size_t)buflen, inlen);
		break;
	case FUSE_GETLK:
		EXPECT_EQ(inlen, fih + sizeof(in.body.getlk));
		EXPECT_EQ((size_t)buflen, inlen);
		break;
	case FUSE_SETLK:
	case FUSE_SETLKW:
		EXPECT_EQ(inlen, fih + sizeof(in.body.setlk));
		EXPECT_EQ((size_t)buflen, inlen);
		break;
	case FUSE_ACCESS:
		EXPECT_EQ(inlen, fih + sizeof(in.body.access));
		EXPECT_EQ((size_t)buflen, inlen);
		break;
	case FUSE_CREATE:
		EXPECT_GE(inlen, fih + sizeof(in.body.create)) <<
			"Missing request body";
		EXPECT_GT(inlen, fih + sizeof(in.body.create)) <<
			"Missing request filename";
		// No redundant information for checking buflen
		break;
	case FUSE_INTERRUPT:
		EXPECT_EQ(inlen, fih + sizeof(in.body.interrupt));
		EXPECT_EQ((size_t)buflen, inlen);
		break;
	case FUSE_BMAP:
		EXPECT_EQ(inlen, fih + sizeof(in.body.bmap));
		EXPECT_EQ((size_t)buflen, inlen);
		break;
	case FUSE_NOTIFY_REPLY:
	case FUSE_BATCH_FORGET:
	case FUSE_FALLOCATE:
	case FUSE_IOCTL:
	case FUSE_POLL:
	case FUSE_READDIRPLUS:
		FAIL() << "Unsupported opcode?";
	default:
		FAIL() << "Unknown opcode " << in.header.opcode;
	}
}

void MockFS::init(uint32_t flags) {
	ssize_t buflen;

	std::unique_ptr<mockfs_buf_in> in(new mockfs_buf_in);
	std::unique_ptr<mockfs_buf_out> out(new mockfs_buf_out);

	read_request(*in, buflen);
	audit_request(*in, buflen);
	ASSERT_EQ(FUSE_INIT, in->header.opcode);

	out->header.unique = in->header.unique;
	out->header.error = 0;
	out->body.init.major = FUSE_KERNEL_VERSION;
	out->body.init.minor = m_kernel_minor_version;;
	out->body.init.flags = in->body.init.flags & flags;
	out->body.init.max_write = m_maxwrite;
	out->body.init.max_readahead = m_maxreadahead;

	if (m_kernel_minor_version < 23) {
		SET_OUT_HEADER_LEN(*out, init_7_22);
	} else {
		out->body.init.time_gran = m_time_gran;
		SET_OUT_HEADER_LEN(*out, init);
	}

	write(m_fuse_fd, out.get(), out->header.len);
}

void MockFS::kill_daemon() {
	m_quit = true;
	if (m_daemon_id != NULL)
		pthread_kill(m_daemon_id, SIGUSR1);
	// Closing the /dev/fuse file descriptor first allows unmount to
	// succeed even if the daemon doesn't correctly respond to commands
	// during the unmount sequence.
	close(m_fuse_fd);
	m_fuse_fd = -1;
}

void MockFS::loop() {
	std::vector<std::unique_ptr<mockfs_buf_out>> out;

	std::unique_ptr<mockfs_buf_in> in(new mockfs_buf_in);
	ASSERT_TRUE(in != NULL);
	while (!m_quit) {
		ssize_t buflen;

		bzero(in.get(), sizeof(*in));
		read_request(*in, buflen);
		if (m_quit)
			break;
		if (verbosity > 0)
			debug_request(*in, buflen);
		audit_request(*in, buflen);
		if (pid_ok((pid_t)in->header.pid)) {
			process(*in, out);
		} else {
			/* 
			 * Reject any requests from unknown processes.  Because
			 * we actually do mount a filesystem, plenty of
			 * unrelated system daemons may try to access it.
			 */
			if (verbosity > 1)
				printf("\tREJECTED (wrong pid %d)\n",
					in->header.pid);
			process_default(*in, out);
		}
		for (auto &it: out)
			write_response(*it);
		out.clear();
	}
}

int MockFS::notify_inval_entry(ino_t parent, const char *name, size_t namelen)
{
	std::unique_ptr<mockfs_buf_out> out(new mockfs_buf_out);

	out->header.unique = 0;	/* 0 means asynchronous notification */
	out->header.error = FUSE_NOTIFY_INVAL_ENTRY;
	out->body.inval_entry.parent = parent;
	out->body.inval_entry.namelen = namelen;
	strlcpy((char*)&out->body.bytes + sizeof(out->body.inval_entry),
		name, sizeof(out->body.bytes) - sizeof(out->body.inval_entry));
	out->header.len = sizeof(out->header) + sizeof(out->body.inval_entry) +
		namelen;
	debug_response(*out);
	write_response(*out);
	return 0;
}

int MockFS::notify_inval_inode(ino_t ino, off_t off, ssize_t len)
{
	std::unique_ptr<mockfs_buf_out> out(new mockfs_buf_out);

	out->header.unique = 0;	/* 0 means asynchronous notification */
	out->header.error = FUSE_NOTIFY_INVAL_INODE;
	out->body.inval_inode.ino = ino;
	out->body.inval_inode.off = off;
	out->body.inval_inode.len = len;
	out->header.len = sizeof(out->header) + sizeof(out->body.inval_inode);
	debug_response(*out);
	write_response(*out);
	return 0;
}

int MockFS::notify_store(ino_t ino, off_t off, const void* data, ssize_t size)
{
	std::unique_ptr<mockfs_buf_out> out(new mockfs_buf_out);

	out->header.unique = 0;	/* 0 means asynchronous notification */
	out->header.error = FUSE_NOTIFY_STORE;
	out->body.store.nodeid = ino;
	out->body.store.offset = off;
	out->body.store.size = size;
	bcopy(data, (char*)&out->body.bytes + sizeof(out->body.store), size);
	out->header.len = sizeof(out->header) + sizeof(out->body.store) + size;
	debug_response(*out);
	write_response(*out);
	return 0;
}

bool MockFS::pid_ok(pid_t pid) {
	if (pid == m_pid) {
		return (true);
	} else if (pid == m_child_pid) {
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

void MockFS::process_default(const mockfs_buf_in& in,
		std::vector<std::unique_ptr<mockfs_buf_out>> &out)
{
	std::unique_ptr<mockfs_buf_out> out0(new mockfs_buf_out);
	out0->header.unique = in.header.unique;
	out0->header.error = -EOPNOTSUPP;
	out0->header.len = sizeof(out0->header);
	out.push_back(std::move(out0));
}

void MockFS::read_request(mockfs_buf_in &in, ssize_t &res) {
	int nready = 0;
	fd_set readfds;
	pollfd fds[1];
	struct kevent changes[1];
	struct kevent events[1];
	struct timespec timeout_ts;
	struct timeval timeout_tv;
	const int timeout_ms = 999;
	int timeout_int, nfds;

	switch (m_pm) {
	case BLOCKING:
		break;
	case KQ:
		timeout_ts.tv_sec = 0;
		timeout_ts.tv_nsec = timeout_ms * 1'000'000;
		while (nready == 0) {
			EV_SET(&changes[0], m_fuse_fd, EVFILT_READ, EV_ADD, 0,
				0, 0);
			nready = kevent(m_kq, &changes[0], 1, &events[0], 1,
				&timeout_ts);
			if (m_quit)
				return;
		}
		ASSERT_LE(0, nready) << strerror(errno);
		ASSERT_EQ(events[0].ident, (uintptr_t)m_fuse_fd);
		if (events[0].flags & EV_ERROR)
			FAIL() << strerror(events[0].data);
		else if (events[0].flags & EV_EOF)
			FAIL() << strerror(events[0].fflags);
		m_nready = events[0].data;
		break;
	case POLL:
		timeout_int = timeout_ms;
		fds[0].fd = m_fuse_fd;
		fds[0].events = POLLIN;
		while (nready == 0) {
			nready = poll(fds, 1, timeout_int);
			if (m_quit)
				return;
		}
		ASSERT_LE(0, nready) << strerror(errno);
		ASSERT_TRUE(fds[0].revents & POLLIN);
		break;
	case SELECT:
		timeout_tv.tv_sec = 0;
		timeout_tv.tv_usec = timeout_ms * 1'000;
		nfds = m_fuse_fd + 1;
		while (nready == 0) {
			FD_ZERO(&readfds);
			FD_SET(m_fuse_fd, &readfds);
			nready = select(nfds, &readfds, NULL, NULL,
				&timeout_tv);
			if (m_quit)
				return;
		}
		ASSERT_LE(0, nready) << strerror(errno);
		ASSERT_TRUE(FD_ISSET(m_fuse_fd, &readfds));
		break;
	default:
		FAIL() << "not yet implemented";
	}
	res = read(m_fuse_fd, &in, sizeof(in));

	if (res < 0 && !m_quit) {
		m_quit = true;
		FAIL() << "read: " << strerror(errno);
	}
	ASSERT_TRUE(res >= static_cast<ssize_t>(sizeof(in.header)) || m_quit);
	/*
	 * Inconsistently, fuse_in_header.len is the size of the entire
	 * request,including header, even though fuse_out_header.len excludes
	 * the size of the header.
	 */
	ASSERT_TRUE(res == static_cast<ssize_t>(in.header.len) || m_quit);
}

void MockFS::write_response(const mockfs_buf_out &out) {
	fd_set writefds;
	pollfd fds[1];
	int nready, nfds;
	ssize_t r;

	switch (m_pm) {
	case BLOCKING:
	case KQ:	/* EVFILT_WRITE is not supported */
		break;
	case POLL:
		fds[0].fd = m_fuse_fd;
		fds[0].events = POLLOUT;
		nready = poll(fds, 1, INFTIM);
		ASSERT_LE(0, nready) << strerror(errno);
		ASSERT_EQ(1, nready) << "NULL timeout expired?";
		ASSERT_TRUE(fds[0].revents & POLLOUT);
		break;
	case SELECT:
		FD_ZERO(&writefds);
		FD_SET(m_fuse_fd, &writefds);
		nfds = m_fuse_fd + 1;
		nready = select(nfds, NULL, &writefds, NULL, NULL);
		ASSERT_LE(0, nready) << strerror(errno);
		ASSERT_EQ(1, nready) << "NULL timeout expired?";
		ASSERT_TRUE(FD_ISSET(m_fuse_fd, &writefds));
		break;
	default:
		FAIL() << "not yet implemented";
	}
	r = write(m_fuse_fd, &out, out.header.len);
	ASSERT_TRUE(r > 0 || errno == EAGAIN) << strerror(errno);
}

void* MockFS::service(void *pthr_data) {
	MockFS *mock_fs = (MockFS*)pthr_data;

	mock_fs->loop();

	return (NULL);
}

void MockFS::unmount() {
	::unmount("mountpoint", 0);
}
