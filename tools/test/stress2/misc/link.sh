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

# Deadlock seen for file systems which suspend writes on unmount, such as
# UFS and tmpfs.
# http://people.freebsd.org/~pho/stress/log/link.txt
# Fixed by r272130

[ `id -u ` -ne 0 ] && echo "Must be root!" && exit 1

. ../default.cfg

here=`pwd`
cd /tmp
sed '1,/^EOF/d' < $here/$0 > link.c
mycc -o link -Wall -Wextra -O2 -g link.c || exit 1
rm -f link.c

mount | grep $mntpoint | grep -q /dev/md && umount -f $mntpoint
[ -c /dev/md$mdstart ] && mdconfig -d -u $mdstart

mdconfig -a -t swap -s 1g -u $mdstart || exit 1
newfs $newfs_flags md$mdstart > /dev/null
mount /dev/md$mdstart $mntpoint

daemon sh -c "(cd $here/../testcases/swap; ./swap -t 5m -i 20 -h -l 100)" \
    > /dev/null 2>&1
/tmp/link $mntpoint > /dev/null 2>&1 &

for i in `jot 100`; do
	umount -f $mntpoint &&
	    mount /dev/md$mdstart $mntpoint
	sleep .1
done
pkill -9 link
wait

while mount | grep $mntpoint | grep -q /dev/md; do
	umount $mntpoint || sleep 1
done
mdconfig -d -u $mdstart

# tmpfs
mount -t tmpfs tmpfs $mntpoint

/tmp/link $mntpoint > /dev/null 2>&1 &

for i in `jot 100`; do
	umount -f $mntpoint &&
	    mount -t tmpfs tmpfs $mntpoint
	sleep .1
done
pkill -9 link swap
wait

while pkill -9 swap; do
	:
done > /dev/null 2>&1
while mount | grep $mntpoint | grep -q tmpfs; do
	umount $mntpoint || sleep 1
done
[ -d "$mntpoint" ] && (cd $mntpoint && find . -delete)
rm -f /tmp/link
exit 0
EOF
#include <sys/param.h>
#include <sys/wait.h>

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <time.h>
#include <stdlib.h>
#include <unistd.h>

#define PARALLEL 16
#define RUNTIME 300

time_t start;
char *dir;

void
trename(void)
{
	int fd;
	char name1[MAXPATHLEN + 1];
	char name2[MAXPATHLEN + 1];

	setproctitle(__func__);
	snprintf(name1, sizeof(name1), "%s/r1.%05d", dir, getpid());
	snprintf(name2, sizeof(name2), "%s/r2.%05d", dir, getpid());
	if ((fd = open(name1, O_RDWR | O_CREAT, 0644)) == -1)
		err(1, "open(%s)", name1);
	close(fd);

	while (time(NULL) - start < RUNTIME) {
		if (rename(name1, name2) == -1) {
			if (errno == ENOENT) {
				if ((fd = open(name1, O_RDWR | O_CREAT, 0644)) == -1)
					err(1, "open(%s)", name1);
				close(fd);
				continue;
			} else
				warn("link(%s, %s)", name1, name2);
		}
		if (rename(name2, name1) == -1)
			warn("link(%s, %s)", name2, name1);
	}
	unlink(name1);
	unlink(name2);
	_exit(0);
}

void
tlink(void)
{
	int fd;
	char name1[MAXPATHLEN + 1];
	char name2[MAXPATHLEN + 1];

	setproctitle(__func__);
	snprintf(name1, sizeof(name1), "%s/f1.%05d", dir, getpid());
	snprintf(name2, sizeof(name2), "%s/f2.%05d", dir, getpid());
	if ((fd = open(name1, O_RDWR | O_CREAT, 0644)) == -1)
		err(1, "open(%s)", name1);
	close(fd);

	while (time(NULL) - start < RUNTIME) {
		unlink(name2);
		if (link(name1, name2) == -1) {
			if (errno == ENOENT) {
				if ((fd = open(name1, O_RDWR | O_CREAT, 0644)) == -1)
					err(1, "open(%s)", name1);
				close(fd);
				continue;
			} else
				warn("link(%s, %s)", name1, name2);
		}
	}
	unlink(name1);
	unlink(name2);
	_exit(0);
}

int
main(int argc, char **argv)
{
	int i, type;

	if (argc != 2)
		errx(1, "Usage: %s <full path to dir>", argv[0]);
	dir = argv[1];
	type = arc4random() % 2; /* test either link() or rename() */

	start = time(NULL);
	for (i = 0; i < PARALLEL; i++) {
		if (type == 0 && fork() == 0)
			tlink();
		if (type == 1 && fork() == 0)
			trename();
	}
	for (i = 0; i < PARALLEL; i++)
		wait(NULL);

	return(0);
}
