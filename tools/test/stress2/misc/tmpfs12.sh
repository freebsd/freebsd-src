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

# umount -f test scenario

# Problems seen:
# panic: cache_enter: Doomed vnode used as src
#     http://people.freebsd.org/~pho/stress/log/kostik676.txt
# panic: Bad link elm 0xfffff800b384e828 next->prev != elm
#     http://people.freebsd.org/~pho/stress/log/kostik677.txt
# panic: unrhdr has 1 allocations
#     http://people.freebsd.org/~pho/stress/log/kostik678.txt
# Fixed in r268605 - r268617 and r268766.

[ $((`sysctl -n hw.usermem` / 1024 / 1024 / 1024)) -lt 6 ] && exit 0
[ `id -u ` -ne 0 ] && echo "Must be root!" && exit 1

. ../default.cfg

N=3

export runRUNTIME=15m
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
	mount  -o size=2g -t tmpfs tmpfs $mp
	chmod 777 $mp
	export RUNDIR=$mp/stressX
	export CTRLDIR=$mp/stressX.control
	su $testuser -c 'cd ..; ./testcases/run/run $TESTPROGS' > \
	    /dev/null 2>&1 &
done
daemon sh -c '(cd ../testcases/swap; ./swap -t 5m -i 20 -h -l 100)' > \
    /dev/null 2>&1

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
