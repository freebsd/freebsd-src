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
 *
 * $FreeBSD$
 */

/*
 * This file tests different polling methods for the /dev/fuse device
 */

extern "C" {
#include <fcntl.h>
#include <semaphore.h>
#include <unistd.h>
}

#include "mockfs.hh"
#include "utils.hh"

using namespace testing;

const char FULLPATH[] = "mountpoint/some_file.txt";
const char RELPATH[] = "some_file.txt";
const uint64_t ino = 42;
const mode_t access_mode = R_OK;

/*
 * Translate a poll method's string representation to the enum value.
 * Using strings with ::testing::Values gives better output with
 * --gtest_list_tests
 */
enum poll_method poll_method_from_string(const char *s)
{
	if (0 == strcmp("BLOCKING", s))
		return BLOCKING;
	else if (0 == strcmp("KQ", s))
		return KQ;
	else if (0 == strcmp("POLL", s))
		return POLL;
	else
		return SELECT;
}

class DevFusePoll: public FuseTest, public WithParamInterface<const char *> {
	virtual void SetUp() {
		m_pm = poll_method_from_string(GetParam());
		FuseTest::SetUp();
	}
};

class Kqueue: public FuseTest {
	virtual void SetUp() {
		m_pm = KQ;
		FuseTest::SetUp();
	}
};

TEST_P(DevFusePoll, access)
{
	expect_access(FUSE_ROOT_ID, X_OK, 0);
	expect_lookup(RELPATH, ino, S_IFREG | 0644, 0, 1);
	expect_access(ino, access_mode, 0);

	ASSERT_EQ(0, access(FULLPATH, access_mode)) << strerror(errno);
}

/* Ensure that we wake up pollers during unmount */
TEST_P(DevFusePoll, destroy)
{
	expect_destroy(0);

	m_mock->unmount();
}

INSTANTIATE_TEST_SUITE_P(PM, DevFusePoll,
		::testing::Values("BLOCKING", "KQ", "POLL", "SELECT"));

static void* statter(void* arg) {
	const char *name;
	struct stat sb;

	name = (const char*)arg;
	return ((void*)(intptr_t)stat(name, &sb));
}

/*
 * A kevent's data field should contain the number of operations available to
 * be immediately read.
 */
TEST_F(Kqueue, data)
{
	pthread_t th0, th1, th2;
	sem_t sem0, sem1;
	int nready0, nready1, nready2;
	uint64_t foo_ino = 42;
	uint64_t bar_ino = 43;
	uint64_t baz_ino = 44;
	Sequence seq;
	void *th_ret;

	ASSERT_EQ(0, sem_init(&sem0, 0, 0)) << strerror(errno);
	ASSERT_EQ(0, sem_init(&sem1, 0, 0)) << strerror(errno);

	EXPECT_LOOKUP(FUSE_ROOT_ID, "foo")
	.WillOnce(Invoke(ReturnImmediate([=](auto in __unused, auto& out) {
		SET_OUT_HEADER_LEN(out, entry);
		out.body.entry.entry_valid = UINT64_MAX;
		out.body.entry.attr.mode = S_IFREG | 0644;
		out.body.entry.nodeid = foo_ino;
	})));
	EXPECT_LOOKUP(FUSE_ROOT_ID, "bar")
	.WillOnce(Invoke(ReturnImmediate([=](auto in __unused, auto& out) {
		SET_OUT_HEADER_LEN(out, entry);
		out.body.entry.entry_valid = UINT64_MAX;
		out.body.entry.attr.mode = S_IFREG | 0644;
		out.body.entry.nodeid = bar_ino;
	})));
	EXPECT_LOOKUP(FUSE_ROOT_ID, "baz")
	.WillOnce(Invoke(ReturnImmediate([=](auto in __unused, auto& out) {
		SET_OUT_HEADER_LEN(out, entry);
		out.body.entry.entry_valid = UINT64_MAX;
		out.body.entry.attr.mode = S_IFREG | 0644;
		out.body.entry.nodeid = baz_ino;
	})));

	EXPECT_CALL(*m_mock, process(
		ResultOf([=](auto in) {
			return (in.header.opcode == FUSE_GETATTR &&
				in.header.nodeid == foo_ino);
		}, Eq(true)),
		_)
	)
	.WillOnce(Invoke(ReturnImmediate([&](auto in, auto& out) {
		nready0 = m_mock->m_nready;

		sem_post(&sem0);
		// Block the daemon so we can accumulate a few more ops
		sem_wait(&sem1);

		out.header.unique = in.header.unique;
		out.header.error = -EIO;
		out.header.len = sizeof(out.header);
	})));

	EXPECT_CALL(*m_mock, process(
		ResultOf([=](auto in) {
			return (in.header.opcode == FUSE_GETATTR &&
				(in.header.nodeid == bar_ino ||
				 in.header.nodeid == baz_ino));
		}, Eq(true)),
		_)
	).InSequence(seq)
	.WillOnce(Invoke(ReturnImmediate([&](auto in, auto& out) {
		nready1 = m_mock->m_nready;
		out.header.unique = in.header.unique;
		out.header.error = -EIO;
		out.header.len = sizeof(out.header);
	})));
	EXPECT_CALL(*m_mock, process(
		ResultOf([=](auto in) {
			return (in.header.opcode == FUSE_GETATTR &&
				(in.header.nodeid == bar_ino ||
				 in.header.nodeid == baz_ino));
		}, Eq(true)),
		_)
	).InSequence(seq)
	.WillOnce(Invoke(ReturnImmediate([&](auto in, auto& out) {
		nready2 = m_mock->m_nready;
		out.header.unique = in.header.unique;
		out.header.error = -EIO;
		out.header.len = sizeof(out.header);
	})));

	/* 
	 * Create cached lookup entries for these files.  It seems that only
	 * one thread at a time can be in VOP_LOOKUP for a given directory
	 */
	access("mountpoint/foo", F_OK);
	access("mountpoint/bar", F_OK);
	access("mountpoint/baz", F_OK);
	ASSERT_EQ(0, pthread_create(&th0, NULL, statter,
		__DECONST(void*, "mountpoint/foo"))) << strerror(errno);
	EXPECT_EQ(0, sem_wait(&sem0)) << strerror(errno);
	ASSERT_EQ(0, pthread_create(&th1, NULL, statter,
		__DECONST(void*, "mountpoint/bar"))) << strerror(errno);
	ASSERT_EQ(0, pthread_create(&th2, NULL, statter,
		__DECONST(void*, "mountpoint/baz"))) << strerror(errno);

	nap();		// Allow th1 and th2 to send their ops to the daemon
	EXPECT_EQ(0, sem_post(&sem1)) << strerror(errno);

	pthread_join(th0, &th_ret);
	ASSERT_EQ(-1, (intptr_t)th_ret);
	pthread_join(th1, &th_ret);
	ASSERT_EQ(-1, (intptr_t)th_ret);
	pthread_join(th2, &th_ret);
	ASSERT_EQ(-1, (intptr_t)th_ret);

	EXPECT_EQ(1, nready0);
	EXPECT_EQ(2, nready1);
	EXPECT_EQ(1, nready2);

	sem_destroy(&sem0);
	sem_destroy(&sem1);
}
