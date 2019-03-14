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

#include <sys/param.h>
#include <sys/module.h>
#include <sys/sysctl.h>

#include <gtest/gtest.h>
#include <unistd.h>

#include "mockfs.hh"
#include "utils.hh"

using namespace testing;

class FuseEnv: public Environment {
	virtual void SetUp() {
		const char *mod_name = "fuse";
		const char *devnode = "/dev/fuse";
		const char *usermount_node = "vfs.usermount";
		int usermount_val = 0;
		size_t usermount_size = sizeof(usermount_val);
		if (modfind(mod_name) == -1) {
			FAIL() << "Module " << mod_name <<
				" could not be resolved";
		}
		if (eaccess(devnode, R_OK | W_OK)) {
			if (errno == ENOENT) {
				FAIL() << devnode << " does not exist";
			} else if (errno == EACCES) {
				FAIL() << devnode <<
				    " is not accessible by the current user";
			} else {
				FAIL() << strerror(errno);
			}
		}
		sysctlbyname(usermount_node, &usermount_val, &usermount_size,
			     NULL, 0);
		if (geteuid() != 0 && !usermount_val)
			FAIL() << "current user is not allowed to mount";
	}
};

void FuseTest::SetUp() {
	const char *node = "vfs.maxbcachebuf";
	int val = 0;
	size_t size = sizeof(val);

	ASSERT_EQ(0, sysctlbyname(node, &val, &size, NULL, 0))
		<< strerror(errno);
	m_maxbcachebuf = val;

	try {
		m_mock = new MockFS(m_maxreadahead, m_init_flags);
	} catch (std::system_error err) {
		FAIL() << err.what();
	}
}

void FuseTest::expect_getattr(uint64_t ino, uint64_t size)
{
	/* Until the attr cache is working, we may send an additional GETATTR */
	EXPECT_CALL(*m_mock, process(
		ResultOf([=](auto in) {
			return (in->header.opcode == FUSE_GETATTR &&
				in->header.nodeid == ino);
		}, Eq(true)),
		_)
	).WillRepeatedly(Invoke([=](auto in, auto out) {
		out->header.unique = in->header.unique;
		SET_OUT_HEADER_LEN(out, attr);
		out->body.attr.attr.ino = ino;	// Must match nodeid
		out->body.attr.attr.mode = S_IFREG | 0644;
		out->body.attr.attr.size = size;
		out->body.attr.attr_valid = UINT64_MAX;
	}));
}

void FuseTest::expect_lookup(const char *relpath, uint64_t ino, mode_t mode,
	int times)
{
	EXPECT_LOOKUP(1, relpath)
	.Times(times)
	.WillRepeatedly(Invoke([=](auto in, auto out) {
		out->header.unique = in->header.unique;
		SET_OUT_HEADER_LEN(out, entry);
		out->body.entry.attr.mode = mode;
		out->body.entry.nodeid = ino;
		out->body.entry.attr.nlink = 1;
		out->body.entry.attr_valid = UINT64_MAX;
	}));
}

void FuseTest::expect_open(uint64_t ino, uint32_t flags, int times)
{
	EXPECT_CALL(*m_mock, process(
		ResultOf([=](auto in) {
			return (in->header.opcode == FUSE_OPEN &&
				in->header.nodeid == ino);
		}, Eq(true)),
		_)
	).Times(times)
	.WillRepeatedly(Invoke([=](auto in, auto out) {
		out->header.unique = in->header.unique;
		out->header.len = sizeof(out->header);
		SET_OUT_HEADER_LEN(out, open);
		out->body.open.fh = FH;
		out->body.open.open_flags = flags;
	}));
}

void FuseTest::expect_opendir(uint64_t ino)
{
	EXPECT_CALL(*m_mock, process(
		ResultOf([](auto in) {
			return (in->header.opcode == FUSE_STATFS);
		}, Eq(true)),
		_)
	).WillRepeatedly(Invoke([=](auto in, auto out) {
		out->header.unique = in->header.unique;
		SET_OUT_HEADER_LEN(out, statfs);
	}));

	EXPECT_CALL(*m_mock, process(
		ResultOf([=](auto in) {
			return (in->header.opcode == FUSE_OPENDIR &&
				in->header.nodeid == ino);
		}, Eq(true)),
		_)
	).WillOnce(Invoke([=](auto in, auto out) {
		out->header.unique = in->header.unique;
		out->header.len = sizeof(out->header);
		SET_OUT_HEADER_LEN(out, open);
		out->body.open.fh = FH;
	}));
}

void FuseTest::expect_read(uint64_t ino, uint64_t offset, uint64_t isize,
	uint64_t osize, const void *contents)
{
	EXPECT_CALL(*m_mock, process(
		ResultOf([=](auto in) {
			return (in->header.opcode == FUSE_READ &&
				in->header.nodeid == ino &&
				in->body.read.fh == FH &&
				in->body.read.offset == offset &&
				in->body.read.size == isize);
		}, Eq(true)),
		_)
	).WillOnce(Invoke([=](auto in, auto out) {
		out->header.unique = in->header.unique;
		out->header.len = sizeof(struct fuse_out_header) + osize;
		memmove(out->body.bytes, contents, osize);
	})).RetiresOnSaturation();
}

void FuseTest::expect_release(uint64_t ino, int times, int error)
{
	EXPECT_CALL(*m_mock, process(
		ResultOf([=](auto in) {
			return (in->header.opcode == FUSE_RELEASE &&
				in->header.nodeid == ino &&
				in->body.release.fh == FH);
		}, Eq(true)),
		_)
	).Times(times)
	.WillRepeatedly(Invoke(ReturnErrno(error)));
}
void FuseTest::expect_write(uint64_t ino, uint64_t offset, uint64_t isize,
	uint64_t osize, uint32_t flags, const void *contents)
{
	EXPECT_CALL(*m_mock, process(
		ResultOf([=](auto in) {
			const char *buf = (const char*)in->body.bytes +
				sizeof(struct fuse_write_in);
			bool pid_ok;

			if (in->body.write.write_flags & FUSE_WRITE_CACHE)
				pid_ok = true;
			else
				pid_ok = (pid_t)in->header.pid == getpid();

			return (in->header.opcode == FUSE_WRITE &&
				in->header.nodeid == ino &&
				in->body.write.fh == FH &&
				in->body.write.offset == offset  &&
				in->body.write.size == isize &&
				pid_ok &&
				in->body.write.write_flags == flags &&
				0 == bcmp(buf, contents, isize));
		}, Eq(true)),
		_)
	).WillOnce(Invoke([=](auto in, auto out) {
		out->header.unique = in->header.unique;
		SET_OUT_HEADER_LEN(out, write);
		out->body.write.size = osize;
	}));
}

static void usage(char* progname) {
	fprintf(stderr, "Usage: %s [-v]\n\t-v increase verbosity\n", progname);
	exit(2);
}

int main(int argc, char **argv) {
	int ch;
	FuseEnv *fuse_env = new FuseEnv;

	InitGoogleTest(&argc, argv);
	AddGlobalTestEnvironment(fuse_env);

	while ((ch = getopt(argc, argv, "v")) != -1) {
		switch (ch) {
			case 'v':
				verbosity++;
				break;
			default:
				usage(argv[0]);
				break;
		}
	}

	return (RUN_ALL_TESTS());
}
