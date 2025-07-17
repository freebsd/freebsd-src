#!/bin/sh

#
# Copyright (c) 2015 EMC Corp.
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

# lockf(3) test scenario.

[ `id -u ` -ne 0 ] && echo "Must be root!" && exit 1

. ../default.cfg

odir=`pwd`
cd /tmp
sed '1,/^EOF/d' < $odir/$0 > lockf4.c
rm -f /tmp/lockf4
mycc -o lockf4 -Wall -Wextra -g -O2 lockf4.c || exit 1
rm -f lockf4.c
cd $odir

[ -c /dev/md$mdstart ] && mdconfig -d -u $mdstart
mdconfig -a -t swap -s 1g -u $mdstart || exit 1
newfs $newfs_flags md$mdstart > /dev/null
mount /dev/md$mdstart $mntpoint
chmod 777 $mntpoint

su $testuser -c "(cd $mntpoint; /tmp/lockf4)" &
su $testuser -c "(cd $mntpoint; /tmp/lockf4)" &
su $testuser -c "(cd $mntpoint; /tmp/lockf4)" &
wait

while mount | grep "on $mntpoint " | grep -q /dev/md; do
	umount $mntpoint || sleep 1
done
mdconfig -d -u $mdstart
rm -f /tmp/lockf4
exit 0
EOF
#include <fcntl.h>
#include <err.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#define N (5 * 1024 * 1024) /* 40 MB */
int fd;
int idx[N];
char file[] = "lockf4.file";

#define TIMEOUT 600

int64_t
add(int ix, int val)
{
	int64_t v;
	off_t offset;
	time_t start;
	int r;

	offset = ix * sizeof(v);
	if (lseek(fd, offset, SEEK_SET) == -1)
		err(1, "lseek");
	start = time(NULL);
	while (lockf(fd, F_LOCK, sizeof(v)) == -1) {
		if (errno != EDEADLK)
			err(1, "lockf(%s, F_LOCK)", file);
		if (time(NULL) - start > TIMEOUT)
			errx(1, "lockf timedout");
	}
	v = 0;
	r = read(fd, &v, sizeof(v));
	if (r == 0)
		v = 0;
	else
		if (r == -1)
			err(1, "read");
	v += val;

	if (lseek(fd, offset, SEEK_SET) == -1)
		err(1, "lseek");
	if (write(fd, &v, sizeof(v)) < 0)
		err(1, "write");
	if (lseek(fd, offset, SEEK_SET) == -1)
		err(1, "lseek");
	if (lockf(fd, F_ULOCK, sizeof(v)) == -1)
		err(1, "lockf(%s, F_ULOCK)", file);

	return (v);
}

int
check(void)
{
	int64_t v;
	int i;

	setproctitle("check");
	if (lseek(fd, 0, SEEK_SET) == -1)
		err(1, "lseek");
	for (i = 1; i < N; i++) {
		if (read(fd, &v, sizeof(v)) < 0)
			err(1, "read");
		if (v != 0)
			return (1);
	}

	return (0);
}

int
main(void)
{
	int64_t r;
	int e, i, j, t;
	char help[80];

	for (i = 1; i < N; i++)
		idx[i] = i;

	for (i = 1; i < N; i++) {
		j = arc4random() % (N - 1) + 1;
		t = idx[i];
		idx[i] = idx[j];
		idx[j] = t;
	}

	if ((fd = open(file, O_RDWR | O_CREAT, 0644)) == -1)
		err(1, "open(%s)", file);

	add(0, 1);
	for (i = 1; i < N; i++) {
		if (i % 500 == 0) {
			snprintf(help, sizeof(help), "add %d%%", i * 100 / N);
			setproctitle("%s", help);
		}
		add(idx[i], 1);
	}
	for (i = 1; i < N; i++) {
		if (i % 500 == 0) {
			snprintf(help, sizeof(help), "sub %d%%", i * 100 / N);
			setproctitle("%s", help);
		}
		add(idx[i], -1);
	}

	e = 0;
	if ((r = add(0, -1)) == 0) {
		e = check();
		if (e == 0)
			unlink(file);
		else
			fprintf(stderr, "FAIL\n");
	}

	close(fd);

	return (e);
}
