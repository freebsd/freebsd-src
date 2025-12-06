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

# Temp file test scenario.
# "temp: unlink(file.034434.48): Permission denied" seen.

. ../default.cfg
[ `id -u ` -ne 0 ] && echo "Must be root!" && exit 1

[ -z "$nfs_export" ] && exit 0
ping -c 2 `echo $nfs_export | sed 's/:.*//'` > /dev/null 2>&1 ||
    exit 0

export LANG=C
dir=/tmp
odir=`pwd`
cd $dir
sed '1,/^EOF/d' < $odir/$0 > $dir/temp.c
mycc -o temp -Wall -Wextra -O0 -g temp.c || exit 1
rm -f temp.c
cd $odir

mount | grep "on $mntpoint " | grep -q nfs && umount $mntpoint
mount -t nfs -o tcp -o retrycnt=3 -o soft -o rw $nfs_export $mntpoint
mp2=$mntpoint/temp.`jot -rc 8 a z | tr -d '\n'`/temp.dir
rm -rf $mp2
mkdir -p $mp2
chmod 0777 $mp2

log=/tmp/temp.log
/tmp/temp $mp2 2> $log
s=$?
#[ $s -eq 0 ] && ministat -C 3 -A < $log
rm -rf $mp2

while mount | grep "on $mntpoint " | grep -q nfs; do
	umount $mntpoint || sleep 1
done
rm -rf /tmp/temp $log
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
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

volatile u_int *share;

#define SYNC 0

#define FILES 1024
#define PARALLEL 4
#define RUNTIME (10 * 60)

void
test(char *dir)
{
	pid_t pid;
	int fd, i;
	char path[1024];

	atomic_add_int(&share[SYNC], 1);
	while (share[SYNC] != PARALLEL)
		;

	pid = getpid();
	for (i = 0; i < FILES; i++) {
		snprintf(path, sizeof(path), "%s/file.%06d.%d", dir, pid, i);
		if ((fd = open(path, O_RDWR | O_CREAT | O_TRUNC, 0640)) == -1)
			err(1, "open(%s)", path);
		if (write(fd, path, sizeof(path)) != sizeof(path))
			err(1, "write");
		close(fd);
		if (unlink(path) == -1)
			err(1, "unlink(%s)", path);
	}
	_exit(0);
}

int
main(int argc, char *argv[])
{
	size_t len;
	struct timeval start, stop, diff;
	struct tm *tp;
	time_t now, then;
	uint64_t usec;
	int e, i, loop, pids[PARALLEL], status;
	char buf[80];

	e = 0;
	if (argc != 2) {
		fprintf(stderr, "Usage: %s <path>\n", argv[0]);
		_exit(1);
	}
	len = PAGE_SIZE;
	if ((share = mmap(NULL, len, PROT_READ | PROT_WRITE,
	    MAP_ANON | MAP_SHARED, -1, 0)) == MAP_FAILED)
		err(1, "mmap");

	loop = 0;
	alarm(2 * RUNTIME);
	now = time(NULL);
	while ((time(NULL) - now) < RUNTIME && e == 0) {
		share[SYNC] = 0;
		gettimeofday(&start, NULL);
		for (i = 0; i < PARALLEL; i++) {
			if ((pids[i] = fork()) == 0)
				test(argv[1]);
		}
		for (i = 0; i < PARALLEL; i++) {
			waitpid(pids[i], &status, 0);
			e += status == 0 ? 0 : 1;
		}
		gettimeofday(&stop, NULL);
		timersub(&stop, &start, &diff);
		usec  = ((uint64_t)1000000 *
		    diff.tv_sec + diff.tv_usec);
		then = time(NULL);
		tp = localtime(&then);
		strftime(buf, sizeof(buf), "%H:%M:%S", tp);
		if (loop != 0) /* Skip warmup */
			fprintf(stderr, "%s %6d %.3f\n",
			    buf, loop,
			    (double)usec / 1000000);
		loop++;
		sleep(5);
	}

	return (e);
}
