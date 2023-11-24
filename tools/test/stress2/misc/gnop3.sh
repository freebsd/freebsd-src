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

# A 8k sector size test.
# "panic: run 8 0xfffff80ff3d1e040 invalid" seen during fix development.

# OOM: https://people.freebsd.org/~pho/stress/log/gnop3.txt

. ../default.cfg

# OOM seen with RAM == 32g
[ $((`sysctl -n hw.usermem` / 1024 / 1024 / 1024)) -lt 32 ] && exit 0
[ $((`sysctl -n vm.swap_total` / 1024 / 1024 / 1024)) -lt 9 ] && exit 0

kldstat | grep -q geom_nop || { gnop load 2>/dev/null || exit 0 &&
    notloaded=1; }
gnop status || exit 1

mount | grep $mntpoint | grep -q /dev/md && umount -f $mntpoint
[ -c /dev/md$mdstart ] &&  mdconfig -d -u $mdstart

set -e
mdconfig -a -t swap -s 8g -u $mdstart
gnop create -S 8k /dev/md$mdstart
newfs $newfs_flags /dev/md$mdstart.nop > /dev/null
mount /dev/md$mdstart.nop $mntpoint
chmod 777 $mntpoint
set +e

cp -a ../../stress2 $mntpoint
here=`pwd`
cd $mntpoint/stress2/misc

export runRUNTIME=20m
export RUNDIR=$mntpoint/stressX

su $testuser -c 'cd ..; ./run.sh marcus.cfg'

cd $here
while mount | grep $mntpoint | grep -q /dev/md; do
	umount $mntpoint || sleep 1
done
gnop destroy /dev/md$mdstart.nop
mdconfig -d -u $mdstart
[ $notloaded ] && gnop unload
exit 0
