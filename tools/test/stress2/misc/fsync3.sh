#!/bin/sh

#
# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) 2023 Peter Holm <pho@FreeBSD.org>
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

# Regression test for D37997: ffs_syncvnode(): avoid a LoR for SU
# https://people.freebsd.org/~pho/stress/log/log0402.txt
# Fixed by 6e1eabadcb1d - main - ffs_syncvnode(): avoid a LoR for SU

# Test scenario based on report by jkim

. ../default.cfg
[ `id -u` -ne 0 ] && echo "Must be root!" && exit 1

dir=/tmp
odir=`pwd`
prog=$(basename "$0" .sh)
cd $dir
sed '1,/^EOF/d' < $odir/$0 > $dir/$prog.c
mycc -o $prog -Wall -Wextra -O0 -g $prog.c -lpthread || exit 1
rm -f $prog.c
cd $odir

set -eu
mount | grep "on $mntpoint " | grep -q /dev/md && umount -f $mntpoint
[ -c /dev/md$mdstart ] &&  mdconfig -d -u $mdstart
mdconfig -a -t swap -s 1g -u $mdstart
newfs -U md$mdstart > /dev/null
mount /dev/md$mdstart $mntpoint
set +e

cd $odir
../testcases/swap/swap -t 1m -i 20 -l 100 > /dev/null &
sleep .5
cd $mntpoint
mkdir -p d1/d2/d3/d4/d5
for i in `jot 8`; do
	$dir/$prog $i &
done
cd $odir
wait

for i in `jot 6`; do
	mount | grep -q "on $mntpoint " || break
	umount $mntpoint && break || sleep 10
	[ $i -eq 6 ] &&
	    { echo FATAL; fstat -mf $mntpoint; exit 1; }
done
mdconfig -d -u $mdstart
rm -rf $dir/$prog
exit 0
EOF
#include <sys/types.h>
#include <sys/stat.h>

#include <err.h>
#include <fcntl.h>
#include <pthread.h>
#include <pthread_np.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define RUNTIME 60

static time_t start;
static volatile int fd, n;
static char buf[4096];

static void *
t1(void *data __unused)
{
	char *path = "d1/d2/d3/d4/d5";
	char d1[1024], d2[1024], file[1024];

	pthread_set_name_np(pthread_self(), __func__);
	snprintf(d1, sizeof(d1), "%s/dir.%d", path, getpid());
	snprintf(d2, sizeof(d2), "%s/new.%d", path, getpid());
	snprintf(file, sizeof(file), "%s/../file.%d", path, getpid());
	while (time(NULL) - start < RUNTIME) {
		if (mkdir(d1, 0740) == -1)
			err(1, "mkdir(%s)", d1);
		if (rename(d1, d2) == -1)
			err(1, "rename(%s, %s)", d1, d2);
		if ((fd = open(file, O_RDWR | O_CREAT | O_TRUNC, 0600)) == -1)
			err(1, "open%s()", file);
		if (write(fd, buf, sizeof(buf)) != sizeof(buf))
			err(1, "write()");
		close(fd);
		if (rename(d2, d1) == -1)
			err(1, "rename(%s, %s)", d2, d1);
		if (rmdir(d1) == -1)
			err(1, "rmdir(%s)", d1);
	}

	return (NULL);
}

static void *
t2(void *data __unused)
{
	pthread_set_name_np(pthread_self(), __func__);
	while (time(NULL) - start < RUNTIME) {
		fsync(fd);
		usleep(arc4random() % 500);
	}

	return (NULL);
}

int
main(int argc __unused, char *argv[])
{
	pthread_t tid[2];
	int r;

	n = atoi(argv[1]);
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
