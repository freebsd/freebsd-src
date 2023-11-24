#!/bin/sh

#
# SPDX-License-Identifier: BSD-2-Clause
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

# fcntl(2) fuzz
# "umount: unmount of /mnt failed: Device busy" seen:
# https://people.freebsd.org/~pho/stress/log/fcntl2.txt

. ../default.cfg
[ `id -u` -ne 0 ] && echo "Must be root!" && exit 1

dir=/tmp
odir=`pwd`
cd $dir
sed '1,/^EOF/d' < $odir/$0 > $dir/fcntl2.c
mycc -o fcntl2 -Wall -Wextra -O0 -g fcntl2.c || exit 1
rm -f fcntl2.c
cd $odir

set -e
mount | grep "on $mntpoint " | grep -q /dev/md && umount -f $mntpoint
[ -c /dev/md$mdstart ] &&  mdconfig -d -u $mdstart
mdconfig -a -t swap -s 2g -u $mdstart
newfs $newfs_flags md$mdstart > /dev/null
mount /dev/md$mdstart $mntpoint
set +e

(cd $odir/../testcases/swap; ./swap -t 3m -i 20) &
cd $mntpoint
limits -n 10000 $dir/fcntl2
s=$?
[ -f fcntl2.core -a $s -eq 0 ] &&
    { ls -l fcntl2.core; mv fcntl2.core $dir; s=1; }
cd $odir
while pkill swap; do :; done

for i in `jot 6`; do
	mount | grep -q "on $mntpoint " || break
	umount $mntpoint && break || sleep 10
	[ $i -eq 6 ] &&
	    { echo FATAL; fstat -mf $mntpoint; exit 1; }
done
mdconfig -d -u $mdstart
rm -rf $dir/fcntl2
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
#include <time.h>
#include <unistd.h>

static volatile u_int *share;

#define PARALLEL 64
#define RUNTIME (3 * 60)
#define SYNC 0

#define N (128 * 1024 / (int)sizeof(u_int32_t))
static u_int32_t r[N];

static unsigned long
makearg(void)
{
	unsigned long val;
	unsigned int i;

	val = arc4random();
	i   = arc4random() % 100;
	if (i < 20)
		val = val & 0xff;
	if (i >= 20 && i < 40)
		val = val & 0xffff;
	if (i >= 40 && i < 60)
		val = (unsigned long)(r) | (val & 0xffff);
#if defined(__LP64__)
	if (i >= 60) {
		val = (val << 32) | arc4random();
		if (i > 80)
			val = val & 0x00007fffffffffffUL;
	}
#endif

	return (val);
}
static void
test(void)
{
	time_t start;
	unsigned long arg3;
	int cmd, fd, i, n, success;
	char file[80];

	atomic_add_int(&share[SYNC], 1);
	while (share[SYNC] != PARALLEL)
		;

	success = 0;
	snprintf(file, sizeof(file), "file.%d", getpid());
	if ((fd = open(file, O_RDWR | O_CREAT | O_TRUNC, DEFFILEMODE))
	    == -1)
		err(1, "open(%s)", file);
	n = arc4random() % 100 + 1;
	for (i = 0; i < n; i++)
		if (write(fd, file, sizeof(file)) != sizeof(file))
			err(1, "write()");
	start = time(NULL);
	while (time(NULL) - start < 60) {
		cmd = arc4random() % 20;
		arg3 = makearg();
		alarm(20);
		if (fcntl(fd, cmd, arg3) != -1)
			success++;
		alarm(0);
	}
	close(fd);
	unlink(file);
	if (success == 0)
		fprintf(stderr, "No calls to fcntl() succeeded.\n");

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
		for (i = 0; i < N; i++)
			r[i] = arc4random();
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
		}
	}

	return (0);
}
