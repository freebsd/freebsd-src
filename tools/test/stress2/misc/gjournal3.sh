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

# Deadlock seen: http://people.freebsd.org/~pho/stress/log/gjournal3.txt
# Fixed in r244925

# panic: Bio not on queue
# https://people.freebsd.org/~pho/stress/log/gjournal3-2.txt

# kib@ wrote:
# gjournal is good for exposing the suspension problems.  The frequency
# of the suspensions called from the gjournal is not achievable by other
# methods, so the tests allow to uncover the problems.  More, the gjournal
# only establishes the suspension, without snapshotting, which also
# makes it easier to see the issues.

# gjournal / ffs snapshot suspension deadlock:
# https://people.freebsd.org/~pho/stress/log/gjournal3-4.txt
# Originally reported as  kern/164252.

[ `id -u ` -ne 0 ] && echo "Must be root!" && exit 1

. ../default.cfg

[ `swapinfo | wc -l` -eq 1 ] && exit 0
size="12g"
jsize="8g"
[ $((`sysctl -n hw.usermem` / 1024 / 1024 / 1024)) -le ${size%g} ] && exit 0
[ `swapinfo -k | tail -1 | awk '{print int($4/1024/1024)}'` -lt \
    ${size%g} ] && exit 0
mount | grep $mntpoint | grep -q /dev/md && umount -f $mntpoint
[ -c /dev/mdmd$mdstart ] && mdconfig -d -u $mdstart
mdconfig -a -t swap -s $size -u $mdstart || exit 1

gjournal load
gjournal label -s $jsize md$mdstart
sleep .5
newfs -J /dev/md$mdstart.journal > /dev/null
mount -o async /dev/md$mdstart.journal $mntpoint
chmod 777 $mntpoint

export runRUNTIME=10m
export RUNDIR=$mntpoint/stressX

su $testuser -c 'cd ../testcases/rw
	./rw -t 2m -i 10 -l 100 > /dev/null 2>&1' &
while kill -0 $! 2>/dev/null; do
	mksnap_ffs $mntpoint $mntpoint/.snap/snap
	sleep .2
	rm -f $mntpoint/.snap/snap
done
wait

gjournal sync
while mount | grep $mntpoint | grep -q /dev/md; do
	umount $mntpoint || sleep 1
done
gjournal stop md$mdstart
gjournal unload
mdconfig -d -u $mdstart
