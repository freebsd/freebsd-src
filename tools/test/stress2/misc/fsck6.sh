#!/bin/sh

#
# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) 2021 Peter Holm <pho@FreeBSD.org>
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
# fsck_ffs -p test

# /dev/md10: CANNOT READ BLK: 320
# /dev/md10: UNEXPECTED SOFT UPDATE INCONSISTENCY; RUN fsck MANUALLY.

[ `id -u ` -ne 0 ] && echo "Must be root!" && exit 1
. ../default.cfg

# Check for lingering threads from the last run
pgrep -x mount && { pgrep -x mount | xargs ps -lp; exit 1; }
../tools/killall.sh || exit 1

fsck=/sbin/fsck_ffs
log=/tmp/fsck6.log
[ -c /dev/md$mdstart ] && mdconfig -d -u $mdstart
mdconfig -a -t swap -s 5g -u $mdstart || exit 1
md=md$mdstart
newfs -U /dev/$md > /dev/null 2>&1 || exit 1

gnop create /dev/$md || exit 1
mount /dev/$md.nop $mntpoint || exit 1

for i in `jot 5`; do
	cp -a /usr/include $mntpoint/d$i
done
(cd $mntpoint; umount $mntpoint)
ls -lR $mntpoint > /dev/null
touch $mntpoint/x
sync; sleep 1; sync; sleep 1; sync

gnop destroy -f /dev/$md.nop

# Wait until forcible unmount, may be up to about 30 seconds,
# but typically very quick if I/O is in progress
s=`date +%s`
n=0
while mount | grep -q "on $mntpoint "; do
	[ $n -eq 0 ] && /bin/echo -n "Waiting for $mntpoint to force umount ..."
	n=$((n + 1))
	sleep 2
	if [ $((`date +%s` - s)) -ge 180 ]; then
	    echo "Giving up on waiting for umount of $mntpoint"
	    umount $mntpoint || umount -f $mntpoint
	    break
	fi
done
[ $n -ne 0 ] && echo

$fsck -p /dev/$md; s=$?
mdconfig -d -u ${md#md}
rm -f $log
exit $s
