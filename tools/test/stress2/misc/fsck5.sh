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

# "INODE 3: FILE SIZE 1073741824 BEYOND END OF ALLOCATED FILE, SIZE SHOULD
# BE 268435456" seen.
# Original test scenario by Jamie Landeg-Jones <jamie@catflap.org>
# Fixed by r346185

. ../default.cfg
[ `id -u` -ne 0 ] && echo "Must be root!" && exit 1

log1=/tmp/fsck5.sh.1.log
log2=/tmp/fsck5.sh.2.log
set -e
mount | grep "on $mntpoint " | grep -q /dev/md && umount -f $mntpoint
[ -c /dev/md$mdstart ] &&  mdconfig -d -u $mdstart
mdconfig -a -t swap -s 1g -u $mdstart
newfs $newfs_flags -n md$mdstart > /dev/null
mount /dev/md$mdstart $mntpoint
set +e

cd $mntpoint
truncate -s 1g test
ls -lis test > $log1
sha256 -r test > sha256.out
cd /
umount $mntpoint

(
	fsck -fy /dev/md$mdstart
	fsck -fy /dev/md$mdstart
	fsck -fy /dev/md$mdstart
) > $log2 2>&1

mount /dev/md$mdstart $mntpoint
cd $mntpoint
ls -lis test >> $log1
sha256 -r test > sha256-2.out
s=0
cmp sha256.out sha256-2.out ||
    { cat $log2 $log1 sha256.out sha256-2.out; s=1; }
cd /
umount $mntpoint
mdconfig -d -u $mdstart
rm -f $log1 $log2
exit $s
