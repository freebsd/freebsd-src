#!/bin/sh

#
# Copyright (c) 2025 Peter Holm <pho@FreeBSD.org>
#
# SPDX-License-Identifier: BSD-2-Clause
#

# msdosfs disk image fuzz test.
# No problems seen

. ../default.cfg

[ `id -u ` -ne 0 ] && echo "Must be root!" && exit 1

cc -o /tmp/flip -Wall -Wextra -O2 ../tools/flip.c || exit 1

set -eu
u1=$mdstart
u2=$((mdstart + 1))
mp1=${mntpoint}$u1
mp2=${mntpoint}$u2
mkdir -p $mp1 $mp2
prog=$(basename "$0" .sh)
backup=/tmp/$prog.sh.diskimage.`date +%Y%m%dT%H%M%S`
cap=$((32 * 1024))	# Only fuzz the first 32k
log=$mp1/$prog.sh.log
diskimage=$mp1/msdos20.sh.diskimage

set +e
mount | grep "on $mp2 " | grep -q /dev/md && umount -f $mp2
mount | grep "on $mp1 " | grep -q /dev/md && umount -f $mp1
[ -c /dev/md$u2 ] && mdconfig -d -u $u2
[ -c /dev/md$u1 ] && mdconfig -d -u $u1
mdconfig -a -t swap -s 2g -u $u1
newfs -U /dev/md$u1 > /dev/null
mount /dev/md$u1 $mp1

size=32m
type=`jot -r 1 1 3`
[ $type -eq 3 ] && size=260m
[ -c /dev/md$u2 ] && mdconfig -d -u $u2
dd if=/dev/zero of=$diskimage bs=$size count=1 status=none
mdconfig -a -t vnode -f $diskimage -u $u2
[ $type -eq 1 ] && newfs_msdos -F 12         /dev/md$u2 > /dev/null 2>&1
[ $type -eq 2 ] && newfs_msdos -F 16         /dev/md$u2 > /dev/null 2>&1
[ $type -eq 3 ] && newfs_msdos -F 32 -b 4096 /dev/md$u2 > /dev/null 2>&1

mount -t msdosfs /dev/md$u2 $mp2 || { echo "Initial mount of type $type failed"; exit 1; }
if [ -d /usr/include/sys ]; then
	mkdir $mp2/sys
	cp /usr/include/sys/elf_common.h $mp2/sys
	cp /usr/include/sys/soundcard.h  $mp2/sys
	cp /usr/include/sys/sysproto.h   $mp2/sys
fi
umount $mp2

cd $mp1
start=`date +%s`
nn=0
s=0
while [ $((`date +%s` - start)) -lt 240 ]; do
	mount -t msdosfs /dev/md$u2 $mp2 2>/dev/null || { s=1; break; }
	ls -lR $mp2 > /dev/null 2>&1 ||  { s=2; break; }
	rm -rf $mp2/* > /dev/null 2>&1 || { s=3; break; }
	touch $mp2/`jot -rc 8 a z | tr -d '\n'` || { s=4; break; }
	while mount | grep -q "on $mp2 "; do umount $mp2; done
	echo * | grep -q core && { s=5; break; }
	sync
	mdconfig -d -u $u2
	/tmp/flip -n 10 -s $cap $diskimage
	cp $diskimage $backup
	fsync $backup
	sync
	mdconfig -a -t vnode -f $diskimage -u $u2
	nn=$((nn + 1))
done
#echo "Exit after $nn loops on a type $type MSDOS FS with code $s"
mount | grep -q "on $mp2 " && umount $mp2
mdconfig -d -u $u2 || exit 1

echo * | grep -q core && { ls -l *.core; cp $log /tmp; exit 106; } ||
cd /tmp
umount $mp1
mdconfig -d -u $u1
rm -f /tmp/flip $backup
exit 0
