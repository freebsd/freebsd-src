#!/bin/sh

#
# SPDX-License-Identifier: BSD-2-Clause-FreeBSD
#
# Copyright (c) 2021 Peter Holm
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

# Sparse files test.
# Delayed write observed.
# Related to:
# Bug 256205 - ZFS: data corruption with SEEK_HOLE/SEEK_DATA on dirty files ...

[ `id -u ` -ne 0 ] && echo "Must be root!" && exit 1
kldstat -v | grep -q zfs.ko  || { kldload zfs.ko; loaded=1; } ||
    exit 0

. ../default.cfg

here=`pwd`
cd /tmp
mycc -o /tmp/lsholes -Wall -Wextra -O0 -g $here/../tools/lsholes.c || exit 1

log1=/tmp/zfs13.sh.1.log
log2=/tmp/zfs13.sh.2.log
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

cd $mp1
dd if=/etc/passwd conv=notrunc oseek=32k of=output status=none
ls -ls | grep output | tee $log1
/tmp/lsholes output
sleep 10
ls -ls | grep output | tee $log2
/tmp/lsholes output
cd $here

zfs umount stress2_tank/test
zfs destroy -r stress2_tank
zpool destroy stress2_tank
mdconfig -d -u $u1
mdconfig -d -u $u2
cmp -s $log1 $log2
s=$?
rm -f /tmp/lsholes $log1 $log2
[ $loaded ] && kldunload zfs.ko
exit $s
