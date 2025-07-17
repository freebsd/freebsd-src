#!/bin/sh

#
# Copyright (c) 2011 Peter Holm <pho@FreeBSD.org>
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

[ `id -u ` -ne 0 ] && echo "Must be root!" && exit 1

# Demonstrate premature "out of inodes" problem with SU

. ../default.cfg

here=`pwd`
cd /tmp
sed '1,/^EOF/d' < $here/$0 > linger.c
mycc -o linger -Wall -O2 linger.c
rm -f linger.c
cd $here

mount | grep $mntpoint | grep -q /dev/md && umount -f $mntpoint
mdconfig -l | grep -q md$mdstart &&  mdconfig -d -u $mdstart
mdconfig -a -t swap -s 1g -u $mdstart
[ $# -eq 1 ] && opt="$1"
[ $# -eq 0 ] && opt=$newfs_flags	# No argument == default flag
echo "newfs $opt md$mdstart"
newfs $opt md$mdstart > /dev/null
mount /dev/md$mdstart $mntpoint
chmod 777 $mntpoint

if ! su $testuser -c "cd $mntpoint; /tmp/linger $size"; then
	min=2
	[ -r $mntpoint/.sujournal ] && min=3
	r=`df -hi $mntpoint | head -1`
	echo "         $r"
	for i in `jot 10`; do
		r=`df -hi $mntpoint | tail -1`
		echo "`date '+%T'` $r"
		[ `echo $r | awk '{print $6}'` = $min ] && break
		sleep 10
	done
	ls -lR $mntpoint
fi

while mount | grep "$mntpoint " | grep -q /dev/md; do
	umount $mntpoint || sleep 1
done
mdconfig -d -u $mdstart
rm -f /tmp/linger
exit
EOF
#include <sys/mount.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/wait.h>

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#define PARALLEL 10
#define TIMEOUT 1200
static int size = 6552;	/* 10 free inodes */

static int
test(void)
{
	int fd, i, j;
	pid_t pid;
	char file[128];

	for (;;) {
		if (access("rendezvous", R_OK) == 0)
			break;
		sched_yield();
	}
	pid = getpid();
	for (j = 0; j < size; j++) {
		sprintf(file,"p%05d.%05d", pid, j);
		if ((fd = creat(file, 0660)) == -1) {
			if (errno != EINTR) {
				warn("creat(%s). %s:%d", file, __FILE__, __LINE__);
				unlink("continue");
				break;
			}
		}
		if (fd != -1 && close(fd) == -1)
			err(2, "close(%d)", j);

	}
	sleep(3);

	for (i = --j; i >= 0; i--) {
		sprintf(file,"p%05d.%05d", pid, i);
		if (unlink(file) == -1)
			warn("unlink(%s)", file);

	}
	return (0);
}

int
main(void)
{
	time_t start;
	int error = 0, fd, i, j, status;

	umask(0);
	if ((fd = open("continue", O_CREAT, 0644)) == -1)
		err(1, "open()");
	close(fd);
	start = time(NULL);
	for (i = 0; i < 100; i++) {
		for (j = 0; j < PARALLEL; j++) {
			if (fork() == 0) {
				test();
				exit(0);
			}
		}

		if ((fd = open("rendezvous", O_CREAT, 0644)) == -1)
			err(1, "open()");
		close(fd);

		for (j = 0; j < PARALLEL; j++) {
			wait(&status);
			error += status;
		}

		unlink("rendezvous");
		if (access("continue", R_OK) == -1)
			break;
		if (time(NULL) - start > TIMEOUT) {
			fprintf(stderr, "FAIL Timeout\n");
			break;
		}
	}
	unlink("continue");

	return (error != 0);
}
