#!/bin/sh

#
# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) 2020 Peter Holm <pho@FreeBSD.org>
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

# No problems seen.

. ../default.cfg

dir=/tmp
odir=`pwd`
cd $dir
sed '1,/^EOF/d' < $odir/$0 > $dir/kevent15.c
mycc -o kevent15 -Wall -Wextra -O0 -g kevent15.c || exit 1
rm -f kevent15.c
cd $odir

(cd ../testcases/swap; ./swap -t 3m -i 20 -l 80) &
sleep 2
cd $dir
timeout 5m ./kevent15
s=$?
while pkill swap; do sleep .1; done
wait
[ -f kevent15.core -a $s -eq 0 ] &&
    { ls -l kevent15.core; s=1; }
cd $odir

rm -rf $dir/kevent15
exit $s

EOF
#include <sys/param.h>
#include <sys/event.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/wait.h>

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

static _Atomic(int) *share;

#define PARALLEL 800
#define RUNTIME (3 * 60)
#define SYNC 0
#define ACT 1

static void
handler(int i __unused) {
	_exit(0);
}

static void
test(void)
{
	struct kevent ev[2];
	struct timespec ts;
	time_t start;
	int kq, ret;

	signal(SIGUSR1, handler);
	(void)atomic_fetch_add(&share[SYNC], 1);
	while (atomic_load(&share[SYNC]) != PARALLEL)
		usleep(1);
	(void)atomic_fetch_add(&share[ACT], 1);

	if ((kq = kqueue()) < 0)
		err(1, "kqueue");

	EV_SET(&ev[0], 42, EVFILT_TIMER, EV_ADD | EV_ENABLE | EV_ONESHOT, 0,
	    0, 0);

	start = time(NULL);
	while (time(NULL) - start < 30) {
		ts.tv_sec  = 0;
		ts.tv_nsec = arc4random() % 2000;
		if ((ret = kevent(kq, &ev[0], 1, &ev[1], 1, &ts)) == -1)
			err(1, "kevent");
		if (ret == 1 && (ev[1].flags & EV_ERROR) != 0)
			errc(1, ev[1].data, "kevent");
	}

	_exit(0);
}

int
main(void)
{
	pid_t pids[PARALLEL];
	size_t len;
	time_t start;
	int i, status;

	len = PAGE_SIZE;
	if ((share = mmap(NULL, len, PROT_READ | PROT_WRITE,
	    MAP_ANON | MAP_SHARED, -1, 0)) == MAP_FAILED)
		err(1, "mmap");

	start = time(NULL);
	while ((time(NULL) - start) < RUNTIME) {
		share[ACT] = share[SYNC] = 0;
		for (i = 0; i < PARALLEL; i++) {
			if ((pids[i] = fork()) == 0)
				test();
			if (pids[i] == -1)
				err(1, "fork()");
		}
		while (share[ACT] != PARALLEL)
			usleep(1);
		usleep(arc4random() % 50000);
		for (i = 0; i < PARALLEL; i++) {
			if (kill(pids[i], SIGUSR1) == -1)
				err(1, "kill");
		}

		for (i = 0; i < PARALLEL; i++) {
			if (waitpid(pids[i], &status, 0) == -1)
				err(1, "waitpid(%d)", pids[i]);
			if (status != 0) {
				if (WIFSIGNALED(status) && WTERMSIG(status) != 2)
					fprintf(stderr,
					    "pid %d exit signal %d\n",
					    pids[i], WTERMSIG(status));
			}
		}
	}

	return (0);
}
