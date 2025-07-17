#!/bin/sh

#
# Copyright (c) 2008 Peter Holm <pho@FreeBSD.org>
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

# Quota / snapshot test scenario by Kris@
# Causes spin in ffs_sync or panic in panic: vfs_allocate_syncvnode: insmntque failed

[ `id -u ` -ne 0 ] && echo "Must be root!" && exit 1
[ "`sysctl -in kern.features.ufs_quota`" != "1" ] && exit 0

. ../default.cfg

D=$diskimage
trap "rm -f $D" 0
dd if=/dev/zero of=$D bs=1m count=1k status=none

mount | grep $mntpoint | grep -q md$mdstart && umount -f $mntpoint
mdconfig -l | grep -q md$mdstart &&  mdconfig -d -u $mdstart

mdconfig -a -t vnode -f $D -u $mdstart
newfs $newfs_flags  md$mdstart > /dev/null
export PATH_FSTAB=/tmp/fstab
echo "/dev/md$mdstart $mntpoint ufs rw,userquota 2 2" > $PATH_FSTAB
mount $mntpoint
set `df -ik $mntpoint | tail -1 | awk '{print $4,$7}'`
export KBLOCKS=$(($1 / 21))
export INODES=$(($2 / 21))
export HOG=1
export INCARNATIONS=40

export QK=$((KBLOCKS / 2))
export QI=$((INODES / 2))
edquota -u -f $mntpoint -e $mntpoint:$((QK - 50)):$QK:$((QI - 50 )):$QI $testuser
quotaon $mntpoint
export RUNDIR=$mntpoint/stressX
mkdir $mntpoint/stressX
chmod 777 $mntpoint/stressX
rm -rf /tmp/stressX.control/*
su $testuser -c "(cd ..;runRUNTIME=20m ./run.sh disk.cfg)"&	# panic: vfs_allocate_syncvnode: insmntque failed
for i in `jot 20`; do
	echo "`date '+%T'` mksnap_ffs $mntpoint $mntpoint/.snap/snap$i"
	mksnap_ffs $mntpoint $mntpoint/.snap/snap$i
	sleep 1
done
i=$(($(date '+%s') % 20 + 1))
echo "rm -f $mntpoint/.snap/snap$i"
rm -f $mntpoint/.snap/snap$i
wait

while mount | grep -q $mntpoint; do
	umount $mntpoint || sleep 1
done
mdconfig -d -u $mdstart
rm -f $D $PATH_FSTAB
