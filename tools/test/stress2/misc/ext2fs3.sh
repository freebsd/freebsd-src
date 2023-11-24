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

# ext2fs(5) test scenario with a 1k block size
# "panic: ext2_reallocblks: alloc mismatch" seen.
# "Fatal trap 12: page fault while in kernel mode" seen.

[ `id -u ` -ne 0 ] && echo "Must be root!" && exit 1
[ -r /usr/src/tools/regression/fsx/fsx.c ] || exit 0

. ../default.cfg

# Uses mke2fs from sysutils/e2fsprogs
[ -z "`type mke2fs 2>/dev/null`" ] &&
    echo "Skipping test as mke2fs not installed" && exit 0

dir=/tmp
odir=`pwd`
cd $dir
cc -o fsx -Wall -Wextra -O2 -g /usr/src/tools/regression/fsx/fsx.c || exit 1
rm -f fsx.c
cd $odir

mount | grep "$mntpoint" | grep -q md$mdstart && umount $mntpoint
mdconfig -l | grep -q md$mdstart &&  mdconfig -d -u $mdstart

mdconfig -a -t swap -s 1g -u $mdstart
mke2fs -m 0 -b 1024 /dev/md$mdstart > /dev/null

mount -t ext2fs /dev/md$mdstart $mntpoint
chmod 777 $mntpoint

cp /tmp/fsx $mntpoint
cd $mntpoint
./fsx -S 2016 -N 2000 ./TEST_FILE
cd $here

while mount | grep $mntpoint | grep -q /dev/md; do
	umount $mntpoint || sleep 1
done

mdconfig -d -u $mdstart
exit 0
