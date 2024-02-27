#!/bin/sh

#
# Copyright (c) 2024 Peter Holm <pho@FreeBSD.org>
#
# SPDX-License-Identifier: BSD-2-Clause
#

# Based on code from https://syzkaller.appspot.com/text?tag=ReproC&x=15d9baada80000
# No problems seen

. ../default.cfg

prog=$(basename "$0" .sh)
odir=`pwd`
cd /tmp
sed '1,/^EOF/d' < $odir/$0 > $prog.c
mycc -o $prog -Wall -Wextra -O0 $prog.c -lpthread || exit 1
rm -f $prog.c

set -e
mount | grep "on $mntpoint " | grep -q /dev/md && umount -f $mntpoint
[ -c /dev/md$mdstart ] &&  mdconfig -d -u $mdstart
mdconfig -a -t swap -s 2g -u $mdstart
newfs $newfs_flags md$mdstart > /dev/null
mount /dev/md$mdstart $mntpoint
set +e

$odir/../testcases/swap/swap -t 2m -i 10 > /dev/null &
cd $mntpoint
/tmp/$prog
cd $odir
while pkill swap; do :; done
wait

for i in `jot 6`; do
	mount | grep -q "on $mntpoint " || break
	umount $mntpoint && break || sleep 10
	[ $i -eq 6 ] &&
	    { echo FATAL; fstat -mf $mntpoint; exit 1; }
done
mdconfig -d -u $mdstart
rm -f /tmp/$prog
exit 0

EOF
#include <sys/param.h>
#include <sys/mman.h>
#include <sys/uio.h>
#include <sys/wait.h>

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define DEBUG 0	/* 1 to enable */
#define THREADS 2

static volatile int go;
static int fd;
static char *p, path[128];

#define ADDR (void *) 0x20000000ul
#define LEN  0x1000000ul

void *
thr(void *arg)
{
	struct iovec iov;
	long n, w;
	char *p1;

	if (*(int *)arg == 0) {
		while (go == 0)
			usleep(100);
		while (go == 1) {
			if ((p1 = mmap(ADDR, LEN, PROT_WRITE, MAP_ANON|MAP_FIXED|MAP_PRIVATE, -1, 0)) == MAP_FAILED)
				err(1, "mmap() in %s", __func__);
			usleep(arc4random() % 50);
			if ((p1 = mmap(ADDR, LEN, PROT_READ|PROT_WRITE, MAP_ANON|MAP_FIXED|MAP_PRIVATE, -1, 0)) == MAP_FAILED)
				err(1, "mmap() in %s", __func__);
			usleep(arc4random() % 10000);
		}
	} else {
		while (go == 0)
			usleep(100);
		n = w = 0;
		while (go == 1) {
			iov.iov_base = p;
			iov.iov_len = 0x100000;
			if (pwritev(fd, &iov, 1, 0) != -1)
				w++;
			n++;
		}
		if (DEBUG == 1)
			fprintf(stderr, "%ld out of %ld  writes (%ld%%)\n", w, n, w * 100 / n);
	}


	return (0);
}

void
test(void)
{
	pthread_t threads[THREADS];
	int nr[THREADS];
	int i, r;

	sprintf(path, "mmap.%06d", getpid());
	if ((fd = open(path, O_CREAT | O_TRUNC | O_RDWR, 0622)) == -1)
		err(1,"open()");


	if ((p = mmap(ADDR, LEN, PROT_READ|PROT_WRITE, MAP_ANON|MAP_FIXED|MAP_PRIVATE, -1, 0)) == MAP_FAILED)
		err(1, "mmap() in %s", __func__);

	go = 0;
	for (i = 0; i < THREADS; i++) {
		nr[i] = i;
		if ((r = pthread_create(&threads[i], NULL, thr,
		    (void *)&nr[i])) != 0)
			errc(1, r, "pthread_create()");
	}

	go = 1;
	sleep(60);
	go = 2;

	for (i = 0; i < THREADS; i++) {
		if ((r = pthread_join(threads[i], NULL)) != 0)
			errc(1, r, "pthread_join(%d)", i);
	}
	close(fd);
	if (DEBUG == 0) {
		if (unlink(path) == -1)
			err(1, "unlink(%s)", path);
	}

	_exit(0);
}

int
main(void)
{
	pid_t pid;
	int i;

	for (i = 0; i < 2; i++) {
		if ((pid = fork()) == 0)
			test();
		if (waitpid(pid, NULL, 0) != pid)
			err(1, "waitpid()");
	}
}
