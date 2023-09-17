#!/bin/sh

#
# Copyright (c) 2014 EMC Corp.
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

. ../default.cfg

here=`pwd`
cd /tmp
sed '1,/^EOF/d' < $here/$0 > lockf3.c
mycc -o lockf3 -Wall -Wextra -O2 lockf3.c || exit 1
rm -f lockf3.c

mount | grep -q "$mntpoint" && umount $mntpoint
mdconfig -l | grep -q $mdstart &&  mdconfig -d -u $mdstart

mdconfig -a -t swap -s 40m -u $mdstart

newfs $newfs_flags md$mdstart > /dev/null
mount /dev/md$mdstart $mntpoint

cd $mntpoint
for i in `jot 3`; do
	$here/../testcases/swap/swap -t 10m -i 200 > /dev/null &
	/tmp/lockf3 || break
	pkill swap
	wait
done
while pgrep -q swap; do
	pkill swap
done
pkill lockf3
wait
cd $here

while mount | grep -q "on $mntpoint "; do
	umount $mntpoint || sleep 1
done
rm -rf /tmp/lockf3
exit 0
EOF
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#define LOOPS 10000
#define PIDS 100

void
handler(int s __unused)
{
}

void
int_handler(int s __unused)
{
	_exit(0);
}

void
ahandler(int s __unused)
{
	fprintf(stderr, "FAIL timed out\n");
	_exit(1);
}

int
main(void)
{
	int fd, i, j;
	char name[128];
	pid_t pid[PIDS];
	struct sigaction sa;
	int status;

	signal(SIGALRM, ahandler);
	alarm(15 * 60);
	for (i = 0; i < PIDS; i++) {
		sprintf(name, "lock.%06d", getpid());
		if ((fd = open(name, O_CREAT | O_TRUNC | O_RDWR, 0640)) == -1)
			err(1, "open(%s)", name);
		if (lockf(fd, F_LOCK, 0) == -1)
			err(1, "flock 1");

		if ((pid[i] = fork()) == -1)
			err(1, "fork");

		if (pid[i] == 0) {
			memset(&sa, 0, sizeof(sa));
			sa.sa_handler = handler;
			if (sigaction(SIGHUP, &sa, NULL) == -1)
				err(1, "signal");
			sa.sa_handler = int_handler;
			if (sigaction(SIGINT, &sa, NULL) == -1)
				err(1, "signal");

			for (;;) {
				if (lockf(fd, F_LOCK, 0) == -1)
					if (errno != EINTR)
						warn("lockf");
			}
			_exit(0);
		}
		unlink(name);
	}

	usleep(10000);

	for (i = 0; i < LOOPS; i++) {
		for (j = 0; j < PIDS; j++) {
			if (kill(pid[j], SIGHUP) == -1) {
				warn("kill(%d), i = %d, j = %d", pid[j], i, j);
				pid[j] = 0;
			}
		}
	}
	for (j = 0; j < PIDS; j++) {
		if (kill(pid[j], SIGINT) == -1) {
			warn("kill(%d), i = %d, j = %d", pid[j], i, j);
			pid[j] = 0;
		}
	}

	for (j = 0; j < PIDS; j++) {
		if (waitpid(pid[j], &status, 0) == -1)
			err(1, "waitpid(%d)", pid[j]);
	}

	return (0);
}
