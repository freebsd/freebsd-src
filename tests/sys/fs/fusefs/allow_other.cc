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
#include <sys/mman.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <pwd.h>
#include <semaphore.h>
}

#include "mockfs.hh"
#include "utils.hh"

using namespace testing;

void sighandler(int __unused sig) {}

static void
get_unprivileged_uid(int *uid)
{
	struct passwd *pw;

	/* 
	 * First try "tests", Kyua's default unprivileged user.  XXX after
	 * GoogleTest gains a proper Kyua wrapper, get this with the Kyua API
	 */
	pw = getpwnam("tests");
	if (pw == NULL) {
		/* Fall back to "nobody" */
		pw = getpwnam("nobody");
	}
	if (pw == NULL)
		GTEST_SKIP() << "Test requires an unprivileged user";
	*uid = pw->pw_uid;
}

class NoAllowOther: public FuseTest {

public:
/* Unprivileged user id */
int m_uid;

virtual void SetUp() {
	if (geteuid() != 0) {
		GTEST_SKIP() << "This test must be run as root";
	}
	get_unprivileged_uid(&m_uid);
	if (IsSkipped())
		return;

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
	const char FULLPATH[] = "mountpoint/some_file.txt";
	const char RELPATH[] = "some_file.txt";
	uint64_t ino = 42;
	int fd;
	pid_t child;
	sem_t *sem;
	int mprot = PROT_READ | PROT_WRITE;
	int mflags = MAP_ANON | MAP_SHARED;
	
	sem = (sem_t*)mmap(NULL, sizeof(*sem), mprot, mflags, -1, 0);
	ASSERT_NE(NULL, sem) << strerror(errno);
	ASSERT_EQ(0, sem_init(sem, 1, 0)) << strerror(errno);

	if ((child = fork()) == 0) {
		/* In child */
		ASSERT_EQ(0, sem_wait(sem)) << strerror(errno);

		/* Drop privileges before accessing */
		if (0 != setreuid(-1, m_uid)) {
			perror("setreuid");
			_exit(1);
		}
		fd = open(FULLPATH, O_RDONLY);
		if (fd < 0) {
			perror("open");
			_exit(1);
		}
		_exit(0);

		/* Deliberately leak fd */
	} else if (child > 0) {
		int child_status;

		/* 
		 * In parent.  Cleanup must happen here, because it's still
		 * privileged.
		 */
		expect_lookup(RELPATH, ino, S_IFREG | 0644, 0, 1);
		expect_open(ino, 0, 1);
		expect_release(ino);
		/* Until the attr cache is working, we may send an additional
		 * GETATTR */
		expect_getattr(ino, 0);
		m_mock->m_child_pid = child;
		/* Signal the child process to go */
		ASSERT_EQ(0, sem_post(sem)) << strerror(errno);

		wait(&child_status);
		ASSERT_EQ(0, WEXITSTATUS(child_status));
	} else {
		FAIL() << strerror(errno);
	}
}

TEST_F(NoAllowOther, disallowed)
{
	const char FULLPATH[] = "mountpoint/some_file.txt";
	int fd;
	pid_t child;
	sem_t *sem;
	int mprot = PROT_READ | PROT_WRITE;
	int mflags = MAP_ANON | MAP_SHARED;

	sem = (sem_t*)mmap(NULL, sizeof(*sem), mprot, mflags, -1, 0);
	ASSERT_NE(NULL, sem) << strerror(errno);
	ASSERT_EQ(0, sem_init(sem, 1, 0)) << strerror(errno);

	if ((child = fork()) == 0) {
		/* In child */
		ASSERT_EQ(0, sem_wait(sem)) << strerror(errno);

		/* Drop privileges before accessing */
		if (0 != setreuid(-1, m_uid)) {
			perror("setreuid");
			_exit(1);
		}
		fd = open(FULLPATH, O_RDONLY);
		if (fd >= 0) {
			fprintf(stderr, "open should've failed\n");
			_exit(1);
		} else if (errno != EPERM) {
			fprintf(stderr,
				"Unexpected error: %s\n", strerror(errno));
			_exit(1);
		}
		_exit(0);

		/* Deliberately leak fd */
	} else if (child > 0) {
		/* 
		 * In parent.  Cleanup must happen here, because it's still
		 * privileged.
		 */
		m_mock->m_child_pid = child;
		/* Signal the child process to go */
		ASSERT_EQ(0, sem_post(sem)) << strerror(errno);
		int child_status;

		wait(&child_status);
		ASSERT_EQ(0, WEXITSTATUS(child_status));
	} else {
		FAIL() << strerror(errno);
	}
}
