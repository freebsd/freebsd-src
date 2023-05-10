#!/bin/sh

#
# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) 2016 Mark Johnston <markj@FreeBSD.org>
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

# ptrace(2) test scenario by Mark Johnston
# https://people.freebsd.org/~markj/ptrace_stop.c
# Fixed by r303423.

. ../default.cfg

cd /tmp
cat > ptrace9.c <<EOF
#include <sys/types.h>
#include <sys/ptrace.h>
#include <sys/wait.h>

#include <err.h>
#include <signal.h>
#include <stdio.h>
#include <unistd.h>

static void
sigalrm(int sig __unused)
{
	_exit(0);
}

static void
sighup(int sig __unused)
{
}

int
main(void)
{
	struct sigaction act;
	pid_t pid;
	int e, status;

	signal(SIGALRM, sigalrm);
	e = 1;
	pid = fork();
	if (pid < 0)
		err(1, "fork");
	if (pid == 0) {
		act.sa_handler = sighup;
		act.sa_flags = 0;
		sigemptyset(&act.sa_mask);
		if (sigaction(SIGHUP, &act, NULL) != 0)
			err(1, "sigaction");
		alarm(5);
		while (1) {
			sleep(1);
		}
	} else {
		alarm(5);
		sleep(1); /* give the child a chance to call sigaction */

		if (kill(pid, SIGSTOP) != 0)
			err(1, "kill(SIGSTOP)");

		printf("waiting for child to stop...\n");
		if (waitpid(pid, &status, WUNTRACED) != pid)
			err(1, "waitpid");
		if (!WIFSTOPPED(status) || WSTOPSIG(status) != SIGSTOP)
			errx(1, "unexpected status %d after SIGSTOP", status);

		if (kill(pid, SIGHUP) != 0)
			err(1, "kill(SIGHUP)");

		if (ptrace(PT_ATTACH, pid, NULL, 0) != 0)
			err(1, "ptrace(PT_ATTACH)");
		if (waitpid(pid, &status, WUNTRACED) != pid)
			err(1, "waitpid");
		if (!WIFSTOPPED(status))
			errx(1, "unexpected status %d after PT_ATTACH", status);
		printf("stopping signal is %d\n", WSTOPSIG(status));
		if (ptrace(PT_DETACH, pid, NULL, 0) != 0)
			err(1, "ptrace(PT_DETACH)");

		/* if ptrace works as expected, we'll block here */
		printf("waiting on child...\n"); fflush(stdout);
		if (waitpid(pid, &status, WUNTRACED) != pid)
			err(1, "waitpid");
		if (!WIFSTOPPED(status))
			errx(1, "unexpected status %d after PT_DETACH", status);
		printf("child is stopped after detach (sig %d)\n",
		    WSTOPSIG(status)); fflush(stdout);
		e = 1;
	}

	return (e);
}
EOF

mycc -o ptrace9 -Wall -Wextra -O2 -g ptrace9.c || exit 1
rm ptrace9.c

echo "Expect:
	waiting for child to stop...
	stopping signal is 17
	waiting on child..."
./ptrace9
s=$?

pkill -9 ptrace9
rm -f ptrace9
exit $s
