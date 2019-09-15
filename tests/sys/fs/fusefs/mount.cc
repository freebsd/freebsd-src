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
#include <sys/uio.h>

#include "mntopts.h"	// for build_iovec
}

#include "mockfs.hh"
#include "utils.hh"

using namespace testing;

class UpdateOk: public FuseTest, public WithParamInterface<const char*> {};
class UpdateErr: public FuseTest, public WithParamInterface<const char*> {};

int mntflag_from_string(const char *s)
{
	if (0 == strcmp("MNT_RDONLY", s))
		return MNT_RDONLY;
	else if (0 == strcmp("MNT_NOEXEC", s))
		return MNT_NOEXEC;
	else if (0 == strcmp("MNT_NOSUID", s))
		return MNT_NOSUID;
	else if (0 == strcmp("MNT_NOATIME", s))
		return MNT_NOATIME;
	else if (0 == strcmp("MNT_SUIDDIR", s))
		return MNT_SUIDDIR;
	else if (0 == strcmp("MNT_USER", s))
		return MNT_USER;
	else
		return 0;
}

/* Some mount options can be changed by mount -u */
TEST_P(UpdateOk, update)
{
	struct statfs statbuf;
	struct iovec *iov = NULL;
	int iovlen = 0;
	int flag;
	int newflags = MNT_UPDATE | MNT_SYNCHRONOUS;

	flag = mntflag_from_string(GetParam());
	if (flag == MNT_NOSUID && 0 != geteuid())
		GTEST_SKIP() << "Only root may clear MNT_NOSUID";
	if (flag == MNT_SUIDDIR && 0 != geteuid())
		GTEST_SKIP() << "Only root may set MNT_SUIDDIR";

	EXPECT_CALL(*m_mock, process(
		ResultOf([](auto in) {
			return (in.header.opcode == FUSE_STATFS);
		}, Eq(true)),
		_)
	).WillRepeatedly(Invoke(ReturnImmediate([=](auto in __unused, auto& out) {
		/* 
		 * All of the fields except f_flags are don't care, and f_flags is set by
		 * the VFS
		 */
		SET_OUT_HEADER_LEN(out, statfs);
	})));

	ASSERT_EQ(0, statfs("mountpoint", &statbuf)) << strerror(errno);
	newflags = (statbuf.f_flags | MNT_UPDATE) ^ flag;

	build_iovec(&iov, &iovlen, "fstype", (void*)statbuf.f_fstypename, -1);
	build_iovec(&iov, &iovlen, "fspath", (void*)statbuf.f_mntonname, -1);
	build_iovec(&iov, &iovlen, "from", __DECONST(void *, "/dev/fuse"), -1);
	ASSERT_EQ(0, nmount(iov, iovlen, newflags)) << strerror(errno);

	ASSERT_EQ(0, statfs("mountpoint", &statbuf)) << strerror(errno);
	EXPECT_FALSE((newflags ^ statbuf.f_flags) & flag);
}

/* Some mount options cannnot be changed by mount -u */
TEST_P(UpdateErr, update)
{
	struct statfs statbuf;
	struct iovec *iov = NULL;
	int iovlen = 0;
	int flag;
	int newflags = MNT_UPDATE | MNT_SYNCHRONOUS;

	flag = mntflag_from_string(GetParam());
	EXPECT_CALL(*m_mock, process(
		ResultOf([](auto in) {
			return (in.header.opcode == FUSE_STATFS);
		}, Eq(true)),
		_)
	).WillRepeatedly(Invoke(ReturnImmediate([=](auto in __unused, auto& out) {
		/* 
		 * All of the fields except f_flags are don't care, and f_flags is set by
		 * the VFS
		 */
		SET_OUT_HEADER_LEN(out, statfs);
	})));

	ASSERT_EQ(0, statfs("mountpoint", &statbuf)) << strerror(errno);
	newflags = (statbuf.f_flags | MNT_UPDATE) ^ flag;

	build_iovec(&iov, &iovlen, "fstype", (void*)statbuf.f_fstypename, -1);
	build_iovec(&iov, &iovlen, "fspath", (void*)statbuf.f_mntonname, -1);
	build_iovec(&iov, &iovlen, "from", __DECONST(void *, "/dev/fuse"), -1);
	/* 
	 * Don't check nmount's return value, because vfs_domount may "fix" the
	 * options for us.  The important thing is to check the final value of
	 * statbuf.f_flags below.
	 */
	(void)nmount(iov, iovlen, newflags);

	ASSERT_EQ(0, statfs("mountpoint", &statbuf)) << strerror(errno);
	EXPECT_TRUE((newflags ^ statbuf.f_flags) & flag);
}

INSTANTIATE_TEST_CASE_P(Mount, UpdateOk,
		::testing::Values("MNT_RDONLY", "MNT_NOEXEC", "MNT_NOSUID", "MNT_NOATIME",
		"MNT_SUIDDIR")
);

INSTANTIATE_TEST_CASE_P(Mount, UpdateErr,
		::testing::Values( "MNT_USER");
);
