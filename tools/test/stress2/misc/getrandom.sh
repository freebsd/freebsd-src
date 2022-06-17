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

# getrandom() non threaded test
# Reset seen on i386

. ../default.cfg
[ `id -u` -ne 0 ] && echo "Must be root!" && exit 1
[ `uname -m` = "i386" ] || exit 0

dir=/tmp
odir=`pwd`
cd $dir
sed '1,/^EOF/d' < $odir/$0 > $dir/getrandom.c
mycc -o getrandom -Wall -Wextra -O0 -g getrandom.c || exit 1
rm -f getrandom.c
cd $odir

set -e
mount | grep "on $mntpoint " | grep -q /dev/md && umount -f $mntpoint
[ -c /dev/md$mdstart ] &&  mdconfig -d -u $mdstart
mdconfig -a -t swap -s 2g -u $mdstart
newfs $newfs_flags md$mdstart > /dev/null
mount /dev/md$mdstart $mntpoint
set +e

(cd $odir/../testcases/swap; ./swap -t 5m -i 40 -l 100) &
cd $mntpoint
timeout 5m limits -c 0 $dir/getrandom
s=0
while pgrep -q swap; do pkill swap; done
cd $odir

for i in `jot 6`; do
	mount | grep -q "on $mntpoint " || break
	umount $mntpoint && break || sleep 10
	[ $i -eq 6 ] &&
	    { echo FATAL; fstat -mf $mntpoint; exit 1; }
done
mdconfig -d -u $mdstart
rm -rf $dir/getrandom
exit $s

EOF
#include <sys/param.h>
#include <sys/mman.h>
#include <sys/random.h>
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

#define PARALLEL 50
#define RUNTIME (5 * 60)
#define SYNC 0

static void
test(void)
{
	time_t start;

	atomic_add_int(&share[SYNC], 1);
	while (share[SYNC] != PARALLEL)
		;

	alarm(65);
	start = time(NULL);
	while ((time(NULL) - start) < 60)
		getrandom((void *)arc4random(), arc4random(), arc4random() & 3);

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
	while ((time(NULL) - start) < RUNTIME) {
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
