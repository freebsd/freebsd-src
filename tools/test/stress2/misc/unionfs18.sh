#!/bin/sh

#
# Copyright (c) 2024 Peter Holm <pho@FreeBSD.org>
#
# SPDX-License-Identifier: BSD-2-Clause
#

# Simple unionfs(8) + tmpfs test

# "rmdir: d2: Directory not empty" seen.

[ `id -u ` -ne 0 ] && echo "Must be root!" && exit 1
. ../default.cfg

mp1=/mnt$mdstart
mp2=/mnt$((mdstart + 1))
mkdir -p $mp1 $mp2
set -e
for i in $mp1 $mp2; do
	mount | grep -q "on $i " && umount -f $i
done

md1=$mdstart
md2=$((md1 + 1))
mp1=/mnt$md1
mp2=/mnt$md2
mkdir -p $mp1 $mp2
for i in $mp1 $mp2; do
	mount | grep -q "on $i " && umount -f $i
done

if [ $# -eq 0 ]; then
	echo "tmpfs version"
	mount -o size=4g -t tmpfs dummy $mp1
	mount -o size=4g -t tmpfs dummy $mp2
else
	echo "UFS version"
	for i in $md1 $md2; do
		mdconfig -l | grep -q md$i && mdconfig -d -u $i
	done
	mdconfig -a -t swap -s 4g -u $md1
	mdconfig -a -t swap -s 4g -u $md2
	newfs $newfs_flags -n md$md1 > /dev/null
	newfs $newfs_flags -n md$md2 > /dev/null
	mount /dev/md$md1 $mp1
	mount /dev/md$md2 $mp2
fi

mount -t unionfs -o noatime $mp1 $mp2
set +e

N=3	# Tree depth
here=`pwd`
cd $mp2
mkdir dir; cd dir
for j in `seq 1 $N`; do
	mkdir d$j && cd d$j
done
for j in `seq $N 1`; do
	cd .. && rmdir d$j
done
cd ..
rmdir dir || { s=1; find dir -ls; }
cd $here

while mount | grep -Eq "on $mp2 .*unionfs"; do
	umount $mp2 && break
	sleep 5
done
umount $mp2
umount $mp1
exit $s
