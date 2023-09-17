#!/bin/sh

#
# Copyright (c) 2009 Peter Holm <pho@FreeBSD.org>
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

# Simple zfs test of vdev as a file and snapshot clones

[ `id -u ` -ne 0 ] && echo "Must be root!" && exit 1
[ `uname -m` = "i386" ] && exit 0
[ `sysctl -n kern.kstack_pages` -lt 4 ] && exit 0

. ../default.cfg

kldstat -v | grep -q zfs.ko  || { kldload zfs.ko ||
    exit 0; loaded=1; }

d1=$diskimage.1
d2=$diskimage.2

dd if=/dev/zero of=$d1 bs=1m count=1k status=none
dd if=/dev/zero of=$d2 bs=1m count=1k status=none

zpool list | egrep -q "^stress2_tank" && zpool destroy stress2_tank
[ -d /stress2_tank ] && rm -rf /stress2_tank
zpool create stress2_tank $d1 $d2
zfs create stress2_tank/test
zfs set quota=100m stress2_tank/test

export RUNDIR=/stress2_tank/test/stressX
export runRUNTIME=10m
(cd ..; ./run.sh vfs.cfg) &

for i in `jot 20`; do
	zfs snapshot stress2_tank/test@snap$i
	zfs clone    stress2_tank/test@snap$i stress2_tank/snap$i
done
for i in `jot 20`; do
	zfs destroy stress2_tank/snap$i
	zfs destroy stress2_tank/test@snap$i
done
wait

zfs destroy -r stress2_tank
zpool destroy stress2_tank

rm -rf $d1 $d2
[ -n "$loaded" ] && kldunload zfs.ko
exit 0
