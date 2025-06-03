#!/bin/sh

#
# Copyright (c) 2024 Peter Holm <pho@FreeBSD.org>
#
# SPDX-License-Identifier: BSD-2-Clause
#

# A SEEK_HOLE / SEEK_DATA test scenario, variation of tmpfs24.sh

# A regression test for "40c1672e886b - main - swap_pager: fix
# seek_data with invalid first page"

[ `id -u ` -ne 0 ] && echo "Must be root!" && exit 1

. ../default.cfg

prog=$(basename "$0" .sh)
exp=/tmp/$prog.exp
here=`pwd`
log=/tmp/$prog.log

cc -o /tmp/lsholes -Wall -Wextra -O2 $here/../tools/lsholes.c | exit 1
cat > $exp <<EXP
Min hole size is 4096, file size is 524288000.
data #1 @ 0, size=4096)
hole #2 @ 4096, size=4096
data #3 @ 8192, size=4096)
hole #4 @ 12288, size=4096
data #5 @ 16384, size=4096)
hole #6 @ 20480, size=524267520
EXP

set -eu
mount -t tmpfs dummy $mntpoint
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
/tmp/lsholes $file > $log 2>&1 || s1=1

cmp -s $exp $log || { s2=2; sdiff $exp $log; }

$here/../testcases/swap/swap -t 2m -i 20 -h > /dev/null &
sleep 10
cp $file $copy
while pkill swap; do :; done
wait
cmp $file $copy || { echo "copy error"; s3=4; }

umount $mntpoint
rm -f /tmp/lsholes $exp $log
exit $((s1 + s2 + s3))
