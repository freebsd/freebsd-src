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

# tmpfs(5) option nonc test scenario

. ../default.cfg
[ `id -u ` -ne 0 ] && echo "Must be root!" && exit 1

dir=/tmp
odir=`pwd`
cd $dir
sed '1,/^EOF/d' < $odir/$0 > $dir/tmpfs17.c
mycc -o tmpfs17 -Wall -Wextra -O0 -g tmpfs17.c || exit 1
rm -f tmpfs17.c
cd $odir

mount | grep "on $mntpoint " | grep -q /dev/md && umount -f $mntpoint
mount -o nonc -t tmpfs tmpfs $mntpoint || { rm /tmp/tmpfs17; exit 0; }

cd $mntpoint
/tmp/tmpfs17 &
pid=$!
sleep .2
pid2=`pgrep tmpfs17`
for i in `jot 5`; do
	for p in $pid2; do
		while true; do procstat -f $p > /dev/null 2>&1; done &
		pids="$pids $!"
	done
done
while kill -0 $pid 2>/dev/null; do
	sleep 2
done
kill $pids 2>/dev/null
wait
cd $odir

for i in `jot 10`; do
	umount $mntpoint && break
	sleep 1
done
s=0
mount | grep -q "on $mntpoint " && { s=1; umount -f $mntpoint; }
rm -rf /tmp/tmpfs17
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

static volatile u_int *share;

#define PARALLEL 4
#define RUNTIME (5 * 60)
#define SYNC 0

static void
test(void)
{
	pid_t pid;
	time_t start;
	int fd, i;
	char file[80];

	atomic_add_int(&share[SYNC], 1);
	while (share[SYNC] != PARALLEL)
		;

	i= 0;
	pid = getpid();
	start = time(NULL);
	while ((time(NULL) - start) < RUNTIME) {
		snprintf(file, sizeof(file), "file.%06d.%03d", pid, i++);
		if ((fd = open(file, O_RDWR|O_CREAT, DEFFILEMODE)) == -1)
			err(1, "open(%s)", file);
		close(fd);
		if (unlink(file) == -1)
			err(1, "unlink(%s)", file);
	}

	_exit(0);
}

int
main(void)
{
	size_t len;
	int e, i, pids[PARALLEL], status;

	e = 0;
	len = PAGE_SIZE;
	if ((share = mmap(NULL, len, PROT_READ | PROT_WRITE,
	    MAP_ANON | MAP_SHARED, -1, 0)) == MAP_FAILED)
		err(1, "mmap");

	for (i = 0; i < PARALLEL; i++) {
		if ((pids[i] = fork()) == 0)
			test();
	}
	for (i = 0; i < PARALLEL; i++) {
		if (waitpid(pids[i], &status, 0) == -1)
			err(1, "waitpid(%d)", pids[i]);
		e += status == 0 ? 0 : 1;
	}

	return (e);
}
