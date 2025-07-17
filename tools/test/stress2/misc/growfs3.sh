#!/bin/sh

#
# Copyright (c) 2025 Peter Holm <pho@FreeBSD.org>
#
# SPDX-License-Identifier: BSD-2-Clause
#

. ../default.cfg
[ `id -u ` -ne 0 ] && echo "Must be root!" && exit 1

set -eu
prog=$(basename "$0" .sh)
log=/tmp/$prog.log
mount | grep "on $mntpoint " | grep -q /dev/md && umount -f $mntpoint
[ -c /dev/md$mdstart ] &&  mdconfig -d -u $mdstart
mdconfig -a -t swap -s 32g -u $mdstart
/sbin/gpart create -s GPT md$mdstart > /dev/null
/sbin/gpart add -t freebsd-ufs -s 2g -a 4k md$mdstart > /dev/null
set +e

newfs_flags=$(echo "-O1" "-O2" "-U" "-j" | awk -v N=`jot -r 1 1 4` '{print $N}')
echo "newfs $newfs_flags md${mdstart}p1"
newfs $newfs_flags md${mdstart}p1 > /dev/null
[ "$newfs_flags" = "-O2" ] &&
    tunefs -n disable md${mdstart}p1 > /dev/null 2>&1
mount /dev/md${mdstart}p1 $mntpoint
cp -r /usr/include $mntpoint/inc1
umount $mntpoint

gpart resize -i 1 -s 31g -a 4k md$mdstart
growfs -y md${mdstart}p1 > /dev/null

mount /dev/md${mdstart}p1 $mntpoint
cp -r /usr/include $mntpoint/inc2
umount $mntpoint
fsck -fy /dev/md${mdstart}p1 > $log 2>&1; s=$?
grep -q "WAS MODIFIED" $log && { cat $log; s=1; }
rm -f $log
mdconfig -d -u $mdstart
exit $s
