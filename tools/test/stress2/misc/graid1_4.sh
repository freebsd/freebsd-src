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

# Non UFS SU gmirror stop -f test.

# Page fault seen:
# https://people.freebsd.org/~pho/stress/log/graid1_4.txt
# Fixed in r316867

[ `id -u ` -ne 0 ] && echo "Must be root!" && exit 1

. ../default.cfg

rm -f $diskimage*
need=1024 # MB
[ `df -k $(dirname $diskimage) | tail -1 | awk '{print int($4 / 1024)}'` -lt \
    $need ] && printf "Need %d MB on %s.\n" $need `dirname $diskimage` && exit

gmirror load > /dev/null 2>&1 && unload=1
[ -c /dev/mirror/test ] && { gmirror stop test; gmirror destroy test; }
old=`sysctl -n kern.geom.mirror.debug`
sysctl kern.geom.mirror.debug=-1 | grep -q -- -1 ||
    sysctl kern.geom.mirror.debug=$old > /dev/null

md1=$mdstart
md2=$((mdstart + 1))

s=0
for u in $md1 $md2; do
	disk="$diskimage.$u"
	dd if=/dev/zero of=$disk bs=1m count=512 status=none
	[ -c /dev/md$u ] && mdconfig -d -u $u
	mdconfig -a -t vnode -f $disk -u $u
done

gmirror label -v -b split -s 2048 test /dev/md$md1 /dev/md$md2 \
    > /dev/null || exit 1
[ "`sysctl -in kern.geom.mirror.launch_mirror_before_timeout`" = "0" ] &&
    sleep $((`sysctl -n kern.geom.mirror.timeout` + 1))
[ -c /dev/mirror/test ] || exit 1
# Soft Updates issues with removal of backing media
newfs /dev/mirror/test > /dev/null
mount /dev/mirror/test $mntpoint
chmod 777 $mntpoint

export runRUNTIME=10m
export RUNDIR=$mntpoint/stressX

su $testuser -c 'cd ..; ./run.sh marcus.cfg' &

while kill -0 $! > /dev/null 2>&1; do
	sleep `jot -r 1 1 5`
	gmirror remove test md$md2
	sleep `jot -r 1 1 5`
	gmirror insert test md$md2
done
wait

gmirror stop -f test # Note the stop *before* umount

while mount | grep $mntpoint | grep -q /mirror/; do
	umount $mntpoint || sleep 1
done
gmirror stop test || s=1
gmirror destroy test 2>/dev/null
[ $unload ] && gmirror unload

for u in $md2 $md1; do
	mdconfig -d -u $u || s=3
done
rm -f $diskimage*
exit $s
