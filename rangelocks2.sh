#!/bin/sh

#
# Copyright (c) 2025 Peter Holm <pho@FreeBSD.org>
#
# SPDX-License-Identifier: BSD-2-Clause
#

. ../default.cfg

set -u
prog=$(basename "$0" .sh)
dir=/tmp
odir=`pwd`
cd $dir
sed '1,/^EOF/d' < $odir/$0 > $dir/$prog.c
mycc -o $prog -Wall -Wextra -O2 -g $prog.c || exit 1
rm -f $prog.c
cd $odir

mdconfig -l | grep -q md$mdstart && mdconfig -d -u $mdstart
set -e
mdconfig -a -t swap -s 6g -u $mdstart
newfs $newfs_flags -n /dev/md$mdstart > /dev/null
mount /dev/md$mdstart $mntpoint
set +e
wd="$mntpoint/$prog.dir"
mkdir -p $wd
dd if=/dev/zero of=$wd/file bs=1m count=5k status=none

[ `jot -r 1 1 100` -le 25 ] &&
    ../testcases/swap/swap -t 10m -i 20 > /dev/null 2>&1 &
cd $wd
touch $wd/out
/tmp/$prog $wd/file $wd/out; s=$?
cd $odir
while pkill swap; do :; done
wait

umount $mntpoint
mdconfig -d -u $mdstart
rm -rf /tmp/$prog $wd
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
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#define DONE 1
#define MAXBLK (100 * 1024 * 1024)
#define MAXPROC 32
#define MAXSIZ (5LL * 1024 * 1024 *1024)
#define RUNTIME (5 * 60)
#define SYNC 0

static volatile u_int *share;
static int parallel;

static char *file, *file2;

static off_t
newpos(int lng)
{
	off_t p;

	do {
		arc4random_buf(&p, sizeof(p));
		p = p & 0xfffffff;
	} while (p + lng > MAXSIZ);
	return (p);
}

static void
test(int indx, int num)
{
	off_t pos, pos2;
	ssize_t i, l;
	time_t start;
	int fd, fd2, n;

	atomic_add_int(&share[SYNC], 1);
	while (share[SYNC] != (unsigned int)parallel)
		sched_yield();

	n = 0;
	start = time(NULL);
	while (share[DONE] != (unsigned int)parallel) {
		setproctitle("test(%d) num %d, n %d", indx, num, n);
		if ((fd = open(file, O_RDWR)) == -1)
			err(1, "open(%s)", file);
		if ((fd2 = open(file2, O_RDWR)) == -1)
			err(1, "open(%s)", file2);

		for (i = 0; i < arc4random() % 512; i++) {
			l = arc4random() % MAXBLK + 1;
			pos = newpos(l);
			pos2 = newpos(l);
			if (copy_file_range(fd, &pos, fd2, &pos2, l, 0) == -1)
				err(1, "copy_file_range()");
		}

		close(fd2);
		close(fd);
		if (n++ == 0)
			atomic_add_int(&share[DONE], 1);
		if (time(NULL) - start >= RUNTIME * 4) {
			fprintf(stderr, "test(%d), %d Timed out\n", indx, num);
			break;
		}
	}
	if (n++ == 0)
		atomic_add_int(&share[DONE], 1);

	_exit(0);
}

void
setup(void)
{

	parallel = arc4random() % MAXPROC + 1;
}

int
main(int argc, char *argv[])
{
	size_t len;
	time_t start;
	int e, i, n, *pids, status;

	if (argc != 3) {
		fprintf(stderr, "Usage: %s <in file> <out file>\n", argv[0]);
		_exit(1);
	}
	e = 0;
	file = argv[1];
	file2 = argv[2];
	len = PAGE_SIZE;
	if ((share = mmap(NULL, len, PROT_READ | PROT_WRITE,
	    MAP_ANON | MAP_SHARED, -1, 0)) == MAP_FAILED)
		err(1, "mmap");

	n = 0;
	start = time(NULL);
	while ((time(NULL) - start) < RUNTIME && e == 0) {
		setup();

		pids = malloc(sizeof(pid_t) * parallel);
		share[SYNC] = share[DONE] = 0;
		for (i = 0; i < parallel; i++) {
			if ((pids[i] = fork()) == 0)
				test(i, n);
		}
		for (i = 0; i < parallel; i++) {
			if (waitpid(pids[i], &status, 0) != pids[i])
				err(1, "waitpid %d", pids[i]);
			e += status == 0 ? 0 : 1;
		}
		n++;
		n = n % 10;
		free(pids);
	}

	return (e);
}
