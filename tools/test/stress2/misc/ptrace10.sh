#!/bin/sh

#
# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) 2021 Mark Johnston <markj@FreeBSD.org>
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
# https://people.freebsd.org/~markj/ptrace_stop_mt.c
# Fixed by r303423.

# stopped on signal 17 after detach
#  UID   PID  PPID CPU PRI NI  VSZ  RSS MWCHAN STAT TT     TIME COMMAND
# 1001 47125 62778   0  52  0 6568 2456 wait   S+    2  0:00.01 /bin/sh ./ptrace10.sh
# 1001 47146 47125   0  23  0 6108 1928 nanslp S+    2  0:00.00 ./ptrace10
# 1001 47148 47146   0  24  0 6240 1932 -      T+    2  0:00.00 ./ptrace10
# 1001 47148 47146   0  24  0 6240 1932 -      T+    2  0:00.00 ./ptrace10

. ../default.cfg

cd /tmp
cat > ptrace10.c <<EOF
#include <sys/types.h>
#include <sys/ptrace.h>
#include <sys/wait.h>

#include <err.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

static void
sighup(int sig __unused)
{
}

static void
sleep_forever(void)
{

	while (1)
		sleep(1);
}

static void *
thread(void *arg __unused)
{

	sleep_forever();
	return (NULL);
}

int
main(void)
{
	struct sigaction act;
	sigset_t set;
	pthread_t t;
	pid_t pid, ret;
	int e, try, limit, r, status;

	e = 0;
	pid = fork();
	if (pid < 0)
		err(1, "fork");
	if (pid == 0) {
		act.sa_handler = sighup;
		act.sa_flags = 0;
		sigemptyset(&act.sa_mask);
		if (sigaction(SIGHUP, &act, NULL) != 0)
			err(1, "sigaction");

		r = pthread_create(&t, NULL, thread, NULL);
		if (r != 0)
			errc(1, r, "pthread_create");

		/* Force SIGHUP to be delivered to the new thread. */
		sigemptyset(&set);
		sigaddset(&set, SIGHUP);
		r = pthread_sigmask(SIG_BLOCK, &set, NULL);
		if (r != 0)
			errc(1, r, "pthread_sigmask");

		sleep_forever();
	} else {
		sleep(1); /* give the child a chance to set itself up */

		limit = 100;
		for (try = 1; try <= limit; try++) {
			if (kill(pid, SIGHUP) != 0)
				err(1, "kill(SIGHUP)");
			if (ptrace(PT_ATTACH, pid, NULL, 0) != 0)
				err(1, "ptrace(PT_ATTACH)");
			if (waitpid(pid, &status, WUNTRACED) != pid)
				err(1, "waitpid 1");
			if (!WIFSTOPPED(status))
				errx(1, "unexpected status %d after PT_ATTACH",
				    status);
			if (ptrace(PT_DETACH, pid, NULL, 0) != 0)
				err(1, "ptrace(PT_DETACH)");

			sleep(1);
			ret = waitpid(pid, &status, WUNTRACED | WNOHANG);
			if (ret < 0)
				err(1, "waitpid");
			if (ret == 0)
				continue;
			if (!WIFSTOPPED(status))
				errx(1, "unexpected status %d after PT_DETACH",
				    status);
			printf("stopped on signal %d after detach\n",
			    WSTOPSIG(status));
			e = 1;
			break;
		}
	}
	kill(pid, SIGINT);

	return (e);
}
EOF

mycc -o ptrace10 -Wall -Wextra -O2 -g ptrace10.c -lpthread || exit 1
rm ptrace10.c

./ptrace10
s=$?
if [ $s -ne 0 ]; then
	ps -lxH | grep -v grep | egrep "UID|ptrace10"
	while pgrep -q ptrace10; do
		pkill -9 ptrace10
	done
fi
wait

rm -f ptrace10
exit $s
