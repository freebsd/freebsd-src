#!/bin/sh

#
# Copyright (c) 2018 Dell EMC Isilon
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

# Testing with links longer than 120 characters, which is not stored in the
# inode but will be placed in its own data fragment.

# "panic: softdep_deallocate_dependencies: dangling deps" seen with SU:
# https://people.freebsd.org/~pho/stress/log/symlink4.txt
# Fixed by r327821

[ `id -u ` -ne 0 ] && echo "Must be root!" && exit 1

. ../default.cfg

dir=/tmp
odir=`pwd`
cd $dir
sed '1,/^EOF/d' < $odir/$0 > $dir/symlink4.c
mycc -o symlink4 -Wall -Wextra -O0 -g symlink4.c || exit 1
rm -f symlink4.c
cd $odir

set -e
mount | grep "on $mntpoint " | grep -q /dev/md && umount -f $mntpoint
[ -c /dev/md$mdstart ] &&  mdconfig -d -u $mdstart
mdconfig -a -t swap -s 1g -u $mdstart
newfs $newfs_flags md$mdstart > /dev/null
mount /dev/md$mdstart $mntpoint
set +e

cd $mntpoint
i=`df -ik $mntpoint | tail -1 | awk '{printf "%d\n", ($7 - 500)/2}'`
/tmp/symlink4 $i
s=$?
[ -f symlink4.core -a $s -eq 0 ] &&
    { ls -l symlink4.core; mv symlink4.core /tmp; s=1; }
cd $odir

for i in `jot 6`; do
	mount | grep -q "on $mntpoint " || break
	umount $mntpoint && break || sleep 10
done
[ $i -eq 6 ] && exit 1
mdconfig -d -u $mdstart
rm -rf $dir/symlink4
exit $s

EOF
#include <sys/types.h>
#include <sys/mount.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/sysctl.h>

#include <unistd.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int
main(int argc, char **argv)
{
	int64_t size;
	pid_t pid;
	int i, j;
	char file[128], link[256];

	if (argc != 2)
		errx(1, "Usage: %s <links>", argv[0]);
	size = atol(argv[1]);
	strcpy(link, "/mnt/not/there/");
	for (i = 15; i < 200; i++)
		link[i] = '1';
	link[++i] = 0;

	pid = getpid();
	for (j = 0; j < size; j++) {
		sprintf(file,"p%05d.%05d", pid, j);
		if (symlink(link, file) == -1) {
			if (errno != EINTR) {
				warn("symlink(%s, %s)", link, file);
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
