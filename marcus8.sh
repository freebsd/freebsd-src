#!/bin/sh

#
# Copyright (c) 2025 Peter Holm <pho@FreeBSD.org>
#
# SPDX-License-Identifier: BSD-2-Clause
#

[ `id -u ` -ne 0 ] && echo "Must be root!" && exit 1

# Run with marcus.cfg on a 5g swap backed MD with UFS non SU fs.
# Check for non empty file system after test.

. ../default.cfg

set -u
mount | grep $mntpoint | grep -q /dev/md && umount -f $mntpoint
mdconfig -l | grep -q md$mdstart &&  mdconfig -d -u $mdstart
mdconfig -a -t swap -s 4g -u $mdstart
newfs_flags=""	# With SU this test runs out of disk space
newfs $newfs_flags md$mdstart > /dev/null
tunefs -n disable md$mdstart	# Remove the default SU flag
mount /dev/md$mdstart $mntpoint
chmod 777 $mntpoint

export runRUNTIME=5m
export CTRLDIR=$mntpoint/stressX.control
export RUNDIR=$mntpoint/stressX

su $testuser -c 'cd ..; ./run.sh marcus.cfg'

nb=`find $RUNDIR | wc -l`
[ $nb -gt 1 ] && { find $RUNDIR -ls | head -12; s=1; } || s=0
n=0
while mount | grep $mntpoint | grep -q /dev/md; do
	umount $mntpoint || sleep 1
	[ $((n += 1)) -gt 300 ] && { echo FAIL; exit 1; }
done
checkfs /dev/md$mdstart; s2=$?
mdconfig -d -u $mdstart
exit $((s + s2))
