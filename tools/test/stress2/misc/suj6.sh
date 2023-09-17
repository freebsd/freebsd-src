#!/bin/sh

#
# Copyright (c) 2010 Peter Holm <pho@FreeBSD.org>
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

# Variation of suj5.sh
# "panic: indir_trunc: Index out of range -2 parent -2061 lbn -2060" seen

. ../default.cfg

here=`pwd`
cd /tmp
sed '1,/^EOF/d' < $here/$0 > suj6.c
mycc -o suj6 -Wall -O2 suj6.c
rm -f suj6.c
cd $here

mount | grep $mntpoint | grep -q /dev/md && umount -f $mntpoint
mdconfig -l | grep -q md$mdstart &&  mdconfig -d -u $mdstart
mdconfig -a -t swap -s 1g -u $mdstart
newfs -j md$mdstart > /dev/null
mount /dev/md$mdstart $mntpoint
chmod 777 $mntpoint

su $testuser -c "cd $mntpoint; /tmp/suj6" > /dev/null

while mount | grep $mntpoint | grep -q /dev/md; do
	umount $mntpoint || sleep 1
done
mdconfig -d -u $mdstart
rm -f /tmp/suj6
exit
EOF
#include <err.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mount.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/wait.h>

#define PARALLEL 10

static int
random_int(int mi, int ma)
{
	return (arc4random()  % (ma - mi + 1) + mi);
}

static int64_t
df(void)
{
	char path[MAXPATHLEN+1];
	struct statfs buf;

	if (getcwd(path, sizeof(path)) == NULL)
		err(1, "getcwd()");

	if (statfs(path, &buf) < 0)
		err(1, "statfs(%s)", path);
	printf("Free space on %s: %jd Mb\n", path, buf.f_bavail * buf.f_bsize / 1024 / 1024);
	return (buf.f_bavail * buf.f_bsize);
}

static void
test(int size)
{
	int buf[1024], index, to;
#ifdef TEST
	int i;
#endif
	int fd;
	char file[128];

	sprintf(file,"p%05d", getpid());
	if ((fd = creat(file, 0660)) == -1)
		err(1, "creat(%s)", file);

	to = sizeof(buf);
	index = 0;
	while (index < size) {
		if (index + to > size)
			to = size - index;
#ifdef TEST
		for (i = 0; i < to; i++)
			buf[i] = index + i;
#endif
		index += to;
		if (write(fd, buf, to) != to)
			err(1, "write(%s), %s:%d", file, __FILE__, __LINE__);
	}
	if (close(fd) == -1)
		err(1, "close(%s), %s:%d", file, __FILE__, __LINE__);

#if 0
	if ((fd = open(file, O_RDONLY)) == -1)
		err(1, "open(%s), %s:%d", file, __FILE__, __LINE__);

	index = 0;
	while (index < size) {
		if (index + to > size)
			to = size - index;
		if (read(fd, buf, to) != to)
			err(1, "rw read. %s.%d", __FILE__, __LINE__);
#ifdef TEST
		for (i = 0; i < to; i++) {
			if (buf[i] != index + i) {
				fprintf(stderr,
					"%s, pid %d: expected %d @ %d, got %d\n",
					getprogname(), getpid(), index+i, index+i,
					buf[i]);
				exit(EXIT_FAILURE);
			}
		}
#endif
		index += to;
	}
	if (close(fd) == -1)
		err(1, "close(%s), %s:%d", file, __FILE__, __LINE__);
#endif
	if (unlink(file) == -1)
		err(1, "unlink(%s), %s:%d", file, __FILE__, __LINE__);
	exit(0);
}

int
main()
{
	int i, j, pct;
	int size;	/* in k */
	int64_t bl;

	bl = df();
	if (bl > (int64_t)INT_MAX * PARALLEL)
		bl = (int64_t)INT_MAX * PARALLEL;
	size = bl / PARALLEL / 1024;

	pct = random_int(1, 50);
	size = size / 100 * pct + 1;
	if (random_int(1, 100) <= 50)
		size = 34 * 1024;	/* Known good deadlock value */
	printf("Max file size: %d Mb\n", size / 1024);

	for (i = 0; i < 100; i++) {
		for (j = 0; j < PARALLEL; j++) {
			if (fork() == 0) {
				test(random_int(1, size) * 1024);
			}
		}
		for (j = 0; j < PARALLEL; j++)
			wait(NULL);
	}

	return (0);
}
