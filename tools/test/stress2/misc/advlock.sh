#!/bin/sh

#
# Copyright (c) 2016 EMC Corp.
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

# From r238952's commit log:
# The first change closes a race where an open() that will block with O_SHLOCK
# or O_EXLOCK can increase the write count while it waits.  If the process
# holding the current lock on the file then tries to call exec() on the file
# it has locked, it can fail with ETXTBUSY even though the advisory lock is
# preventing other threads from successfully completing a writable open().
#
# The second change closes a race where a read-only open() with O_SHLOCK or
# O_EXLOCK may return successfully while the write count is non-zero due to
# another descriptor that had the advisory lock and was blocking the open()
# still being in the process of closing.  If the process that completed the
# open() then attempts to call exec() on the file it locked, it can fail with
# ETXTBUSY even though the other process that held a write lock has closed
# the file and released the lock.

# https://people.freebsd.org/~pho/stress/log/kostik859.txt
# https://people.freebsd.org/~pho/stress/log/kostik860.txt

# Fixed by r294204.

. ../default.cfg

[ `id -u ` -ne 0 ] && echo "Must be root!" && exit 1

dir=/tmp
odir=`pwd`
cd $dir
sed '1,/^EOF/d' < $odir/$0 > $dir/advlock.c
mycc -o advlock -Wall -Wextra -O0 -g advlock.c || exit 1
rm -f advlock.c

mount | grep "on $mntpoint " | grep -q /dev/md && umount -f $mntpoint
mdconfig -l | grep -q md$mdstart &&  mdconfig -d -u $mdstart
mdconfig -a -t swap -s 512m -u $mdstart || exit 1
newfs $newfs_flags md$mdstart > /dev/null
mount /dev/md$mdstart $mntpoint

cp /usr/bin/true $mntpoint
cd $mntpoint
/tmp/advlock
r=$?
cd $odir

while mount | grep "on $mntpoint " | grep -q /dev/md; do
	umount $mntpoint || sleep 1
done
mdconfig -d -u $mdstart
rm -f /tmp/advlock
exit $r

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

volatile u_int *share;
char *cmdline[] = { "./true", NULL };
const char *tp;

#define SYNC 0
#define PARALLEL 2

#define RUNTIME (1 * 60)

void
handler(int i __unused) {

        fprintf(stderr, "ALARM from %s.\n", tp);
	_exit(1);
}

void
slock(void)
{
	int fd;

	setproctitle("%s", __func__);
	atomic_add_int(&share[SYNC], 1);
	while (share[SYNC] != PARALLEL)
		;

	tp = __func__;
	alarm(2);
	if ((fd = open(cmdline[0], O_RDONLY | O_SHLOCK)) == -1)
		err(1, "open(%s). %d", cmdline[0], __LINE__);
	usleep(500);
	close(fd);

	_exit(0);
}

void
elock(void)
{
	int fd;

	setproctitle("%s", __func__);
	atomic_add_int(&share[SYNC], 1);
	while (share[SYNC] != PARALLEL)
		;

	tp = __func__;
	alarm(2);
	if ((fd = open(cmdline[0], O_WRONLY | O_EXLOCK)) == -1) {
		if (errno != ETXTBSY)
			err(1, "open(%s). %d", cmdline[0], __LINE__);
	} else {
		usleep(500);
		close(fd);
	}

	_exit(0);
}

void
stest(void)
{
	int fd;

	setproctitle("%s", __func__);
	atomic_add_int(&share[SYNC], 1);
	while (share[SYNC] != PARALLEL)
		;

	tp = __func__;
	alarm(2);
	if ((fd = open(cmdline[0], O_RDONLY | O_SHLOCK)) == -1)
		err(1, "open(%s). %d", cmdline[0], __LINE__);

	if (execve(cmdline[0], cmdline, NULL) == -1)
		err(1, "execve(%s) @ %d", cmdline[0], __LINE__);

	_exit(0);
}

void
etest(void)
{
	int fd;

	setproctitle("%s", __func__);
	atomic_add_int(&share[SYNC], 1);
	while (share[SYNC] != PARALLEL)
		;

	tp = __func__;
	alarm(2);
	if ((fd = open(cmdline[0], O_RDONLY | O_EXLOCK)) == -1)
		err(1, "open(%s). %d", cmdline[0], __LINE__);

	if (execve(cmdline[0], cmdline, NULL) == -1)
		err(1, "execve(%s) @ %d", cmdline[0], __LINE__);

	_exit(0);
}

int
main(void)
{
	size_t len;
	time_t start;
	int i, n, r, s;

	len = PAGE_SIZE;
	if ((share = mmap(NULL, len, PROT_READ | PROT_WRITE, MAP_ANON | MAP_SHARED,
	    -1, 0)) == MAP_FAILED)
		err(1, "mmap");

	signal(SIGALRM, handler);
	n = r = 0;
	start = time(NULL);
	while ((time(NULL) - start) < RUNTIME) {
		n++;
		share[SYNC] = 0;
		if (fork() == 0)
			slock();
		if (fork() == 0)
			stest();

		for (i = 0; i < PARALLEL; i++) {
			wait(&s);
			r += s == 0 ? 0 : 1;
		}
		if (r != 0)
			break;

		share[SYNC] = 0;
		if (fork() == 0)
			elock();
		if (fork() == 0)
			etest();

		for (i = 0; i < PARALLEL; i++) {
			wait(&s);
			r += s == 0 ? 0 : 1;
		}
		if (r != 0)
			break;
	}
	if (r != 0)
		fprintf(stderr, "FAIL @ %d\n", n);

	return (r);
}
