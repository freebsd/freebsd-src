#!/bin/sh

#
# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) 2020 Peter Holm <pho@FreeBSD.org>
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
[ `swapinfo | wc -l` -eq 1 ] && exit 0

# Variation of marcus5.sh
# "panic: Assertion pgrp->pg_jobc > 0 failed at kern_proc.c:740" seen.

. ../default.cfg

mount | grep $mntpoint | grep -q /dev/md && umount -f $mntpoint
mdconfig -l | grep -q md$mdstart &&  mdconfig -d -u $mdstart
mdconfig -a -t swap -s 5g -u $mdstart
newfs $newfs_flags md$mdstart > /dev/null
mount /dev/md$mdstart $mntpoint
chmod 777 $mntpoint

size=$((`sysctl -n hw.physmem` / 1024 / 1024))
[ "`sysctl -n debug.deadlkres.sleepfreq 2>/dev/null`" = "3" ] &&
    { echo "deadlkres must be disabled for this test."; exit 0; }

m=`su $testuser -c "limits | awk '/maxprocesses/ {print \\$NF}'"`
[ $m -gt 100000 ] && m=100000

rm -rf /tmp/stressX.control
export RUNDIR=$mntpoint/stressX
export INCARNATIONS=$((m / 11))
export runRUNTIME=3m
export LOAD=80
export symlinkLOAD=80
export rwLOAD=80
export TESTPROGS="
testcases/lockf2/lockf2
testcases/symlink/symlink
testcases/openat/openat
testcases/rw/rw
testcases/fts/fts
testcases/link/link
testcases/lockf/lockf
testcases/creat/creat
testcases/mkdir/mkdir
testcases/rename/rename
testcases/mkfifo/mkfifo
"

timeout -s SIGINT -k 2m 1m su $testuser -c 'cd ..; ./testcases/run/run $TESTPROGS'

n=0
while mount | grep $mntpoint | grep -q /dev/md; do
	umount $mntpoint || sleep 1
	[ $((n += 1)) -gt 60 ] && { echo "Timed out"; exit 1; }
done
checkfs /dev/md$mdstart; s=$!
mdconfig -d -u $mdstart
exit $s
