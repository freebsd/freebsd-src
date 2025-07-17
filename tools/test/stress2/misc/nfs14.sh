#!/bin/sh

#
# Copyright (c) 2016 EMC Corp.
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

# Bring the network down and up again during NFS tests.

# "panic: re_txeof: freeing NULL mbufs!" seen:
# https://people.freebsd.org/~pho/stress/log/nfs14.txt

# This is a very disruptive test, so be aware!
[ -z "$footshoot" ] && exit 0

[ `id -u ` -ne 0 ] && echo "Must be root!" && exit 1

. ../default.cfg

[ -z "$nfs_export" ] && exit 0
ping -c 2 `echo $nfs_export | sed 's/:.*//'` > /dev/null 2>&1 ||
    exit 0

if=`ifconfig -lu | awk '{print $1}'`
[ -z "$if" ] && exit 0

[ ! -d $mntpoint ] &&  mkdir $mntpoint
mount | grep "on $mntpoint " | grep -q nfs && umount $mntpoint
mount -t nfs -o tcp -o retrycnt=3 -o soft \
	-o rw $nfs_export $mntpoint

sleep .5
export RUNDIR=$mntpoint/nfs14.`jot -rc 8 a z | tr -d '\n'`/stressX
rm -rf $RUNDIR
mkdir -p $RUNDIR
chmod 777 $RUNDIR
export runRUNTIME=3m
rm -rf /tmp/stressX.control/*

su $testuser -c '(cd ..; ./run.sh rw.cfg) > /dev/null 2>&1' &

sleep `jot -r 1 5 15`.`jot -r 1 1 9`
downtime=`jot -r 1 100 150`
echo "Testing with $downtime seconds downtime."
ifconfig $if down; sleep $downtime; ifconfig $if up

wait
rm -rf $RUNDIR
while mount | grep -q $mntpoint; do
	umount -f $mntpoint > /dev/null 2>&1
done
../tools/killall.sh
exit 0
