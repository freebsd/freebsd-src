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

# flock(2) read (shared) lock test.

# FAIL: Unfair scheduling?
# share[1] = 359171
# share[2] = 394437
# share[3] = 359488
# share[4] = 394429
# share[5] = 359441
# share[6] = 394281
# share[7] = 359314
# share[8] = 394615

. ../default.cfg

[ `id -u ` -ne 0 ] && echo "Must be root!" && exit 1

dir=/tmp
odir=`pwd`
cd $dir
sed '1,/^EOF/d' < $odir/$0 > $dir/flock.c
mycc -o flock -Wall -Wextra -O0 -g flock.c || exit 1
rm -f flock.c
cd $odir

mount | grep "on $mntpoint " | grep -q /dev/md && umount -f $mntpoint
mdconfig -l | grep -q md$mdstart &&  mdconfig -d -u $mdstart
mdconfig -a -t swap -s 1g -u $mdstart || exit 1
newfs $newfs_flags md$mdstart > /dev/null
mount /dev/md$mdstart $mntpoint

(cd $mntpoint; /tmp/flock)
e=$?

while mount | grep "on $mntpoint " | grep -q /dev/md; do
	umount $mntpoint || sleep 1
done
mdconfig -d -u $mdstart
rm -rf /tmp/flock
exit $e

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
#include <string.h>
#include <time.h>
#include <unistd.h>

volatile u_int *share;
int fd;

#define RENDEZVOUS 0

#define CHILDREN 8
#define LOOPS 8000
#define PARALLEL 1
#define RUNTIME (1 * 60)

void
chld(int id)
{
	while (share[RENDEZVOUS] == 0)
		;

	while (share[RENDEZVOUS] == 1) {
		if (flock(fd, LOCK_SH) == -1)
			err(1, "fcntl @ %d", __LINE__);
		atomic_add_int(&share[id + 1], 1);
		if (flock(fd, LOCK_UN) == -1)
			err(1, "fcntl @ %d", __LINE__);
		usleep(100);
	}

	_exit(0);
}

void
test(void)
{
	int i;
	char file[80];

	snprintf(file, sizeof(file), "file.%05d", getpid());
	if ((fd = open(file, O_RDWR | O_CREAT, 0640)) == -1)
		err(1, "open(%s)", file);
	if (flock(fd, LOCK_EX) == -1)
		err(1, "fcntl @ %d", __LINE__);

	for (i = 0; i < CHILDREN; i++) {
		if (fork() == 0)
			chld(i);
	}

	usleep(200);
	atomic_add_int(&share[RENDEZVOUS], 1); /* start chld */
	for (i = 0; i < LOOPS; i++) {
		if (flock(fd, LOCK_UN) == -1)
			err(1, "fcntl @ %d", __LINE__);
		if (flock(fd, LOCK_EX) == -1)
			err(1, "fcntl @ %d", __LINE__);
	}
	atomic_add_int(&share[RENDEZVOUS], 1); /* stop chld */

	for (i = 0; i < CHILDREN; i++)
		wait(NULL);

	close(fd);
	unlink(file);

	_exit(0);
}

int
main(void)
{
	size_t len;
	time_t start;
	int i, n, pct;

	len = getpagesize();
	if ((share = mmap(NULL, len, PROT_READ | PROT_WRITE, MAP_ANON |
	    MAP_SHARED, -1, 0)) == MAP_FAILED)
		err(1, "mmap");

	start = time(NULL);
	while ((time(NULL) - start) < RUNTIME) {
		share[RENDEZVOUS] = 0;
		for (i = 0; i < PARALLEL; i++) {
			if (fork() == 0)
				test();
		}
		for (i = 0; i < PARALLEL; i++)
			wait(NULL);
	}
	n = 0;
	for (i = 0; i < CHILDREN; i++)
		n += share[i + 1];
	n /= CHILDREN;
	for (i = 0; i < CHILDREN; i++) {
		pct = abs((int)share[i + 1] - n) * 100 / n;
		if (pct > 1) {
			fprintf(stderr, "Unfair scheduling?\n");
			for (i = 0; i < CHILDREN; i++) {
				pct = abs((int)share[i + 1] - n) * 100 / n;
				fprintf(stderr, "share[%d] = %d\n",
				    i+1, share[i+1]);
			}
			break;
		}
	}

	return (0);
}
