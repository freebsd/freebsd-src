#!/bin/sh

#
# SPDX-License-Identifier: BSD-2-Clause-FreeBSD
#
# Copyright (c) 2019 Dell EMC Isilon
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

# "panic: Lock (rw) vm object not locked @ vm/vm_page.c:1013" seen:
# https://people.freebsd.org/~pho/stress/log/mlockall6-2.txt

. ../default.cfg

[ `id -u ` -ne 0 ] && echo "Must be root!" && exit 1
[ `swapinfo | wc -l` -eq 1 ] && exit 0

dir=/tmp
odir=`pwd`
cd $dir
sed '1,/^EOF/d' < $odir/$0 > $dir/mlockall6.c
mycc -o mlockall6 -Wall -Wextra -O0 -g mlockall6.c || exit 1
rm -f mlockall6.c
cd $odir

mount | grep "on $mntpoint " | grep -q /dev/md && umount -f $mntpoint
mdconfig -l | grep -q md$mdstart &&  mdconfig -d -u $mdstart
mdconfig -a -t swap -s 512m -u $mdstart || exit 1
newfs $newfs_flags -n md$mdstart > /dev/null
mount /dev/md$mdstart $mntpoint || exit 1

daemon sh -c "(cd $odir/../testcases/swap; ./swap -t 20m -i 20 -l 100)" \
    > /dev/null 2>&1
sleep 2

(cd $mntpoint; /tmp/mlockall6 || echo FAIL)

while pgrep -q swap; do
	pkill -9 swap
done

n=0
while mount | grep "on $mntpoint " | grep -q /dev/md; do
	umount $mntpoint || sleep 1
	n=$((n + 1))
	[ $n -gt 10 ] && { echo FAIL; exit 1; }
done
mdconfig -d -u $mdstart
rm -rf /tmp/mlockall6
exit 0

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
#include <time.h>
#include <unistd.h>

#define LOOPS 2
#define PARALLEL 8
#define R0 0
#define R1 1
#define R2 2
#define RUNTIME (10 * 60)

static volatile u_int *share;
static int ps;
static char c[32 * 1024 * 1024];

static void
touch(void)
{
	int i;

	for (i = 0; i < (int)sizeof(c); i += ps)
		c[i] = 1;
}

static void
test2(void)
{
	pid_t pid;
	volatile u_int *share2;
	size_t len;
	int i, status;

	len = ps;
	if ((share2 = mmap(NULL, len, PROT_READ | PROT_WRITE,
	    MAP_ANON | MAP_SHARED, -1, 0)) == MAP_FAILED)
		err(1, "mmap");
	touch();
	usleep(arc4random() % 100000);
	alarm(600);
	if ((pid = fork()) == 0) {
		alarm(600);
		while (share2[R1] == 0)	/* Wait for parent */
			;
		atomic_add_int(&share2[R1], 1);
		if (arc4random() % 100 < 50)
			usleep(arc4random() % 1000);
		if (mlockall(MCL_CURRENT | MCL_FUTURE) == -1)
			err(1, "mlock");
		touch();
		atomic_add_int(&share2[R2], 1);
		_exit(0);
	}
	atomic_add_int(&share2[R1], 1);
	while (share2[R1] != 2)	/* Wait for child */
		;

	for (i = 0; i < 100000 && share2[R2] == 0; i++)
		touch();	/* while child is running */

	if (waitpid(pid, &status, 0) != pid)
		err(1, "wait");

	if (status != 0)
		fprintf(stderr, "Got signal %d\n", WTERMSIG(status));
	_exit(WTERMSIG(status));
}

static void
test(void)
{
	pid_t pid;
	int i, s, status;

	while (share[R0] == 0)
		;
	s = 0;
	for (i = 0; i < LOOPS; i++) {
		if ((pid = fork()) == 0)
			test2();
		waitpid(pid, &status, 0);
		s = (s == 0) ? status : s;
	}
	_exit(s);
}

int
main(void)
{
	pid_t pids[PARALLEL];
	size_t len;
	time_t start;
	int i, s, status;

	ps = getpagesize();
	len = ps;
	if ((share = mmap(NULL, len, PROT_READ | PROT_WRITE,
	    MAP_ANON | MAP_SHARED, -1, 0)) == MAP_FAILED)
		err(1, "mmap");
	start = time(NULL);
	s = 0;
	while (s == 0 && (time(NULL) - start) < RUNTIME) {
		for (i = 0; i < PARALLEL; i++) {
			if ((pids[i] = fork()) == 0)
				test();
		}
		atomic_add_int(&share[R0], 1);	/* Start test() runs */
		for (i = 0; i < PARALLEL; i++) {
			if (waitpid(pids[i], &status, 0) != pids[i])
				err(1, "wait");
			if (status != 0) {
				fprintf(stderr, "FAIL: status = %d\n",
				    status);
			}
			s = (s == 0) ? status : s;
		}
		atomic_add_int(&share[R0], -1);
	}

	return (s);
}
