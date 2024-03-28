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
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <signal.h>
#include <sys/syscall.h>

static void
sigterm_handler(int signum)
{
    printf("Child received SIGTERM, but will not terminate. %d\n", signum);
}

static int
test_sigterm(void)
{
    pid_t pid = fork();

    if (pid < 0) {
        perror("fork");
        return (0);
    } else if (pid == 0) {
        printf("Child process started. PID: %d\n", getpid());

        while (1) {
            sleep(1);
        }
    } else {
        int status;
        pid_t result;

        sleep(1);
        printf("Sending SIGTERM to child...\n");

        if (syscall(SYS_kill, pid, SIGTERM) == -1) {
            perror("kill");
            return (0);
        }

        sleep(1);

        result = syscall(SYS_wait4, pid, &status, WNOHANG, NULL);

        if (result > 0) {
            printf("Child process is terminated.\n");
            return (1);
        }
        printf("Child process is still alive.\n");
        return (0);
    }
}

static int
test_sigkill(void)
{
    pid_t pid = fork();

    if (pid < 0) {
        perror("fork");
        return (0);
    } else if (pid == 0) {
        printf("Child process started. PID: %d\n", getpid());

        struct sigaction sa;

        sa.sa_handler = sigterm_handler;
        sigemptyset(&sa.sa_mask);
        sa.sa_flags = 0;

        if (sigaction(SIGTERM, &sa, NULL) == -1) {
            perror("sigaction");
            exit(EXIT_FAILURE);
        }

        while (1) {
            sleep(1);
        }
    } else {
        int status;
        pid_t result;

        sleep(1);
        printf("Sending SIGTERM to child...\n");

        if (syscall(SYS_kill, pid, SIGTERM) == -1) {
            perror("kill");
            return (0);
        }

        sleep(1);
        result = syscall(SYS_wait4, pid, &status, WNOHANG, NULL);

        if (result == 0) {
            printf("Process is still alive.\n");
        } else {
            return (0);
        }

        printf("Sending SIGKILL to child...\n");

        if (syscall(SYS_kill, pid, SIGKILL) == -1) {
            perror("kill");
            return (0);
        }

        sleep(1);
        result = syscall(SYS_wait4, pid, &status, WNOHANG, NULL);

        if (result == 0) {
            printf("Process is still alive.\n");
            return (0);
        } else if (result == -1) {
            perror("waitpid");
            return (0);
        } else {
            printf("Process is dead.\n");
            return (1);
        }

        return (0);
    }
}

ATF_TC(test_sigterm_tc);
ATF_TC_HEAD(test_sigterm_tc, tc)
{
    atf_tc_set_md_var(tc, "descr", "Test SIGTERM handling.");
}
ATF_TC_BODY(test_sigterm_tc, tc)
{
    ATF_CHECK(test_sigterm() == 1);
}

ATF_TC(test_sigkill_tc);
ATF_TC_HEAD(test_sigkill_tc, tc)
{
    atf_tc_set_md_var(tc, "descr", "Test SIGKILL handling after SIGTERM.");
}
ATF_TC_BODY(test_sigkill_tc, tc)
{
    ATF_CHECK(test_sigkill() == 1);
}

ATF_TP_ADD_TCS(tp)
{
    ATF_TP_ADD_TC(tp, test_sigterm_tc);
    ATF_TP_ADD_TC(tp, test_sigkill_tc);

    return (atf_no_error());
}

