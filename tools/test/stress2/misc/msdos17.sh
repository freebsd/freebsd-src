#!/bin/sh

#
# Copyright (c) 2025 Peter Holm <pho@FreeBSD.org>
#
# SPDX-License-Identifier: BSD-2-Clause
#

# No problems observed

[ `id -u ` -ne 0 ] && echo "Must be root!" && exit 1

. ../default.cfg

set -u
prog=$(basename "$0" .sh)
mount | grep -q "on $mntpoint " && umount $mntpoint
[ -c /dev/md$mdstart ] && mdconfig -d -u $mdstart

mdconfig -a -t swap -s 1g -u $mdstart
gpart create -s bsd md$mdstart > /dev/null
gpart add -t freebsd-ufs md$mdstart > /dev/null
part=a
newfs_msdos -F 32 -b 8192 /dev/md${mdstart}$part > /dev/null || exit 1
mount -t msdosfs /dev/md${mdstart}$part $mntpoint

here=`pwd`
cd /tmp
cat > $prog.c <<EOF
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
#include <unistd.h>

static volatile u_int *share;
static char file1[80], file2[80];

#define SYNC 0
#define STOP 1

static void
test0(void)
{
	struct stat sb;

	while (share[STOP] == 0) {
		while (share[SYNC] != 0)
			usleep(100);
		if (rename(file1, file2) == -1)
			err(1, "rename(%s, %s)", file1, file2);
		if (stat(file1, &sb) == 0)
			err(1, "stat(%s)", file1);
		atomic_add_int(&share[SYNC], 1);
	}

	_exit(0);
}

static void
test1(void)
{
	struct stat sb;

	while (share[STOP] == 0) {
		while (share[SYNC] != 1)
			usleep(100);
		if (rename(file2, file1) == -1)
			err(1, "rename(%s, %s)", file2, file1);
		if (stat(file2, &sb) == 0)
			err(1, "stat(%s)", file2);
		atomic_add_int(&share[SYNC], -1);
	}

	_exit(0);
}

int
main(void)
{
	pid_t pids[2];
	size_t len;
	int fd;
	char cwd[80];

	len = PAGE_SIZE;
	if ((share = mmap(NULL, len, PROT_READ | PROT_WRITE,
	    MAP_ANON | MAP_SHARED, -1, 0)) == MAP_FAILED)
		err(1, "mmap");

	if (getcwd(cwd, sizeof(cwd)) == NULL)
		err(1, "getcwd()");
	snprintf(file1, sizeof(file1), "%s/a.%06d", cwd, getpid());
	snprintf(file2, sizeof(file2), "%s/b.%06d", cwd, getpid());
	if ((fd = open(file1, O_CREAT, 0640)) == -1)
		err(1, "open(%s)", file1);
	close(fd);

	if ((pids[0] = fork()) == 0)
		test0();
	if ((pids[1] = fork()) == 0)
		test1();

	sleep(120);
	share[STOP] = 1;

	if (waitpid(pids[0], NULL, 0) == -1)
		err(1, "waitpid(%d)", pids[0]);
	if (waitpid(pids[1], NULL, 0) == -1)
		err(1, "waitpid(%d)", pids[1]);
	unlink(file1);
	unlink(file2);
}
EOF
mycc -o $prog -Wall $prog.c || exit 1
rm -f $prog.c
cd $here

(cd ../testcases/swap; ./swap -t 5m -i 20 -l 100) &
cd $mntpoint
pids=""
for i in `jot 30`; do
	/tmp/$prog &
	pids="$pids $!"
done
for pid in $pids; do
	wait $pid
done
cd $here
while pkill swap; do :; done
wait

umount $mntpoint
mdconfig -d -u $mdstart
rm -f /tmp/$prog
exit 0
