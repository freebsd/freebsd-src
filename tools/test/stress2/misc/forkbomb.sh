#!/bin/sh

#
# Copyright (c) 2015 EMC Corp.
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

# Fork bomb memory leak test scenario.
# https://en.wikipedia.org/wiki/Fork_bomb

# OO memory seen:
# https://people.freebsd.org/~pho/stress/log/forkbomb.txt
# Fixed by r289026.

. ../default.cfg
[ `id -u ` -ne 0 ] && echo "Must be root!" && exit 1

dir=/tmp
odir=`pwd`
cd $dir
sed '1,/^EOF/d' < $odir/$0 > $dir/forkbomb.c
mycc -o forkbomb -Wall -Wextra -O0 -g forkbomb.c || exit 1
rm -f forkbomb.c
cd $odir

mount | grep "on $mntpoint " | grep -q /dev/md && umount -f $mntpoint
mdconfig -l | grep -q md$mdstart &&  mdconfig -d -u $mdstart
mdconfig -a -t swap -s 512m -u $mdstart || exit 1
newfs $newfs_flags md$mdstart > /dev/null
mount /dev/md$mdstart $mntpoint

su $testuser -c /tmp/forkbomb

while mount | grep "on $mntpoint " | grep -q /dev/md; do
	umount $mntpoint || sleep 1
done
mdconfig -d -u $mdstart
rm -rf /tmp/forkbomb
exit 0

EOF
#include <sys/param.h>
#include <sys/mman.h>

#include <machine/atomic.h>

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

volatile u_int *share;

#define R1 1 /* sync start  */
#define R2 2 /* forks       */
#define R3 3 /* exits       */
#define R4 4 /* fork failed */

//#define DEBUG
#define MXFAIL 100
#define MAXPROC 40000	/* Arbitrary cap */
#define PARALLEL 200

void
test(void)
{
	int r;

	alarm(1200);
	atomic_add_int(&share[R1], 1);
	while (share[R1] != PARALLEL)
		;
	atomic_add_int(&share[R2], 1);

	for (;;) {
		if (share[R2] >= MAXPROC || share[R4] > MXFAIL)
			break;
		atomic_add_int(&share[R2], 1);
		if ((r = fork()) == -1) {
			atomic_add_int(&share[R4], 1);
			atomic_add_int(&share[R2], -1);
			break;
		}
	}

	atomic_add_int(&share[R3], 1);
	_exit(0);
}

int
main(void)
{
	struct sigaction sa;
	size_t len;
	int i;

	alarm(1200);
	len = PAGE_SIZE;
	if ((share = mmap(NULL, len, PROT_READ | PROT_WRITE,
	    MAP_ANON | MAP_SHARED, -1, 0)) == MAP_FAILED)
		err(1, "mmap");

	sa.sa_handler = SIG_IGN;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = 0;
	if (sigaction(SIGCHLD, &sa, 0) == -1)
		err(1, "sigaction");

	for (i = 0; i < PARALLEL; i++) {
		if (fork() == 0)
			test();
	}

	while (share[R2] == 0 || share[R3] < share[R2])
		sleep(1);

#if defined(DEBUG)
	fprintf(stderr, "MAXPROC: %d. forks: %u / exits: %u / fails: %u\n",
	    MAXPROC, share[R2], share[R3], share[R4]);
#endif

	return (0);
}
