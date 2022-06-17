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

# Demonstrate rename(2) cache problem, where the original name lingers in the VFS cache.

# Original test scenario by Anton Yuzhaninov <citrin citrin ru>

. ../default.cfg

here=`pwd`
cd /tmp
sed '1,/^EOF/d' < $here/$0 > rename6.c
mycc -o rename6 -Wall -Wextra -O2 rename6.c
rm -f rename6.c
cd $here

mount | grep $mntpoint | grep -q /dev/md && umount -f $mntpoint
mdconfig -l | grep -q md$mdstart &&  mdconfig -d -u $mdstart
mdconfig -a -t swap -s 2g -u $mdstart
newfs $newfs_flags md$mdstart > /dev/null
mount /dev/md$mdstart $mntpoint
chmod 777 $mntpoint

su $testuser -c "cd $mntpoint; /tmp/rename6"

while mount | grep -q md$mdstart; do
	umount $mntpoint || sleep 1
done
mdconfig -d -u $mdstart
rm -f /tmp/rename6
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

void
cleanup()
{
	if (kill(spid, SIGINT) == -1 && errno != ESRCH)
		err(1, "kill(%d)", spid);
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
		}
		usleep(1000);
	}
}

int
main(void)
{
	struct stat sb1, sb2;
	int fd, i;
	char new[128];

	if ((spid = fork()) == 0)
		Stat();

	setproctitle("main");
	atexit(cleanup);
	for (i = 0; i < 20000; i++) {
		sprintf(new, "test.log.%05d", i);
		if ((fd = open(logfile, O_RDWR | O_CREAT | O_TRUNC, 0644)) == -1)
			err(1, "creat(%s)", logfile);
		close(fd);
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
		if (stat(logfile, &sb1) == 0 && stat(new, &sb2) == 0 &&
		    bcmp(&sb1, &sb2, sizeof(sb1)) == 0) {
			fprintf(stderr, "At loop #%d\n", i);
			fprintf(stderr, "%-15s: ino = %ju, nlink = %ju,"
			   " size = %jd\n", logfile, (uintmax_t)sb1.st_ino,
			   (uintmax_t)sb1.st_nlink, sb1.st_blocks);
			fprintf(stderr, "%-15s: ino = %ju, nlink = %ju, "
			    "size = %jd\n", new    , (uintmax_t)sb2.st_ino,
			    (uintmax_t)sb2.st_nlink, sb2.st_blocks);
		}
		unlink(new);
	}

	kill(spid, SIGINT);
	wait(NULL);

	return (0);
}
