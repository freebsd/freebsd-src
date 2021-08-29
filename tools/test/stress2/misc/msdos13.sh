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

# Simple msdosfs rename example

[ `id -u ` -ne 0 ] && echo "Must be root!" && exit 1

. ../default.cfg

[ -x /sbin/mount_msdosfs ] || exit 0
log=/tmp/msdos13.log

set -e
mount | grep "on $mntpoint " | grep -q /dev/md && umount -f $mntpoint
[ -c /dev/md$mdstart ] &&  mdconfig -d -u $mdstart
mdconfig -a -t swap -s 2g -u $mdstart
bsdlabel -w md$mdstart auto
newfs_msdos -F 32 -b 8192 /dev/md$mdstart$part > /dev/null
mount -t msdosfs /dev/md$mdstart$part $mntpoint
mkdir $mntpoint/stressX
chmod 0777 $mntpoint/stressX
set +e

here=`pwd`
cd $mntpoint/stressX
N=4000
s=0
sort /dev/zero & pid=$!
sleep 2
for j in `jot $N`; do
	touch f$i-$j
done
for j in `jot $N`; do
	mv f$i-$j g$i-$j
	[ -f f$i-$j ] && s=1
done
for j in `jot $N`; do
	mv g$i-$j f$i-$j
done
for j in `jot $N`; do
	rm f$i-$j
done
kill $pid
wait
leftover=`find . -type f | wc -l`
if [ $leftover -gt 0 ]; then
	echo "Unexpected leftover files:"
	s=2
	find . -type f | head -5
fi
cd $here

for i in `jot 6`; do
	mount | grep -q "on $mntpoint " || break
	umount $mntpoint && break || sleep 10
	[ $i -eq 6 ] &&
	    { echo FATAL; fstat -mf $mntpoint; exit 1; }
done
fsck -t msdosfs -y /dev/md$mdstart$part > $log 2>&1
if egrep -q "BAD|INCONSISTENCY|MODIFIED" $log; then
	echo "fsck problems:"
	cat $log
	s=3

	mount -t msdosfs /dev/md$mdstart$part $mntpoint || exit 1
	ls -lR $mntpoint | head -5
	umount $mntpoint
fi
mdconfig -d -u $mdstart
rm -f $log
exit $s
