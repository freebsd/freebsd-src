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

# "panic: ffs_valloc: dup alloc" seen:
# https://people.freebsd.org/~pho/stress/log/kostik1128.txt

. ../default.cfg
[ `id -u ` -ne 0 ] && echo "Must be root!" && exit 1

md5=c2e2d89745914bf12c5b251c358e1b3f
size=$(((5368709120 + 13964088) / 1024 + 1))
zimg=tmp.disk.xz
log=fsck4.sh.log

[ `df -k $(dirname $diskimage) | tail -1 | awk '{print $4}'` -lt $size ] &&
{ echo "Not enough disk space."; exit 0; }
[ -z "`which fetch`" ] && exit 0

cd `dirname $diskimage`
trap "rm -f $diskimage $zimg" EXIT INT
fetch -q https://people.freebsd.org/~pho/$zimg || exit 0

m=`md5 < $zimg`
[ $m != $md5 ] && { echo "md5 diff"; rm $zimg; exit 1; }
unxz < $zimg > $diskimage
rm $zimg

mdconfig -a -t vnode -f $diskimage -u $mdstart
fsck_ffs -fy $diskimage > $log 2>&1
if grep -q "MARKED CLEAN" $log; then
	mount /dev/md$mdstart $mntpoint
	touch $mntpoint/xxxxxxxx	# Panics here
	umount $mntpoint
	s=0
else
	cat $log
	s=1
fi
mdconfig -d -u $mdstart

exit $s
