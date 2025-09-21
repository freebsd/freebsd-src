#/bin/sh

#
# Copyright (c) 2025 Peter Holm <pho@FreeBSD.org>
#
# SPDX-License-Identifier: BSD-2-Clause
#

[ `id -u` -ne 0 ] && echo "Must be root!" && exit 1

. ../default.cfg

[ -f "`which rsync`" ] || exit 0
[ -d /usr/src/sys ]    || exit 0

set -eu
prog=$(basename "$0" .sh)
log=/tmp/$prog.log
mount | grep "on $mntpoint " | grep -q /dev/md && umount -f $mntpoint
[ -c /dev/md$mdstart ] &&  mdconfig -d -u $mdstart
mdconfig -a -t swap -s 15g -u $mdstart
newfs $newfs_flags md$mdstart > /dev/null
mount /dev/md$mdstart $mntpoint
set +e

mkdir -p $mntpoint/usr/src
rsync -avrq /usr/src/sys $mntpoint/usr/src; s=$?
if [ $s -eq 0 ]; then
	(cd $mntpoint; umount $mntpoint > /dev/null 2>&1)	# sync
	rsync -avrq  /usr/src/sys $mntpoint/usr/src; s=$?
fi

if [ $s -eq 0 ]; then
	diff --no-dereference -rq /usr/src/sys $mntpoint/usr/src/sys > $log; s=$?
	[ $s -ne 0 ] &&
	    { echo "/usr/src $mntpoint/usr/src differ!"; head -10 $log; }
fi

while mount | grep -q "on $mntpoint "; do
	umount $mntpoint || break
	sleep 1
done
if [ $s -eq 0 ]; then
	fsck_ffs -fy /dev/md$mdstart > $log 2>&1; s=$?
	grep -Eq "WAS MODIFIED" $log && { cat $log; s=1; }
fi
mdconfig -d -u $mdstart
rm -f $log
exit $s
