#!/bin/sh

#
# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) 2018 Dell EMC Isilon
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
sed '1,/^EOF/d' < $odir/$0 > $dir/sigstop2.c
mycc -o sigstop2 -Wall -Wextra -O0 -g sigstop2.c || exit 1
rm -f sigstop2.c

$dir/sigstop2
s=$?
[ -f sigstop2.core -a $s -eq 0 ] &&
    { ls -l sigstop2.core; mv sigstop2.core $dir; s=1; }

rm -rf $dir/sigstop2
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

#define N 100
#define PARALLEL 4
#define RUNTIME (1 * 60)
#define SYNC 0

static void
test(void)
{
	pid_t pids[N];
	int i;

	atomic_add_int(&share[SYNC], 1);
	while (share[SYNC] != PARALLEL)
		;

	for (i = 0; i < N; i++) {
		if ((pids[i] = fork()) == 0) {
			for (;;)
				pause();
			_exit(0);
		}
	}

	for (i = 0; i < N; i++)
		if (kill(pids[i], SIGSTOP) == -1)
			err(1, "kill(%d)", pids[i]);
	for (i = 0; i < N; i++)
		if (kill(pids[i], SIGINT) == -1)
			err(1, "kill(%d)", pids[i]);
	for (i = 0; i < N; i++)
		if (kill(pids[i], SIGKILL) == -1)
			err(1, "kill(%d)", pids[i]);
	for (i = 0; i < N; i++)
		if (waitpid(pids[i], NULL, 0) != pids[i])
			err(1, "waitpid(%d)", pids[i]);

	_exit(0);
}

int
main(void)
{
	pid_t pids[PARALLEL];
	size_t len;
	time_t start;
	int e, i, status;

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
				test();
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
