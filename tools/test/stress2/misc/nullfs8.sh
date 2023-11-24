#!/bin/sh

#
# Copyright (c) 2012 Peter Holm <pho@FreeBSD.org>
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

# Based on scenario by rea@, kern/164261. Different panic.
# insmntque: mp-safe fs and non-locked vp: 0xcb413984 is not exclusive
# locked but should be

[ `id -u ` -ne 0 ] && echo "Must be root!" && exit 1

. ../default.cfg

nullfs_srcdir=${nullfs_srcdir:-/tmp}
opt="-o nfsv3,rw,udp,rdirplus,noauto,retrycnt=3"
grep -q $mntpoint /etc/exports ||
    { echo "$mntpoint missing from /etc/exports"; exit 0; }

mount | grep -wq $mntpoint && umount $mntpoint
mount -t nullfs $nullfs_srcdir $mntpoint

mntpoint2=${mntpoint}2
mntpoint3=${mntpoint}3
for m in $mntpoint2 $mntpoint3; do
	[ -d $m ] || mkdir $m
	mount | grep -wq $m && umount $m
	mount -t nfs $opt 127.0.0.1:$mntpoint $m
done

for i in `jot 50` ; do
	su $testuser -c "cp -r /usr/include $mntpoint2/nullfs8-2 2>/dev/null" &
	su $testuser -c "cp -r /usr/include $mntpoint3/nullfs8-2 2>/dev/null" &
	wait
	su $testuser -c "find $mntpoint2 > /dev/null 2>&1" &
	su $testuser -c "find $mntpoint3 > /dev/null 2>&1" &
	wait
	rm -rf $nullfs_srcdir/nullfs8-2
done

for m in $mntpoint3 $mntpoint2 $mntpoint; do
	while mount | grep -wq $m;  do
		umount $m || sleep 1
	done
done
