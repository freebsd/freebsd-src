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

# Demonstrate rename(2) cache problem for tmpfs(4). Fixed in r226987.
# Variation of rename6.sh

. ../default.cfg

here=`pwd`
cd /tmp
sed '1,/^EOF/d' < $here/$0 > tmpfs8.c
mycc -o tmpfs8 -Wall -Wextra -O2 tmpfs8.c
rm -f tmpfs8.c
cd $here

mount | grep $mntpoint | grep -q tmpfs && umount -f $mntpoint
mount -t tmpfs tmpfs $mntpoint
chmod 777 $mntpoint

su $testuser -c "cd $mntpoint; /tmp/tmpfs8"

while mount | grep $mntpoint | grep -q tmpfs; do
	umount $mntpoint || sleep 1
done
rm -f /tmp/tmpfs8
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
const char *logfile = "test.log";
char new[128];

void
cleanup()
{
	kill(spid, SIGINT);
}

static void
Stat()
{
	struct stat sb;
	int i;

	setproctitle("Stat");
	for (;;) {
		for (i = 0; i < 1000; i++) {
			stat(logfile, &sb);
			stat(new, &sb);
		}
//		usleep(1000);
		usleep(100);
	}
}

int
main(void)
{
	struct stat sb1, sb2, sb3;
	int fd, i, r1, r2, r3;

	if ((spid = fork()) == 0)
		Stat();

	setproctitle("main");
	atexit(cleanup);
	for (i = 0; i < 200000; i++) {
		bzero(&sb1, sizeof(sb1));
		bzero(&sb2, sizeof(sb2));
		if ((fd = open(logfile, O_RDWR | O_CREAT | O_TRUNC, 0644)) == -1)
			err(1, "creat(%s)", logfile);
		close(fd);

		sprintf(new, "test.log.%05d", i);
		if ((fd = open(new, O_RDWR | O_CREAT | O_TRUNC, 0644)) == -1)
			err(1, "creat(%s)", new);
		write(fd, "xxx", 3);
		close(fd);
		if ((r3 = stat(new, &sb3)) == -1)
			err(1, "stat(%s)", new);
#if 1
		if (rename(logfile, new) == -1)
			warn("rename(%s, %s)", logfile, new);
#else
		/* No cache problem is seen */
		if (link(logfile, new) == -1)
			err(1, "link(%s, %s)", logfile, new);
		if (unlink(logfile) == -1)
			err(1, "unlink(%s)", logfile);
#endif
		/*
		 * stat() for logfile and new will be identical sometimes,
		 * but only when Stat() is running.
		 */
		r1 = stat(logfile, &sb1);
		r2 = stat(new, &sb2);
		if (r1 == 0 && r2 == 0 &&
		    bcmp(&sb1, &sb2, sizeof(sb1)) == 0) {
			fprintf(stderr, "FAIL 1\n");
			fprintf(stderr, "%-15s: ino = %4ju, nlink = %ju, "
			    "size  = %jd\n", logfile, (uintmax_t)sb1.st_ino,
			   (uintmax_t)sb1.st_nlink, sb1.st_blocks);
			fprintf(stderr, "%-15s: ino = %4ju, nlink = %ju, "
			    "size = %jd\n", new    , (uintmax_t)sb2.st_ino,
			    (uintmax_t)sb2.st_nlink, sb2.st_blocks);
		}
		if (bcmp(&sb2, &sb3, sizeof(sb2)) == 0) {
			fprintf(stderr, "Old to file is lingering\n");
		}
		if (sb2.st_ino == sb3.st_ino) {
			fprintf(stderr, "FAIL 2\n");
			if (r1 == 0)
				fprintf(stderr,
				    "sb1: %-15s: ino = %4ju, nlink = %ju, "
				    "size = %jd\n", logfile,
				    (uintmax_t)sb1.st_ino,
				    (uintmax_t)sb1.st_nlink, sb1.st_blocks);
			if (r2 == 0)
				fprintf(stderr,
				    "sb2: %-15s: ino = %4ju, nlink = %ju, "
				    "size = %jd\n", new, (uintmax_t)sb2.st_ino,
				    (uintmax_t)sb2.st_nlink, sb2.st_blocks);
			if (r3 == 0)
				fprintf(stderr,
				    "sb3: %-15s: ino = %4ju, nlink = %ju, "
				    "size = %jd\n", new, (uintmax_t)sb3.st_ino,
				    (uintmax_t)sb3.st_nlink, sb3.st_blocks);
			exit(1);
		}
		unlink(new);
	}

	kill(spid, SIGINT);
	wait(NULL);

	return (0);
}
