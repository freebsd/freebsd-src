#!/bin/sh

#
# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) 2022 Peter Holm <pho@FreeBSD.org>
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
# FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
# OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
# HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
# LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
# OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
# SUCH DAMAGE.
#

# A SEEK_HOLE / SEEK_DATA test scenario

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
mdconfig -l | grep -q md$mdstart && mdconfig -d -u $mdstart
mdconfig -s 2g -u $mdstart
newfs -U /dev/md$mdstart > /dev/null
mount /dev/md$mdstart $mntpoint
set +e

file=$mntpoint/file
truncate -s 500m $file
bs=`getconf MIN_HOLE_SIZE $file`
printf "\001" | dd of=$file seek=$((0*bs)) bs=1 count=1 conv=notrunc status=none
printf "\002" | dd of=$file seek=$((2*bs)) bs=1 count=1 conv=notrunc status=none
printf "\003" | dd of=$file seek=$((4*bs)) bs=1 count=1 conv=notrunc status=none
s1=0
s2=0
/tmp/lsholes $file > $log 2>&1; s1=$?

cat $log
cmp -s $exp $log || s2=1

umount $mntpoint
mdconfig -d -u $mdstart
rm -f /tmp/lsholes $exp $log
exit $((s1 + s2))
