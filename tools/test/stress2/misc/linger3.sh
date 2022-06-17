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

# Test for SU problem with false EMLINK

[ `id -u ` -ne 0 ] && echo "Must be root!" && exit 1

. ../default.cfg

here=`pwd`
cd /tmp
sed '1,/^EOF/d' < $here/$0 > linger3.c
mycc -o linger3 -Wall -Wextra -O2 linger3.c
rm -f linger3.c

mount | grep "$mntpoint" | grep -q md$mdstart && umount $mntpoint
mdconfig -l | grep -q md$mdstart &&  mdconfig -d -u $mdstart

mdconfig -a -t swap -s 2g -u $mdstart
[ $# -eq 1 ] && opt="$1"
[ $# -eq 0 ] && opt=$newfs_flags	# No argument == default flag
echo "newfs $opt md$mdstart"
newfs $opt md$mdstart > /dev/null
mount /dev/md$mdstart $mntpoint

cd $mntpoint
chmod 777 $mntpoint

su $testuser -c "/tmp/linger3"

cd $here

while mount | grep -q $mntpoint; do
	umount $mntpoint 2> /dev/null || sleep 1
done
mdconfig -d -u $mdstart
rm -f /tmp/linger3
exit 0
EOF
#include <sys/types.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#define PARALLEL 4
#define ND ((32767 / PARALLEL) - 2)

static pid_t p;

void
setup(int loop)
{
	int d;
	char name[128];

	for (d = 0; d < ND; d++) {
		snprintf(name, sizeof(name), "d%06d.%03d", p, d);
		if (mkdir(name, 00700) == -1)
			err(1, "mkdir(%s) @ loop #%d", name, loop);
	}
}

void
prune(int loop)
{
	int d;
	char name[128];

	for (d = 0; d < ND; d++) {
		snprintf(name, sizeof(name), "d%06d.%03d", p, d);
		if (rmdir(name) == -1)
			err(1, "rmdir(%s) @ loop #%d", name, loop);
	}
}

int
main()
{
	int i, j;
	int linger = 0;

	if (getenv("LINGER"))
		linger = atoi(getenv("LINGER"));

	for (i = 0; i < PARALLEL; i++) {
		if (fork() == 0) {
			p = getpid();
			for (j = 0; j < 10; j++) {
				setup(j);
				prune(j);
				if (linger != 0)
					sleep(linger);
			}
			_exit(0);
		}
	}

	for (i = 0; i < PARALLEL; i++)
		wait(NULL);

	return (0);
}
