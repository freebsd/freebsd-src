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

# msdosfs disk image fuzz test.

# "panic: wrong dirclust" seen:
# https://people.freebsd.org/~pho/stress/log/log0206.txt

. ../default.cfg

[ `id -u ` -ne 0 ] && echo "Must be root!" && exit 1

cc -o /tmp/flip -Wall -Wextra -O2 ../tools/flip.c || exit 1

set -e
u1=$mdstart
u2=$((mdstart + 1))
mp1=${mntpoint}$u1
mp2=${mntpoint}$u2
mkdir -p $mp1 $mp2
log=$mp1/msdos15.sh.log
diskimage=$mp1/msdos15.sh.diskimage
cap=$((32 * 1024))		# Only fuzz the first 32k
max=$((10 * 1024 * 1024))	# dos disk size

set +e
mount | grep "on $mp1 " | grep -q /dev/md && umount -f $mp1
[ -c /dev/md$u1 ] && mdconfig -d -u $u1
mdconfig -a -t swap -s 1g -u $u1
newfs -U /dev/md$u1 > /dev/null
mount /dev/md$u1 $mp1

[ -c /dev/md$u2 ] && mdconfig -d -u $u2
dd if=/dev/zero of=$diskimage bs=$max count=1 status=none
mdconfig -a -t vnode -f $diskimage -u $u2
newfs_msdos /dev/md$u2 > /dev/null 2>&1	# FAT12
mount -t msdosfs /dev/md$u2 $mp2
[ -d /usr/include/sys ] && cp -r /usr/include/sys $mp2
umount $mp2

cd $mp1
start=`date +%s`
while [ $((`date +%s` - start)) -lt 60 ]; do
	mount -t msdosfs /dev/md$u2 $mp2 2>/dev/null || break
	ls -lR $mp2 > /dev/null 2>&1 || break
	rm -rf $mp2/* > /dev/null 2>&1 || break
	touch $mp2/`jot -rc 8 a z | tr -d '\n'` || break
	while mount | grep -q "on $mp2 "; do umount $mp2; done
	echo * | grep -q core && break
	sync
	mdconfig -d -u $u2
	/tmp/flip -n 10 -s $cap $diskimage
	sync
	mdconfig -a -t vnode -f $diskimage -u $u2
done
mount | grep -q "on $mp2 " && umount $mp2
mdconfig -d -u $u2 || exit 1

echo * | grep -q core && { ls -l *.core; cp $log /tmp; exit 106; } ||
cd /tmp
umount $mp1
mdconfig -d -u $u1
rm -f /tmp/flip
exit 0
