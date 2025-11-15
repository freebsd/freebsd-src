#/bin/sh

#
# Copyright (c) 2025 Peter Holm <pho@FreeBSD.org>
#
# SPDX-License-Identifier: BSD-2-Clause
#

# tmpfs version of rsync.sh

[ `id -u` -ne 0 ] && echo "Must be root!" && exit 1

. ../default.cfg

[ -f "`which rsync`" ] || exit 0
[ -d /usr/src/sys ]    || exit 0

set -eu
prog=$(basename "$0" .sh)
log=/tmp/$prog.log
mount | grep "on $mntpoint " | grep -q /dev/md && umount -f $mntpoint
[ -c /dev/md$mdstart ] &&  mdconfig -d -u $mdstart
mount -t tmpfs dummy $mntpoint
set +e

mkdir -p $mntpoint/usr/src
rsync -avrq /usr/src/sys $mntpoint/usr/src; s=$?
if [ $s -eq 0 ]; then
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
rm -f $log
exit $s
