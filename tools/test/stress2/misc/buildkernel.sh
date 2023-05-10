#!/bin/sh

#
# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) 2021 Peter Holm <pho@FreeBSD.org>
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

# Buildkernel test with SU

. ../default.cfg

[ `id -u ` -ne 0 ] && echo "Must be root!" && exit 1

[ -d /usr/src/sys ] || exit 0
set -e
mount | grep -q "on $mntpoint " && umount $mntpoint
mdconfig -a -t swap -s 5g -u $mdstart
newfs $newfs_flags md$mdstart > /dev/null
mount /dev/md$mdstart $mntpoint
set +e

cd /usr/src
export MAKEOBJDIRPREFIX=$mntpoint/obj
export TMPDIR=$mntpoint/tmp
mkdir $TMPDIR
chmod 0777 $TMPDIR
log=$mntpoint/log

p=$((`sysctl -n hw.ncpu`+ 1))
p=`jot -r 1 1 $p`
echo "make -j $p buildkernel KERNCONF=GENERIC DESTDIR=$mntpoint" \
    "TARGET=amd64 TARGET_ARCH=amd64"
make -j $p buildkernel KERNCONF=GENERIC DESTDIR=$mntpoint TARGET=amd64 \
    TARGET_ARCH=amd64 > $log 2>&1; s=$?
[ $s -ne 0 ] && tail -50 $log

while mount | grep -q "on $mntpoint "; do
	umount $mntpoint || sleep 1
done
mdconfig -d -u $mdstart
exit $s
