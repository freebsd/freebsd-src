#!/bin/sh

#
# Copyright (c) 2024 Peter Holm <pho@FreeBSD.org>
# Copyright (c) 2026 Jason Harmening <jah@FreeBSD.org>
#
# SPDX-License-Identifier: BSD-2-Clause
#

# unionfs(4) test
# Variation of unionfs7.sh, but with unionfs lookup
# traversing a tmpfs mount.
# No issues seen with this test.

[ `id -u ` -ne 0 ] && echo "Must be root!" && exit 1
. ../default.cfg

mp1=/mnt$mdstart
mp2=/mnt$((mdstart + 1))
mp3=$mp2/unionfs

mkdir -p $mp1 $mp2
set -e
for i in $mp1 $mp2; do
	mount | grep -q "on $i " && umount -f $i
done

mount -o size=4g -t tmpfs dummy $mp1
mount -o size=4g -t tmpfs dummy $mp2
mkdir -p $mp3

mount -t unionfs -o noatime $mp1 $mp3
set +e
export 'INODES=100000'

export CTRLDIR=$mp3/stressX.control
export INCARNATIONS=10
export LOAD=80
export RUNDIR=$mp3/stressX
export runRUNTIME=5m
export rwLOAD=80
export symlinkLOAD=80

export TESTPROGS="
unionfs/stress2/testcases/lockf2/lockf2
unionfs/stress2/testcases/symlink/symlink
unionfs/stress2/testcases/openat/openat
unionfs/stress2/testcases/rw/rw
unionfs/stress2/testcases/fts/fts
unionfs/stress2/testcases/link/link
unionfs/stress2/testcases/lockf/lockf
unionfs/stress2/testcases/creat/creat
unionfs/stress2/testcases/mkdir/mkdir
unionfs/stress2/testcases/rename/rename
unionfs/stress2/testcases/mkfifo/mkfifo
unionfs/stress2/testcases/dirnprename/dirnprename
unionfs/stress2/testcases/dirrename/dirrename
unionfs/stress2/testcases/swap/swap
"

cp -r ../../stress2 $mp3
export TESTPROGS=`echo $TESTPROGS | sed 's/\n/ /g'`

set +e
chmod 777 $mp3
su $testuser -c \
	"(cd $mp2; ./unionfs/stress2/testcases/run/run $TESTPROGS)"

while mount | grep -Eq "on $mp3 .*unionfs"; do
	umount $mp3 && break
	sleep 5
done
umount $mp2
n=`find $mp1/stressX | wc -l`
[ $n -eq 1 ] && s=0 || { find $mp1/stressX -ls | head -12; s=1; }
umount $mp1
exit $s
