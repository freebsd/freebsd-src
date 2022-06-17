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

# Test for SU problem with "out of inodes" for CREAT

[ `id -u ` -ne 0 ] && echo "Must be root!" && exit 1

. ../default.cfg

here=`pwd`
cd /tmp
sed '1,/^EOF/d' < $here/$0 > linger4.c
mycc -o linger4 -Wall -Wextra -O2 linger4.c
rm -f linger4.c

mount | grep "$mntpoint" | grep -q md$mdstart && umount $mntpoint
mdconfig -l | grep -q md$mdstart &&  mdconfig -d -u $mdstart

mdconfig -a -t swap -s 2g -u $mdstart
[ $# -eq 1 ] && opt="$1"
[ $# -eq 0 ] && opt="$newfs_flags -n"	# No argument == default flag
echo "newfs $opt md$mdstart"
newfs $opt md$mdstart > /dev/null
mount /dev/md$mdstart $mntpoint

cd $mntpoint
chmod 777 $mntpoint

su $testuser -c "/tmp/linger4" ||
    { ls -la $mntpoint; df -i $mntpoint; }

cd $here

while mount | grep -q $mntpoint; do
	umount $mntpoint 2> /dev/null || sleep 1
done
mdconfig -d -u $mdstart
rm -f /tmp/linger4
exit 0
EOF
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#define LOOPS 300
#define NUMBER 80000 / PARALLEL		/* Number of files to use. Max is 131068 */
#define PARALLEL 4
#define TIMEOUT (20 * 60)

void
Creat(int loopno)
{
	int e, fd, i, j;
	char file[128];
	char path[128];
	pid_t pid;

	e = 0;
	pid = getpid();
	sprintf(path, "f%06d", pid);
        if (mkdir(path, 0770) == -1)
                warn("mkdir(%s), %s:%d, loop #%d", path, __FILE__, __LINE__, loopno);
	chdir(path);

	for (j = 0; j < NUMBER; j++) {
		sprintf(file,"p%05d.%05d", pid, j);
		if ((fd = creat(file, 0660)) == -1) {
			if (errno != EINTR) {
				warn("creat(%s). %s:%d, loop #%d", file, __FILE__, __LINE__, loopno);
				e = 1;
				break;
			}
		}
		if (fd != -1 && close(fd) == -1)
			err(2, "close(%d)", j);

	}

	for (i = --j; i >= 0; i--) {
		sprintf(file,"p%05d.%05d", pid, i);
		if (unlink(file) == -1)
			err(3, "unlink(%s)", file);

	}
        chdir ("..");
        if (rmdir(path) == -1)
                err(1, "rmdir(%s), %s:%d, loop #%d", path, __FILE__, __LINE__, loopno);

	_exit(e);
}

int
main()
{
	time_t start;
	int e, i, j, status;

	e = 0;
	start = time(NULL);
	for (j = 0; j < LOOPS; j++) {
		for (i = 0; i < PARALLEL; i++) {
			if (fork() == 0)
				Creat(j);
		}

		for (i = 0; i < PARALLEL; i++) {
			wait(&status);
			e += WEXITSTATUS(status);
		}
		if (e != 0)
			break;
//		sleep(60); /* No problems if this is included */
		if (time(NULL) - start > TIMEOUT) {
			fprintf(stderr, "Timeout.\n");
			e++;
			break;
		}
	}

	return (e);
}
