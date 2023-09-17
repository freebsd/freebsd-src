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

# Component looses it's name:
# g_dev_taste: make_dev_p() failed
# (gp->name=gptid/7c598e03-19cb-11e7-b62b-001e6756c168, error=17)

# Deadlock seen:
# https://people.freebsd.org/~pho/stress/log/graid1_7.txt

[ `id -u ` -ne 0 ] && echo "Must be root!" && exit 1

. ../default.cfg

gmirror load > /dev/null 2>&1 && unload=1
[ -c /dev/mirror/test ] && { gmirror stop test; gmirror destroy test; }
old=`sysctl -n kern.geom.mirror.debug`
sysctl kern.geom.mirror.debug=-1 | grep -q -- -1 ||
    sysctl kern.geom.mirror.debug=$old > /dev/null

u1=$mdstart
s=0
[ -c /dev/md$u1 ] && mdconfig -d -u $u1
mdconfig -a -t swap -s 1g -u $u1

set -e
(
gpart create -s GPT md$u1
gpart add -t freebsd-ufs -s 341m md$u1
gpart add -t freebsd-ufs -s 341m md$u1
gpart add -t freebsd-ufs -s 341m md$u1
) > /dev/null
gmirror label test md${u1}p1 md${u1}p2 md${u1}p3
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

su $testuser -c 'cd ..; ./run.sh io.cfg' > /dev/null 2>&1 &
pid=$!

mlog=/tmp/graid1_6.log
tail -F -n 0 /var/log/messages > $mlog & mpid=$!
sleep 2
cont=/tmp/graid1_7.cont
touch $cont
for i in `jot 8`; do
	while [ -f $cont ]; do
		for u in md${u1}p2 md${u1}p3; do
			gmirror forget test
			gmirror remove test $u
			gmirror insert test $u
			id=`gmirror status test | grep gptid | awk '{print $1}'`
			if [ $i -eq 1 -a -n "$id" ]; then
				echo "FAIL Remove component $id"
				gmirror remove test $id
			fi
		done 2>/dev/null
	done &
done
while kill -0 $pid 2>/dev/null; do sleep 5; done
rm $cont
wait $!
gmirror status test | grep -qw md${u1}p2 || gmirror insert test md${u1}p2
gmirror status test | grep -qw md${u1}p3 || gmirror insert test md${u1}p3
i=0
while ! gmirror status test | grep -q COMPLETE; do
	sleep 5
	if [ $((i += 1)) -gt 20 ]; then
		echo "FAIL to COMPLETE"
		gmirror status test
		s=1
		break
	fi
done

while mount | grep $mntpoint | grep -q /mirror/; do
	umount $mntpoint || sleep 5
done
checkfs /dev/mirror/test || s=2
while gmirror status test | grep -q SYNCHRONIZING; do sleep 10; done
for i in `jot 10`; do
	gmirror stop test && break || sleep 30
done
[ $i -eq 10 ] && s=3
gmirror destroy test 2>/dev/null
[ $unload ] && gmirror unload

mdconfig -d -u $mdstart || s=4
grep -m 1 "check-hash" $mlog && s=5
kill $mpid
wait
rm -f $mlog
exit $s
