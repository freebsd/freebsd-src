#!/bin/sh

#
# Copyright (c) 2013 EMC Corp.
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

# No problems seen with SU. Panics with SU+J, just like suj30.sh

# Triggers "known LOR in SU code" when crossmp8.sh is run first:
# https://people.freebsd.org/~pho/stress/log/rename12.txt.

[ `id -u ` -ne 0 ] && echo "Must be root!" && exit 1

. ../default.cfg

here=`pwd`
cd /tmp
sed '1,/^EOF/d' < $here/$0 > rename12.c
mycc -o rename12 -Wall -Wextra rename12.c || exit 1
rm -f rename12.c

mount | grep "on $mntpoint " | grep -q /dev/md && umount -f $mntpoint
mdconfig -l | grep -q md$mdstart &&  mdconfig -d -u $mdstart

mdconfig -a -t swap -s 4g -u $mdstart || exit 1
newfs $newfs_flags md$mdstart > /dev/null
mount /dev/md$mdstart $mntpoint

inodes=`df -i $mntpoint | tail -1 | awk '{print $7}'`
loops=4
parallel=12
timeout=1200
start=`date '+%s'`
for i in `jot $loops`; do
	for j in `jot $parallel`; do
		mkdir -p $mntpoint/d$j/dir1
		mkdir -p $mntpoint/d$j/dir2
		(cd $mntpoint/d$j; /tmp/rename12 $((inodes/parallel)) ) &
	done
	wait
	for j in `jot $parallel`; do
		rmdir  $mntpoint/d$j/dir1
		rmdir  $mntpoint/d$j/dir2
	done
	[ $((`date '+%s'` - start)) -lt $timeout ] && break
done

while mount | grep "on $mntpoint " | grep -q /dev/md; do
	umount $mntpoint || sleep 1
done

checkfs /dev/md$mdstart; s=$?
mdconfig -d -u $mdstart
rm -rf /tmp/rename12
exit $s
EOF
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <libgen.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

pid_t crpid;
long n;
int mvpipe[2], rmpipe[2];

void
cr(void)
{
	pid_t pid;
	int i;
	char name[80];

	setproctitle("cr");
	pid = getpid();
	usleep(arc4random() & 1000);
	for (i = 0; i < n; i++) {
		snprintf(name, sizeof(name), "dir1/d.%06d.%06d", pid, i);
		if (mkdir(name, 0700) == -1)
			err(1, "mkdir(%s). %s:%d", name, __FILE__, __LINE__);
		if (write(mvpipe[1], &i, sizeof(i)) != sizeof(i))
			err(1, "write mvpipe");
	}

	_exit(0);
}

void
mv(void)
{
	pid_t pid;
	int i;
	char name[80], to[80];

	setproctitle("mv");
	pid = crpid;
	i = 0;
	while (i != n - 1 &&
		    read(mvpipe[0], &i, sizeof(i)) == sizeof(i)) {
		snprintf(name, sizeof(name), "dir1/d.%06d.%06d", pid, i);
		snprintf(to  , sizeof(to  ), "dir2/d.%06d.%06d", pid, i);
		if (rename(name, to) == -1)
			warn("rename(%s, %s)", name, to);
		if (write(rmpipe[1], &i, sizeof(i)) != sizeof(i))
			err(1, "write rmpipe");
	}
	_exit(0);
}

void
rm(void)
{
	pid_t pid;
	int i;
	char to[80];

	setproctitle("rm");
	pid = crpid;
	i = 0;
	while (i != n - 1 &&
		    read(rmpipe[0], &i, sizeof(i)) == sizeof(i)) {
		snprintf(to, sizeof(to  ), "dir2/d.%06d.%06d", pid, i);
		if (rmdir(to) == -1)
			warn("rmdir(%s)", to);
	}
	_exit(0);
}

int
main(int argc, char **argv)
{
	int r, s;

	if (argc != 2)
		errx(1, "Usage %s <num inodes>", argv[0]);
	n = atol(argv[1]);
	if (n > 32765) {
		n = 32765 - 1;
	}

	if (pipe(mvpipe) == -1)
		err(1, "pipe()");
	if (pipe(rmpipe) == -1)
		err(1, "pipe()");

	r = 0;
	if ((crpid = fork()) == 0)
		cr();
	if (fork() == 0)
		mv();
	if (fork() == 0)
		rm();

	wait(&s);
	r += WEXITSTATUS(s);
	wait(&s);
	r += WEXITSTATUS(s);
	wait(&s);
	r += WEXITSTATUS(s);

	return (r);
}
