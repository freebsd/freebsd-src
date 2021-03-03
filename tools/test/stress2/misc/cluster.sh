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

# Open four (sparse) files for random read and write.

# "panic: softdep_deallocate_dependencies: dangling deps" seen:
# https://people.freebsd.org/~pho/stress/log/kirk075.txt

[ `id -u ` -ne 0 ] && echo "Must be root!" && exit 1

. ../default.cfg

dir=`dirname $diskimage`
free=`df -k $dir | tail -1 | awk '{print $4}'`
[ $((free / 1024 / 1024)) -lt 9 ] && echo "Not enough disk space." && exit

odir=`pwd`
cd /tmp
sed '1,/^EOF/d' < $odir/$0 > cluster.c
rm -f /tmp/cluster
mycc -o cluster -Wall -Wextra -g -O2 cluster.c || exit 1
rm -f cluster.c
cd $odir

su $testuser -c "/tmp/cluster $dir abc"

rm -f /tmp/cluster
exit 0
EOF
#include <sys/param.h>
#include <sys/wait.h>

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define BSIZE (8 * 1024 * 1024)
#define MX (8LL * 1024 * 1024 * 1024)
#define PARALLEL 4
#define RUNTIME 600
#define WRLOOPS  1024

int rfd;
char *buf;
char *path;
char *uid;
char file[MAXPATHLEN + 1];

unsigned long long
rnd(void) {
	unsigned long long v;

	read(rfd, &v, sizeof(v));
	v = v % MX;
	return (v);
}

void
wr(int idx)
{
	off_t offset;
	size_t ln;
	int fd, i, n;

	snprintf(file, sizeof(file), "%s/f.%s.%06d", path, uid, idx);
	setproctitle(__func__);
	if ((fd = open(file, O_RDWR | O_CREAT, 0644)) == -1)
		err(1, "open(%s)", file);
	n = arc4random() % WRLOOPS + 1;
	for (i = 0; i < n; i++) {
		ln = rnd() % BSIZE + 1;
		offset = rnd() % (MX - ln);
		if (lseek(fd, offset, SEEK_SET) == -1)
			err(1, "lseek in rw 1");
		while (lockf(fd, F_LOCK, ln) == -1) {
			if (errno != EDEADLK)
				err(1, "lockf(%s, F_LOCK)", file);
		}
		if (write(fd, buf, ln) < 0)
			err(1, "write");
		if (lseek(fd, offset, SEEK_SET) == -1)
			err(1, "lseek in rw 2");
		if (lockf(fd, F_ULOCK, ln) == -1)
			err(1, "lockf(%s, F_ULOCK)", file);
	}
	close(fd);
	_exit(0);
}

void
rd(int idx)
{
	off_t offset;
	size_t ln;
	int fd, i, n;

	snprintf(file, sizeof(file), "%s/f.%s.%06d", path, uid, idx);
	setproctitle(__func__);
	for (i = 0; i < 100; i++) {
		if (access(file, R_OK) == 0)
			break;
		usleep(1000);
	}
	if ((fd = open(file, O_RDONLY)) == -1)
		if (errno != ENOENT)
			err(1, "open(%s)for read", file);
	n = arc4random() % WRLOOPS + 1;
	for (i = 0; i < n; i++) {
		ln = rnd() % BSIZE + 1;
		offset = rnd() % (MX - ln);
		if (lseek(fd, offset, SEEK_SET) == -1) {
			if (errno == EBADF)
				continue;
			err(1, "lseek in rd");
		}
		if (read(fd, buf, ln) < 0)
			err(1, "write");
	}
	close(fd);
	_exit(0);
}

void
mv(int idx)
{
	int i;
	char file2[MAXPATHLEN + 1];

	snprintf(file, sizeof(file), "%s/f.%s.%06d", path, uid, idx);
	snprintf(file2, sizeof(file2), "%s/f.%s.%06d.old", path, uid, idx);
	for (i = 0; i < 100; i++) {
		if (access(file, R_OK) == 0)
			break;
		usleep(1000);
	}
	if (rename(file, file2) == -1)
		if (errno != ENOENT)
			warn("rename(%s, %s)", file, file2);
	_exit(0);
}

void
tr(int idx)
{
	off_t offset;
	int fd;

	if (arc4random() % 100 < 10) {
		snprintf(file, sizeof(file), "%s/f.%s.%06d", path, uid, idx);
		setproctitle(__func__);
		if ((fd = open(file, O_RDWR | O_CREAT, 0644)) == -1)
			err(1, "open(%s)for read", file);
		offset = rnd() % MX;
		offset = rnd();
		if (ftruncate(fd, offset) == -1)
			err(1, "truncate");
		close(fd);
	}
	_exit(0);
}

void
rm(int idx)
{
	int i;
	char file2[MAXPATHLEN + 1];

	snprintf(file2, sizeof(file2), "%s/f.%s.%06d.old", path, uid, idx);
	for (i = 0; i < 100; i++) {
		if (access(file2, R_OK) == 0)
			break;
		usleep(1000);
	}
	if (unlink(file2) == -1)
		if (errno != ENOENT)
			warn("unlink(%s)", file2);
	_exit(0);
}

void
test2(void (*func)(int nr))
{
	time_t start;
	int i;

	setproctitle(__func__);
	start = time(NULL);
	while (time(NULL) - start < RUNTIME) {
		for (i = 0; i < PARALLEL; i++) {
			if (fork() == 0)
				func(i);
		}
		for (i = 0; i < PARALLEL; i++)
			wait(NULL);
	}
	_exit(0);

}

void
test(void (*func)(int nr))
{

	if (fork() == 0)
		test2(func);
}

int
main(int argc, char *argv[])
{
	int i;

	if (argc != 3)
		errx(1, "Usage: %s <path> <uid>", argv[0]);

	path = argv[1];
	uid = argv[2];

	if ((rfd = open("/dev/random", O_RDONLY)) == -1)
		err(1, "open(/dev/random)");
	setproctitle(__func__);
	buf = malloc(BSIZE);
	test(wr);
	test(rd);
	test(tr);
	test(mv);
	for (i = 0; i < 4; i++)
		if (wait(NULL) == -1)
			err(1, "wait");

	for (i = 0; i < PARALLEL; i++) {
		snprintf(file, sizeof(file), "%s/f.%s.%06d", path, uid, i);
		unlink(file);
		snprintf(file, sizeof(file), "%s/f.%s.%06d.old", path, uid, i);
		unlink(file);
	}

	return (0);
}
