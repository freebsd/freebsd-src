#!/bin/sh

#
# Copyright (c) 2016 Dell EMC
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

# A 8k sector size test using buildworld.

# "panic: DI already started" seen:
# https://people.freebsd.org/~pho/stress/log/kostik1017.txt
# Fixed by r322175

. ../default.cfg

gigs=9
[ $((`sysctl -n vm.swap_total` / 1024 / 1024 / 1024)) -lt $gigs ] && exit 0
[ -d /usr/src/sys ] || exit 0

kldstat | grep -q geom_nop || { gnop load 2>/dev/null || exit 0 &&
    notloaded=1; }
gnop status || exit 1

mount | grep $mntpoint | grep -q /dev/md && umount -f $mntpoint
[ -c /dev/md$mdstart ] &&  mdconfig -d -u $mdstart

set -e
mdconfig -a -t swap -s ${gigs}g -u $mdstart
gnop create -S 8k /dev/md$mdstart
newfs $newfs_flags /dev/md$mdstart.nop > /dev/null
mount /dev/md$mdstart.nop $mntpoint
chmod 777 $mntpoint
set +e

(cd /usr; tar --exclude compile --exclude-vcs -cf - src) | \
    (cd $mntpoint; tar xf -)

cd $mntpoint/src
export MAKEOBJDIRPREFIX=$mntpoint/obj

p=$((`sysctl -n hw.ncpu`+ 1))
timeout 10m \
    make -i -j $p buildworld  DESTDIR=$mntpoint TARGET=amd64 \
    TARGET_ARCH=amd64 > /dev/null

cd /
while mount | grep $mntpoint | grep -q /dev/md; do
	umount $mntpoint || sleep 1
done
gnop destroy /dev/md$mdstart.nop
mdconfig -d -u $mdstart
[ $notloaded ] && gnop unload
exit 0
