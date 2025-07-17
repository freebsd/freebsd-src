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

# Test scenario by Mark Johnston <markj@FreeBSD.org>

# Page fault seen:
# https://people.freebsd.org/~pho/stress/log/graid1_3.txt

. ../default.cfg

gmirror load > /dev/null 2>&1 && unload=1
[ -c /dev/mirror/markj-mirror ] &&
    { gmirror stop markj-mirror; gmirror destroy markj-mirror; }
old=`sysctl -n kern.geom.mirror.debug`
sysctl kern.geom.mirror.debug=-1 | grep -q -- -1 ||
    sysctl kern.geom.mirror.debug=$old > /dev/null
u1=$mdstart
u2=$((mdstart + 1))
size=$((5 * 1024  * 1024))
for u in $u1 $u2; do
	dd if=/dev/zero of=/tmp/graid1_2_di$u bs=$size count=1 \
	    status=none
	[ -c /dev/md$u ] && mdconfig -d -u $u
	mdconfig -a -t vnode -f /tmp/graid1_2_di$u -u $u
done
set -e

(
gpart create -s GPT md$u1
gpart create -s GPT md$u2
gpart add -t freebsd-ufs -s 1M md$u1
gpart add -t freebsd-ufs -s 1M md$u2
) > /dev/null

gmirror label markj-mirror md${u1}p1
set +e

while true; do
	gmirror label markj-mirror md${u1}p1
	gmirror destroy markj-mirror
done 2>/dev/null &
pid1=$!
while true; do
	gmirror insert markj-mirror md${u2}p1
	gmirror remove markj-mirror md${u2}p1
done 2>/dev/null &
pid2=$!

for i in `jot 60`; do
	gmirror list markj-mirror
	sleep 1
done > /dev/null 2>&1
sleep 60

kill $pid1 $pid2
wait
sleep 1

gmirror remove markj-mirror md${u2}p1 > /dev/null 2>&1
gmirror destroy markj-mirror > /dev/null 2>&1

mdconfig -d -u $u1 || exit 1
mdconfig -d -u $u2 || exit 1
rm -f /tmp/graid1_2_di*
[ $unload ] && gmirror unload
exit 0
