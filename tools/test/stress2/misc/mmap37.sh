#!/bin/sh

#
# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) 2019 Dell EMC Isilon
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

# Regression test for https://reviews.freebsd.org/D20800
# "Use a consistent snapshot of the fd's rights in fget_mmap()"
# https://people.freebsd.org/~pho/stress/log/mmap37.txt

# Reported by syzbot+ae359438769fda1840f8@syzkaller.appspotmail.com
# Test scenario suggestion by markj@
# Fixed by r349547

. ../default.cfg

odir=`pwd`
cd /tmp
sed '1,/^EOF/d' < $odir/$0 > mmap37.c
mycc -o mmap37 -Wall -Wextra -O0 mmap37.c -lpthread || exit 1
rm -f mmap37.c

set -e
mount | grep "on $mntpoint " | grep -q /dev/md && umount -f $mntpoint
[ -c /dev/md$mdstart ] &&  mdconfig -d -u $mdstart
mdconfig -a -t swap -s 2g -u $mdstart
newfs $newfs_flags md$mdstart > /dev/null
mount /dev/md$mdstart $mntpoint
set +e

cd $mntpoint
/tmp/mmap37
cd $odir

for i in `jot 6`; do
	mount | grep -q "on $mntpoint " || break
	umount $mntpoint && break || sleep 10
	[ $i -eq 6 ] &&
	    { echo FATAL; fstat -mf $mntpoint; exit 1; }
done
mdconfig -d -u $mdstart
rm -f /tmp/mmap37
exit

EOF
#include <sys/types.h>
#include <sys/mman.h>
#include <sys/param.h>

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <unistd.h>

#define THREADS 2
#define SIZ  0x06400000U /* 100 Mb */

static volatile int go;
static char path[128];

void *
thr(void *arg)
{
	size_t len;
	void *p;
	int fd;

	fd = 0;
	len = SIZ;
	if (*(int *)arg == 0) {
		while (go == 1) {
			if ((fd = open(path, 2)) == -1)
				err(1,"open()");
			p = mmap(NULL, len, PROT_READ | PROT_WRITE,
			    MAP_SHARED, fd, 0);
			munmap(p, len);
			close(fd);
			usleep(10);
		}
	} else {
		while (go == 1) {
			close(fd);
			usleep(10);
		}
	}

	return (0);
}

int
main(void)
{
	pthread_t threads[THREADS];
	size_t len;
	int nr[THREADS];
	int i, fd, r;

	sprintf(path, "mmap37.%06d", getpid());
	if ((fd = open(path, O_CREAT | O_TRUNC | O_RDWR, 0622)) == -1)
		err(1,"open()");
	len = SIZ;
	if (ftruncate(fd, len) == -1)
		err(1, "ftruncate");
	close(fd);

	go = 1;
	for (i = 0; i < THREADS; i++) {
		nr[i] = i;
		if ((r = pthread_create(&threads[i], NULL, thr,
		    (void *)&nr[i])) != 0)
			errc(1, r, "pthread_create()");
	}

	sleep(30);
	go = 0;

	for (i = 0; i < THREADS; i++) {
		if ((r = pthread_join(threads[i], NULL)) != 0)
			errc(1, r, "pthread_join(%d)", i);
	}
	if (unlink(path) == -1)
		err(1, "unlink(%s)", path);

	return (0);
}
