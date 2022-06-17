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

# Simple isofs / union test scenario

[ `id -u ` -ne 0 ] && echo "Must be root!" && exit 1
[ -z "`which mkisofs`" ] && echo "mkisofs not found" && exit 0

. ../default.cfg

D=`dirname $diskimage`/dir
I=`dirname $diskimage`/dir.iso

rm -rf $D $I
mkdir -p $D
cp -r ../../stress2 $D 2>/dev/null

mkisofs -o $I -r $D > /dev/null 2>&1

mount | grep -q /dev/md$mdstart && umount -f /dev/md${mdstart}
[ -c /dev/md$mdstart ] && mdconfig -d -u $mdstart
mdconfig -a -t vnode -f $I -u $mdstart || exit 1
mount -t cd9660 /dev/md$mdstart $mntpoint || exit 1

m2=$((mdstart + 1))
mdconfig -s 1g -u $m2
newfs $newfs_flags md${m2} > /dev/null

mount -o union /dev/md${m2} $mntpoint || exit 1

export RUNDIR=$mntpoint/stressX
export runRUNTIME=5m
(cd $mntpoint/stress2; ./run.sh marcus.cfg) > /dev/null

umount $mntpoint
mdconfig -d -u $m2
umount $mntpoint
mdconfig -d -u $mdstart
rm -rf $D $I
exit 0
