#!/bin/sh

#
# Copyright (c) 2025 Peter Holm <pho@FreeBSD.org>
#
# SPDX-License-Identifier: BSD-2-Clause
#

# A kqueuex(KQUEUE_CPONFORK) test scenario

# Sleeping thread seen in WiP code:
# https://people.freebsd.org/~pho/stress/log/log0615.txt

. ../default.cfg

[ `id -u ` -ne 0 ] && echo "Must be root!" && exit 1
ulimit -k 5000 || { echo FAIL; exit 1; }

odir=`pwd`
prog=$(basename "$0" .sh)

cd /tmp
sed '1,/^EOF/d' < $odir/$0 > $prog.c
mycc -o $prog -Wall -Wextra -O2 -g $prog.c -lpthread || exit 1
rm -f $prog.c
cd $odir

mount | grep "on $mntpoint " | grep -q md$mdstart && umount -f $mntpoint
mdconfig -l | grep -q $mdstart && mdconfig -d -u $mdstart

mdconfig -a -t swap -s 2g -u $mdstart
newfs $newfs_flags md$mdstart > /dev/null
mount /dev/md$mdstart $mntpoint
chmod 777 $mntpoint

su $testuser -c "(cd $mntpoint; /tmp/$prog)" &
for i in `jot 99`; do
	sleep 1
	kill -0 $! 2>/dev/null || break
done
pkill $prog
wait
umount -f $mntpoint

while mount | grep -q $mntpoint; do
	umount $mntpoint || sleep 1
done
mdconfig -d -u $mdstart
rm -f /tmp/$prog

exit 0
EOF
#include <sys/types.h>
#include <sys/event.h>
#include <sys/mman.h>
#include <sys/wait.h>

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <sched.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define PARALLEL 64

static int fd;
static char path[80];

static void *
spin(void *arg __unused)
{
	int i;

	for (i= 0;; i++) {
		snprintf(path, sizeof(path), "file.%06d.%d", getpid(), i);
		fd = open(path, O_CREAT | O_TRUNC | O_RDWR, 0622);
		if (fd == -1 && errno == ENOTDIR)
			break;
		if (fd == -1)
			err(1, "open(%s)", path);
		close(fd);
		fd = 0;
		unlink(path);
	}
	fprintf(stderr, "spin loops: %d\n", i + 1);
	return (NULL);
}

static void *
test(void *arg __unused)
{
	struct kevent ev;
	struct timespec ts;
	pid_t pid;
	int i, kq, n;

	for (i = 0; i < 500000; i++) {
		if ((kq = kqueuex(KQUEUE_CPONFORK)) < 0)
			err(1, "kqueueex(KQUEUE_CPONFORK)");

		n = 0;
		memset(&ev, 0, sizeof(ev));
		EV_SET(&ev, fd, EVFILT_VNODE, EV_ADD | EV_ENABLE | EV_CLEAR,
			NOTE_DELETE|NOTE_RENAME|NOTE_EXTEND, 0, 0);
		n++;

		if ((pid = fork()) == 0) {
			kevent(kq, &ev, n, NULL, 0, NULL);
			_exit(0);
		}
		if (waitpid(pid, NULL, 0) != pid)
			err(1, "waitpid(%d)", pid);

		kevent(kq, &ev, n, NULL, 0, NULL);
		memset(&ev, 0, sizeof(ev));
		ts.tv_sec  = 0;
		ts.tv_nsec = 1000000;
		if ((n = kevent(kq, NULL, 0, &ev, 1, &ts)) == -1)
			err(1, "kevent()");

		close(kq);
	}
	return (NULL);
}

int
main(void)
{
	pthread_t cp[PARALLEL], sp;
	int e, i;

	if ((e = pthread_create(&sp, NULL, spin, NULL)) != 0)
		errc(1, e, "pthread_create");

	for (i = 0; i < PARALLEL; i++) {
		if ((e = pthread_create(&cp[i], NULL, test, NULL)) != 0)
			errc(1, e, "pthread_create");
	}

	for (i = 0; i < PARALLEL; i++)
		pthread_join(cp[i], NULL);
	pthread_join(sp, NULL);

	close(fd);

	return (0);
}
