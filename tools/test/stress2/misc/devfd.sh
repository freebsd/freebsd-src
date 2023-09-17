#!/bin/sh

#
# Copyright (c) 2011 Peter Holm
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

# "panic: vn_lock 0xc65b5828: zero hold count" seen.

# Originally found by the iknowthis test suite
# by Tavis Ormandy <taviso  cmpxchg8b com>
# Fixed by r227952

[ `id -u ` -ne 0 ] && echo "Must be root!" && exit 1

. ../default.cfg

odir=`pwd`
cd /tmp
sed '1,/^EOF/d' < $odir/$0 > devfd.c
rm -f /tmp/devfd
mycc -o devfd -Wall -Wextra -O2 -g devfd.c -lpthread || exit 1
rm -f devfd.c

mount | grep $mntpoint | grep -q /dev/md && umount -f $mntpoint
mdconfig -l | grep -q md$mdstart &&  mdconfig -d -u $mdstart

mdconfig -a -t swap -s 1g -u $mdstart || exit 1
newfs $newfs_flags md$mdstart > /dev/null
mount /dev/md$mdstart $mntpoint
chmod 777 $mntpoint

su $testuser -c "(cd $mntpoint; /tmp/devfd)"

while mount | grep $mntpoint | grep -q /dev/md; do
	umount $mntpoint || sleep 1
done
mdconfig -d -u $mdstart
rm -f /tmp/devfd
exit
EOF
#include <err.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

int fd[3], fd2[3];

void *
thr1(void *arg __unused)
{
	int i, j;
	char path[80];

	for (i = 0; i < 100000; i++) {
		for (j = 0; j < 3; j++) {
			if (fd[j] != -1)
				close(fd[j]);
			sprintf(path, "fx%d", j);
			fd[j] = open(path, O_RDWR | O_CREAT, 0640);
		}
	}
	return (0);
}

void *
thr2(void *arg __unused)
{
	int i, j;
	char path[80];

	for (i = 0; i < 100000; i++) {
		for (j = 0; j < 3; j++) {
			if (fd2[j] != -1)
				close(fd2[j]);
			sprintf(path, "/dev/fd/%d", j);
			if ((fd2[j] = open(path, O_RDONLY)) != -1)
				fchflags(fd2[j], UF_NODUMP);
		}

	}
	return (0);
}

int
main(void)
{
	pthread_t p1, p2;
	int r;

	close(0);
	close(1);
	close(2);
	if ((r = pthread_create(&p1, NULL, thr1, NULL)) != 0)
		errc(1, r, "pthread_create");
	if ((r = pthread_create(&p2, NULL, thr2, NULL)) != 0)
		errc(1, r, "pthread_create");
	pthread_join(p1, NULL);
	pthread_join(p2, NULL);

	return (0);
}

