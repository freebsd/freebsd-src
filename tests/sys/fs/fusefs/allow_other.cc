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
 * Tests for the "allow_other" mount option.  They must be in their own
 * file so they can be run as root
 */

extern "C" {
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
}

#include "mockfs.hh"
#include "utils.hh"

using namespace testing;

const static char FULLPATH[] = "mountpoint/some_file.txt";
const static char RELPATH[] = "some_file.txt";

class NoAllowOther: public FuseTest {

public:
/* Unprivileged user id */
int m_uid;

virtual void SetUp() {
	if (geteuid() != 0) {
		GTEST_SKIP() << "This test must be run as root";
	}

	FuseTest::SetUp();
}
};

class AllowOther: public NoAllowOther {

public:
virtual void SetUp() {
	m_allow_other = true;
	NoAllowOther::SetUp();
}
};

TEST_F(AllowOther, allowed)
{
	fork(true, [&] {
			uint64_t ino = 42;

			expect_lookup(RELPATH, ino, S_IFREG | 0644, 0, 1);
			expect_open(ino, 0, 1);
			expect_release(ino);
			expect_getattr(ino, 0);
		}, []() {
			int fd;

			fd = open(FULLPATH, O_RDONLY);
			if (fd < 0) {
				perror("open");
				return(1);
			}
			return 0;
		}
	);
}

TEST_F(NoAllowOther, disallowed)
{
	fork(true, [] {
		}, []() {
			int fd;

			fd = open(FULLPATH, O_RDONLY);
			if (fd >= 0) {
				fprintf(stderr, "open should've failed\n");
				return(1);
			} else if (errno != EPERM) {
				fprintf(stderr, "Unexpected error: %s\n",
					strerror(errno));
				return(1);
			}
			return 0;
		}
	);
}
