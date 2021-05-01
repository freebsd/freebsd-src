#!/bin/sh

#
# Copyright (c) 2015 EMC Corp.
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

# Test dynamic kern.maxvnodes implementation.

# "panic: vm_fault_hold: fault on nofault entry, addr: 0xfffffe00b1b3c000"
# seen: https://people.freebsd.org/~pho/stress/log/kostik1175.txt

# https://people.freebsd.org/~pho/stress/log/log0084.txt
# Fixed by: dc532884d582

. ../default.cfg

kldstat | grep -q tmpfs && loaded=1
mount | grep $mntpoint | grep -q /dev/md && umount -f $mntpoint
mount -o size=2g -t tmpfs tmpfs $mntpoint || exit 1
chmod 777 $mntpoint

oldmx=`sysctl -n kern.maxvnodes`
trap "sysctl kern.maxvnodes=$oldmx > /dev/null" EXIT SIGINT

export runRUNTIME=10m
export RUNDIR=$mntpoint/stressX
export TESTPROGS="
testcases/creat/creat
testcases/mkdir/mkdir
testcases/swap/swap
"
export creatINCARNATIONS=50
export creatLOAD=100

su $testuser -c 'cd ..; ./testcases/run/run $TESTPROGS' > /dev/null 2>&1 &

min=1000
max=$((oldmx * 4))
while kill -0 $! 2>/dev/null; do
	sysctl kern.maxvnodes=`jot -r 1 $min $max` > /dev/null
	sleep .2
done
wait

while mount | grep $mntpoint | grep -q tmpfs; do
	umount $mntpoint || sleep 1
done
[ $loaded ] && kldunload tmpfs.ko
exit 0
