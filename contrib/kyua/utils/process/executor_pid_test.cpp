/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2022 Dell Inc.
 * Author: Eric van Gyzen
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
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#if 0

1. Run some "bad" tests that prevent kyua from removing the work directory.
   We use "chflags uunlink".  Mounting a file system from an md(4) device
   is another common use case.
2. Fork a lot, nearly wrapping the PID number space, so step 3 will re-use
   a PID from step 1.  Running the entire FreeBSD test suite is a more
   realistic scenario for this step.
3. Run some more tests.  If the stars align, the bug is not fixed yet, and
   kyua is built with debugging, kyua will abort with the following messages.
   Without debugging, the tests in step 3 will reuse the context from step 1,
   including stdout, stderr, and working directory, which are still populated
   with stuff from step 1.  When I found this bug, step 3 was
   __test_cases_list__, which expects a certain format in stdout and failed
   when it found something completely unrelated.
4. You can clean up with: chflags -R nouunlink /tmp/kyua.*; rm -rf /tmp/kyua.*

$ cc -o pid_wrap -latf-c pid_wrap.c
$ kyua test
pid_wrap:leak_0  ->  passed  [0.001s]
pid_wrap:leak_1  ->  passed  [0.001s]
pid_wrap:leak_2  ->  passed  [0.001s]
pid_wrap:leak_3  ->  passed  [0.001s]
pid_wrap:leak_4  ->  passed  [0.001s]
pid_wrap:leak_5  ->  passed  [0.001s]
pid_wrap:leak_6  ->  passed  [0.001s]
pid_wrap:leak_7  ->  passed  [0.001s]
pid_wrap:leak_8  ->  passed  [0.001s]
pid_wrap:leak_9  ->  passed  [0.001s]
pid_wrap:pid_wrap  ->  passed  [1.113s]
pid_wrap:pid_wrap_0  ->  passed  [0.001s]
pid_wrap:pid_wrap_1  ->  passed  [0.001s]
pid_wrap:pid_wrap_2  ->  passed  [0.001s]
pid_wrap:pid_wrap_3  ->  *** /usr/src/main/contrib/kyua/utils/process/executor.cpp:779: Invariant check failed: PID 60876 already in all_exec_handles; not properly cleaned up or reused too fast
*** Fatal signal 6 received
*** Log file is /home/vangyzen/.kyua/logs/kyua.20221006-193544.log
*** Please report this problem to kyua-discuss@googlegroups.com detailing what you were doing before the crash happened; if possible, include the log file mentioned above
Abort trap (core dumped)

#endif

#include <sys/stat.h>

#include <atf-c++.hpp>

#include <fcntl.h>
#include <signal.h>
#include <unistd.h>

#include <cerrno>
#include <cstring>

void
leak_work_dir()
{
	int fd;

	ATF_REQUIRE((fd = open("unforgettable", O_CREAT|O_EXCL|O_WRONLY, 0600))
	    >= 0);
	ATF_REQUIRE_EQ(0, fchflags(fd, UF_NOUNLINK));
	ATF_REQUIRE_EQ(0, close(fd));
}

void
wrap_pids()
{
	pid_t begin, current, target;
	bool wrapped;

	begin = getpid();
	target = begin - 15;
	if (target <= 1) {
		target += 99999;    // PID_MAX
		wrapped = true;
	} else {
		wrapped = false;
	}

	ATF_REQUIRE(signal(SIGCHLD, SIG_IGN) != SIG_ERR);

	do {
		current = vfork();
		if (current == 0) {
			_exit(0);
		}
		ATF_REQUIRE(current != -1);
		if (current < begin) {
			wrapped = true;
		}
	} while (!wrapped || current < target);
}

void
test_work_dir_reuse()
{
	// If kyua is built with debugging, it would abort here before the fix.
}

void
clean_up()
{
	(void)system("chflags -R nouunlink ../..");
}

ATF_TEST_CASE_WITHOUT_HEAD(leak_0);
ATF_TEST_CASE_BODY(leak_0) { leak_work_dir(); }
ATF_TEST_CASE_WITHOUT_HEAD(leak_1);
ATF_TEST_CASE_BODY(leak_1) { leak_work_dir(); }
ATF_TEST_CASE_WITHOUT_HEAD(leak_2);
ATF_TEST_CASE_BODY(leak_2) { leak_work_dir(); }
ATF_TEST_CASE_WITHOUT_HEAD(leak_3);
ATF_TEST_CASE_BODY(leak_3) { leak_work_dir(); }
ATF_TEST_CASE_WITHOUT_HEAD(leak_4);
ATF_TEST_CASE_BODY(leak_4) { leak_work_dir(); }
ATF_TEST_CASE_WITHOUT_HEAD(leak_5);
ATF_TEST_CASE_BODY(leak_5) { leak_work_dir(); }
ATF_TEST_CASE_WITHOUT_HEAD(leak_6);
ATF_TEST_CASE_BODY(leak_6) { leak_work_dir(); }
ATF_TEST_CASE_WITHOUT_HEAD(leak_7);
ATF_TEST_CASE_BODY(leak_7) { leak_work_dir(); }
ATF_TEST_CASE_WITHOUT_HEAD(leak_8);
ATF_TEST_CASE_BODY(leak_8) { leak_work_dir(); }
ATF_TEST_CASE_WITHOUT_HEAD(leak_9);
ATF_TEST_CASE_BODY(leak_9) { leak_work_dir(); }

ATF_TEST_CASE_WITHOUT_HEAD(pid_wrap);
ATF_TEST_CASE_BODY(pid_wrap) { wrap_pids(); }

ATF_TEST_CASE_WITHOUT_HEAD(pid_wrap_0);
ATF_TEST_CASE_BODY(pid_wrap_0) { test_work_dir_reuse(); }
ATF_TEST_CASE_WITHOUT_HEAD(pid_wrap_1);
ATF_TEST_CASE_BODY(pid_wrap_1) { test_work_dir_reuse(); }
ATF_TEST_CASE_WITHOUT_HEAD(pid_wrap_2);
ATF_TEST_CASE_BODY(pid_wrap_2) { test_work_dir_reuse(); }
ATF_TEST_CASE_WITHOUT_HEAD(pid_wrap_3);
ATF_TEST_CASE_BODY(pid_wrap_3) { test_work_dir_reuse(); }
ATF_TEST_CASE_WITHOUT_HEAD(pid_wrap_4);
ATF_TEST_CASE_BODY(pid_wrap_4) { test_work_dir_reuse(); }
ATF_TEST_CASE_WITHOUT_HEAD(pid_wrap_5);
ATF_TEST_CASE_BODY(pid_wrap_5) { test_work_dir_reuse(); }
ATF_TEST_CASE_WITHOUT_HEAD(pid_wrap_6);
ATF_TEST_CASE_BODY(pid_wrap_6) { test_work_dir_reuse(); }
ATF_TEST_CASE_WITHOUT_HEAD(pid_wrap_7);
ATF_TEST_CASE_BODY(pid_wrap_7) { test_work_dir_reuse(); }
ATF_TEST_CASE_WITHOUT_HEAD(pid_wrap_8);
ATF_TEST_CASE_BODY(pid_wrap_8) { test_work_dir_reuse(); }
ATF_TEST_CASE_WITHOUT_HEAD(pid_wrap_9);
ATF_TEST_CASE_BODY(pid_wrap_9) { test_work_dir_reuse(); }

ATF_TEST_CASE_WITHOUT_HEAD(really_clean_up);
ATF_TEST_CASE_BODY(really_clean_up) { clean_up(); }

ATF_INIT_TEST_CASES(tcs)
{
	ATF_ADD_TEST_CASE(tcs, leak_0);
	ATF_ADD_TEST_CASE(tcs, leak_1);
	ATF_ADD_TEST_CASE(tcs, leak_2);
	ATF_ADD_TEST_CASE(tcs, leak_3);
	ATF_ADD_TEST_CASE(tcs, leak_4);
	ATF_ADD_TEST_CASE(tcs, leak_5);
	ATF_ADD_TEST_CASE(tcs, leak_6);
	ATF_ADD_TEST_CASE(tcs, leak_7);
	ATF_ADD_TEST_CASE(tcs, leak_8);
	ATF_ADD_TEST_CASE(tcs, leak_9);

	ATF_ADD_TEST_CASE(tcs, pid_wrap);

	ATF_ADD_TEST_CASE(tcs, pid_wrap_0);
	ATF_ADD_TEST_CASE(tcs, pid_wrap_1);
	ATF_ADD_TEST_CASE(tcs, pid_wrap_2);
	ATF_ADD_TEST_CASE(tcs, pid_wrap_3);
	ATF_ADD_TEST_CASE(tcs, pid_wrap_4);
	ATF_ADD_TEST_CASE(tcs, pid_wrap_5);
	ATF_ADD_TEST_CASE(tcs, pid_wrap_6);
	ATF_ADD_TEST_CASE(tcs, pid_wrap_7);
	ATF_ADD_TEST_CASE(tcs, pid_wrap_8);
	ATF_ADD_TEST_CASE(tcs, pid_wrap_9);

	ATF_ADD_TEST_CASE(tcs, really_clean_up);
}
