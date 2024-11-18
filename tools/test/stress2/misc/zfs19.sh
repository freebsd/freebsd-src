#!/bin/sh

#
# Copyright (c) 2024 Peter Holm <pho@FreeBSD.org>
#
# SPDX-License-Identifier: BSD-2-Clause
#

# Hunt for "vm_fault: pager read error, pid 99058 (mmap)"

[ `id -u ` -ne 0 ] && echo "Must be root!" && exit 1
kldstat -v | grep -q zfs.ko  || { kldload zfs.ko; loaded=1; } ||
    exit 0

. ../default.cfg

prog=$(basename "$0" .sh)
here=`pwd`
log=/tmp/$prog.log
mp1=/stress2_tank/test
u1=$mdstart
u2=$((u1 + 1))

set -e
mdconfig -l | grep -q md$u1 && mdconfig -d -u $u1
mdconfig -l | grep -q md$u2 && mdconfig -d -u $u2

mdconfig -s 2g -u $u1
mdconfig -s 2g -u $u2

zpool list | egrep -q "^stress2_tank" && zpool destroy stress2_tank
[ -d /stress2_tank ] && rm -rf /stress2_tank
zpool create stress2_tank md$u1 md$u2
zfs create stress2_tank/test
set +e

export RUNDIR=/stress2_tank/test/stressX
export runRUNTIME=2m
export LOAD=70
export mmapLOAD=100
export TESTPROGS="testcases/mmap/mmap testcases/swap/swap"

(cd ..; ./testcases/run/run $TESTPROGS > /dev/null 2>&1) & rpid=$!
sleep 5

tail -F -n 0 /var/log/messages > $log & lpid=$!

start=`date +%s`
while [ $((`date +%s` - start)) -lt 120 ]; do
	zfs umount -f stress2_tank/test &&
	    zfs mount stress2_tank/test
	sleep 5
	zfs list | grep -q /stress2_tank/test || break
	pgrep -q mmap || break
done
pkill run swap mmap
while pgrep -q swap; do pkill swap; done
wait $rpid

zfs umount stress2_tank/test
zfs destroy -r stress2_tank
zpool destroy stress2_tank

mdconfig -d -u $u1
mdconfig -d -u $u2
[ -n "$loaded" ] && kldunload zfs.ko

kill $lpid && wait $lpid
grep -m 1 "pager read error" $log && s=1 || s=0
rm $log
s=0	# This is an expected behavior for zfs
exit $s
