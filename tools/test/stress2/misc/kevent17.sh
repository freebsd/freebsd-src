#!/bin/sh

#
# Copyright (c) 2025 Peter Holm <pho@FreeBSD.org>
#
# SPDX-License-Identifier: BSD-2-Clause
#

# A kqueuex(KQUEUE_CPONFORK) test scenario
# Test scenario suggestion by: kib

. ../default.cfg
[ `id -u ` -ne 0 ] && echo "Must be root!" && exit 1

dir=/tmp
odir=`pwd`
prog=$(basename "$0" .sh)
cd $dir
sed '1,/^EOF/d' < $odir/$0 > $dir/$prog.c
mycc -o $prog -Wall -Wextra -O0 -g $prog.c || exit 1
rm -f $prog.c
cd $odir

set -e
mount | grep "on $mntpoint " | grep -q /dev/md && umount -f $mntpoint
[ -c /dev/md$mdstart ] &&  mdconfig -d -u $mdstart
mdconfig -a -t swap -s 2g -u $mdstart
newfs $newfs_flags md$mdstart > /dev/null
mount /dev/md$mdstart $mntpoint
set +e

(cd $odir/../testcases/swap; ./swap -t 5m -i 20 -h -l 100 > /dev/null) &
cd $mntpoint
$dir/$prog
s=$?
[ -f $prog.core -a $s -eq 0 ] &&
    { ls -l $prog.core; mv $prog.core /tmp; s=1; }
cd $odir
while pkill -9 swap; do :; done
wait

for i in `jot 6`; do
	mount | grep -q "on $mntpoint " || break
	umount $mntpoint && break || sleep 10
done
[ $i -eq 6 ] && exit 1
mdconfig -d -u $mdstart
rm -rf $dir/$prog
exit $s

EOF
#include <sys/param.h>
#include <sys/event.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/wait.h>

#include <machine/atomic.h>

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

static volatile u_int *share;
static int  loops;
static char *file = "file";

#define MAXLOOPS 100
#define PARALLEL 1
#define RUNTIME (2 * 60)
#define SYNC 0

static void
test(void)
{
	pid_t pid;
        struct kevent ev[2];
        struct timespec ts;
        int kq, fd, n;

	if ((fd = open(file, O_RDONLY, 0)) == -1)
		err(1, "open(%s). %s:%d", file, __func__, __LINE__);

	atomic_add_int(&share[SYNC], 1);
	while (share[SYNC] != PARALLEL)
		usleep(10000);

	if ((kq = kqueuex(KQUEUE_CPONFORK)) < 0)
		err(1, "kqueuex");

	ts.tv_sec  = 5;
	ts.tv_nsec = 0;
	n = 0;
	EV_SET(&ev[n], fd, EVFILT_VNODE, EV_ADD | EV_ENABLE | EV_CLEAR,
		NOTE_DELETE, 0, 0);
	n++;

	if (kevent(kq, ev, n, NULL, 0, NULL) < 0)
		err(1, "kevent()");
	if (loops >= MAXLOOPS) {	/* start using fork(2) */
		if ((pid = fork()) == 0) {
			n = kevent(kq, NULL, 0, ev, 1, &ts);
			if (n == -1)
				err(1, "kevent() in fork\n");
			close(fd);
			close(kq);
			_exit(0);
		}
		if (waitpid(pid, NULL, 0) != pid)
			err(1, "waitpid(%d)\n", pid);
	}

	n = kevent(kq, NULL, 0, ev, 1, &ts);
	if (n == -1)
		err(1, "kevent()");
	close(fd);
	close(kq);

	_exit(0);
}

int
main(void)
{
	pid_t pids[PARALLEL];
	size_t len;
	time_t start;
	int e, fd, i, status;

	e = 0;
	len = PAGE_SIZE;
	loops = 0;
	if ((share = mmap(NULL, len, PROT_READ | PROT_WRITE,
	    MAP_ANON | MAP_SHARED, -1, 0)) == MAP_FAILED)
		err(1, "mmap");

	start = time(NULL);
	while ((time(NULL) - start) < RUNTIME && e == 0) {
		loops++;
		if ((fd = open(file, O_CREAT | O_TRUNC | O_RDWR, 0660)) ==
		    -1)
			err(1, "open(%s)", file);
		close(fd);
		share[SYNC] = 0;
		for (i = 0; i < PARALLEL; i++) {

			if ((pids[i] = fork()) == 0)
				test();
			if (pids[i] == -1)
				err(1, "fork()");
		}
		while (share[SYNC] != PARALLEL)
			usleep(10000);
		if (unlink(file) == -1)
			err(1, "unlink(%s). %s:%d\n", file,
				    __FILE__, __LINE__);
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
