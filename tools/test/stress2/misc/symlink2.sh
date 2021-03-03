#!/bin/sh

#
# Copyright (c) 2008 Peter Holm <pho@FreeBSD.org>
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

# Testing problem with buffer cache inconsistency

[ `id -u ` -ne 0 ] && echo "Must be root!" && exit 1

. ../default.cfg

D=$diskimage
dd if=/dev/zero of=$D bs=1m count=10 status=none || exit 1

odir=`pwd`
dir=$mntpoint

cd /tmp
sed '1,/^EOF/d' < $odir/$0 > symlink2.c
mycc -o symlink2 -Wall symlink2.c
rm -f symlink2.c
cd $odir

mount | grep "$mntpoint" | grep md$mdstart > /dev/null && umount $mntpoint
mdconfig -l | grep md$mdstart > /dev/null &&  mdconfig -d -u $mdstart

mdconfig -a -t vnode -f $D -u $mdstart

for i in "" "-U"; do
	[ "$i" = "-U" -a "$newfs_flags" != "-U" ] && continue
	echo "newfs $i /dev/md$mdstart"
	newfs $i /dev/md$mdstart > /dev/null 2>&1
	mount /dev/md$mdstart $mntpoint
	mkdir $mntpoint/dir

	/tmp/symlink2 $mntpoint/dir/link

	ls -l $mntpoint/dir > /dev/null 2>&1
	if [ $? -ne 0 ]; then
		set -x
		ls -l $mntpoint/dir
		umount $mntpoint
		mount /dev/md$mdstart $mntpoint
		ls -l $mntpoint/dir
		set +x
	fi

	umount -f $mntpoint
done
rm -f /tmp/symlink2 $D
mdconfig -d -u $mdstart
exit
EOF
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <pthread.h>
#include <err.h>
#include <string.h>
#include <sys/wait.h>

static char *path;

int
main(int argc, char **argv)
{
	int i, n;
	pid_t p;
	char buf[128];

	path = argv[1];
	for (i = 0; i < 100; i++) {
		if ((p = fork()) == 0) {
			if ((n = readlink(path, buf, sizeof(buf) -1)) < 0) {
				for (i = 0; i < 60; i++) {
					sleep(1);
					if ((n = readlink(path, buf, sizeof(buf) -1)) > 0) {
						break;
					}
				}
			}
			if (n < 0)
				err(1, "readlink(%s). %s:%d", path, __FILE__, __LINE__);
			exit(0);
		}
	}
	(void) unlink(path);
	sleep(2);
	if (symlink("1234", path) < 0)
		err(1, "symlink(%s, %s)", path, "1234");

	for (i = 0; i < 100; i++) {
		if (wait(&n) == -1)
			err(1, "wait(), %s:%d", __FILE__, __LINE__);
	}

	return (0);
}
