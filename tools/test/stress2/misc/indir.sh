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

# Hunt for "panic: ufsdirhash_dirtrunc: bad offset"
# by making the directory inode grow into the indirect blocks.

# Truncate directories larger than 12 * block_size bytes
# default is 12 * 32768 = 393216, 384k

# No problems seen.

. ../default.cfg
[ `id -u` -ne 0 ] && echo "Must be root!" && exit 1

dir=/tmp
odir=`pwd`
cd $dir
sed '1,/^EOF/d' < $odir/$0 > $dir/indir.c
sed -i '' -e "s#MNTPOINT#$mntpoint#g" $dir/indir.c
mycc -o indir -Wall -Wextra -O0 -g indir.c || exit 1
rm -f indir.c
cd $odir

set -e
mount | grep "on $mntpoint " | grep -q /dev/md && umount -f $mntpoint
[ -c /dev/md$mdstart ] &&  mdconfig -d -u $mdstart
mdconfig -a -t swap -s 1g -u $mdstart
newfs -j -n -b 16384 -f 2048 -i 2048 md$mdstart > /dev/null
mount /dev/md$mdstart $mntpoint
ifree=`df -i $mntpoint | tail -1 | awk '{print $7 - 50}'`
set +e

if [ $dtrace ]; then
	dtrace -w -n '*ufsdirhash_dirtrunc:entry {@rw[execname,probefunc] = \
	    count(); }' &
	dpid=$!
	sleep 2
fi

/tmp/indir $mntpoint $ifree

if [ $dtrace ]; then
	kill -s TERM $dpid
	wait $dpid
fi

for i in `jot 6`; do
	mount | grep -q "on $mntpoint " || break
	umount $mntpoint && break || sleep 10
	[ $i -eq 6 ] &&
	    { echo FATAL; fstat -mf $mntpoint; exit 1; }
done
mdconfig -d -u $mdstart
rm -f /tmp/indir
if [ $dtrace ]; then
	while pgrep -q dtrace; do sleep 1; done
	kldstat | grep -q dtraceall &&
	    kldunload dtraceall.ko
fi
exit 0
EOF
#include <sys/param.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/wait.h>

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

static long files;
static char *path;

#define PARALLEL 64
#define RUNTIME (10 * 60)

static void
test(int idx)
{
	long j;
	int fd;
	pid_t pid;
	char dir[128], file[128], new[128];

	sleep(1);

	snprintf(dir, sizeof(dir), "%s/d%d", path, idx);
	if (mkdir(dir, 0755) == -1) {
		if (errno != EEXIST)
			err(1, "mkdir(%s)", dir);
	}
	if (chdir(dir) == -1)
		err(1, "chdir(%s)", dir);

	pid = getpid();
	for (j = 0; j < files; j++) {
		sprintf(file,"p%05d.%05ld", pid, j);
		if ((fd = creat(file, 0660)) == -1)
			err(1, "creat(%s). %s:%d", file, __FILE__, __LINE__);
		if (fd != -1 && close(fd) == -1)
			err(2, "close(%ld)", j);
	}

	for (j = 0; j < files; j++) {
		sprintf(file,"p%05d.%05ld", pid, j);
		sprintf(new,"p%05d.%05ld.old", pid, j);
		if (rename(file, new) == -1)
			err(1, "rename(%s, %s)", file, new);
	}

	for (j = 0; j < files; j++) {
		sprintf(file,"p%05d.%05ld", pid, j);
		sprintf(new,"p%05d.%05ld.old", pid, j);
		if (rename(new, file) == -1)
			err(1, "rename(%s, %s)", new, file);
	}

	for (j = 0; j < files; j++) {
		sprintf(file,"p%05d.%05ld", pid, j);
		if (unlink(file) == -1)
			warn("unlink(%s)", file);

	}
	if (chdir("..") == -1)
		err(1, "chdir ..");

	_exit(0);
}

int
main(int argc, char *argv[])
{
	pid_t pids[PARALLEL];
	time_t start;
	int e, i, n, status;

	if (argc != 3) {
		fprintf(stderr, "Usage %s <path> <files>\n", argv[0]);
		exit(1);
	}
	path = argv[1];
	files = atol(argv[2]) / PARALLEL;
	fprintf(stderr, "Using %ld inodes per dir. %d threads. %ld inodes in total\n",
		files, PARALLEL, files * PARALLEL);
	e = n = 0;
	start = time(NULL);
	while ((time(NULL) - start) < RUNTIME && e == 0) {
		fprintf(stderr, "Loop #%d\n", ++n);
		for (i = 0; i < PARALLEL; i++) {
			if ((pids[i] = fork()) == 0)
				test(i);
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
//		system("(cd MNTPOINT; umount MNTPOINT) > /dev/null 2>&1");
	}

	return (e);
}
