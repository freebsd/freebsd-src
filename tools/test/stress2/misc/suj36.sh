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

# SU+J and snapshots

# "panic: handle_disk_io_initiation: Unexpected type jnewblk" seen
# https://people.freebsd.org/~pho/stress/log/log0392.txt

[ `id -u ` -ne 0 ] && echo "Must be root!" && exit 1

. ../default.cfg

set -eu
prog=$(basename "$0" .sh)
log=/tmp/$prog.log

mount | grep "on $mntpoint " | grep -q /dev/md && umount -f $mntpoint
[ -c /dev/md$mdstart ] &&  mdconfig -d -u $mdstart
mdconfig -a -t swap -s 5g -u $mdstart
[ "$newfs_flags" = "-U" ] && newfs_flags="-j"
newfs $newfs_flags md$mdstart > /dev/null
mount /dev/md$mdstart $mntpoint
chmod 777 $mntpoint

export LOAD=80
export rwLOAD=80
export runRUNTIME=10m
export RUNDIR=$mntpoint/stressX
export CTRLDIR=$mntpoint/stressX.control
export MAXSWAPPCT=80
export TESTPROGS=`cd ..; find testcases/ -perm -1 -type f | \
    egrep -Ev "/run/|/badcode/|/pty/|/shm/|/socket/|sysctl|tcp|thr|udp"`

su $testuser -c 'cd ..; ./testcases/run/run $TESTPROGS' > /dev/null &

sleep 5
for i in `jot 10`; do
	for j in `jot 5`; do
		rm -f $mntpoint/.snap/snap.$j
		mksnap_ffs  $mntpoint  $mntpoint/.snap/snap.$j
		sleep 10
	done
	sleep 10
done
wait

../tools/killall.sh
for i in `jot 6`; do
	mount | grep -q "on $mntpoint " || break
	umount $mntpoint && break || sleep 10
	[ $i -eq 6 ] &&
	    { echo FATAL; fstat -mf $mntpoint; exit 1; }
done
fsck_ffs -fy /dev/md$mdstart > $log 2>&1; s=$?
grep -q "WAS MODIFIED" $log && { tail -12 $log; s=100; }
mdconfig -d -u $mdstart
rm -f $log
exit $s
