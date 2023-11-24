#!/bin/sh

#
# Copyright (c) 2017 Dell EMC Isilon
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

# Hunt for lost wakeup problem.

. ../default.cfg

dir=/tmp
odir=`pwd`
cd $dir
sed '1,/^EOF/d' < $odir/$0 > $dir/pause.c
mycc -o pause -Wall -Wextra -O0 -g pause.c || exit 1
rm -f pause.c

pkill pause
$dir/pause &
pid=$!
start=`date +%s`
while pgrep -q pause; do
	sleep .5
	[ $((`date +%s` - $start)) -gt 1200 ] &&
		{ echo "Timed out"; pgrep pause | xargs ps -lp; exit 1; }
done
wait $pid
s=$?

cd $odir
rm -rf $dir/pause
exit $s

EOF
#include <sys/param.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/wait.h>

#include <machine/atomic.h>

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

static volatile u_int *share;

#define PARALLEL 400
#define RUNTIME (5 * 60)
#define SYNC 0

void
hand(int i __unused) {
}

static void
test(int idx)
{
	pid_t pid;
	time_t start;

	atomic_add_int(&share[SYNC], 1);
	while (share[SYNC] != PARALLEL)
		usleep(1);
	share[idx] = 0;

	if ((pid = fork()) == 0) {
		share[idx] = 1;
		for (;;)
			pause();
		_exit(0);
	}
	while (share[idx] == 0)
		usleep(10);
	start = time(NULL);
	while (time(NULL) - start < 60) {
		usleep(arc4random() % 100);
		if (kill(pid, SIGHUP) == -1)
			err(1, "kill(%d)", pid);
	}
	kill(pid, SIGTERM);
	if (waitpid(pid, NULL, 0) != pid)
		err(1, "waitpid(%d)", pid);

	_exit(0);
}

int
main(void)
{
	pid_t pids[PARALLEL];
	struct sigaction sa;
	size_t len;
	time_t start;
	int e, i, status;

	sa.sa_handler = hand;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = 0;
	if (sigaction(SIGHUP, &sa, NULL) == -1)
		err(1, "sigaction");

	e = 0;
	len = PAGE_SIZE;
	if ((share = mmap(NULL, len, PROT_READ | PROT_WRITE,
	    MAP_ANON | MAP_SHARED, -1, 0)) == MAP_FAILED)
		err(1, "mmap");

	start = time(NULL);
	while ((time(NULL) - start) < RUNTIME && e == 0) {
		share[SYNC] = 0;
		for (i = 0; i < PARALLEL; i++) {
			if ((pids[i] = fork()) == 0)
				test(i + 1);
			if (pids[i] == -1)
				err(1, "fork()");
		}
		for (i = 0; i < PARALLEL; i++) {
			if (waitpid(pids[i], &status, 0) == -1)
				err(1, "waitpid(%d)", pids[i]);
			if (status != 0) {
				if (WIFSIGNALED(status))
					fprintf(stderr,
					    "pid %d exit signal %d\n",
					    pids[i], WTERMSIG(status));
			}
			e += status == 0 ? 0 : 1;
		}
	}

	return (e);
}
