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

# "panic: Freeing unused sector 79510 22 ff800000" seen.
# http://people.freebsd.org/~pho/stress/log/msdos5.txt
# FS corruption seen: http://people.freebsd.org/~pho/stress/log/msdos5-2.txt
# Fixed by r333693.

[ `id -u ` -ne 0 ] && echo "Must be root!" && exit 1
[ -x /sbin/mount_msdosfs ] || exit

. ../default.cfg

mycc -o /tmp/fstool -Wall -Wextra -O2 ../tools/fstool.c || exit 1

cd /tmp
mount | grep "on $mntpoint " | grep -q /dev/md && umount -f $mntpoint
mdconfig -l | grep -q md$mdstart &&  mdconfig -d -u $mdstart
mdconfig -a -t swap -s 3g -u $mdstart
gpart create -s bsd md$mdstart > /dev/null
gpart add -t freebsd-ufs md$mdstart > /dev/null
part=a
newfs_msdos -F 32 -b 8192 /dev/md${mdstart}$part > /dev/null 2>&1
mount_msdosfs -m 777 /dev/md${mdstart}$part $mntpoint

for i in `jot 5`; do
	(mkdir $mntpoint/test$i; cd $mntpoint/test$i; /tmp/fstool -l -f 400 -n 200 -s ${i}k)
done

sleep 1
rm -rf $mntpoint/* || echo FAIL

while mount | grep -q "$mntpoint "; do
	umount $mntpoint || sleep 1
done
rm -f /tmp/fstool
