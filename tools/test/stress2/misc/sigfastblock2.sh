#!/bin/sh

#
# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) 2020 corydoras@ridiculousfish.com
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
# FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
# OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
# HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
# LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
# OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
# SUCH DAMAGE.
#

# The test scenario is from:
# Bug 246385 - SIGCHLD dropped if generated while blocked in sigfastblock
# https://bugs.freebsd.org/bugzilla/show_bug.cgi?id=246385
# Fixed by r360940

. ../default.cfg

cat > /tmp/sigfastblock2.c <<EOF
#include <assert.h>
#include <fcntl.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include <sys/types.h>
#include <sys/wait.h>
#include <sys/time.h>

static int pipes[2];

static void assert_noerr(int res)
{
    if (res < 0)
    {
        fprintf(stderr, "error: %s", strerror(res));
        exit(EXIT_FAILURE);
    }
}

static void sigchld_handler(int sig)
{
    // Write a byte to the write end of pipes.
    (void)sig;
    char c = 0;
    ssize_t amt = write(pipes[1], &c, 1);
    if (amt < 0)
    {
        perror("write");
        exit(EXIT_FAILURE);
    }
}

static pid_t spawn_child()
{
    pid_t pid = fork();
    assert_noerr(pid);
    if (pid == 0)
    {
        _exit(0);
    }
    return pid;
}

int main(void)
{
    // Install SIGCHLD handler with SA_RESTART.
    struct sigaction chld_act = {};
    sigemptyset(&chld_act.sa_mask);
    chld_act.sa_handler = &sigchld_handler;
    chld_act.sa_flags = SA_RESTART;
    assert_noerr(sigaction(SIGCHLD, &chld_act, NULL));

    // Make our self-pipes.
    assert_noerr(pipe(pipes));

    // Spawn and reap children in a loop.
    for (;;)
    {
        // Spawn a child.
        pid_t child = spawn_child();
        assert(child >= 0 && "Invalid pid");

        // Wait for the SIGCHLD handler to write to us.
        for (;;)
        {
            char buf[PIPE_BUF];
	    alarm(5);
            ssize_t amt = read(pipes[0], buf, sizeof buf);
            if (amt >= 0)
                break;
	    alarm(0);
        }

        // Reap it.
        int status = 0;
        int pid_res = waitpid(child, &status, WUNTRACED | WCONTINUED);
        assert(pid_res == child && "Unexpected pid");
    }

    return 0;
}
EOF
mycc -o /tmp/sigfastblock2 -Wall -Wextra /tmp/sigfastblock2.c -lpthread ||
    exit 1

timeout 5m /tmp/sigfastblock2; s=$?
[ $s -eq 124 ] && s=0

rm -f /tmp/sigfastblock2 /tmp/sigfastblock2.c
exit $s
