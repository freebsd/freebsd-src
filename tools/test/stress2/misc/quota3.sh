#!/bin/sh

#
# Copyright (c) 2008-2011 Peter Holm <pho@FreeBSD.org>
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
[ "`sysctl -in kern.features.ufs_quota`" != "1" ] && exit 0

. ../default.cfg

D=$diskimage
export PATH_FSTAB=/tmp/fstab
trap "rm -f $D $PATH_FSTAB" 0
dd if=/dev/zero of=$D bs=1m count=1k status=none || exit 1

mount | grep "$mntpoint" | grep -q md$mdstart && umount $mntpoint
mdconfig -l | grep md$mdstart > /dev/null &&  mdconfig -d -u $mdstart

mdconfig -a -t vnode -f $D -u $mdstart
newfs $newfs_flags  md$mdstart > /dev/null
echo "/dev/md$mdstart $mntpoint ufs rw,userquota 2 2" > $PATH_FSTAB
mount $mntpoint
edquota -u -f $mntpoint -e $mntpoint:850000:900000:130000:140000 root
quotacheck $mntpoint
quotaon $mntpoint
mksnap_ffs $mntpoint $mntpoint/.snap/stress2
export RUNDIR=$mntpoint/stressX
export runRUNTIME=10m            # Run tests for 10 minutes
(cd ..; ./run.sh disk.cfg)
while mount | grep $mntpoint | grep -q /dev/md; do
	umount $mntpoint || sleep 1
done
mdconfig -d -u $mdstart
