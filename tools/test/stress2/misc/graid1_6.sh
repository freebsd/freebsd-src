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

# Variation of graid1_4.sh

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
newfs $newfs_flags /dev/mirror/test > /dev/null
mount /dev/mirror/test $mntpoint
chmod 777 $mntpoint

mlog=/tmp/graid1_6.log
tail -F -n 0 /var/log/messages > $mlog & mpid=$!
export runRUNTIME=4m
export RUNDIR=$mntpoint/stressX
su $testuser -c 'cd ..; ./run.sh marcus.cfg > /dev/null' &

while kill -0 $! > /dev/null 2>&1; do
	sleep `jot -r 1 1 5`
	gmirror remove test md$md2
	sleep `jot -r 1 1 5`
	gmirror insert test md$md2
done
wait $!
i=0
while ! gmirror status test | grep -q COMPLETE; do
	sleep 10
	if [ $((i += 1)) -gt 20 ]; then
		echo "FAIL to COMPLETE"
		graid status test
		s=1
		break
	fi
done

while mount | grep $mntpoint | grep -q /mirror/; do
	umount $mntpoint || sleep 1
done
checkfs /dev/mirror/test || s=2
gmirror stop -f test ||s=3
gmirror destroy test 2>/dev/null
[ $unload ] && gmirror unload

for u in $md2 $md1; do
	mdconfig -d -u $u || s=4
done
rm -f $diskimage*
grep -m 1 "check-hash" $mlog && s=5
kill $mpid
wait
rm -f $mlog
exit $s
