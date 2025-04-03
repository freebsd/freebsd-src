#!/bin/sh

#
# Copyright (c) 2025 Peter Holm <pho@FreeBSD.org>
#
# SPDX-License-Identifier: BSD-2-Clause
#

# Variation of nullfs25.sh, using cp(1) (copy_file_range())

# Page fault in vn_copy_file_range() seen:
# https://people.freebsd.org/~pho/stress/log/log0497.txt
# Fixed by: 23210f538a00

[ `id -u ` -ne 0 ] && echo "Must be root!" && exit 1
. ../default.cfg

set -u
mounts=4	# Number of parallel scripts
: ${nullfs_srcdir:=$mntpoint}
: ${nullfs_dstdir:=$mntpoint}
prog=$(basename "$0" .sh)
CONT=/tmp/$prog.continue

mount | grep $mntpoint | grep -q /dev/md && umount -f $mntpoint
mdconfig -l | grep -q md$mdstart &&  mdconfig -d -u $mdstart
mdconfig -a -t swap -s 1g -u $mdstart
newfs $newfs_flags md$mdstart > /dev/null
mount /dev/md$mdstart $mntpoint
chmod 777 $mntpoint
(cd $mntpoint; jot 500 | xargs touch)
(cd ../testcases/swap; ./swap -t 5m -i 20 > /dev/null) &

for i in `jot $mounts $mdstart`; do
	[ ! -d ${nullfs_dstdir}$i ] && mkdir ${nullfs_dstdir}$i
	mount | grep -q " ${nullfs_dstdir}$i " &&
	    umount ${nullfs_dstdir}$i
done

# Start the parallel tests
touch $CONT
for i in `jot $mounts $mdstart`; do
	while [ -f $CONT ]; do
		cp /etc/group ${nullfs_dstdir}$i > \
		    /dev/null 2>&1
	done &
	# The test: Parallel mount and unmount
	start=`date +%s`
	(
		while [ $((`date +%s` - start))  -lt 300 ]; do
			mount_nullfs $nullfs_srcdir ${nullfs_dstdir}$i > \
			    /dev/null 2>&1
			opt=$([ `jot -r 1 0 1` -eq 0 ] && echo "-f")
			while mount | grep -q ${nullfs_dstdir}$i; do
				umount $opt ${nullfs_dstdir}$i > \
				    /dev/null 2>&1
			done
		done
		rm -f $CONT
	) &
done
while [ -f $CONT ] ; do sleep 1; done
while pgrep -q swap; do pkill swap; done
wait

for i in `jot $mounts`; do
	umount ${nullfs_dstdir}$i > /dev/null 2>&1
done
n=0
while mount | grep $mntpoint | grep -q /dev/md; do
	umount $mntpoint || sleep 1
	[ $((n += 1)) -gt 300 ] && { echo FAIL; exit 1; }
done
mdconfig -d -u $mdstart
exit 0
