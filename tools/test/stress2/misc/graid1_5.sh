#!/bin/sh

#
# Copyright (c) 2017 Dell EMC Isilon
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

# Mirror tests with gnop(8) errors introduced in 2 out of three partitions.

# https://people.freebsd.org/~pho/stress/log/graid1_5-2.txt
# Fixed by r327698

[ `id -u ` -ne 0 ] && echo "Must be root!" && exit 1

. ../default.cfg

gmirror load > /dev/null 2>&1 && unload=1
gmirror status > /dev/null || exit 0

gnop load > /dev/null && unload2=1;
gnop status > /dev/null || exit 0

[ -c /dev/mirror/test ] && { gmirror stop test; gmirror destroy test; }
old=`sysctl -n kern.geom.mirror.debug`
sysctl kern.geom.mirror.debug=-1 | grep -q -- -1 ||
	sysctl kern.geom.mirror.debug=$old > /dev/null

u1=$mdstart
u2=$((mdstart + 1))
u3=$((mdstart + 2))
s=0
for u in $u1 $u2 $u3; do
	[ -c /dev/md$u ] && mdconfig -d -u $u
	mdconfig -a -t swap -s 341m -u $u
	gpart create -s GPT md$u
done > /dev/null

set -e
(
gpart add -t freebsd-ufs -s 340m md$u1
gpart add -t freebsd-ufs -s 340m md$u2
gpart add -t freebsd-ufs -s 340m md$u3
) > /dev/null
gnop create md$u2
gnop create md$u3
gmirror label test md${u1}p1 md$u2.nopp1 md$u3.nopp1
[ "`sysctl -in kern.geom.mirror.launch_mirror_before_timeout`" = "0" ] &&
    sleep $((`sysctl -n kern.geom.mirror.timeout` + 1))
[ -c /dev/mirror/test ] || exit 1

newfs /dev/mirror/test > /dev/null
mount /dev/mirror/test $mntpoint
set +e
chmod 777 $mntpoint

export runRUNTIME=5m
export RUNDIR=$mntpoint/stressX
rm -rf /tmp/stressX.control

su $testuser -c 'cd ..; ./run.sh marcus.cfg' > /dev/null 2>&1 &
pid=$!

gnop configure -r 0 -w 1 md$u2.nop
gnop configure -r 0 -w 1 md$u3.nop
while kill -0 $pid > /dev/null 2>&1; do
	if ! gmirror status test | grep -q md$u2.nopp1; then
		gmirror forget test
		gmirror remove test md$u2.nopp1 2>/dev/null
		gmirror insert test md$u2.nopp1 2>/dev/null
	fi
	if ! gmirror status test | grep -q md$u3.nopp1; then
		gmirror forget test
		gmirror remove test md$u3.nopp1 2>/dev/null
		gmirror insert test md$u3.nopp1 2>/dev/null
	fi
	sleep 1
done
wait

while mount | grep $mntpoint | grep -q /mirror/; do
	umount $mntpoint || sleep 5
done
while gmirror status test | grep -q SYNCHRONIZING; do sleep 10; done
for i in `jot 10`; do
	gmirror stop test && break || sleep 30
done
[ $i -eq 10 ] && s=1
gmirror destroy test 2>/dev/null
[ $unload ] && gmirror unload

for u in $u1 $u2 $u3; do
	mdconfig -d -u $u || s=3
done
[ $unload2 ] && gnop unload
exit $s
