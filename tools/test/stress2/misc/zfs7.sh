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

# Non random based test scenario
# No problems seen.

[ `id -u ` -ne 0 ] && echo "Must be root!" && exit 1
[ `uname -m` = "i386" ] && exit 0
[ `sysctl -n kern.kstack_pages` -lt 4 ] && exit 0

. ../default.cfg

[ `sysctl -n hw.physmem` -lt $(( 4 * 1024 * 1024 * 1024)) ] && exit 0
kldstat -v | grep -q zfs.ko  || { kldload zfs.ko ||
    exit 0; loaded=1; }

u1=$mdstart
u2=$((u1 + 1))

mdconfig -l | grep -q md$u1 && mdconfig -d -u $u1
mdconfig -l | grep -q md$u2 && mdconfig -d -u $u2

mdconfig -s 1g -u $u1
mdconfig -s 1g -u $u2

zpool list | egrep -q "^stress2_tank" && zpool destroy stress2_tank
[ -d /stress2_tank ] && rm -rf /stress2_tank
zpool create stress2_tank md$u1 md$u2
zfs create stress2_tank/test

cc -o /tmp/fstool ../tools/fstool.c || exit 1
start=`date +%s`
for j in `jot 3`; do
	mkdir -p /stress2_tank/test/d$j
	(cd /stress2_tank/test/d$j; /tmp/fstool -f 100 -n 3000 -s 1k -t 5) &
done
wait
echo "Elapsed $((`date +%s` - start))"
rm /tmp/fstool

zfs destroy -r stress2_tank
zpool destroy stress2_tank

mdconfig -d -u $u1
mdconfig -d -u $u2
[ -n "$loaded" ] && kldunload zfs.ko
exit 0
