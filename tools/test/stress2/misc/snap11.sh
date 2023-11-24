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

[ `id -u ` -ne 0 ] && echo "Must be root!" && exit 1

# Page fault seen in call from handle_workitem_freeblocks():
# https://people.freebsd.org/~pho/stress/log/snap11.txt
# Fixed by r321347-9.

# "panic: handle_written_inodeblock: not started" seen:
# https://people.freebsd.org/~pho/stress/log/snap11-2.txt

# "panic: Barrier request in delayed write" seen:
# https://people.freebsd.org/~pho/stress/log/snap11-3.txt

. ../default.cfg

s=0
log=/tmp/snap11.log
mount | grep "on $mntpoint " | grep -q /dev/md && umount -f $mntpoint
[ -c /dev/md$mdstart ] &&  mdconfig -d -u $mdstart
mdconfig -a -t swap -s 2g -u $mdstart || exit 1
newfs $newfs_flags md$mdstart > /dev/null
mount /dev/md$mdstart $mntpoint

last=`tail -1 /var/log/messages | cut -c1-15`
[ -z "$last" ] && last=dummy
export RUNDIR=$mntpoint/stressX
export runRUNTIME=2m
(cd ..; ./run.sh io.cfg) > /dev/null &

while kill -0 $! 2>/dev/null; do
	rm -f $mntpoint/.snap/stress2
	mksnap_ffs $mntpoint $mntpoint/.snap/stress2 || continue

	m2=$((mdstart + 1))
	[ -c /dev/md$m2 ] && mdconfig -d -u $m2
	mdconfig -a -t vnode -f $mntpoint/.snap/stress2 -u $m2 -o readonly
	fsck -t ufs -y /dev/md$m2 > $log 2>&1 || { echo "fsck failed"; s=1; }
	egrep -v "WARNING|WRITE" $log | LANG=C grep -q "[A-Z][A-Z]" &&
	    { cat $log; s=1; }
	egrep -q "check-hash|Can't open" $log &&  { cat $log; s=1; }
	mdconfig -d -u $m2
	[ $s -ne 0 ] && { ../tools/killall.sh; break; }
done
wait

sleep 1
tail -50 /var/log/messages | grep -m 1 "g_vfs_done():md5a" && s=1

for i in `jot 6`; do
	mount | grep -q "on $mntpoint " || break
	umount $mntpoint && break || sleep 10
done
[ $i -eq 6 ] && exit 1
fsck -t ufs -y /dev/md$mdstart > $log 2>&1 || s=1
egrep -v "IS CLEAN" $log | grep -q "[A-Z]" $log || { cat $log; s=1; }
mdconfig -d -u $mdstart
rm -rf $log
sed "1,/$last/d" < /var/log/messages | tail -20 | grep -m 1 "check-hash" && s=1
exit $s
