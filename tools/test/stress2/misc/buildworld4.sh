#!/bin/sh

#
# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) 2018 Dell EMC Isilon
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

# Buildworld test with SUJ
# "fsync: giving up on dirty (error = 35): tag devfs, type VCHR" seen.

. ../default.cfg

[ `id -u ` -ne 0 ] && echo "Must be root!" && exit 1

[ -d /usr/src/sys ] || exit 0
rm -f $diskimage
mount | grep -q "on $mntpoint " && umount $mntpoint
mdconfig -a -t swap -s 5g -u $mdstart
[ "$newfs_flags" = "-U" ] && newfs_flags="-j"
newfs $newfs_flags md$mdstart > /dev/null
mount /dev/md$mdstart $mntpoint
mount | grep $mntpoint

cd /usr/src
export MAKEOBJDIRPREFIX=$mntpoint/obj
export TMPDIR=$mntpoint/tmp
mkdir $TMPDIR
chmod 0777 $TMPDIR

p=$((`sysctl -n hw.ncpu`+ 1))
[ `sysctl -n vm.swap_total` -gt 0 ] && p=$((p * 4))
p=`jot -r 1 1 $p`
echo "make -i -j $p buildworld  DESTDIR=$mntpoint TARGET=amd64 "\
    "TARGET_ARCH=amd64"
make -i -j $p buildworld  DESTDIR=$mntpoint TARGET=amd64 TARGET_ARCH=amd64 \
    > /dev/null &
sleep 1
start=`date +%s`
while [ $((`date +%s` - start)) -lt 600 ]; do
	kill -0 $! > /dev/null 2>&1 || break
	sleep 30
done
kill $!  > /dev/null 2>&1
wait

while mount | grep -q "on $mntpoint "; do
	umount $mntpoint || sleep 1
done
checkfs /dev/md$mdstart; s=$?
mdconfig -d -u $mdstart
exit $s
