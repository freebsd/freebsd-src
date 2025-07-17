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

# Bug 236977 - [msdosfs] returns cached truncated data
# Fixed by r345847

. ../default.cfg
[ `id -u` -ne 0 ] && echo "Must be root!" && exit 1

dir=/tmp
odir=`pwd`
cd $dir
sed '1,/^EOF/d' < $odir/$0 > $dir/truncate8.c
mycc -o truncate8 -Wall -Wextra -O0 -g truncate8.c || exit 1
rm -f truncate8.c
cd $odir

echo ufs:
mount | grep -q "$mntpoint " && umount $mntpoint
mdconfig -l | grep -q $mdstart && mdconfig -d -u $mdstart
mdconfig -a -t swap -s 1g -u $mdstart
gpart create -s bsd md$mdstart > /dev/null
gpart add -t freebsd-ufs md$mdstart > /dev/null
part=a
newfs $newfs_flags md${mdstart}$part > /dev/null
mount /dev/md${mdstart}$part $mntpoint

(cd $mntpoint; /tmp/truncate8)
s=$?
[ $s -ne 0 ] && { echo "UFS exit status is $s"; status=1; }
while mount | grep -q "$mntpoint "; do
	umount $mntpoint || sleep 1
done
mdconfig -d -u $mdstart

echo nfs:
if ping -c 2 `echo $nfs_export | sed 's/:.*//'` > /dev/null 2>&1; then
	mount -t nfs -o tcp -o retrycnt=3 -o intr,soft -o rw $nfs_export \
	    $mntpoint
	sleep .2
	(cd $mntpoint; /tmp/truncate8)
	s=$?
	[ $s -ne 0 ] && { echo "NFS exit status is $s"; status=1; }

	umount $mntpoint || umount $mntpoint
fi

echo msdos:
if [ -x /sbin/mount_msdosfs ]; then
	mdconfig -a -t swap -s 1g -u $mdstart
	gpart create -s bsd md$mdstart > /dev/null
	gpart add -t freebsd-ufs md$mdstart > /dev/null
	part=a
	newfs_msdos -F 16 -b 8192 /dev/md${mdstart}$part > /dev/null 2>&1
	mount_msdosfs -m 777 /dev/md${mdstart}$part $mntpoint

	(cd /mnt; /tmp/truncate8)
	s=$?
	[ $s -ne 0 ] && { echo "MSDOS exit status is $s"; status=1; }
	while mount | grep -q "$mntpoint "; do
		umount $mntpoint || sleep 1
	done
	mdconfig -d -u $mdstart
fi

echo tmpfs:
mount -t tmpfs null $mntpoint
chmod 777 $mntpoint

(cd $mntpoint; /tmp/truncate8)
s=$?
[ $s -ne 0 ] && { echo "TMPFS exit status is $s"; status=1; }
while mount | grep -q "$mntpoint "; do
	umount $mntpoint || sleep 1
done

rm -rf /tmp/truncate8
exit $status

EOF
#include <sys/param.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/wait.h>

#include <machine/atomic.h>

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

static volatile u_int *share;

#define BSIZE 5120
#define FSIZE (128 * 1024 * 1024)
#define PARALLEL 2
#define RUNTIME (1 * 60)
#define SYNC 0

static void
test(void)
{
	time_t start;
	off_t pos;
	int fd, i, n, r;
	char *buf, name[128];

	atomic_add_int(&share[SYNC], 1);
	while (share[SYNC] != PARALLEL)
		;

	srand48(getpid());
	buf = malloc(BSIZE);
	for (i = 0; i < (int)sizeof(buf); i++)
		buf[i] = 123;

	sprintf(name, "f%05d", getpid());
	if ((fd = open(name, O_RDWR | O_CREAT, 0640)) == -1)
		err(1, "%s", name);
	for (i = 0; i < FSIZE / BSIZE; i++)
		if (write(fd, buf, BSIZE) != BSIZE)
			err(1, "write");

	start = time(NULL);
	while (time(NULL) - start < RUNTIME) {
		pos = lrand48() % (FSIZE - BSIZE);
//		fprintf(stderr, "truncate(%jd)\n", (intmax_t)pos);
		if (ftruncate(fd, pos) == -1)
			err(1, "ftruncate");

		if (ftruncate(fd, FSIZE) == -1)
			err(1, "ftruncate");
		if (lseek(fd, pos, SEEK_SET) == -1)
			err(1, "lseek");
		n = 0;
		while ((r = read(fd, buf, BSIZE)) == BSIZE) {
			n++;
			for (i = 0; i < BSIZE; i++) {
				if (buf[i] != 0)
					errx(1, "Bad read value @ %jd = %d",
					    (intmax_t)(pos + i), buf[i]);
			}
		}
		if (r == -1)
			err(1, "read");
		if (n != (FSIZE - pos) / BSIZE)
			fprintf(stderr, "n = %d, target = %d\n", n, (int)(FSIZE - pos) / BSIZE);
	}
	unlink(name);

	_exit(0);
}

int
main(void)
{
	pid_t pids[PARALLEL];
	size_t len;
	int e, i, status;

	e = 0;
	len = PAGE_SIZE;
	if ((share = mmap(NULL, len, PROT_READ | PROT_WRITE,
	    MAP_ANON | MAP_SHARED, -1, 0)) == MAP_FAILED)
		err(1, "mmap");

	for (i = 0; i < PARALLEL; i++) {
		if ((pids[i] = fork()) == 0)
			test();
		if (pids[i] == -1)
			err(1, "fork()");
	}
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

	return (e);
}
