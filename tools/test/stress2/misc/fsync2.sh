#!/bin/sh

#
# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) 2020 Peter Holm <pho@FreeBSD.org>
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

# Deadlock seen: https://people.freebsd.org/~pho/stress/log/mark169.txt
# Test scenario based on the syzkaller reproducer syzkaller21.sh and comments
# by markj@.

. ../default.cfg
[ `id -u` -ne 0 ] && echo "Must be root!" && exit 1

dir=/tmp
odir=`pwd`
cd $dir
sed '1,/^EOF/d' < $odir/$0 > $dir/fsync2.c
mycc -o fsync2 -Wall -Wextra -O0 -g fsync2.c -lpthread || exit 1
rm -f fsync2.c
cd $odir

set -e
mount | grep "on $mntpoint " | grep -q /dev/md && umount -f $mntpoint
[ -c /dev/md$mdstart ] &&  mdconfig -d -u $mdstart
mdconfig -a -t swap -s 2g -u $mdstart
newfs $newfs_flags md$mdstart > /dev/null
mount /dev/md$mdstart $mntpoint
set +e

(cd $mntpoint; $dir/fsync2)

for i in `jot 6`; do
	mount | grep -q "on $mntpoint " || break
	umount $mntpoint && break || sleep 10
	[ $i -eq 6 ] &&
	    { echo FATAL; fstat -mf $mntpoint; exit 1; }
done
mdconfig -d -u $mdstart
rm -rf $dir/fsync2
exit 0
EOF
#include <sys/types.h>
#include <sys/stat.h>

#include <err.h>
#include <fcntl.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define RUNTIME 60

static time_t start;
static volatile int fd;

static void *
t1(void *data __unused)
{
	char cwd[1024];

	if (getcwd(cwd, sizeof(cwd)) == NULL)
		err(1, "getcwd()");
	while (time(NULL) - start < RUNTIME) {
		if (mkdir("./file0", 0740) == -1)
			err(1, "mkdir(file0)");
		if (mkdir("./file1", 0740) == -1)
			err(1, "mkdir(file2)");
		if (rename("./file1", "./file0/file0") == -1)
			err(1, "rename()");
		if ((fd = open("./file0/file0", O_RDONLY)) == -1)
			err(1, "open()");
		if (chdir("./file0/file0") == -1)
			err(1, "chdir()");
		if (chdir(cwd) == -1)
			err(1, "chdir(HOME)");
		close(fd);
		rmdir("./file0/file0");
		rmdir("./file0");
	}

	return (NULL);
}

static void *
t2(void *data __unused)
{
	while (time(NULL) - start < RUNTIME) {
		fsync(fd);
	}

	return (NULL);
}

int
main(void)
{
	pthread_t tid[2];
	int r;

	start = time(NULL);
	if ((r = pthread_create(&tid[0], NULL, t1, NULL)) != 0)
		errc(1, r, "pthread_create");
	if ((r = pthread_create(&tid[1], NULL, t2, NULL)) != 0)
		errc(1, r, "pthread_create");

	if ((r = pthread_join(tid[0], NULL)) != 0)
		errc(1, r, "pthread_join");
	if ((r = pthread_join(tid[1], NULL)) != 0)
		errc(1, r, "pthread_join");

	return (0);
}
