#!/bin/sh

#
# Copyright (c) 2009 Peter Holm <pho@FreeBSD.org>
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

# Deadlock seen when deleting the snapshots during an "ls" of the FS

# Based on test scenario by John Kozubik <john at kozubik dot com>
# kern/94769: [ufs] Multiple file deletions on multi-snapshotted filesystems
# causes hang

# panic: handle_workitem_remove: bad dir delta
# https://people.freebsd.org/~pho/stress/log/log0121.txt

[ `id -u ` -ne 0 ] && echo "Must be root!" && exit 1

. ../default.cfg

mount | grep -q /dev/md$mdstart && umount -f /dev/md${mdstart}
mdconfig -l | grep -q md$mdstart && mdconfig -d -u $mdstart

parallel=20
size=25  # Gb
mx=$((`sysctl -n hw.physmem` / 1024 / 1024 / 1024 / 2))
[ $size -gt $mx ] && size=$mx

[ `df -k $(dirname $diskimage) | tail -1 | awk '{print $4}'` -lt \
    $((size * 1024 * 1024)) ] && \
                echo "Not enough disk space." && exit 1
truncate -s ${size}G $diskimage

mdconfig -a -t vnode -f $diskimage -u $mdstart
newfs -O2 $newfs_flags md$mdstart > /dev/null
mount /dev/md$mdstart $mntpoint

mycc -o /tmp/fstool -Wall ../tools/fstool.c
for i in `jot $parallel`; do
	(mkdir $mntpoint/test$i; cd $mntpoint/test$i; \
	    timeout 10m /tmp/fstool -l -f 50 -n 500 -s 8k) &
done
wait
rm -f /tmp/fstool

mksnap_ffs $mntpoint $mntpoint/.snap/snap1
mksnap_ffs $mntpoint $mntpoint/.snap/snap2
mksnap_ffs $mntpoint $mntpoint/.snap/snap3
mksnap_ffs $mntpoint $mntpoint/.snap/snap4
mksnap_ffs $mntpoint $mntpoint/.snap/snap5
mksnap_ffs $mntpoint $mntpoint/.snap/snap6
mksnap_ffs $mntpoint $mntpoint/.snap/snap7
mksnap_ffs $mntpoint $mntpoint/.snap/snap8
mksnap_ffs $mntpoint $mntpoint/.snap/snap9

for i in `jot $parallel`; do
	rm -rf $mntpoint/test$i &
done
wait

rm -rf $mntpoint/.snap/snap? &

for i in `jot 10`; do
	ls -lsrt $mntpoint > /dev/null 2>&1
	sleep 2
done
wait

umount /dev/md$mdstart

mount | grep -q /dev/md$mdstart && umount -f /dev/md${mdstart}
mdconfig -l | grep -q md$mdstart && mdconfig -d -u $mdstart
rm -f $diskimage
