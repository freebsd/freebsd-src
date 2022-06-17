#!/bin/sh

#
# SPDX-License-Identifier: BSD-2-Clause-FreeBSD
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

# Simple regression test for
# "Superblock check-hash failed: recorded check-hash 0xba55a4ff != computed
# check-hash 0x709ea926".

[ `id -u ` -ne 0 ] && echo "Must be root!" && exit 1

. ../default.cfg

s=0
log=/tmp/snap12.log
m2=$((mdstart + 1))
mp2=${mntpoint}$m2
[ -d $mp2 ] || mkdir -p $mp2
mount | grep "on $mntpoint " | grep -q /dev/md && umount -f $mntpoint
[ -c /dev/md$mdstart ] &&  mdconfig -d -u $mdstart
[ -c /dev/md$m2 ] && mdconfig -d -u $m2
mdconfig -a -t swap -s 2g -u $mdstart || exit 1
newfs $newfs_flags md$mdstart > /dev/null
mount /dev/md$mdstart $mntpoint
touch $mntpoint/file

rm -f $mntpoint/.snap/stress2
for i in 1; do
	mksnap_ffs $mntpoint $mntpoint/.snap/stress2 || { s=1; break; }
	mdconfig -a -t vnode -f $mntpoint/.snap/stress2 -u $m2 -o readonly ||
		{ s=2; break; }
	mount -t ufs -o ro /dev/md$m2 $mp2 || { s=3; break; }
	[ -f $mp2/file ] || { s=4; ls -l $mp2; }
	umount $mp2
	mdconfig -d -u $m2
done

for i in `jot 6`; do
	mount | grep -q "on $mntpoint " || break
	umount $mntpoint && break || sleep 10
done
[ $i -eq 6 ] && exit 1
fsck -t ufs -y /dev/md$mdstart > $log 2>&1 || s=5
egrep -v "IS CLEAN" $log | grep -q "[A-Z]" $log || { cat $log; s=6; }
mdconfig -d -u $mdstart
rm -rf $log
exit $s
