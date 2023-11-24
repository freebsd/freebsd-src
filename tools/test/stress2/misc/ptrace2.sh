#!/bin/sh

#
# Copyright (c) 2013 EMC Corp.
# All rights reserved.
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

# Regression test for kern/142757, race condition in traced process signal
# handling. Fixed in r202692.

# Test scenario by Tijl Coosemans, tijl@

. ../default.cfg

cd /tmp

cat > race1.c <<EOF
#include <sys/types.h>
#include <sys/ptrace.h>
#include <sys/wait.h>
#include <signal.h>
#include <stdio.h>
#include <unistd.h>

int
main(void)
{
	pid_t pid;
	int i, status;

	alarm(120);
	/* fork dummy child process */
	pid = fork();
	if (pid == 0) {
		/* child does nothing */
		for (;;) {
			sleep(1);
		}
	} else {
		/* parent */
		sleep(1);
		for (i = 0; i < 100000; i++) {
			/* loop: attach, wait, detach */
			printf("attach ");
			fflush(stdout);
			ptrace(PT_ATTACH, pid, (caddr_t) 0, 0);

			printf("wait ");
			fflush(stdout);
			wait4(pid, &status, 0, NULL);

			printf("detach ");
			fflush(stdout);
			ptrace(PT_DETACH, pid, (caddr_t) 1, 0);

			printf("ok\n");
			fflush(stdout);
		}
	}
	kill(pid, SIGINT);

	return (0);
}
EOF

cat > race2.c <<EOF
#include <sys/types.h>
#include <sys/ptrace.h>
#include <sys/wait.h>
#include <signal.h>
#include <stdio.h>
#include <unistd.h>

int
main(void)
{
	pid_t pid;
	int i, status;

	alarm(120);
	/* fork dummy child process */
	pid = fork();
	if (pid == 0) {
		/* child does nothing */
		for (;;) {
			sleep(1);
		}
	} else {
		/* parent */
		sleep(1);
		ptrace(PT_ATTACH, pid, (caddr_t) 0, 0);
		wait4(pid, &status, 0, NULL);
		for (i = 0; i < 100000; i++) {
			/* loop: continue, kill, wait */
			printf("continue ");
			fflush(stdout);
			ptrace(PT_CONTINUE, pid, (caddr_t) 1, 0);

			printf("kill ");
			fflush(stdout);
			kill(pid, SIGINT);

			printf("wait ");
			fflush(stdout);
			wait4(pid, &status, 0, NULL);

			printf("ok\n");
			fflush(stdout);
		}
	}

	return (0);
}
EOF

mycc -o race1 -Wall -Wextra race1.c
mycc -o race2 -Wall -Wextra race2.c

./race1 > /dev/null || echo "FAIL #1"
./race2 > /dev/null || echo "FAIL #2"

rm -f race1.c race1 race2.c race2
