#!/bin/sh

#
# Copyright (c) 2024 Peter Holm <pho@FreeBSD.org>
#
# SPDX-License-Identifier: BSD-2-Clause
#

# unionfs(8) test
# Variation of unionfs7.sh, but with tmpfs

# "mkdir: rmdir(d17) Directory not empty" seen.

[ `id -u ` -ne 0 ] && echo "Must be root!" && exit 1
. ../default.cfg

mp1=/mnt$mdstart
mp2=/mnt$((mdstart + 1))
mkdir -p $mp1 $mp2
set -e
for i in $mp1 $mp2; do
	mount | grep -q "on $i " && umount -f $i
done

mount -o size=4g -t tmpfs dummy $mp1
mount -o size=4g -t tmpfs dummy $mp2

mount -t unionfs -o noatime $mp1 $mp2
set +e
export 'INODES=100000'

export CTRLDIR=$mp2/stressX.control
export INCARNATIONS=10
export LOAD=80
export RUNDIR=$mp2/stressX
export runRUNTIME=5m
export rwLOAD=80
export symlinkLOAD=80

export TESTPROGS="
testcases/lockf2/lockf2
testcases/symlink/symlink
testcases/openat/openat
testcases/rw/rw
testcases/fts/fts
testcases/link/link
testcases/lockf/lockf
testcases/creat/creat
testcases/mkdir/mkdir
testcases/rename/rename
testcases/mkfifo/mkfifo
testcases/dirnprename/dirnprename
testcases/dirrename/dirrename
testcases/swap/swap
"

cp -r ../../stress2 $mp2
export TESTPROGS=`echo $TESTPROGS | sed 's/\n/ /g'`

set +e
chmod 777 $mp2
su $testuser -c \
	"(cd $mp2/stress2; ./testcases/run/run $TESTPROGS)"

while mount | grep -Eq "on $mp2 .*unionfs"; do
	umount $mp2 && break
	sleep 5
done
umount $mp2
n=`find $mp1/stressX | wc -l`
[ $n -eq 1 ] && s=0 || { find $mp1/stressX -ls | head -12; s=1; }
umount $mp1
exit $s
