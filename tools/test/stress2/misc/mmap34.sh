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

# "panic: caller failed to provide space -546873344 at address 0x20a00000"
# seen.
# https://people.freebsd.org/~pho/stress/log/indir_trunc.txt
# Fixed by r348968

# Test scenario inspired by:
# syzbot+6532e9aab8911f58beeb@syzkaller.appspotmail.com

. ../default.cfg
[ `id -u` -ne 0 ] && echo "Must be root!" && exit 1

dir=/tmp
odir=`pwd`
cd $dir
sed '1,/^EOF/d' < $odir/$0 > $dir/mmap34.c
mycc -o mmap34 -Wall -Wextra -O0 -g mmap34.c -lpthread || exit 1
cd $odir

set -e
mount | grep "on $mntpoint " | grep -q /dev/md && umount -f $mntpoint
[ -c /dev/md$mdstart ] &&  mdconfig -d -u $mdstart
mdconfig -a -t swap -s 4g -u $mdstart
newfs $newfs_flags -n md$mdstart > /dev/null
mount /dev/md$mdstart $mntpoint
set +e

(cd ../testcases/swap; ./swap -t 5m -i 20) &
cd $mntpoint
$dir/mmap34
s=$?
while pkill swap; do :; done
wait
[ -f mmap34.core ] &&
    { ls -l mmap34.core; mv mmap34.core $dir; s=1; }
cd $odir

for i in `jot 6`; do
	mount | grep -q "on $mntpoint " || break
	umount $mntpoint && break || sleep 10
	[ $i -eq 6 ] &&
	    { echo FATAL; fstat -mf $mntpoint; exit 1; }
done
mdconfig -d -u $mdstart
rm -f $dir/mmap34
[ $s -eq 0 ] && rm -f /tmp/mmap34.c
exit $s

EOF
#include <sys/param.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/wait.h>

#include <machine/atomic.h>

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

static volatile u_int *share;

#if defined(__LP64__)
#define MASK 0x7ffffffffffffULL
#else
#define MASK 0xffffffffULL
#endif
#define PARALLEL 64
#define RUNTIME (5 * 60)
#define SYNC 0

static int fd;

static void *
t1(void *data __unused)
{
	off_t offset;
	time_t start;

	start = time(NULL);
	while (time(NULL) - start < 30) {
		offset = arc4random();
		offset = (offset << 32) | arc4random();
		offset &= MASK;
		if (lseek(fd, offset, SEEK_SET) == -1)
			err(1, "lseek(%jd)", offset);
		if (write(fd, "a", 1) != 1)
			err(1, "write");
		if (fsync(fd) == -1)
			err(1, "fsync");
		usleep(100);
	}
	return (NULL);
}
static void *
t2(void *data __unused)
{
	off_t offset, old;
	time_t start;
	void *p;
	char *c;

	old = 0;
	start = time(NULL);
	while (time(NULL) - start < 30) {
		if (old != 0)
			munmap(p, old);
		offset = arc4random();
		offset = (offset << 32) | arc4random();
		offset &= MASK;
		if (ftruncate(fd, offset) == -1)
			err(1, "ftruncate(%jd)", offset);
		write(fd, "b", 1);
		p = mmap(NULL, offset, PROT_READ | PROT_WRITE, MAP_SHARED,
		    fd, 0);
		if (p == MAP_FAILED)
			old = 0;
		else {
			old = offset;
			c = p;
			c[offset / 2] = 1;
			if (offset > 0)
				c[offset - 1] = 2;
		}
		usleep(20000);
	}
	return (NULL);
}

static void
test(void)
{
	pthread_t tid[2];
	int r;
	char file[80];

	atomic_add_int(&share[SYNC], 1);
	while (share[SYNC] != PARALLEL)
		;

	snprintf(file, sizeof(file), "file.%d", getpid());
	if ((fd = open(file, O_RDWR | O_CREAT, DEFFILEMODE)) == -1)
		err(1, "open(%s)", file);

	if ((r = pthread_create(&tid[0], NULL, t1, NULL)) != 0)
		errc(1, r, "pthread_create");
	if ((r = pthread_create(&tid[1], NULL, t2, NULL)) != 0)
		errc(1, r, "pthread_create");

	if ((r = pthread_join(tid[0], NULL)) != 0)
		errc(1, r, "pthread_join");
	if ((r = pthread_join(tid[1], NULL)) != 0)
		errc(1, r, "pthread_join");
	close(fd);
	unlink(file);

	_exit(0);
}

int
main(void)
{
	pid_t pids[PARALLEL];
	size_t len;
	time_t start;
	int e, i, status;

	e = 0;
	len = PAGE_SIZE;
	if ((share = mmap(NULL, len, PROT_READ | PROT_WRITE,
	    MAP_ANON | MAP_SHARED, -1, 0)) == MAP_FAILED)
		err(1, "mmap");

	start = time(NULL);
	while ((time(NULL) - start) < RUNTIME && e == 0) {
		share[SYNC] = 0;
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
	}

	return (e);
}
