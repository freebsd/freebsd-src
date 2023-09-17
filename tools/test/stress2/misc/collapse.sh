#!/bin/sh

#
# Copyright (c) 2020 Jeffrey Roberson <jeff@FreeBSD.org>
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

. ../default.cfg

# "panic: freeing mapped page 0xfffffe000aa73910" seen:
# https://people.freebsd.org/~pho/stress/log/collapse.txt

odir=`pwd`
cd /tmp
sed '1,/^EOF/d' < $odir/$0 > collapse.c
mycc -o collapse -Wall -Wextra -g -O0 collapse.c || exit 1
rm -f collapse.c
cd $odir

daemon sh -c '(cd ../testcases/swap; ./swap -t 20m -i 16 -l 85)' > \
    /dev/null 2>&1
sleep 2
/tmp/collapse
while pgrep -q swap; do
	pkill -9 swap
done
rm -f /tmp/collapse
exit 0

EOF
#include <sys/param.h>
#include <sys/mman.h>
#include <sys/wait.h>

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define ADRSPACE  (256 * 1024)
#define	DEPTH	6
#define	WIDTH	3
#define PARALLEL 4
#define RUNTIME 1200
#define CHILDTIME 5
#define STARTTIME 5
#define	TOUCH	16

char *p;

static void
child(int depth, time_t start)
{
	time_t run, delay;
	int i, shared, off;
	int len;

	/* Pick a random bit of address space to change inherit on. */
	for (i = 0; i < ADRSPACE; i += len) {
		shared = arc4random() & 0x1;
		len = roundup2(arc4random() % ((ADRSPACE - i) / 4),
		    PAGE_SIZE);
		if (minherit(p + i, len, shared ? INHERIT_SHARE :
		    INHERIT_COPY) != 0)
			err(1, "minherit");
	}

	for (i = 0; depth != 0 && i < WIDTH; i++)
		if (fork() == 0)
			child(depth - 1, start);

	/*
	 * Touch all of the memory and exit at a random time to collapse
	 * some portion of the chain.
	 */
	delay = arc4random() % (CHILDTIME - 1);
	run = arc4random() % (CHILDTIME - delay);
	for (;;) {
		if (time(NULL) >= start + delay)
			break;
		usleep(100);
	}
	while (time(NULL) - start < run) {
		off = rounddown2(arc4random() % ADRSPACE, PAGE_SIZE);
		bzero(p + off, PAGE_SIZE);
		usleep((run * 1000) / TOUCH);
	}

	_exit(0);
}

static void
work(int depth)
{

	if ((p = mmap(NULL, ADRSPACE, PROT_READ | PROT_WRITE,
	    MAP_SHARED | MAP_ANON, -1, 0)) == MAP_FAILED) {
		if (errno == ENOMEM)
			return;
		err(1, "mmap()");
	}
	child(depth, time(NULL) + STARTTIME);
}

int
main(void)
{
	pid_t pids[PARALLEL];
	time_t start;
	int i, n;

	start = time(NULL);
	while (time(NULL) - start < RUNTIME) {
		n = arc4random() % PARALLEL + 1;
		for (i = 0; i < n; i++) {
			if ((pids[i] = fork()) == 0)
				work(DEPTH);
		}

		sleep(CHILDTIME + STARTTIME + 1);

		for (i = 0; i < n; i++)
			if (waitpid(pids[i], NULL, 0) != pids[i])
				err(1, "waitpid(%d)", pids[i]);
	}

	return (0);
}
