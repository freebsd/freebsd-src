#!/bin/sh

#
# Copyright (c) 2014 EMC Corp.
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

# umount -f test scenario

# "panic: deadlkres: possible deadlock detected..." seen.
# http://people.freebsd.org/~pho/stress/log/tmpfs13.txt
# Fixed by r269457.

# "panic: vm_domainset_iter_first: Invalid domain 2" seen.
# Fixed by r339075

. ../default.cfg

N=`sysctl -n hw.ncpu`
[ $N -gt 32 ] && N=32  # Arbitrary cap
usermem=`sysctl -n hw.usermem`
[ `swapinfo | wc -l` -eq 1 ] && usermem=$((usermem/100*80))
size=$((usermem / 1024 / 1024 / 2))

export runRUNTIME=15m
export RUNDIR=$mp1/stressX
export CTRLDIR=$mp1/stressX.control
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

for i in `jot $N 1`; do
	eval mp$i=${mntpoint}$i
done

for i in `jot $N 1`; do
	eval mp=\$mp$i
	[ -d $mp ] || mkdir -p $mp
	mount | grep $mp | grep -q tmpfs && umount -f $mp
	mount  -o size=${size}m -t tmpfs tmpfs $mp
	chmod 777 $mp
	export RUNDIR=$mp/stressX
	export CTRLDIR=$mp/stressX.control
	su $testuser -c 'cd ..; ./testcases/run/run $TESTPROGS' > \
	    /dev/null 2>&1 &
done

sleep 60

for i in `jot $N 1`; do
	eval mp=\$mp$i
	while mount | grep "$mp " | grep -q tmpfs; do
		umount -f $mp || sleep 1
	done
done
../tools/killall.sh
while pgrep -q swap; do
	pkill -9 swap
done
wait
