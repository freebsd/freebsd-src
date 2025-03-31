#!/bin/sh

#
# Copyright (c) 2024 Peter Holm <pho@FreeBSD.org>
#
# SPDX-License-Identifier: BSD-2-Clause
#

# Copy from nullfs over zfs to nullfs over ufs
# Test scenario description by: mjguzik

# Page fault seen:
# https://people.freebsd.org/~pho/stress/log/log0498.txt

[ `id -u ` -ne 0 ] && echo "Must be root!" && exit 1
[ `sysctl -n kern.kstack_pages` -lt 4 ] && exit 0

. ../default.cfg

set -u
kldstat -v | grep -q zfs.ko  || { kldload zfs.ko ||
    exit 0; loaded=1; }

u1=$mdstart
u2=$((u1 + 1))
u3=$((u2 + 1))
mp0=/stress2_tank/test		# zfs mount
mp1=$mntpoint			# nullfs of zfs
mp2=$mntpoint$mdstart		# ufs
mp3=$mntpoint$((mdstart + 1))	# nullfs of ufs
mkdir -p $mp2 $mp3

mdconfig -l | grep -q md$u1 && mdconfig -d -u $u1
mdconfig -l | grep -q md$u2 && mdconfig -d -u $u2

mdconfig -s 2g -u $u1
mdconfig -s 2g -u $u2

zpool list | egrep -q "^stress2_tank" && zpool destroy stress2_tank
[ -d /stress2_tank ] && rm -rf /stress2_tank
zpool create stress2_tank raidz md$u1 md$u2
zfs create ${mp0#/}

mount | grep -q $mp1 && umount -f $mp1
mount -t nullfs $mp0 $mp1

mdconfig -a -t swap -s 1g -u $u3
newfs $newfs_flags /dev/md$u3 > /dev/null
mount /dev/md$u3 $mp2
mount -t nullfs $mp2 $mp3

dd if=/dev/zero of=$diskimage bs=1m count=50 status=none
cp $diskimage $mp1
cp $mp1/diskimage $mp3
rm -f $diskimage

umount $mp3
umount $mp2
mdconfig -d -u $u3

while mount | grep -q "on $mntpoint "; do
	umount $mntpoint && break
	sleep 1
done

zfs umount ${mp0#/}
zfs destroy -r stress2_tank
zpool destroy stress2_tank

mdconfig -d -u $u2
mdconfig -d -u $u1
set +u
[ -n "$loaded" ] && kldunload zfs.ko
exit 0
