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

# fcntl(2) locking scenario, using UFS and a nullfs mount.
# No problems seen.

[ `id -u ` -ne 0 ] && echo "Must be root!" && exit 1

. ../default.cfg

here=`pwd`
cd /tmp
sed '1,/^EOF/d' < $here/$0 > nullfs22.c
mycc -o nullfs22 -Wall -Wextra -O0 -g nullfs22.c || exit 1
rm -f nullfs22.c

mp2=${mntpoint}2
[ -d $mp2 ] || mkdir -p $mp2
mount | grep -q "on $mp2 " && umount $mp2
mount | grep "on $mntpoint " | grep -q /dev/md && umount -f $mntpoint
[ -c /dev/md$mdstart ] &&  mdconfig -d -u $mdstart
mdconfig -a -t swap -s 512m -u $mdstart || exit 1
bsdlabel -w md$mdstart auto
newfs -n md${mdstart}$part > /dev/null
mount /dev/md${mdstart}$part $mntpoint

mount -t nullfs $mntpoint $mp2

/tmp/nullfs22 $mntpoint $mp2
status=$?

while mount | grep -q "on $mp2 "; do
	umount $mp2
done
while mount | grep "on $mntpoint " | grep -q /dev/md; do
	umount $mntpoint || sleep 1
done
mdconfig -d -u $mdstart
rm -f /tmp/nullfs22
exit $status
EOF
#include <sys/param.h>
#include <sys/mman.h>
#include <sys/wait.h>

#include <machine/atomic.h>

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define LOOPS 1024
#define N (512)
#define PARALLEL 4

#define DONE 1
#define SYNC 0

int fd;
volatile u_int *share;
char name1[80], name2[80];

static void
ahandler(int s __unused)
{
	fprintf(stderr, "In alarm handler\n");
	unlink(name1);
	_exit(1);
}

void
add(int n, int increment)
{
        struct flock fl;
	off_t pos;
	long val, oval __unused;
	int r;

	pos = n * sizeof(val);
	memset(&fl, 0, sizeof(fl));
        fl.l_start = pos;
        fl.l_len = sizeof(val);
        fl.l_type = F_WRLCK;
        fl.l_whence = SEEK_SET;

	while (fcntl(fd, F_SETLKW, &fl) < 0) {
		if (errno != EAGAIN)
			err(1, "F_SETLKW (child)");
		usleep(100);
	}

	if (lseek(fd, pos, SEEK_SET) == -1)
		err(1, "lseek");
	oval = 999999;
	while ((r = read(fd, &val, sizeof(val)) != sizeof(val))) {
		if (r == -1 && errno != EAGAIN)
			err(1, "read");
		if (lseek(fd, pos, SEEK_SET) == -1)
			err(1, "lseek");
	}
	oval = val;
	val = val + increment;
#if defined(DEBUG)
	fprintf(stderr, "add(%d, %d) @ pos %ld: %ld = %ld + %d\n",
	    n, increment, (long)pos, val, oval, increment);
#endif
	if (lseek(fd, pos, SEEK_SET) == -1)
		err(1, "lseek");
	while ((r = write(fd, &val, sizeof(val)) != sizeof(val))) {
		if (r == -1 && errno != EAGAIN)
			err(1, "write");
		if (lseek(fd, pos, SEEK_SET) == -1)
			err(1, "lseek");
	}

        fl.l_type = F_UNLCK;
        if (fcntl(fd, F_SETLK, &fl) < 0)
                err(1, "F_UNLCK");

}

void
count(int val)
{
	int i, j;
	char help[80], *name;

	if (val == 1)
		name = name1;
	else
		name = name2;
	snprintf(help, sizeof(help), "%s %d %s", __func__, val, name);
	setproctitle("%s", help);
	atomic_add_int(&share[SYNC], 1);
	while (share[SYNC] != 2 * PARALLEL)
		;

	/* Need to re-open after a fork() */
	close(fd);
	if ((fd = open(name, O_RDWR)) == -1)
		err(1, "open(%s)", name);

	for (i = 0; i < LOOPS; i++) {
		for (j = 0; j < N; j++)
			add(j, val);
	}

	atomic_add_int(&share[DONE], 1);

	_exit(0);
}

int
main(int argc, char *argv[])
{
	off_t len;
	size_t mlen;
	long val, sum;
	int i, s, stat;

	if (argc != 3) {
		fprintf(stderr, "Usage: %s <dir1> <dir2>\n", argv[0]);
		exit(1);
	}

	mlen = PAGE_SIZE;
	if ((share = mmap(NULL, mlen, PROT_READ | PROT_WRITE,
	    MAP_ANON | MAP_SHARED, -1, 0)) == MAP_FAILED)
		err(1, "mmap");
	snprintf(name1, sizeof(name1), "%s/work", argv[1]);
	snprintf(name2, sizeof(name2), "%s/work", argv[2]);
	signal(SIGALRM, ahandler);
	alarm(300);
	if ((fd = open(name1, O_RDWR | O_CREAT | O_TRUNC, 0640)) == -1)
		err(1, "open(%s)", name1);
	len = N * sizeof(val);
	if (ftruncate(fd, len) == -1)
		err(1, "ftruncate");

	for (i = 0; i < PARALLEL; i++) {
		if (fork() == 0)
			count(1);
	}
	for (i = 0; i < PARALLEL; i++) {
		if (fork() == 0)
			count(-1);
	}

	while (share[DONE] != 2 * PARALLEL)
		usleep(10000);

	if (lseek(fd, 0, SEEK_SET) == -1)
		err(1, "lseek");
	sum = 0;
	for (i = 0; i < N; i++) {
		if (read(fd, &val, sizeof(val)) != sizeof(val))
			err(1, "Final read");
		if (val != 0)
			fprintf(stderr, "index %d: %ld\n", i, val);
		sum += val;
	}
	if (sum != 0)
		fprintf(stderr, "FAIL\n");
	unlink(name1);

	s = 0;
	for (i = 0; i < PARALLEL; i++) {
		wait(&stat);
		s += WEXITSTATUS(stat);
		wait(&stat);
		s += WEXITSTATUS(stat);
	}

	close(fd);

	return (sum != 0 || s != 0);
}
