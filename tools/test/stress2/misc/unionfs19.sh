#!/bin/sh

#
# Copyright (c) 2024 Peter Holm <pho@FreeBSD.org>
#
# SPDX-License-Identifier: BSD-2-Clause
#

# umount FS with memory mapped file
# "panic: general protection fault" seen:
# https://people.freebsd.org/~pho/stress/log/log0519.txt

[ `id -u ` -ne 0 ] && echo "Must be root!" && exit 1

. ../default.cfg

prog=$(basename "$0" .sh)
here=`pwd`
log=/tmp/$prog.log
md1=$mdstart
md2=$((md1 + 1))
mp1=/mnt$md1
mp2=/mnt$md2

set -e
mdconfig -l | grep -q md$md && mdconfig -d -u $md1
mdconfig -l | grep -q md$u2 && mdconfig -d -u $md2

mdconfig -s 2g -u $md1
newfs $newfs_flags /dev/md$md1 > /dev/null
mdconfig -s 2g -u $md2
newfs $newfs_flags /dev/md$md2 > /dev/null

mkdir -p $mp1 $mp2
mount /dev/md$md1 $mp1
mount /dev/md$md2 $mp2
mount -t unionfs -o noatime $mp1 $mp2
mount | grep -E "$mp1|$mp2"
set +e

export RUNDIR=$mp2/stressX
export runRUNTIME=2m
export LOAD=70
export mmapLOAD=100
export TESTPROGS="testcases/mmap/mmap testcases/swap/swap"

(cd ..; ./testcases/run/run $TESTPROGS > /dev/null 2>&1) & rpid=$!
sleep 5

tail -F -n 0 /var/log/messages > $log & lpid=$!

start=`date +%s`
while [ $((`date +%s` - start)) -lt 120 ]; do
	umount -f $mp2 &&
	    mount -t unionfs -o noatime $mp1 $mp2
	sleep 5
	mount | grep -q unionfs || break
	pgrep -q mmap || break
done
pkill run swap mmap
while pgrep -q swap; do pkill swap; done
wait $rpid

umount $mp2 # The unionfs mount
umount $mp2
umount $mp1

mdconfig -d -u $md1
mdconfig -d -u $md2

kill $lpid && wait $lpid
grep -m 1 "pager read error" $log && s=1 || s=0
rm $log
exit $s
