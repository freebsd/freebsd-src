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

[ `id -u ` -ne 0 ] && echo "Must be root!" && exit 1

# D29955: ptrace(PT_COREDUMP) test scenario.

# "panic: Assertion td->td_sleepqueue != NULL failed" seen with WiP kernel code.

. ../default.cfg

mount | grep $mntpoint | grep -q /dev/md && umount -f $mntpoint
mdconfig -l | grep -q md$mdstart &&  mdconfig -d -u $mdstart
mdconfig -a -t swap -s 2g -u $mdstart
newfs $newfs_flags -n md$mdstart > /dev/null
mount /dev/md$mdstart $mntpoint
chmod 777 $mntpoint

export runRUNTIME=10m
export RUNDIR=$mntpoint/stressX
set `df -ik $mntpoint | tail -1 | awk '{print $4,$7}'`
export KBLOCKS=$(($1 / 2))
export INODES=$(($2 / 2))

su $testuser -c 'cd ..; ./run.sh marcus.cfg' &
sleep 5
while pgrep -q run; do
	# exclude "swap" due to core file size
	list=`ps -U$testuser | sed 1d | grep -vE "\.sh|run|defunct|swap" | \
	    awk '{print $1}'`
	[ -z "$list" ] && { pkill swap; continue; }
	n=0
	pids=
	for p in $list; do
		( gcore -k -c $mntpoint/core.$p $p 2>&1 | \
		      grep -v 'No such process' ) &
		pids="$pids $!"
		n=$((n + 1))
		[ $n -ge 2 ] && break
	done
	for p in "$pids"; do
		wait $p
	done
	rm -f $mntpoint/core.*
done
while pkill -U$testuser; do sleep 2; done
wait

n=0
while mount | grep $mntpoint | grep -q /dev/md; do
	umount $mntpoint || sleep 1
	[ $((n += 1)) -gt 300 ] && { echo FAIL; exit 1; }
done
checkfs /dev/md$mdstart; s=$?
mdconfig -d -u $mdstart
exit $s
