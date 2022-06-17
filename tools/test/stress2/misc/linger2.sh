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

# Variation of linger.sh, with emphasis on blocks.

. ../default.cfg

here=`pwd`
cd /tmp
sed '1,/^EOF/d' < $here/$0 > linger2.c
mycc -o linger2 -Wall -O2 linger2.c
rm -f linger2.c
cd $here

mount | grep $mntpoint | grep -q /dev/md && umount -f $mntpoint
mdconfig -l | grep -q md$mdstart &&  mdconfig -d -u $mdstart
mdconfig -a -t swap -s 1g -u $mdstart
[ $# -eq 1 ] && opt="$1"
[ $# -eq 0 ] && opt=$newfs_flags	# No argument == default flag
newfs $opt -n md$mdstart > /dev/null
mount /dev/md$mdstart $mntpoint
chmod 777 $mntpoint
set `df -i $mntpoint | tail -1 | awk '{print $3, $6}'`

min=24
[ -r $mntpoint/.sujournal ] && { size=88; min=8232; }
if ! su $testuser -c "cd $mntpoint; /tmp/linger2 $size 2>/dev/null"; then
	r=`df -i $mntpoint | head -1`
	echo "         $r"
	for i in `jot 12`; do
		r=`df -ik $mntpoint | tail -1`
		[ "$r" != "$old" ] && echo "`date '+%T'` $r"
		old=$r
		[ `echo $r | awk '{print $3}'` -le $min ] && break
		sleep 10
	done
fi

while mount | grep "$mntpoint " | grep -q /dev/md; do
	umount $mntpoint || sleep 1
done
mdconfig -d -u $mdstart
rm -f /tmp/linger2
exit
EOF
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#define PARALLEL 10
static int size = 89;

int
test(void)
{
	int error = 0, fd, i, j;
	pid_t pid;
	char file[128];
	char *buf;

	for (;;) {
		if (access("rendezvous", R_OK) == 0)
			break;
		sched_yield();
	}
	pid = getpid();
	buf = malloc(1024 * 1024);
	for (j = 0; j < size; j++) {
		sprintf(file,"p%05d.%05d", pid, j);
		if ((fd = creat(file, 0660)) == -1) {
			if (errno != EINTR) {
				warn("creat(%s). %s:%d", file, __FILE__, __LINE__);
				unlink("continue");
				error = 1;
				break;
			}
		}
		if (write(fd, buf, 1024 * 1024) != 1024 * 1024) {
			warn("write()");
			unlink("continue");
			error = 1;
			break;
		}
		if (fd != -1 && close(fd) == -1)
			err(2, "close(%d)", j);

	}
	sleep(3);

	if (error == 0)
		j--;
	for (i = j; i >= 0; i--) {
		sprintf(file,"p%05d.%05d", pid, i);
		if (unlink(file) == -1)
			warn("unlink(%s)", file);

	}
	return (error);
}

int
main(int argc, char **argv)
{
	int error = 0, fd, i, j, status;

	if (argc == 2)
		size = atoi(argv[1]);

	umask(0);
	if ((fd = open("continue", O_CREAT, 0644)) == -1)
		err(1, "open()");
	close(fd);
	for (i = 0; i < 200; i++) {
		for (j = 0; j < PARALLEL; j++) {
			if (fork() == 0)
				exit(test());
		}

		if ((fd = open("rendezvous", O_CREAT, 0644)) == -1)
			err(1, "open()");
		close(fd);

		for (j = 0; j < PARALLEL; j++) {
			wait(&status);
			error += status;
		}

		unlink("rendezvous");
		if (access("continue", R_OK) == -1) {
			break;
		}
	}
	unlink("continue");

	return (error != 0);
}
