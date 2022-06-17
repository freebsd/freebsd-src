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

# SUJ, quota and snapshots test scenario

[ `id -u ` -ne 0 ] && echo "Must be root!" && exit 1
[ "`sysctl -in kern.features.ufs_quota`" != "1" ] && exit 0

. ../default.cfg

snap () {
	for i in `jot 5`; do
		mksnap_ffs $1 $2 2>&1 | grep -v "Resource temporarily unavailable"
		[ ! -s $2 ] && rm -f $2	|| return 0
		sleep 1
	done
	return 1
}

mount | grep "$mntpoint" | grep -q md$mdstart && umount $mntpoint
mdconfig -l | grep -q md$mdstart &&  mdconfig -d -u $mdstart

mdconfig -a -t swap -s 1g -u $mdstart
newfs -j  md$mdstart > /dev/null
export PATH_FSTAB=/tmp/fstab
echo "/dev/md$mdstart $mntpoint ufs rw,userquota 2 2" > $PATH_FSTAB
mount $mntpoint
set `df -ik $mntpoint | tail -1 | awk '{print $4,$7}'`
export QK=$(($1 / 4))
export QI=$(($2 / 4))
edquota -u -f $mntpoint -e ${mntpoint}:$((QK - 50)):$QK:$((QI - 50 )):$QI $testuser
quotaon $mntpoint
export RUNDIR=$mntpoint/stressX
chmod 777 $mntpoint
su $testuser -c 'sh -c "(cd ..;runRUNTIME=20m ./run.sh disk.cfg > /dev/null 2>&1)"' &

for i in `jot 20`; do
	echo "`date '+%T'` mksnap_ffs $mntpoint $mntpoint/.snap/snap$i"
	snap $mntpoint $mntpoint/.snap/snap$i || break
	sleep 1
done
i=$(($(date '+%S') % 20 + 1))
echo "rm -f $mntpoint/.snap/snap$i"
rm -f $mntpoint/.snap/snap$i
wait

while mount | grep -q $mntpoint; do
	umount $mntpoint || sleep 1
done
mdconfig -d -u $mdstart
rm -f $PATH_FSTAB
