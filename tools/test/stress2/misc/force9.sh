#!/bin/sh

#
# SPDX-License-Identifier: BSD-2-Clause-FreeBSD
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

# "mdconfig -o force" test scenario.
# "panic: ffs_checkblk: cylinder group read failed" seen:
# https://people.freebsd.org/~pho/stress/log/log0170.txt

" "panic: softdep_update_inodeblock inconsistent ip ..." seen:
# https://people.freebsd.org/~pho/stress/log/log0184.txt

[ `id -u ` -ne 0 ] && echo "Must be root!" && exit 1
. ../default.cfg

log=/tmp/force7.sh.log
mount | grep $mntpoint | grep -q /dev/md && umount -f $mntpoint
mdconfig -l | grep -q md$mdstart &&  mdconfig -d -u $mdstart
truncate -s 10g $diskimage
mdconfig -a -t vnode -f $diskimage -u $mdstart
flags=$newfs_flags
echo "newfs $flags md$mdstart"
newfs $flags md$mdstart > /dev/null 2>&1

export TESTPROGS=`cd ..; find testcases/ -perm -1 -type f | \
    egrep -Ev "/run/|/badcode/|/pty/|/shm/|/socket/|sysctl|tcp|thr|udp"`
export runRUNTIME=3m
export RUNDIR=$mntpoint/stressX
start=`date +%s`
while [ $((`date +%s` - start)) -lt $((15 * 60)) ]; do
	mount /dev/md$mdstart $mntpoint
	rm -fr $mntpoint/lost+found
	chmod 777 $mntpoint

	echo "Start tests"
	su $testuser -c 'cd ..; ./testcases/run/run $TESTPROGS' > \
	    /dev/null 2>&1 &

	sleep `jot -r 1 60 180`
	echo "Force destroy MD disk"
	while mdconfig -l | grep -q md$mdstart; do
		mdconfig -d -u $mdstart -o force || sleep 1
	done
	sleep 1
	../tools/killall.sh
	wait
	n=0
	while mount | grep $mntpoint | grep -q /dev/md; do
		umount $mntpoint || sleep 1
		[ $((n += 1)) -gt 300 ] && { echo FAIL; exit 1; }
	done
	mdconfig -a -t vnode -f $diskimage -u $mdstart
	fsck_ffs -fyR /dev/md$mdstart > $log 2>&1; s=$?
	[ $s -ne 0 ] && break
	grep -Eq "IS CLEAN|MARKED CLEAN" $log || { s=100; break; }
done
if [ $s -eq 0 ]; then
	mount /dev/md$mdstart $mntpoint
	cp -R /usr/include $mntpoint
	dd if=/dev/zero of=$mntpoint/big bs=1m count=10 status=none
	find $mntpoint/* -delete
	umount $mntpoint
	mdconfig -d -u $mdstart
	rm -f $diskimage $log
else
	cat $log
fi
exit $s
