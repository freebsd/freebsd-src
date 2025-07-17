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

# minherit() test scenario inspired by Jeff's collapse.sh test.
# No problems seen.

. ../default.cfg
[ `id -u` -ne 0 ] && echo "Must be root!" && exit 1

dir=/tmp
odir=`pwd`
cd $dir
sed '1,/^EOF/d' < $odir/$0 > $dir/minherit.c
mycc -o minherit -Wall -Wextra -O0 -g minherit.c || exit 1
rm -f minherit.c
cd $odir

(cd $odir/../testcases/swap; ./swap -t 5m -i 10 -l 50) &
pid=$!
cd /tmp
$dir/minherit
s=$?
while pkill swap; do :; done
wait $oid
[ -f minherit.core -a $s -eq 0 ] &&
    { ls -l minherit.core; mv minherit.core $dir; s=1; }
cd $odir

rm -rf $dir/minherit
exit $s

EOF
#include <sys/param.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/wait.h>

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

static _Atomic(int) *share;

#define ADRSPACE  (1024 * 1024)
#define CHILDREN 200
#define PARALLEL 10
#define RUNTIME (5 * 60)
#define SYNC 0

static void
test(void)
{
	pid_t pids[CHILDREN];
	off_t len;
	void *p;
	int i, ix, j, n, shared;
	char *cp;

	(void)atomic_fetch_add(&share[SYNC], 1);
	while (atomic_load(&share[SYNC]) != PARALLEL)
		;

	if ((p = mmap(NULL, ADRSPACE, PROT_READ | PROT_WRITE,
	    MAP_SHARED | MAP_ANON, -1, 0)) == MAP_FAILED) {
		if (errno == ENOMEM)
			return;
		err(1, "mmap()");
	}

	/* Pick a random bit of address space to change inherit on. */
	for (i = 0; i < ADRSPACE; i += len) {
		shared = arc4random() & 0x1;
		len = roundup2(arc4random() % ((ADRSPACE - i) / 4),
		    PAGE_SIZE);
		if (minherit(p + i, len, shared ? INHERIT_SHARE :
		    INHERIT_COPY) != 0)
			err(1, "minherit");
	}

	n = arc4random() % CHILDREN + 1;
	for (i = 0; i < n; i++) {
		pids[i] = fork();
		if (pids[i] == -1)
			err(1, "fork()");
		if (pids[i] == 0) {
			usleep(arc4random() % 100);
			for (j = 0; j < 10; j++) {
				cp = p;
				for (ix = 0; ix < ADRSPACE; ix += PAGE_SIZE) {
					cp[ix] = 1;
					if (arc4random() % 100 < 5)
						usleep(arc4random() % 50);
				}
			}
			_exit(0);
		}
	}
	for (i = 0; i < n; i++) {
		if (waitpid(pids[i], NULL, 0) == -1)
			err(1, "waitpid(%d)", pids[i]);
	}

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
				e = status;
				break;
			}
		}
	}

	return (e);
}
