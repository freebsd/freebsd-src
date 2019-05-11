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

/*
 * This file tests different polling methods for the /dev/fuse device
 */

extern "C" {
#include <fcntl.h>
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

TEST_P(DevFusePoll, access)
{
	expect_access(1, X_OK, 0);
	expect_lookup(RELPATH, ino, S_IFREG | 0644, 0, 1);
	expect_access(ino, access_mode, 0);

	ASSERT_EQ(0, access(FULLPATH, access_mode)) << strerror(errno);
}

/* Ensure that we wake up pollers during unmount */
TEST_P(DevFusePoll, destroy)
{
	expect_forget(1, 1);
	expect_destroy(0);

	m_mock->unmount();
}

INSTANTIATE_TEST_CASE_P(PM, DevFusePoll,
		::testing::Values("BLOCKING", "KQ", "POLL", "SELECT"));
