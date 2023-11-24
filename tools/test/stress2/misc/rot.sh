#!/bin/sh

#
# Copyright (c) 2012 Peter Holm <pho@FreeBSD.org>
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

[ `id -u ` -ne 0 ] && echo "Must be root!" && exit 1

# Unmapped I/O test scenario:
# http://people.freebsd.org/~pho/stress/log/kostik515.txt

. ../default.cfg

here=`pwd`
cd /tmp
sed '1,/^EOF/d' < $here/$0 > rot.c
mycc -o rot -Wall -Wextra -O2 -g rot.c || exit 1
rm -f rot.c
cd $here

mount | grep $mntpoint | grep -q /dev/md && umount -f $mntpoint
mdconfig -l | grep -q md$mdstart &&  mdconfig -d -u $mdstart

mdconfig -a -t swap -s 2g -u $mdstart || exit 1

newfs $newfs_flags md$mdstart > /dev/null

mount /dev/md$mdstart $mntpoint
chmod 777 $mntpoint

(cd $mntpoint; /tmp/rot)
(cd `dirname $diskimage`; /tmp/rot)

while mount | grep $mntpoint | grep -q /dev/md; do
	umount $mntpoint || sleep 1
done
mdconfig -d -u $mdstart
rm -f /tmp/rot
exit 0
EOF
#include <err.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#define N 10240	/* 40 Mb */
#define PARALLEL 20
#define RUNTIME (60 * 15)

int
test(void)
{
	int fd, i, j, s;
	unsigned char *buf;
	char path[128];

	s = getpagesize();

	sprintf(path,"%s.%05d", getprogname(), getpid());
	if ((fd = open(path, O_RDWR | O_CREAT | O_TRUNC, 0600)) < 0)
		err(1, "open(%s)", path);
	if (lseek(fd, s * N - 1, SEEK_SET) == -1)
                        err(1, "lseek error");

	/* write a dummy byte at the last location */
	if (write(fd, "", 1) != 1)
		err(1, "write error");

	for (i = 0; i < N; i++) {
		if ((buf = mmap(0, s, PROT_READ | PROT_WRITE, MAP_SHARED, fd,
		    i * s)) == MAP_FAILED)
			err(1, "write map");
		for (j = 0; j < s; j++)
			buf[j] = i % 256;
	}
	if (munmap(buf, s) == -1)
		err(1, "unmap (write)");
	close(fd);

	if ((fd = open(path, O_RDONLY)) < 0)
		err(1, "open(%s)", path);
	for (i = 0; i < N; i++) {
		if ((buf = mmap(0, s, PROT_READ, MAP_SHARED, fd, i * s)) ==
		    MAP_FAILED)
			err(1, "write map");
		for (j = 0; j < s; j++)
			if (buf[j] != i % 256)
				fprintf(stderr, "read %d, expected %d at %d\n",
						buf[j], i % 256, i);
	}
	if (munmap(buf, s) == -1)
		err(1, "unmap (read)");
	close(fd);
	unlink(path);

	_exit(0);
}

int
main(void)
{
	time_t start;
	int i;

	start = time(NULL);
	while (time(NULL) - start < RUNTIME) {
		for (i = 0; i < PARALLEL; i++)
			if (fork() == 0)
				test();
		for (i = 0; i < PARALLEL; i++)
			wait(NULL);
	}

	return(0);
}
