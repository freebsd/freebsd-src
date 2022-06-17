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

# Hunt for;
# Bug 204764 - Filesystem deadlock, process in vodead state
#    https://bugs.freebsd.org/bugzilla/show_bug.cgi?id=204764
# No problem seen.

. ../default.cfg

dir=/tmp
odir=`pwd`
cd $dir
sed '1,/^EOF/d' < $odir/$0 > $dir/lstat.c
mycc -o lstat -Wall -Wextra -O0 -g lstat.c || exit 1
rm -f lstat.c
cd $odir

mount | grep "on $mntpoint " | grep -q /dev/md && umount -f $mntpoint
mdconfig -l | grep -q md$mdstart &&  mdconfig -d -u $mdstart
mdconfig -a -t swap -s 1g -u $mdstart || exit 1
newfs -n -b 4096 -f 512 -i 1024 md$mdstart > /dev/null
mount -o async /dev/md$mdstart $mntpoint || exit 1

path=$mntpoint/a/b/c
mkdir -p $path

$dir/lstat $path

while mount | grep "on $mntpoint " | grep -q /dev/md; do
	umount $mntpoint || sleep 1
done
mdconfig -d -u $mdstart
rm -rf $dir/lstat $wdir/lstat.tmp.*
exit

EOF
#include <sys/param.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/wait.h>

#include <machine/atomic.h>

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <fts.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

static volatile u_int *dirs, *share;
static char *arg;

#define R1 0
#define R2 1

#define MXDIRS 3000
#define PARALLEL 2
#define RUNTIME 600
#define SLPTIME 400

static void
tfts(int idx)
{
	FTS *fts;
	FTSENT *p;
	struct stat sb;
	int ftsoptions;
	char *args[2];

	if (idx != 0)
		_exit(0);

	ftsoptions = FTS_PHYSICAL;
	args[0] = arg,
	args[1] = 0;

	setproctitle("fts");
	while (share[R2] == 0) {
		if ((fts = fts_open(args, ftsoptions, NULL)) == NULL)
			err(1, "fts_open");

		while ((p = fts_read(fts)) != NULL) {
			lstat(fts->fts_path, &sb);
			if (share[R2] == 1)
				break;
		}

		if (fts_close(fts) == -1)
			err(1, "fts_close()");
	}

	_exit(0);
}

static int
test(int idx)
{
	struct stat sb;
	pid_t fpid, pd, pid;
	size_t len;
	int i, r;
	char dir[128], path[128];

	atomic_add_int(&share[R1], 1);
	while (share[R1] != PARALLEL)
		;

	len = PAGE_SIZE;
	if ((dirs = mmap(NULL, len, PROT_READ | PROT_WRITE,
	    MAP_ANON | MAP_SHARED, -1, 0)) == MAP_FAILED)
		err(1, "mmap");

	if ((fpid = fork()) == 0)
		tfts(idx);

	pid = getpid();
	snprintf(dir, sizeof(dir), "%s/dir.%d", arg, pid);
	if (mkdir(dir, 0777) == -1)
		err(1, "mkdir(%s)", dir);
	if (chdir(dir) == -1)
		err(1, "chdir(%s)", dir);
	if ((pd = fork()) == 0) {
		setproctitle("mkdir");
		i = 0;
		while (share[R2] == 0) {
			snprintf(path, sizeof(path), "%s/d.%d.%d", arg, pid,
			    i);
			while (dirs[0] > MXDIRS && share[R2] == 0)
				usleep(SLPTIME);
			while ((r = mkdir(path, 0777)) == -1) {
				if (errno != EMLINK)
					err(1, "mkdir(%s) @ %d", path,
					    __LINE__);
				usleep(SLPTIME);
				if (share[2] == 1)
					break;
			}
			if (r == 0) {
				atomic_add_int(&dirs[0], 1);
				i++;
			}
		}

		_exit(0);
	}

	i = 0;
	setproctitle("rmdir");
	while (dirs[0] > 0 || share[R2] == 0) {
		if (dirs[0] < MXDIRS / 2)
			usleep(SLPTIME);
		snprintf(path, sizeof(path), "%s/d.%d.%d", arg, pid, i);
		while (lstat(path, &sb) == -1 && share[R2] == 0) {
			usleep(SLPTIME);
		}
		if (rmdir(path) == -1) {
			if (errno != ENOENT)
				err(1, "rmdir(%s)", path);
		} else {
			atomic_add_int(&dirs[0], -1);
			i++;
		}
	}
	waitpid(pd, NULL, 0);
	waitpid(fpid, NULL, 0);

	chdir("..");
	if ((rmdir(dir)) == -1)
		err(1, "unlink(%s)", dir);

	_exit(0);
}

int
main(int argc, char *argv[])
{
	size_t len;
	int e, i, pids[PARALLEL], status;

	if (argc != 2)
		errx(1, "Usage: %s <path>", argv[0]);
	arg = argv[1];

	e = 0;
	len = PAGE_SIZE;
	if ((share = mmap(NULL, len, PROT_READ | PROT_WRITE,
	    MAP_ANON | MAP_SHARED, -1, 0)) == MAP_FAILED)
		err(1, "mmap");

	share[R1] = 0;
	for (i = 0; i < PARALLEL; i++) {
		if ((pids[i] = fork()) == 0)
			test(i);
	}
	sleep(RUNTIME);
	share[R2] = 1;
	for (i = 0; i < PARALLEL; i++) {
		waitpid(pids[i], &status, 0);
		e += status == 0 ? 0 : 1;
	}

	return (e);
}
