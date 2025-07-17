#!/bin/sh

#
# Copyright (c) 2024 Peter Holm <pho@FreeBSD.org>
#
# SPDX-License-Identifier: BSD-2-Clause
#

# No problems seen with this test scenario

[ `id -u ` -ne 0 ] && echo "Must be root!" && exit 1
. ../default.cfg

[ -d /usr/src/tools/test/stress2 ] || exit 0
prog=$(basename "$0" .sh)
log=/tmp/$prog.log
md1=$mdstart
md2=$((md1 + 1))
mp1=/mnt$md1
mp2=/mnt$md2
s=0

mkdir -p $mp1 $mp2
set -e
for i in $mp1 $mp2; do
	mount | grep -q "on $i " && umount -f $i
done
for i in $md1 $md2; do
	mdconfig -l | grep -q md$i && mdconfig -d -u $i
done

mdconfig -a -t swap -s 3g -u $md1
mdconfig -a -t swap -s 3g -u $md2
newfs $newfs_flags -n md$md1 > /dev/null
newfs $newfs_flags -n md$md2 > /dev/null
mount /dev/md$md1 $mp1
mount /dev/md$md2 $mp2
(cd $mp1; cp -a /usr/src/tools/test/stress2 .)
(cd $mp2; cp -a /usr/src/tools/test/stress2 .)
rm $mp1/stress2/testcases/run/run
rm $mp2/stress2/testcases/swap/swap
mount -u -o ro $mp1
mount -t unionfs -o below $mp1 $mp2
rm $mp2/stress2/testcases/mkdir/mkdir
chmod 777 $mp2
set +e

(cd $mp2/stress2; make > /dev/null 2>&1)
export RUNDIR=$mp2/stressX
export runRUNTIME=1m
su $testuser -c "cd $mp2/stress2; ./run.sh vfs.cfg" > /dev/null 2>&1
umount $mp2

while mount | grep -Eq "unionfs.* on $mp2 "; do
	umount $mp2 && break
	sleep 5
done
fsck_ffs -fy /dev/md$md2 > $log 2>&1
grep -Eq "WAS MODIFIED" $log && { cat $log; s=1; }
umount $mp2
umount $mp1
mdconfig -d -u $md1
mdconfig -d -u $md2
rm -f $log
exit $s
