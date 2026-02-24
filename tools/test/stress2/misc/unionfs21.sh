#!/bin/sh

# "panic: unionfs_lock_lvp: vnode is locked but should not be" seen

# Test scenario based on:
# Bug 275871 - [unionfs] [nullfs] [zfs] corrupt filesystem
# Bug 172334 - unionfs permits recursive union mounts; causes panic quickly

. ../default.cfg

prog=$(basename "$0" .sh)
here=`pwd`
log=/tmp/$prog.log
md1=$mdstart
md2=$((md1 + 1))
mp1=/mnt$md1
mp2=/mnt$md2

set -eu
mdconfig -l | grep -q md$md1 && mdconfig -d -u $md1
mdconfig -l | grep -q md$md2 && mdconfig -d -u $md2

mdconfig -s 2g -u $md1
newfs $newfs_flags /dev/md$md1 > /dev/null
mdconfig -s 2g -u $md2
newfs $newfs_flags /dev/md$md2 > /dev/null

mkdir -p $mp1 $mp2
mount /dev/md$md1 $mp1
mount /dev/md$md2 $mp2
set +e
mount -t unionfs -o noatime $mp1 $mp2
mount -t unionfs -o noatime $mp2 $mp1

ls -lr $mp1 $mp2 > /dev/null # triggers the panic

umount $mp2 # The unionfs mount
umount $mp2
umount $mp1

mdconfig -d -u $md1
mdconfig -d -u $md2
rm -f /tmp/$prog
exit 0
