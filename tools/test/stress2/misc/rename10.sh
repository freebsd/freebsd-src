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

[ `id -u ` -ne 0 ] && echo "Must be root!" && exit 1

# Regression test for rename(2) problem with missing reference release of
# a busy "to" vnode, resulting in a leak.
# Fixed in r253998.

. ../default.cfg

here=`pwd`
cd /tmp
sed '1,/^EOF/d' < $here/$0 > rename10.c
mycc -o rename10 -Wall -Wextra -g -O2 rename10.c || exit 1
rm -f rename10.c
cd $here

mount | grep "on $mntpoint " | grep -q /dev/md && umount -f $mntpoint
mdconfig -l | grep -q md$mdstart &&  mdconfig -d -u $mdstart

mdconfig -a -t swap -s 4g -u $mdstart || exit 1
newfs $newfs_flags md$mdstart > /dev/null
mount /dev/md$mdstart $mntpoint
avail=`df -k $mntpoint | tail -1 | awk '{print $4}'`

cd $mntpoint
/tmp/rename10; s=$?
cd $here

for i in `jot 3`; do
	sync
	sleep 2
done

if [ `df -k $mntpoint | tail -1 | awk '{print $4}'` -lt $avail ]; then
	echo FAIL
	ls -ial $mntpoint
	df -i $mntpoint
fi

n=0
while mount | grep "on $mntpoint " | grep -q /dev/md; do
	umount $mntpoint || sleep 1
	n=$((n + 1))
	[ $n -gt 5 ] && { umount -f $mntpoint; break; }
done

checkfs /dev/md$mdstart || s=$?
rm -f /tmp/rename10
mdconfig -d -u $mdstart
exit $s
EOF
#include <err.h>
#include <fcntl.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#define PARALLEL 4
#define SIZE (1 * 1024 * 1024)

static char *logfile = "logfile";
static char *oldfiles[] = {
    "logfile.0", "logfile.1", "logfile.2", "logfile.3", "logfile.4"
};

void *
logger(void)
{
	int fd;
	char * cp;

	setproctitle("logger");
	cp = calloc(1, SIZE);
	for(;;) {
		if ((fd = open(logfile, O_RDWR | O_APPEND)) != -1) {
			if (write(fd, cp, SIZE) != SIZE)
				err(1, "write()");
			close(fd);
		}
		usleep(1);
	}

	_exit(0);
}

void *
spin(void)
{
	int fd, i;

	setproctitle("spin");
	for(;;) {
		for (i = 0; i < 5; i++) {
			if ((fd = open(oldfiles[i], O_RDWR | O_APPEND)) != -1)
				close(fd);
		}
		usleep(1);
	}
	_exit(0);
}

void
renamer()
{
	int fd, i;
	time_t start;

	setproctitle("renamer");
	start = time(NULL);
	i = 0;
	while (time(NULL) - start < 60) {
		if ((fd = open(logfile, O_RDWR | O_CREAT | O_EXCL, 0644)) == -1)
			err(1, "creat(%s)", logfile);
		close(fd);
		if (rename(logfile, oldfiles[i]) == -1)
			err(1, "rename(%s, %s)", logfile, oldfiles[i]);
		i = (i + 1) % 5;
	}
	for (i = 0; i < 5; i++) {
		unlink(oldfiles[i]);
	}
	unlink(logfile);

}

int
main() {
	pid_t pids[PARALLEL], spids[PARALLEL];
	int i;

	for (i = 0; i < PARALLEL; i++) {
		if ((pids[i] = fork()) == 0)
			logger();
		if ((spids[i] = fork()) == 0)
			spin();
	}

	renamer();

	for (i = 0; i < PARALLEL; i++) {
		if (kill(pids[i], SIGINT) == -1)
			err(1, "kill(%d)", pids[i]);
		if (kill(spids[i], SIGINT) == -1)
			err(1, "kill(%d)", spids[i]);
	}
	for (i = 0; i < PARALLEL * 2; i++)
		wait(NULL);
	wait(NULL);

	return (0);
}
