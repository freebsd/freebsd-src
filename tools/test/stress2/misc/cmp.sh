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

# Cross mount test of mkdir(2).
# Page fault seen:
# http://people.freebsd.org/~pho/stress/log/cmp.txt
# Fixed by r275347

[ `id -u ` -ne 0 ] && echo "Must be root!" && exit 1

. ../default.cfg

here=`pwd`
cd /tmp
sed '1,/^EOF/d' < $here/$0 > cmp.c
mycc -o cmp -Wall -Wextra -O2 -g cmp.c || exit 1
rm -f cmp.c

mount | grep $mntpoint | grep -q /dev/md && umount -f $mntpoint
[ -c /dev/md$mdstart ] && mdconfig -d -u $mdstart

mdconfig -a -t swap -s 2g -u $mdstart || exit 1
# Don't use SU due to bogus "out of inodes" messages.
newfs md$mdstart > /dev/null
mount /dev/md$mdstart $mntpoint
chmod 777 $mntpoint

daemon sh -c "(cd $here/../testcases/swap; ./swap -t 5m -i 20 -h -l 100)" \
    > /dev/null 2>&1
sleep 1
su $testuser -c "/tmp/cmp $mntpoint" &

while kill -0 $! 2>/dev/null; do
	umount -f $mntpoint &&
	    mount /dev/md$mdstart $mntpoint
	chmod 777 $mntpoint
	sleep .1
done
wait

while mount | grep $mntpoint | grep -q /dev/md; do
	umount $mntpoint || sleep 1
done
mdconfig -d -u $mdstart
[ -d "$mntpoint" ] && (cd $mntpoint && find . -delete)

# tmpfs
mount -t tmpfs tmpfs $mntpoint
chmod 777 $mntpoint

su $testuser -c "/tmp/cmp $mntpoint" &

while kill -0 $! 2>/dev/null; do
	umount -f $mntpoint &&
	    mount -t tmpfs tmpfs $mntpoint
	chmod 777 $mntpoint
	sleep .1
done
pkill -9 swap
wait

while pkill -9 swap; do
	:
done > /dev/null 2>&1
while mount | grep $mntpoint | grep -q tmpfs; do
	umount $mntpoint || sleep 1
done
[ -d "$mntpoint" ] && (cd $mntpoint && find . -delete)
rm -f /tmp/cmp
exit 0
EOF
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/wait.h>

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define LOOPS 160
#define PARALLEL 16

int nbc, nbd;
char *dir;

void
tmkdir(void)
{
	int i, j;
	char d[MAXPATHLEN + 1], name[MAXPATHLEN + 1];

	setproctitle(__func__);

	i = 0;
	snprintf(name, sizeof(name), "%s/d1.%05d", dir, getpid());
	if (mkdir(name, 0755) == -1) {
		if (errno != ENAMETOOLONG && errno != ENOENT &&
		    errno != EBUSY && errno != EACCES && errno != EPERM)
			warn("mkdir(%s)", name);
		_exit(0);
	}
	for (;;) {
		snprintf(d, sizeof(d), "/%d", i++);
		strncat(name, d, sizeof(name) - 1);
		if (mkdir(name, 0755) == -1) {
			if (errno != ENAMETOOLONG && errno != ENOENT &&
			    errno != EBUSY && errno != EACCES && errno != EPERM)
				warn("mkdir(%s)", name);
			i--;
			break;
		}
		nbc++;
	}

	while (i >= 0) {
		snprintf(name, sizeof(name), "%s/d1.%05d", dir, getpid());
		for (j = 0; j < i; j++) {
			snprintf(d, sizeof(d), "/%d", j);
			strncat(name, d, sizeof(name) - 1);
		}
		if (rmdir(name) == -1) {
			if (errno != ENOTEMPTY && errno != ENOENT && errno !=
			    EBUSY)
				warn("rmdir(%s)", name);
		} else
			nbd++;
		i--;
	}
#if defined(TEST)
	if (nbc == 0)
		fprintf(stderr, "FAIL nbc = %d, nbd = %d\n", nbc, nbd);
#endif
	_exit(0);
}

int
main(int argc, char **argv)
{
	int i, j;

	if (argc != 2) {
		fprintf(stderr, "Usage: %s <full path to dir>", argv[0]);
		exit(1);
	}
	dir = argv[1];

	for (j = 0; j < LOOPS; j++) {
		for (i = 0; i < PARALLEL; i++) {
			if (fork() == 0)
				tmkdir();
		}
		for (i = 0; i < PARALLEL; i++)
			wait(NULL);
	}

	return(0);
}
