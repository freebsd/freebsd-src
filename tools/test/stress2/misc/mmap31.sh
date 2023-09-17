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

# Test scenario for 2MB page promotions.
# No problems seen.

. ../default.cfg
[ `id -u ` -ne 0 ] && echo "Must be root!" && exit 1
[ "`uname -p`" = "amd64" ] || exit 0

dir=/tmp
odir=`pwd`
cd $dir
sed '1,/^EOF/d' < $odir/$0 > $dir/mmap31.c
mycc -o mmap31 -Wall -Wextra -O0 -g mmap31.c || exit 1
rm -f mmap31.c
cd $odir

(cd ../testcases/swap; ./swap -t 10m -i 20) > /dev/null &

/tmp/mmap31
s=$?
kill $! 2>/dev/null
while pkill -9 swap; do :; done
wait

rm -rf /tmp/mmap31
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
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <time.h>
#include <unistd.h>

#define SEGS 512
static size_t l[SEGS];
static char *c[SEGS];
static volatile char r;
static volatile u_int *share;

#define PARALLEL 8
#define RUNTIME (2 * 60)
#define SYNC 0

static void
handler(int s __unused)
{
	_exit(0);
}

static void
touch(void)
{
	time_t t;
	int idx;
	char *c1;

	t = time(NULL);
	while (time(NULL) - t < 5) {
		idx = arc4random() % SEGS;
		c1 = c[idx];
		idx = arc4random() % l[idx];
		r += c1[idx];
		c1[idx] += 1;
	}
}

static void
test(void)
{
	pid_t pid;
	int i, mode, status;

	r = 0;
	for (i = 0; i < SEGS; i++) {
		l[i] = (arc4random() % 512 + 1) * PAGE_SIZE;
		mode = PROT_READ | PROT_WRITE;
		if ((c[i] = (char *)mmap(NULL, l[i], mode,
		    MAP_ANON | MAP_SHARED, -1, 0)) == MAP_FAILED)
			err(1, "mmap");
	}

	if ((pid = fork()) == 0) {
		signal(SIGHUP, handler);
		atomic_add_int(&share[SYNC], 1);
		while (share[SYNC] != PARALLEL)
			;
		touch();
		pause();
		_exit(0);
	}

	while (share[SYNC] != PARALLEL)
		;
	touch();

	kill(pid, SIGHUP);
	if (waitpid(pid, &status, 0) != pid)
		err(1, "waitpid");
	if (status != 0)
		errx(1, "child status = %d", status);

	_exit(status);
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
			e += status == 0 ? 0 : 1;
		}
	}

	return (e);
}
