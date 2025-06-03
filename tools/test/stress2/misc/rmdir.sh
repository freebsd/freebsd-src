#!/bin/sh

#
# Copyright (c) 2025 Peter Holm <pho@FreeBSD.org>
#
# SPDX-License-Identifier: BSD-2-Clause
#

# Based on msdos11.sh

. ../default.cfg

set -u
prog=$(basename "$0" .sh)
log=/tmp/$prog.log

cat > /tmp/$prog.c <<EOF
#include <sys/param.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/wait.h>


#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

static _Atomic(int) *share;

#define PARALLEL 3
#define RUNTIME (2 * 60)
#define SYNC 0

static void
test(void)
{
	time_t start;

	atomic_fetch_add(&share[SYNC], 1);
	while (share[SYNC] != PARALLEL)
		;
	start = time(NULL);
	while ((time(NULL) - start) < 30) {
		mkdir("a", 0755);
		rename("a", "b");
		rmdir("b");
	}

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
				if (WIFSIGNALED(status))
					fprintf(stderr,
					    "pid %d exit signal %d\n",
					    pids[i], WTERMSIG(status));
			}
			e += status == 0 ? 0 : 1;
		}
	}

	return (e);
}
EOF
mycc -o /tmp/$prog -Wall -Wextra -O0 /tmp/$prog.c || exit 1

set -e
mount | grep "on $mntpoint " | grep -q /dev/md && umount -f $mntpoint
[ -c /dev/md$mdstart ] &&  mdconfig -d -u $mdstart
mdconfig -a -t swap -s 512m -u $mdstart
newfs $newfs_flags md$mdstart > /dev/null
mount /dev/md$mdstart $mntpoint
set +e

here=`pwd`
(cd $here/../testcases/swap; ./swap -t 2m -i 20 -l 100 > /dev/null) &
sleep .5

cd $mntpoint
/tmp/$prog; s=$?
cd -
wait
umount $mntpoint
fsck_ffs /dev/md$mdstart > $log 2>&1
grep -Eq "WAS MODIFIED" $log && { cat $log; s=32; }
mdconfig -d -u $mdstart
rm -f /tmp/$prog /tmp/$prog.c $log
exit $s
