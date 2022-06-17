#!/bin/sh

#
# Copyright (c) 2017 Dell EMC Isilon
# All rights reserved.
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

[ `id -u ` -ne 0 ] && echo "Must be root!" && exit 1

# swapoff(8) with swap file gone missing (known issue).

# panic: swap_pager_force_pagein: read from swap failed
# https://people.freebsd.org/~pho/stress/log/swapoff2.txt

. ../default.cfg

set -e
mount | grep $mntpoint | grep -q /dev/md && umount -f $mntpoint
mdconfig -l | grep -q md$mdstart &&  mdconfig -d -u $mdstart
mdconfig -a -t swap -s 2g -u $mdstart
newfs $newfs_flags md$mdstart > /dev/null
mount /dev/md$mdstart $mntpoint
set +e

: ${size:=2m}
dd if=/dev/zero of=$mntpoint/swapfile bs=$size count=1 status=none
ls -lh $mntpoint/swapfile
swapoff -a
md2=$((mdstart + 1))
mdconfig -l | grep -q md$md2 &&  mdconfig -d -u $md2
mdconfig -a -t vnode -f $mntpoint/swapfile -u $md2
set +e
swapon /dev/md$md2

su $testuser -c 'cd ../testcases/swap; nice ./swap -t 2m -i 20 -h -l 100' &
for i in `jot 20`; do
	[ `swapinfo | tail -1 | awk '{print $3}'` -gt 0 ] && break
	sleep 5
done
umount -f $mntpoint
swapoff /dev/md$md2

while pgrep -q swap; do pkill -9 swap; done
wait

n=0
while mount | grep $mntpoint | grep -q /dev/md; do
	umount $mntpoint || sleep 1
	n=$((n + 1))
	[ $n -gt 300 ] && { echo FAIL; exit 1; }
done
mdconfig -d -u $mdstart
swapon -a
exit 0
