#!/bin/sh

#
# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) 2023 Peter Holm <pho@FreeBSD.org>
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

# growfs(8) test with output to FS to be grown.
# A regression test for D37896 ufs/suspend: deny suspension if calling
# process has file from mp opened for write
# Before D37896 this would result in growfs(8) hanging.

. ../default.cfg
[ `id -u ` -ne 0 ] && echo "Must be root!" && exit 1

log=/tmp/growfs2.sh.log
s=0
set -eu
mount | grep "on $mntpoint " | grep -q /dev/md && umount -f $mntpoint
[ -c /dev/md$mdstart ] &&  mdconfig -d -u $mdstart
mdconfig -a -t swap -s 32g -u $mdstart
/sbin/gpart create -s GPT md$mdstart > /dev/null
/sbin/gpart add -t freebsd-ufs -s 2g -a 4k md$mdstart > /dev/null
set +e

newfs $newfs_flags md${mdstart}p1 > /dev/null
mount /dev/md${mdstart}p1 $mntpoint
cp -r /usr/include $mntpoint/inc1

gpart resize -i 1 -s 31g -a 4k md$mdstart
echo "Expect: growfs: UFSSUSPEND: Resource deadlock avoided"
growfs -y md${mdstart}p1 > $mntpoint/log && s=1 ||  s=0

cp -r /usr/include $mntpoint/inc2
umount $mntpoint
fsck -fy /dev/md${mdstart}p1 > $log 2>&1
grep -q "WAS MODIFIED" $log && s=2
grep -q CLEAN $log || s=3
[ $s -ne 0 ] && cat $log

mdconfig -d -u $mdstart
rm -f $log
exit $s
