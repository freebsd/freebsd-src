#!/bin/sh

#
# Copyright (c) 2025 Peter Holm <pho@FreeBSD.org>
#
# SPDX-License-Identifier: BSD-2-Clause
#

# Variation of the datamove.sh, using MSDOSFS

# No problems seen

[ `id -u ` -ne 0 ] && echo "Must be root!" && exit 1

. ../default.cfg

here=`pwd`
prog=$(basename "$0" .sh)
cd /tmp
sed '1,/^EOF/d' < $here/datamove.sh > $prog.c
mycc -o $prog -Wall -Wextra -O2 -g $prog.c
rm -f $prog.c

set -eu
mount | grep "on $mntpoint " | grep -q /dev/md && umount -f $mntpoint
[ -c /dev/md$mdstart ] &&  mdconfig -d -u $mdstart
mdconfig -a -t swap -s 2g -u $mdstart
newfs_msdos -F 32 -b 8192 /dev/md$mdstart 2> /dev/null
#mount -t msdosfs /dev/md$mdstart $mntpoint
mount_msdosfs -m 777 /dev/md$mdstart $mntpoint
set +e

$here/../testcases/swap/swap -t 5m -i 100 -h &
for i in `jot 5`; do
	su $testuser -c "cd $mntpoint; /tmp/$prog"
done
mv /tmp/$prog $mntpoint
for i in `jot 5`; do
	mkdir -p $mntpoint/datamove.dir.$i
	cd $mntpoint/datamove.dir.$i
	$mntpoint/$prog &
done
pkill swap
wait
while mount | grep -q $mntpoint; do
	umount -f $mntpoint > /dev/null 2>&1
done
mdconfig -d -u $mdstart

exit 0
