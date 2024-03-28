/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2024 Lin Lee <leelin2602@gmail.com>
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

#include <atf-c.h>
#include <sys/wait.h>
#include <sys/syscall.h>

static int
simple_test(int exit_status)
{
    pid_t pid;
    int status;

    pid = syscall(SYS_fork);

    if (pid == -1) {
        return (0);
    } else if (pid == 0) {
        syscall(SYS_exit, exit_status);
    } else {
        syscall(SYS_wait4, pid, &status, 0, NULL);
        if (WIFEXITED(status)) {
            return (WEXITSTATUS(status) == exit_status);
        }
    }
    return (0);
}



ATF_TC(test_simple_test_89);
ATF_TC_HEAD(test_simple_test_89, tc) {
    atf_tc_set_md_var(tc, "descr", "Test simple_test with expected code 89.");
}
ATF_TC_BODY(test_simple_test_89, tc) {
    ATF_REQUIRE(simple_test(89));
}

ATF_TC(test_simple_test_64);
ATF_TC_HEAD(test_simple_test_64, tc) {
    atf_tc_set_md_var(tc, "descr", "Test simple_test with expected code 64.");
}
ATF_TC_BODY(test_simple_test_64, tc) {
    ATF_REQUIRE(simple_test(64));
}

ATF_TP_ADD_TCS(tp) {
    ATF_TP_ADD_TC(tp, test_simple_test_89);
    ATF_TP_ADD_TC(tp, test_simple_test_64);

    return (atf_no_error());
}
