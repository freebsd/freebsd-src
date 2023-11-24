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

# Rename scenario from kern/156545 by Konstantin,
# konstantin malov kaspersky com

# On 8.2-STABLE fsck reports
# "xxx IS AN EXTRANEOUS HARD LINK TO DIRECTORY yyy"
# Fixed by r220986

[ `id -u ` -ne 0 ] && echo "Must be root!" && exit 1

. ../default.cfg

mount | grep -q "$mntpoint" && umount $mntpoint
mdconfig -l | grep -q $mdstart &&  mdconfig -d -u $mdstart

mdconfig -a -t swap -s 1g -u $mdstart

newfs $newfs_flags md$mdstart > /dev/null
mount /dev/md$mdstart $mntpoint

here=`pwd`
cd /tmp
sed '1,/^EOF/d' < $here/$0 > rename5.c
mycc -o rename5 -Wall -Wextra -O2 rename5.c
rm -f rename5.c

cd $mntpoint

/tmp/rename5

cd $here
rm -f /tmp/rename5

while mount | grep -q md$mdstart; do
	umount $mntpoint || sleep 1
done

checkfs /dev/md$mdstart; s=$?

mdconfig -d -u $mdstart
exit $s
EOF
#include <sys/stat.h>
#include <sys/wait.h>

#include <err.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#define N 1000
#define RUNTIME (5 * 60)

void
test(void)
{
	pid_t pid;
	int i;
	char from[128], to[128];

	pid = getpid();
	for (i = 0; i < N; i++) {
		snprintf(from, sizeof(from), "src/dir.%06d", i);
		snprintf(to  , sizeof(to),   "src/dir.%06d.%06d", i, pid);
		(void)rename(from, to);
	}
	_exit(0);
}

int
main()
{
	time_t start;
	int fd, i;
	char dir[128], file[128];

	start = time(NULL);
	while (time(NULL) - start < RUNTIME) {
		if (mkdir("src", 0700) == -1)
			err(1, "mkdir(src)");

		for (i = 0; i < N; i++) {
			snprintf(dir, sizeof(dir), "src/dir.%06d", i);
			if (mkdir(dir, 0700) == -1)
				err(1, "mkdir(%s)", dir);
			snprintf(file, sizeof(file), "%s/meta", dir);
			if ((fd = open(file, O_RDWR | O_CREAT, 0600)) < 0)
				err(1, "open(%s)", file);
			close(fd);
			snprintf(file, sizeof(file), "%s/data", dir);
			if ((fd = open(file, O_RDWR | O_CREAT, 0600)) < 0)
				err(1, "open(%s)", file);
			close(fd);
		}

		for (i = 0; i < 2; i++) {
			if (fork() == 0)
				test();
		}
		for (i = 0; i < 2; i++)
			wait(NULL);

		system("rm -rf src > /dev/null 2>&1");
	}

        return (0);
}
