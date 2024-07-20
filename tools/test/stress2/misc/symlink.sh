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

# Testing problem with premature disk full problem with symlinks.
# Not seen as of 20210117.

[ `id -u ` -ne 0 ] && echo "Must be root!" && exit 1

. ../default.cfg
dbt=`sysctl -n vfs.dirtybufthresh`
[ $dbt -lt 1000 ] && echo "Note: vfs.dirtybufthresh = $dbt"

D=$diskimage
dd if=/dev/zero of=$D bs=1m count=128 status=none || exit 1

odir=`pwd`
dir=$mntpoint

cd /tmp
sed '1,/^EOF/d' < $odir/$0 > symlink.c
mycc -o symlink -Wall -Wextra symlink.c || exit 1
rm -f symlink.c
cd $odir

mount | grep -q "on $mntpoint " && umount $mntpoint
mdconfig -l | grep md$mdstart > /dev/null &&  mdconfig -d -u $mdstart

mdconfig -a -t vnode -f $D -u $mdstart

tst() {
	local i j k

	cd $dir
	i=`df -ik $mntpoint | tail -1 | awk '{printf "%d\n", ($7 - 500)/2}'`
	[ $i -gt 20000 ] && i=20000

	for k in `jot 3`; do
		for j in `jot 2`; do
			/tmp/symlink $i &
		done
		wait
	done
	cd $odir
}

s=0
for i in "" "-U" "-j"; do
	newfs $i /dev/md$mdstart > /dev/null 2>&1
	[ "$i" = "" ] && tunefs -n disable md$mdstart
	mount /dev/md$mdstart $mntpoint

	t1=`date +%s`
	tst; s=$?
	t2=$((`date +%s` - t1))

	umount -f $mntpoint
	t2=$((`date +%s` - t1))
	    echo "$t2 seconds elapsed for newfs option \"$i\""
	[ $t2 -gt 1000 ] && s=111
done
rm -f /tmp/symlink $D
mdconfig -d -u $mdstart
exit $s
EOF
#include <sys/types.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int
main(int argc __unused, char **argv)
{
	pid_t pid;
	int64_t size;
	int i, j;
	char file[128];

	size = atol(argv[1]);

	pid = getpid();
	for (j = 0; j < size; j++) {
		sprintf(file,"p%05d.%05d", pid, j);
		if (symlink("/mnt/not/there", file) == -1) {
			if (errno != EINTR) {
				warn("symlink(%s)", file);
				printf("break out at %d, errno %d\n", j,
				    errno);
				break;
			}
		}
	}

	for (i = --j; i >= 0; i--) {
		sprintf(file,"p%05d.%05d", pid, i);
		if (unlink(file) == -1)
			err(3, "unlink(%s)", file);

	}

	return (0);
}
