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

# mknod(2) regression test
# "panic: ffs_write: type 0xca2b02d0 8 (0,3)" seen.
# Reported by: Dmitry Vyukov <dvyukov@google.com>
# Fixed by r324853

. ../default.cfg
[ `id -u ` -ne 0 ] && echo "Must be root!" && exit 1

dir=/tmp
odir=`pwd`
cd $dir
sed '1,/^EOF/d' < $odir/$0 > $dir/mknod.c
mycc -o mknod -Wall -Wextra -O0 -g mknod.c || exit 1
rm -f mknod.c
cd $odir

set -e
mount | grep "on $mntpoint " | grep -q /dev/md && umount -f $mntpoint
[ -c /dev/md$mdstart ] &&  mdconfig -d -u $mdstart
mdconfig -a -t swap -s 2g -u $mdstart
newfs $newfs_flags md$mdstart > /dev/null
mount /dev/md$mdstart $mntpoint
set +e

cd $mntpoint
$dir/mknod $mntpoint
s=$?
[ -f mknod.core -a $s -eq 0 ] &&
    { ls -l mknod.core; mv mknod.core /tmp; s=1; }
cd $odir

for i in `jot 6`; do
	mount | grep -q "on $mntpoint " || break
	umount $mntpoint && break || sleep 10
done
[ $i -eq 6 ] && exit 1
mdconfig -d -u $mdstart
rm -rf $dir/mknod
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
static char *mp;

#define PARALLEL 4
#define RUNTIME (1 * 60)
#define SYNC 0

static void
test(void)
{
	dev_t dev;
	mode_t mode;
	time_t start;
	int fd, n, r;
	char path[128];

	atomic_add_int(&share[SYNC], 1);
	while (share[SYNC] != PARALLEL)
		;
	n = 0;
	snprintf(path, sizeof(path), "%s/node.%06d.%d", mp, getpid(), n);
	start = time(NULL);
	while ((time(NULL) - start) < RUNTIME) {
		dev = makedev(arc4random(), arc4random());
		mode = arc4random() % 0x10000;
		r = mknod(path, mode, dev);
		if (r == 0) {
			if ((fd = open(path, O_RDWR)) != -1) {
				write(fd, "x", 1);
				close(fd);
			}
			unlink(path);
			n++;
			snprintf(path, sizeof(path), "%s/node.%06d.%d", mp,
			    getpid(), n);
		}
	}

	_exit(0);
}

int
main(int argc, char *argv[])
{
	pid_t pids[PARALLEL];
	size_t len;
	int e, i, status;

	if (argc != 2)
		return (1);
	mp = argv[1];
	e = 0;
	len = PAGE_SIZE;
	if ((share = mmap(NULL, len, PROT_READ | PROT_WRITE,
	    MAP_ANON | MAP_SHARED, -1, 0)) == MAP_FAILED)
		err(1, "mmap");

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
			if (WIFSIGNALED(status))
				fprintf(stderr,
				    "pid %d exit signal %d\n",
				    pids[i], WTERMSIG(status));
		}
		e += status == 0 ? 0 : 1;
	}

	return (e);
}
