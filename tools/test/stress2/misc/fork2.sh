#!/bin/sh

#
# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) 2022 Peter Holm
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

# Regression test for D36069
# thread_create(): call cpu_copy_thread() after td_pflags is zeroed

# Seen before fix:
# cc: error: unable to execute command: Segmentation fault (core dumped)
# cc: error: linker command failed due to signal (use -v to see invocation)
# Aug  9 18:27:47 freebsd-vm kernel: pid 32094 (ld.lld), jid 0, uid 0: exited on signal 11 (core dumped)

. ../default.cfg
[ `id -u` -ne 0 ] && echo "Must be root!" && exit 1

dir=/tmp
odir=`pwd`
cd $dir
sed '1,/^EOF/d' < $odir/$0 > $dir/fork2.c
mycc -o fork2 -Wall -Wextra -O0 -g fork2.c || exit 1
cd $odir

set -e
mount | grep "on $mntpoint " | grep -q /dev/md && umount -f $mntpoint
[ -c /dev/md$mdstart ] &&  mdconfig -d -u $mdstart
mdconfig -a -t swap -s 2g -u $mdstart
newfs $newfs_flags md$mdstart > /dev/null
mount /dev/md$mdstart $mntpoint
set +e

cd $mntpoint
$dir/fork2
s=$?
pkill fork2
[ -f fork2.core -a $s -eq 0 ] &&
    { ls -l fork2.core; mv fork2.core $dir; s=1; }
cd $odir

for i in `jot 6`; do
	mount | grep -q "on $mntpoint " || break
	umount $mntpoint && break || sleep 10
	[ $i -eq 6 ] &&
	    { echo FATAL; fstat -mf $mntpoint; exit 1; }
done
mdconfig -d -u $mdstart
cd $dir
mycc -o $dir/fork2 -Wall -Wextra -O0 -g $dir/fork2.c; s=$?
rm -rf $dir/fork2 $dir/fork2.c
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

#define MX 5000
#define RUNTIME (1 * 60)
#define SYNC 0

int
main(void)
{
	pid_t pid;
	size_t len;
	time_t start;
	int n;

	len = PAGE_SIZE;
	if ((share = mmap(NULL, len, PROT_READ | PROT_WRITE,
	    MAP_ANON | MAP_SHARED, -1, 0)) == MAP_FAILED)
		err(1, "mmap");

	n = 0;
	signal(SIGCHLD, SIG_IGN);
	start = time(NULL);
	while ((time(NULL) - start) < RUNTIME) {
		while ((atomic_load_int(&share[SYNC])) > MX)
			usleep(100);
		n++;
		pid = fork();
		if (pid == -1)
			err(1, "fork)");
		if (pid == 0) {
			atomic_add_int(&share[SYNC], 1);
			while (atomic_load_int(&share[SYNC]) <= MX)
				usleep(10000);
			usleep(arc4random() % 1000);
			atomic_add_int(&share[SYNC], -1);
			raise(SIGHUP);
			_exit(0);
		}
	}
	atomic_add_int(&share[SYNC], MX * 2);
	fprintf(stderr, "%d fork() calls\n", n);
}
