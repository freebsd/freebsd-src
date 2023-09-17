#!/bin/sh

#
# Copyright (c) 2011 Peter Holm <pho@FreeBSD.org>
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

# Deadlock scenario based on kern/154228, fixed in r217880.

[ `id -u ` -ne 0 ] && echo "Must be root!" && exit 1

. ../default.cfg

size="2g"
[ `swapinfo | wc -l` -eq 1 ] && exit 0
[ `swapinfo -k | tail -1 | awk '{print int($4/1024/1024)}'` -lt \
    ${size%g} ] && exit 0
m=$((mdstart + 1))
mp2=${mntpoint}$m
mount | grep $mp2    | grep -q /dev/md && umount -f $mp2
mount | grep $mntpoint | grep -q /dev/md && umount -f $mntpoint
[ -c /dev/md$mdstart ] && mdconfig -d -u $mdstart
[ -c /dev/md$m ] && mdconfig -d -u $m
mkdir -p $mp2
mdconfig -a -t swap -s $size -u $mdstart || exit 1

gjournal load
gjournal label -s 512m md$mdstart
sleep .5
newfs -J /dev/md$mdstart.journal > /dev/null
mount -o async /dev/md$mdstart.journal $mntpoint

here=`pwd`
cd $mntpoint
dd if=/dev/zero of=image bs=1m count=1k status=none
mdconfig -a -t vnode -f image -u $m
newfs md${m} > /dev/null
mount /dev/md${m} $mp2
# dd will suspend in wdrain
echo "Expect \"$mp2: write failed, filesystem is full\""
dd if=/dev/zero of=$mp2/zero bs=1M > /dev/null 2>&1
while mount | grep $mp2 | grep -q /dev/md; do
	umount $mp2 || sleep 1
done
mdconfig -d -u $m
cd $here

gjournal sync
while mount | grep $mntpoint | grep -q /dev/md; do
	umount $mntpoint || sleep 1
done
gjournal stop md$mdstart
gjournal unload
mdconfig -d -u $mdstart
