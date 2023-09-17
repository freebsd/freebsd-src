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

# Attempt to reproduce "vm_page_assert_xbusied: page XXXX not exclusive busy"
# No problems seen.

# Test scenario idea by markj@

. ../default.cfg
[ `id -u` -ne 0 ] && echo "Must be root!" && exit 1
[ `sysctl -n vm.swap_total` -eq 0 ] && exit 0

dir=/tmp
odir=`pwd`
cd $dir
sed '1,/^EOF/d' < $odir/$0 > $dir/mmap40.c
mycc -o mmap40 -Wall -Wextra -O0 -g mmap40.c || exit 1
rm -f mmap40.c
cd $odir

set -e
mount | grep "on $mntpoint " | grep -q /dev/md && umount -f $mntpoint
[ -c /dev/md$mdstart ] &&  mdconfig -d -u $mdstart
mdconfig -a -t swap -s 2g -u $mdstart
newfs $newfs_flags md$mdstart > /dev/null
mount /dev/md$mdstart $mntpoint
set +e

u1=`swapinfo | tail -1 | awk '{print $3}'`
(nice $odir/../testcases/swap/swap -t 10m -i 30 -h -l 100) &
while [ $((`swapinfo | tail -1 | awk '{print $3}'` - $u1)) -le 100 ]; do
	sleep 1
done

$dir/mmap40
s=0
while pkill swap; do :; done
wait
[ -f mmap40.core -a $s -eq 0 ] &&
    { ls -l mmap40.core; mv mmap40.core $dir; s=1; }
cd $odir

for i in `jot 6`; do
	mount | grep -q "on $mntpoint " || break
	umount $mntpoint && break || sleep 10
	[ $i -eq 6 ] &&
	    { echo FATAL; fstat -mf $mntpoint; exit 1; }
done
mdconfig -d -u $mdstart
rm -rf $dir/mmap40
exit $s

EOF
#include <sys/param.h>
#include <sys/mman.h>
#include <sys/wait.h>

#include <err.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define PARALLEL 512
#define RUNTIME 300

void
test(void)
{
	pid_t pid;
	size_t i, len;
	time_t start;
	void *p;
	char *vec;

	len = 1024 * 1024;
	if ((p = mmap(NULL, len, PROT_READ | PROT_WRITE, MAP_ANON, -1, 0)) ==
	    MAP_FAILED)
		err(1, "mmap");
	memset(p, 0, len); /* dirty the memory */
	if ((vec = malloc(len / PAGE_SIZE)) == NULL)
		err(1, "malloc");

	start = time(NULL);
	while (time(NULL) - start < RUNTIME) {
		usleep(arc4random() % 1000);
		if (mincore(p, len, vec) == -1)
			err(1, "mincore");
		for (i = 0; i < len / PAGE_SIZE; i++) {
			if ((vec[i] & MINCORE_MODIFIED) == 0) {
				_exit(0);
			}
		}
		if ((pid = fork()) == 0) {
			_exit(0);
		}
		if (waitpid(pid, NULL, 0) != pid)
			err(1, "waitpid)");
	}

	_exit(0);
}

int
main(void)
{
	pid_t pids[PARALLEL];
	time_t start;
	int i;

	for (i = 0; i < PARALLEL; i++) {
		if ((pids[i] = fork()) == 0)
			test();
	}

	start = time(NULL);
	while (time(NULL) - start < RUNTIME) {
		for (i = 0; i < PARALLEL; i++) {
			if (waitpid(pids[i], NULL, WNOHANG) == pids[i]) {
				if ((pids[i] = fork()) == 0)
					test();
			}
		}
	}

	for (i = 0; i < PARALLEL; i++) {
		if (waitpid(pids[i], NULL, 0) != pids[i])
			err(1, "waitpid");
	}

	return (0);
}
