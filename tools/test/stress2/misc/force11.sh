#!/bin/sh

#
# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) 2022 Peter Holm <pho@FreeBSD.org>
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
. ../default.cfg

set -u
backup=`dirname $diskimage`/force11.sh.diskimage.`date +%Y%m%dT%H%M%S`
log=/tmp/force11.sh.log
mount | grep $mntpoint | grep -q /dev/md && umount -f $mntpoint
mdconfig -l | grep -q md$mdstart && mdconfig -d -u $mdstart
truncate -s 10g $diskimage
mdconfig -a -t vnode -f $diskimage -u $mdstart
flags=$newfs_flags
[ `jot -r 1 0 1` -eq 1 ] && flags="-j"
echo "newfs $flags md$mdstart"
newfs $flags md$mdstart > /dev/null 2>&1

export TESTPROGS=`cd ..; find testcases/ -perm -1 -type f | \
    egrep -Ev "/run/|/badcode/|/pty/|/shm/|/socket/|sysctl|tcp|thr|udp|rename"`
export runRUNTIME=3m
export RUNDIR=$mntpoint/stressX
export CTRLDIR=$mntpoint/stressX.control
start=`date +%s`
while [ $((`date +%s` - start)) -lt $((15 * 60)) ]; do
	mount /dev/md$mdstart $mntpoint
	rm -fr $mntpoint/lost+found
	chmod 777 $mntpoint

	su $testuser -c 'cd ..; ./testcases/run/run $TESTPROGS' > \
	    /dev/null 2>&1 &

	sleep `jot -r 1 30 60`
	while mdconfig -l | grep -q md$mdstart; do
		mdconfig -d -u $mdstart -o force || sleep 1
	done
	sleep 1
	../tools/killall.sh
	wait
	dd if=$diskimage of=$backup bs=1m conv=sparse,sync status=none
	sync; sleep .5; sync; sleep .5; sync
	n=0
	while mount | grep $mntpoint | grep -q /dev/md; do
		umount $mntpoint || sleep 1
		[ $((n += 1)) -gt 300 ] && { echo FAIL; exit 1; }
	done
	mdconfig -a -t vnode -f $diskimage -u $mdstart
	c=0
	for i in `jot 5`; do
		fsck_ffs -fyR /dev/md$mdstart > $log 2>&1; s=$?
		grep -q CLEAN $log && grep -q "MODIFIED" $log && c=$((c+=1))
		grep -Eq "FILE SYSTEM WAS MODIFIED" $log || break
	done
	[ $c -gt 1 ] &&
	    { echo "Note: FS marked clean+modified $c times out of $i fsck runs"; s=101; }
	[ $s -ne 0 ] && break
	grep -Eq "IS CLEAN|MARKED CLEAN" $log || { s=100; break; }
	break # For now, only once
done
if [ $s -eq 0 ]; then
	mount /dev/md$mdstart $mntpoint
	cp -R /usr/include $mntpoint || s=1
	dd if=/dev/zero of=$mntpoint/big bs=1m count=10 status=none || s=2
	ls -lR $mntpoint > /dev/null || s=3
	find $mntpoint/* -delete || s=4
	umount $mntpoint
	[ $s -eq 0 ] &&
	    rm -f $diskimage $log $backup
else
	tail -10 $log
fi
mdconfig -d -u $mdstart
[ -f $backup ] && xz -T0 $backup
exit $s
