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

# Mirror test where the third disk is partially wiped:
# Silent Data Corruption.
# fsck() will trash your FS in this scenario.

[ `id -u ` -ne 0 ] && echo "Must be root!" && exit 1

. ../default.cfg

gmirror load > /dev/null 2>&1 && unload=1
mount | grep -q "on $mntpoint " && umount $mntpoint
[ -c /dev/mirror/test ] && { gmirror stop test; gmirror destroy test; }
old=`sysctl -n kern.geom.mirror.debug`
sysctl kern.geom.mirror.debug=-1 | grep -q -- -1 ||
	sysctl kern.geom.mirror.debug=$old > /dev/null

u1=$mdstart
u2=$((mdstart + 1))
u3=$((mdstart + 2))

for u in $u1 $u2 $u3; do
	[ -c /dev/md$u ] && mdconfig -d -u $u
	dd if=/dev/zero of=$diskimage.$u bs=1m count=100 status=none
	mdconfig -a -t vnode -f $diskimage.$u -u $u
done > /dev/null

gmirror label test md$u1 md$u2 md$u3 || exit 1
[ "`sysctl -in kern.geom.mirror.launch_mirror_before_timeout`" = "0" ] &&
    sleep 5
(
gpart create -s BSD mirror/test
gpart add -t freebsd-ufs -s 99m mirror/test
) > /dev/null
[ -c /dev/mirror/testa ] || exit 1

newfs -n /dev/mirror/testa > /dev/null
mount /dev/mirror/testa $mntpoint
jot 10 | xargs -P0 -I% cp /etc/passwd $mntpoint/%

# The test: zap part of the third disk
dd if=/dev/random of=$diskimage.$u3 bs=1m count=80 conv=notrunc status=none
umount $mntpoint
log=/tmp/graid1_8.sh.log

if [ $# -eq 1 ]; then # This will fix the mirror
	gmirror remove test md$u3
	gmirror insert test md$u3
	while gmirror status test | grep -q SYNCHRONIZING; do
		sleep 2
	done
fi

fsck -fy /dev/mirror/testa > $log 2>&1
grep -q RERUN $log && fsck -fy /dev/mirror/testa > /dev/null 2>&1
grep -Eq "MODIFIED|BAD" $log &&
    { s=1; head -5 $log; } ||
    s=0
rm $log
mount /dev/mirror/testa $mntpoint
[ `ls $mntpoint | wc -l` -lt 10 ] && ls -l $mntpoint
umount $mntpoint

while gmirror status test | grep -q SYNCHRONIZING; do sleep 10; done
for i in `jot 10`; do
	gmirror stop test && break || sleep 30
done
[ $i -eq 10 ] && s=1
gmirror destroy test 2>/dev/null
[ $unload ] && gmirror unload

for u in $u1 $u2 $u3; do
	mdconfig -d -u $u || s=3
	rm $diskimage.$u
done
exit $s
