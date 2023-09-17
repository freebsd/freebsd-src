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

# vm.stats.vm.v_laundry_count test. WiP. No problems seen.

. ../default.cfg
[ `id -u ` -ne 0 ] && echo "Must be root!" && exit 1

dir=/tmp
odir=`pwd`
cd $dir
sed '1,/^EOF/d' < $odir/$0 > $dir/laundry.c
mycc -o laundry -Wall -Wextra -O0 -g laundry.c || exit 1
rm -f laundry.c
cd $odir

size=`sysctl -n hw.usermem`
swaptotal=`sysctl -n vm.swap_total`
[ $swaptotal -eq 0 ] && exit 0
[ $size -gt $swaptotal ] && size=$swaptotal
size=$((size / 10 * 8))
set -e
mount | grep "on $mntpoint " | grep -q /dev/md && umount -f $mntpoint
[ -c /dev/md$mdstart ] &&  mdconfig -d -u $mdstart
mdconfig -a -t swap -s 1g -u $mdstart
newfs $newfs_flags md$mdstart > /dev/null
mount /dev/md$mdstart $mntpoint
set +e

cd $mntpoint
$dir/laundry $size
s=$?
[ -f laundry.core -a $s -eq 0 ] &&
    { ls -l laundry.core; s=1; }
cd $odir

for i in `jot 6`; do
	mount | grep -q "on $mntpoint " || break
	umount $mntpoint && break || sleep 10
done
[ $i -eq 6 ] && exit 1
mdconfig -d -u $mdstart
rm -rf $dir/laundry
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
size_t size;

#define PARALLEL 4
#define RUNTIME (5 * 60)
#define SYNC 0

static void
test(void)
{
	size_t i, sz;
	time_t start;
	int n;
	char *cp;

	atomic_add_int(&share[SYNC], 1);
	while (share[SYNC] != PARALLEL)
		;

	sz = size;
	if ((cp = mmap(0, sz, PROT_READ|PROT_WRITE, MAP_ANON, -1, 0)) ==
		(caddr_t) - 1)
		err(1, "mmap size %zd", sz);

	n = 0;
	start = time(NULL);
	while ((time(NULL) - start) < RUNTIME) {
		n++;
		n = n & 0xff;
		for (i = 0; i < sz; i += PAGE_SIZE)
			cp[i] = n;
		usleep(100);
	}
	munmap(cp, sz);

	_exit(0);
}

int
main(int argc __unused, char *argv[])
{
	pid_t pids[PARALLEL];
	size_t len;
	time_t start;
	int e, status;
	u_int i;

	sscanf(argv[1], "%zd", &size);
	size /= PARALLEL;
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
				if (WIFEXITED(status)) {
					printf("exited, status=%d\n",
					    WEXITSTATUS(status));
				} else if (WIFSIGNALED(status)) {
					printf("killed by signal %d\n",
					    WTERMSIG(status));
				} else if (WIFSTOPPED(status)) {
					printf("stopped by signal %d\n",
					    WSTOPSIG(status));
				} else if (WIFCONTINUED(status)) {
					printf("continued\n");
				}
				fprintf(stderr, "pid %d exit code %d\n",
						pids[i], status);
			}
			e += status == 0 ? 0 : 1;
		}
	}

	return (e);
}

