#!/bin/sh

#
# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) 2019 Dell EMC Isilon
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

# Failed Disk Test.
# "panic: witness_warn" seen in WiP kernel code.
# https://people.freebsd.org/~pho/stress/log/kirk102.txt

[ `id -u ` -ne 0 ] && echo "Must be root!" && exit 1

kldstat | grep -q geom_nop || { gnop load 2>/dev/null || exit 0 &&
    notloaded=1; }
gnop status || exit 1

. ../default.cfg

set -e
mount | grep $mntpoint | grep -q /dev/md && umount -f $mntpoint
[ -c /dev/md$mdstart ] &&  mdconfig -d -u $mdstart

mdconfig -a -t swap -s 2g -u $mdstart
gnop create /dev/md$mdstart
newfs $newfs_flags /dev/md$mdstart.nop > /dev/null
mount /dev/md$mdstart.nop $mntpoint
chmod 777 $mntpoint
set +e

export runRUNTIME=3m
export RUNDIR=$mntpoint/stressX
echo "Expect:
    g_vfs_done():md10.nop[READ(offset=1077805056, length=32768)]error = 5
    g_vfs_done():md10.nop[WRITE(offset=553680896, length=32768)]error = 5"

su $testuser -c 'cd ..; ./run.sh marcus.cfg' > /dev/null 2>&1 &
sleep `jot -r 1 60 120`
gnop configure -e 5 -r 100 -w 100 /dev/md$mdstart.nop
sleep 2
gnop configure -e 0 -r 0 -w 0 /dev/md$mdstart.nop
wait

while mount | grep -q "on $mntpoint "; do
	umount $mntpoint && break
	sleep 1
	[ $((n += 1)) -le 10 ] && continue
	echo "umount $mntpoint failed"
	s=1
	umount -f $mntpoint
	break
done
fsck_ffs -Rfy /dev/md$mdstart.nop || s=2
gnop destroy /dev/md$mdstart.nop
mdconfig -d -u $mdstart

[ $notloaded ] && gnop unload
exit $s
