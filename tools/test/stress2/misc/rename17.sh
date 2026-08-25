#!/bin/sh

#
# Copyright (c) 2026 Peter Holm <pho@FreeBSD.org>
#
# SPDX-License-Identifier: BSD-2-Clause
#

# Test rename(2) over nfs.
# This needs to be in /etc/exports: /mnt	-maproot=root 127.0.0.1

# "panic: Lock rename not exclusively locked @ ../../../kern/vfs_subr.c:5878" seen

. ../default.cfg

[ `id -u ` -ne 0 ] && echo "Must be root!" && exit 1
grep -q $mntpoint /etc/exports ||
	{ echo "$mntpoint missing from /etc/exports"; exit 0; }
ps -x | grep -v grep | grep -q nfsd || { echo "nfsd is not running"; exit 0; }

set -u
mp1=$mntpoint
mp2=${mntpoint}2

mount | grep "$mp2" | grep nfs > /dev/null && umount -f $mp2
mount | grep "$mp1" | grep -q /md && umount -f $mp1
mdconfig -l | grep -q $mdstart  &&  mdconfig -d -u $mdstart

set -e
mdconfig -a -t swap -s 2g -u $mdstart
newfs $newfs_flags /dev/md$mdstart > /dev/null
mount /dev/md$mdstart $mp1
set +e

mkdir $mp1/stressX $mp1/stressX.control
chmod 777 $mp1/stressX $mp1/stressX.control

[ ! -d $mp2 ] &&  mkdir $mp2
chmod 777 $mp2

mount -t nfs -o tcp -o retrycnt=3 -o soft -o rw \
    127.0.0.1:$mp1 $mp2

sleep 1
export CTRLDIR=$mp2/stressX.control
export RUNDIR=$mp2/stressX
export renameLOAD=100
export runRUNTIME=5m
export TESTPROGS="
testcases/rename/rename
testcases/swap/swap"
su -m $testuser -c 'cd ..; ./testcases/run/run $TESTPROGS'

umount -f $mp2 > /dev/null 2>&1
umount -f $mp1 > /dev/null 2>&1
mdconfig -d -u $mdstart
exit 0
