#!/bin/sh

#
# Copyright (c) 2013 EMC Corp.
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

# Regression test for r218019: "panic: oof, we didn't get our fd"

# Page fault seen:
# https://people.freebsd.org/~pho/stress/log/kostik895.txt
# Caused by r300792 + r300793.
# Fixed by r301580.

[ `id -u ` -ne 0 ] && echo "Must be root!" && exit 1

. ../default.cfg

odir=`pwd`
cd /tmp
sed '1,/^EOF/d' < $odir/$0 > setuid.c
rm -f /tmp/setuid
mycc -o setuid -Wall -Wextra -O2 -g setuid.c -static || exit 1
rm -f setuid.c

mount | grep $mntpoint | grep -q /dev/md && umount -f $mntpoint
mdconfig -l | grep -q md$mdstart &&  mdconfig -d -u $mdstart

mdconfig -a -t swap -s 2g -u $mdstart || exit 1
newfs $newfs_flags md$mdstart > /dev/null
mount /dev/md$mdstart $mntpoint
chmod 777 $mntpoint
mkdir $mntpoint/root

cp /tmp/setuid $mntpoint/root/nop
chown nobody:nobody $mntpoint/root/nop
chmod 4755 $mntpoint/root/nop

chmod 777 $mntpoint/root

echo "Expect Abort trap"
./setuid $mntpoint/root 1
./setuid $mntpoint/root 2
./setuid $mntpoint/root 3
./setuid $mntpoint/root 4
./setuid $mntpoint/root 5
./setuid $mntpoint/root 6
./setuid $mntpoint/root 7

while mount | grep "on $mntpoint " | grep -q /dev/md; do
	umount $mntpoint || sleep 1
done
mdconfig -d -u $mdstart
rm -f /tmp/setuid /tmp/nop
exit
EOF
#include <sys/types.h>
#include <pwd.h>
#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int
main(int argc, char **argv)
{
	char *av[2];
	int fd;

	if (argc == 1)
		return (0);

	if (chroot(argv[1]) != 0)
		err(1, "chroot(%s)", argv[1]);
	fd = atoi(argv[2]);

	if (fd & 1) {
		fprintf(stderr, "Close fd 0 ");
		close(0);
	}
	if (fd & 2) {
		fprintf(stderr, "Close fd 1 ");
		close(1);
	}
	if (fd & 4) {
		fprintf(stderr, "Close fd 2\n");
		close(2);
	} else
		fprintf(stderr, "\n");

	if (chdir("/") != 0)
		err(1, "chdir");
	av[0] = "/nop";
	av[1] = 0;
	if (execve(av[0], av, NULL) == -1)
		err(1, "execve");

	return (0);
}
