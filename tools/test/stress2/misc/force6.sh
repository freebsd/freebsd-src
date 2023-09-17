#!/bin/sh

#
# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) 2020 Peter Holm
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

# "mdconfig -o force" test scenario.
# Verify file integrity after a fsync(1) followed by a forced umount
# No problems seen.

[ `id -u ` -ne 0 ] && echo "Must be root!" && exit 1

. ../default.cfg

mount | grep $mntpoint | grep -q /dev/md && umount -f $mntpoint
mdconfig -l | grep -q md$mdstart &&  mdconfig -d -u $mdstart
dd if=/dev/zero of=$diskimage bs=1m count=1k status=none || exit 1
mdconfig -a -t vnode -f $diskimage -u $mdstart
flags=$newfs_flags
[ `jot -r 1 0 1` -eq 1 ] && flags="-j"
echo "newfs $flags md$mdstart"
newfs $flags md$mdstart > /dev/null 2>&1
mount /dev/md$mdstart $mntpoint

file=$mntpoint/file
dd if=/dev/random of=$file bs=1k count=`jot -r 1 1 1024` status=none
s1=`cat $mntpoint/file | md5`
fsync $file

while mdconfig -l | grep -q md$mdstart; do
	mdconfig -d -u $mdstart -o force || sleep 1
done

n=0
while mount | grep $mntpoint | grep -q /dev/md; do
	umount $mntpoint || sleep 1
	[ $((n += 1)) -gt 300 ] && { echo FAIL; exit 1; }
done

mdconfig -a -t vnode -f $diskimage -u $mdstart
fsck_ffs -Rfy /dev/md$mdstart > /dev/null; s=$?
if [ $s -eq 0 ]; then
	mount /dev/md$mdstart $mntpoint
	[ ! -f $file ] &&
	{ echo "Lost $file"; s=111; }
	if [ $s -eq 0 ]; then
		s2=`cat $mntpoint/file | md5`
		[ "$s1" != "$s2" ] &&
		    { echo "Checksum error"; s=222; ls -l $file; }
	fi
	umount $mntpoint
fi
mdconfig -d -u $mdstart

rm -f $diskimage
exit $s
