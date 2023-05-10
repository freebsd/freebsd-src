#!/bin/sh

#
# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) 2018 Dell EMC Isilon
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

# Buildworld / quota test scenario.

# "panic: chkdquot: missing dquot" seen
# https://people.freebsd.org/~pho/stress/log/kostik1113.txt
# Fixed in r338798 + r338799

. ../default.cfg

[ `id -u ` -ne 0 ] && echo "Must be root!" && exit 1

[ "`sysctl -in kern.features.ufs_quota`" != "1" ] && exit 0
[ -d /usr/src/sys ] || exit 0
mount | grep -q "on $mntpoint " && umount $mntpoint
mdconfig -a -t swap -s 2g -u $mdstart
newfs $newfs_flags md$mdstart > /dev/null

export PATH_FSTAB=/tmp/fstab
trap "rm -f $PATH_FSTAB" EXIT INT
echo "/dev/md$mdstart $mntpoint ufs rw,userquota 2 2" > $PATH_FSTAB
mount $mntpoint
set `df -ik $mntpoint | tail -1 | awk '{print $4,$7}'`
export QK=$(($1 / 2))
export QI=$(($2 / 2))
edquota -u -f $mntpoint -e \
    ${mntpoint}:$((QK - 50)):$QK:$((QI - 50 )):$QI $testuser
quotaon $mntpoint
mount | grep $mntpoint

cd /usr/src
export MAKEOBJDIRPREFIX=$mntpoint/obj
export TMPDIR=$mntpoint/tmp
mkdir $TMPDIR $MAKEOBJDIRPREFIX
chmod 0777 $TMPDIR $MAKEOBJDIRPREFIX

p=$((`sysctl -n hw.ncpu`+ 1))
su $testuser -c \
    "make -i -j $p buildworld  DESTDIR=$mntpoint TARGET=amd64 \
    TARGET_ARCH=amd64 > /dev/null" &
sleep 2
start=`date +%s`
while [ $((`date +%s` - start)) -lt 1200 ]; do
	kill -0 $! > /dev/null 2>&1 || break
	sleep 2
done
kill $! > /dev/null 2>&1
# Let make run 50% of the time so quotaoff runs on an active FS
[ `jot -r 1 0 1` -eq 1 ] &&
	pkill -U$testuser make
wait

while ! quotaoff $mntpoint; do
	sync
	sleep 5
done
pgrep -q -U$testuser make && pkill -U$testuser make
export tmp=/tmp/$(basename $0).$$
quotacheck -v $mntpoint > $tmp 2>&1
grep -q failed $tmp && { cat $tmp; s=1; }
while mount | grep -q "on $mntpoint "; do
	umount $mntpoint || sleep 1
done
checkfs /dev/md$mdstart || s=$?
mdconfig -d -u $mdstart
rm -f $PATH_FSTAB
exit $s
