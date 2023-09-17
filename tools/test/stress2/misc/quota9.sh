#!/bin/sh

#
# Copyright (c) 2008 Peter Holm
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

# Test if quotacheck reports actual usage.

. ../default.cfg

export tmp=/tmp/$(basename $0).$$
export D=$diskimage

qc() {
	local s
	umount $1
	s=0
	quotacheck -v $1 > $tmp 2>&1
	grep -q fixed $tmp && { cat $tmp; s=1; }
	mount $1
	return $s
}

trap "rm -f $D $tmp" 0
[ `id -u ` -ne 0 ] && echo "Must be root!" && exit 1
[ "`sysctl -in kern.features.ufs_quota`" != "1" ] && exit 0

dd if=/dev/zero of=$D bs=1m count=50 status=none || exit 1

mount | grep "$mntpoint" | grep -q md$mdstart &&
    umount -f $mntpoint
[ -c /dev/md$mdstart ] && mdconfig -d -u $mdstart

mdconfig -a -t vnode -f $D -u $mdstart
newfs $newfs_flags  md$mdstart > /dev/null
export PATH_FSTAB=/tmp/fstab
echo "/dev/md$mdstart $mntpoint ufs rw,userquota 2 2" \
    > $PATH_FSTAB
mount $mntpoint
mkdir $mntpoint/stressX
chown $testuser $mntpoint/stressX
set `df -ik $mntpoint | tail -1 | awk '{print $4,$7}'`
export KBLOCKS=$1
export INODES=$2

export QK=$((KBLOCKS / 2))
export QI=$((INODES / 2))
edquota -u -f $mntpoint -e \
    ${mntpoint}:$((QK - 50)):$QK:$((QI - 50 )):$QI $testuser > \
    /dev/null 2>&1
quotaon $mntpoint

qc $mntpoint

su $testuser -c '
	for i in `jot 20`; do
		dd if=/dev/zero of=$mntpoint/stressX/d$i bs=1m count=1 \
		    status=none
	done
	'

qc $mntpoint; s=$?

while mount | grep -q $mntpoint; do
	umount $([ $((`date '+%s'` % 2)) -eq 0 ] &&
	    echo "-f" || echo "") $mntpoint > /dev/null 2>&1
done
mdconfig -d -u $mdstart
rm -f $D $PATH_FSTAB
exit $s
