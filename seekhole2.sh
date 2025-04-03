#!/bin/sh

#
# Copyright (c) 2025 Peter Holm <pho@FreeBSD.org>
#
# SPDX-License-Identifier: BSD-2-Clause
#

# A SEEK_HOLE / SEEK_DATA test scenario, FFS version

[ `id -u ` -ne 0 ] && echo "Must be root!" && exit 1

. ../default.cfg

prog=$(basename "$0" .sh)
exp=/tmp/$prog.exp
here=`pwd`
log=/tmp/$prog.log

cc -o /tmp/lsholes -Wall -Wextra -O2 $here/../tools/lsholes.c | exit 1
cat > $exp <<EXP
Min hole size is 32768, file size is 524288000.
data #1 @ 0, size=32768)
hole #2 @ 32768, size=32768
data #3 @ 65536, size=32768)
hole #4 @ 98304, size=32768
data #5 @ 131072, size=32768)
hole #6 @ 163840, size=524091392
data #7 @ 524255232, size=32768)
hole #8 @ 524288000, size=0
EXP

set -eu
mount | grep "on $mntpoint " | grep -q /dev/md && umount -f $mntpoint
[ -c /dev/md$mdstart ] &&  mdconfig -d -u $mdstart
mdconfig -a -t swap -s 2g -u $mdstart
newfs $newfs_flags md$mdstart > /dev/null
mount /dev/md$mdstart $mntpoint
set +e

file=$mntpoint/file
copy=$mntpoint/copy
truncate -s 500m $file
bs=`getconf MIN_HOLE_SIZE $file`
printf "\001" | dd of=$file seek=$((0*bs)) bs=1 count=1 conv=notrunc status=none
printf "\002" | dd of=$file seek=$((2*bs)) bs=1 count=1 conv=notrunc status=none
printf "\003" | dd of=$file seek=$((4*bs)) bs=1 count=1 conv=notrunc status=none
s1=0
s2=0
s3=0
/tmp/lsholes $file > $log 2>&1; s1=$?

sdiff -s $exp $log || s2=1

$here/../testcases/swap/swap -t 2m -i 20 -h > /dev/null &
sleep 10
cp $file $copy
while pkill swap; do :; done
wait
cmp $file $copy || { echo "copy error"; s3=1; }

umount $mntpoint
mdconfig -d -u $mdstart
rm -f /tmp/lsholes $exp $log
exit $((s1 + s2 + s3))
