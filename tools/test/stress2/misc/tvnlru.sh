#!/bin/sh

#
# Copyright (c) 2017 Dell EMC Isilon
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

# Scenario: backup of a number (> maxvnodes) of small files.

# Test how vnlru impacts open(2).

# FreeBSD 12.0-CURRENT #0 r312620: Mon Jan 23 23:27:46 CET 2017
# /usr/src/sys/amd64/compile/BENCH  amd64
# ./tvnlru.sh
# FAIL 328/306502
# files =  500000, maxvnodes = 500000, ave=0.000018, max=0.000328, elapsed  4
# files =  500000, maxvnodes = 500000, ave=0.000018, max=0.000155, elapsed  4
# files =  500000, maxvnodes = 500000, ave=0.000018, max=0.000227, elapsed  4
# files =  500000, maxvnodes = 500000, ave=0.000014, max=0.000126, elapsed  3
#
# files = 1000000, maxvnodes = 500000, ave=0.000035, max=0.205627, elapsed 14
# files = 1000000, maxvnodes = 500000, ave=0.000033, max=0.205185, elapsed 14
# files = 1000000, maxvnodes = 500000, ave=0.000038, max=0.306502, elapsed 14
# files = 1000000, maxvnodes = 500000, ave=0.000037, max=0.205177, elapsed 14

. ../default.cfg
[ `id -u ` -ne 0 ] && echo "Must be root!" && exit 1
[ `sysctl -n hw.physmem` -lt $(( 4 * 1024 * 1024 * 1024)) ] && exit 0

files=1000000
[ `sysctl -n kern.maxvnodes` -lt $files ] && exit 0

log=/tmp/tvnlru.log
dir=/tmp
odir=`pwd`
cd $dir
sed '1,/^EOF/d' < $odir/$0 > $dir/tvnlru.c
mycc -o tvnlru -Wall -Wextra -O0 -g tvnlru.c || exit 1
rm -f tvnlru.c
cd $odir

mount | grep -q "on $mntpoint " && umount -f $mntpoint
[ -c /dev/md$mdstart ] &&  mdconfig -d -u $mdstart
mdconfig -a -t swap -s 2g -u $mdstart || exit 1
newfs -n -b 4096 -f 512 -i 512 md$mdstart > /dev/null
mount -o async /dev/md$mdstart $mntpoint

ncpu=`sysctl -n hw.ncpu`
[ $ncpu -lt 4 ] && { rm /tmp/tvnlru; exit 0; }
ncpu=4
inodes=`df -i $mntpoint | tail -1 | awk "{print \\$7 - $ncpu - 1}"`
oldmx=`sysctl -n kern.maxvnodes`
[ $files -gt $inodes ] && { echo "Disk too small"; files=$inodes; }
[ $files -gt $oldmx ] &&
    { echo "$files exceed old maxvnods"; files=$oldmx; }
newmaxvnodes=$((files / 2))
trap "sysctl kern.maxvnodes=$oldmx > /dev/null" EXIT SIGINT
sysctl kern.maxvnodes=$newmaxvnodes > /dev/null

# warmup
cd $mntpoint
t1=`/tmp/tvnlru $ncpu $newmaxvnodes $newmaxvnodes $mntpoint 2>/dev/null`
cd $odir
umount $mntpoint
newfs -n -b 4096 -f 512 -i 512 md$mdstart > /dev/null
mount -o async /dev/md$mdstart $mntpoint

cd $mntpoint
t1=`/tmp/tvnlru $ncpu $newmaxvnodes $newmaxvnodes $mntpoint 2>$log`
cd $odir
umount $mntpoint
newfs -n -b 4096 -f 512 -i 512 md$mdstart > /dev/null
mount -o async /dev/md$mdstart $mntpoint

cd $mntpoint
echo >> $log
t2=`/tmp/tvnlru $ncpu $files $newmaxvnodes $mntpoint 2>>$log`
s=$?
cd $odir

s=0
for i in `jot 10`; do
	mount | grep -q "on $mntpoint " || break
	umount $mntpoint || sleep 2
done
mount | grep -q "on $mntpoint " && { s=2; umount -f $mntpoint; }
mdconfig -d -u $mdstart
[ $t2 -gt $((t1 * 3)) ] && { s=3; echo "Fail $t1/$t2"; cat $log; }
rm -rf /tmp/tvnlru /tmp/tvnlru.log
exit $s

EOF
#include <sys/param.h>
#include <sys/mman.h>
#include <sys/time.h>
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

#define SYNC 0

static long maxvnodes, parallel, tvnodes, vnodes;
static char *mp;

static void
test(int idx)
{
	struct timeval diff, start, stop;
	time_t st;
	uint64_t mx, tot, usec;
	pid_t pid;
	int fd, i, n;
	char dir[80], file[80], help[80];

	atomic_add_int(&share[SYNC], 1);
	while (share[SYNC] != (unsigned int)parallel)
		;

	pid = getpid();
	snprintf(dir, sizeof(dir), "d%09ld", (long)pid);
	if (mkdir(dir, 0700) == -1)
		err(1, "mkdir(%s)", dir);
	if (chdir(dir) == -1)
		err(1, "chdir(%s)", dir);

	for (i = 0; i < vnodes; i++) {
		snprintf(file, sizeof(file), "f%09d", i);
		if ((fd = open(file, O_RDWR | O_CREAT, DEFFILEMODE)) == -1)
			err(1, "open(%s)", file);
		close(fd);
	}

	snprintf(help, sizeof(help), "umount %s > /dev/null 2>&1", mp);
	system(help); /* flush the cache */

	mx = 0;
	n = 0;
	st = time(NULL);
	tot = 0;
	for (i = 0; i < vnodes; i++) {
		snprintf(file, sizeof(file), "f%09d", i);
		gettimeofday(&start, NULL);
		if ((fd = open(file, O_RDONLY)) == -1)
			err(1, "open(%s)", file);
		gettimeofday(&stop, NULL);
		timersub(&stop, &start, &diff);
		usec  = ((uint64_t)1000000 * diff.tv_sec + diff.tv_usec);
		tot += usec;
		n++;
		if (mx < usec)
			mx = usec;
		close(fd);
	}
	fprintf(stderr,
	    "files = %7ld, maxvnodes = %ld, ave=%.6f, max=%.6f, "
	    "elapsed %2ld\n",
	    tvnodes, maxvnodes, (double)tot / 1000000 / n, (double)mx /
	    1000000, time(NULL) - st);
	share[idx] = mx;

	for (i = 0; i < vnodes; i++) {
		snprintf(file, sizeof(file), "f%09d", i);
		if (unlink(file) == -1)
			err(1, "unlink(%s)", file);
	}
	chdir("..");
	if (rmdir(dir) == -1)
		err(1, "rmdir(%s)", dir);

	_exit(0);
}

int
main(int argc, char *argv[])
{
	size_t len;
	pid_t *pids;
	int e, i, status;
	u_int mx;

	if (argc != 5) {
		fprintf(stderr, "Usage: %s <ncpu> <inodes> <maxvnodes> <mount point>\n",
		    argv[0]);
		exit(1);
	}
	parallel = atol(argv[1]);
	pids = calloc(parallel, sizeof(pid_t));
	tvnodes = atol(argv[2]);
	vnodes = tvnodes / parallel;
	maxvnodes = atol(argv[3]);
	mp = argv[4];
	e = 0;
	len = PAGE_SIZE;
	if ((share = mmap(NULL, len, PROT_READ | PROT_WRITE,
	    MAP_ANON | MAP_SHARED, -1, 0)) == MAP_FAILED)
		err(1, "mmap");

	for (i = 0; i < parallel; i++) {
		if ((pids[i] = fork()) == 0)
			test(i + 1);
	}

	for (i = 0; i < parallel; i++) {
		if (waitpid(pids[i], &status, 0) == -1)
			err(1, "waitpid(%d)", pids[i]);
		e += status == 0 ? 0 : 1;
	}

	mx = 0;
	for (i = 0; i < parallel; i++) {
//		fprintf(stderr, "share[%d] = %u\n", i + 1, share[i + 1]);
		if (mx < share[i + 1])
			mx = share[i + 1];
	}
	fprintf(stdout, "%lu\n", (unsigned long)mx);

	return (e);
}
