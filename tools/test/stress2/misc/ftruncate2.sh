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

# A fuzz test triggered a failed block allocation unwinding problem.

# "panic: ffs_blkfree_cg: freeing free block" seen:
# https://people.freebsd.org/~pho/stress/log/kostik923.txt
# Fixed by r304232.

[ `id -u ` -ne 0 ] && echo "Must be root!" && exit 1

. ../default.cfg

odir=`pwd`
cd /tmp
sed '1,/^EOF/d' < $odir/$0 > ftruncate2.c
rm -f /tmp/ftruncate2
mycc -o ftruncate2 -Wall -Wextra -O2 -g ftruncate2.c -lpthread || exit 1
rm -f ftruncate2.c

echo "Expect: \"/mnt: write failed, filesystem is full\""
mount | grep $mntpoint | grep -q "on $mntpoint " && umount -f $mntpoint
mdconfig -l | grep -q md$mdstart &&  mdconfig -d -u $mdstart
mdconfig -a -t swap -s 1g -u $mdstart || exit 1
newfs md$mdstart > /dev/null		# Non SU panics
mount /dev/md$mdstart $mntpoint

dir=$mntpoint
chmod 777 $dir

cd $dir
jot 500 | xargs touch
jot 500 | xargs chmod 666
cd $odir

(cd /tmp; /tmp/ftruncate2 $dir)
e=$?

rm -rf $dir/*

while mount | grep -q "on $mntpoint "; do
	umount $mntpoint || sleep 1
done
mdconfig -d -u $mdstart
rm -f /tmp/ftruncate2
exit $e
EOF
#include <sys/param.h>

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <fts.h>
#include <libutil.h>
#include <pthread.h>
#include <pwd.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define N (128 * 1024 / (int)sizeof(u_int32_t))
#define RUNTIME 180
#define THREADS 2

static int fd[900];
static u_int32_t r[N];
static char *args[2];

static unsigned long
makearg(void)
{
	unsigned long val;

	val = arc4random();
#if defined(__LP64__)
	val = (val << 32) | arc4random();
	val = val & 0x00007fffffffffffUL;
#endif

	return(val);
}

static void *
test(void *arg __unused)
{
	FTS *fts;
	FTSENT *p;
	int ftsoptions, i, n;

	ftsoptions = FTS_PHYSICAL;

	for (;;) {
		for (i = 0; i < N; i++)
			r[i] = arc4random();
		if ((fts = fts_open(args, ftsoptions, NULL)) == NULL)
			err(1, "fts_open");

		i = n = 0;
		while ((p = fts_read(fts)) != NULL) {
			if (fd[i] > 0)
				close(fd[i]);
			if ((fd[i] = open(p->fts_path, O_RDWR)) == -1)
				if ((fd[i] = open(p->fts_path, O_WRONLY)) == -1)
				continue;
			if (ftruncate(fd[i], 0) != 0)
				err(1, "ftruncate");
			i++;
			i = i % nitems(fd);
		}

		if (fts_close(fts) == -1)
			err(1, "fts_close()");
		sleep(1);
	}
	return(0);
}

static void *
calls(void *arg __unused)
{
	off_t offset;
	time_t start;
	int fd2;

	start = time(NULL);
	while ((time(NULL) - start) < RUNTIME) {
		fd2 = makearg() % nitems(fd) + 3;
		offset = makearg();
		if (lseek(fd2, offset - 1, SEEK_SET) != -1) {
			if (write(fd2, "x", 1) != 1)
				if (errno != EBADF && errno != ENOSPC && errno != E2BIG &&
				    errno != ESTALE && errno != EFBIG)
					warn("write");
		} else
			if (errno != EBADF)
				warn("lseek");
		if (fsync(fd2) == -1)
			if (errno != EBADF)
				warn("x");
	}

	return (0);
}

int
main(int argc, char **argv)
{
	struct passwd *pw;
	pthread_t rp, cp[THREADS];
	int e, i;

	if (argc != 2) {
		fprintf(stderr, "Usage: %s <dir>\n", argv[0]);
		exit(1);
	}
	args[0] = argv[1];
	args[1] = 0;

	if ((pw = getpwnam("nobody")) == NULL)
		err(1, "failed to resolve nobody");
	if (setgroups(1, &pw->pw_gid) ||
	    setegid(pw->pw_gid) || setgid(pw->pw_gid) ||
	    seteuid(pw->pw_uid) || setuid(pw->pw_uid))
		err(1, "Can't drop privileges to \"nobody\"");
	endpwent();

	if ((e = pthread_create(&rp, NULL, test, NULL)) != 0)
		errc(1, e, "pthread_create");
	usleep(1000);
	for (i = 0; i < THREADS; i++)
		if ((e = pthread_create(&cp[i], NULL, calls, NULL)) != 0)
			errc(1, e, "pthread_create");
	for (i = 0; i < THREADS; i++)
		pthread_join(cp[i], NULL);

	return (0);
}
