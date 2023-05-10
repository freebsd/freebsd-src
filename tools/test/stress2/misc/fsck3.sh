#!/bin/sh

#
# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) 2018 Dell EMC Isilon
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

# fsck_ffs test.
# Test scenario broken superblock, use backup SB.

. ../default.cfg
set -e
u1=$mdstart
u2=$((mdstart + 1))
mp1=${mntpoint}$u1
mp2=${mntpoint}$u2
mkdir -p $mp1 $mp2
log=$mp1/fsck3.sh.log
diskimage=$mp1/diskimage

max=$((10 * 1024 * 1024))
[ "$newfs_flags" = "-j" ] &&
    max=$((20 * 1024 * 1024))

set -e
mount | grep "on $mp1 " | grep -q /dev/md && umount -f $mp1
[ -c /dev/md$u1 ] && mdconfig -d -u $u1
mdconfig -a -t swap -s 1g -u $u1
newfs $newfs_flags /dev/md$u1 > /dev/null
mount /dev/md$u1 $mp1

[ -c /dev/md$u2 ] && mdconfig -d -u $u2
dd if=/dev/zero of=$diskimage bs=$max count=1 status=none
mdconfig -a -t vnode -f $diskimage -u $u2
backups=`newfs -N $newfs_flags md$u2 | grep -A1 "super-block backups" | \
    tail -1 | sed 's/,//g'`
newfs $newfs_flags md$u2 > /dev/null
sblock=`dumpfs md$u2 | grep -m1 "superblock location" | awk '{print $3}'`
mount /dev/md$u2 $mp2 || s=100
touch $mp2/file
umount $mp2
set +e

s=0
fsck_ffs -y /dev/md$u2 > $log 2>&1
r=$?
for i in $backups; do
	dd if=/dev/random of=$diskimage oseek=$sblock bs=1 count=8 \
	    conv=notrunc status=none
	fsck_ffs -y /dev/md$u2 > $log 2>&1
	r=$?
	echo "fsck_ffs -b $i -y /dev/md$u2"
	fsck_ffs -b $i -y /dev/md$u2 > $log 2>&1
	r=$?
	mount /dev/md$u2 $mp2 || s=100
	[ -f $mp2/file ] || { echo "$mp2/file not found"; s=101; }
	umount $mp2
done

mdconfig -d -u $u2
[ -f fsck_ffs.core ] && { ls -l fsck_ffs.core; s=102; }

umount $mp1
mdconfig -d -u $u1
rm -f $diskimage
exit $s
