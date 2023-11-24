#!/bin/sh

#
# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) 2019 Peter Holm
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

# pipe(2) tests.
# "panic: vm_page_dequeue: queued unlocked page 0xfffffe0019a73518" seen.

# Reported by syzkaller
# Fixed by r354400

. ../default.cfg

dir=/tmp
odir=`pwd`
cd $dir
sed '1,/^EOF/d' < $odir/$0 > $dir/pipe3.c
mycc -o pipe3 -Wall -Wextra -O0 -g pipe3.c || exit 1
rm -f pipe3.c
cd $odir

(cd ../testcases/swap; ./swap -t 5m -i 20 -l 100) &
sleep 1
for i in `jot 25`; do
	/tmp/pipe3 &
done
while pkill -0 pipe3; do sleep 2; done
while pkill -9 swap; do sleep 1; done
wait
rm -rf /tmp/pipe3
exit 0

EOF
#include <sys/types.h>
#include <sys/wait.h>

#include <err.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#define PARALLEL 64
#define RUNTIME (5 * 60)

static void
handler(int i __unused) {
	_exit(0);
}

void
test(void)
{
	int fds[2], r;
	char c;

	if (pipe(fds) == -1)
		err(1, "pipe");

	if (fork() == 0) {
		signal(SIGALRM, handler);
		ualarm(1 + arc4random() % 10000, 0);
		close(fds[1]);
		if ((r = write(fds[0], &c, sizeof(c))) != sizeof(c))
			if (r == -1)
				err(1, "pipe write");
		_exit(0);
	}
	signal(SIGALRM, handler);
	ualarm(1 + arc4random() % 10000, 0);
	close(fds[0]);
	if ((r = read(fds[1], &c, sizeof(c))) != sizeof(c))
		if (r == -1)
			err(1, "pipe read");
	wait(NULL);
	_exit(0);
}

int
main(void)
{
	pid_t pids[PARALLEL], rpid;
	time_t start;
	int i, running, status;
	bool done;

	for (i = 0; i < PARALLEL; i++)
		pids[i] = 0;
	running = 0;
	start = time(NULL);
	for (;;) {
		done = (time(NULL) - start) >= RUNTIME;
		for (i = 0; i < PARALLEL; i++) {
			if (pids[i] == 0 && !done) {
				if ((pids[i] = fork()) == 0)
					test();
				if (pids[i] == -1)
					err(1, "fork()");
				running++;
			}
		}
		for (i = 0; i < PARALLEL; i++) {
			if (pids[i] != 0) {
				if ((rpid = waitpid(pids[i], &status,
				    WNOHANG)) == -1)
					err(1, "waitpid(%d)", pids[i]);
				if (rpid == 0)
					continue;
				if (rpid != pids[i])
					err(1, "waitpid(%d)", pids[i]);
				running --;
				pids[i] = 0;
				break;
			}
		}
		if (running == 0 && done)
			break;
		usleep(100);
	}

	return (0);
}
