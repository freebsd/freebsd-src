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

# After a few runs this will happen:
# $ umount /mnt
# umount: unmount of /mnt failed: Device busy
# $ umount -f /mnt
# $

. ../default.cfg

here=`pwd`
cd /tmp
sed '1,/^EOF/d' < $here/$0 > rename7.c
mycc -o rename7 -Wall -Wextra -O2 rename7.c || exit
rm -f rename7.c
cd $here

mount | grep $mntpoint | grep -q /dev/md && umount -f $mntpoint
mdconfig -l | grep -q md$mdstart &&  mdconfig -d -u $mdstart
mdconfig -a -t swap -s 2g -u $mdstart
newfs $newfs_flags md$mdstart > /dev/null
mount /dev/md$mdstart $mntpoint
chmod 777 $mntpoint

su $testuser -c "cd $mntpoint; /tmp/rename7 || echo FAIL"

for i in `jot 10`; do
	mount | grep -q md$mdstart  && \
		umount $mntpoint && mdconfig -d -u $mdstart && break
done
if mount | grep -q md$mdstart; then
	echo "Test failed"
	exit 1
fi
rm -f /tmp/rename7
exit 0
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

const char *logfile = "test.log";
pid_t wpid, spid;

void
r1(void)
{
	int i;
	struct stat sb1, sb2;

	for (i = 0; i < 800000; i++) {
		rename(logfile, "r1");
		if (stat("r1", &sb1) == 0 && stat("r2", &sb2) == 0 &&
		    bcmp(&sb1, &sb2, sizeof(sb1)) == 0) {
			fprintf(stderr, "r1 and r2 are identical after rename(%s, \"r1\")\n", logfile);
			system("ls -ail");
			_exit(1);
		}
	}
	_exit(0);
}

void
r2(void)
{
	int i;
	struct stat sb1, sb2;

//	_exit(0); /* No problems with only r1 running */
	for (i = 0; i < 800000; i++) {
		rename(logfile, "r2");
		if (stat("r1", &sb1) == 0 && stat("r2", &sb2) == 0 &&
		    bcmp(&sb1, &sb2, sizeof(sb1)) == 0) {
			usleep(10000);
			fprintf(stderr, "r1 and r2 are identical after rename(%s, \"r2\")\n", logfile);
			system("ls -ail");
			_exit(1);
		}
	}
	_exit(0);
}
int
main(void)
{
	pid_t wpid, spid;
	int e, fd, i, status;

	if ((wpid = fork()) == 0)
		r1();
	if ((spid = fork()) == 0)
		r2();

	setproctitle("main");
	e = 0;

	for (i = 0; i < 800000; i++) {
		if ((fd = open(logfile, O_RDWR | O_CREAT | O_TRUNC, 0644)) == -1)
			warn("creat(%s)", logfile);
		close(fd);
	}

	kill(wpid, SIGINT);
	kill(spid, SIGINT);
	wait(&status);
	e += WEXITSTATUS(status);
	wait(&status);
	e += WEXITSTATUS(status);

	return (e);
}
