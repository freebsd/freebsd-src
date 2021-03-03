#!/bin/sh

#
# Copyright (c) 2018 Dell EMC Isilon
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

# Read error handling for synchronization requests
# Test scenario by Mark Johnston <markj@FreeBSD.org>
# Fixed by r327779

[ `id -u ` -ne 0 ] && echo "Must be root!" && exit 1

. ../default.cfg

md1=$mdstart
md2=$((mdstart + 1))

s=0
size=1g
[ $((`sysctl -n hw.usermem` / 1024 / 1024 / 1024)) -le 4 ] &&
    size=512m

for u in $md1 $md2; do
	mdconfig -l | grep -q md$u && mdconfig -d -u $u
	mdconfig -a -t swap -s $size -u $u
done

gmirror load > /dev/null 2>&1 && unload=1
old=`sysctl -n kern.geom.mirror.debug`
sysctl kern.geom.mirror.debug=-1 | grep -q -- -1 ||
    sysctl kern.geom.mirror.debug=$old > /dev/null
gmirror label -v -b split -s 2048 test /dev/md$md1 /dev/md$md2 \
    > /dev/null || exit 1
[ "`sysctl -in kern.geom.mirror.launch_mirror_before_timeout`" = "0" ] &&
    sleep $((`sysctl -n kern.geom.mirror.timeout` + 1))
[ -c /dev/mirror/test ] || exit 1
newfs /dev/mirror/test > /dev/null
mount /dev/mirror/test $mntpoint

gpid=`pgrep -fS "g_mirror test"`
s=0
start=`date +%s`
roid=debug.fail_point.g_mirror_regular_request_read
while [ $((`date +%s` - start)) -lt 120 ]; do
	gmirror forget test md$md2 2>/dev/null
	gmirror remove test md$md2 2>/dev/null
	sysctl $roid="1%return(5)[pid $gpid]" > /dev/null
	gmirror insert test md$md2
	sleep 10
	sysctl $roid="off" > /dev/null
	n=0
	while gmirror status test | grep -q DEGRADED; do
		sleep 2
		[ $((n += 1)) -gt 30 ] && { s=1; break 2; }
	done
	sysctl $roid="off" > /dev/null
done
[ $s -ne 0 ] && { echo "Timed out"; gmirror status test; }

for i in `jot 12`; do
	gmirror status test | grep -q SYNCHRONIZING || break
	sleep 10
done
while mount | grep $mntpoint | grep -q /mirror/; do
	umount $mntpoint || sleep 1
done
gmirror stop test || s=2
gmirror destroy test 2>/dev/null
[ $unload ] && gmirror unload

for u in $md2 $md1; do
	mdconfig -d -u $u || s=3
done
exit $s
