#!/bin/sh

#
# Copyright (c) 2012 Peter Holm <pho@FreeBSD.org>
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

# Variation of rename6.sh. Cache problem of "to" file name seen.

. ../default.cfg

here=`pwd`
cd /tmp
sed '1,/^EOF/d' < $here/$0 > rename9.c
mycc -o rename9 -Wall -Wextra -O2 rename9.c
rm -f rename9.c
cd $here

mount | grep $mntpoint | grep -q /dev/md && umount -f $mntpoint
mdconfig -l | grep -q md$mdstart &&  mdconfig -d -u $mdstart
mdconfig -a -t swap -s 2g -u $mdstart
newfs $newfs_flags md$mdstart > /dev/null
mount /dev/md$mdstart $mntpoint
rm -rf $mntpoint/.snap
chmod 777 $mntpoint

(while true; do ls -lRi $mntpoint > /dev/null 2>&1; done) &
su $testuser -c "cd $mntpoint; /tmp/rename9"
kill $! > /dev/null 2>&1
wait
ls -ilR $mntpoint | egrep -v "^total "

while mount | grep -q md$mdstart; do
	umount $mntpoint || sleep 1
done
mdconfig -d -u $mdstart
rm -f /tmp/rename9
exit
EOF
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <sched.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

pid_t spid;
char *fromFile = "fromFile.log";
char toFile[128];

void
cleanup()
{
	kill(spid, SIGINT);
}

static void
statFrom()
{
	struct stat sb;

	setproctitle("Stat");
	for (;;) {
		stat(fromFile, &sb);
	}
}

int
main(void)
{
	struct stat fb, tb, fa, ta;
	int fd, i;

	if ((spid = fork()) == 0)
		statFrom();

	setproctitle("main");
	atexit(cleanup);
	for (i = 0;i < 100000; i++) {
		bzero(&fb, sizeof(fb));
		bzero(&tb, sizeof(tb));
		bzero(&fa, sizeof(fa));
		bzero(&ta, sizeof(ta));

		if ((fd = open(fromFile, O_RDWR | O_CREAT | O_TRUNC, 0644))
		    == -1)
			err(1, "creat(%s)", fromFile);
		close(fd);

		sprintf(toFile, "toFile.log.%05d", i);
		if ((fd = open(toFile, O_RDWR | O_CREAT | O_TRUNC, 0644))
		    == -1)
			err(1, "creat(%s)", toFile);
		write(fd, "xxx", 3);
		close(fd);

		stat(fromFile, &fb);
		stat(toFile, &tb);
		if (rename(fromFile, toFile) == -1)
			warn("rename(%s, %s)", fromFile, toFile);
		stat(fromFile, &fa);
		if (stat(toFile, &ta) == -1)
			err(1, "stat(%s)", toFile);

		if (tb.st_ino == ta.st_ino) {
			fprintf(stderr, "FAIL: old and new \"To\" inode "
			    "number is identical\n");
			fprintf(stderr, "stat() before the rename():\n");
			fprintf(stderr,
			    "%-16s: ino = %4ju, nlink = %ju, size = %jd\n",
			    fromFile, (uintmax_t)fb.st_ino, (uintmax_t)fb.st_nlink,
			    fb.st_blocks);
			fprintf(stderr,
			    "%-16s: ino = %4ju, nlink = %ju, size = %jd\n",
			    toFile, (uintmax_t)tb.st_ino, (uintmax_t)tb.st_nlink,
			    tb.st_blocks);
			fprintf(stderr, "\nstat() after the rename():\n");
			if (fa.st_ino != 0)
				fprintf(stderr,
				    "%-16s: ino = %4ju, nlink = %ju, size = "
				    "%jd\n", fromFile, (uintmax_t)fa.st_ino,
				    (uintmax_t)fa.st_nlink, fa.st_blocks);
			fprintf(stderr,
			    "%-16s: ino = %4ju, nlink = %ju, size = %jd\n",
			    toFile, (uintmax_t)ta.st_ino, (uintmax_t)ta.st_nlink,
			    ta.st_blocks);
			kill(spid, SIGINT);
			exit(1);
		}
		unlink(toFile);
	}

	kill(spid, SIGINT);
	wait(NULL);

	return (0);
}
