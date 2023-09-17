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

# Simple dtrace(1) test scenario.
# https://people.freebsd.org/~pho/stress/log/dtrace.txt

[ `id -u ` -ne 0 ] && echo "Must be root!" && exit 1
. ../default.cfg
dtrace -n 'dtrace:::BEGIN { exit(0); }' > /dev/null 2>&1 || exit 0

mount | grep "on $mntpoint " | grep -q /dev/md && umount -f $mntpoint
[ -c /dev/md$mdstart ] &&  mdconfig -d -u $mdstart
mdconfig -a -t swap -s 2g -u $mdstart
newfs md$mdstart > /dev/null
mount /dev/md$mdstart $mntpoint
chmod 777 $mntpoint

export runRUNTIME=5m
export RUNDIR=$mntpoint/stressX

log=/tmp/dtrace.$$
trap "rm -f $log" EXIT INT
dtrace -w -n 'syscall::*read:entry,syscall::*write:entry {\
    @rw[execname,probefunc] = count(); }' > $log 2>&1 &
pid=$!
sleep 1
su $testuser -c 'cd ..; ./run.sh marcus.cfg' > /dev/null 2>&1
kill -INT $pid > /dev/null 2>&1
while pgrep -q dtrace; do
	pkill dtrace
	sleep 2
done
wait
tail -5 $log

while mount | grep "on $mntpoint " | grep -q /dev/md; do
	umount $mntpoint || sleep 1
done
mdconfig -d -u $mdstart
kldstat | grep -q dtraceall &&
    kldunload dtraceall.ko
exit 0
