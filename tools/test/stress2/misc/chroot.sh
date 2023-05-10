#!/bin/sh

#
# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) 2021 Peter Holm <pho@FreeBSD.org>
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

# Test scenario input by: "Patrick Sullivan" sulli00777@gmail.com

# Bug 253593
# "panic: ldvp 0xffff... fl 0x1 dvp 0xffff... fl 0 flags 0x34048144" seen.
# https://people.freebsd.org/~pho/stress/log/log0087.txt

[ `id -u ` -ne 0 ] && echo "Must be root!" && exit 1

. ../default.cfg

mount | grep $mntpoint | grep -q /dev/md && umount -f $mntpoint
mdconfig -l | grep -q md$mdstart &&  mdconfig -d -u $mdstart

odir=`pwd`
cd /tmp
sed '1,/^EOF/d' < $odir/$0 > chroot.c
rm -f /tmp/chroot
mycc -o chroot -Wall -Wextra -O0 -g chroot.c -static || exit 1
rm -f chroot.c

mdconfig -a -t swap -s 10m -u $mdstart || exit 1
newfs -n $newfs_flags md$mdstart > /dev/null
mount /dev/md$mdstart $mntpoint
mkdir -p $mntpoint/root/dir $mntpoint/jail $mntpoint/dev
mount -t nullfs $mntpoint/root $mntpoint/jail
mount -t devfs null $mntpoint/dev
mv /tmp/chroot $mntpoint/root

chroot $mntpoint/jail ./chroot &
sleep .5
mv $mntpoint/root/dir $mntpoint
wait

umount $mntpoint/dev
umount $mntpoint/jail
while mount | grep "on $mntpoint " | grep -q /dev/md; do
	umount $mntpoint || sleep 1
done
mdconfig -d -u $mdstart
exit
EOF
#include <sys/types.h>
#include <err.h>
#include <stdio.h>
#include <unistd.h>

int
main(void)
{
	if (chdir("dir") == -1)
		err(1, "chdir() #1");
	sleep(2);
	fprintf(stderr, "cwd is %s\n", getwd(NULL));
}
