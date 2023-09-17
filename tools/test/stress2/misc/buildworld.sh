#!/bin/sh

#
# Copyright (c) 2015 EMC Corp.
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

# "panic: Invalid page 0xfffff8007f227cf0 on inact queue" seen.
# https://people.freebsd.org/~pho/stress/log/buildworld.txt
# Fixed by r289377

. ../default.cfg

[ `id -u ` -ne 0 ] && echo "Must be root!" && exit 1

[ -d /usr/src/sys ] || exit 0
[ `sysctl -n hw.physmem` -gt $(( 2 * 1024 * 1024 * 1024)) ] &&
    { echo "RAM must be clamped to 2GB or less for this test."; exit 0; }
rm -f $diskimage
[ `df -k $(dirname $diskimage) | tail -1 | awk '{print $4}'` -lt \
    $((3 * 1024 * 1024)) ] && { echo "Need 3GB on `dirname $diskimage`"; \
    exit 0; }
mount | grep -q "on $mntpoint " && umount $mntpoint
dd if=/dev/zero of=$diskimage bs=1m count=3k status=none
trap "rm -f $diskimage" EXIT INT
mdconfig -a -t vnode -f $diskimage -u $mdstart
newfs $newfs_flags md$mdstart > /dev/null
mount -o async /dev/md$mdstart $mntpoint

cd /usr/src
export MAKEOBJDIRPREFIX=$mntpoint/obj
export TMPDIR=$mntpoint/tmp
mkdir $TMPDIR
chmod 0777 $TMPDIR

p=$((`sysctl -n hw.ncpu`+ 1))
timeout 20m make -i -j $p buildworld  DESTDIR=$mntpoint TARGET=amd64 \
    TARGET_ARCH=amd64 > /dev/null

while mount | grep -q "on $mntpoint "; do
	umount $mntpoint || sleep 1
done
checkfs /dev/md$mdstart || s=$?
mdconfig -d -u $mdstart
exit $s
