#!/bin/sh

#
# Copyright (c) 2013 Peter Holm <pho@FreeBSD.org>
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

# Run with marcus.cfg on a 2g swap backed MD with UFS SU fs.
# Copy of marcus.sh, but with maximum load.

# "panic: vm_radix_remove: invalid key found" seen.
# watchdog fired: https://people.freebsd.org/~pho/stress/log/marcus2.txt

. ../default.cfg

mount | grep $mntpoint | grep -q /dev/md && umount -f $mntpoint
mdconfig -l | grep -q md$mdstart &&  mdconfig -d -u $mdstart
mdconfig -a -t swap -s 2g -u $mdstart
newfs $newfs_flags md$mdstart > /dev/null
mount /dev/md$mdstart $mntpoint
chmod 777 $mntpoint

export runRUNTIME=20m
export RUNDIR=$mntpoint/stressX

n=`find ../testcases -perm -1 -type f | wc -l`
m=`su $testuser -c "limits | awk '/maxprocesses/ {print \\$NF}'"`
export INCARNATIONS=$((m / n))
export swapINCARNATIONS=$INCARNATIONS

su $testuser -c 'cd ..; ./run.sh marcus.cfg' &

sleep 10
start=`date '+%s'`
while pgrep -q run; do
	[ $((`date '+%s'` - start)) -gt 1500 ] &&
	    ../tools/killall.sh
	sleep 10
done
wait

n=0
while mount | grep $mntpoint | grep -q /dev/md; do
	umount $mntpoint || sleep 1
	[ $((n += 1)) -gt 300 ] && { echo FAIL; exit 1; }
done
checkfs /dev/md$mdstart; s=$?
mdconfig -d -u $mdstart
exit $s
